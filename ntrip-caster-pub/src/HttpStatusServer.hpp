/*
 * Jimmy Paputto 2026
 *
 * HTTP status page for NtripCaster.  Serves static assets from
 * `web_root` and a JSON API under /api/ used by the front-end.
 * Protected by HTTP Basic auth covering both static + API routes.
 *
 * Header-only; uses vendored cpp-httplib.
 */

#ifndef NTRIP_CASTER_HTTP_STATUS_HPP_
#define NTRIP_CASTER_HTTP_STATUS_HPP_

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <thread>

#include "httplib.h"

#include "Base64.hpp"
#include "NtripCaster.hpp"
#include "NtripLog.hpp"
#include "NtripStats.hpp"

namespace JimmyPaputto
{

    /// Tiny JSON serializer — sufficient for our flat shapes.
    class JsonWriter
    {
    public:
        std::string str() const { return out_.str(); }

        JsonWriter& objBegin() { sep(); out_ << '{'; first_ = true; return *this; }
        JsonWriter& objEnd()   { out_ << '}'; first_ = false; return *this; }
        JsonWriter& arrBegin() { sep(); out_ << '['; first_ = true; return *this; }
        JsonWriter& arrEnd()   { out_ << ']'; first_ = false; return *this; }

        JsonWriter& key(const char* k)
        {
            sep();
            quote(k);
            out_ << ':';
            first_ = true;
            return *this;
        }

        JsonWriter& vStr(const std::string& s) { sep(); quote(s.c_str()); first_ = false; return *this; }
        JsonWriter& vBool(bool b)   { sep(); out_ << (b ? "true" : "false"); first_ = false; return *this; }
        JsonWriter& vNull()         { sep(); out_ << "null"; first_ = false; return *this; }

        JsonWriter& vInt(long long n)
        {
            sep();
            out_ << n;
            first_ = false;
            return *this;
        }

        JsonWriter& vUint(unsigned long long n)
        {
            sep();
            out_ << n;
            first_ = false;
            return *this;
        }

        JsonWriter& vDouble(double d)
        {
            sep();
            char buf[40];
            std::snprintf(buf, sizeof(buf), "%.6f", d);
            out_ << buf;
            first_ = false;
            return *this;
        }

    private:
        void sep()
        {
            if (!first_) out_ << ',';
            first_ = false;
        }

        void quote(const char* s)
        {
            out_ << '"';
            for (const char* p = s; *p; ++p)
            {
                unsigned char c = static_cast<unsigned char>(*p);
                switch (c)
                {
                    case '"':  out_ << "\\\""; break;
                    case '\\': out_ << "\\\\"; break;
                    case '\b': out_ << "\\b";  break;
                    case '\f': out_ << "\\f";  break;
                    case '\n': out_ << "\\n";  break;
                    case '\r': out_ << "\\r";  break;
                    case '\t': out_ << "\\t";  break;
                    default:
                        if (c < 0x20)
                        {
                            char esc[8];
                            std::snprintf(esc, sizeof(esc), "\\u%04x", c);
                            out_ << esc;
                        }
                        else
                            out_ << static_cast<char>(c);
                }
            }
            out_ << '"';
        }

        std::ostringstream out_;
        bool first_ = true;
    };


    class HttpStatusServer : public NtripLoggable
    {
    public:
        HttpStatusServer(NtripCaster& caster,
                         std::string host, uint16_t port,
                         std::string user, std::string pass,
                         std::string realm,
                         std::string webRoot)
            : caster_(caster)
            , host_(std::move(host))
            , port_(port)
            , user_(std::move(user))
            , pass_(std::move(pass))
            , realm_(std::move(realm))
            , webRoot_(std::move(webRoot))
        {
        }

        ~HttpStatusServer() { stop(); }

        HttpStatusServer(const HttpStatusServer&) = delete;
        HttpStatusServer& operator=(const HttpStatusServer&) = delete;

        bool start()
        {
            if (running_.exchange(true))
                return false;

            // Resolve and validate web root.
            namespace fs = std::filesystem;
            std::string root = resolveWebRoot();
            if (root.empty() || !fs::is_directory(root))
            {
                log(ENtripLogLevel::Warning,
                    "[Http] web_root '%s' not found; static assets disabled",
                    root.c_str());
                root.clear();
            }
            resolvedWebRoot_ = root;

            srv_ = std::make_unique<httplib::Server>();

            // Pre-routing: enforce Basic auth on every request.
            srv_->set_pre_routing_handler(
                [this](const httplib::Request& req, httplib::Response& res)
                    -> httplib::Server::HandlerResponse
                {
                    if (!checkAuth(req))
                    {
                        res.status = 401;
                        res.set_header("WWW-Authenticate",
                                       std::string("Basic realm=\"") +
                                           realm_ + "\"");
                        res.set_content("Unauthorized\n", "text/plain");
                        return httplib::Server::HandlerResponse::Handled;
                    }
                    return httplib::Server::HandlerResponse::Unhandled;
                });

            // Static assets.
            if (!root.empty())
            {
                if (!srv_->set_mount_point("/", root))
                {
                    log(ENtripLogLevel::Warning,
                        "[Http] failed to mount static dir '%s'", root.c_str());
                }
            }

            // ── API routes ───────────────────────────────────────────
            srv_->Get("/api/health",
                      [](const httplib::Request&, httplib::Response& res)
                      {
                          res.set_content("{\"ok\":true}", "application/json");
                      });

            srv_->Get("/api/status",
                      [this](const httplib::Request&, httplib::Response& res)
                      { handleStatus(res); });

            srv_->Get("/api/sources",
                      [this](const httplib::Request&, httplib::Response& res)
                      { handleSources(res); });

            srv_->Get(R"(/api/mountpoint/(.+))",
                      [this](const httplib::Request& req, httplib::Response& res)
                      { handleMountpoint(req, res); });

            srv_->set_keep_alive_max_count(8);
            srv_->set_read_timeout(5, 0);
            srv_->set_write_timeout(5, 0);

            // Bind synchronously so we can report failure to the caller.
            if (!srv_->bind_to_port(host_, port_))
            {
                log(ENtripLogLevel::Error,
                    "[Http] failed to bind %s:%u", host_.c_str(), port_);
                running_ = false;
                srv_.reset();
                return false;
            }

            thread_ = std::thread([this]
                                  {
                try { srv_->listen_after_bind(); }
                catch (const std::exception& e) {
                    log(ENtripLogLevel::Error,
                        "[Http] listener exception: %s", e.what());
                } });

            log(ENtripLogLevel::Info,
                "[Http] status page listening on http://%s:%u/  (web_root=%s)",
                host_.c_str(), port_, root.empty() ? "(none)" : root.c_str());
            return true;
        }

        void stop()
        {
            if (!running_.exchange(false))
                return;
            if (srv_)
            {
                srv_->stop();
                if (thread_.joinable())
                    thread_.join();
                srv_.reset();
            }
        }

    private:
        bool checkAuth(const httplib::Request& req) const
        {
            if (user_.empty())
                return true; // open

            auto it = req.headers.find("Authorization");
            if (it == req.headers.end()) return false;

            const std::string& h = it->second;
            constexpr const char* kPrefix = "Basic ";
            if (h.rfind(kPrefix, 0) != 0) return false;

            std::string decoded = base64Decode(h.substr(6));
            auto colon = decoded.find(':');
            if (colon == std::string::npos) return false;

            std::string u = decoded.substr(0, colon);
            std::string p = decoded.substr(colon + 1);

            // Constant-time compare — string sizes are tiny so the
            // information-leak risk is mostly academic, but do it anyway.
            return constantTimeEquals(u, user_) &&
                   constantTimeEquals(p, pass_);
        }

        static bool constantTimeEquals(const std::string& a,
                                       const std::string& b)
        {
            if (a.size() != b.size()) return false;
            unsigned char d = 0;
            for (size_t i = 0; i < a.size(); ++i)
                d |= static_cast<unsigned char>(a[i] ^ b[i]);
            return d == 0;
        }

        std::string resolveWebRoot() const
        {
            namespace fs = std::filesystem;
            if (!webRoot_.empty()) return webRoot_;

            // Search candidates relative to the executable's install
            // prefix and the source tree (helpful when running from build/).
            const char* env = std::getenv("NTRIP_CASTER_WEB_ROOT");
            if (env && *env) return env;

            for (const char* candidate :
                 {
#ifdef NTRIP_CASTER_INSTALLED_WEB_ROOT
                  NTRIP_CASTER_INSTALLED_WEB_ROOT,
#endif
                  "/usr/local/share/ntrip-caster/web",
                  "/usr/share/ntrip-caster/web",
                  "./web",
                  "../web",
                  "../../web"})
            {
                std::error_code ec;
                if (fs::is_directory(candidate, ec))
                    return candidate;
            }
            return {};
        }

        // ── Handlers ─────────────────────────────────────────────────
        void handleStatus(httplib::Response& res)
        {
            NtripStats s = caster_.getStats();
            std::string mp = caster_.mountpoint();

            JsonWriter w;
            w.objBegin();
            w.key("mountpoint");
            if (mp.empty()) w.vNull(); else w.vStr(mp);
            w.key("clients").vUint(caster_.clientCount());
            w.key("bytes_tx").vUint(s.bytesTx);
            w.key("bytes_rx").vUint(s.bytesRx);
            w.key("frames_tx").vUint(s.framesTx);
            w.key("uptime_ms").vUint(s.uptimeMs);
            w.key("last_frame_age_ms").vUint(s.lastFrameAgeMs);
            w.key("avg_inter_frame_ms").vDouble(s.avgInterFrameMs);
            w.key("max_inter_frame_ms").vDouble(s.maxInterFrameMs);

            w.key("message_types").objBegin();
            for (const auto& [type, count] : s.messageTypeCounts)
            {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "%u", type);
                w.key(buf).vUint(count);
            }
            w.objEnd();

            w.objEnd();
            res.set_content(w.str(), "application/json");
        }

        void handleSources(httplib::Response& res)
        {
            auto sources = caster_.connectedSources();
            std::string activeMount = caster_.mountpoint();

            JsonWriter w;
            w.objBegin();
            w.key("active_mountpoint");
            if (activeMount.empty()) w.vNull(); else w.vStr(activeMount);
            w.key("sources").arrBegin();
            for (const auto& s : sources)
            {
                w.objBegin();
                w.key("peer").vStr(s.peer);
                w.key("mountpoint");
                if (s.mountpoint.empty()) w.vNull(); else w.vStr(s.mountpoint);
                w.key("connected_unix_ms").vUint(s.connectedUnixMs);
                w.objEnd();
            }
            w.arrEnd();
            w.objEnd();
            res.set_content(w.str(), "application/json");
        }

        void handleMountpoint(const httplib::Request& req,
                              httplib::Response& res)
        {
            std::string requested = req.matches.size() > 1
                                        ? req.matches[1].str()
                                        : std::string();
            std::string activeMount = caster_.mountpoint();

            if (requested.empty() || requested != activeMount)
            {
                res.status = 404;
                res.set_content(
                    "{\"error\":\"unknown mountpoint\"}",
                    "application/json");
                return;
            }

            NtripStats   s    = caster_.getStats();
            RtcmSnapshot snap = caster_.rtcmSnapshot();

            JsonWriter w;
            w.objBegin();
            w.key("name").vStr(requested);
            w.key("clients").vUint(caster_.clientCount());
            w.key("uptime_ms").vUint(s.uptimeMs);
            w.key("last_frame_age_ms").vUint(s.lastFrameAgeMs);
            w.key("bytes_tx").vUint(s.bytesTx);
            w.key("frames_tx").vUint(s.framesTx);
            w.key("avg_inter_frame_ms").vDouble(s.avgInterFrameMs);
            w.key("max_inter_frame_ms").vDouble(s.maxInterFrameMs);

            // Base ARP
            w.key("arp");
            if (snap.arp)
            {
                w.objBegin();
                w.key("lat_deg").vDouble(snap.arp->latitudeDeg);
                w.key("lon_deg").vDouble(snap.arp->longitudeDeg);
                w.key("height_m").vDouble(snap.arp->heightMeters);
                if (snap.arpEcefX) w.key("ecef_x").vDouble(*snap.arpEcefX);
                if (snap.arpEcefY) w.key("ecef_y").vDouble(*snap.arpEcefY);
                if (snap.arpEcefZ) w.key("ecef_z").vDouble(*snap.arpEcefZ);
                w.objEnd();
            }
            else
            {
                w.vNull();
            }

            // Per-message-type counts and last-seen ages
            w.key("message_types").objBegin();
            uint64_t nowMs = unixNowMs();
            for (const auto& [type, count] : snap.messageTypeCounts)
            {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "%u", type);
                w.key(buf).objBegin();
                w.key("count").vUint(count);
                auto it = snap.messageTypeLastMs.find(type);
                if (it != snap.messageTypeLastMs.end() && nowMs >= it->second)
                    w.key("last_age_ms").vUint(nowMs - it->second);
                else
                    w.key("last_age_ms").vNull();
                w.objEnd();
            }
            w.objEnd();

            // Per-constellation visible satellites
            w.key("constellations").arrBegin();
            const bool haveBase = snap.arpEcefX && snap.arpEcefY && snap.arpEcefZ;
            const double gpsTow = currentGpsTowSeconds(nowMs);
            for (const auto& [g, view] : snap.constellations)
            {
                w.objBegin();
                w.key("gnss").vStr(gnssName(g));
                w.key("last_msg_type").vUint(view.lastMsgType);
                w.key("msm").vUint(view.msmNumber);
                w.key("ref_station").vUint(view.refStation);
                w.key("epoch_time_ms").vUint(view.epochTimeMs);
                if (view.lastSeenUnixMs && nowMs >= view.lastSeenUnixMs)
                    w.key("last_age_ms").vUint(nowMs - view.lastSeenUnixMs);
                else
                    w.key("last_age_ms").vNull();
                w.key("sat_ids").arrBegin();
                for (uint8_t id : view.satIds()) w.vUint(id);
                w.arrEnd();
                w.key("signal_ids").arrBegin();
                for (uint8_t id : view.signalIds()) w.vUint(id);
                w.arrEnd();
                // Per-SV az/el computed from cached ephemerides
                w.key("sats").arrBegin();
                for (uint8_t msmId : view.satIds())
                {
                    uint8_t prn = msmIdToPrn(g, msmId);
                    SvKey   key{ msmGnssToCode(g), prn };
                    auto    it = snap.ephemerides.find(key);

                    w.objBegin();
                    w.key("sv_id").vUint(prn);
                    w.key("msm_id").vUint(msmId);
                    if (it != snap.ephemerides.end() && haveBase)
                    {
                        const KeplerEph& eph = it->second;
                        double tow = gnssTowFromGpsTow(gpsTow, eph.gnss);
                        double az, el, sx, sy, sz;
                        if (computeAzEl(eph, *snap.arpEcefX, *snap.arpEcefY,
                                        *snap.arpEcefZ, tow,
                                        az, el, sx, sy, sz))
                        {
                            w.key("az_deg").vDouble(az);
                            w.key("el_deg").vDouble(el);
                            uint64_t ageMs = (nowMs >= eph.receivedUnixMs)
                                                 ? nowMs - eph.receivedUnixMs
                                                 : 0;
                            w.key("eph_age_s").vDouble(
                                static_cast<double>(ageMs) / 1000.0);
                        }
                        else
                        {
                            w.key("az_deg").vNull();
                            w.key("el_deg").vNull();
                            w.key("eph_age_s").vNull();
                        }
                    }
                    else
                    {
                        w.key("az_deg").vNull();
                        w.key("el_deg").vNull();
                        w.key("eph_age_s").vNull();
                    }
                    w.objEnd();
                }
                w.arrEnd();
                w.objEnd();
            }
            w.arrEnd();

            w.objEnd();
            res.set_content(w.str(), "application/json");
        }

        static uint64_t unixNowMs()
        {
            using namespace std::chrono;
            return static_cast<uint64_t>(
                duration_cast<milliseconds>(
                    system_clock::now().time_since_epoch()).count());
        }

        /// Map MSM constellation enum → ephemeris GnssCode.
        static uint8_t msmGnssToCode(EGnss g)
        {
            switch (g)
            {
                case EGnss::GPS:     return GnssCode::GPS;
                case EGnss::Galileo: return GnssCode::GAL;
                case EGnss::QZSS:    return GnssCode::QZSS;
                case EGnss::BeiDou:  return GnssCode::BDS;
                default:             return 0;
            }
        }

        /// MSM in-mask satellite index → constellation-native PRN.
        /// QZSS MSM mask uses 1..10 mapping to PRN 193..202; the rest
        /// are 1:1.  GLONASS / SBAS / NavIC indices pass through but
        /// we don't currently match against an ephemeris cache for
        /// those.
        static uint8_t msmIdToPrn(EGnss g, uint8_t msmId)
        {
            if (g == EGnss::QZSS) return static_cast<uint8_t>(192 + msmId);
            return msmId;
        }

        NtripCaster& caster_;
        std::string  host_;
        uint16_t     port_;
        std::string  user_;
        std::string  pass_;
        std::string  realm_;
        std::string  webRoot_;
        std::string  resolvedWebRoot_;

        std::atomic<bool> running_{false};
        std::unique_ptr<httplib::Server> srv_;
        std::thread thread_;
    };

}

#endif // NTRIP_CASTER_HTTP_STATUS_HPP_

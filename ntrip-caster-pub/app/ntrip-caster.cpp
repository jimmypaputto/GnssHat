/*
 * Jimmy Paputto 2026
 *
 * ntrip-caster — standalone NTRIP v2.0 caster daemon.
 *
 * Operates as a relay: a single base station POSTs RTCM3 corrections
 * to the configured mountpoint; rover clients GET that mountpoint
 * to receive the relayed stream.
 *
 * Run:
 *   ntrip-caster --port 2101 \
 *                --user rover --pass secret \
 *                [--tls-cert /etc/ssl/cert.pem --tls-key /etc/ssl/key.pem]
 *
 * The mountpoint name is claimed by the first source that POSTs to the
 * caster; lat/lon are auto-decoded from RTCM 1005/1006 messages in the
 * source stream and advertised in the sourcetable.
 */

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <string>
#include <thread>

#include "NtripCaster.hpp"
#include "CasterConfig.hpp"
#include "HttpStatusServer.hpp"

using namespace JimmyPaputto;

namespace {

std::atomic<bool> g_running{true};

void signalHandler(int) { g_running = false; }

void printUsage(const char* prog)
{
    std::printf(
        "Usage: %s [options]\n"
        "\n"
        "  --config <path>       Load TOML config file (CLI flags override)\n"
        "  --host <addr>         Bind address           (default 0.0.0.0)\n"
        "  --port <n>            Listen port            (default 2101)\n"
        "  --max-clients <n>     Max concurrent clients (default 64)\n"
        "  --user <name>         Basic-auth username    (default: open access)\n"
        "  --pass <pwd>          Basic-auth password\n"
        "  --tls-cert <path>     PEM certificate (enables TLS)\n"
        "  --tls-key  <path>     PEM private key\n"
        "  --log-level <lvl>     error|warning|info|debug (default info)\n"
        "  --stats-interval <s>  Print stats every N seconds (0=off, default 30)\n"
        "  --http                Enable HTTP status page (default off)\n"
        "  --no-http             Disable HTTP status page (overrides config)\n"
        "  --http-port <n>       HTTP status page port  (default 8080)\n"
        "  --http-user <name>    HTTP Basic-auth user   (default rtk)\n"
        "  --http-pass <pwd>     HTTP Basic-auth pass   (default rtkpassword)\n"
        "  --http-web-root <p>   Override static-asset directory\n"
        "  -h, --help            Show this help\n"
        "\n"
        "The mountpoint name is claimed by the first source that POSTs.\n"
        "Lat/lon are auto-decoded from RTCM 1005/1006 messages in the\n"
        "source stream and advertised in the sourcetable.\n",
        prog);
}

const char* logLevelTag(ENtripLogLevel l)
{
    switch (l)
    {
        case ENtripLogLevel::Error:   return "ERR ";
        case ENtripLogLevel::Warning: return "WARN";
        case ENtripLogLevel::Info:    return "INFO";
        case ENtripLogLevel::Debug:   return "DBG ";
    }
    return "    ";
}

std::string nowStamp()
{
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

void printStats(const NtripStats& s, size_t clients, const std::string& mount)
{
    std::printf(
        "[%s] STATS  mount=%s  clients=%zu  rxBytes=%llu  txBytes=%llu  "
        "frames=%llu  lastFrame=%llums  uptime=%llus\n",
        nowStamp().c_str(),
        mount.empty() ? "(none)" : mount.c_str(),
        clients,
        (unsigned long long)s.bytesRx,
        (unsigned long long)s.bytesTx,
        (unsigned long long)s.framesTx,
        (unsigned long long)s.lastFrameAgeMs,
        (unsigned long long)(s.uptimeMs / 1000));
    std::fflush(stdout);
}

} // namespace

int main(int argc, char** argv)
{
    // ── Defaults via CasterConfig ─────────────────────────────────
    CasterConfig cfg;

    // First pass: pick up --config so subsequent CLI flags can override.
    auto needRaw = [&](int i, const char* opt) {
        if (i + 1 >= argc)
        {
            std::fprintf(stderr, "Error: %s requires a value.\n", opt);
            std::exit(2);
        }
        return std::string(argv[i + 1]);
    };

    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        if (a == "--config")
        {
            std::string path = needRaw(i, "--config");
            try { cfg.loadFromToml(path); }
            catch (const std::exception& e)
            {
                std::fprintf(stderr, "Error: %s\n", e.what());
                return 2;
            }
            ++i;
        }
    }

    // Second pass: CLI flags override TOML.
    auto need = needRaw;

    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        if      (a == "-h" || a == "--help")   { printUsage(argv[0]); return 0; }
        else if (a == "--config")              { ++i; /* already handled */ }
        else if (a == "--host")                { cfg.host = need(i, "--host"); ++i; }
        else if (a == "--port")                { cfg.port = static_cast<uint16_t>(std::stoi(need(i, "--port"))); ++i; }
        else if (a == "--max-clients")         { cfg.maxClients = static_cast<size_t>(std::stoul(need(i, "--max-clients"))); ++i; }
        else if (a == "--user")                { cfg.user = need(i, "--user"); ++i; }
        else if (a == "--pass")                { cfg.pass = need(i, "--pass"); ++i; }
        else if (a == "--tls-cert")            { cfg.tlsCert = need(i, "--tls-cert"); ++i; }
        else if (a == "--tls-key")             { cfg.tlsKey  = need(i, "--tls-key"); ++i; }
        else if (a == "--log-level")           { cfg.logLevel = need(i, "--log-level"); ++i; }
        else if (a == "--stats-interval")      { cfg.statsInterval = std::stoi(need(i, "--stats-interval")); ++i; }
        else if (a == "--http")                { cfg.httpEnabled = true; }
        else if (a == "--no-http")             { cfg.httpEnabled = false; }
        else if (a == "--http-port")           { cfg.httpPort = static_cast<uint16_t>(std::stoi(need(i, "--http-port"))); ++i; }
        else if (a == "--http-user")           { cfg.httpUser = need(i, "--http-user"); ++i; }
        else if (a == "--http-pass")           { cfg.httpPass = need(i, "--http-pass"); ++i; }
        else if (a == "--http-web-root")       { cfg.httpWebRoot = need(i, "--http-web-root"); ++i; }
        else
        {
            std::fprintf(stderr, "Error: unknown option '%s'\n", a.c_str());
            printUsage(argv[0]);
            return 2;
        }
    }

    if (!cfg.tlsCert.empty() != !cfg.tlsKey.empty())
    {
        std::fprintf(stderr, "Error: tls cert and key must be specified together.\n");
        return 2;
    }

    if (!cfg.user.empty() && cfg.pass.empty())
        std::fprintf(stderr, "Warning: user given without pass; password is empty.\n");

    ENtripLogLevel logLevel = parseLogLevelString(cfg.logLevel);

    // ── Signal handling ───────────────────────────────────────────
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGPIPE, SIG_IGN);

    // ── Build & start caster ──────────────────────────────────────
    NtripCaster caster(cfg.host, cfg.port, cfg.maxClients);

    caster.setLogLevel(logLevel);
    caster.setLogCallback([](ENtripLogLevel lvl, const std::string& msg) {
        FILE* out = (lvl == ENtripLogLevel::Error || lvl == ENtripLogLevel::Warning)
                    ? stderr : stdout;
        std::fprintf(out, "[%s] %s %s\n",
                     nowStamp().c_str(), logLevelTag(lvl), msg.c_str());
        std::fflush(out);
    });

    if (!cfg.user.empty())
        caster.setCredentials(cfg.user, cfg.pass);

    if (!cfg.tlsCert.empty())
    {
        if (!NtripCaster::isTlsAvailable())
        {
            std::fprintf(stderr,
                "Error: TLS requested but binary was built without "
                "OpenSSL support (-DNTRIP_CASTER_TLS=ON).\n");
            return 1;
        }
        if (!caster.setTls(cfg.tlsCert, cfg.tlsKey))
        {
            std::fprintf(stderr, "Error: failed to load TLS cert/key.\n");
            return 1;
        }
        std::printf("[%s] TLS enabled (cert=%s)\n",
                    nowStamp().c_str(), cfg.tlsCert.c_str());
    }

    if (!caster.start())
    {
        std::fprintf(stderr, "Error: caster failed to start.\n");
        return 1;
    }

    std::printf("[%s] ntrip-caster listening on %s:%u "
                "(max %zu clients)%s\n",
                nowStamp().c_str(), cfg.host.c_str(), cfg.port,
                cfg.maxClients, cfg.user.empty() ? " [open]" : " [auth]");
    std::fflush(stdout);

    // ── HTTP status server ────────────────────────────────────────
    std::unique_ptr<HttpStatusServer> http;
    if (cfg.httpEnabled)
    {
        http = std::make_unique<HttpStatusServer>(
            caster, cfg.httpHost, cfg.httpPort,
            cfg.httpUser, cfg.httpPass, cfg.httpRealm, cfg.httpWebRoot);
        http->setLogLevel(logLevel);
        http->setLogCallback([](ENtripLogLevel lvl, const std::string& msg) {
            FILE* out = (lvl == ENtripLogLevel::Error || lvl == ENtripLogLevel::Warning)
                        ? stderr : stdout;
            std::fprintf(out, "[%s] %s %s\n",
                         nowStamp().c_str(), logLevelTag(lvl), msg.c_str());
            std::fflush(out);
        });
        if (!http->start())
        {
            std::fprintf(stderr,
                "Warning: HTTP status server failed to start; continuing without it.\n");
            http.reset();
        }
    }

    // ── Main loop ─────────────────────────────────────────────────
    auto nextStats = std::chrono::steady_clock::now() +
                     std::chrono::seconds(cfg.statsInterval);

    while (g_running.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        if (cfg.statsInterval > 0 &&
            std::chrono::steady_clock::now() >= nextStats)
        {
            printStats(caster.getStats(), caster.clientCount(),
                       caster.mountpoint());
            nextStats = std::chrono::steady_clock::now() +
                        std::chrono::seconds(cfg.statsInterval);
        }
    }

    std::printf("[%s] Shutting down…\n", nowStamp().c_str());
    if (http) http->stop();
    caster.stop();
    return 0;
}

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
 *   ntrip-caster --port 2101 --mount GNSS_HAT \
 *                --user rover --pass secret \
 *                [--tls-cert /etc/ssl/cert.pem --tls-key /etc/ssl/key.pem]
 */

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <thread>

#include "NtripCaster.hpp"

using namespace JimmyPaputto;

namespace {

std::atomic<bool> g_running{true};

void signalHandler(int) { g_running = false; }

void printUsage(const char* prog)
{
    std::printf(
        "Usage: %s [options]\n"
        "\n"
        "  --host <addr>         Bind address           (default 0.0.0.0)\n"
        "  --port <n>            Listen port            (default 2101)\n"
        "  --mount <name>        Mountpoint name        (default CASTER)\n"
        "  --max-clients <n>     Max concurrent clients (default 64)\n"
        "  --user <name>         Basic-auth username    (default: open access)\n"
        "  --pass <pwd>          Basic-auth password\n"
        "  --lat <deg>           Advertised latitude    (default 0.0)\n"
        "  --lon <deg>           Advertised longitude   (default 0.0)\n"
        "  --tls-cert <path>     PEM certificate (enables TLS)\n"
        "  --tls-key  <path>     PEM private key\n"
        "  --log-level <lvl>     error|warning|info|debug (default info)\n"
        "  --stats-interval <s>  Print stats every N seconds (0=off, default 30)\n"
        "  -h, --help            Show this help\n"
        "\n"
        "The caster expects exactly one base station to POST RTCM3 to the\n"
        "mountpoint; all GET clients receive the relayed stream.\n",
        prog);
}

ENtripLogLevel parseLogLevel(const std::string& s)
{
    if (s == "error")   return ENtripLogLevel::Error;
    if (s == "warning") return ENtripLogLevel::Warning;
    if (s == "info")    return ENtripLogLevel::Info;
    if (s == "debug")   return ENtripLogLevel::Debug;
    return ENtripLogLevel::Info;
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

void printStats(const NtripStats& s, size_t clients)
{
    std::printf(
        "[%s] STATS  clients=%zu  rxBytes=%llu  txBytes=%llu  "
        "frames=%llu  lastFrame=%llums  uptime=%llus\n",
        nowStamp().c_str(),
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
    // ── Defaults ───────────────────────────────────────────────────
    std::string host = "0.0.0.0";
    uint16_t    port = 2101;
    std::string mount = "CASTER";
    size_t      maxClients = 64;
    std::string user, pass;
    double      lat = 0.0, lon = 0.0;
    std::string tlsCert, tlsKey;
    ENtripLogLevel logLevel = ENtripLogLevel::Info;
    int         statsIntervalSec = 30;

    // ── Parse args ─────────────────────────────────────────────────
    auto need = [&](int i, const char* opt) {
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
        if (a == "-h" || a == "--help")        { printUsage(argv[0]); return 0; }
        else if (a == "--host")                { host = need(i, "--host"); ++i; }
        else if (a == "--port")                { port = static_cast<uint16_t>(std::stoi(need(i, "--port"))); ++i; }
        else if (a == "--mount")               { mount = need(i, "--mount"); ++i; }
        else if (a == "--max-clients")         { maxClients = static_cast<size_t>(std::stoul(need(i, "--max-clients"))); ++i; }
        else if (a == "--user")                { user = need(i, "--user"); ++i; }
        else if (a == "--pass")                { pass = need(i, "--pass"); ++i; }
        else if (a == "--lat")                 { lat = std::stod(need(i, "--lat")); ++i; }
        else if (a == "--lon")                 { lon = std::stod(need(i, "--lon")); ++i; }
        else if (a == "--tls-cert")            { tlsCert = need(i, "--tls-cert"); ++i; }
        else if (a == "--tls-key")             { tlsKey  = need(i, "--tls-key"); ++i; }
        else if (a == "--log-level")           { logLevel = parseLogLevel(need(i, "--log-level")); ++i; }
        else if (a == "--stats-interval")      { statsIntervalSec = std::stoi(need(i, "--stats-interval")); ++i; }
        else
        {
            std::fprintf(stderr, "Error: unknown option '%s'\n", a.c_str());
            printUsage(argv[0]);
            return 2;
        }
    }

    if (!tlsCert.empty() != !tlsKey.empty())
    {
        std::fprintf(stderr, "Error: --tls-cert and --tls-key must be specified together.\n");
        return 2;
    }

    if (!user.empty() && pass.empty())
        std::fprintf(stderr, "Warning: --user given without --pass; password is empty.\n");

    // ── Signal handling ───────────────────────────────────────────
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGPIPE, SIG_IGN);

    // ── Build & start caster ──────────────────────────────────────
    NtripCaster caster(host, port, mount, maxClients);

    caster.setLogLevel(logLevel);
    caster.setLogCallback([](ENtripLogLevel lvl, const std::string& msg) {
        FILE* out = (lvl == ENtripLogLevel::Error || lvl == ENtripLogLevel::Warning)
                    ? stderr : stdout;
        std::fprintf(out, "[%s] %s %s\n",
                     nowStamp().c_str(), logLevelTag(lvl), msg.c_str());
        std::fflush(out);
    });

    if (!user.empty())
        caster.setCredentials(user, pass);

    caster.updatePosition(lat, lon);

    if (!tlsCert.empty())
    {
        if (!NtripCaster::isTlsAvailable())
        {
            std::fprintf(stderr,
                "Error: TLS requested but binary was built without "
                "OpenSSL support (-DNTRIP_CASTER_TLS=ON).\n");
            return 1;
        }
        if (!caster.setTls(tlsCert, tlsKey))
        {
            std::fprintf(stderr, "Error: failed to load TLS cert/key.\n");
            return 1;
        }
        std::printf("[%s] TLS enabled (cert=%s)\n",
                    nowStamp().c_str(), tlsCert.c_str());
    }

    if (!caster.start())
    {
        std::fprintf(stderr, "Error: caster failed to start.\n");
        return 1;
    }

    std::printf("[%s] ntrip-caster listening on %s:%u/%s "
                "(max %zu clients)%s\n",
                nowStamp().c_str(), host.c_str(), port, mount.c_str(),
                maxClients, user.empty() ? " [open]" : " [auth]");
    std::fflush(stdout);

    // ── Main loop ─────────────────────────────────────────────────
    auto nextStats = std::chrono::steady_clock::now() +
                     std::chrono::seconds(statsIntervalSec);

    while (g_running.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        if (statsIntervalSec > 0 &&
            std::chrono::steady_clock::now() >= nextStats)
        {
            printStats(caster.getStats(), caster.clientCount());
            nextStats = std::chrono::steady_clock::now() +
                        std::chrono::seconds(statsIntervalSec);
        }
    }

    std::printf("[%s] Shutting down…\n", nowStamp().c_str());
    caster.stop();
    return 0;
}

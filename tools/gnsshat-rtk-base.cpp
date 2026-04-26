/*
 * Jimmy Paputto 2026
 *
 * gnsshat-rtk-base — RTK base station tool.
 *
 * Configures the GNSS HAT as an RTK base and serves corrections
 * via a local NTRIP caster or pushes them to a remote caster.
 *
 * Usage:
 *   gnsshat-rtk-base [options]
 *   gnsshat-rtk-base --config <path.toml> [overrides]
 */

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <thread>

#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "GnssHat.hpp"
#include "ntrip/NtripCaster.hpp"
#include "ntrip/NtripServer.hpp"
#include "RtkBaseConfig.hpp"

using namespace JimmyPaputto;

// ── Globals set at startup ──────────────────────────────────────────
static std::atomic<bool> g_running{true};
static bool g_serviceMode = false;                     // formatted output style
static ENtripLogLevel g_logLevel = ENtripLogLevel::Info;
static uint64_t g_watchdogUsec = 0;                    // 0 = disabled

static void signalHandler(int) { g_running = false; }

// ── Logging ─────────────────────────────────────────────────────────
enum class LogLvl { Error = 3, Warning = 4, Info = 6, Debug = 7 };

static bool shouldLog(LogLvl lvl)
{
    // Map LogLvl → ENtripLogLevel severity ordering (Error=0 … Debug=3).
    const int threshold = static_cast<int>(g_logLevel);
    int curr = 0;
    switch (lvl)
    {
        case LogLvl::Error:   curr = 0; break;
        case LogLvl::Warning: curr = 1; break;
        case LogLvl::Info:    curr = 2; break;
        case LogLvl::Debug:   curr = 3; break;
    }
    return curr <= threshold;
}

static void logVLine(LogLvl lvl, const char* fmt, va_list ap)
{
    if (!shouldLog(lvl)) return;

    char body[1024];
    std::vsnprintf(body, sizeof(body), fmt, ap);

    // Strip trailing newline from body; we always add exactly one.
    size_t len = std::strlen(body);
    while (len > 0 && (body[len - 1] == '\n' || body[len - 1] == '\r'))
        body[--len] = '\0';

    FILE* out = (lvl == LogLvl::Error || lvl == LogLvl::Warning) ? stderr : stdout;

    if (g_serviceMode)
    {
        // sd-daemon priority prefix parsed by journald when SyslogLevelPrefix=true.
        std::fprintf(out, "<%d>%s\n", static_cast<int>(lvl), body);
    }
    else
    {
        char ts[32];
        std::time_t now = std::time(nullptr);
        std::tm tm{};
        gmtime_r(&now, &tm);
        std::strftime(ts, sizeof(ts), "%FT%TZ", &tm);
        const char* tag = "INF";
        switch (lvl)
        {
            case LogLvl::Error:   tag = "ERR"; break;
            case LogLvl::Warning: tag = "WRN"; break;
            case LogLvl::Info:    tag = "INF"; break;
            case LogLvl::Debug:   tag = "DBG"; break;
        }
        std::fprintf(out, "[%s] %s %s\n", ts, tag, body);
    }
    std::fflush(out);
}

static void logLine(LogLvl lvl, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    logVLine(lvl, fmt, ap);
    va_end(ap);
}

static void ntripLogPrinter(ENtripLogLevel level, const std::string& msg)
{
    LogLvl mapped = LogLvl::Info;
    switch (level)
    {
        case ENtripLogLevel::Error:   mapped = LogLvl::Error;   break;
        case ENtripLogLevel::Warning: mapped = LogLvl::Warning; break;
        case ENtripLogLevel::Info:    mapped = LogLvl::Info;    break;
        case ENtripLogLevel::Debug:   mapped = LogLvl::Debug;   break;
    }
    logLine(mapped, "[NTRIP] %s", msg.c_str());
}

// ── sd_notify (inline, no libsystemd) ───────────────────────────────
// Sends a datagram to $NOTIFY_SOCKET. Supports abstract sockets (leading '@').
// No-op when NOTIFY_SOCKET is unset. Returns true on send success.
static bool sdNotifyRaw(const char* state)
{
    const char* sock = std::getenv("NOTIFY_SOCKET");
    if (!sock || !*sock) return false;

    const size_t len = std::strlen(sock);
    if (len >= sizeof(((struct sockaddr_un*)0)->sun_path)) return false;

    int fd = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return false;

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    socklen_t addrlen = 0;
    if (sock[0] == '@')
    {
        // Abstract namespace: first byte is NUL, rest is the name.
        addr.sun_path[0] = '\0';
        std::memcpy(addr.sun_path + 1, sock + 1, len - 1);
        addrlen = offsetof(struct sockaddr_un, sun_path) + len;
    }
    else
    {
        std::memcpy(addr.sun_path, sock, len);
        addr.sun_path[len] = '\0';
        addrlen = offsetof(struct sockaddr_un, sun_path) + len + 1;
    }

    ssize_t n = ::sendto(fd, state, std::strlen(state), MSG_NOSIGNAL,
                         reinterpret_cast<struct sockaddr*>(&addr), addrlen);
    ::close(fd);
    return n > 0;
}

// Initialize watchdog interval from WATCHDOG_USEC / WATCHDOG_PID.
// Only enables pings if the PID check passes (or WATCHDOG_PID is unset).
static void initWatchdog(bool enabledByConfig)
{
    g_watchdogUsec = 0;
    if (!enabledByConfig) return;

    const char* usecStr = std::getenv("WATCHDOG_USEC");
    if (!usecStr || !*usecStr) return;

    const char* pidStr = std::getenv("WATCHDOG_PID");
    if (pidStr && *pidStr)
    {
        char* end = nullptr;
        long wpid = std::strtol(pidStr, &end, 10);
        if (end == pidStr || wpid != static_cast<long>(::getpid()))
            return;  // not for us
    }

    char* end = nullptr;
    unsigned long long v = std::strtoull(usecStr, &end, 10);
    if (end == usecStr || v == 0) return;

    g_watchdogUsec = v;
}

static void pingWatchdog()
{
    if (g_watchdogUsec == 0) return;
    sdNotifyRaw("WATCHDOG=1");
}

// ── Auto-detect whether we're a systemd service ─────────────────────
static bool detectServiceMode()
{
    if (std::getenv("INVOCATION_ID")) return true;
    if (std::getenv("JOURNAL_STREAM")) return true;
    if (std::getenv("NOTIFY_SOCKET"))  return true;
    if (!::isatty(STDOUT_FILENO))      return true;
    return false;
}

static void printUsage(const char* progname)
{
    std::printf(
        "Usage: %s [options]\n"
        "\n"
        "RTK base station tool. Configures the GNSS HAT as an RTK base\n"
        "and serves RTCM3 corrections via NTRIP.\n"
        "\n"
        "Options:\n"
        "  --config <path>          Load TOML configuration file\n"
        "  --port <port>            NTRIP port (default: 2101)\n"
        "  --mountpoint <name>      NTRIP mountpoint (default: GNSS_HAT)\n"
        "  --host <host>            NTRIP host/bind address (default: 0.0.0.0)\n"
        "  --mode <caster|server>   NTRIP mode (default: caster)\n"
        "  --username <user>        NTRIP username\n"
        "  --password <pass>        NTRIP password\n"
        "  --survey-in              Use survey-in base mode (default)\n"
        "  --fixed-lla <lat,lon,h>  Use fixed LLA position (h = WGS-84 HAE in meters)\n"
        "  --fixed-ecef <x,y,z>    Use fixed ECEF position\n"
        "  --accuracy <meters>      Position accuracy for fixed mode (default: 0.5)\n"
        "  --reset <cold|hot|none>  Receiver reset mode (default: cold)\n"
        "  --validate               Validate config and exit (no hardware access)\n"
        "  --service                Force service-mode output (no timestamps,\n"
        "                           journald priority prefixes, periodic summaries)\n"
        "  --no-service             Force interactive output (default when on a TTY)\n"
        "  --log-level <lvl>        error|warning|info|debug (default: info)\n"
        "  --log-interval <sec>     Summary interval in seconds\n"
        "                           (0 = auto: 1 interactive, 30 service)\n"
        "  --no-watchdog            Disable systemd watchdog pings\n"
        "  --help, -h               Show this help\n"
        "\n"
        "Config file is optional. CLI arguments override config file values.\n"
        "See example-rtk-base.toml for all available configuration options.\n",
        progname
    );
}

static bool parseCoordTriple(const char* str, double& a, double& b, double& c)
{
    return std::sscanf(str, "%lf,%lf,%lf", &a, &b, &c) == 3;
}

int main(int argc, char* argv[])
{
    std::setvbuf(stdout, nullptr, _IOLBF, 0);

    RtkBaseToolConfig cfg;
    bool validateOnly = false;

    // ── Track which CLI overrides were given ────────────────────────
    bool cliPort = false, cliMountpoint = false, cliHost = false;
    bool cliMode = false, cliReset = false, cliBase = false;
    bool cliAccuracy = false, cliUsername = false, cliPassword = false;

    std::string configPath;

    // ── First pass: find --config so we load TOML before overrides ──
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc)
        {
            configPath = argv[++i];
            break;
        }
    }

    // ── Load TOML config if given ───────────────────────────────────
    if (!configPath.empty())
    {
        try
        {
            cfg = loadConfigFromToml(configPath);
            std::printf("Loaded config from %s\n", configPath.c_str());
        }
        catch (const std::exception& e)
        {
            std::fprintf(stderr, "Error loading config: %s\n", e.what());
            return 1;
        }
    }

    // ── Second pass: CLI overrides ──────────────────────────────────
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc)
        {
            ++i;  // already handled
        }
        else if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc)
        {
            cfg.ntripPort = static_cast<uint16_t>(std::atoi(argv[++i]));
            cliPort = true;
        }
        else if (std::strcmp(argv[i], "--mountpoint") == 0 && i + 1 < argc)
        {
            cfg.ntripMountpoint = argv[++i];
            cliMountpoint = true;
        }
        else if (std::strcmp(argv[i], "--host") == 0 && i + 1 < argc)
        {
            cfg.ntripHost = argv[++i];
            cliHost = true;
        }
        else if (std::strcmp(argv[i], "--mode") == 0 && i + 1 < argc)
        {
            const std::string m = argv[++i];
            if (m == "caster")
                cfg.ntripMode = ENtripMode::Caster;
            else if (m == "server")
                cfg.ntripMode = ENtripMode::Server;
            else
            {
                std::fprintf(stderr, "Unknown NTRIP mode: %s (use 'caster' or 'server')\n", m.c_str());
                return 1;
            }
            cliMode = true;
        }
        else if (std::strcmp(argv[i], "--username") == 0 && i + 1 < argc)
        {
            cfg.ntripUsername = argv[++i];
            cliUsername = true;
        }
        else if (std::strcmp(argv[i], "--password") == 0 && i + 1 < argc)
        {
            cfg.ntripPassword = argv[++i];
            cliPassword = true;
        }
        else if (std::strcmp(argv[i], "--survey-in") == 0)
        {
            cfg.baseMode = "survey_in";
            cliBase = true;
        }
        else if (std::strcmp(argv[i], "--fixed-lla") == 0 && i + 1 < argc)
        {
            double lat, lon, h;
            if (!parseCoordTriple(argv[++i], lat, lon, h))
            {
                std::fprintf(stderr, "Invalid --fixed-lla format. Use: lat,lon,height\n");
                return 1;
            }
            cfg.baseMode = "fixed";
            cfg.fixedType = "lla";
            cfg.fixedLatitude_deg = lat;
            cfg.fixedLongitude_deg = lon;
            cfg.fixedHeight_m = h;
            cliBase = true;
        }
        else if (std::strcmp(argv[i], "--fixed-ecef") == 0 && i + 1 < argc)
        {
            double x, y, z;
            if (!parseCoordTriple(argv[++i], x, y, z))
            {
                std::fprintf(stderr, "Invalid --fixed-ecef format. Use: x,y,z\n");
                return 1;
            }
            cfg.baseMode = "fixed";
            cfg.fixedType = "ecef";
            cfg.fixedEcefX_m = x;
            cfg.fixedEcefY_m = y;
            cfg.fixedEcefZ_m = z;
            cliBase = true;
        }
        else if (std::strcmp(argv[i], "--accuracy") == 0 && i + 1 < argc)
        {
            cfg.fixedAccuracy_m = std::atof(argv[++i]);
            cliAccuracy = true;
        }
        else if (std::strcmp(argv[i], "--reset") == 0 && i + 1 < argc)
        {
            const std::string r = argv[++i];
            if (r == "cold")      cfg.resetMode = EResetMode::Cold;
            else if (r == "hot")  cfg.resetMode = EResetMode::Hot;
            else if (r == "none") cfg.resetMode = EResetMode::None;
            else
            {
                std::fprintf(stderr, "Unknown reset mode: %s (use 'cold', 'hot', or 'none')\n", r.c_str());
                return 1;
            }
            cliReset = true;
        }
        else if (std::strcmp(argv[i], "--validate") == 0)
        {
            validateOnly = true;
        }
        else if (std::strcmp(argv[i], "--service") == 0)
        {
            cfg.serviceMode = 1;
        }
        else if (std::strcmp(argv[i], "--no-service") == 0)
        {
            cfg.serviceMode = 0;
        }
        else if (std::strcmp(argv[i], "--no-watchdog") == 0)
        {
            cfg.watchdogEnabled = false;
        }
        else if (std::strcmp(argv[i], "--log-level") == 0 && i + 1 < argc)
        {
            const std::string l = argv[++i];
            if (l == "error")        cfg.logLevel = ENtripLogLevel::Error;
            else if (l == "warning") cfg.logLevel = ENtripLogLevel::Warning;
            else if (l == "info")    cfg.logLevel = ENtripLogLevel::Info;
            else if (l == "debug")   cfg.logLevel = ENtripLogLevel::Debug;
            else
            {
                std::fprintf(stderr, "Unknown log level: %s\n", l.c_str());
                return 1;
            }
        }
        else if (std::strcmp(argv[i], "--log-interval") == 0 && i + 1 < argc)
        {
            cfg.logIntervalSec = std::atoi(argv[++i]);
            if (cfg.logIntervalSec < 0)
            {
                std::fprintf(stderr, "Invalid --log-interval: must be >= 0\n");
                return 1;
            }
        }
        else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0)
        {
            printUsage(argv[0]);
            return 0;
        }
        else
        {
            std::fprintf(stderr, "Unknown option: %s\n", argv[i]);
            printUsage(argv[0]);
            return 1;
        }
    }

    // Suppress unused-variable warnings for future use
    (void)cliPort; (void)cliMountpoint; (void)cliHost; (void)cliMode;
    (void)cliReset; (void)cliBase; (void)cliAccuracy;
    (void)cliUsername; (void)cliPassword;

    // ── Resolve service mode + log level + interval ─────────────────
    g_serviceMode = (cfg.serviceMode == 1) ? true
                  : (cfg.serviceMode == 0) ? false
                  : detectServiceMode();
    g_logLevel = cfg.logLevel;

    // Keep NTRIP subsystem in sync with the global log level when the user
    // hasn't pinned ntrip_log_level explicitly (simple rule: take the looser).
    if (static_cast<int>(cfg.logLevel) > static_cast<int>(cfg.ntripLogLevel))
        cfg.ntripLogLevel = cfg.logLevel;

    const int logInterval = (cfg.logIntervalSec > 0)
        ? cfg.logIntervalSec
        : (g_serviceMode ? 30 : 1);

    // ── Print config summary ────────────────────────────────────────
    logLine(LogLvl::Info, "gnsshat-rtk-base starting (%s mode, log interval %ds)",
            g_serviceMode ? "service" : "interactive", logInterval);
    logLine(LogLvl::Info, "  Reset:      %s",
        cfg.resetMode == EResetMode::Cold ? "cold" :
        cfg.resetMode == EResetMode::Hot  ? "hot" : "none");
    logLine(LogLvl::Info, "  Base mode:  %s", cfg.baseMode.c_str());
    logLine(LogLvl::Info, "  NTRIP mode: %s on %s:%u/%s",
        cfg.ntripMode == ENtripMode::Caster ? "caster" : "server",
        cfg.ntripHost.c_str(), cfg.ntripPort, cfg.ntripMountpoint.c_str());

    if (validateOnly)
    {
        logLine(LogLvl::Info, "Configuration is valid.");
        return 0;
    }

    // ── Set up signal handlers ──────────────────────────────────────
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    // ── HAT pre-flight check ────────────────────────────────────────
    // IGnssHat::create() terminates on unknown HATs; we check first so
    // we can exit cleanly with a specific code (78 = EX_CONFIG) that the
    // unit file treats as "do not restart".
    static constexpr const char* kRequiredHat = "L1/L5 GNSS RTK HAT";
    static constexpr int kExitConfig = 78;

    const std::string detectedHat = Hat::detectProduct();
    if (detectedHat.empty())
    {
        logLine(LogLvl::Error,
                "No GNSS HAT detected at /proc/device-tree/hat/product "
                "(is the HAT seated and the device-tree overlay loaded?)");
        sdNotifyRaw("STATUS=No HAT detected");
        return kExitConfig;
    }
    if (detectedHat != kRequiredHat)
    {
        logLine(LogLvl::Error,
                "Unsupported HAT for RTK base: got '%s', required '%s'",
                detectedHat.c_str(), kRequiredHat);
        char notify[256];
        std::snprintf(notify, sizeof(notify),
                      "STATUS=Unsupported HAT: %s", detectedHat.c_str());
        sdNotifyRaw(notify);
        return kExitConfig;
    }
    logLine(LogLvl::Info, "HAT: %s", detectedHat.c_str());

    // ── Create and start GNSS HAT ───────────────────────────────────
    auto* hat = IGnssHat::create();
    if (!hat)
    {
        logLine(LogLvl::Error, "Failed to create GNSS HAT instance");
        return 1;
    }

    switch (cfg.resetMode)
    {
        case EResetMode::Cold:
            sdNotifyRaw("STATUS=Cold-resetting receiver...");
            hat->hardResetUbloxSom_ColdStart();
            break;
        case EResetMode::Hot:
            sdNotifyRaw("STATUS=Hot-resetting receiver...");
            hat->softResetUbloxSom_HotStart();
            break;
        case EResetMode::None: break;
    }

    sdNotifyRaw("STATUS=Configuring receiver...");
    const auto gnssConfig = cfg.buildGnssConfig();
    if (!hat->start(gnssConfig))
    {
        logLine(LogLvl::Error, "Failed to start GNSS");
        sdNotifyRaw("STATUS=Failed to configure receiver");
        delete hat;
        return 1;
    }
    logLine(LogLvl::Info, "GNSS started. Waiting for TimeOnlyFix...");
    sdNotifyRaw("STATUS=Waiting for base fix (TimeOnlyFix)...");

    // ── Start NTRIP ─────────────────────────────────────────────────
    NtripCaster* caster = nullptr;
    NtripServer* server = nullptr;

    if (cfg.ntripMode == ENtripMode::Caster)
    {
        caster = new NtripCaster(cfg.ntripHost, cfg.ntripPort,
                                 cfg.ntripMountpoint, cfg.ntripMaxClients);
        caster->setLogCallback(ntripLogPrinter);
        caster->setLogLevel(cfg.ntripLogLevel);

        if (!cfg.ntripUsername.empty())
            caster->setCredentials(cfg.ntripUsername, cfg.ntripPassword);

        if (cfg.ntripTlsEnabled && !cfg.ntripTlsCertFile.empty())
            caster->setTls(cfg.ntripTlsCertFile, cfg.ntripTlsKeyFile);

        if (!caster->start())
        {
            logLine(LogLvl::Error, "Failed to start NTRIP caster");
            delete caster;
            delete hat;
            return 1;
        }
        logLine(LogLvl::Info, "NTRIP caster started on %s:%u/%s",
                cfg.ntripHost.c_str(), cfg.ntripPort, cfg.ntripMountpoint.c_str());
    }
    else
    {
        server = new NtripServer(cfg.ntripHost, cfg.ntripPort,
                                 cfg.ntripMountpoint,
                                 cfg.ntripUsername, cfg.ntripPassword);
        server->setLogCallback(ntripLogPrinter);
        server->setLogLevel(cfg.ntripLogLevel);

        if (cfg.ntripAutoReconnect)
            server->setAutoReconnect(true, cfg.ntripReconnectInitialMs, cfg.ntripReconnectMaxMs);

        if (cfg.ntripTlsEnabled)
            server->setUseTls(true, cfg.ntripTlsVerifyPeer);

        if (!server->connect())
        {
            logLine(LogLvl::Error, "Failed to connect to %s:%u/%s",
                    cfg.ntripHost.c_str(), cfg.ntripPort, cfg.ntripMountpoint.c_str());
            delete server;
            delete hat;
            return 1;
        }
        logLine(LogLvl::Info, "Connected to NTRIP caster %s:%u/%s",
                cfg.ntripHost.c_str(), cfg.ntripPort, cfg.ntripMountpoint.c_str());
    }

    // ── Notify systemd we're up and configure watchdog ──────────────
    initWatchdog(cfg.watchdogEnabled);
    {
        char readyMsg[256];
        std::snprintf(readyMsg, sizeof(readyMsg),
                      "MAINPID=%ld\nREADY=1\nSTATUS=Serving RTCM on %s:%u/%s",
                      static_cast<long>(::getpid()),
                      cfg.ntripHost.c_str(), cfg.ntripPort,
                      cfg.ntripMountpoint.c_str());
        sdNotifyRaw(readyMsg);
    }
    if (g_watchdogUsec > 0)
    {
        logLine(LogLvl::Info, "systemd watchdog enabled (%.1fs interval)",
                (g_watchdogUsec / 2) / 1e6);
    }

    // ── Main loop (rate-limited) ────────────────────────────────────
    using Clock = std::chrono::steady_clock;
    const auto intervalMs = std::chrono::milliseconds(logInterval * 1000);
    const auto wdIntervalMs = (g_watchdogUsec > 0)
        ? std::chrono::milliseconds(static_cast<int64_t>(g_watchdogUsec / 2 / 1000))
        : std::chrono::milliseconds(0);
    auto lastSummary = Clock::now();
    auto lastWdPing  = Clock::now();

    EFixType prevFix = EFixType::NoFix;
    size_t   prevClientCount = static_cast<size_t>(-1);
    bool     firstRtcmSeen = false;

    // High-level phase, reflected in systemd STATUS= on every transition so
    // `systemctl status` is informative even between periodic summaries.
    enum class Phase { AwaitingFix, FixNoRtcm, Streaming, FixLost };
    auto phaseStr = [](Phase p) {
        switch (p) {
            case Phase::AwaitingFix: return "awaiting-fix";
            case Phase::FixNoRtcm:   return "fix-no-rtcm";
            case Phase::Streaming:   return "streaming";
            case Phase::FixLost:     return "fix-lost";
        }
        return "?";
    };
    Phase phase = Phase::AwaitingFix;
    bool phaseEmitted = false;
    auto setPhase = [&](Phase p, const char* detail)
    {
        if (phaseEmitted && p == phase) return;
        phase = p;
        phaseEmitted = true;
        char msg[192];
        std::snprintf(msg, sizeof(msg), "STATUS=%s: %s", phaseStr(p), detail);
        sdNotifyRaw(msg);
    };

    // Counters accumulated across the interval window.
    uint64_t winFrames = 0;
    uint64_t winBytes  = 0;
    uint32_t winEpochs = 0;
    uint32_t winNoFixEpochs = 0;
    size_t   winMaxClients = 0;

    while (g_running)
    {
        const auto nav = hat->waitAndGetFreshNavigation();
        const auto& pvt = nav.pvt;
        ++winEpochs;

        // Periodic watchdog ping (independent of summary emission).
        if (g_watchdogUsec > 0)
        {
            const auto now = Clock::now();
            if (now - lastWdPing >= wdIntervalMs)
            {
                pingWatchdog();
                lastWdPing = now;
            }
        }

        // Fix-type transitions — log immediately.
        if (pvt.fixType != prevFix)
        {
            logLine(LogLvl::Info, "Fix transition: %s -> %s",
                    Utils::eFixType2string(prevFix).c_str(),
                    Utils::eFixType2string(pvt.fixType).c_str());
            prevFix = pvt.fixType;
        }

        if (pvt.fixType != EFixType::TimeOnlyFix)
        {
            ++winNoFixEpochs;
            // Lost fix after we had one, otherwise still waiting.
            if (firstRtcmSeen || phase == Phase::FixNoRtcm)
                setPhase(Phase::FixLost, Utils::eFixType2string(pvt.fixType).c_str());
            else
                setPhase(Phase::AwaitingFix, Utils::eFixType2string(pvt.fixType).c_str());
        }
        else
        {
            auto corrections = hat->rtk()->base()->getFullCorrections();
            if (corrections.empty())
            {
                setPhase(Phase::FixNoRtcm, "base fix acquired, waiting for RTCM");
            }
            else
            {
                size_t thisBytes = 0;
                for (const auto& frame : corrections) thisBytes += frame.size();

                winFrames += corrections.size();
                winBytes  += thisBytes;

                if (!firstRtcmSeen)
                {
                    logLine(LogLvl::Info, "First RTCM3 frames ready (%zu)", corrections.size());
                    firstRtcmSeen = true;
                }
                setPhase(Phase::Streaming, caster ? "serving RTCM to clients"
                                                  : "pushing RTCM to upstream caster");

                if (caster)
                {
                    caster->feed(corrections);
                    caster->updatePosition(pvt.latitude, pvt.longitude);

                    const size_t cc = caster->clientCount();
                    if (cc > winMaxClients) winMaxClients = cc;
                    if (cc != prevClientCount)
                    {
                        if (prevClientCount != static_cast<size_t>(-1))
                            logLine(LogLvl::Info, "NTRIP clients: %zu -> %zu", prevClientCount, cc);
                        prevClientCount = cc;
                    }
                }
                else if (server)
                {
                    server->feed(corrections);
                }
            }
        }

        // Periodic summary.
        const auto now = Clock::now();
        if (now - lastSummary >= intervalMs)
        {
            const double windowSec = std::chrono::duration<double>(now - lastSummary).count();

            // Pull the latest snapshot for fix/position context.
            const std::string fixStr = Utils::eFixType2string(pvt.fixType);

            char posBuf[96];
            if (pvt.fixType == EFixType::NoFix)
            {
                std::snprintf(posBuf, sizeof(posBuf), "pos=n/a");
            }
            else
            {
                // alt_hae = height above WGS-84 ellipsoid (what u-blox CFG-TMODE3
                //           LLA mode expects, and what to paste into
                //           [base.fixed_position] height_m after a survey-in).
                // alt_msl = height above mean sea level (geoid), for humans.
                std::snprintf(posBuf, sizeof(posBuf),
                              "pos=%.7f,%.7f alt_hae=%.2fm alt_msl=%.2fm "
                              "hAcc=%.2fm sats=%u",
                              pvt.latitude, pvt.longitude,
                              static_cast<double>(pvt.altitude),
                              static_cast<double>(pvt.altitudeMSL),
                              static_cast<double>(pvt.horizontalAccuracy),
                              static_cast<unsigned>(pvt.visibleSatellites));
            }

            char status[384];
            if (caster)
            {
                std::snprintf(status, sizeof(status),
                              "%s: fix=%s %s frames=%llu bytes=%llu clients=%zu "
                              "no_fix_epochs=%u/%u in %.1fs",
                              phaseStr(phase),
                              fixStr.c_str(), posBuf,
                              (unsigned long long)winFrames,
                              (unsigned long long)winBytes,
                              caster->clientCount(),
                              winNoFixEpochs, winEpochs, windowSec);
            }
            else
            {
                auto st = server->getStats();
                std::snprintf(status, sizeof(status),
                              "%s: fix=%s %s frames=%llu bytes=%llu total_tx=%lluB "
                              "uptime=%.1fs no_fix_epochs=%u/%u in %.1fs",
                              phaseStr(phase),
                              fixStr.c_str(), posBuf,
                              (unsigned long long)winFrames,
                              (unsigned long long)winBytes,
                              (unsigned long long)st.bytesTx,
                              st.uptimeMs / 1000.0,
                              winNoFixEpochs, winEpochs, windowSec);
            }

            logLine(LogLvl::Info, "stats: %s", status);

            // Reflect latest state in systemd STATUS=.
            char notify[448];
            std::snprintf(notify, sizeof(notify), "STATUS=%s", status);
            sdNotifyRaw(notify);

            winFrames = winBytes = 0;
            winEpochs = winNoFixEpochs = 0;
            winMaxClients = 0;
            lastSummary = now;
        }
    }

    // ── Cleanup ─────────────────────────────────────────────────────
    sdNotifyRaw("STOPPING=1\nSTATUS=Shutting down");
    logLine(LogLvl::Info, "Shutting down...");

    if (caster)
    {
        caster->stop();
        delete caster;
    }
    if (server)
    {
        auto stats = server->getStats();
        logLine(LogLvl::Info, "Session: %llu bytes TX, %llu frames, %.1fs uptime",
                static_cast<unsigned long long>(stats.bytesTx),
                static_cast<unsigned long long>(stats.framesTx),
                stats.uptimeMs / 1000.0);
        server->disconnect();
        delete server;
    }

    delete hat;
    logLine(LogLvl::Info, "Done.");
    sdNotifyRaw("STATUS=Stopped");
    return 0;
}

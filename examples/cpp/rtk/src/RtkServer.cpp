/*
 * Jimmy Paputto 2025
 *
 * NtripServer example — push RTCM3 corrections from a local GNSS
 * base station to a remote NTRIP caster.
 *
 * Build:
 *   cmake -DBUILD_EXAMPLES=ON ..
 *   make RtkServer
 *
 * Usage:
 *   ./RtkServer <caster_host> <port> <mountpoint> [username] [password]
 */

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <thread>

#include <jimmypaputto/GnssHat.hpp>
#include <jimmypaputto/ntrip/NtripServer.hpp>

using namespace JimmyPaputto;

static std::atomic<bool> g_running{true};

static void signalHandler(int) { g_running = false; }

int main(int argc, char* argv[])
{
    if (argc < 4)
    {
        std::fprintf(stderr,
            "Usage: %s <caster_host> <port> <mountpoint> [username] [password]\n",
            argv[0]);
        return 1;
    }

    const char* host     = argv[1];
    uint16_t    port     = static_cast<uint16_t>(std::atoi(argv[2]));
    const char* mount    = argv[3];
    const char* username = (argc > 4) ? argv[4] : "";
    const char* password = (argc > 5) ? argv[5] : "";

    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    // ── Configure GNSS as Base ──────────────────────────────────────
    GnssConfig config {
        .measurementRate_Hz = 1,
        .dynamicModel = EDynamicModel::Stationary,
        .timepulsePinConfig = TimepulsePinConfig {
            .active = true,
            .fixedPulse = TimepulsePinConfig::Pulse { 1, 0.1 },
            .pulseWhenNoFix = std::nullopt,
            .polarity = ETimepulsePinPolarity::RisingEdgeAtTopOfSecond
        },
        .geofencing = std::nullopt,
        .rtk = RtkConfig {
            .mode = ERtkMode::Base,
            .base = BaseConfig {
                .mode = BaseConfig::SurveyIn {
                    .minimumObservationTime_s = 120,
                    .requiredPositionAccuracy_m = 50.0
                }
            }
        }
    };

    auto* hat = IGnssHat::create();
    if (!hat)
    {
        std::fprintf(stderr, "Failed to create GNSS HAT instance\n");
        return 1;
    }

    hat->hardResetUbloxSom_ColdStart();
    if (!hat->start(config))
    {
        std::fprintf(stderr, "Failed to start GNSS\n");
        return 1;
    }

    std::printf("GNSS started. Waiting for TimeOnlyFix...\n");

    // ── Connect NtripServer to remote caster ────────────────────────
    NtripServer server(host, port, mount, username, password);
    server.setLogLevel(ENtripLogLevel::Info);
    server.setAutoReconnect(true, 1000, 30000);

    if (!server.connect())
    {
        std::fprintf(stderr, "Failed to connect to %s:%u/%s\n",
                     host, port, mount);
        return 1;
    }

    std::printf("Connected to %s:%u/%s — pushing corrections\n",
                host, port, mount);

    // ── Main loop ───────────────────────────────────────────────────
    while (g_running)
    {
        const auto nav = hat->waitAndGetFreshNavigation();
        if (nav.pvt.fixType != EFixType::TimeOnlyFix)
        {
            std::printf("Waiting for TimeOnlyFix...\n");
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        auto corrections = hat->rtk()->base()->getFullCorrections();
        if (!corrections.empty())
            server.feed(corrections);

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    auto stats = server.getStats();
    std::printf("\nSession: %llu bytes TX, %llu frames, %.1fs uptime\n",
                static_cast<unsigned long long>(stats.bytesTx),
                static_cast<unsigned long long>(stats.framesTx),
                stats.uptimeMs / 1000.0);

    server.disconnect();
    delete hat;
    std::printf("Disconnected.\n");
    return 0;
}

/*
 * Jimmy Paputto 2026
 *
 * RTK Rover example (C++)
 *
 * Demonstrates configuring the GNSS module as an RTK Rover
 * with an integrated NTRIP client that receives RTCM3
 * corrections from a caster and applies them to the receiver.
 *
 * Usage: ./RtkRover [--host HOST] [--port PORT]
 *                   [--mountpoint MP] [--user U] [--password P]
 */

#include <chrono>
#include <cstdio>
#include <cstring>
#include <stop_token>
#include <thread>

#include <jimmypaputto/GnssHat.hpp>
#include <jimmypaputto/ntrip/NtripClient.hpp>

using namespace JimmyPaputto;


GnssConfig createConfig()
{
    return GnssConfig {
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
            .mode = ERtkMode::Rover
        }
    };
}


/*
 * Correction application thread.
 *
 * Receives RTCM3 frames from the NTRIP client and forwards
 * them to the receiver via rtk()->rover()->applyCorrections().
 */
void correctionsThread(std::stop_token stoken, IGnssHat* hat,
                       NtripClient* client)
{
    auto* rtk = hat->rtk();
    if (!rtk)
    {
        fprintf(stderr, "RTK not available on this HAT\r\n");
        return;
    }

    while (!stoken.stop_requested())
    {
        if (!client->isConnected())
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        auto frames = client->receiveFrames();
        if (!frames.empty())
            rtk->rover()->applyCorrections(frames);
        else
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}


auto main(int argc, char* argv[]) -> int
{
    // Defaults
    std::string host = "localhost";
    uint16_t port = 2101;
    std::string mountpoint = "GNSS_HAT";
    std::string user;
    std::string password;

    for (int i = 1; i < argc - 1; i += 2)
    {
        if (strcmp(argv[i], "--host") == 0)       host = argv[i + 1];
        else if (strcmp(argv[i], "--port") == 0)  port = static_cast<uint16_t>(atoi(argv[i + 1]));
        else if (strcmp(argv[i], "--mountpoint") == 0) mountpoint = argv[i + 1];
        else if (strcmp(argv[i], "--user") == 0)  user = argv[i + 1];
        else if (strcmp(argv[i], "--password") == 0) password = argv[i + 1];
    }

    auto* ubxHat = IGnssHat::create();
    if (!ubxHat)
    {
        printf("Failed to create GNSS HAT instance\r\n");
        return -1;
    }

    ubxHat->softResetUbloxSom_HotStart();
    const bool isStartupDone = ubxHat->start(createConfig());
    if (!isStartupDone)
    {
        printf("Failed to start GNSS\r\n");
        return -1;
    }
    printf("GNSS started as RTK Rover\r\n");

    NtripClient client(host, port, mountpoint, user, password);
    if (!client.connect())
    {
        printf("Failed to connect to NTRIP caster %s:%u/%s\r\n",
               host.c_str(), port, mountpoint.c_str());
        delete ubxHat;
        return -1;
    }

    std::jthread corrThread(correctionsThread, ubxHat, &client);

    while (true)
    {
        const auto nav = ubxHat->waitAndGetFreshNavigation();
        const auto& pvt = nav.pvt;
        printf("[%s] %s (%s)  %.6f, %.6f  alt=%.1fm  sats=%d\r\n",
            Utils::utcTimeFromGnss_ISO8601(pvt).c_str(),
            Utils::eFixQuality2string(pvt.fixQuality).c_str(),
            Utils::eFixType2string(pvt.fixType).c_str(),
            pvt.latitude, pvt.longitude,
            pvt.altitude, pvt.visibleSatellites
        );
    }

    corrThread.request_stop();
    client.disconnect();
    delete ubxHat;

    return 0;
}

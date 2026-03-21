/*
 * Jimmy Paputto 2025
 *
 * RTK Rover example (C++)
 *
 * Demonstrates configuring the GNSS module as an RTK Rover.
 * The corrections thread shows where RTCM3 data from an NTRIP
 * caster should be applied to the receiver.
 *
 * NOTE: A full NTRIP client implementation is available in the
 * Python example (examples/Python/rtk_rover.py) using pygnssutils.
 * A native C++ NTRIP client is under development and will be
 * added here in a future release.
 */

#include <chrono>
#include <cstdio>
#include <stop_token>
#include <thread>

#include <jimmypaputto/GnssHat.hpp>

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
 * In a real setup this thread would:
 *   1. Connect to an NTRIP caster (host, port, mountpoint, credentials)
 *   2. Receive a stream of RTCM3 frames
 *   3. Forward them to the receiver via rtk()->rover()->applyCorrections()
 *
 * Below is the skeleton — replace the TODO section with your NTRIP
 * client code or pipe data from an external source.
 */
void correctionsThread(std::stop_token stoken, IGnssHat* hat)
{
    auto* rtk = hat->rtk();
    if (!rtk)
    {
        fprintf(stderr, "RTK not available on this HAT\r\n");
        return;
    }

    while (!stoken.stop_requested())
    {
        // TODO: Receive RTCM3 frames from your NTRIP caster here.
        //
        // Example (pseudocode):
        //
        //   auto frames = ntripClient.receiveFrames();
        //   if (!frames.empty())
        //       rtk->rover()->applyCorrections(frames);
        //

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}


auto main() -> int
{
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

    std::jthread corrThread(correctionsThread, ubxHat);

    while (true)
    {
        const auto pvt = ubxHat->waitAndGetFreshNavigation().pvt;
        const auto fixQuality = pvt.fixQuality;
        const auto fixType = pvt.fixType;
        printf("[%s] Fix Quality: %s, Fix Type: %s\r\n",
            Utils::utcTimeFromGnss_ISO8601(pvt).c_str(),
            Utils::eFixQuality2string(fixQuality).c_str(),
            Utils::eFixType2string(fixType).c_str()
        );
    }

    corrThread.request_stop();
    delete ubxHat;

    return 0;
}

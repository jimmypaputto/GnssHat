/*
 * Jimmy Paputto 2025
 */

#include <cstdio>

#include <jimmypaputto/GnssHat.hpp>

#define PULSE_RATE_HZ 5


using namespace JimmyPaputto;

GnssConfig createDefaultConfig()
{
    const auto pulse = TimepulsePinConfig::Pulse {
        PULSE_RATE_HZ, 0.1
    };
    return GnssConfig {
        .measurementRate_Hz = PULSE_RATE_HZ,
        .dynamicModel = EDynamicModel::Stationary,
        .timepulsePinConfig = TimepulsePinConfig {
            .active = true,
            .fixedPulse = pulse,
            .pulseWhenNoFix = pulse,
            .polarity = ETimepulsePinPolarity::RisingEdgeAtTopOfSecond
        },
        .geofencing = std::nullopt
    };
}

auto main() -> int
{
    auto* ubxHat = IGnssHat::create();
    ubxHat->hardResetUbloxSom_ColdStart();
    const bool isStartupDone = ubxHat->start(createDefaultConfig());
    if (!isStartupDone)
    {
        printf("Startup failed, exit\r\n");
        return -1;
    }

    printf("Startup done, ublox configured\r\n");

    while (true)
    {
        ubxHat->timepulse();
        const auto time = ubxHat->navigation().pvt.utc;
        static uint32_t counter = 0;
        printf("Timepulse: %u, %02d:%02d:%02d\r\n",
            counter++, time.hh, time.mm, time.ss);
    }

    return 0;
}

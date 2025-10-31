/*
 * Jimmy Paputto 2025
 */

#include <chrono>
#include <cstdio>
#include <thread>

#include <jimmypaputto/GnssHat.hpp>


JimmyPaputto::GnssConfig createConfig()
{
    return JimmyPaputto::GnssConfig {
        .measurementRate_Hz = 1,
        .dynamicModel = JimmyPaputto::EDynamicModel::Stationary,
        .timepulsePinConfig = JimmyPaputto::TimepulsePinConfig {
            .active = true,
            .fixedPulse = JimmyPaputto::TimepulsePinConfig::Pulse { 1, 0.1 },
            .pulseWhenNoFix = std::nullopt,
            .polarity = JimmyPaputto::ETimepulsePinPolarity::RisingEdgeAtTopOfSecond
        },
        .geofencing = std::nullopt,
        .rtk = JimmyPaputto::RtkConfig {
            .mode = JimmyPaputto::ERtkMode::Rover
        }
    };
}

auto main() -> int
{
    auto* ubxHat = JimmyPaputto::IGnssHat::create();
    if (!ubxHat)
    {
        printf("Failed to create GNSS HAT instance\r\n");
        return -1;
    }

    ubxHat->hardResetUbloxSom_ColdStart();
    const bool isStartupDone = ubxHat->start(createConfig());
    if (!isStartupDone)
    {
        printf("Failed to start GNSS\r\n");
        return -1;
    }
    printf("GNSS started successfully. Monitoring navigation data...\r\n");

    while (true)
    {
        const auto pvt = ubxHat->waitAndGetFreshNavigation().pvt;
        const auto fixQuality = pvt.fixQuality;
        const auto fixType = pvt.fixType;
        printf("[%s] Fix Quality: %s, Fix Type: %s\r\n",
            JimmyPaputto::Utils::utcTimeFromGnss_ISO8601(pvt).c_str(),
            JimmyPaputto::Utils::eFixQuality2string(fixQuality).c_str(),
            JimmyPaputto::Utils::eFixType2string(fixType).c_str()
        );
    }

    return 0;
}

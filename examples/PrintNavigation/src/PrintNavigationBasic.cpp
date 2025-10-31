/*
 * Jimmy Paputto 2025
 */

#include <chrono>
#include <cstdio>
#include <thread>

#include <jimmypaputto/GnssHat.hpp>


void printNavigation(const JimmyPaputto::Navigation& navigation)
{
    printf("GNSS Navigation Data:\r\n");
    printf("\tFix Quality: %s\r\n", JimmyPaputto::Utils::eFixQuality2string(navigation.pvt.fixQuality).c_str());
    printf("\tFix Status: %s\r\n", JimmyPaputto::Utils::eFixStatus2string(navigation.pvt.fixStatus).c_str());
    printf("\tFix Type: %s\r\n", JimmyPaputto::Utils::eFixType2string(navigation.pvt.fixType).c_str());
    printf("\tVisible Satellites: %d\n", navigation.pvt.visibleSatellites);
    printf("\tLatitude: %.6f°\n", navigation.pvt.latitude);
    printf("\tLongitude: %.6f°\n", navigation.pvt.longitude);
    printf("\tAltitude: %.2f m\n", navigation.pvt.altitude);
    printf("\tTime: %s\n", JimmyPaputto::Utils::utcTimeFromGnss_ISO8601(navigation.pvt).c_str());
    printf("\tTime accuracy: %d nanoseconds\n", navigation.pvt.utc.accuracy);
    printf("\tDate valid: %s\n", navigation.pvt.date.valid ? "true" : "false");
}

JimmyPaputto::GnssConfig createDefaultConfig()
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
        .rtk = std::nullopt
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

    ubxHat->softResetUbloxSom_HotStart();
    const bool isStartupDone = ubxHat->start(createDefaultConfig());
    if (!isStartupDone)
    {
        printf("Failed to start GNSS\r\n");
        return -1;
    }
    printf("GNSS started successfully. Monitoring navigation data...\r\n");

    while (true)
    {
        const auto navigation = ubxHat->waitAndGetFreshNavigation();
        printNavigation(navigation);
    }

    return 0;
}

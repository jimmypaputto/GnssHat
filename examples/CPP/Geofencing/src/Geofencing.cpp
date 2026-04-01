/*
 * Jimmy Paputto 2025
 */

#include <cstdio>

#include <jimmypaputto/GnssHat.hpp>


using namespace JimmyPaputto;

/*
 * Geofencing confidence levels map:
 * 0 - no confidence required
 * 1 - 68%
 * 2 - 95%
 * 3 - 99.7%
 * 4 - 99.99%
 * 5 - 99.9999%
 */

GnssConfig createDefaultConfig()
{
    const auto geofencing = GnssConfig::Geofencing {
        .geofences = std::vector<Geofence> {
            Geofence {
                .lat = 41.902205071091224,
                .lon = 12.4539203390548,
                .radius = 2005
            },
            Geofence {
                .lat = 52.257211745024186,
                .lon = 20.311759615806704,
                .radius = 1810
            }
        },
        .confidenceLevel = 3,
        .pioPinPolarity = EPioPinPolarity::LowMeansOutside
    };

    return GnssConfig {
        .measurementRate_Hz = 1,
        .dynamicModel = EDynamicModel::Stationary,
        .timepulsePinConfig = TimepulsePinConfig {
            .active = true,
            .fixedPulse = TimepulsePinConfig::Pulse { 1, 0.1 },
            .pulseWhenNoFix = std::nullopt,
            .polarity =
                ETimepulsePinPolarity::RisingEdgeAtTopOfSecond
        },
        .geofencing = geofencing
    };
}

void print(const Geofencing& geofencing)
{
    printf("Geofencing:\r\n");
    printf("  Configuration:\r\n");
    const auto confidenceLevel2str = [](const uint8_t l) -> const char*
    {
        switch (l)
        {
            case 0: return "No confidence required";
            case 1: return "68%";
            case 2: return "95%";
            case 3: return "99.7%";
            case 4: return "99.99%";
            case 5: return "99.9999%";
        }
        return "Fatal error";
    };
    printf("    Confidence level: %s\r\n",
        confidenceLevel2str(geofencing.cfg.confidenceLevel));
    printf("    Geofences:\r\n");
    for (const auto& geofence : geofencing.cfg.geofences)
    {
        printf("      Latitude: %f, Longitude: %f, Radius: %.2f m\r\n",
            geofence.lat, geofence.lon, geofence.radius);
    }

    printf("  Navigation:\r\n");

    const auto geofencingStatus2str =
        [](const EGeofencingStatus s) -> const char*
        {
            switch (s)
            {
                case EGeofencingStatus::NotAvalaible:
                    return "NotAvalaible";
                case EGeofencingStatus::Active:
                    return "Active";
            }
            return "Fatal error";
        };

    printf("    Geofencing status: %s\r\n",
        geofencingStatus2str(geofencing.nav.geofencingStatus));
    printf("    Number of geofences: %d\r\n", geofencing.nav.numberOfGeofences);

    const auto geofenceStatus2str =
        [](const EGeofenceStatus s) -> const char*
        {
            switch (s)
            {
                case EGeofenceStatus::Unknown: return "Unknown";
                case EGeofenceStatus::Inside: return "Inside";
                case EGeofenceStatus::Outside: return "Outside";
            }
            return "Fatal error";
        };

    printf("    Combined state: %s\r\n",
        geofenceStatus2str(geofencing.nav.combinedState));
    for (int8_t i = 0; i < geofencing.nav.numberOfGeofences; ++i)
    {
        printf("      Geofence no %d: %s\r\n",
            i+1, geofenceStatus2str(geofencing.nav.geofencesStatus[i]));
    }
}

auto main() -> int
{
    auto* ubxHat = IGnssHat::create();
    ubxHat->softResetUbloxSom_HotStart();
    const bool isStartupDone = ubxHat->start(createDefaultConfig());
    if (!isStartupDone)
    {
        printf("Startup failed, exit\r\n");
        return -1;
    }

    printf("Startup done, ublox configured\r\n");

    while (true)
    {
        const auto geofencing = ubxHat->waitAndGetFreshNavigation().geofencing;
        print(geofencing);
    }

    return 0;
}

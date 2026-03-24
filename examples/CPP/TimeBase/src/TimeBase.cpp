/*
 * Jimmy Paputto 2026
 *
 * Time Base example for L1/L5 GNSS TIME HAT (NEO-F10T)
 *
 * Demonstrates configuring the timing module in "time base" mode
 * using either survey-in or fixed known position (a priori).
 * This allows the F10T to achieve better time precision by entering
 * a "time fix" mode (TimeOnlyFix) similar to an RTK base station.
 */

#include <cstdio>

#include <jimmypaputto/GnssHat.hpp>

using namespace JimmyPaputto;


GnssConfig createSurveyInConfig()
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
        .rtk = std::nullopt,
        .timeBase = BaseConfig {
            .mode = BaseConfig::SurveyIn {
                .minimumObservationTime_s = 120,
                .requiredPositionAccuracy_m = 50.0
            }
        }
    };
}

GnssConfig createFixedPositionConfig_Lla()
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
        .rtk = std::nullopt,
        .timeBase = BaseConfig {
            .mode = BaseConfig::FixedPosition {
                .position = BaseConfig::FixedPosition::Lla {
                    .latitude_deg  = 52.232222222,
                    .longitude_deg = 21.008055556,
                    .height_m      = 110.0
                },
                .positionAccuracy_m = 0.5
            }
        }
    };
}

GnssConfig createFixedPositionConfig_Ecef()
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
        .rtk = std::nullopt,
        .timeBase = BaseConfig {
            .mode = BaseConfig::FixedPosition {
                .position = BaseConfig::FixedPosition::Ecef {
                    .x_m = 3656215.987,
                    .y_m = 1409547.654,
                    .z_m = 5049982.321
                },
                .positionAccuracy_m = 0.5
            }
        }
    };
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
    const bool isStartupDone = ubxHat->start(createSurveyInConfig());
    if (!isStartupDone)
    {
        printf("Failed to start GNSS\r\n");
        return -1;
    }
    printf("GNSS started with time base configuration.\r\n");
    printf("Waiting for TimeOnlyFix...\r\n");

    while (true)
    {
        const auto pvt = ubxHat->waitAndGetFreshNavigation().pvt;
        const auto time = Utils::utcTimeFromGnss_ISO8601(pvt);
        printf(
            "[%s] Fix: %s  tAcc: %d ns  Lat: %.7f  Lon: %.7f  Alt: %.2f m\r\n",
            time.c_str(),
            Utils::eFixType2string(pvt.fixType).c_str(),
            pvt.utc.accuracy,
            pvt.latitude,
            pvt.longitude,
            pvt.altitudeMSL
        );
    }

    return 0;
}

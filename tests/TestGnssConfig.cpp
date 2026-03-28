#include <gtest/gtest.h>

#include "ublox/GnssConfig.hpp"


using namespace JimmyPaputto;

TEST(MeasurementRate, ValidRanges)
{
    EXPECT_TRUE(checkMeasurmentRate(1));
    EXPECT_TRUE(checkMeasurmentRate(10));
    EXPECT_TRUE(checkMeasurmentRate(25));
}

TEST(MeasurementRate, InvalidValues)
{
    EXPECT_FALSE(checkMeasurmentRate(0));
    EXPECT_FALSE(checkMeasurmentRate(26));
    EXPECT_FALSE(checkMeasurmentRate(100));
}

TEST(GeofencingValidation, NulloptIsValid)
{
    EXPECT_TRUE(checkGeofencing(std::nullopt));
}

TEST(GeofencingValidation, EmptyGeofencesAreValid)
{
    GnssConfig::Geofencing gf { .geofences = {}, .confidenceLevel = 3 };
    EXPECT_TRUE(checkGeofencing(gf));
}

TEST(GeofencingValidation, TooHighConfidenceLevel)
{
    GnssConfig::Geofencing gf { .geofences = {}, .confidenceLevel = 6 };
    EXPECT_FALSE(checkGeofencing(gf));
}

TEST(GeofencingValidation, MaxConfidenceLevelIsValid)
{
    GnssConfig::Geofencing gf { .geofences = {}, .confidenceLevel = 5 };
    EXPECT_TRUE(checkGeofencing(gf));
}

TEST(GeofencingValidation, TooManyGeofences)
{
    std::vector<Geofence> fences(5, { .lat = 0, .lon = 0, .radius = 100 });
    GnssConfig::Geofencing gf { .geofences = fences, .confidenceLevel = 3 };
    EXPECT_FALSE(checkGeofencing(gf));
}

TEST(GeofencingValidation, FourGeofencesAreValid)
{
    std::vector<Geofence> fences(4, { .lat = 45.0f, .lon = 12.0f, .radius = 500.0f });
    GnssConfig::Geofencing gf { .geofences = fences, .confidenceLevel = 3 };
    EXPECT_TRUE(checkGeofencing(gf));
}

TEST(GeofencingValidation, InvalidLatitude)
{
    GnssConfig::Geofencing gf {
        .geofences = {{ .lat = 91.0f, .lon = 0, .radius = 100 }},
        .confidenceLevel = 3
    };
    EXPECT_FALSE(checkGeofencing(gf));

    gf.geofences[0].lat = -91.0f;
    EXPECT_FALSE(checkGeofencing(gf));
}

TEST(GeofencingValidation, InvalidLongitude)
{
    GnssConfig::Geofencing gf {
        .geofences = {{ .lat = 0, .lon = 181.0f, .radius = 100 }},
        .confidenceLevel = 3
    };
    EXPECT_FALSE(checkGeofencing(gf));

    gf.geofences[0].lon = -181.0f;
    EXPECT_FALSE(checkGeofencing(gf));
}

TEST(GeofencingValidation, ZeroRadius)
{
    GnssConfig::Geofencing gf {
        .geofences = {{ .lat = 45.0f, .lon = 12.0f, .radius = 0.0f }},
        .confidenceLevel = 3
    };
    EXPECT_FALSE(checkGeofencing(gf));
}

TEST(GeofencingValidation, NegativeRadius)
{
    GnssConfig::Geofencing gf {
        .geofences = {{ .lat = 45.0f, .lon = 12.0f, .radius = -10.0f }},
        .confidenceLevel = 3
    };
    EXPECT_FALSE(checkGeofencing(gf));
}

TEST(GeofencingValidation, BoundaryCoordinates)
{
    GnssConfig::Geofencing gf {
        .geofences = {{ .lat = 90.0f, .lon = 180.0f, .radius = 1.0f }},
        .confidenceLevel = 0
    };
    EXPECT_TRUE(checkGeofencing(gf));

    gf.geofences[0] = { .lat = -90.0f, .lon = -180.0f, .radius = 0.01f };
    EXPECT_TRUE(checkGeofencing(gf));
}

TEST(TimepulseValidation, InactiveIsAlwaysValid)
{
    TimepulsePinConfig cfg {
        .active = false,
        .fixedPulse = { .frequency = 1, .pulseWidth = 1.5f },
        .polarity = ETimepulsePinPolarity::RisingEdgeAtTopOfSecond
    };
    EXPECT_TRUE(checkTimepulsePinConfig(cfg));
}

TEST(TimepulseValidation, ValidPulseWidth)
{
    TimepulsePinConfig cfg {
        .active = true,
        .fixedPulse = { .frequency = 1, .pulseWidth = 0.5f },
        .polarity = ETimepulsePinPolarity::RisingEdgeAtTopOfSecond
    };
    EXPECT_TRUE(checkTimepulsePinConfig(cfg));
}

TEST(TimepulseValidation, PulseWidthTooHigh)
{
    TimepulsePinConfig cfg {
        .active = true,
        .fixedPulse = { .frequency = 1, .pulseWidth = 1.0f },
        .polarity = ETimepulsePinPolarity::RisingEdgeAtTopOfSecond
    };
    EXPECT_FALSE(checkTimepulsePinConfig(cfg));
}

TEST(TimepulseValidation, PulseWidthNegative)
{
    TimepulsePinConfig cfg {
        .active = true,
        .fixedPulse = { .frequency = 1, .pulseWidth = -0.1f },
        .polarity = ETimepulsePinPolarity::RisingEdgeAtTopOfSecond
    };
    EXPECT_FALSE(checkTimepulsePinConfig(cfg));
}

TEST(TimepulseValidation, PulseWhenNoFixValid)
{
    TimepulsePinConfig cfg {
        .active = true,
        .fixedPulse = { .frequency = 1, .pulseWidth = 0.5f },
        .pulseWhenNoFix = TimepulsePinConfig::Pulse { .frequency = 1, .pulseWidth = 0.3f },
        .polarity = ETimepulsePinPolarity::RisingEdgeAtTopOfSecond
    };
    EXPECT_TRUE(checkTimepulsePinConfig(cfg));
}

TEST(TimepulseValidation, PulseWhenNoFixInvalid)
{
    TimepulsePinConfig cfg {
        .active = true,
        .fixedPulse = { .frequency = 1, .pulseWidth = 0.5f },
        .pulseWhenNoFix = TimepulsePinConfig::Pulse { .frequency = 1, .pulseWidth = 1.0f },
        .polarity = ETimepulsePinPolarity::RisingEdgeAtTopOfSecond
    };
    EXPECT_FALSE(checkTimepulsePinConfig(cfg));
}

TEST(TimepulseValidation, ZeroPulseWidthIsValid)
{
    TimepulsePinConfig cfg {
        .active = true,
        .fixedPulse = { .frequency = 10, .pulseWidth = 0.0f },
        .polarity = ETimepulsePinPolarity::FallingEdgeAtTopOfSecond
    };
    EXPECT_TRUE(checkTimepulsePinConfig(cfg));
}

TEST(TimingValidation, NulloptIsValid)
{
    EXPECT_TRUE(checkTiming(std::nullopt));
}

TEST(TimingValidation, BothDisabledFails)
{
    TimingConfig cfg {
        .enableTimeMark = false,
        .timeBase = std::nullopt
    };
    EXPECT_FALSE(checkTiming(cfg));
}

TEST(TimingValidation, EnableTimeMarkOnlyIsValid)
{
    TimingConfig cfg {
        .enableTimeMark = true,
        .timeBase = std::nullopt
    };
    EXPECT_TRUE(checkTiming(cfg));
}

TEST(TimingValidation, SurveyInValid)
{
    BaseConfig cfg {
        .mode = BaseConfig::SurveyIn {
            .minimumObservationTime_s = 120,
            .requiredPositionAccuracy_m = 50.0
        }
    };
    EXPECT_TRUE(checkBaseConfig(cfg));
}

TEST(TimingValidation, SurveyInZeroTimeFails)
{
    BaseConfig cfg {
        .mode = BaseConfig::SurveyIn {
            .minimumObservationTime_s = 0,
            .requiredPositionAccuracy_m = 50.0
        }
    };
    EXPECT_FALSE(checkBaseConfig(cfg));
}

TEST(TimingValidation, SurveyInZeroAccuracyFails)
{
    BaseConfig cfg {
        .mode = BaseConfig::SurveyIn {
            .minimumObservationTime_s = 120,
            .requiredPositionAccuracy_m = 0.0
        }
    };
    EXPECT_FALSE(checkBaseConfig(cfg));
}

TEST(TimingValidation, SurveyInNegativeAccuracyFails)
{
    BaseConfig cfg {
        .mode = BaseConfig::SurveyIn {
            .minimumObservationTime_s = 120,
            .requiredPositionAccuracy_m = -1.0
        }
    };
    EXPECT_FALSE(checkBaseConfig(cfg));
}

TEST(TimingValidation, FixedPositionLlaValid)
{
    BaseConfig cfg {
        .mode = BaseConfig::FixedPosition {
            .position = BaseConfig::FixedPosition::Lla {
                .latitude_deg = 52.232222,
                .longitude_deg = 21.008055,
                .height_m = 110.0
            },
            .positionAccuracy_m = 0.5
        }
    };
    EXPECT_TRUE(checkBaseConfig(cfg));
}

TEST(TimingValidation, FixedPositionEcefValid)
{
    BaseConfig cfg {
        .mode = BaseConfig::FixedPosition {
            .position = BaseConfig::FixedPosition::Ecef {
                .x_m = 3656215.987,
                .y_m = 1409547.654,
                .z_m = 5049982.321
            },
            .positionAccuracy_m = 0.5
        }
    };
    EXPECT_TRUE(checkBaseConfig(cfg));
}

TEST(TimingValidation, FixedPositionZeroAccuracyFails)
{
    BaseConfig cfg {
        .mode = BaseConfig::FixedPosition {
            .position = BaseConfig::FixedPosition::Ecef {
                .x_m = 3656215.987,
                .y_m = 1409547.654,
                .z_m = 5049982.321
            },
            .positionAccuracy_m = 0.0
        }
    };
    EXPECT_FALSE(checkBaseConfig(cfg));
}

TEST(TimingValidation, FixedPositionNegativeAccuracyFails)
{
    BaseConfig cfg {
        .mode = BaseConfig::FixedPosition {
            .position = BaseConfig::FixedPosition::Lla {
                .latitude_deg = 50.0,
                .longitude_deg = 20.0,
                .height_m = 100.0
            },
            .positionAccuracy_m = -0.1
        }
    };
    EXPECT_FALSE(checkBaseConfig(cfg));
}

TEST(TimingValidation, LlaLatitudeOutOfRangeFails)
{
    BaseConfig cfg {
        .mode = BaseConfig::FixedPosition {
            .position = BaseConfig::FixedPosition::Lla {
                .latitude_deg = 91.0,
                .longitude_deg = 21.0,
                .height_m = 110.0
            },
            .positionAccuracy_m = 0.5
        }
    };
    EXPECT_FALSE(checkBaseConfig(cfg));

    cfg.mode = BaseConfig::FixedPosition {
        .position = BaseConfig::FixedPosition::Lla {
            .latitude_deg = -91.0,
            .longitude_deg = 21.0,
            .height_m = 110.0
        },
        .positionAccuracy_m = 0.5
    };
    EXPECT_FALSE(checkBaseConfig(cfg));
}

TEST(TimingValidation, LlaLongitudeOutOfRangeFails)
{
    BaseConfig cfg {
        .mode = BaseConfig::FixedPosition {
            .position = BaseConfig::FixedPosition::Lla {
                .latitude_deg = 52.0,
                .longitude_deg = 181.0,
                .height_m = 110.0
            },
            .positionAccuracy_m = 0.5
        }
    };
    EXPECT_FALSE(checkBaseConfig(cfg));

    cfg.mode = BaseConfig::FixedPosition {
        .position = BaseConfig::FixedPosition::Lla {
            .latitude_deg = 52.0,
            .longitude_deg = -181.0,
            .height_m = 110.0
        },
        .positionAccuracy_m = 0.5
    };
    EXPECT_FALSE(checkBaseConfig(cfg));
}

TEST(TimingValidation, LlaBoundaryCoordinatesValid)
{
    BaseConfig cfg {
        .mode = BaseConfig::FixedPosition {
            .position = BaseConfig::FixedPosition::Lla {
                .latitude_deg = 90.0,
                .longitude_deg = 180.0,
                .height_m = 0.0
            },
            .positionAccuracy_m = 1.0
        }
    };
    EXPECT_TRUE(checkBaseConfig(cfg));

    cfg.mode = BaseConfig::FixedPosition {
        .position = BaseConfig::FixedPosition::Lla {
            .latitude_deg = -90.0,
            .longitude_deg = -180.0,
            .height_m = -10.0
        },
        .positionAccuracy_m = 0.01
    };
    EXPECT_TRUE(checkBaseConfig(cfg));
}

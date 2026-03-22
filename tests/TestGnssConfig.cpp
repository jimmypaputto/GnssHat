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

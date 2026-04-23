/*
 * Tests for atmospheric correction models:
 *   - UNB3m troposphere delay
 *   - Klobuchar ionospheric delay
 *   - Simple troposphere fallback
 */

#include <gtest/gtest.h>
#include <cmath>

#include "GnssMath.hpp"
#include "Troposphere.hpp"
#include "Ionosphere.hpp"

// ── UNB3m troposphere ───────────────────────────────────────────────

TEST(Troposphere, ZenithDelayReasonable)
{
    // Mid-latitude, sea level, elevation = 90° (zenith)
    const double lat = 47.0 * GnssMath::DEG2RAD;
    const double alt = 200.0;
    const double elev = GnssMath::PI / 2.0;  // Zenith
    const int doy = 100;  // April

    double delay = Troposphere::troposphereDelay(lat, alt, elev, doy);

    // Zenith troposphere delay should be ~2.3 m at sea level
    EXPECT_GT(delay, 1.5);
    EXPECT_LT(delay, 3.5);
}

TEST(Troposphere, LowElevationHigherDelay)
{
    const double lat = 47.0 * GnssMath::DEG2RAD;
    const double alt = 200.0;
    const int doy = 100;

    double delayHigh = Troposphere::troposphereDelay(lat, alt, GnssMath::PI / 3.0, doy);
    double delayLow  = Troposphere::troposphereDelay(lat, alt, 10.0 * GnssMath::DEG2RAD, doy);

    // Low elevation should have higher delay (longer path through atmosphere)
    EXPECT_GT(delayLow, delayHigh);
}

TEST(Troposphere, HigherAltitudeLowerDelay)
{
    const double lat = 47.0 * GnssMath::DEG2RAD;
    const double elev = GnssMath::PI / 4.0;  // 45°
    const int doy = 100;

    double delayLow  = Troposphere::troposphereDelay(lat, 0.0, elev, doy);
    double delayHigh = Troposphere::troposphereDelay(lat, 3000.0, elev, doy);

    // Higher altitude → less atmosphere above → less delay
    EXPECT_GT(delayLow, delayHigh);
}

TEST(Troposphere, SeasonalVariation)
{
    const double lat = 47.0 * GnssMath::DEG2RAD;
    const double alt = 200.0;
    const double elev = GnssMath::PI / 4.0;

    double delaySummer = Troposphere::troposphereDelay(lat, alt, elev, 200);  // July
    double delayWinter = Troposphere::troposphereDelay(lat, alt, elev, 15);   // January

    // Delays should differ somewhat between seasons (wet component varies)
    // Both should still be in reasonable range
    EXPECT_GT(delaySummer, 2.0);
    EXPECT_LT(delaySummer, 30.0);
    EXPECT_GT(delayWinter, 2.0);
    EXPECT_LT(delayWinter, 30.0);
}

TEST(Troposphere, VeryLowElevationCapped)
{
    const double lat = 47.0 * GnssMath::DEG2RAD;
    const double alt = 200.0;
    const int doy = 100;

    // 3° elevation — should give large but finite delay
    double delay = Troposphere::troposphereDelay(lat, alt, 3.0 * GnssMath::DEG2RAD, doy);

    EXPECT_TRUE(std::isfinite(delay));
    EXPECT_GT(delay, 10.0);   // At least 10 m at low elevation
    EXPECT_LT(delay, 100.0);  // But not absurd
}

// ── Simple troposphere fallback ─────────────────────────────────────

TEST(Troposphere, SimpleFallbackZenith)
{
    double delay = GnssMath::troposphereDelaySimple(GnssMath::PI / 2.0);
    EXPECT_NEAR(delay, 2.3, 0.01);
}

TEST(Troposphere, SimpleFallbackLowElev)
{
    double delay = GnssMath::troposphereDelaySimple(10.0 * GnssMath::DEG2RAD);
    EXPECT_GT(delay, 10.0);
}

// ── Klobuchar ionosphere ────────────────────────────────────────────

TEST(Ionosphere, KlobucharReturnsPositiveDelay)
{
    // Typical Klobuchar parameters (from GPS broadcast)
    Ionosphere::KlobucharParams params;
    params.alpha[0] =  1.117587089538574e-08;
    params.alpha[1] = -7.450580596923828e-09;
    params.alpha[2] = -5.960464477539063e-08;
    params.alpha[3] =  1.192092895507812e-07;
    params.beta[0]  =  1.167360000000000e+05;
    params.beta[1]  = -1.310720000000000e+05;
    params.beta[2]  = -1.310720000000000e+05;
    params.beta[3]  =  8.519680000000000e+05;

    const double lat = 47.0 * GnssMath::DEG2RAD;
    const double lon = 15.0 * GnssMath::DEG2RAD;
    const double elev = 45.0 * GnssMath::DEG2RAD;
    const double az = 0.0;
    const double tow = 50000.0;  // Daytime

    double delay = Ionosphere::klobucharDelay(params, lat, lon, elev, az, tow);

    // Ionospheric delay should be positive and typically 2–15 m
    EXPECT_GT(delay, 0.5);
    EXPECT_LT(delay, 50.0);
}

TEST(Ionosphere, KlobucharNighttimeLower)
{
    Ionosphere::KlobucharParams params;
    params.alpha[0] =  1.117587089538574e-08;
    params.alpha[1] = -7.450580596923828e-09;
    params.alpha[2] = -5.960464477539063e-08;
    params.alpha[3] =  1.192092895507812e-07;
    params.beta[0]  =  1.167360000000000e+05;
    params.beta[1]  = -1.310720000000000e+05;
    params.beta[2]  = -1.310720000000000e+05;
    params.beta[3]  =  8.519680000000000e+05;

    const double lat = 47.0 * GnssMath::DEG2RAD;
    const double lon = 15.0 * GnssMath::DEG2RAD;
    const double elev = 45.0 * GnssMath::DEG2RAD;
    const double az = 0.0;

    double delayDay   = Ionosphere::klobucharDelay(params, lat, lon, elev, az, 50000.0);
    double delayNight = Ionosphere::klobucharDelay(params, lat, lon, elev, az, 0.0);

    // Nighttime delay should be the minimum (5 ns · c ≈ 1.5 m)
    // Daytime should be higher
    EXPECT_GE(delayDay, delayNight);
}

TEST(Ionosphere, KlobucharVariesWithElevation)
{
    Ionosphere::KlobucharParams params;
    params.alpha[0] =  1.117587089538574e-08;
    params.alpha[1] = -7.450580596923828e-09;
    params.alpha[2] = -5.960464477539063e-08;
    params.alpha[3] =  1.192092895507812e-07;
    params.beta[0]  =  1.167360000000000e+05;
    params.beta[1]  = -1.310720000000000e+05;
    params.beta[2]  = -1.310720000000000e+05;
    params.beta[3]  =  8.519680000000000e+05;

    const double lat = 47.0 * GnssMath::DEG2RAD;
    const double lon = 15.0 * GnssMath::DEG2RAD;
    const double tow = 50000.0;

    double delayHigh = Ionosphere::klobucharDelay(
        params, lat, lon, 80.0 * GnssMath::DEG2RAD, 0.0, tow);
    double delayLow = Ionosphere::klobucharDelay(
        params, lat, lon, 15.0 * GnssMath::DEG2RAD, 0.0, tow);

    // Low elevation → longer path through ionosphere → higher delay
    EXPECT_GT(delayLow, delayHigh);
}

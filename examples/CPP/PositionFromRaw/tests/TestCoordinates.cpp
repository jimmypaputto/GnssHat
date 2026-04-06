/*
 * Tests for coordinate conversion functions:
 *   - ECEF ↔ LLA round-trip
 *   - ECEF → ENU at known reference
 *   - Elevation / azimuth angle computation
 */

#include <gtest/gtest.h>
#include <cmath>

#include "GnssMath.hpp"
#include "MockData.hpp"

using namespace GnssMath;

// ── ECEF → LLA → ECEF round-trip ───────────────────────────────────

TEST(Coordinates, Ecef2LlaKnownPoint)
{
    Ecef ecef{MockData::TRUE_X, MockData::TRUE_Y, MockData::TRUE_Z};
    Lla lla = ecef2lla(ecef);

    EXPECT_NEAR(lla.lat_rad * RAD2DEG, MockData::TRUE_LAT_DEG, 0.01);
    EXPECT_NEAR(lla.lon_rad * RAD2DEG, MockData::TRUE_LON_DEG, 0.01);
    EXPECT_NEAR(lla.alt, MockData::TRUE_ALT, 50.0);  // Rough alt check
}

TEST(Coordinates, Lla2EcefRoundTrip)
{
    Ecef original{MockData::TRUE_X, MockData::TRUE_Y, MockData::TRUE_Z};
    Lla lla = ecef2lla(original);
    Ecef reconstructed = lla2ecef(lla);

    EXPECT_NEAR(reconstructed.x, original.x, 0.001);
    EXPECT_NEAR(reconstructed.y, original.y, 0.001);
    EXPECT_NEAR(reconstructed.z, original.z, 0.001);
}

TEST(Coordinates, Ecef2LlaZeroIsOrigin)
{
    Ecef ecef{0, 0, 0};
    Lla lla = ecef2lla(ecef);
    EXPECT_NEAR(lla.lat_rad, 0.0, 0.01);
    EXPECT_NEAR(lla.lon_rad, 0.0, 0.01);
}

TEST(Coordinates, Ecef2LlaEquator)
{
    // Point on equator at prime meridian, ~sea level
    Lla equator{0.0, 0.0, 0.0};
    Ecef ecef = lla2ecef(equator);

    EXPECT_NEAR(ecef.x, WGS84_A, 1.0);
    EXPECT_NEAR(ecef.y, 0.0, 1.0);
    EXPECT_NEAR(ecef.z, 0.0, 1.0);
}

TEST(Coordinates, Ecef2LlaNorthPole)
{
    Lla pole{PI / 2.0, 0.0, 0.0};
    Ecef ecef = lla2ecef(pole);
    Lla back = ecef2lla(ecef);

    EXPECT_NEAR(back.lat_rad, PI / 2.0, 1e-6);
    EXPECT_NEAR(back.alt, 0.0, 50.0);
}

// ── ENU computation ─────────────────────────────────────────────────

TEST(Coordinates, Ecef2EnuZeroAtRef)
{
    Ecef ref{MockData::TRUE_X, MockData::TRUE_Y, MockData::TRUE_Z};
    Enu enu = ecef2enu(ref, ref);

    EXPECT_NEAR(enu.e, 0.0, 1e-6);
    EXPECT_NEAR(enu.n, 0.0, 1e-6);
    EXPECT_NEAR(enu.u, 0.0, 1e-6);
}

TEST(Coordinates, Ecef2EnuUpDirection)
{
    // Move straight up from the reference point (in LLA)
    Ecef ref{MockData::TRUE_X, MockData::TRUE_Y, MockData::TRUE_Z};
    Lla refLla = ecef2lla(ref);

    Lla upLla = refLla;
    upLla.alt += 100.0;  // 100 m up
    Ecef up = lla2ecef(upLla);

    Enu enu = ecef2enu(up, ref);

    EXPECT_NEAR(enu.u, 100.0, 1.0);  // Should be ~100 m up
    EXPECT_NEAR(enu.e, 0.0, 1.0);    // No east movement
    EXPECT_NEAR(enu.n, 0.0, 1.0);    // No north movement
}

// ── Elevation and azimuth ───────────────────────────────────────────

TEST(Coordinates, ElevationAngleRange)
{
    Ecef rx{MockData::TRUE_X, MockData::TRUE_Y, MockData::TRUE_Z};

    for (const auto& sv : MockData::satellites())
    {
        Ecef svPos{sv.x, sv.y, sv.z};
        double elev = elevationAngle(rx, svPos);

        // All mock SVs should be above the horizon
        EXPECT_GT(elev, 0.0) << "SV " << (int)sv.svId << " below horizon";
        EXPECT_LT(elev, PI / 2.0) << "SV " << (int)sv.svId << " above zenith";
    }
}

TEST(Coordinates, AzimuthAngleRange)
{
    Ecef rx{MockData::TRUE_X, MockData::TRUE_Y, MockData::TRUE_Z};

    for (const auto& sv : MockData::satellites())
    {
        Ecef svPos{sv.x, sv.y, sv.z};
        double az = azimuthAngle(rx, svPos);

        // Azimuth should be in [-π, π]
        EXPECT_GE(az, -PI) << "SV " << (int)sv.svId;
        EXPECT_LE(az,  PI) << "SV " << (int)sv.svId;
    }
}

// ── Earth rotation correction ───────────────────────────────────────

TEST(Coordinates, EarthRotationSmallAngle)
{
    Ecef sv{20000000.0, 0.0, 20000000.0};
    double transit = 0.07;  // ~70 ms

    Ecef corrected = earthRotationCorrection(sv, transit);

    // Y component should decrease (rotation compensates for Earth turning
    // eastward during signal transit, so satellite shifts to negative Y)
    EXPECT_LT(corrected.y, 0.0);
    // Z should be unchanged
    EXPECT_NEAR(corrected.z, sv.z, 0.001);
    // Distance from origin should be preserved
    double r0 = std::sqrt(sv.x * sv.x + sv.y * sv.y);
    double r1 = std::sqrt(corrected.x * corrected.x + corrected.y * corrected.y);
    EXPECT_NEAR(r0, r1, 0.001);
}

TEST(Coordinates, EarthRotationZeroTransitNoChange)
{
    Ecef sv{20000000.0, 5000000.0, 15000000.0};
    Ecef corrected = earthRotationCorrection(sv, 0.0);

    EXPECT_NEAR(corrected.x, sv.x, 1e-6);
    EXPECT_NEAR(corrected.y, sv.y, 1e-6);
    EXPECT_NEAR(corrected.z, sv.z, 1e-6);
}

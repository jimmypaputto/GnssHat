/*
 * Jimmy Paputto 2026
 *
 * Unit tests for SatPos.hpp — Kepler propagator + ENU geometry.
 *
 * No external reference data: tests are built around degenerate /
 * analytically-tractable orbits so they verify the implementation
 * against closed-form ground truth.
 */

#include <gtest/gtest.h>

#include <cmath>

#include "SatPos.hpp"

using namespace JimmyPaputto;

namespace
{
    constexpr double kDegPerRad = 180.0 / M_PI;
}

// ---------------------------------------------------------------- keplerSolve

TEST(SatPosKepler, SolveZeroEccentricityIsIdentity)
{
    // e = 0 → E = M (no iteration needed).
    for (double M : {0.0, 0.3, 1.0, 2.5, M_PI})
    {
        double E = keplerSolve(M, 0.0);
        EXPECT_NEAR(E, M, 1e-12);
    }
}

TEST(SatPosKepler, SolveAtPiIsPi)
{
    // M = π is a fixed point of the equation for any e (sin π = 0).
    double E = keplerSolve(M_PI, 0.1);
    EXPECT_NEAR(E, M_PI, 1e-10);
}

TEST(SatPosKepler, SolveSatisfiesEquation)
{
    // Verify E - e sin E ≈ M for a range of (M, e).
    for (double e : {0.001, 0.01, 0.05, 0.1})
    {
        for (double M = -3.0; M < 3.0; M += 0.4)
        {
            double E = keplerSolve(M, e);
            double residual = E - e * std::sin(E) - M;
            EXPECT_NEAR(residual, 0.0, 1e-10) << "M=" << M << " e=" << e;
        }
    }
}

// ---------------------------------------------------------------- propagator

TEST(SatPosPropagate, CircularEquatorialOrbitClosedForm)
{
    // Circular (e=0), equatorial (i=0), Earth-fixed (OMEGAdot = OMEGAe so
    // the node tracks the rotating Earth, OMEGAk = 0 throughout).  At
    // t=toe the satellite must be at (A, 0, 0); a quarter-period later
    // at (0, A, 0); a half-period later at (-A, 0, 0).
    KeplerEph eph;
    eph.gnss      = GnssCode::GPS;
    eph.svId      = 1;
    eph.sqrtA     = 5153.7;          // typical GPS ~26 561 km semi-major axis
    eph.e         = 0.0;
    eph.i0        = 0.0;
    eph.OMEGA0    = 0.0;
    eph.omega     = 0.0;
    eph.M0        = 0.0;
    eph.deltaN    = 0.0;
    eph.idot      = 0.0;
    eph.OMEGAdot  = omegaEarthOf(GnssCode::GPS); // cancels Earth-rotation term
    eph.toe       = 0.0;

    const double A   = eph.sqrtA * eph.sqrtA;
    const double mu  = muOf(GnssCode::GPS);
    const double n0  = std::sqrt(mu / (A * A * A));
    const double T   = 2.0 * M_PI / n0;

    double x, y, z;

    // t = toe
    propagateKepler(eph, 0.0, x, y, z);
    EXPECT_NEAR(x, A, 1.0);
    EXPECT_NEAR(y, 0.0, 1.0);
    EXPECT_NEAR(z, 0.0, 1.0);

    // t = toe + T/4  →  M = π/2
    propagateKepler(eph, T / 4.0, x, y, z);
    EXPECT_NEAR(x, 0.0, 1.0);
    EXPECT_NEAR(y, A, 1.0);
    EXPECT_NEAR(z, 0.0, 1.0);

    // t = toe + T/2  →  M = π
    propagateKepler(eph, T / 2.0, x, y, z);
    EXPECT_NEAR(x, -A, 1.0);
    EXPECT_NEAR(y, 0.0, 1.0);
    EXPECT_NEAR(z, 0.0, 1.0);
}

TEST(SatPosPropagate, MalformedEphemerisReturnsZero)
{
    KeplerEph bad;          // sqrtA defaulted to 0
    double x = 1, y = 1, z = 1;
    propagateKepler(bad, 0.0, x, y, z);
    EXPECT_EQ(x, 0.0);
    EXPECT_EQ(y, 0.0);
    EXPECT_EQ(z, 0.0);
}

// ----------------------------------------------------------------- ecefToLla

TEST(SatPosEcefToLla, OriginOfPrimeMeridian)
{
    // (lat=0, lon=0, h=0) is on the equator on the prime meridian.
    // ECEF = (a, 0, 0) where a = WGS-84 equatorial radius.
    double lat, lon, h;
    ecefToLla(kWgs84A, 0.0, 0.0, lat, lon, h);
    EXPECT_NEAR(lat, 0.0, 1e-9);
    EXPECT_NEAR(lon, 0.0, 1e-9);
    EXPECT_NEAR(h, 0.0, 1e-3);
}

TEST(SatPosEcefToLla, NinetyEastEquator)
{
    double lat, lon, h;
    ecefToLla(0.0, kWgs84A, 0.0, lat, lon, h);
    EXPECT_NEAR(lat, 0.0, 1e-9);
    EXPECT_NEAR(lon * kDegPerRad, 90.0, 1e-7);
    EXPECT_NEAR(h, 0.0, 1e-3);
}

// ----------------------------------------------------------------- ENU az/el

TEST(SatPosEnu, OverheadIsNinetyDegElevation)
{
    // Observer at (0,0,0) lat/lon, satellite straight up the local
    // vertical means dECEF = (+1000, 0, 0).
    double e, n, u, az, el;
    ecefDeltaToEnu(1000.0, 0.0, 0.0, 0.0, 0.0, e, n, u);
    enuToAzEl(e, n, u, az, el);
    EXPECT_NEAR(el, 90.0, 1e-9);
    EXPECT_NEAR(u, 1000.0, 1e-9);
}

TEST(SatPosEnu, EastHorizonIsAz90El0)
{
    // At (lat=0, lon=0): rotation matrix maps dECEF=(0,1000,0) to
    // east=1000, north=0, up=0 — i.e. azimuth 90°, elevation 0°.
    double e, n, u, az, el;
    ecefDeltaToEnu(0.0, 1000.0, 0.0, 0.0, 0.0, e, n, u);
    enuToAzEl(e, n, u, az, el);
    EXPECT_NEAR(az, 90.0, 1e-9);
    EXPECT_NEAR(el,  0.0, 1e-9);
}

TEST(SatPosEnu, NorthPoleIsAz0El0)
{
    // dECEF=(0,0,+1000) at equator — north pole direction → az=0 N, el=0.
    double e, n, u, az, el;
    ecefDeltaToEnu(0.0, 0.0, 1000.0, 0.0, 0.0, e, n, u);
    enuToAzEl(e, n, u, az, el);
    EXPECT_NEAR(az, 0.0, 1e-9);
    EXPECT_NEAR(el, 0.0, 1e-9);
}

TEST(SatPosEnu, AzimuthIsClockwiseFromNorth)
{
    // ENU (e=1, n=1, u=0) → due NE → az=45°.
    double az, el;
    enuToAzEl(1.0, 1.0, 0.0, az, el);
    EXPECT_NEAR(az, 45.0, 1e-9);
    EXPECT_NEAR(el, 0.0, 1e-9);

    enuToAzEl(-1.0, 0.0, 0.0, az, el);   // west
    EXPECT_NEAR(az, 270.0, 1e-9);

    enuToAzEl(0.0, -1.0, 0.0, az, el);   // south
    EXPECT_NEAR(az, 180.0, 1e-9);
}

// ----------------------------------------------------------- computeAzEl one-shot

TEST(SatPosComputeAzEl, OverheadObserverFromPropagator)
{
    // Place observer at lat=0, lon=0, h=0 — ECEF (a, 0, 0).  Use the
    // closed-form circular-equatorial orbit at t=toe so the satellite
    // is at (A, 0, 0): straight-up direction → elevation 90°.
    KeplerEph eph;
    eph.gnss      = GnssCode::GPS;
    eph.sqrtA     = 5153.7;
    eph.OMEGAdot  = omegaEarthOf(GnssCode::GPS);
    // M0=0 already; everything else default 0.

    double az, el, sx, sy, sz;
    bool ok = computeAzEl(eph,
                          /*baseX*/ kWgs84A, /*baseY*/ 0.0, /*baseZ*/ 0.0,
                          /*tow*/ 0.0,
                          az, el, sx, sy, sz);
    ASSERT_TRUE(ok);
    EXPECT_NEAR(el, 90.0, 1e-3);
}

TEST(SatPosComputeAzEl, RejectsZeroSqrtA)
{
    KeplerEph bad;       // sqrtA = 0
    double az, el, sx, sy, sz;
    EXPECT_FALSE(computeAzEl(bad, kWgs84A, 0, 0, 0.0, az, el, sx, sy, sz));
}

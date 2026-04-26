/*
 * Jimmy Paputto 2026
 *
 * Pure-math helpers for satellite position propagation and sky-plot
 * geometry.  Inputs: a Keplerian ephemeris (decoded from RTCM 1019 /
 * 1042 / 1044 / 1046) and the ground antenna ECEF position.  Outputs:
 * azimuth / elevation in degrees.
 *
 * GLONASS is intentionally out of scope (uses non-Keplerian state-
 * vector ephemerides — handled separately in a future revision).
 *
 * Header-only, no I/O, no dependencies beyond <cmath>.
 */

#ifndef NTRIP_CASTER_SAT_POS_HPP_
#define NTRIP_CASTER_SAT_POS_HPP_

#include <cmath>
#include <cstdint>
#include <optional>

namespace JimmyPaputto
{

    /// Generic Keplerian ephemeris.  Field names follow the standard
    /// GPS interface specification (IS-GPS-200).  Galileo I/NAV and
    /// BDS broadcast use the same model with slightly different time
    /// scales / Earth-rotation rate.
    struct KeplerEph
    {
        // Identity and quality
        uint8_t  gnss        = 0;     // 1=GPS, 3=GAL, 5=QZSS, 6=BDS
        uint8_t  svId        = 0;     // PRN within the GNSS
        uint16_t weekNumber  = 0;     // GNSS-specific week (modulo)
        double   toe         = 0.0;   // ephemeris reference time, s of week
        double   toc         = 0.0;   // clock reference time, s of week

        // Orbit parameters
        double sqrtA      = 0.0;
        double e          = 0.0;
        double i0         = 0.0;     // rad
        double OMEGA0     = 0.0;     // rad
        double omega      = 0.0;     // arg of perigee, rad
        double M0         = 0.0;     // rad
        double deltaN     = 0.0;     // rad/s
        double idot       = 0.0;     // rad/s
        double OMEGAdot   = 0.0;     // rad/s
        double Cuc = 0.0, Cus = 0.0;  // rad
        double Crc = 0.0, Crs = 0.0;  // m
        double Cic = 0.0, Cis = 0.0;  // rad

        // Clock corrections (mostly ignored — we want geometry only)
        double af0 = 0.0, af1 = 0.0, af2 = 0.0;

        // Status
        uint8_t  health        = 0;
        uint64_t receivedUnixMs = 0;  // wall clock when this frame arrived
    };

    /// GNSS index codes used internally.
    namespace GnssCode
    {
        constexpr uint8_t GPS  = 1;
        constexpr uint8_t GAL  = 3;
        constexpr uint8_t QZSS = 5;
        constexpr uint8_t BDS  = 6;
    }

    /// Earth gravitational constant (m^3/s^2) per GNSS.
    inline double muOf(uint8_t gnss)
    {
        if (gnss == GnssCode::GAL) return 3.986004418e14;
        if (gnss == GnssCode::BDS) return 3.986004418e14;
        return 3.986005e14; // GPS / QZSS
    }

    /// Earth rotation rate (rad/s) per GNSS.
    inline double omegaEarthOf(uint8_t gnss)
    {
        if (gnss == GnssCode::BDS) return 7.2921150e-5;
        return 7.2921151467e-5;
    }

    /// WGS-84 ellipsoid (good enough for BDS too — CGCS2000 is identical
    /// to WGS-84 within sub-millimetre).
    constexpr double kWgs84A = 6378137.0;
    constexpr double kWgs84F = 1.0 / 298.257223563;

    /// Solve Kepler's equation E - e sin E = M for E by Newton iteration.
    inline double keplerSolve(double M, double e)
    {
        double E = M;
        for (int i = 0; i < 8; ++i)
        {
            double f  = E - e * std::sin(E) - M;
            double fp = 1.0 - e * std::cos(E);
            double dE = f / fp;
            E -= dE;
            if (std::fabs(dE) < 1e-12) break;
        }
        return E;
    }

    /// Propagate a Keplerian ephemeris to time-of-week `t` (seconds).
    /// Returns satellite ECEF position (metres) at that instant.
    inline void propagateKepler(const KeplerEph& eph, double t,
                                double& xEcef, double& yEcef, double& zEcef)
    {
        const double mu       = muOf(eph.gnss);
        const double OMEGAe   = omegaEarthOf(eph.gnss);
        const double A        = eph.sqrtA * eph.sqrtA;
        if (A <= 0.0)
        {
            xEcef = yEcef = zEcef = 0.0;
            return;
        }
        const double n0 = std::sqrt(mu / (A * A * A));
        const double n  = n0 + eph.deltaN;

        // Time from ephemeris reference epoch with week-rollover guard.
        double tk = t - eph.toe;
        if (tk >  302400.0) tk -= 604800.0;
        if (tk < -302400.0) tk += 604800.0;

        const double Mk = eph.M0 + n * tk;
        const double Ek = keplerSolve(Mk, eph.e);

        const double sinEk = std::sin(Ek);
        const double cosEk = std::cos(Ek);
        const double sqrt1me2 = std::sqrt(1.0 - eph.e * eph.e);
        const double sinNuk = (sqrt1me2 * sinEk) / (1.0 - eph.e * cosEk);
        const double cosNuk = (cosEk - eph.e)     / (1.0 - eph.e * cosEk);
        const double nuk = std::atan2(sinNuk, cosNuk);

        const double phik = nuk + eph.omega;
        const double s2p = std::sin(2.0 * phik);
        const double c2p = std::cos(2.0 * phik);

        const double duk = eph.Cus * s2p + eph.Cuc * c2p;
        const double drk = eph.Crs * s2p + eph.Crc * c2p;
        const double dik = eph.Cis * s2p + eph.Cic * c2p;

        const double uk = phik + duk;
        const double rk = A * (1.0 - eph.e * cosEk) + drk;
        const double ik = eph.i0 + dik + eph.idot * tk;

        const double xkOrb = rk * std::cos(uk);
        const double ykOrb = rk * std::sin(uk);

        // Corrected longitude of ascending node (BDS GEO sats are
        // handled the same — we accept the modest inclination error
        // for sky-plot purposes).
        const double OMEGAk =
            eph.OMEGA0 + (eph.OMEGAdot - OMEGAe) * tk - OMEGAe * eph.toe;

        const double cosOk = std::cos(OMEGAk);
        const double sinOk = std::sin(OMEGAk);
        const double cosIk = std::cos(ik);
        const double sinIk = std::sin(ik);

        xEcef = xkOrb * cosOk - ykOrb * cosIk * sinOk;
        yEcef = xkOrb * sinOk + ykOrb * cosIk * cosOk;
        zEcef = ykOrb * sinIk;
    }

    /// ECEF → geodetic (lat, lon, h) using Bowring closed-form.
    inline void ecefToLla(double x, double y, double z,
                          double& latRad, double& lonRad, double& hMeters)
    {
        const double a  = kWgs84A;
        const double f  = kWgs84F;
        const double b  = a * (1.0 - f);
        const double e2 = f * (2.0 - f);
        const double ep2 = (a * a - b * b) / (b * b);
        const double p = std::sqrt(x * x + y * y);
        const double th = std::atan2(z * a, p * b);
        const double sinTh = std::sin(th);
        const double cosTh = std::cos(th);
        latRad = std::atan2(z + ep2 * b * sinTh * sinTh * sinTh,
                            p - e2 * a * cosTh * cosTh * cosTh);
        lonRad = std::atan2(y, x);
        const double sinLat = std::sin(latRad);
        const double N = a / std::sqrt(1.0 - e2 * sinLat * sinLat);
        hMeters = p / std::cos(latRad) - N;
    }

    /// ECEF vector (sat - obs) → ENU at the observer.
    inline void ecefDeltaToEnu(double dx, double dy, double dz,
                               double obsLatRad, double obsLonRad,
                               double& east, double& north, double& up)
    {
        const double sl = std::sin(obsLatRad), cl = std::cos(obsLatRad);
        const double so = std::sin(obsLonRad), co = std::cos(obsLonRad);
        east  = -so * dx + co * dy;
        north = -sl * co * dx - sl * so * dy + cl * dz;
        up    =  cl * co * dx + cl * so * dy + sl * dz;
    }

    /// ENU vector → azimuth (deg, 0=N, CW) and elevation (deg, +up).
    inline void enuToAzEl(double e, double n, double u,
                          double& azDeg, double& elDeg)
    {
        const double horiz = std::sqrt(e * e + n * n);
        elDeg = std::atan2(u, horiz) * 180.0 / M_PI;
        double az = std::atan2(e, n) * 180.0 / M_PI;
        if (az < 0.0) az += 360.0;
        azDeg = az;
    }

    /// One-shot helper: ephemeris + base ECEF + signal-time → az/el.
    /// Returns false if the ephemeris is malformed.
    inline bool computeAzEl(const KeplerEph& eph,
                            double baseX, double baseY, double baseZ,
                            double tow,
                            double& azDeg, double& elDeg,
                            double& satX, double& satY, double& satZ)
    {
        if (eph.sqrtA <= 0.0) return false;
        propagateKepler(eph, tow, satX, satY, satZ);

        double obsLat, obsLon, obsH;
        ecefToLla(baseX, baseY, baseZ, obsLat, obsLon, obsH);

        // Light Earth-rotation correction (signal travel ~70 ms)
        const double range  = std::sqrt((satX - baseX) * (satX - baseX) +
                                        (satY - baseY) * (satY - baseY) +
                                        (satZ - baseZ) * (satZ - baseZ));
        const double dt     = range / 299792458.0;
        const double OMEGAe = omegaEarthOf(eph.gnss);
        const double dTheta = OMEGAe * dt;
        const double cs = std::cos(dTheta), sn = std::sin(dTheta);
        const double xRot =  cs * satX + sn * satY;
        const double yRot = -sn * satX + cs * satY;
        satX = xRot; satY = yRot;

        double e, n, u;
        ecefDeltaToEnu(satX - baseX, satY - baseY, satZ - baseZ,
                       obsLat, obsLon, e, n, u);
        enuToAzEl(e, n, u, azDeg, elDeg);
        return true;
    }

}

#endif // NTRIP_CASTER_SAT_POS_HPP_

/*
 * Jimmy Paputto 2026
 *
 * Minimal GNSS math utilities — constants, ECEF types, coordinate
 * transforms. Kept small. The SPP solver lives in Solver.hpp.
 */

#ifndef GNSS_MATH_HPP_
#define GNSS_MATH_HPP_

#include <cmath>
#include <cstdint>

namespace GnssMath
{

// ── Physical constants ──────────────────────────────────────────────
constexpr double C          = 299792458.0;              // Speed of light [m/s]
constexpr double PI         = 3.14159265358979323846;
constexpr double DEG2RAD    = PI / 180.0;
constexpr double RAD2DEG    = 180.0 / PI;

// ── WGS-84 ──────────────────────────────────────────────────────────
constexpr double WGS84_A    = 6378137.0;                // Semi-major axis [m]
constexpr double WGS84_F    = 1.0 / 298.257223563;      // Flattening
constexpr double WGS84_E2   = 2 * WGS84_F - WGS84_F * WGS84_F;
constexpr double OMEGA_E    = 7.2921151467e-5;          // Earth rotation [rad/s]
constexpr double MU_GPS     = 3.986005e14;              // GM [m^3/s^2] (GPS)
constexpr double MU_BDS     = 3.986004418e14;           // GM (BeiDou)
constexpr double MU_GAL     = 3.986004418e14;           // GM (Galileo)
constexpr double MU_GLO     = 3.9860044e14;             // GM (GLONASS PZ-90.02)
constexpr double OMEGA_BDS  = 7.2921150e-5;             // BeiDou Earth rotation
constexpr double OMEGA_GLO  = 7.292115e-5;              // GLONASS Earth rotation

// ── Signal frequencies ──────────────────────────────────────────────
constexpr double GPS_L1_FREQ   = 1575.42e6;
constexpr double GPS_L5_FREQ   = 1176.45e6;
constexpr double GAL_E1_FREQ   = 1575.42e6;
constexpr double BDS_B1I_FREQ  = 1561.098e6;
constexpr double GLO_L1_BASE   = 1602.0e6;
constexpr double GLO_L1_STEP   = 0.5625e6;

constexpr double GPS_L1_LAMBDA = C / GPS_L1_FREQ;

// ── Types ───────────────────────────────────────────────────────────
struct Ecef { double x, y, z; };
struct Lla  { double lat_rad, lon_rad, alt; };
struct Enu  { double e, n, u; };

// ── Coordinate transforms ───────────────────────────────────────────
inline Ecef lla2ecef(const Lla& p)
{
    const double s = std::sin(p.lat_rad);
    const double c = std::cos(p.lat_rad);
    const double N = WGS84_A / std::sqrt(1.0 - WGS84_E2 * s * s);
    return {
        (N + p.alt) * c * std::cos(p.lon_rad),
        (N + p.alt) * c * std::sin(p.lon_rad),
        (N * (1.0 - WGS84_E2) + p.alt) * s
    };
}

inline Lla ecef2lla(const Ecef& e)
{
    const double p = std::sqrt(e.x * e.x + e.y * e.y);
    double lat = std::atan2(e.z, p * (1.0 - WGS84_E2));
    double N = WGS84_A;
    for (int i = 0; i < 8; ++i)
    {
        const double s = std::sin(lat);
        N = WGS84_A / std::sqrt(1.0 - WGS84_E2 * s * s);
        lat = std::atan2(e.z + WGS84_E2 * N * s, p);
    }
    return {
        lat,
        std::atan2(e.y, e.x),
        p / std::cos(lat) - N
    };
}

inline Enu ecef2enu(const Ecef& target, const Ecef& ref)
{
    const Lla r = ecef2lla(ref);
    const double dx = target.x - ref.x;
    const double dy = target.y - ref.y;
    const double dz = target.z - ref.z;
    const double sLat = std::sin(r.lat_rad), cLat = std::cos(r.lat_rad);
    const double sLon = std::sin(r.lon_rad), cLon = std::cos(r.lon_rad);
    return {
        -sLon * dx + cLon * dy,
        -sLat * cLon * dx - sLat * sLon * dy + cLat * dz,
         cLat * cLon * dx + cLat * sLon * dy + sLat * dz
    };
}

}  // namespace GnssMath

#endif  // GNSS_MATH_HPP_

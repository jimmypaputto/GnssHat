/*
 * Jimmy Paputto 2026
 *
 * Klobuchar Ionospheric Delay Model
 *
 * Computes the L1 ionospheric group delay using the Klobuchar
 * single-frequency broadcast model. The GPS navigation message
 * (subframe 4, page 18) transmits eight coefficients (α₀–α₃, β₀–β₃)
 * that parameterize a cosine model of the ionospheric delay as a
 * function of local time, geomagnetic latitude, and elevation angle.
 *
 * The model removes approximately 50–70% of the ionospheric delay
 * compared to ignoring it entirely, which is typically 5–15 m of
 * range error on L1. At night the model applies a constant floor
 * of 5 ns (~1.5 m).
 *
 * Algorithm (IS-GPS-200 Section 20.3.3.5.2.5):
 *   1. Compute the ionospheric pierce point (IPP) — where the
 *      satellite line-of-sight intersects a thin-shell ionosphere
 *      at 350 km altitude.
 *   2. Convert IPP to geomagnetic latitude.
 *   3. Compute local time at the IPP.
 *   4. Evaluate amplitude and period from the α/β polynomials.
 *   5. Apply obliquity factor to scale zenith delay to slant delay.
 *
 * This file contains only the algorithm. The Klobuchar coefficients
 * must be obtained from the GPS navigation message (see GpsEphemeris.hpp).
 *
 * Reference: IS-GPS-200N, Section 20.3.3.5.2.5
 */

#ifndef IONOSPHERE_HPP_
#define IONOSPHERE_HPP_

#include <cmath>
#include <algorithm>

namespace Ionosphere
{

constexpr double SPEED_OF_LIGHT = 299792458.0;
constexpr double PI = 3.14159265358979323846;

// ── Klobuchar broadcast parameters ─────────────────────────────────
//
// Transmitted in GPS subframe 4, page 18.
// α coefficients control the amplitude of the daytime cosine.
// β coefficients control the period of the daytime cosine.
//
struct KlobucharParams
{
    double alpha[4] = {};   // α₀..α₃ (seconds, s/semi-circle, s/semi-circle², s/semi-circle³)
    double beta[4]  = {};   // β₀..β₃ (seconds, s/semi-circle, s/semi-circle², s/semi-circle³)
};

// ── Klobuchar delay computation ─────────────────────────────────────
//
// Parameters:
//   params       — broadcast ionospheric coefficients
//   lat_rad      — receiver geodetic latitude (radians)
//   lon_rad      — receiver geodetic longitude (radians)
//   elevationRad — satellite elevation angle (radians, > 0)
//   azimuthRad   — satellite azimuth angle (radians, 0 = North)
//   gpsTow       — GPS time of week (seconds)
//
// Returns: L1 ionospheric slant delay in meters (always ≥ 0).
//          The delay should be subtracted from the pseudorange
//          (or equivalently added to the modelled geometric range).
//
inline double klobucharDelay(const KlobucharParams& params,
                             double lat_rad, double lon_rad,
                             double elevationRad, double azimuthRad,
                             double gpsTow)
{
    // Convert to semi-circles (IS-GPS-200 convention)
    const double phi_u = lat_rad / PI;       // User latitude (semi-circles)
    const double lam_u = lon_rad / PI;       // User longitude (semi-circles)
    const double El    = elevationRad / PI;  // Elevation (semi-circles)
    const double Az    = azimuthRad;         // Already in radians for trig

    // 1. Earth-centred angle (semi-circles)
    const double psi = 0.0137 / (El + 0.11) - 0.022;

    // 2. Latitude of the ionospheric pierce point (semi-circles)
    double phi_i = phi_u + psi * std::cos(Az);
    if (phi_i >  0.416) phi_i =  0.416;
    if (phi_i < -0.416) phi_i = -0.416;

    // 3. Longitude of the IPP (semi-circles)
    const double lam_i = lam_u + psi * std::sin(Az) / std::cos(phi_i * PI);

    // 4. Geomagnetic latitude (semi-circles)
    const double phi_m = phi_i + 0.064 * std::cos((lam_i - 1.617) * PI);

    // 5. Local time at IPP (seconds)
    double t = 43200.0 * lam_i + gpsTow;
    t = std::fmod(t, 86400.0);
    if (t < 0) t += 86400.0;

    // 6. Obliquity factor (dimensionless)
    const double F = 1.0 + 16.0 * std::pow(0.53 - El, 3);

    // 7. Amplitude of the cosine (seconds), must be ≥ 0
    const double phi_m2 = phi_m * phi_m;
    const double phi_m3 = phi_m2 * phi_m;
    double A = params.alpha[0]
             + params.alpha[1] * phi_m
             + params.alpha[2] * phi_m2
             + params.alpha[3] * phi_m3;
    if (A < 0) A = 0;

    // 8. Period of the cosine (seconds), must be ≥ 72000
    double P = params.beta[0]
             + params.beta[1] * phi_m
             + params.beta[2] * phi_m2
             + params.beta[3] * phi_m3;
    if (P < 72000.0) P = 72000.0;

    // 9. Phase (radians)
    const double x = 2.0 * PI * (t - 50400.0) / P;

    // 10. Ionospheric time delay (seconds)
    double T_iono;
    if (std::fabs(x) < 1.57)
        T_iono = F * (5.0e-9 + A * (1.0 - x * x / 2.0 + x * x * x * x / 24.0));
    else
        T_iono = F * 5.0e-9;  // Night-time floor

    // Convert to meters
    return T_iono * SPEED_OF_LIGHT;
}

}  // Ionosphere

#endif  // IONOSPHERE_HPP_

/*
 * Jimmy Paputto 2026
 *
 * Tropospheric Delay Model — UNB3m
 *
 * Computes the tropospheric path delay experienced by GNSS signals
 * as they traverse the neutral atmosphere. The delay has two components:
 *
 *   1. Hydrostatic (dry) delay — caused by the dry gas mixture,
 *      primarily a function of surface pressure. Accounts for ~90%
 *      of the total zenith delay (~2.3 m).
 *
 *   2. Wet delay — caused by water vapour. Highly variable,
 *      typically ~0.1–0.4 m at zenith.
 *
 * Model:
 *   - Zenith delays are computed using the Saastamoinen model with
 *     meteorological parameters from the UNB3m lookup table.
 *   - UNB3m provides annual-mean and seasonal-amplitude values for
 *     pressure (P), temperature (T), water vapour pressure (e),
 *     temperature lapse rate (β), and water vapour lapse rate (λ)
 *     at five latitude bands (15°–75°). Values are interpolated for
 *     the receiver's latitude and day-of-year.
 *   - The Neill mapping function maps zenith delays to slant delays
 *     at the satellite's elevation angle.
 *
 * This is a self-contained model requiring no external data.
 *
 * Reference:
 *   Leandro, R., Santos, M., Langley, R. (2006). "UNB3m_pack: a
 *   neutral atmosphere delay package for radiometric space techniques."
 *   GPS Solutions, 12(1), 65-70.
 */

#ifndef TROPOSPHERE_HPP_
#define TROPOSPHERE_HPP_

#include <cmath>
#include <algorithm>

namespace Troposphere
{

// ── Physical constants ──────────────────────────────────────────────
constexpr double K1    = 77.604;     // K/hPa — refractivity coefficient
constexpr double K2    = 382000.0;   // K²/hPa — refractivity coefficient
constexpr double RD    = 287.054;    // J/(kg·K) — specific gas constant for dry air
constexpr double GM    = 9.784;      // m/s² — gravitational acceleration (mean)
constexpr double DOY0  = 28.0;       // Day-of-year offset for southern hemisphere

// ── UNB3m lookup table ──────────────────────────────────────────────
//
// Five latitude bands: 15°, 30°, 45°, 60°, 75°
// Each row: { P(hPa), T(K), e(hPa), β(K/m), λ(dimensionless) }
// Two tables: annual mean and seasonal amplitude (cosine model).
//

constexpr int N_LATS = 5;
constexpr double LATS[N_LATS] = {15.0, 30.0, 45.0, 60.0, 75.0};

// Annual mean values
constexpr double AVG[N_LATS][5] = {
    // P(hPa)    T(K)      e(hPa)   β(K/m)     λ
    { 1013.25,  299.65,   26.31,  0.00630,  2.77 },  // 15°
    { 1017.25,  294.15,   21.79,  0.00605,  3.15 },  // 30°
    { 1015.75,  283.15,   11.66,  0.00558,  2.57 },  // 45°
    { 1011.75,  272.15,    6.78,  0.00539,  1.81 },  // 60°
    { 1013.00,  263.65,    4.11,  0.00453,  1.55 },  // 75°
};

// Seasonal amplitude (peak-to-mean)
constexpr double AMP[N_LATS][5] = {
    {  0.00,   0.00,  0.00,  0.00000, 0.00 },  // 15°
    { -3.75,   7.00,  8.85,  0.00025, 0.33 },  // 30°
    { -2.25,  11.00,  7.24,  0.00032, 0.46 },  // 45°
    { -1.75,  15.00,  5.36,  0.00081, 0.74 },  // 60°
    { -0.50,  14.50,  3.39,  0.00062, 0.30 },  // 75°
};

// ── Latitude interpolation ──────────────────────────────────────────
//
// Linearly interpolates a 5-element table for the given absolute
// latitude (degrees). Clamps at the table boundaries.
//
inline void interpolateMeteo(double absLatDeg, int doy, bool southernHemi,
                             double& P, double& T, double& e,
                             double& beta, double& lambda)
{
    // Clamp latitude to table range
    const double lat = std::clamp(absLatDeg, LATS[0], LATS[N_LATS - 1]);

    // Find bracketing indices
    int lo = 0;
    for (int i = 0; i < N_LATS - 1; ++i)
    {
        if (lat >= LATS[i] && lat <= LATS[i + 1])
        {
            lo = i;
            break;
        }
    }
    const int hi = lo + 1;
    const double frac = (lat - LATS[lo]) / (LATS[hi] - LATS[lo]);

    // Cosine seasonal model: value = mean - amplitude * cos(2π(doy - doy_min)/365.25)
    // Southern hemisphere: shift by half year
    const double doyShift = southernHemi ? (doy + 182.625) : static_cast<double>(doy);
    const double cosFactor = std::cos(2.0 * M_PI * (doyShift - DOY0) / 365.25);

    double vals[5];
    for (int k = 0; k < 5; ++k)
    {
        const double meanLo = AVG[lo][k];
        const double meanHi = AVG[hi][k];
        const double ampLo  = AMP[lo][k];
        const double ampHi  = AMP[hi][k];

        const double mean = meanLo + frac * (meanHi - meanLo);
        const double amp  = ampLo  + frac * (ampHi  - ampLo);

        vals[k] = mean - amp * cosFactor;
    }

    P      = vals[0];
    T      = vals[1];
    e      = vals[2];
    beta   = vals[3];
    lambda = vals[4];
}

// ── Saastamoinen zenith delays ──────────────────────────────────────
//
//   ZHD = (0.0022768 * P) / (1 - 0.00266·cos(2φ) - 0.00028·h_km)
//   ZWD = (0.002277 / (1255/T + 0.05)) * e
//
// where P = pressure (hPa), T = temperature (K), e = water vapour
// pressure (hPa), φ = latitude (rad), h_km = height (km).
//
// Height correction: meteorological values at sea level are reduced
// to receiver height using the standard atmosphere model.
//

inline double zenithHydrostaticDelay(double P, double lat_rad, double alt_m)
{
    const double h_km = alt_m / 1000.0;
    const double denom = 1.0 - 0.00266 * std::cos(2.0 * lat_rad) - 0.00028 * h_km;
    return 0.0022768 * P / denom;
}

inline double zenithWetDelay(double T, double e, double beta, double lambda)
{
    // The ZWD from Saastamoinen simplified using UNB3m parameters
    const double Tm = T * (1.0 - beta * 9000.0 / (2.0 * T));  // Mean temperature estimate
    return 0.002277 * (1255.0 / Tm + 0.05) * e;
}

// ── Height reduction of meteorological parameters ───────────────────
//
// Reduces sea-level UNB3m values to receiver altitude using standard
// atmosphere relationships.
//
inline void reduceToHeight(double alt_m, double& P, double& T, double& e,
                           double beta, double lambda)
{
    if (alt_m < 1.0) return;  // Already near sea level

    const double T0 = T;
    T = T0 - beta * alt_m;
    P = P * std::pow(T / T0, GM / (RD * beta));
    e = e * std::pow(T / T0, (lambda + 1.0) * GM / (RD * beta));
}

// ── Neill dry mapping function ──────────────────────────────────────
//
// Maps zenith delay to slant delay as a function of elevation angle.
// Uses Marini continued-fraction form: m(el) = 1/sin(el) at high
// elevations, with corrections at low elevations.
//
// Simplified Neill (1996) hydrostatic mapping function coefficients
// for a mean atmosphere. Full Neill uses latitude/height/DOY-dependent
// coefficients; this uses a simplified set adequate for SPP.
//
inline double neilDryMapping(double elevationRad)
{
    const double sinEl = std::sin(elevationRad);
    if (sinEl < 0.02) return 1.0 / 0.02;  // Clip at ~1° elevation

    // Simplified Marini continued fraction: m = 1 / (sin(el) + a/(sin(el) + b/(sin(el) + c)))
    constexpr double a = 0.001185;
    constexpr double b = 0.001144;
    constexpr double c = 0.000090;

    const double m = 1.0 / (sinEl + a / (sinEl + b / (sinEl + c)));
    return m;
}

// ── Neill wet mapping function ──────────────────────────────────────
inline double neilWetMapping(double elevationRad)
{
    const double sinEl = std::sin(elevationRad);
    if (sinEl < 0.02) return 1.0 / 0.02;

    constexpr double a = 0.000583;
    constexpr double b = 0.000147;
    constexpr double c = 0.000043;

    return 1.0 / (sinEl + a / (sinEl + b / (sinEl + c)));
}

// ── Public interface ────────────────────────────────────────────────
//
// Computes total tropospheric slant delay (meters) for a signal at
// the given elevation angle, from a receiver at (lat, alt) on a
// given day-of-year.
//
// Parameters:
//   lat_rad      — receiver geodetic latitude (radians)
//   alt_m        — receiver ellipsoidal height (meters)
//   elevationRad — satellite elevation angle (radians)
//   dayOfYear    — integer 1–366
//
// Returns: total slant delay in meters (always positive, to be
//          subtracted from measured pseudorange or added to modelled range)
//
inline double troposphereDelay(double lat_rad, double alt_m,
                               double elevationRad, int dayOfYear)
{
    const double absLatDeg = std::fabs(lat_rad) * 180.0 / M_PI;
    const bool southern = (lat_rad < 0);

    // Interpolate UNB3m meteorological parameters at sea level
    double P, T, e, beta, lambda;
    interpolateMeteo(absLatDeg, dayOfYear, southern, P, T, e, beta, lambda);

    // Reduce to receiver height
    reduceToHeight(alt_m, P, T, e, beta, lambda);

    // Zenith delays
    const double zhd = zenithHydrostaticDelay(P, lat_rad, alt_m);
    const double zwd = zenithWetDelay(T, e, beta, lambda);

    // Map to slant
    const double mh = neilDryMapping(elevationRad);
    const double mw = neilWetMapping(elevationRad);

    return zhd * mh + zwd * mw;
}

}  // Troposphere

#endif  // TROPOSPHERE_HPP_

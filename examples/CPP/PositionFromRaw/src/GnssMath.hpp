/*
 * Jimmy Paputto 2026
 *
 * Standalone GNSS math utilities for position computation from raw
 * pseudorange observations using a weighted least-squares solver.
 *
 * This is an educational / proof-of-concept implementation.
 * It computes a single-point position (SPP) from L1 pseudoranges and
 * broadcast ephemeris-derived satellite positions (via UBX-RXM-SFRBX).
 *
 * Features:
 *   - Multi-constellation satellite positions from broadcast ephemeris
 *     (GPS/Galileo/BeiDou via Keplerian orbit, GLONASS via RK4 integration)
 *   - Satellite clock bias correction (af0/af1/af2 + relativistic + TGD)
 *   - Earth rotation correction during signal transit
 *   - Pluggable atmospheric correction model (troposphere + ionosphere)
 *   - UNB3m troposphere model with Neill mapping functions
 *   - Klobuchar single-frequency ionospheric model from broadcast params
 *   - Dual-frequency iono-free combination (L1/L5) when second frequency
 *     is available, eliminating first-order ionospheric error entirely
 *
 * Integer ambiguity resolution (IAR) pipeline:
 *   - CarrierPhase.hpp: cycle slip detection + phase validation
 *   - AmbiguityFloat.hpp: extended WLS with [x,y,z,cdt,N1..Nm] state
 *   - AmbiguityLambda.hpp: LAMBDA decorrelation + integer search + ratio test
 *   - DualFrequency.hpp: Melbourne-Wübbena wide-lane bootstrapping
 */

#ifndef GNSS_MATH_HPP_
#define GNSS_MATH_HPP_

#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include <jimmypaputto/GnssHat.hpp>

namespace GnssMath
{

// ── WGS-84 constants ────────────────────────────────────────────────
constexpr double WGS84_A  = 6378137.0;           // Semi-major axis (m)
constexpr double WGS84_F  = 1.0 / 298.257223563; // Flattening
constexpr double WGS84_E2 = 2 * WGS84_F - WGS84_F * WGS84_F; // First eccentricity^2
constexpr double C        = 299792458.0;          // Speed of light (m/s)
constexpr double OMEGA_E  = 7.2921151467e-5;      // Earth rotation rate (rad/s)
constexpr double PI       = 3.14159265358979323846;
constexpr double DEG2RAD  = PI / 180.0;
constexpr double RAD2DEG  = 180.0 / PI;

// GPS L1 frequency
constexpr double GPS_L1_FREQ = 1575.42e6;         // Hz
constexpr double GPS_L1_LAMBDA = C / GPS_L1_FREQ;  // Wavelength (m)

// GLONASS L1 base frequency
constexpr double GLO_L1_BASE = 1602.0e6;
constexpr double GLO_L1_STEP = 0.5625e6;

// ── Coordinate types ────────────────────────────────────────────────
struct Ecef
{
    double x, y, z;
};

struct Lla
{
    double lat_rad, lon_rad, alt;
};

struct Enu
{
    double e, n, u;
};

// ── Satellite state (from ephemeris or fallback approximation) ───────
struct SatelliteState
{
    JimmyPaputto::EGnssId gnssId;
    uint8_t svId;
    Ecef position;        // ECEF position at signal transmission time (m)
    double clockBias;     // Satellite clock bias (s)
};

// ── Computed position result ────────────────────────────────────────
enum class SolutionMode { SPP, Float, Fixed };

struct PositionSolution
{
    Ecef ecef;
    Lla  lla;
    double receiverClockBias_m;   // Receiver clock bias in meters
    double hdop;
    double vdop;
    double pdop;
    int    usedSatellites;
    int    iterations;
    double residualRms;           // RMS of post-fit residuals (m)
    bool   converged;

    // Ambiguity resolution info (populated when carrier phase is used)
    SolutionMode mode = SolutionMode::SPP;
    int    fixedAmbiguities = 0;  // Number of resolved integer ambiguities
    double ratioTest = 0.0;       // LAMBDA ratio test value (0 = not attempted)
};

// ── Observation paired with satellite state ─────────────────────────
//
// If ionoFreePr is set, the solver uses it instead of obs.prMes and
// skips the ionospheric correction (the iono-free combination already
// eliminates the 1/f² ionospheric delay). Troposphere is still applied
// because it is non-dispersive.
//
struct PairedObservation
{
    JimmyPaputto::RawObservation obs;
    SatelliteState sv;
    std::optional<double> ionoFreePr;  // Iono-free pseudorange (m), if dual-freq

    // Carrier phase fields (populated by CarrierPhase::validateCarrierPhase)
    std::optional<double> cpMeters;    // Carrier phase measurement (m)
    double wavelength = 0.0;           // Signal wavelength (m)
    bool   cpUsable = false;           // True if phase is valid + no cycle slip
};

// ── Coordinate conversions ──────────────────────────────────────────

inline Lla ecef2lla(const Ecef& ecef)
{
    const double p = std::sqrt(ecef.x * ecef.x + ecef.y * ecef.y);
    const double lon = std::atan2(ecef.y, ecef.x);

    // Iterative latitude computation (Bowring method)
    double lat = std::atan2(ecef.z, p * (1.0 - WGS84_E2));
    for (int i = 0; i < 10; ++i)
    {
        const double sinLat = std::sin(lat);
        const double N = WGS84_A / std::sqrt(1.0 - WGS84_E2 * sinLat * sinLat);
        lat = std::atan2(ecef.z + WGS84_E2 * N * sinLat, p);
    }

    const double sinLat = std::sin(lat);
    const double N = WGS84_A / std::sqrt(1.0 - WGS84_E2 * sinLat * sinLat);
    const double alt = p / std::cos(lat) - N;

    return { lat, lon, alt };
}

inline Ecef lla2ecef(const Lla& lla)
{
    const double sinLat = std::sin(lla.lat_rad);
    const double cosLat = std::cos(lla.lat_rad);
    const double sinLon = std::sin(lla.lon_rad);
    const double cosLon = std::cos(lla.lon_rad);
    const double N = WGS84_A / std::sqrt(1.0 - WGS84_E2 * sinLat * sinLat);

    return {
        (N + lla.alt) * cosLat * cosLon,
        (N + lla.alt) * cosLat * sinLon,
        (N * (1.0 - WGS84_E2) + lla.alt) * sinLat
    };
}

inline Enu ecef2enu(const Ecef& point, const Ecef& ref)
{
    const auto refLla = ecef2lla(ref);
    const double sinLat = std::sin(refLla.lat_rad);
    const double cosLat = std::cos(refLla.lat_rad);
    const double sinLon = std::sin(refLla.lon_rad);
    const double cosLon = std::cos(refLla.lon_rad);

    const double dx = point.x - ref.x;
    const double dy = point.y - ref.y;
    const double dz = point.z - ref.z;

    return {
        -sinLon * dx + cosLon * dy,
        -sinLat * cosLon * dx - sinLat * sinLon * dy + cosLat * dz,
         cosLat * cosLon * dx + cosLat * sinLon * dy + sinLat * dz
    };
}

// ── Simple troposphere fallback (used when no CorrectionModel given) ─
inline double troposphereDelaySimple(double elevationRad)
{
    constexpr double zenithDelay = 2.3;
    const double sinEl = std::sin(elevationRad);
    if (sinEl < 0.05) return zenithDelay / 0.05;
    return zenithDelay / sinEl;
}

// ── Atmospheric correction model ────────────────────────────────────
//
// Pluggable correction functions used by the solver. Each function
// takes the receiver position, satellite geometry, and GPS time, and
// returns a delay in meters to add to the modelled geometric range.
//
// Default: simple 2.3 m/sin(el) troposphere, no ionosphere.
// Replace with UNB3m + Klobuchar for improved accuracy.
//
struct CorrectionModel
{
    // (receiverLla, elevationRad, azimuthRad, gpsTow) → delay (m)
    std::function<double(const Lla&, double, double, double)> troposphere;
    std::function<double(const Lla&, double, double, double)> ionosphere;

    CorrectionModel()
        : troposphere([](const Lla&, double el, double, double) {
              return troposphereDelaySimple(el);
          })
        , ionosphere([](const Lla&, double, double, double) {
              return 0.0;
          })
    {}
};

// ── Elevation and azimuth from receiver to satellite ────────────────
inline double elevationAngle(const Ecef& receiver, const Ecef& satellite)
{
    const auto enu = ecef2enu(satellite, receiver);
    const double horizDist = std::sqrt(enu.e * enu.e + enu.n * enu.n);
    return std::atan2(enu.u, horizDist);
}

inline double azimuthAngle(const Ecef& receiver, const Ecef& satellite)
{
    const auto enu = ecef2enu(satellite, receiver);
    return std::atan2(enu.e, enu.n);  // 0 = North, π/2 = East
}

// ── Earth rotation correction ───────────────────────────────────────
inline Ecef earthRotationCorrection(const Ecef& svPos, double transitTime)
{
    const double angle = OMEGA_E * transitTime;
    const double cosA = std::cos(angle);
    const double sinA = std::sin(angle);
    return {
         cosA * svPos.x + sinA * svPos.y,
        -sinA * svPos.x + cosA * svPos.y,
         svPos.z
    };
}

// ── Weighted Least-Squares position solver ──────────────────────────
//
// Solves for [x, y, z, cdt] where cdt is receiver clock bias in meters.
// Uses pseudoranges corrected for atmospheric delays (troposphere +
// ionosphere) and satellite clock errors.
//
// The CorrectionModel provides pluggable atmospheric delay functions.
// Weights observations by C/N0.
//
// gpsTow is required for time-dependent corrections (ionosphere).
//
inline std::optional<PositionSolution> solvePosition(
    const std::vector<PairedObservation>& paired,
    const Ecef& initialGuess = {0, 0, 0},
    int maxIterations = 20,
    double convergenceThreshold = 1e-4,
    double gpsTow = 0.0,
    const CorrectionModel& corrections = CorrectionModel{})
{
    if (paired.size() < 4)
        return std::nullopt;

    const int n = static_cast<int>(paired.size());

    // State vector: [x, y, z, cdt]
    double state[4] = {initialGuess.x, initialGuess.y, initialGuess.z, 0.0};

    // If initial guess is origin, use average of satellite positions
    if (state[0] == 0 && state[1] == 0 && state[2] == 0)
    {
        for (const auto& p : paired)
        {
            state[0] += p.sv.position.x;
            state[1] += p.sv.position.y;
            state[2] += p.sv.position.z;
        }
        state[0] /= n;
        state[1] /= n;
        state[2] /= n;
    }

    bool converged = false;
    int iter = 0;
    double residualRms = 0;

    for (iter = 0; iter < maxIterations; ++iter)
    {
        // Build H matrix (n x 4) and residual vector (n x 1)
        // Using normal equations: H^T W H dx = H^T W dz
        double HTwH[4][4] = {};
        double HTwdz[4] = {};
        double sumWeightedResid2 = 0;
        double sumWeight = 0;

        const Ecef rxPos = {state[0], state[1], state[2]};

        for (int i = 0; i < n; ++i)
        {
            const auto& p = paired[i];

            // Geometric range
            const double dx = p.sv.position.x - state[0];
            const double dy = p.sv.position.y - state[1];
            const double dz = p.sv.position.z - state[2];
            const double range = std::sqrt(dx * dx + dy * dy + dz * dz);

            if (range < 1.0) continue;

            // Direction cosines
            const double ax = -dx / range;
            const double ay = -dy / range;
            const double az = -dz / range;

            // Expected pseudorange = range + cdt - svClockBias*c + tropo + iono
            //
            // When a dual-frequency iono-free pseudorange is available,
            // use it as the measurement and skip the iono model (the IF
            // combination already removes the first-order iono delay).
            // Troposphere correction is always applied (non-dispersive).
            //
            const double elev = elevationAngle(rxPos, p.sv.position);
            const double azim = azimuthAngle(rxPos, p.sv.position);
            const auto rxLla = ecef2lla(rxPos);
            const double tropo = corrections.troposphere(rxLla, elev, azim, gpsTow);
            const double iono  = p.ionoFreePr.has_value()
                ? 0.0  // IF combination already iono-free
                : corrections.ionosphere(rxLla, elev, azim, gpsTow);
            const double measurement = p.ionoFreePr.value_or(p.obs.prMes);
            const double expected = range + state[3]
                                  - p.sv.clockBias * C + tropo + iono;

            // Residual
            const double resid = measurement - expected;

            // Weight by C/N0 (linear scale: higher C/N0 = more weight)
            const double w = static_cast<double>(p.obs.cno) / 40.0;

            // Row of H: [ax, ay, az, 1]
            const double H[4] = {ax, ay, az, 1.0};

            // Accumulate normal equations
            for (int r = 0; r < 4; ++r)
            {
                for (int c = 0; c < 4; ++c)
                    HTwH[r][c] += w * H[r] * H[c];
                HTwdz[r] += w * H[r] * resid;
            }

            sumWeightedResid2 += w * resid * resid;
            sumWeight += w;
        }

        // Solve 4x4 system by Gaussian elimination
        double A[4][5];
        for (int r = 0; r < 4; ++r)
        {
            for (int c = 0; c < 4; ++c)
                A[r][c] = HTwH[r][c];
            A[r][4] = HTwdz[r];
        }

        for (int col = 0; col < 4; ++col)
        {
            // Partial pivoting
            int maxRow = col;
            for (int row = col + 1; row < 4; ++row)
                if (std::fabs(A[row][col]) > std::fabs(A[maxRow][col]))
                    maxRow = row;

            if (maxRow != col)
                for (int j = 0; j < 5; ++j)
                    std::swap(A[col][j], A[maxRow][j]);

            if (std::fabs(A[col][col]) < 1e-15)
                return std::nullopt; // Singular

            for (int row = col + 1; row < 4; ++row)
            {
                const double factor = A[row][col] / A[col][col];
                for (int j = col; j < 5; ++j)
                    A[row][j] -= factor * A[col][j];
            }
        }

        double dx_sol[4];
        for (int row = 3; row >= 0; --row)
        {
            dx_sol[row] = A[row][4];
            for (int col = row + 1; col < 4; ++col)
                dx_sol[row] -= A[row][col] * dx_sol[col];
            dx_sol[row] /= A[row][row];
        }

        // Update state
        for (int i = 0; i < 4; ++i)
            state[i] += dx_sol[i];

        residualRms = (sumWeight > 0)
            ? std::sqrt(sumWeightedResid2 / sumWeight)
            : 0;

        const double step = std::sqrt(
            dx_sol[0] * dx_sol[0] +
            dx_sol[1] * dx_sol[1] +
            dx_sol[2] * dx_sol[2]);

        if (step < convergenceThreshold)
        {
            converged = true;
            break;
        }
    }

    // Compute DOP from (H^T H)^-1 (recompute H at final position)
    double HTH[4][4] = {};
    const Ecef finalPos = {state[0], state[1], state[2]};

    for (int i = 0; i < n; ++i)
    {
        const double dx = paired[i].sv.position.x - state[0];
        const double dy = paired[i].sv.position.y - state[1];
        const double dz = paired[i].sv.position.z - state[2];
        const double range = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (range < 1.0) continue;

        const double H[4] = {-dx / range, -dy / range, -dz / range, 1.0};
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c)
                HTH[r][c] += H[r] * H[c];
    }

    // Invert 4x4 via augmented matrix
    double Aug[4][8] = {};
    for (int r = 0; r < 4; ++r)
    {
        for (int c = 0; c < 4; ++c)
            Aug[r][c] = HTH[r][c];
        Aug[r][r + 4] = 1.0;
    }
    for (int col = 0; col < 4; ++col)
    {
        int maxRow = col;
        for (int row = col + 1; row < 4; ++row)
            if (std::fabs(Aug[row][col]) > std::fabs(Aug[maxRow][col]))
                maxRow = row;
        if (maxRow != col)
            for (int j = 0; j < 8; ++j)
                std::swap(Aug[col][j], Aug[maxRow][j]);
        if (std::fabs(Aug[col][col]) < 1e-15)
            return std::nullopt;
        const double pivot = Aug[col][col];
        for (int j = 0; j < 8; ++j)
            Aug[col][j] /= pivot;
        for (int row = 0; row < 4; ++row)
        {
            if (row == col) continue;
            const double factor = Aug[row][col];
            for (int j = 0; j < 8; ++j)
                Aug[row][j] -= factor * Aug[col][j];
        }
    }

    // DOP values (transform to local ENU for HDOP/VDOP)
    const auto lla = ecef2lla(finalPos);
    const double sinLat = std::sin(lla.lat_rad);
    const double cosLat = std::cos(lla.lat_rad);
    const double sinLon = std::sin(lla.lon_rad);
    const double cosLon = std::cos(lla.lon_rad);

    // Rotation matrix ECEF -> ENU
    double R[3][3] = {
        {-sinLon,           cosLon,          0      },
        {-sinLat * cosLon, -sinLat * sinLon, cosLat },
        { cosLat * cosLon,  cosLat * sinLon, sinLat }
    };

    // Q_enu = R * Q_xyz * R^T  (3x3 position part)
    double Qxyz[3][3];
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            Qxyz[r][c] = Aug[r][c + 4];

    double Qenu[3][3] = {};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            for (int k = 0; k < 3; ++k)
                for (int l = 0; l < 3; ++l)
                    Qenu[i][j] += R[i][k] * Qxyz[k][l] * R[j][l];

    const double hdop = std::sqrt(Qenu[0][0] + Qenu[1][1]);
    const double vdop = std::sqrt(Qenu[2][2]);
    const double pdop = std::sqrt(Qenu[0][0] + Qenu[1][1] + Qenu[2][2]);

    return PositionSolution {
        .ecef = finalPos,
        .lla  = lla,
        .receiverClockBias_m = state[3],
        .hdop = hdop,
        .vdop = vdop,
        .pdop = pdop,
        .usedSatellites = n,
        .iterations = iter + 1,
        .residualRms = residualRms,
        .converged = converged
    };
}

// ── Filter valid GPS observations for SPP ───────────────────────────
// Takes raw observations and filters to usable L1 pseudoranges
inline std::vector<JimmyPaputto::RawObservation> filterL1Observations(
    const std::vector<JimmyPaputto::RawObservation>& observations,
    double minCno = 15.0,
    double minElevation_deg = 5.0)
{
    std::vector<JimmyPaputto::RawObservation> filtered;

    for (const auto& obs : observations)
    {
        if (!obs.prValid) continue;
        if (obs.cno < minCno) continue;

        // Accept L1 signals: GPS L1CA (sigId 0), Galileo E1 (0,1),
        // GLONASS L1OF (0), BeiDou B1 (0,1)
        bool isL1 = false;
        switch (obs.gnssId)
        {
            case JimmyPaputto::EGnssId::GPS:      isL1 = (obs.sigId == 0); break;
            case JimmyPaputto::EGnssId::Galileo:   isL1 = (obs.sigId <= 1); break;
            case JimmyPaputto::EGnssId::GLONASS:   isL1 = (obs.sigId == 0); break;
            case JimmyPaputto::EGnssId::BeiDou:    isL1 = (obs.sigId <= 1); break;
            case JimmyPaputto::EGnssId::SBAS:      isL1 = (obs.sigId == 0); break;
            default: break;
        }

        if (isL1)
            filtered.push_back(obs);
    }

    return filtered;
}

}  // GnssMath

#endif  // GNSS_MATH_HPP_

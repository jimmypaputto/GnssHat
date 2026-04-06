/*
 * Jimmy Paputto 2026
 *
 * Float Ambiguity Estimation via Extended Least-Squares
 *
 * Extends the SPP solver to jointly estimate receiver position and
 * carrier phase ambiguities as real (float) numbers. This is the
 * first step toward integer ambiguity resolution:
 *
 *   1. Float solution: solve for [x, y, z, cdt, N1, N2, ..., Nm]
 *      using both pseudorange and carrier phase observations
 *   2. Extract the ambiguity covariance matrix Q_NN
 *   3. Pass float ambiguities + Q_NN to LAMBDA for integer fixing
 *
 * Measurement model (per satellite i):
 *
 *   Code:   P_i = ρ_i + cdt - c·δt_sv,i + T_i + I_i + ε_P
 *   Phase:  φ_i = ρ_i + cdt - c·δt_sv,i + T_i - I_i + λ·N_i + ε_φ
 *
 * When the iono-free combination is used (dual-freq), the I terms
 * cancel and both code and phase use the IF measurement.
 *
 * State vector: [x, y, z, cdt, N_1, N_2, ..., N_m]
 *   - 4 position/clock unknowns
 *   - m ambiguity unknowns (one per satellite with valid phase)
 *
 * The H matrix has 2m rows × (4+m) columns:
 *   Code row i:  [ax_i, ay_i, az_i, 1, 0, ..., 0]
 *   Phase row i: [ax_i, ay_i, az_i, 1, 0, ..., λ_i, ..., 0]
 *                                                ^col 4+i
 *
 * Weighting: phase gets much higher weight than code because its
 * noise is ~1000× smaller (σ_φ ≈ 2 mm vs σ_P ≈ 2 m).
 */

#ifndef AMBIGUITY_FLOAT_HPP_
#define AMBIGUITY_FLOAT_HPP_

#include <algorithm>
#include <cmath>
#include <cstring>
#include <optional>
#include <vector>

#include "GnssMath.hpp"
#include "CarrierPhase.hpp"

namespace AmbiguityFloat
{

// ── Observation with both code and carrier phase ────────────────────
struct DualObservation
{
    GnssMath::PairedObservation code;   // Pseudorange observation + SV state
    CarrierPhase::CpObservation phase;  // Validated carrier phase (meters)
    int ambiguityIndex;                 // Index into ambiguity state (0..m-1)
};

// ── Float solution result ───────────────────────────────────────────
struct FloatSolution
{
    GnssMath::Ecef ecef;
    GnssMath::Lla lla;
    double receiverClockBias_m;

    // Float ambiguities (cycles)
    std::vector<double> ambiguities;        // N_i (float, in cycles)
    std::vector<double> wavelengths;        // λ_i for each ambiguity

    // Full covariance of ambiguities Q_NN (m×m, row-major)
    // This is the critical input for LAMBDA
    std::vector<double> Q_NN;

    // Cross-covariance Q_xN (4×m, row-major)
    // Used for fixed solution: x_fix = x_float - Q_xN · Q_NN^-1 · (N_float - N_fix)
    std::vector<double> Q_xN;

    int usedSatellites;
    int phaseObservations;
    int iterations;
    double residualRms;
    bool converged;

    double hdop, vdop, pdop;
};

// ── Dense matrix utilities (row-major) ──────────────────────────────
//
// Minimal inline helpers for the extended normal equations.
// These operate on flat std::vector<double> with explicit dimensions.
//

// A[rows×cols], x[cols], y[rows] : y = A*x
inline void matVecMul(const double* A, const double* x, double* y,
                      int rows, int cols)
{
    for (int i = 0; i < rows; ++i)
    {
        y[i] = 0;
        for (int j = 0; j < cols; ++j)
            y[i] += A[i * cols + j] * x[j];
    }
}

// Solve A*x = b for x, A is n×n, modifies A and b in place
// Returns false if singular
inline bool solveLinearSystem(std::vector<double>& A, std::vector<double>& b, int n)
{
    // Gaussian elimination with partial pivoting
    for (int col = 0; col < n; ++col)
    {
        // Find pivot
        int maxRow = col;
        double maxVal = std::fabs(A[col * n + col]);
        for (int row = col + 1; row < n; ++row)
        {
            double val = std::fabs(A[row * n + col]);
            if (val > maxVal) { maxVal = val; maxRow = row; }
        }

        if (maxVal < 1e-15) return false;

        // Swap rows
        if (maxRow != col)
        {
            for (int j = 0; j < n; ++j)
                std::swap(A[col * n + j], A[maxRow * n + j]);
            std::swap(b[col], b[maxRow]);
        }

        // Eliminate below
        for (int row = col + 1; row < n; ++row)
        {
            double factor = A[row * n + col] / A[col * n + col];
            for (int j = col; j < n; ++j)
                A[row * n + j] -= factor * A[col * n + j];
            b[row] -= factor * b[col];
        }
    }

    // Back substitution
    for (int row = n - 1; row >= 0; --row)
    {
        for (int col = row + 1; col < n; ++col)
            b[row] -= A[row * n + col] * b[col];
        b[row] /= A[row * n + row];
    }

    return true;
}

// Invert n×n matrix A in-place via Gauss-Jordan
// Returns false if singular
inline bool invertMatrix(std::vector<double>& A, int n)
{
    std::vector<double> Aug(n * 2 * n, 0.0);
    for (int r = 0; r < n; ++r)
    {
        for (int c = 0; c < n; ++c)
            Aug[r * 2 * n + c] = A[r * n + c];
        Aug[r * 2 * n + n + r] = 1.0;
    }

    for (int col = 0; col < n; ++col)
    {
        int maxRow = col;
        for (int row = col + 1; row < n; ++row)
            if (std::fabs(Aug[row * 2 * n + col]) > std::fabs(Aug[maxRow * 2 * n + col]))
                maxRow = row;

        if (maxRow != col)
            for (int j = 0; j < 2 * n; ++j)
                std::swap(Aug[col * 2 * n + j], Aug[maxRow * 2 * n + j]);

        double pivot = Aug[col * 2 * n + col];
        if (std::fabs(pivot) < 1e-15) return false;

        for (int j = 0; j < 2 * n; ++j)
            Aug[col * 2 * n + j] /= pivot;

        for (int row = 0; row < n; ++row)
        {
            if (row == col) continue;
            double factor = Aug[row * 2 * n + col];
            for (int j = 0; j < 2 * n; ++j)
                Aug[row * 2 * n + j] -= factor * Aug[col * 2 * n + j];
        }
    }

    for (int r = 0; r < n; ++r)
        for (int c = 0; c < n; ++c)
            A[r * n + c] = Aug[r * 2 * n + n + c];

    return true;
}

// ── Float ambiguity solver ──────────────────────────────────────────
//
// Joint estimation of position + clock + float ambiguities from
// pseudorange and carrier phase observations.
//
// The solver runs iterative weighted least-squares with an extended
// state vector. On each iteration:
//   1. Linearize around current position estimate
//   2. Form normal equations with code + phase observations
//   3. Solve for position correction + ambiguity updates
//   4. Check convergence on position component
//
// After convergence, extract the ambiguity covariance Q_NN from
// the full inverse of the normal equation matrix.
//
inline std::optional<FloatSolution> solveFloat(
    const std::vector<DualObservation>& observations,
    const GnssMath::Ecef& initialGuess = {0, 0, 0},
    int maxIterations = 20,
    double convergenceThreshold = 1e-4,
    double gpsTow = 0.0,
    const GnssMath::CorrectionModel& corrections = GnssMath::CorrectionModel{})
{
    const int m = static_cast<int>(observations.size());
    if (m < 4) return std::nullopt;

    const int stateSize = 4 + m;  // [x, y, z, cdt, N_1..N_m]
    const int numObs = 2 * m;     // code + phase per satellite

    // Initialize state
    std::vector<double> state(stateSize, 0.0);
    state[0] = initialGuess.x;
    state[1] = initialGuess.y;
    state[2] = initialGuess.z;
    // state[3] = cdt (0)
    // state[4..4+m-1] = ambiguities (initialized from first iteration)

    // Initialize ambiguities from code-phase difference:
    //   N_i ≈ (P_i - φ_i) / λ_i   (rough initial estimate)
    bool ambigInitialized = false;

    if (state[0] == 0 && state[1] == 0 && state[2] == 0)
    {
        for (const auto& obs : observations)
        {
            state[0] += obs.code.sv.position.x;
            state[1] += obs.code.sv.position.y;
            state[2] += obs.code.sv.position.z;
        }
        state[0] /= m;
        state[1] /= m;
        state[2] /= m;
    }

    bool converged = false;
    int iter = 0;
    double residualRms = 0;

    // Normal equation matrix and RHS (rebuilt each iteration)
    std::vector<double> N_mat(stateSize * stateSize);
    std::vector<double> rhs(stateSize);

    for (iter = 0; iter < maxIterations; ++iter)
    {
        std::fill(N_mat.begin(), N_mat.end(), 0.0);
        std::fill(rhs.begin(), rhs.end(), 0.0);
        double sumWeightedResid2 = 0;
        double sumWeight = 0;

        const GnssMath::Ecef rxPos = {state[0], state[1], state[2]};

        for (int i = 0; i < m; ++i)
        {
            const auto& obs = observations[i];
            const auto& sv = obs.code.sv;

            // Geometric range
            const double dx = sv.position.x - state[0];
            const double dy = sv.position.y - state[1];
            const double dz = sv.position.z - state[2];
            const double range = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (range < 1.0) continue;

            // Direction cosines
            const double ax = -dx / range;
            const double ay = -dy / range;
            const double az = -dz / range;

            // Atmospheric corrections
            const double elev = GnssMath::elevationAngle(rxPos, sv.position);
            const double azim = GnssMath::azimuthAngle(rxPos, sv.position);
            const auto rxLla = GnssMath::ecef2lla(rxPos);
            const double tropo = corrections.troposphere(rxLla, elev, azim, gpsTow);

            // Ionosphere: zero if using IF combo, otherwise modelled
            const double ionoCode = obs.code.ionoFreePr.has_value()
                ? 0.0
                : corrections.ionosphere(rxLla, elev, azim, gpsTow);
            // Phase iono has opposite sign (but zero for IF)
            const double ionoPhase = -ionoCode;

            const double svClockCorrection = sv.clockBias * GnssMath::C;

            // ── Initialize ambiguities on first iteration ───────
            if (!ambigInitialized)
            {
                const double codeMeas = obs.code.ionoFreePr.value_or(obs.code.obs.prMes);
                const double phaseMeas = obs.phase.cpMeters;
                // N ≈ (code - phase) / λ  (includes iono + noise, rough)
                state[4 + i] = (codeMeas - phaseMeas) / obs.phase.wavelength;
            }

            // ── Code observation ────────────────────────────────
            {
                const double codeMeas = obs.code.ionoFreePr.value_or(obs.code.obs.prMes);
                const double expected = range + state[3] - svClockCorrection
                                      + tropo + ionoCode;
                const double resid = codeMeas - expected;

                // Weight: σ_P ≈ 2 m → w_P = 1/(σ_P²) ∝ 0.25
                // Scale by C/N0
                const double w = (static_cast<double>(obs.code.obs.cno) / 40.0) * 0.25;

                // H row: [ax, ay, az, 1, 0, ..., 0]
                // Accumulate into normal equations
                double H[4] = {ax, ay, az, 1.0};

                for (int r = 0; r < 4; ++r)
                {
                    for (int c = 0; c < 4; ++c)
                        N_mat[r * stateSize + c] += w * H[r] * H[c];
                    rhs[r] += w * H[r] * resid;
                }

                sumWeightedResid2 += w * resid * resid;
                sumWeight += w;
            }

            // ── Phase observation ───────────────────────────────
            {
                const double phaseMeas = obs.phase.cpMeters;
                const double lambda = obs.phase.wavelength;
                const double expected = range + state[3] - svClockCorrection
                                      + tropo + ionoPhase
                                      + lambda * state[4 + i];
                const double resid = phaseMeas - expected;

                // Weight: σ_φ ≈ 0.002 m → w_φ = 1/(σ_φ²) ∝ 250000
                // But we need to keep relative to code weight.
                // Ratio: (σ_P/σ_φ)² = (2/0.002)² = 1e6, but capped
                // to avoid numerical issues. Use factor of 10000.
                const double w = (static_cast<double>(obs.phase.cno) / 40.0) * 10000.0;

                // H row: [ax, ay, az, 1, 0, ..., λ, ..., 0]
                //                               ^col 4+i
                // Position part
                double H_pos[4] = {ax, ay, az, 1.0};

                // Accumulate position-position block
                for (int r = 0; r < 4; ++r)
                {
                    for (int c = 0; c < 4; ++c)
                        N_mat[r * stateSize + c] += w * H_pos[r] * H_pos[c];

                    // Position-ambiguity cross block
                    N_mat[r * stateSize + (4 + i)] += w * H_pos[r] * lambda;
                    N_mat[(4 + i) * stateSize + r] += w * lambda * H_pos[r];

                    rhs[r] += w * H_pos[r] * resid;
                }

                // Ambiguity-ambiguity diagonal
                N_mat[(4 + i) * stateSize + (4 + i)] += w * lambda * lambda;
                rhs[4 + i] += w * lambda * resid;

                sumWeightedResid2 += w * resid * resid;
                sumWeight += w;
            }
        }

        ambigInitialized = true;

        // Solve normal equations: N_mat · dx = rhs
        auto N_copy = N_mat;
        auto rhs_copy = rhs;

        if (!solveLinearSystem(N_copy, rhs_copy, stateSize))
            return std::nullopt;

        // Update state
        for (int i = 0; i < stateSize; ++i)
            state[i] += rhs_copy[i];

        residualRms = (sumWeight > 0)
            ? std::sqrt(sumWeightedResid2 / sumWeight)
            : 0;

        // Convergence check on position only
        const double step = std::sqrt(
            rhs_copy[0] * rhs_copy[0] +
            rhs_copy[1] * rhs_copy[1] +
            rhs_copy[2] * rhs_copy[2]);

        if (step < convergenceThreshold)
        {
            converged = true;
            break;
        }
    }

    // ── Extract covariance matrix from final normal equations ───────
    //
    // Q = N^-1 (inverse of normal equation matrix)
    // We need:
    //   Q_NN: ambiguity-ambiguity block (m×m) — for LAMBDA
    //   Q_xN: position-ambiguity cross block (4×m) — for fixed solution
    //
    auto Q_full = N_mat;  // Use last iteration's normal matrix
    if (!invertMatrix(Q_full, stateSize))
        return std::nullopt;

    // Extract Q_NN (rows/cols 4..4+m-1)
    std::vector<double> Q_NN(m * m);
    for (int r = 0; r < m; ++r)
        for (int c = 0; c < m; ++c)
            Q_NN[r * m + c] = Q_full[(4 + r) * stateSize + (4 + c)];

    // Extract Q_xN (rows 0..3, cols 4..4+m-1)
    std::vector<double> Q_xN(4 * m);
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < m; ++c)
            Q_xN[r * m + c] = Q_full[r * stateSize + (4 + c)];

    // Extract ambiguities and wavelengths
    std::vector<double> ambiguities(m);
    std::vector<double> wavelengths(m);
    for (int i = 0; i < m; ++i)
    {
        ambiguities[i] = state[4 + i];
        wavelengths[i] = observations[i].phase.wavelength;
    }

    // Compute DOP (from position part of Q)
    const GnssMath::Ecef finalPos = {state[0], state[1], state[2]};
    const auto lla = GnssMath::ecef2lla(finalPos);
    const double sinLat = std::sin(lla.lat_rad);
    const double cosLat = std::cos(lla.lat_rad);
    const double sinLon = std::sin(lla.lon_rad);
    const double cosLon = std::cos(lla.lon_rad);

    double R[3][3] = {
        {-sinLon,           cosLon,          0      },
        {-sinLat * cosLon, -sinLat * sinLon, cosLat },
        { cosLat * cosLon,  cosLat * sinLon, sinLat }
    };

    double Qxyz[3][3];
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            Qxyz[r][c] = Q_full[r * stateSize + c];

    double Qenu[3][3] = {};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            for (int k = 0; k < 3; ++k)
                for (int l = 0; l < 3; ++l)
                    Qenu[i][j] += R[i][k] * Qxyz[k][l] * R[j][l];

    return FloatSolution {
        .ecef = finalPos,
        .lla = lla,
        .receiverClockBias_m = state[3],
        .ambiguities = std::move(ambiguities),
        .wavelengths = std::move(wavelengths),
        .Q_NN = std::move(Q_NN),
        .Q_xN = std::move(Q_xN),
        .usedSatellites = m,
        .phaseObservations = m,
        .iterations = iter + 1,
        .residualRms = residualRms,
        .converged = converged,
        .hdop = std::sqrt(Qenu[0][0] + Qenu[1][1]),
        .vdop = std::sqrt(Qenu[2][2]),
        .pdop = std::sqrt(Qenu[0][0] + Qenu[1][1] + Qenu[2][2])
    };
}

}  // AmbiguityFloat

#endif  // AMBIGUITY_FLOAT_HPP_

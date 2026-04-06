/*
 * Tests for the single-point position (SPP) weighted least-squares solver.
 *
 * Uses MockData with known receiver position and synthetic pseudoranges
 * to verify position convergence accuracy, clock bias estimation, and
 * DOP computation.
 */

#include <gtest/gtest.h>
#include <cmath>

#include "GnssMath.hpp"
#include "MockData.hpp"

using namespace GnssMath;

// ── Basic SPP solution with perfect pseudoranges ────────────────────

// Pass zero-corrections since mock pseudoranges contain no atmospheric delays
static const auto NC = MockData::noCorrections();

TEST(SppSolver, ConvergesWithPerfectData)
{
    auto paired = MockData::makePairedObservations(0.0);

    auto sol = solvePosition(paired, {0,0,0}, 20, 1e-4, 0.0, NC);

    ASSERT_TRUE(sol.has_value());
    EXPECT_TRUE(sol->converged);
}

TEST(SppSolver, PositionAccuracyNoiseFree)
{
    auto paired = MockData::makePairedObservations(0.0);

    auto sol = solvePosition(paired, {0,0,0}, 20, 1e-4, 0.0, NC);
    ASSERT_TRUE(sol.has_value());
    ASSERT_TRUE(sol->converged);

    // With perfect data, should converge to within ~1 m of truth
    EXPECT_NEAR(sol->ecef.x, MockData::TRUE_X, 1.0);
    EXPECT_NEAR(sol->ecef.y, MockData::TRUE_Y, 1.0);
    EXPECT_NEAR(sol->ecef.z, MockData::TRUE_Z, 1.0);
}

TEST(SppSolver, ClockBiasEstimation)
{
    auto paired = MockData::makePairedObservations(0.0);

    auto sol = solvePosition(paired, {0,0,0}, 20, 1e-4, 0.0, NC);
    ASSERT_TRUE(sol.has_value());
    ASSERT_TRUE(sol->converged);

    // Clock bias should be close to the true value
    EXPECT_NEAR(sol->receiverClockBias_m, MockData::CLOCK_BIAS_M, 1.0);
}

TEST(SppSolver, UsesAllSatellites)
{
    auto paired = MockData::makePairedObservations(0.0);

    auto sol = solvePosition(paired, {0,0,0}, 20, 1e-4, 0.0, NC);
    ASSERT_TRUE(sol.has_value());

    EXPECT_EQ(sol->usedSatellites, 8);
}

// ── SPP with noisy pseudoranges ─────────────────────────────────────

TEST(SppSolver, ConvergesWithNoise)
{
    // 2 m pseudorange noise
    auto paired = MockData::makePairedObservations(2.0);

    auto sol = solvePosition(paired, {0,0,0}, 20, 1e-4, 0.0, NC);
    ASSERT_TRUE(sol.has_value());
    EXPECT_TRUE(sol->converged);

    // With 2 m noise and 8 SVs, position should be within ~10 m
    double dx = sol->ecef.x - MockData::TRUE_X;
    double dy = sol->ecef.y - MockData::TRUE_Y;
    double dz = sol->ecef.z - MockData::TRUE_Z;
    double error3d = std::sqrt(dx * dx + dy * dy + dz * dz);

    EXPECT_LT(error3d, 20.0);
}

// ── Minimum satellite count ─────────────────────────────────────────

TEST(SppSolver, FailsWith3Satellites)
{
    auto paired = MockData::makePairedObservations(0.0);
    paired.resize(3);  // Only 3 SVs

    auto sol = solvePosition(paired, {0,0,0}, 20, 1e-4, 0.0, NC);
    EXPECT_FALSE(sol.has_value());
}

TEST(SppSolver, WorksWith4Satellites)
{
    auto paired = MockData::makePairedObservations(0.0);
    paired.resize(4);  // Minimum: 4 SVs

    auto sol = solvePosition(paired, {0,0,0}, 20, 1e-4, 0.0, NC);
    ASSERT_TRUE(sol.has_value());
    EXPECT_TRUE(sol->converged);
}

// ── Warm start with initial guess ───────────────────────────────────

TEST(SppSolver, WarmStartConvergesFaster)
{
    auto paired = MockData::makePairedObservations(0.0);

    // Cold start (origin guess)
    auto cold = solvePosition(paired, {0, 0, 0}, 20, 1e-4, 0.0, NC);
    ASSERT_TRUE(cold.has_value());

    // Warm start (close to truth)
    Ecef guess{MockData::TRUE_X + 10, MockData::TRUE_Y + 10, MockData::TRUE_Z + 10};
    auto warm = solvePosition(paired, guess, 20, 1e-4, 0.0, NC);
    ASSERT_TRUE(warm.has_value());

    // Warm start should take fewer iterations
    EXPECT_LE(warm->iterations, cold->iterations);
}

// ── DOP values ──────────────────────────────────────────────────────

TEST(SppSolver, DopValuesReasonable)
{
    auto paired = MockData::makePairedObservations(0.0);

    auto sol = solvePosition(paired, {0,0,0}, 20, 1e-4, 0.0, NC);
    ASSERT_TRUE(sol.has_value());

    // DOP should be reasonable (1-5 range typically)
    EXPECT_GT(sol->hdop, 0.0);
    EXPECT_LT(sol->hdop, 10.0);
    EXPECT_GT(sol->vdop, 0.0);
    EXPECT_LT(sol->vdop, 10.0);
    EXPECT_GT(sol->pdop, 0.0);
    EXPECT_LT(sol->pdop, 15.0);

    // PDOP² ≈ HDOP² + VDOP²
    double pdop2 = sol->hdop * sol->hdop + sol->vdop * sol->vdop;
    EXPECT_NEAR(sol->pdop * sol->pdop, pdop2, 0.01);
}

// ── Default SolutionMode is SPP ─────────────────────────────────────

TEST(SppSolver, DefaultModeSpp)
{
    auto paired = MockData::makePairedObservations(0.0);

    auto sol = solvePosition(paired, {0,0,0}, 20, 1e-4, 0.0, NC);
    ASSERT_TRUE(sol.has_value());

    EXPECT_EQ(sol->mode, SolutionMode::SPP);
    EXPECT_EQ(sol->fixedAmbiguities, 0);
    EXPECT_NEAR(sol->ratioTest, 0.0, 1e-10);
}

// ── Iono-free pseudorange path ──────────────────────────────────────

TEST(SppSolver, UsesIonoFreePrWhenSet)
{
    auto paired = MockData::makePairedObservations(0.0);

    // Set ionoFreePr to the same clean pseudorange for all
    // (simulates iono-free combination with zero iono)
    for (auto& p : paired)
        p.ionoFreePr = p.obs.prMes;

    auto sol = solvePosition(paired, {0,0,0}, 20, 1e-4, 0.0, NC);
    ASSERT_TRUE(sol.has_value());
    EXPECT_TRUE(sol->converged);

    EXPECT_NEAR(sol->ecef.x, MockData::TRUE_X, 1.0);
    EXPECT_NEAR(sol->ecef.y, MockData::TRUE_Y, 1.0);
    EXPECT_NEAR(sol->ecef.z, MockData::TRUE_Z, 1.0);
}

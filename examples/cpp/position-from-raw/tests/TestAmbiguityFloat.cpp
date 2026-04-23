/*
 * Tests for the float ambiguity solver.
 *
 * Verifies that given consistent mock code + carrier phase observations,
 * the solver recovers the correct receiver position AND estimates float
 * ambiguities close to the true integer values.
 */

#include <gtest/gtest.h>
#include <cmath>

#include "AmbiguityFloat.hpp"
#include "MockData.hpp"

using namespace AmbiguityFloat;

static const auto NC = MockData::noCorrections();

// ── Helper: build DualObservation list from mock data ────────────────
static std::vector<DualObservation> makeDualObs(int baseAmbiguity = 1000000)
{
    auto paired = MockData::makePairedWithPhase(0.0, baseAmbiguity);
    const auto svs = MockData::satellites();
    std::vector<DualObservation> obs;

    for (size_t i = 0; i < paired.size(); ++i)
    {
        CarrierPhase::CpObservation cpData{};
        cpData.cpMeters   = *paired[i].cpMeters;
        cpData.wavelength = paired[i].wavelength;
        cpData.frequency  = MockData::C / paired[i].wavelength;
        cpData.locktime   = 5000;
        cpData.cno        = 42;
        cpData.valid       = true;

        obs.push_back({
            .code = paired[i],
            .phase = cpData,
            .ambiguityIndex = static_cast<int>(i)
        });
    }
    return obs;
}

// ── Float solution convergence ──────────────────────────────────────

TEST(AmbiguityFloatSolver, ConvergesWithPerfectData)
{
    auto obs = makeDualObs();

    auto sol = solveFloat(obs, {0,0,0}, 20, 1e-4, 0.0, NC);

    ASSERT_TRUE(sol.has_value());
    EXPECT_TRUE(sol->converged);
}

TEST(AmbiguityFloatSolver, PositionAccuracy)
{
    auto obs = makeDualObs();

    auto sol = solveFloat(obs, {0,0,0}, 20, 1e-4, 0.0, NC);
    ASSERT_TRUE(sol.has_value());
    ASSERT_TRUE(sol->converged);

    // Phase-aided position should be at least as good as SPP
    EXPECT_NEAR(sol->ecef.x, MockData::TRUE_X, 2.0);
    EXPECT_NEAR(sol->ecef.y, MockData::TRUE_Y, 2.0);
    EXPECT_NEAR(sol->ecef.z, MockData::TRUE_Z, 2.0);
}

TEST(AmbiguityFloatSolver, ClockBiasAccuracy)
{
    auto obs = makeDualObs();

    auto sol = solveFloat(obs, {0,0,0}, 20, 1e-4, 0.0, NC);
    ASSERT_TRUE(sol.has_value());
    ASSERT_TRUE(sol->converged);

    EXPECT_NEAR(sol->receiverClockBias_m, MockData::CLOCK_BIAS_M, 2.0);
}

TEST(AmbiguityFloatSolver, AmbiguitiesCloseToTruth)
{
    const int baseAmb = 1000000;
    auto obs = makeDualObs(baseAmb);
    auto trueAmb = MockData::trueAmbiguities(baseAmb);

    auto sol = solveFloat(obs, {0,0,0}, 20, 1e-4, 0.0, NC);
    ASSERT_TRUE(sol.has_value());
    ASSERT_TRUE(sol->converged);

    ASSERT_EQ(sol->ambiguities.size(), trueAmb.size());

    for (size_t i = 0; i < trueAmb.size(); ++i)
    {
        // Float ambiguities should be within ~1 cycle of truth
        // (they're real numbers, so won't be exact integers)
        EXPECT_NEAR(sol->ambiguities[i], static_cast<double>(trueAmb[i]), 1.5)
            << "Ambiguity " << i << " out of range";
    }
}

TEST(AmbiguityFloatSolver, WavelengthsPreserved)
{
    auto obs = makeDualObs();

    auto sol = solveFloat(obs, {0,0,0}, 20, 1e-4, 0.0, NC);
    ASSERT_TRUE(sol.has_value());

    ASSERT_EQ(sol->wavelengths.size(), obs.size());
    for (size_t i = 0; i < obs.size(); ++i)
        EXPECT_NEAR(sol->wavelengths[i], obs[i].phase.wavelength, 1e-10);
}

// ── Covariance matrix sanity ────────────────────────────────────────

TEST(AmbiguityFloatSolver, QnnIsSymmetric)
{
    auto obs = makeDualObs();

    auto sol = solveFloat(obs, {0,0,0}, 20, 1e-4, 0.0, NC);
    ASSERT_TRUE(sol.has_value());

    const int m = static_cast<int>(sol->ambiguities.size());
    ASSERT_EQ(static_cast<int>(sol->Q_NN.size()), m * m);

    for (int i = 0; i < m; ++i)
        for (int j = i + 1; j < m; ++j)
            EXPECT_NEAR(sol->Q_NN[i * m + j], sol->Q_NN[j * m + i], 1e-6)
                << "Q_NN[" << i << "," << j << "] not symmetric";
}

TEST(AmbiguityFloatSolver, QnnDiagonalPositive)
{
    auto obs = makeDualObs();

    auto sol = solveFloat(obs, {0,0,0}, 20, 1e-4, 0.0, NC);
    ASSERT_TRUE(sol.has_value());

    const int m = static_cast<int>(sol->ambiguities.size());
    for (int i = 0; i < m; ++i)
        EXPECT_GT(sol->Q_NN[i * m + i], 0.0)
            << "Q_NN diagonal " << i << " not positive";
}

TEST(AmbiguityFloatSolver, QxnCorrectDimensions)
{
    auto obs = makeDualObs();

    auto sol = solveFloat(obs, {0,0,0}, 20, 1e-4, 0.0, NC);
    ASSERT_TRUE(sol.has_value());

    const int m = static_cast<int>(sol->ambiguities.size());
    EXPECT_EQ(static_cast<int>(sol->Q_xN.size()), 4 * m);
}

// ── DOP from float solution ─────────────────────────────────────────

TEST(AmbiguityFloatSolver, DopValuesReasonable)
{
    auto obs = makeDualObs();

    auto sol = solveFloat(obs, {0,0,0}, 20, 1e-4, 0.0, NC);
    ASSERT_TRUE(sol.has_value());

    EXPECT_GT(sol->hdop, 0.0);
    EXPECT_LT(sol->hdop, 10.0);
    EXPECT_GT(sol->vdop, 0.0);
    EXPECT_LT(sol->vdop, 10.0);
}

// ── Minimum observations ────────────────────────────────────────────

TEST(AmbiguityFloatSolver, FailsWith3Observations)
{
    auto obs = makeDualObs();
    obs.resize(3);

    auto sol = solveFloat(obs, {0,0,0}, 20, 1e-4, 0.0, NC);
    EXPECT_FALSE(sol.has_value());
}

TEST(AmbiguityFloatSolver, WorksWith5Observations)
{
    auto obs = makeDualObs();
    obs.resize(5);
    for (int i = 0; i < 5; ++i)
        obs[i].ambiguityIndex = i;

    auto sol = solveFloat(obs, {0,0,0}, 20, 1e-4, 0.0, NC);
    ASSERT_TRUE(sol.has_value());
    EXPECT_TRUE(sol->converged);
}

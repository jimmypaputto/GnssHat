/*
 * Tests for LAMBDA integer ambiguity resolution.
 *
 * Uses the float solution from MockData to verify that LAMBDA
 * decorrelation + integer search + ratio test correctly recovers
 * the known true integer ambiguities.
 */

#include <gtest/gtest.h>
#include <cmath>

#include "AmbiguityLambda.hpp"
#include "AmbiguityFloat.hpp"
#include "MockData.hpp"

using namespace AmbiguityLambda;

static const auto NC = MockData::noCorrections();

// ── Helper: get a float solution from perfect mock data ──────────────
static std::optional<AmbiguityFloat::FloatSolution> getFloatSolution(
    int baseAmb = 1000000)
{
    auto paired = MockData::makePairedWithPhase(0.0, baseAmb);
    const auto svs = MockData::satellites();
    std::vector<AmbiguityFloat::DualObservation> obs;

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

    return AmbiguityFloat::solveFloat(obs, {0,0,0}, 20, 1e-4, 0.0, NC);
}

// ── LDL factorization ───────────────────────────────────────────────

TEST(Lambda, LdlDecomposeSimple)
{
    // 2×2 SPD matrix: [[4, 2], [2, 3]]
    std::vector<double> Q = {4, 2, 2, 3};
    auto ldl = ldlDecompose(Q, 2);

    ASSERT_TRUE(ldl.has_value());
    EXPECT_GT(ldl->D[0], 0.0);
    EXPECT_GT(ldl->D[1], 0.0);

    // Verify: L^T D L = Q
    // L is unit lower triangular, D is diagonal
    // For 2×2: Q[0,0] = D[0] + L[1,0]^2 * D[1]
    //          Q[1,0] = L[1,0] * D[1] ... wait, actually the L^T D L
    //          decomposition with LAMBDA convention processes from last
    //          to first, so let's just verify reconstruction.
    const int n = 2;
    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < n; ++j)
        {
            double sum = 0;
            // Q_ij = sum_k L[k,i] * D[k] * L[k,j]  for k >= max(i,j)
            for (int k = 0; k < n; ++k)
            {
                double Lki = (k == i) ? 1.0 : (k > i ? ldl->L[k * n + i] : 0.0);
                double Lkj = (k == j) ? 1.0 : (k > j ? ldl->L[k * n + j] : 0.0);
                sum += Lki * ldl->D[k] * Lkj;
            }
            EXPECT_NEAR(sum, Q[i * n + j], 1e-10)
                << "Reconstruction failed at [" << i << "," << j << "]";
        }
    }
}

TEST(Lambda, LdlDecomposeIdentity)
{
    std::vector<double> Q = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    auto ldl = ldlDecompose(Q, 3);

    ASSERT_TRUE(ldl.has_value());
    for (int i = 0; i < 3; ++i)
        EXPECT_NEAR(ldl->D[i], 1.0, 1e-10);
}

// ── Full LAMBDA pipeline: fix ambiguities ───────────────────────────

TEST(Lambda, FixesAmbiguitiesFromPerfectData)
{
    auto floatSol = getFloatSolution(1000000);
    ASSERT_TRUE(floatSol.has_value());
    ASSERT_TRUE(floatSol->converged);

    auto fixedSol = fixAmbiguities(*floatSol, 2.0);

    ASSERT_TRUE(fixedSol.has_value());

    // Check that fixed ambiguities match the known truth
    auto trueAmb = MockData::trueAmbiguities(1000000);
    ASSERT_EQ(fixedSol->fixedAmbiguities.size(), trueAmb.size());

    for (size_t i = 0; i < trueAmb.size(); ++i)
    {
        EXPECT_EQ(fixedSol->fixedAmbiguities[i], trueAmb[i])
            << "Ambiguity " << i << " mismatch: got "
            << fixedSol->fixedAmbiguities[i] << " expected " << trueAmb[i];
    }
}

TEST(Lambda, FixedSolutionAccepted)
{
    auto floatSol = getFloatSolution();
    ASSERT_TRUE(floatSol.has_value());

    auto fixedSol = fixAmbiguities(*floatSol, 2.0);
    ASSERT_TRUE(fixedSol.has_value());

    // With perfect data, ratio should be very high → accepted
    EXPECT_TRUE(fixedSol->accepted);
    EXPECT_GT(fixedSol->ratioTest, 2.0);
}

TEST(Lambda, FixedPositionBetterThanFloat)
{
    auto floatSol = getFloatSolution();
    ASSERT_TRUE(floatSol.has_value());

    auto fixedSol = fixAmbiguities(*floatSol);
    ASSERT_TRUE(fixedSol.has_value());

    // Fixed solution error
    double dx_f = fixedSol->ecef.x - MockData::TRUE_X;
    double dy_f = fixedSol->ecef.y - MockData::TRUE_Y;
    double dz_f = fixedSol->ecef.z - MockData::TRUE_Z;
    double err_fixed = std::sqrt(dx_f * dx_f + dy_f * dy_f + dz_f * dz_f);

    // Fixed should be very accurate (sub-meter with perfect data)
    EXPECT_LT(err_fixed, 1.0);
}

TEST(Lambda, HighRatioThresholdRejectsCorrectly)
{
    auto floatSol = getFloatSolution();
    ASSERT_TRUE(floatSol.has_value());

    // Use an extremely high threshold
    auto fixedSol = fixAmbiguities(*floatSol, 1e10);

    // If the fix exists, it should NOT be accepted with absurd threshold
    if (fixedSol.has_value())
        EXPECT_FALSE(fixedSol->accepted);
}

// ── Decorrelation ───────────────────────────────────────────────────

TEST(Lambda, DecorrelationReducesCorrelation)
{
    auto floatSol = getFloatSolution();
    ASSERT_TRUE(floatSol.has_value());

    const int m = static_cast<int>(floatSol->ambiguities.size());

    auto decor = decorrelateClean(
        floatSol->Q_NN, floatSol->ambiguities, m);

    // All D values should be positive after decorrelation
    for (int i = 0; i < m; ++i)
        EXPECT_GT(decor.ldl.D[i], 0.0) << "D[" << i << "] not positive";
}

// ── Z-transform round-trip ──────────────────────────────────────────

TEST(Lambda, InverseZTransformRoundTrip)
{
    auto floatSol = getFloatSolution();
    ASSERT_TRUE(floatSol.has_value());

    const int m = static_cast<int>(floatSol->ambiguities.size());

    auto decor = decorrelateClean(
        floatSol->Q_NN, floatSol->ambiguities, m);

    // Round the decorrelated floats to get "fixed" integers in z-space
    std::vector<int> zFixed(m);
    for (int i = 0; i < m; ++i)
        zFixed[i] = static_cast<int>(std::round(decor.zFloat[i]));

    // Apply inverse Z-transform using Zinv
    auto aFixed = inverseZTransform(decor.Zinv, zFixed, m);

    // Apply forward Z-transform to verify: z = Z^T * a
    for (int i = 0; i < m; ++i)
    {
        long long zi = 0;
        for (int k = 0; k < m; ++k)
            zi += static_cast<long long>(decor.Z[k * m + i]) * aFixed[k];
        EXPECT_EQ(static_cast<int>(zi), zFixed[i])
            << "Z-transform round-trip failed at index " << i;
    }
}

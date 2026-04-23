/*
 * Tests for carrier phase validation and cycle slip detection.
 */

#include <gtest/gtest.h>
#include <cmath>

#include "CarrierPhase.hpp"
#include "MockData.hpp"

using namespace CarrierPhase;
using namespace JimmyPaputto;

// ── Helper to make a raw observation with specific CP fields ────────
static RawObservation makeObs(
    double cpMes, float doMes, uint16_t locktime,
    uint8_t cno = 40, bool cpValid = true, bool halfCyc = false)
{
    RawObservation obs{};
    obs.gnssId    = EGnssId::GPS;
    obs.svId      = 1;
    obs.sigId     = 0;   // L1CA
    obs.freqId    = 0;
    obs.prMes     = 22000000.0;
    obs.cpMes     = cpMes;
    obs.doMes     = doMes;
    obs.locktime  = locktime;
    obs.cno       = cno;
    obs.prStdev   = 2;
    obs.cpStdev   = 1;
    obs.doStdev   = 1;
    obs.prValid   = true;
    obs.cpValid   = cpValid;
    obs.halfCyc   = halfCyc;
    obs.subHalfCyc = false;
    return obs;
}

// ── Carrier phase validation ────────────────────────────────────────

TEST(CarrierPhaseValidation, ValidObservation)
{
    auto obs = makeObs(115000000.0, -1500.0, 3000, 40, true, false);
    auto cp = validateCarrierPhase(obs);

    ASSERT_TRUE(cp.has_value());
    EXPECT_TRUE(cp->valid);
    EXPECT_GT(cp->cpMeters, 0.0);
    EXPECT_GT(cp->wavelength, 0.0);
    EXPECT_GT(cp->frequency, 0.0);
}

TEST(CarrierPhaseValidation, RejectedWhenCpInvalid)
{
    auto obs = makeObs(115000000.0, -1500.0, 3000, 40, false, false);
    auto cp = validateCarrierPhase(obs);

    EXPECT_FALSE(cp.has_value());
}

TEST(CarrierPhaseValidation, RejectedWhenHalfCycle)
{
    auto obs = makeObs(115000000.0, -1500.0, 3000, 40, true, true);
    auto cp = validateCarrierPhase(obs);

    EXPECT_FALSE(cp.has_value());
}

TEST(CarrierPhaseValidation, RejectedWhenLowCno)
{
    auto obs = makeObs(115000000.0, -1500.0, 3000, 10, true, false);
    auto cp = validateCarrierPhase(obs);

    EXPECT_FALSE(cp.has_value());
}

TEST(CarrierPhaseValidation, WavelengthIsCorrectForGpsL1)
{
    auto obs = makeObs(115000000.0, -1500.0, 3000);
    auto cp = validateCarrierPhase(obs);

    ASSERT_TRUE(cp.has_value());
    // GPS L1 wavelength ≈ 0.1903 m
    EXPECT_NEAR(cp->wavelength, 299792458.0 / 1575.42e6, 1e-6);
}

TEST(CarrierPhaseValidation, CpMetersEqualsWavelengthTimesCycles)
{
    double cycles = 115000000.0;
    auto obs = makeObs(cycles, -1500.0, 3000);
    auto cp = validateCarrierPhase(obs);

    ASSERT_TRUE(cp.has_value());
    EXPECT_NEAR(cp->cpMeters, cycles * cp->wavelength, 0.001);
}

// ── Cycle slip detection ────────────────────────────────────────────

TEST(SlipDetector, FirstEpochReturnsFalse)
{
    SlipDetector detector;

    bool arcOk = detector.checkAndUpdate(
        EGnssId::GPS, 1, 0, 115000000.0, -1500.0, 3000, 100.0);

    // First observation — new arc, returns false
    EXPECT_FALSE(arcOk);
}

TEST(SlipDetector, ContinuousArcDetected)
{
    SlipDetector detector;

    // Epoch 1: initialize arc
    double cp = 115000000.0;
    float doppler = -1500.0;  // Hz
    detector.checkAndUpdate(EGnssId::GPS, 1, 0, cp, doppler, 3000, 100.0);

    // Epoch 2: consistent phase change (Doppler * dt cycles)
    // Doppler = -1500 Hz → phase changes by ~-1500 cycles/s * 1s = -1500 cycles
    double dt = 1.0;
    double cpNext = cp + (-doppler * dt);  // predicted: +1500 cycles
    bool arcOk = detector.checkAndUpdate(
        EGnssId::GPS, 1, 0, cpNext, doppler, 3100, 101.0);

    EXPECT_TRUE(arcOk);
}

TEST(SlipDetector, CycleSlipDetected)
{
    SlipDetector detector;

    double cp = 115000000.0;
    float doppler = -1500.0;
    detector.checkAndUpdate(EGnssId::GPS, 1, 0, cp, doppler, 3000, 100.0);

    // Epoch 2: add a 100-cycle slip on top of the Doppler-predicted change
    double dt = 1.0;
    double cpNext = cp + (-doppler * dt) + 100.0;  // 100-cycle slip
    bool arcOk = detector.checkAndUpdate(
        EGnssId::GPS, 1, 0, cpNext, doppler, 3100, 101.0);

    // Should detect the slip → arc broken
    EXPECT_FALSE(arcOk);
}

TEST(SlipDetector, LocktimeDropDetectsSlip)
{
    SlipDetector detector;

    double cp = 115000000.0;
    float doppler = -1500.0;
    detector.checkAndUpdate(EGnssId::GPS, 1, 0, cp, doppler, 5000, 100.0);

    // Epoch 2: locktime drops from 5000 to 100 (receiver lost lock)
    double cpNext = cp + (-doppler * 1.0);
    bool arcOk = detector.checkAndUpdate(
        EGnssId::GPS, 1, 0, cpNext, doppler, 100, 101.0);

    EXPECT_FALSE(arcOk);
}

TEST(SlipDetector, ArcLengthIncrementsCorrectly)
{
    SlipDetector detector;

    double cp = 115000000.0;
    float doppler = -1500.0;
    detector.checkAndUpdate(EGnssId::GPS, 1, 0, cp, doppler, 3000, 100.0);
    EXPECT_EQ(detector.getArcLength(EGnssId::GPS, 1, 0), 1);

    for (int i = 1; i <= 5; ++i)
    {
        double cpNext = cp + (-doppler * i);
        detector.checkAndUpdate(
            EGnssId::GPS, 1, 0, cpNext, doppler, 3000 + i * 100, 100.0 + i);
    }

    EXPECT_EQ(detector.getArcLength(EGnssId::GPS, 1, 0), 6);
}

TEST(SlipDetector, IndependentPerSatellite)
{
    SlipDetector detector;

    // Two different satellites
    detector.checkAndUpdate(EGnssId::GPS, 1, 0, 115000000.0, -1500.0, 3000, 100.0);
    detector.checkAndUpdate(EGnssId::GPS, 5, 0, 120000000.0, -2000.0, 4000, 100.0);

    // Continue SV 1 normally
    bool ok1 = detector.checkAndUpdate(
        EGnssId::GPS, 1, 0, 115000000.0 + 1500.0, -1500.0, 3100, 101.0);
    EXPECT_TRUE(ok1);

    // Slip on SV 5
    bool ok5 = detector.checkAndUpdate(
        EGnssId::GPS, 5, 0, 120000000.0 + 2000.0 + 50.0, -2000.0, 4100, 101.0);
    EXPECT_FALSE(ok5);

    // SV 1 should still have arc length 2, SV 5 reset to 1
    EXPECT_EQ(detector.getArcLength(EGnssId::GPS, 1, 0), 2);
    EXPECT_EQ(detector.getArcLength(EGnssId::GPS, 5, 0), 1);
}

TEST(SlipDetector, ResetClearsAll)
{
    SlipDetector detector;
    detector.checkAndUpdate(EGnssId::GPS, 1, 0, 115000000.0, -1500.0, 3000, 100.0);
    detector.reset();

    EXPECT_EQ(detector.getArcLength(EGnssId::GPS, 1, 0), 0);
}

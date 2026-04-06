/*
 * Tests for dual-frequency processing:
 *   - Iono-free combination
 *   - Signal classification (L1/L5)
 *   - Carrier frequency lookup
 *   - Observation pairing
 *   - Melbourne-Wübbena wide-lane
 */

#include <gtest/gtest.h>
#include <cmath>

#include "DualFrequency.hpp"

using namespace DualFrequency;
using namespace JimmyPaputto;

// ── Iono-free combination ───────────────────────────────────────────

TEST(DualFreq, IonoFreeEliminatesIono)
{
    // Simulated L1 and L5 pseudoranges with ionospheric delay
    const double trueRange = 22000000.0;
    const double iono_L1 = 10.0;  // 10 m iono on L1

    // Iono scales as 1/f²: iono_L5 = iono_L1 * (f1/f5)²
    const double iono_L5 = iono_L1 * (GPS_L1 * GPS_L1) / (GPS_L5 * GPS_L5);

    const double pr1 = trueRange + iono_L1;
    const double pr5 = trueRange + iono_L5;

    double prIF = ionoFreeRange(GPS_L1, GPS_L5, pr1, pr5);

    // IF combination should recover the true range (iono eliminated)
    EXPECT_NEAR(prIF, trueRange, 0.001);
}

TEST(DualFreq, IonoFreeWithZeroIono)
{
    const double range = 20000000.0;
    double prIF = ionoFreeRange(GPS_L1, GPS_L5, range, range);
    EXPECT_NEAR(prIF, range, 0.001);
}

// ── Signal classification ───────────────────────────────────────────

TEST(DualFreq, GpsL1Classification)
{
    EXPECT_TRUE(isL1Signal(EGnssId::GPS, 0));
    EXPECT_FALSE(isL1Signal(EGnssId::GPS, 6));
    EXPECT_FALSE(isL1Signal(EGnssId::GPS, 7));
}

TEST(DualFreq, GpsL5Classification)
{
    EXPECT_FALSE(isL5Signal(EGnssId::GPS, 0));
    EXPECT_TRUE(isL5Signal(EGnssId::GPS, 6));
    EXPECT_TRUE(isL5Signal(EGnssId::GPS, 7));
}

TEST(DualFreq, GalileoClassification)
{
    EXPECT_TRUE(isL1Signal(EGnssId::Galileo, 0));
    EXPECT_TRUE(isL1Signal(EGnssId::Galileo, 1));
    EXPECT_FALSE(isL1Signal(EGnssId::Galileo, 3));

    EXPECT_TRUE(isL5Signal(EGnssId::Galileo, 3));
    EXPECT_TRUE(isL5Signal(EGnssId::Galileo, 4));
    EXPECT_FALSE(isL5Signal(EGnssId::Galileo, 0));
}

TEST(DualFreq, GlonassClassification)
{
    EXPECT_TRUE(isL1Signal(EGnssId::GLONASS, 0));
    EXPECT_TRUE(isL5Signal(EGnssId::GLONASS, 2));
    EXPECT_FALSE(isL1Signal(EGnssId::GLONASS, 2));
    EXPECT_FALSE(isL5Signal(EGnssId::GLONASS, 0));
}

TEST(DualFreq, BeidouClassification)
{
    EXPECT_TRUE(isL1Signal(EGnssId::BeiDou, 0));
    EXPECT_TRUE(isL1Signal(EGnssId::BeiDou, 1));
    EXPECT_TRUE(isL5Signal(EGnssId::BeiDou, 7));
    EXPECT_TRUE(isL5Signal(EGnssId::BeiDou, 8));
}

// ── Carrier frequency lookup ────────────────────────────────────────

TEST(DualFreq, GpsFrequencies)
{
    EXPECT_NEAR(carrierFrequency(EGnssId::GPS, 0, 0), GPS_L1, 1.0);
    EXPECT_NEAR(carrierFrequency(EGnssId::GPS, 6, 0), GPS_L5, 1.0);
}

TEST(DualFreq, GalileoFrequencies)
{
    EXPECT_NEAR(carrierFrequency(EGnssId::Galileo, 1, 0), GAL_E1, 1.0);
    EXPECT_NEAR(carrierFrequency(EGnssId::Galileo, 3, 0), GAL_E5a, 1.0);
}

TEST(DualFreq, GlonassFdmaFrequencies)
{
    // Slot 0 (freqId = 7): f = 1602.0 MHz
    EXPECT_NEAR(carrierFrequency(EGnssId::GLONASS, 0, 7),
                GLO_L1_BASE, 1.0);

    // Slot +1 (freqId = 8): f = 1602.0 + 0.5625 MHz
    EXPECT_NEAR(carrierFrequency(EGnssId::GLONASS, 0, 8),
                GLO_L1_BASE + GLO_L1_STEP, 1.0);

    // Slot -1 (freqId = 6): f = 1602.0 - 0.5625 MHz
    EXPECT_NEAR(carrierFrequency(EGnssId::GLONASS, 0, 6),
                GLO_L1_BASE - GLO_L1_STEP, 1.0);
}

TEST(DualFreq, BeidouFrequencies)
{
    EXPECT_NEAR(carrierFrequency(EGnssId::BeiDou, 0, 0), BDS_B1I, 1.0);
    EXPECT_NEAR(carrierFrequency(EGnssId::BeiDou, 7, 0), BDS_B2a, 1.0);
}

TEST(DualFreq, UnknownSignalReturnsZero)
{
    EXPECT_EQ(carrierFrequency(EGnssId::GPS, 99, 0), 0.0);
    EXPECT_EQ(carrierFrequency(EGnssId::SBAS, 0, 0), 0.0);
}

// ── Observation pairing ─────────────────────────────────────────────

static RawObservation makeRawObs(EGnssId gnss, uint8_t svId, uint8_t sigId,
                                  double pr, uint8_t cno = 40)
{
    RawObservation obs{};
    obs.gnssId  = gnss;
    obs.svId    = svId;
    obs.sigId   = sigId;
    obs.freqId  = 0;
    obs.prMes   = pr;
    obs.cpMes   = pr / 0.19;
    obs.doMes   = 0;
    obs.locktime = 3000;
    obs.cno     = cno;
    obs.prValid = true;
    obs.cpValid = true;
    obs.halfCyc = false;
    return obs;
}

TEST(DualFreq, PairingDualFreqGps)
{
    std::vector<RawObservation> obs;
    obs.push_back(makeRawObs(EGnssId::GPS, 5, 0, 22000000.0));  // L1
    obs.push_back(makeRawObs(EGnssId::GPS, 5, 6, 22000010.0));  // L5 (same SV)

    auto result = pairObservations(obs);

    EXPECT_EQ(result.paired.size(), 1u);
    EXPECT_EQ(result.unpaired.size(), 0u);
    EXPECT_GT(result.paired[0].ionoFreePr, 0.0);
}

TEST(DualFreq, PairingSingleFreqUnpaired)
{
    std::vector<RawObservation> obs;
    obs.push_back(makeRawObs(EGnssId::GPS, 5, 0, 22000000.0));  // L1 only

    auto result = pairObservations(obs);

    EXPECT_EQ(result.paired.size(), 0u);
    EXPECT_EQ(result.unpaired.size(), 1u);
}

TEST(DualFreq, PairingMixedFreqSatellites)
{
    std::vector<RawObservation> obs;
    // SV 5: dual-freq
    obs.push_back(makeRawObs(EGnssId::GPS, 5, 0, 22000000.0));
    obs.push_back(makeRawObs(EGnssId::GPS, 5, 6, 22000010.0));
    // SV 10: single-freq
    obs.push_back(makeRawObs(EGnssId::GPS, 10, 0, 23000000.0));
    // SV 15: dual-freq
    obs.push_back(makeRawObs(EGnssId::Galileo, 1, 1, 24000000.0));
    obs.push_back(makeRawObs(EGnssId::Galileo, 1, 3, 24000015.0));

    auto result = pairObservations(obs);

    EXPECT_EQ(result.paired.size(), 2u);
    EXPECT_EQ(result.unpaired.size(), 1u);
}

TEST(DualFreq, PairingRejectsLowCno)
{
    std::vector<RawObservation> obs;
    obs.push_back(makeRawObs(EGnssId::GPS, 5, 0, 22000000.0, 40));  // L1 ok
    obs.push_back(makeRawObs(EGnssId::GPS, 5, 6, 22000010.0, 5));   // L5 too weak

    auto result = pairObservations(obs);

    EXPECT_EQ(result.paired.size(), 0u);
    EXPECT_EQ(result.unpaired.size(), 1u);
}

// ── Melbourne-Wübbena wide-lane ─────────────────────────────────────

TEST(DualFreq, MwRejectsInvalidPhase)
{
    auto l1 = makeRawObs(EGnssId::GPS, 5, 0, 22000000.0);
    auto l5 = makeRawObs(EGnssId::GPS, 5, 6, 22000010.0);
    l1.cpValid = false;

    auto mw = melbourneWubbena(l1, l5, GPS_L1, GPS_L5);
    EXPECT_FALSE(mw.has_value());
}

TEST(DualFreq, MwRejectsHalfCycle)
{
    auto l1 = makeRawObs(EGnssId::GPS, 5, 0, 22000000.0);
    auto l5 = makeRawObs(EGnssId::GPS, 5, 6, 22000010.0);
    l1.halfCyc = true;

    auto mw = melbourneWubbena(l1, l5, GPS_L1, GPS_L5);
    EXPECT_FALSE(mw.has_value());
}

TEST(DualFreq, MwReturnsValueForValidPair)
{
    auto l1 = makeRawObs(EGnssId::GPS, 5, 0, 22000000.0);
    auto l5 = makeRawObs(EGnssId::GPS, 5, 6, 22000010.0);

    auto mw = melbourneWubbena(l1, l5, GPS_L1, GPS_L5);
    ASSERT_TRUE(mw.has_value());
    // MW value should be finite
    EXPECT_TRUE(std::isfinite(*mw));
}

// ── WideLaneResolver ────────────────────────────────────────────────

TEST(DualFreq, WideLaneNotResolvedImmediately)
{
    WideLaneResolver resolver;
    constexpr double lambda_wl = 299792458.0 / (GPS_L1 - GPS_L5);

    resolver.update(0x0005, 42.3, lambda_wl);
    EXPECT_EQ(resolver.getWideLane(0x0005), 0);
}

TEST(DualFreq, WideLaneResolvesAfterAveraging)
{
    WideLaneResolver resolver;
    constexpr double lambda_wl = 299792458.0 / (GPS_L1 - GPS_L5);

    // Feed 15 epochs with MW ≈ 42.1 (should round to 42)
    for (int i = 0; i < 15; ++i)
        resolver.update(0x0005, 42.05 + 0.01 * (i % 3 - 1), lambda_wl);

    EXPECT_EQ(resolver.getWideLane(0x0005), 42);
    EXPECT_EQ(resolver.resolvedCount(), 1);
}

TEST(DualFreq, WideLaneResetClearsState)
{
    WideLaneResolver resolver;
    constexpr double lambda_wl = 299792458.0 / (GPS_L1 - GPS_L5);

    for (int i = 0; i < 15; ++i)
        resolver.update(0x0005, 42.05, lambda_wl);

    EXPECT_NE(resolver.getWideLane(0x0005), 0);

    resolver.reset(0x0005);
    EXPECT_EQ(resolver.getWideLane(0x0005), 0);
    EXPECT_EQ(resolver.resolvedCount(), 0);
}

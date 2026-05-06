/*
 * Jimmy Paputto 2026
 *
 * Unit tests for RtcmEphemeris.hpp — bit-level decoders for RTCM
 * 1019 / 1042 / 1044 / 1046 and the GPS-TOW helpers.
 *
 * Approach: build payloads in-memory using a tiny BitWriter that
 * mirrors the MSB-first layout of detail::bitsU, then decode and
 * check the scaled fields.
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "RtcmEphemeris.hpp"

using namespace JimmyPaputto;

namespace
{
    /// Tiny MSB-first bit writer matching the layout of detail::bitsU.
    class BitWriter
    {
    public:
        explicit BitWriter(size_t reservedBytes = 256) { buf_.resize(reservedBytes, 0); }
        void putU(uint64_t value, size_t nbits)
        {
            ensure(pos_ + nbits);
            for (size_t i = 0; i < nbits; ++i)
            {
                size_t bit = pos_ + i;
                uint64_t v = (value >> (nbits - 1 - i)) & 1ULL;
                buf_[bit / 8] |= static_cast<uint8_t>(v << (7 - (bit % 8)));
            }
            pos_ += nbits;
        }
        void putS(int64_t value, size_t nbits)
        {
            // Two's-complement encode into nbits.
            uint64_t mask = (nbits >= 64) ? ~0ULL : ((1ULL << nbits) - 1ULL);
            putU(static_cast<uint64_t>(value) & mask, nbits);
        }
        const uint8_t* data() const { return buf_.data(); }
        size_t bits() const { return pos_; }

    private:
        void ensure(size_t bits)
        {
            size_t bytes = (bits + 7) / 8;
            if (bytes > buf_.size()) buf_.resize(bytes, 0);
        }
        std::vector<uint8_t> buf_;
        size_t pos_ = 0;
    };
}

// -------------------------------------------------------- GPS TOW helpers

TEST(EphemerisTow, GpsEpochYieldsZero)
{
    // Unix 315964800 = GPS epoch 1980-01-06 00:00:00 UTC; before leap
    // adjustment this maps to TOW = leapSecs.
    double tow = currentGpsTowSeconds(315964800ULL * 1000ULL, /*leap*/ 18);
    EXPECT_NEAR(tow, 18.0, 1e-9);
}

TEST(EphemerisTow, OneSecondPastEpoch)
{
    double tow = currentGpsTowSeconds((315964800ULL + 1) * 1000ULL, 18);
    EXPECT_NEAR(tow, 19.0, 1e-9);
}

TEST(EphemerisTow, MillisecondFractionPreserved)
{
    double tow = currentGpsTowSeconds(315964800ULL * 1000ULL + 250, 0);
    EXPECT_NEAR(tow, 0.250, 1e-9);
}

TEST(EphemerisTow, BdsTowLagsGpsBy14s)
{
    EXPECT_NEAR(gnssTowFromGpsTow(100.0, GnssCode::BDS), 86.0, 1e-12);
    EXPECT_NEAR(gnssTowFromGpsTow(100.0, GnssCode::GPS), 100.0, 1e-12);
    EXPECT_NEAR(gnssTowFromGpsTow(100.0, GnssCode::GAL), 100.0, 1e-12);
    EXPECT_NEAR(gnssTowFromGpsTow(100.0, GnssCode::QZSS), 100.0, 1e-12);
}

TEST(EphemerisTow, BdsTowWrapsAtWeekStart)
{
    // GPS TOW = 5 → BDS TOW = 5 - 14 + 604800 = 604791
    double t = gnssTowFromGpsTow(5.0, GnssCode::BDS);
    EXPECT_NEAR(t, 604791.0, 1e-12);
}

// ----------------------------------------------------------------- 1019

TEST(EphemerisDecode1019, DecodesIdentityAndScaling)
{
    // Build a payload where every field is zero except a few that
    // exercise unsigned/signed decoding and the scale factors.
    //
    // Wire fields after the 12-bit msg type:
    //   svId u6          = 15
    //   week u10         = 2200
    //   URA  u4          = 0
    //   CODE u2          = 0
    //   idot s14         = 0
    //   IODE u8          = 0
    //   toc  u16         = 0
    //   af2  s8 / af1 s16 / af0 s22 = 0
    //   IODC u10         = 0
    //   Crs  s16         = 0
    //   dN   s16         = 0
    //   M0   s32         = -2 → -2 * 2^-31 * π rad
    //   Cuc  s16         = 0
    //   e    u32         = 0x80000000 → e = 2^-2 = 0.25
    //   Cus  s16         = 0
    //   sqrtA u32        = 1 << 19 → sqrtA = 1.0
    //   toe  u16         = 0
    //   ... everything else 0

    BitWriter w;
    w.putU(1019, 12);          // msg type
    w.putU(15, 6);             // svId
    w.putU(800, 10);           // week (10-bit field)
    w.putU(0, 4);              // URA
    w.putU(0, 2);              // CODE
    w.putS(0, 14);             // idot
    w.putU(0, 8);              // IODE
    w.putU(0, 16);             // toc
    w.putS(0, 8);              // af2
    w.putS(0, 16);             // af1
    w.putS(0, 22);             // af0
    w.putU(0, 10);             // IODC
    w.putS(0, 16);             // Crs
    w.putS(0, 16);             // dN
    w.putS(-2, 32);            // M0 raw
    w.putS(0, 16);             // Cuc
    w.putU(0x80000000ULL, 32); // e raw → 0.25
    w.putS(0, 16);             // Cus
    w.putU(1ULL << 19, 32);    // sqrtA raw → 1.0
    w.putU(0, 16);             // toe
    w.putS(0, 16);             // Cic
    w.putS(0, 32);             // OMEGA0
    w.putS(0, 16);             // Cis
    w.putS(0, 32);             // i0
    w.putS(0, 16);             // Crc
    w.putS(0, 32);             // omega
    w.putS(0, 24);             // OMEGAdot
    w.putS(0, 8);              // tGD
    w.putU(0x2A, 6);           // health = 0x2A
    // pad up to required 12+488 = 500 bits
    while (w.bits() < 500) w.putU(0, 1);

    auto eph = decodeRtcm1019(w.data(), w.bits());
    ASSERT_TRUE(eph.has_value());
    EXPECT_EQ(eph->gnss, GnssCode::GPS);
    EXPECT_EQ(eph->svId, 15);
    EXPECT_EQ(eph->weekNumber, 800);
    EXPECT_NEAR(eph->sqrtA, 1.0, 1e-12);
    EXPECT_NEAR(eph->e, 0.25, 1e-12);
    // M0 raw = -2; scale = 2^-31 semicircles → π * raw * 2^-31 rad
    EXPECT_NEAR(eph->M0, -2.0 * std::ldexp(1.0, -31) * M_PI, 1e-18);
    EXPECT_EQ(eph->health, 0x2A);
}

TEST(EphemerisDecode1019, RejectsShortPayload)
{
    uint8_t tiny[8] = {0};
    auto eph = decodeRtcm1019(tiny, 64);
    EXPECT_FALSE(eph.has_value());
}

// ----------------------------------------------------------------- 1044

TEST(EphemerisDecode1044, QzssPrnOffsetBy192)
{
    BitWriter w;
    w.putU(1044, 12);
    w.putU(3, 4);              // svId raw = 3 → PRN 195
    // Pad with zeros up to required 12+485 = 497 bits.
    while (w.bits() < 497) w.putU(0, 1);

    auto eph = decodeRtcm1044(w.data(), w.bits());
    ASSERT_TRUE(eph.has_value());
    EXPECT_EQ(eph->gnss, GnssCode::QZSS);
    EXPECT_EQ(eph->svId, 195);
}

// ----------------------------------------------------------------- 1042

TEST(EphemerisDecode1042, BeidouIdentity)
{
    BitWriter w;
    w.putU(1042, 12);
    w.putU(7, 6);              // svId
    w.putU(800, 13);           // week
    while (w.bits() < 12 + 499) w.putU(0, 1);

    auto eph = decodeRtcm1042(w.data(), w.bits());
    ASSERT_TRUE(eph.has_value());
    EXPECT_EQ(eph->gnss, GnssCode::BDS);
    EXPECT_EQ(eph->svId, 7);
    EXPECT_EQ(eph->weekNumber, 800);
}

// ----------------------------------------------------------------- 1046

TEST(EphemerisDecode1046, GalileoIdentity)
{
    BitWriter w;
    w.putU(1046, 12);
    w.putU(11, 6);             // svId
    w.putU(1234, 12);          // week
    while (w.bits() < 12 + 504) w.putU(0, 1);

    auto eph = decodeRtcm1046(w.data(), w.bits());
    ASSERT_TRUE(eph.has_value());
    EXPECT_EQ(eph->gnss, GnssCode::GAL);
    EXPECT_EQ(eph->svId, 11);
    EXPECT_EQ(eph->weekNumber, 1234);
}

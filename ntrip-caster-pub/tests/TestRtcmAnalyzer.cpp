/*
 * Jimmy Paputto 2026
 *
 * Unit tests for RtcmAnalyzer.hpp — CRC-24Q, MSM constellation
 * mapping, RTCM frame splitter and MSM/ARP header decoding.
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "RtcmAnalyzer.hpp"

using namespace JimmyPaputto;

namespace
{
    /// Bit-by-bit reference CRC-24Q (poly 0x1864CFB, MSB-first, init 0)
    /// to validate the table-driven implementation in RtcmAnalyzer.
    uint32_t crc24qReference(const uint8_t* data, size_t len)
    {
        constexpr uint32_t kPoly = 0x1864CFB;
        uint32_t crc = 0;
        for (size_t i = 0; i < len; ++i)
        {
            crc ^= (static_cast<uint32_t>(data[i]) << 16);
            for (int b = 0; b < 8; ++b)
            {
                if (crc & 0x800000) crc = ((crc << 1) ^ kPoly) & 0xFFFFFF;
                else                crc = (crc << 1) & 0xFFFFFF;
            }
        }
        return crc;
    }

    /// MSB-first bit writer (matches detail::bitsU layout).
    class BitWriter
    {
    public:
        explicit BitWriter(size_t reservedBytes = 64) { buf_.resize(reservedBytes, 0); }
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
            uint64_t mask = (nbits >= 64) ? ~0ULL : ((1ULL << nbits) - 1ULL);
            putU(static_cast<uint64_t>(value) & mask, nbits);
        }
        void padTo(size_t nbits) { while (pos_ < nbits) putU(0, 1); }
        const uint8_t* data() const { return buf_.data(); }
        size_t bits() const { return pos_; }
        size_t bytes() const { return (pos_ + 7) / 8; }

    private:
        void ensure(size_t bits)
        {
            size_t bytes = (bits + 7) / 8;
            if (bytes > buf_.size()) buf_.resize(bytes, 0);
        }
        std::vector<uint8_t> buf_;
        size_t pos_ = 0;
    };

    /// Wrap a payload buffer into a complete RTCM3 frame:
    /// 0xD3 | 6-bit reserved=0 | 10-bit length | payload | 24-bit CRC.
    std::vector<uint8_t> wrapFrame(const uint8_t* payload, size_t payloadBytes)
    {
        std::vector<uint8_t> frame(3 + payloadBytes + 3, 0);
        frame[0] = 0xD3;
        frame[1] = static_cast<uint8_t>((payloadBytes >> 8) & 0x03);
        frame[2] = static_cast<uint8_t>(payloadBytes & 0xFF);
        for (size_t i = 0; i < payloadBytes; ++i)
            frame[3 + i] = payload[i];
        uint32_t crc = crc24qReference(frame.data(), 3 + payloadBytes);
        frame[3 + payloadBytes + 0] = static_cast<uint8_t>((crc >> 16) & 0xFF);
        frame[3 + payloadBytes + 1] = static_cast<uint8_t>((crc >> 8) & 0xFF);
        frame[3 + payloadBytes + 2] = static_cast<uint8_t>(crc & 0xFF);
        return frame;
    }
}

// ------------------------------------------------------------ CRC-24Q

TEST(RtcmCrc24q, MatchesBitwiseReference)
{
    const std::vector<std::vector<uint8_t>> samples = {
        {},
        {0x00},
        {0xFF},
        {0xD3, 0x00, 0x13},
        {0x01, 0x02, 0x03, 0x04, 0x05},
        {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE},
    };
    for (const auto& s : samples)
    {
        uint32_t expected = crc24qReference(s.data(), s.size());
        uint32_t got = detail::crc24q(s.data(), s.size());
        EXPECT_EQ(got, expected);
    }
}

// ------------------------------------------------------------ msmGnss()

TEST(MsmClassify, MapsMsgTypeToConstellation)
{
    EXPECT_EQ(msmGnss(1077), EGnss::GPS);
    EXPECT_EQ(msmGnss(1087), EGnss::GLONASS);
    EXPECT_EQ(msmGnss(1097), EGnss::Galileo);
    EXPECT_EQ(msmGnss(1107), EGnss::SBAS);
    EXPECT_EQ(msmGnss(1117), EGnss::QZSS);
    EXPECT_EQ(msmGnss(1127), EGnss::BeiDou);
    EXPECT_EQ(msmGnss(1137), EGnss::NavIC);
    EXPECT_EQ(msmGnss(1005), EGnss::Unknown);
    EXPECT_EQ(msmGnss(1019), EGnss::Unknown);
    EXPECT_EQ(msmGnss(1070), EGnss::Unknown); // MSM number 0
    EXPECT_EQ(msmGnss(1079), EGnss::Unknown); // MSM number 9 (out of range)
    EXPECT_EQ(msmGnss(1080), EGnss::Unknown); // MSM number 0
}

TEST(MsmClassify, MsmNumberIsLowDigit)
{
    EXPECT_EQ(msmNumber(1074), 4);
    EXPECT_EQ(msmNumber(1077), 7);
    EXPECT_EQ(msmNumber(1124), 4);
}

// ------------------------------------------------------------ ConstellationView::satIds

TEST(ConstellationView, SatIdsExtractsMsbFirstMask)
{
    ConstellationView v;
    // MSB = sat 1.  Set bits for sats 1, 5, 64.
    v.satMask = (1ULL << 63) | (1ULL << (63 - 4)) | (1ULL << 0);
    auto ids = v.satIds();
    ASSERT_EQ(ids.size(), 3u);
    EXPECT_EQ(ids[0], 1);
    EXPECT_EQ(ids[1], 5);
    EXPECT_EQ(ids[2], 64);
}

TEST(ConstellationView, SignalIdsExtractsMsbFirstMask)
{
    ConstellationView v;
    v.signalMask = (1u << 31) | (1u << (31 - 1)) | 1u;
    auto ids = v.signalIds();
    ASSERT_EQ(ids.size(), 3u);
    EXPECT_EQ(ids[0], 1);
    EXPECT_EQ(ids[1], 2);
    EXPECT_EQ(ids[2], 32);
}

// ------------------------------------------------------------ Frame splitter / 1005

TEST(RtcmAnalyzer, DecodesArpFromRtcm1005)
{
    // Build a 1005 payload:  ECEF = (a, 0, 0)  →  lat=0, lon=0, h=0
    BitWriter w;
    w.putU(1005, 12);           // msg type
    w.putU(0, 12);              // ref station ID
    w.putU(0, 6);               // ITRF
    w.putU(0, 4);               // GPS / GLO / GAL / ref-stn indicator bits
    int64_t xRaw = static_cast<int64_t>(6378137.0 / 1e-4); // 0.0001 m units
    w.putS(xRaw, 38);
    w.putU(0, 1);               // single recv osc ind
    w.putU(0, 1);               // reserved
    w.putS(0, 38);              // ECEF Y
    w.putU(0, 2);               // quart cycle ind
    w.putS(0, 38);              // ECEF Z
    ASSERT_EQ(w.bits(), 152u);

    std::vector<uint8_t> payload(w.data(), w.data() + 19);
    auto frame = wrapFrame(payload.data(), payload.size());

    RtcmAnalyzer ana;
    ana.feed(frame.data(), frame.size());
    auto snap = ana.snapshot();

    EXPECT_EQ(snap.totalFrames, 1u);
    EXPECT_EQ(snap.totalBytes, frame.size());
    ASSERT_TRUE(snap.arp.has_value());
    EXPECT_NEAR(snap.arp->latitudeDeg, 0.0, 1e-7);
    EXPECT_NEAR(snap.arp->longitudeDeg, 0.0, 1e-7);
    EXPECT_NEAR(snap.arp->heightMeters, 0.0, 1e-3);
    ASSERT_TRUE(snap.arpEcefX.has_value());
    EXPECT_NEAR(*snap.arpEcefX, 6378137.0, 1e-3);
    EXPECT_NEAR(*snap.arpEcefY, 0.0, 1e-3);
    EXPECT_NEAR(*snap.arpEcefZ, 0.0, 1e-3);

    // Message-type counter populated.
    ASSERT_TRUE(snap.messageTypeCounts.count(1005));
    EXPECT_EQ(snap.messageTypeCounts.at(1005), 1u);
}

TEST(RtcmAnalyzer, RejectsBadCrc)
{
    BitWriter w;
    w.putU(1005, 12);
    w.padTo(152);
    auto frame = wrapFrame(w.data(), 19);
    frame.back() ^= 0xFF;        // corrupt CRC

    RtcmAnalyzer ana;
    ana.feed(frame.data(), frame.size());
    auto snap = ana.snapshot();

    EXPECT_EQ(snap.totalFrames, 0u);
    EXPECT_FALSE(snap.arp.has_value());
}

// ------------------------------------------------------------ MSM header

TEST(RtcmAnalyzer, DecodesMsm7HeaderForGps)
{
    // MSM fixed-header bits: 12 msg + 12 ref + 30 epoch + 19 flags +
    // 64 satMask + 32 signalMask = 169 bits.  Pad to 22 bytes (176).
    BitWriter w;
    w.putU(1077, 12);                        // GPS MSM7
    w.putU(0xABC, 12);                       // ref station = 0xABC
    w.putU(123456, 30);                      // epoch time
    w.putU(0, 19);                           // flags block
    w.putU((1ULL << 63) | (1ULL << 50), 64); // sats 1 and 14
    w.putU((1u << 31) | (1u << 30), 32);     // signals 1 and 2
    while (w.bits() < 22 * 8) w.putU(0, 1);  // pad

    auto frame = wrapFrame(w.data(), 22);
    RtcmAnalyzer ana;
    ana.feed(frame.data(), frame.size());
    auto snap = ana.snapshot();

    ASSERT_EQ(snap.constellations.count(EGnss::GPS), 1u);
    const auto& v = snap.constellations.at(EGnss::GPS);
    EXPECT_EQ(v.lastMsgType, 1077);
    EXPECT_EQ(v.msmNumber, 7);
    EXPECT_EQ(v.refStation, 0xABCu);
    EXPECT_EQ(v.epochTimeMs, 123456u);

    auto sats = v.satIds();
    ASSERT_EQ(sats.size(), 2u);
    EXPECT_EQ(sats[0], 1);
    EXPECT_EQ(sats[1], 14);

    auto sigs = v.signalIds();
    ASSERT_EQ(sigs.size(), 2u);
    EXPECT_EQ(sigs[0], 1);
    EXPECT_EQ(sigs[1], 2);
}

TEST(RtcmAnalyzer, ResetsToEmptySnapshot)
{
    BitWriter w;
    w.putU(1005, 12);
    w.padTo(152);
    auto frame = wrapFrame(w.data(), 19);

    RtcmAnalyzer ana;
    ana.feed(frame.data(), frame.size());
    EXPECT_EQ(ana.snapshot().totalFrames, 1u);

    ana.reset();
    auto snap = ana.snapshot();
    EXPECT_EQ(snap.totalFrames, 0u);
    EXPECT_EQ(snap.totalBytes, 0u);
    EXPECT_TRUE(snap.messageTypeCounts.empty());
    EXPECT_FALSE(snap.arp.has_value());
}

// ------------------------------------------------------------ Streaming / fragmented feed

TEST(RtcmAnalyzer, ReassemblesFrameAcrossFeeds)
{
    // Same 1005 frame, but fed one byte at a time.
    BitWriter w;
    w.putU(1005, 12);
    w.padTo(152);
    auto frame = wrapFrame(w.data(), 19);

    RtcmAnalyzer ana;
    for (uint8_t b : frame) ana.feed(&b, 1);
    EXPECT_EQ(ana.snapshot().totalFrames, 1u);
}

TEST(RtcmAnalyzer, SkipsLeadingGarbage)
{
    BitWriter w;
    w.putU(1005, 12);
    w.padTo(152);
    auto frame = wrapFrame(w.data(), 19);

    std::vector<uint8_t> stream{0x00, 0xFF, 0x12, 0xAB};
    stream.insert(stream.end(), frame.begin(), frame.end());

    RtcmAnalyzer ana;
    ana.feed(stream.data(), stream.size());
    EXPECT_EQ(ana.snapshot().totalFrames, 1u);
}

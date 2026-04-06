/*
 * Jimmy Paputto 2026
 *
 * RTCM3 message encoder — generates RTCM3 MSM4-style observation
 * messages from raw GNSS measurements.
 *
 * Supported output messages:
 *   1074 — GPS MSM4     (pseudorange + carrier phase)
 *   1084 — GLONASS MSM4
 *   1094 — Galileo MSM4
 *   1124 — BeiDou MSM4
 *   1005 — Station coordinates (ARP, ECEF)
 *
 * This is a minimal educational encoder. Full MSM7 with extended
 * carrier phase and Doppler is left for future work.
 */

#ifndef RTCM3_ENCODER_HPP_
#define RTCM3_ENCODER_HPP_

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include <jimmypaputto/GnssHat.hpp>
#include "GnssMath.hpp"

namespace Rtcm3Encode
{

// ── CRC-24Q (same polynomial as the library) ───────────────────────
static uint32_t crc24q(const uint8_t* data, size_t len)
{
    constexpr uint32_t poly = 0x1864CFB;
    uint32_t crc = 0;
    for (size_t i = 0; i < len; ++i)
    {
        crc ^= static_cast<uint32_t>(data[i]) << 16;
        for (int bit = 0; bit < 8; ++bit)
        {
            crc <<= 1;
            if (crc & 0x1000000)
                crc ^= poly;
        }
    }
    return crc & 0xFFFFFF;
}

// ── Bit writer ──────────────────────────────────────────────────────
class BitWriter
{
public:
    void writeBits(uint64_t value, int numBits)
    {
        for (int i = numBits - 1; i >= 0; --i)
        {
            if (bitPos_ == 0)
                data_.push_back(0);

            if (value & (1ULL << i))
                data_.back() |= (0x80 >> bitPos_);

            bitPos_ = (bitPos_ + 1) % 8;
        }
    }

    void writeSignedBits(int64_t value, int numBits)
    {
        uint64_t uval = static_cast<uint64_t>(value) & ((1ULL << numBits) - 1);
        writeBits(uval, numBits);
    }

    std::vector<uint8_t> data() const { return data_; }
    size_t bitCount() const { return data_.size() * 8 - (bitPos_ == 0 ? 0 : (8 - bitPos_)); }

    void padToNextByte()
    {
        if (bitPos_ != 0)
            writeBits(0, 8 - bitPos_);
    }

private:
    std::vector<uint8_t> data_;
    int bitPos_ = 0;
};

// ── Wrap data in RTCM3 frame (preamble + length + data + CRC24) ─────
inline std::vector<uint8_t> wrapRtcm3Frame(const std::vector<uint8_t>& payload)
{
    const uint16_t len = static_cast<uint16_t>(payload.size());
    std::vector<uint8_t> frame;
    frame.reserve(3 + len + 3);

    // Header: preamble + reserved (6 bits) + length (10 bits)
    frame.push_back(0xD3);
    frame.push_back(static_cast<uint8_t>((len >> 8) & 0x03));
    frame.push_back(static_cast<uint8_t>(len & 0xFF));

    // Payload
    frame.insert(frame.end(), payload.begin(), payload.end());

    // CRC-24Q over header + payload
    const uint32_t crc = crc24q(frame.data(), frame.size());
    frame.push_back(static_cast<uint8_t>((crc >> 16) & 0xFF));
    frame.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
    frame.push_back(static_cast<uint8_t>(crc & 0xFF));

    return frame;
}

// ── RTCM3 Message 1005: Station coordinates ─────────────────────────
inline std::vector<uint8_t> encodeMsg1005(
    uint16_t stationId,
    const GnssMath::Ecef& position)
{
    BitWriter bw;

    bw.writeBits(1005, 12);            // Message number
    bw.writeBits(stationId, 12);       // Reference station ID
    bw.writeBits(6, 6);               // ITRF realization year (placeholder)
    bw.writeBits(1, 1);               // GPS indicator
    bw.writeBits(1, 1);               // GLONASS indicator
    bw.writeBits(0, 1);               // Galileo indicator
    bw.writeBits(0, 1);               // Reference station indicator
    bw.writeSignedBits(static_cast<int64_t>(std::round(position.x * 10000.0)), 38); // X in 0.1mm
    bw.writeBits(0, 1);               // Single receiver osc indicator
    bw.writeBits(0, 1);               // Reserved
    bw.writeSignedBits(static_cast<int64_t>(std::round(position.y * 10000.0)), 38); // Y in 0.1mm
    bw.writeBits(0, 2);               // Quarter cycle indicator
    bw.writeSignedBits(static_cast<int64_t>(std::round(position.z * 10000.0)), 38); // Z in 0.1mm

    bw.padToNextByte();
    return wrapRtcm3Frame(bw.data());
}

// ── MSM4 helper: encode observations for a single GNSS constellation ─
//
// MSM4 format (simplified):
//   Header: msg#, stationId, epoch, flags, satellite mask, signal mask, cell mask
//   Satellite data: rough ranges
//   Signal data: fine pseudorange, fine carrier phase, lock indicator, CNR
//
inline std::vector<uint8_t> encodeMsm4(
    uint16_t msgNumber,
    uint16_t stationId,
    uint32_t epochMs,    // Constellation-specific epoch in ms
    const std::vector<JimmyPaputto::RawObservation>& observations)
{
    if (observations.empty())
        return {};

    // Build satellite and signal masks
    // Satellite mask: 64 bits (ID 1-64)
    // Signal mask: 32 bits (signal ID 0-31)
    uint64_t satMask = 0;
    uint32_t sigMask = 0;

    for (const auto& obs : observations)
    {
        if (obs.svId >= 1 && obs.svId <= 64)
            satMask |= (1ULL << (64 - obs.svId));
        if (obs.sigId <= 31)
            sigMask |= (1U << (31 - obs.sigId));
    }

    const int nSat = __builtin_popcountll(satMask);
    const int nSig = __builtin_popcount(sigMask);

    // Cell mask: indicates which satellite-signal combinations exist
    // For each satellite, one bit per signal
    std::vector<bool> cellMask(nSat * nSig, false);

    // Map satellite IDs and signal IDs to indices
    auto satIndex = [&](uint8_t svId) -> int {
        int idx = 0;
        for (int bit = 63; bit >= 0; --bit)
        {
            if (satMask & (1ULL << bit))
            {
                if ((64 - bit) == svId) return idx;
                ++idx;
            }
        }
        return -1;
    };

    auto sigIndex = [&](uint8_t sigId) -> int {
        int idx = 0;
        for (int bit = 31; bit >= 0; --bit)
        {
            if (sigMask & (1U << bit))
            {
                if ((31 - bit) == sigId) return idx;
                ++idx;
            }
        }
        return -1;
    };

    // Build cell list: ordered by satellite then signal
    struct CellData
    {
        int satIdx, sigIdx;
        double prMes, cpMes;
        uint8_t cno;
        uint16_t locktime;
        bool cpValid;
    };
    std::vector<CellData> cells;

    for (const auto& obs : observations)
    {
        const int si = satIndex(obs.svId);
        const int sgi = sigIndex(obs.sigId);
        if (si < 0 || sgi < 0) continue;

        cellMask[si * nSig + sgi] = true;
        cells.push_back({si, sgi, obs.prMes, obs.cpMes,
                         obs.cno, obs.locktime, obs.cpValid});
    }

    // Sort by satellite index, then signal index
    std::sort(cells.begin(), cells.end(),
        [](const CellData& a, const CellData& b) {
            return (a.satIdx != b.satIdx) ? (a.satIdx < b.satIdx)
                                          : (a.sigIdx < b.sigIdx);
        });

    const int nCell = static_cast<int>(cells.size());

    // Compute rough ranges per satellite (ms, unsigned 8 + 10 bits)
    std::vector<uint32_t> roughRangeMs(nSat, 0);
    std::vector<uint32_t> roughRangeFrac(nSat, 0);

    for (const auto& cell : cells)
    {
        const double rangeMs = cell.prMes / GnssMath::C * 1000.0;
        roughRangeMs[cell.satIdx] = static_cast<uint32_t>(rangeMs) & 0xFF;
        roughRangeFrac[cell.satIdx] = static_cast<uint32_t>(
            std::fmod(rangeMs, 1.0) * 1024.0) & 0x3FF;
    }

    // ── Encode ──────────────────────────────────────────────────────
    BitWriter bw;

    // MSM header
    bw.writeBits(msgNumber, 12);       // Message number
    bw.writeBits(stationId, 12);       // Station ID
    bw.writeBits(epochMs, 30);         // GNSS epoch time (ms)
    bw.writeBits(0, 1);               // Multiple message bit
    bw.writeBits(0, 3);               // Issue of data station
    bw.writeBits(0, 7);               // Reserved
    bw.writeBits(0, 2);               // Clock steering indicator
    bw.writeBits(0, 2);               // External clock indicator
    bw.writeBits(0, 1);               // GNSS smoothing indicator
    bw.writeBits(0, 3);               // GNSS smoothing interval

    // Satellite mask (64 bits)
    bw.writeBits(satMask >> 32, 32);
    bw.writeBits(satMask & 0xFFFFFFFF, 32);

    // Signal mask (32 bits)
    bw.writeBits(sigMask, 32);

    // Cell mask
    for (int i = 0; i < nSat * nSig; ++i)
        bw.writeBits(cellMask[i] ? 1 : 0, 1);

    // Satellite data: rough range integer ms (8 bits each)
    for (int s = 0; s < nSat; ++s)
        bw.writeBits(roughRangeMs[s], 8);

    // Satellite data: rough range fractional (10 bits each)
    for (int s = 0; s < nSat; ++s)
        bw.writeBits(roughRangeFrac[s], 10);

    // Signal data: fine pseudorange (15 bits, signed, 2^-24 ms)
    for (const auto& cell : cells)
    {
        const double rangeMs = cell.prMes / GnssMath::C * 1000.0;
        const double rough = roughRangeMs[cell.satIdx]
                           + roughRangeFrac[cell.satIdx] / 1024.0;
        const double fine = (rangeMs - rough) * (1 << 24); // Scale to 2^-24 ms
        int16_t finePr = static_cast<int16_t>(
            std::clamp(fine, -16384.0, 16383.0));
        bw.writeSignedBits(finePr, 15);
    }

    // Signal data: fine carrier phase (22 bits, signed, 2^-29 ms)
    for (const auto& cell : cells)
    {
        if (cell.cpValid)
        {
            const double cpMs = cell.cpMes * GnssMath::GPS_L1_LAMBDA
                              / GnssMath::C * 1000.0;
            const double rough = roughRangeMs[cell.satIdx]
                               + roughRangeFrac[cell.satIdx] / 1024.0;
            const double fine = (cpMs - rough) * (1 << 29);
            int32_t fineCp = static_cast<int32_t>(
                std::clamp(fine, -2097152.0, 2097151.0));
            bw.writeSignedBits(fineCp, 22);
        }
        else
        {
            bw.writeSignedBits(-2097152, 22); // Invalid indicator
        }
    }

    // Signal data: lock time indicator (4 bits)
    for (const auto& cell : cells)
    {
        uint8_t lockInd = 0;
        if      (cell.locktime >= 524288) lockInd = 15;
        else if (cell.locktime >= 262144) lockInd = 14;
        else if (cell.locktime >= 131072) lockInd = 13;
        else if (cell.locktime >= 65536)  lockInd = 12;
        else if (cell.locktime >= 32768)  lockInd = 11;
        else if (cell.locktime >= 16384)  lockInd = 10;
        else if (cell.locktime >= 8192)   lockInd = 9;
        else if (cell.locktime >= 4096)   lockInd = 8;
        else if (cell.locktime >= 2048)   lockInd = 7;
        else if (cell.locktime >= 1024)   lockInd = 6;
        else if (cell.locktime >= 512)    lockInd = 5;
        else if (cell.locktime >= 256)    lockInd = 4;
        else if (cell.locktime >= 128)    lockInd = 3;
        else if (cell.locktime >= 64)     lockInd = 2;
        else if (cell.locktime >= 32)     lockInd = 1;
        bw.writeBits(lockInd, 4);
    }

    // Signal data: half-cycle ambiguity (1 bit)
    for (const auto& cell : cells)
        bw.writeBits(0, 1); // Simplified: no half-cycle

    // Signal data: CNR (6 bits, 1 dB-Hz resolution)
    for (const auto& cell : cells)
        bw.writeBits(std::min(static_cast<uint8_t>(63), cell.cno), 6);

    bw.padToNextByte();
    return wrapRtcm3Frame(bw.data());
}

// ── Convenience: encode all constellations from RawMeasurements ─────
struct Rtcm3Output
{
    std::vector<std::vector<uint8_t>> frames;
};

inline Rtcm3Output encodeAllMsm4(
    uint16_t stationId,
    const JimmyPaputto::RawMeasurements& raw,
    const GnssMath::Ecef* stationPosition = nullptr)
{
    Rtcm3Output out;

    // Group observations by GNSS constellation
    std::vector<JimmyPaputto::RawObservation> gps, glonass, galileo, beidou;

    for (const auto& obs : raw.observations)
    {
        if (!obs.prValid) continue;

        switch (obs.gnssId)
        {
            case JimmyPaputto::EGnssId::GPS:      gps.push_back(obs);      break;
            case JimmyPaputto::EGnssId::GLONASS:   glonass.push_back(obs);  break;
            case JimmyPaputto::EGnssId::Galileo:   galileo.push_back(obs);  break;
            case JimmyPaputto::EGnssId::BeiDou:    beidou.push_back(obs);   break;
            default: break;
        }
    }

    // Compute GPS epoch: TOW in ms
    const uint32_t gpsEpochMs = static_cast<uint32_t>(
        std::fmod(raw.rcvTow * 1000.0, 604800000.0));

    // Station coordinates (message 1005)
    if (stationPosition)
    {
        auto frame1005 = encodeMsg1005(stationId, *stationPosition);
        if (!frame1005.empty())
            out.frames.push_back(std::move(frame1005));
    }

    // GPS MSM4 (1074)
    if (!gps.empty())
    {
        auto frame = encodeMsm4(1074, stationId, gpsEpochMs, gps);
        if (!frame.empty())
            out.frames.push_back(std::move(frame));
    }

    // GLONASS MSM4 (1084)
    if (!glonass.empty())
    {
        // GLONASS epoch: day-of-week in ms within the day
        const uint32_t gloEpoch = gpsEpochMs; // Simplified
        auto frame = encodeMsm4(1084, stationId, gloEpoch, glonass);
        if (!frame.empty())
            out.frames.push_back(std::move(frame));
    }

    // Galileo MSM4 (1094)
    if (!galileo.empty())
    {
        auto frame = encodeMsm4(1094, stationId, gpsEpochMs, galileo);
        if (!frame.empty())
            out.frames.push_back(std::move(frame));
    }

    // BeiDou MSM4 (1124)
    if (!beidou.empty())
    {
        // BeiDou epoch offset: BDT = GPS - 14 seconds
        const uint32_t bdtEpochMs = static_cast<uint32_t>(
            std::fmod((raw.rcvTow - 14.0) * 1000.0, 604800000.0));
        auto frame = encodeMsm4(1124, stationId, bdtEpochMs, beidou);
        if (!frame.empty())
            out.frames.push_back(std::move(frame));
    }

    return out;
}

}  // Rtcm3Encode

#endif  // RTCM3_ENCODER_HPP_

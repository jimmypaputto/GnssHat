/*
 * Jimmy Paputto 2026
 *
 * GLONASS L1OF decoder (strings 1..4 — ephemeris).
 * Ref: GLONASS ICD L1/L2 Ed 5.1 §4.4 + §4.7 (Hamming).
 *
 * NOTE (2026-04-25): On ZED-F9P HPG L1L5 firmware 1.40, GLONASS L1OF
 * SFRBX delivery is fragile — after a long uptime the receiver may
 * deliver a fixed placeholder pattern instead of real bits. A power-
 * cycle restores normal operation. When healthy, this decoder
 * produces valid R-ephemerides.
 *
 * --------------------------------------------------------------------
 * SIGNAL & TRANSPORT
 * --------------------------------------------------------------------
 * Signal: gnssId=6, sigId=0 (L1OF). u-blox SFRBX delivers 4 U4 dwords
 * (128 bits). The ICD numbers data bits as m_85 .. m_1 (m_85 first
 * transmitted). u-blox layout (per §3.17.9 of the F9 IDD):
 *
 *   dwrd[0] : m_85 .. m_54   (32 bits, MSB = m_85)
 *   dwrd[1] : m_53 .. m_22
 *   dwrd[2] : m_21 .. m_1 (top 21 bits) || KX(8) || idle(3 bits)
 *   dwrd[3] : time-mark chips (ignored)
 *
 * 85 data + 8 hamming = 93 bits live in the top 93 bits of the first
 * three dwords concatenated MSB-first. We pack them into a 12-byte
 * left-justified buffer where bit index 0 (MSB of byte 0) corresponds
 * to ICD bit m_85, and bit index 92 corresponds to KX_1.
 *
 * --------------------------------------------------------------------
 * FIELD ENCODING
 * --------------------------------------------------------------------
 * Most signed quantities use SIGN-MAGNITUDE (NOT two's complement):
 * the leading bit is the sign (1 = negative), the remaining bits the
 * magnitude. Helper getsm() handles this.
 *
 * --------------------------------------------------------------------
 * STRING LAYOUTS (ICD Table 4.5)
 * --------------------------------------------------------------------
 * S1: m=1, tk(76..65, 12), xn'(64..41, 24), xn''(40..36, 5), xn(35..9, 27)
 * S2: m=2, Bn(80..78), P2(77), tb(76..70, 7), yn'(64..41), yn''(40..36), yn(35..9)
 * S3: m=3, P3(80), gammaN(79..69, 11), p(66), ln(65), zn'(64..41), zn''(40..36), zn(35..9)
 * S4: m=4, tauN(80..59, 22)
 *
 * UNITS:  x,y,z   2^-11 km → m
 *         vx..vz  2^-20 km/s → m/s
 *         ax..az  2^-30 km/s² → m/s²
 *
 * Per-(svId, stringNumber) dedupe of the SubframeBuffer ring.
 */

#ifndef GLONASS_L1OF_HPP_
#define GLONASS_L1OF_HPP_

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <unordered_map>
#include <vector>

#include "EphemerisStore.hpp"
#include "GnssMath.hpp"

namespace GlonassL1of
{

// ── Bit helpers (left-justified MSB-first) ────────────────────────
inline uint32_t getbitu(const uint8_t* buf, int pos, int nBits)
{
    uint32_t v = 0;
    for (int i = pos; i < pos + nBits; ++i)
        v = (v << 1) | ((buf[i >> 3] >> (7 - (i & 7))) & 1u);
    return v;
}

inline uint64_t getbitu64(const uint8_t* buf, int pos, int nBits)
{
    uint64_t v = 0;
    for (int i = pos; i < pos + nBits; ++i)
        v = (v << 1) | ((buf[i >> 3] >> (7 - (i & 7))) & 1ull);
    return v;
}

// Sign-magnitude: top bit is sign (1 = negative).
inline int64_t getsm(const uint8_t* buf, int pos, int nBits)
{
    const uint32_t signBit = (buf[pos >> 3] >> (7 - (pos & 7))) & 1u;
    const uint64_t mag     = getbitu64(buf, pos + 1, nBits - 1);
    const int64_t  v       = static_cast<int64_t>(mag);
    return signBit ? -v : v;
}

// ── Hamming(85,77) parity check per GLONASS ICD §4.7 ───────────────
inline bool hammingCheck(const uint8_t buf[12])
{
    auto a = [&](int i) -> uint8_t {
        const int p = 85 - i;
        return static_cast<uint8_t>((buf[p >> 3] >> (7 - (p & 7))) & 1u);
    };
    auto par = [&](std::initializer_list<int> idx) {
        uint8_t s = 0;
        for (int k : idx) s ^= a(k);
        return s;
    };

    const uint8_t C1 = a(1) ^ par({
        9,10,12,13,15,17,19,20,22,24,26,28,30,32,34,35,37,39,41,
        43,45,47,49,51,53,55,57,59,61,63,65,66,68,70,72,74,76,78,
        80,82,84});
    const uint8_t C2 = a(2) ^ par({
        9,11,12,14,15,18,19,21,22,25,26,29,30,33,34,36,37,40,41,
        44,45,48,49,52,53,56,57,60,61,64,65,67,68,71,72,75,76,79,
        80,83,84});
    const uint8_t C3 = a(3) ^ par({
        10,11,12,16,17,18,19,23,24,25,26,31,32,33,34,38,39,40,41,
        46,47,48,49,54,55,56,57,62,63,64,65,69,70,71,72,77,78,79,
        80,85});
    const uint8_t C4 = a(4) ^ par({
        13,14,15,16,17,18,19,27,28,29,30,31,32,33,34,42,43,44,45,
        46,47,48,49,58,59,60,61,62,63,64,65,73,74,75,76,77,78,79,80});
    const uint8_t C5 = a(5) ^ par({
        20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,50,51,52,53,
        54,55,56,57,58,59,60,61,62,63,64,65,81,82,83,84,85});
    const uint8_t C6 = a(6) ^ par({
        35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,
        54,55,56,57,58,59,60,61,62,63,64,65});
    const uint8_t C7 = a(7) ^ par({
        66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85});

    uint8_t Csum = 0;
    for (int i = 1; i <= 85; ++i) Csum ^= a(i);
    Csum ^= a(8);

    return (C1 | C2 | C3 | C4 | C5 | C6 | C7 | Csum) == 0;
}

inline void wordsToBuffer(const std::vector<uint32_t>& w, uint8_t buf[12])
{
    std::memset(buf, 0, 12);
    auto pack32 = [&](uint32_t v, int byteOff) {
        buf[byteOff + 0] = static_cast<uint8_t>(v >> 24);
        buf[byteOff + 1] = static_cast<uint8_t>(v >> 16);
        buf[byteOff + 2] = static_cast<uint8_t>(v >>  8);
        buf[byteOff + 3] = static_cast<uint8_t>(v);
    };
    pack32(w[0], 0);
    pack32(w[1], 4);
    pack32(w[2], 8);
}

// ── Decoder ───────────────────────────────────────────────────────
class Decoder
{
public:
    bool feed(uint8_t svId,
              uint8_t freqSlotP7,
              const std::vector<uint32_t>& words,
              EphemerisStore::Store& store);

    size_t stringCount = 0;
    size_t parityFails = 0;
    size_t s1Count = 0, s2Count = 0, s3Count = 0, s4Count = 0;

private:
    struct Partial
    {
        uint8_t haveMask = 0;        // bit i set when string (i+1) decoded
        EphemerisStore::Glonass g{};
    };
    std::unordered_map<uint8_t, Partial>                 partial_;
    std::unordered_map<uint16_t, std::array<uint32_t,3>> lastWords_;

    void decodeS1(Partial& p, const uint8_t b[12]);
    void decodeS2(Partial& p, const uint8_t b[12]);
    void decodeS3(Partial& p, const uint8_t b[12]);
    void decodeS4(Partial& p, const uint8_t b[12]);
    void tryAssemble(uint8_t svId, int8_t freqSlot, Partial& p,
                     EphemerisStore::Store& store);
};

// ICD 1-based "high" bit number → buffer 0-based start index (MSB first).
inline int bitPos(int icdHi) { return 85 - icdHi; }

inline void Decoder::decodeS1(Partial& p, const uint8_t b[12])
{
    constexpr double KM = 1000.0;
    const uint32_t tk = getbitu(b, bitPos(76), 12);
    const int64_t  vx = getsm  (b, bitPos(64), 24);
    const int64_t  ax = getsm  (b, bitPos(40),  5);
    const int64_t  x  = getsm  (b, bitPos(35), 27);

    const uint32_t tk_h  = (tk >> 7) & 0x1Fu;
    const uint32_t tk_m  = (tk >> 1) & 0x3Fu;
    const uint32_t tk_30 = tk & 1u;
    p.g.tb = static_cast<double>(tk_h * 3600 + tk_m * 60) + tk_30 * 30.0;

    p.g.x  = static_cast<double>(x)  * std::ldexp(1.0, -11) * KM;
    p.g.vx = static_cast<double>(vx) * std::ldexp(1.0, -20) * KM;
    p.g.ax = static_cast<double>(ax) * std::ldexp(1.0, -30) * KM;
    p.haveMask |= 0x01;
}

inline void Decoder::decodeS2(Partial& p, const uint8_t b[12])
{
    constexpr double KM = 1000.0;
    const uint32_t tb = getbitu(b, bitPos(76),  7);
    const int64_t  vy = getsm  (b, bitPos(64), 24);
    const int64_t  ay = getsm  (b, bitPos(40),  5);
    const int64_t  y  = getsm  (b, bitPos(35), 27);

    p.g.tb = static_cast<double>(tb) * 900.0;
    p.g.y  = static_cast<double>(y)  * std::ldexp(1.0, -11) * KM;
    p.g.vy = static_cast<double>(vy) * std::ldexp(1.0, -20) * KM;
    p.g.ay = static_cast<double>(ay) * std::ldexp(1.0, -30) * KM;
    p.haveMask |= 0x02;
}

inline void Decoder::decodeS3(Partial& p, const uint8_t b[12])
{
    constexpr double KM = 1000.0;
    const int64_t gn = getsm(b, bitPos(79), 11);
    const int64_t vz = getsm(b, bitPos(64), 24);
    const int64_t az = getsm(b, bitPos(40),  5);
    const int64_t z  = getsm(b, bitPos(35), 27);

    p.g.gammaN = static_cast<double>(gn) * std::ldexp(1.0, -40);
    p.g.z  = static_cast<double>(z)  * std::ldexp(1.0, -11) * KM;
    p.g.vz = static_cast<double>(vz) * std::ldexp(1.0, -20) * KM;
    p.g.az = static_cast<double>(az) * std::ldexp(1.0, -30) * KM;
    p.haveMask |= 0x04;
}

inline void Decoder::decodeS4(Partial& p, const uint8_t b[12])
{
    const int64_t tau = getsm(b, bitPos(80), 22);
    p.g.tauN = static_cast<double>(tau) * std::ldexp(1.0, -30);
    p.haveMask |= 0x08;
}

inline void Decoder::tryAssemble(uint8_t svId, int8_t freqSlot,
                                 Partial& p, EphemerisStore::Store& store)
{
    if ((p.haveMask & 0x0F) != 0x0F) return;
    p.g.svId     = svId;
    p.g.freqSlot = freqSlot;
    p.g.valid    = true;
    store.put(p.g);
}

inline bool Decoder::feed(uint8_t svId,
                          uint8_t freqSlotP7,
                          const std::vector<uint32_t>& words,
                          EphemerisStore::Store& store)
{
    if (words.size() < 4)
    {
        ++parityFails;
        return false;
    }

    uint8_t buf[12];
    wordsToBuffer(words, buf);

    if (!hammingCheck(buf))
    {
        ++parityFails;
        return false;
    }

    // String number m: 4 LSB of top 5 bits (top bit is "idle 0").
    const uint32_t m5 = getbitu(buf, 0, 5);
    const uint32_t m  = m5 & 0x0Fu;

    ++stringCount;

    const uint16_t key = (static_cast<uint16_t>(svId) << 8) | (m & 0xFFu);
    auto& last = lastWords_[key];
    if (last[0] == words[0] && last[1] == words[1] && last[2] == words[2])
        return false;
    last = {words[0], words[1], words[2]};

    auto& p = partial_[svId];
    const int8_t freqSlot = static_cast<int8_t>(freqSlotP7) - 7;

    switch (m)
    {
        case 1: ++s1Count; decodeS1(p, buf); break;
        case 2: ++s2Count; decodeS2(p, buf); break;
        case 3: ++s3Count; decodeS3(p, buf); break;
        case 4: ++s4Count; decodeS4(p, buf); break;
        default: return false;
    }

    const bool wasValid = p.g.valid;
    tryAssemble(svId, freqSlot, p, store);
    return p.g.valid && !wasValid;
}

}  // namespace GlonassL1of

#endif  // GLONASS_L1OF_HPP_

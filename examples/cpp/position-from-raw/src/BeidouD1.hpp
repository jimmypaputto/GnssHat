/*
 * Jimmy Paputto 2026
 *
 * BeiDou B1I D1 NAV decoder.
 * Ref: BDS-SIS-ICD-B1I v3.0 §5.2.3 (channel coding) + §5.2.4 (D1
 *      NAV message for MEO/IGSO satellites) + Tables 5-11-1/2/3
 *      (subframes 1/2/3 bit layout).
 *
 * Signal: gnssId=3, sigId=0 (B1I D1). u-blox delivers 10 × U4 words
 * per RXM-SFRBX message. Each 30-bit transmitted word sits RIGHT-
 * justified in the low 30 bits of a U4; bit 29 = MSB = first
 * transmitted bit. Bits 31..30 are zero padding.
 * (Verified empirically: the B1I preamble 0b11100010010 = 0x712 lands
 *  at (word[0] >> 19) & 0x7FF on live data from a ZED-F9P.)
 *
 * Channel coding (§5.2.3):
 *   Word 1 (bits 1..30):
 *     Pre(11) + Rev(4) + FraID(3) + SOW_MSB(8) + Parity(4)
 *     — no interleaving; 4 parity bits form a BCH(15,11,1) codeword
 *       with the 11 info bits at positions 16..26.
 *   Words 2..10 (bits 1..30):
 *     Two BCH(15,11,1) codewords column-interleaved.
 *     - Even transmitted positions (0,2,4,…,28 0-indexed) → block1.
 *     - Odd  transmitted positions (1,3,5,…,29 0-indexed) → block2.
 *     - Each 15-bit block: 11 info (MSB first) + 4 parity.
 *     - Generator g(x) = x⁴ + x + 1 (polynomial 0b10011).
 *
 * After de-interleave + parity-strip, 22 info bits per word 2..10
 * (plus 26 info bits in word 1) are packed MSB-first in TRANSMITTED
 * ORDER into a 224-bit subframe info buffer. Info buffer layout:
 *   bits   0..25  word 1 info (= transmitted bits 1..26)
 *   bits  26..47  word 2 info
 *   bits  48..69  word 3 info
 *   bits  70..91  word 4 info
 *   bits  92..113 word 5 info
 *   bits 114..135 word 6 info
 *   bits 136..157 word 7 info
 *   bits 158..179 word 8 info
 *   bits 180..201 word 9 info
 *   bits 202..223 word 10 info
 *
 * With transmitted-order packing, an ICD reference "word W bits a..b"
 * (1-indexed within the 30-bit transmitted word) maps to:
 *   W == 1 : info bit (a-1)        length (b-a+1)
 *   W >= 2 : info bit 26 + (W-2)*22 + (a-1)
 *
 * Cross-word fields (e.g. SOW, toc, M0, ecc, sqrtA) are assembled from
 * two or three contiguous slices, MSB segment first.
 *
 * Null-frame pattern: some MEO SVs emit repeating 0x6eeec3a2 in all
 * words when no valid NAV data is available — those are skipped.
 *
 * STATUS: phase B — full ephemeris extraction from SF1+SF2+SF3.
 *         toe is split between SF2 (2 MSB) and SF3 (15 LSB).
 *         Assembly trigger: all three subframes received; AODE used
 *         only to reset the partial when SF1 shows a new value.
 *         BDT→GPST 14s offset is NOT applied here — it belongs at
 *         SPP query time.
 */

#ifndef BEIDOU_D1_HPP_
#define BEIDOU_D1_HPP_

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <vector>

#include "EphemerisStore.hpp"
#include "GnssMath.hpp"

namespace BeidouD1
{

// ── Bit helpers over a byte buffer (MSB-first, 0-indexed) ─────────
inline uint32_t getbitu(const uint8_t* buf, int pos, int len)
{
    uint32_t v = 0;
    for (int i = pos; i < pos + len; ++i)
        v = (v << 1) | ((buf[i >> 3] >> (7 - (i & 7))) & 1u);
    return v;
}

inline int32_t getbits(const uint8_t* buf, int pos, int len)
{
    uint32_t v = getbitu(buf, pos, len);
    if (v & (1u << (len - 1)))
        v |= ~((1u << len) - 1);
    return static_cast<int32_t>(v);
}

inline uint64_t getbitu64(const uint8_t* buf, int pos, int len)
{
    uint64_t v = 0;
    for (int i = pos; i < pos + len; ++i)
        v = (v << 1) | ((uint64_t)((buf[i >> 3] >> (7 - (i & 7))) & 1u));
    return v;
}

inline int64_t getbits64(const uint8_t* buf, int pos, int len)
{
    uint64_t v = getbitu64(buf, pos, len);
    if (v & (uint64_t{1} << (len - 1)))
        v |= ~((uint64_t{1} << len) - 1);
    return static_cast<int64_t>(v);
}

inline void setbit(uint8_t* buf, int pos, uint32_t bit)
{
    if (bit & 1u)
        buf[pos >> 3] |= static_cast<uint8_t>(1u << (7 - (pos & 7)));
}

// ── Convert ICD (wordIdx, startBit 1-based) → info-buffer bit ─────
inline int infoPos(int wordIdx, int startBit)
{
    if (wordIdx == 1) return startBit - 1;
    return 26 + (wordIdx - 2) * 22 + (startBit - 1);
}

// ── Read an unsigned field strictly inside a single ICD word ──────
inline uint32_t fetchU(const uint8_t* info, int w, int s, int n)
{
    return getbitu(info, infoPos(w, s), n);
}

// ── Read a signed, two's-complement field split across two ICD
//    slices (MSB segment first). nHi+nLo bits total. ───────────────
inline int64_t fetch2S(const uint8_t* info,
                       int w1, int s1, int n1,
                       int w2, int s2, int n2)
{
    const uint64_t hi = getbitu64(info, infoPos(w1, s1), n1);
    const uint64_t lo = getbitu64(info, infoPos(w2, s2), n2);
    const int     n   = n1 + n2;
    uint64_t v        = (hi << n2) | lo;
    if (v & (uint64_t{1} << (n - 1)))
        v |= ~((uint64_t{1} << n) - 1);
    return static_cast<int64_t>(v);
}

// ── Read an unsigned field split across two ICD slices ────────────
inline uint64_t fetch2U(const uint8_t* info,
                        int w1, int s1, int n1,
                        int w2, int s2, int n2)
{
    const uint64_t hi = getbitu64(info, infoPos(w1, s1), n1);
    const uint64_t lo = getbitu64(info, infoPos(w2, s2), n2);
    return (hi << n2) | lo;
}

// ── Null-frame signature: all 10 words == 0x6eeec3a2 ──────────────
inline bool isNullFrame(const std::vector<uint32_t>& words)
{
    constexpr uint32_t kNullPattern = 0x6eeec3a2u;
    for (const uint32_t w : words)
        if (w != kNullPattern) return false;
    return true;
}

// ── BCH(15,11,1) syndrome (g(x) = x⁴ + x + 1). 0 == valid. ────────
inline uint8_t bchSyndrome(uint16_t cw15)
{
    uint32_t r = cw15 & 0x7FFFu;
    for (int i = 14; i >= 4; --i)
    {
        if (r & (1u << i))
            r ^= (0b10011u << (i - 4));
    }
    return static_cast<uint8_t>(r & 0xFu);
}

// ── De-interleave one 30-bit word into two 15-bit BCH codewords ──
//
// Input w30 carries the transmitted bits in U4 positions 29..0 (bit
// 29 = first transmitted bit). Bits 31..30 are zero padding.
inline void deinterleaveWord(uint32_t w30,
                             uint16_t& block1, uint16_t& block2)
{
    block1 = 0;
    block2 = 0;
    for (int i = 0; i < 15; ++i)
    {
        const uint32_t b1 = (w30 >> (29 - 2 * i))     & 1u;
        const uint32_t b2 = (w30 >> (29 - 2 * i - 1)) & 1u;
        block1 = static_cast<uint16_t>((block1 << 1) | b1);
        block2 = static_cast<uint16_t>((block2 << 1) | b2);
    }
}

// ── BCH-check word 1 (first 15 bits direct preamble+rev; info 16..26
//    with parity at 27..30 forms the single BCH codeword). ─────────
//
// Word 1 layout in U4 (bit 29 = first transmitted bit):
//   bits 29..19  Preamble (11)
//   bits 18..15  Rev      (4)
//   bits 14..12  FraID    (3)
//   bits 11..4   SOW_MSB  (8)
//   bits  3..0   Parity   (4)
inline bool bchCheckWord1(uint32_t w30)
{
    const uint16_t info11  = static_cast<uint16_t>((w30 >> 4) & 0x7FFu);
    const uint16_t parity4 = static_cast<uint16_t>(w30 & 0xFu);
    const uint16_t cw15    = static_cast<uint16_t>((info11 << 4) | parity4);
    return bchSyndrome(cw15) == 0;
}

// ── Decode full subframe into 224-bit info buffer (transmitted
//    order). Returns false on preamble mismatch or any BCH fail. ──
inline bool decodeSubframeBits(const std::vector<uint32_t>& words,
                               uint8_t info[28])
{
    constexpr uint32_t kPreamble = 0x712u;  // 11 bits: 11100010010

    std::memset(info, 0, 28);

    // --- Word 1: preamble + BCH-11/15 over bits 16..30 ------------
    const uint32_t w0 = words[0];
    if (((w0 >> 19) & 0x7FFu) != kPreamble) return false;
    if (!bchCheckWord1(w0))                 return false;

    // First 26 transmitted bits go straight in (no interleave).
    // Transmitted bit k (1-based) lives at U4 bit (30-k).
    int outPos = 0;
    for (int b = 29; b >= 4; --b, ++outPos)
        setbit(info, outPos, (w0 >> b) & 1u);

    // --- Words 2..10 ----------------------------------------------
    //
    // Empirical observation on ZED-F9P HPG L1L5 1.40: word 1 BCH
    // parity ALWAYS passes, but words 2..10 BCH parity NEVER passes
    // across all tracked SVs — even though preamble and the direct
    // word-1 parity are both intact. This strongly suggests u-blox
    // pre-decodes the column-interleaved BCH on words 2..10 internally
    // and rewrites the 30-bit payload.
    //
    // We therefore perform the de-interleave + BCH strip (i.e. read 22
    // "info" bits per word in transmitted order from the interleave
    // pattern), WITHOUT enforcing a BCH check on words 2..10. Word 1's
    // direct 4-bit parity is still enforced as an anti-junk filter.
    for (int wi = 1; wi < 10; ++wi)
    {
        uint16_t b1, b2;
        deinterleaveWord(words[wi], b1, b2);
        // Transmitted order: block1[0], block2[0], block1[1], ... (22 info bits)
        for (int bi = 0; bi < 11; ++bi)
        {
            setbit(info, outPos++, (b1 >> (14 - bi)) & 1u);
            setbit(info, outPos++, (b2 >> (14 - bi)) & 1u);
        }
    }
    return outPos == 224;
}

// ── Scale helper ──────────────────────────────────────────────────
constexpr double P2(int n) { return std::ldexp(1.0, n); }

// ── Decoder ────────────────────────────────────────────────────────
class Decoder
{
public:
    bool feed(uint8_t svId,
              const std::vector<uint32_t>& words,
              EphemerisStore::Store& store);

    size_t subframeCount = 0;
    size_t nullFrames    = 0;
    size_t parityFails   = 0;
    size_t sf1Count      = 0;
    size_t sf2Count      = 0;
    size_t sf3Count      = 0;

private:
    struct Partial
    {
        uint8_t  haveMask  = 0;      // bit0=SF1, bit1=SF2, bit2=SF3
        uint16_t aode      = 0xFFFFu;
        uint32_t toeMsb2   = 0;      // 2 MSB of toe (from SF2)
        EphemerisStore::Kepler k{};
        bool                          haveIonoSF1 = false;
        EphemerisStore::IonoKlobuchar ionoStaged{};
    };
    std::unordered_map<uint8_t, Partial> partial_;
    std::unordered_map<uint8_t, uint32_t> lastWord0_;  // dedupe

    void decodeSF1(Partial& p, const uint8_t info[28]);
    void decodeSF2(Partial& p, const uint8_t info[28]);
    void decodeSF3(Partial& p, const uint8_t info[28]);

    void tryAssemble(uint8_t svId, Partial& p,
                     EphemerisStore::Store& store);
};

// ── Subframe 1 — clock & system data (Table 5-11-1) ───────────────
inline void Decoder::decodeSF1(Partial& p, const uint8_t info[28])
{
    // SatH1 (1), AODC (5), URAI (4)
    // const uint32_t satH1 = fetchU(info, 2, 13, 1);
    // const uint32_t aodc  = fetchU(info, 2, 14, 5);
    // const uint32_t urai  = fetchU(info, 2, 19, 4);

    const uint32_t wn    = fetchU(info, 3, 1, 13);
    // toc = W3 b14-22 (9 MSB) || W4 b1-8 (8 LSB) = 17 bits, scale 2^3
    const uint64_t tocRaw = fetch2U(info, 3, 14, 9, 4, 1, 8);
    // TGD1 = W4 b9-18 (10 bits signed, scale 0.1 ns)
    const int32_t tgd1 = getbits(info, infoPos(4, 9), 10);
    (void)wn;

    // ── Klobuchar iono (Table 5-11-1; reference to B1I 1561.098 MHz) ──
    // α0..α3 = 8-bit signed, scales 2^-30, 2^-27, 2^-24, 2^-24
    // β0..β3 = 8-bit signed, scales 2^11, 2^14, 2^16, 2^16
    const int32_t a0 = getbits(info, infoPos(5,  7), 8);
    const int32_t a1 = getbits(info, infoPos(5, 15), 8);
    const int32_t a2 = getbits(info, infoPos(6,  1), 8);
    const int32_t a3 = getbits(info, infoPos(6,  9), 8);
    const int32_t b0 = static_cast<int32_t>(fetch2S(info, 6, 17, 6, 7, 1, 2));
    const int32_t b1 = getbits(info, infoPos(7,  3), 8);
    const int32_t b2 = getbits(info, infoPos(7, 11), 8);
    const int32_t b3 = static_cast<int32_t>(fetch2S(info, 7, 19, 4, 8, 1, 4));
    EphemerisStore::IonoKlobuchar iono;
    iono.valid    = true;
    iono.alpha[0] = a0 * P2(-30);
    iono.alpha[1] = a1 * P2(-27);
    iono.alpha[2] = a2 * P2(-24);
    iono.alpha[3] = a3 * P2(-24);
    iono.beta [0] = b0 * 2048.0;       // 2^11
    iono.beta [1] = b1 * 16384.0;      // 2^14
    iono.beta [2] = b2 * 65536.0;      // 2^16
    iono.beta [3] = b3 * 65536.0;      // 2^16
    p.ionoStaged   = iono;
    p.haveIonoSF1  = true;

    // a2 = W8 b5-15 (11 bits signed, scale 2^-66 s/s^2)
    const int32_t af2 = getbits(info, infoPos(8, 5), 11);
    // a0 = W8 b16-22 (7 MSB) || W9 b1-17 (17 LSB) = 24 bits, scale 2^-33
    const int64_t af0 = fetch2S(info, 8, 16, 7, 9, 1, 17);
    // a1 = W9 b18-22 (5 MSB) || W10 b1-17 (17 LSB) = 22 bits, scale 2^-50
    const int64_t af1 = fetch2S(info, 9, 18, 5, 10, 1, 17);
    // AODE = W10 b18-22 (5 bits)
    const uint32_t aode = fetchU(info, 10, 18, 5);

    // Reset partial when AODE changes (new ephemeris set).
    if (p.aode != aode)
    {
        p.aode     = static_cast<uint16_t>(aode);
        p.haveMask = 0;
        p.toeMsb2  = 0;
        p.k        = EphemerisStore::Kepler{};
    }

    p.k.toc = static_cast<double>(tocRaw) * 8.0;   // 2^3
    p.k.af0 = af0 * P2(-33);
    p.k.af1 = af1 * P2(-50);
    p.k.af2 = af2 * P2(-66);
    p.k.tgd = tgd1 * 1e-10;                        // 0.1 ns

    p.haveMask |= 0x01;
}

// ── Subframe 2 — ephemeris (1/2) (Table 5-11-2) ───────────────────
inline void Decoder::decodeSF2(Partial& p, const uint8_t info[28])
{
    // deltaN = W2 b13-22 (10 MSB) || W3 b1-6 (6 LSB) = 16 bits, 2^-43 semicircle
    const int64_t dN    = fetch2S(info, 2, 13, 10, 3, 1,  6);
    // Cuc = W3 b7-22 (16 MSB) || W4 b1-2 (2 LSB) = 18 bits, 2^-31 rad
    const int64_t cuc   = fetch2S(info, 3,  7, 16, 4, 1,  2);
    // M0 = W4 b3-22 (20 MSB) || W5 b1-12 (12 LSB) = 32 bits, 2^-31 semicircle
    const int64_t M0    = fetch2S(info, 4,  3, 20, 5, 1, 12);
    // ecc = W5 b13-22 (10 MSB) || W6 b1-22 (22 LSB) = 32 bits unsigned, 2^-33
    const uint64_t eRaw = fetch2U(info, 5, 13, 10, 6, 1, 22);
    // Cus = W7 b1-18 (18 bits signed, 2^-31 rad)
    const int32_t cus   = getbits(info, infoPos(7, 1), 18);
    // Crc = W7 b19-22 (4 MSB) || W8 b1-14 (14 LSB) = 18 bits, 2^-6 m
    const int64_t crc   = fetch2S(info, 7, 19,  4, 8, 1, 14);
    // Crs = W8 b15-22 (8 MSB) || W9 b1-10 (10 LSB) = 18 bits, 2^-6 m
    const int64_t crs   = fetch2S(info, 8, 15,  8, 9, 1, 10);
    // sqrtA = W9 b11-22 (12 MSB) || W10 b1-20 (20 LSB) = 32 bits unsigned, 2^-19
    const uint64_t sqrtA = fetch2U(info, 9, 11, 12, 10, 1, 20);
    // toe MSB (2 bits) = W10 b21-22
    const uint32_t toeMsb = fetchU(info, 10, 21, 2);

    p.k.deltaN = dN    * P2(-43) * GnssMath::PI;
    p.k.cuc    = cuc   * P2(-31);
    p.k.M0     = M0    * P2(-31) * GnssMath::PI;
    p.k.ecc    = static_cast<double>(eRaw)  * P2(-33);
    p.k.cus    = cus   * P2(-31);
    p.k.crc    = crc   * P2(-6);
    p.k.crs    = crs   * P2(-6);
    p.k.sqrtA  = static_cast<double>(sqrtA) * P2(-19);
    p.toeMsb2  = toeMsb;
    p.haveMask |= 0x02;
}

// ── Subframe 3 — ephemeris (2/2) (Table 5-11-3) ───────────────────
inline void Decoder::decodeSF3(Partial& p, const uint8_t info[28])
{
    // toe LSB = W2 b13-22 (10 MSB) || W3 b1-5 (5 LSB) = 15 bits unsigned
    const uint64_t toeLsb = fetch2U(info, 2, 13, 10, 3, 1, 5);
    // i0 = W3 b6-22 (17 MSB) || W4 b1-15 (15 LSB) = 32 bits, 2^-31 semicircle
    const int64_t i0    = fetch2S(info, 3,  6, 17, 4,  1, 15);
    // Cic = W4 b16-22 (7 MSB) || W5 b1-11 (11 LSB) = 18 bits, 2^-31 rad
    const int64_t cic   = fetch2S(info, 4, 16,  7, 5,  1, 11);
    // OmegaDot = W5 b12-22 (11 MSB) || W6 b1-13 (13 LSB) = 24 bits, 2^-43 semicircle/s
    const int64_t Omd   = fetch2S(info, 5, 12, 11, 6,  1, 13);
    // Cis = W6 b14-22 (9 MSB) || W7 b1-9 (9 LSB) = 18 bits, 2^-31 rad
    const int64_t cis   = fetch2S(info, 6, 14,  9, 7,  1,  9);
    // IDOT = W7 b10-22 (13 MSB) || W8 b1 (1 LSB) = 14 bits, 2^-43 semicircle/s
    const int64_t iDot  = fetch2S(info, 7, 10, 13, 8,  1,  1);
    // Omega0 = W8 b2-22 (21 MSB) || W9 b1-11 (11 LSB) = 32 bits, 2^-31 semicircle
    const int64_t Om0   = fetch2S(info, 8,  2, 21, 9,  1, 11);
    // omega = W9 b12-22 (11 MSB) || W10 b1-21 (21 LSB) = 32 bits, 2^-31 semicircle
    const int64_t omega = fetch2S(info, 9, 12, 11, 10, 1, 21);

    // Combine toe: 2 MSB from SF2 + 15 LSB from SF3 = 17 bits, scale 2^3
    const uint32_t toeRaw = (p.toeMsb2 << 15) | static_cast<uint32_t>(toeLsb);

    p.k.i0       = i0    * P2(-31) * GnssMath::PI;
    p.k.cic      = cic   * P2(-31);
    p.k.OmegaDot = Omd   * P2(-43) * GnssMath::PI;
    p.k.cis      = cis   * P2(-31);
    p.k.iDot     = iDot  * P2(-43) * GnssMath::PI;
    p.k.Omega0   = Om0   * P2(-31) * GnssMath::PI;
    p.k.omega    = omega * P2(-31) * GnssMath::PI;
    p.k.toe      = static_cast<double>(toeRaw) * 8.0;  // 2^3

    p.haveMask |= 0x04;
}

inline void Decoder::tryAssemble(uint8_t svId, Partial& p,
                                 EphemerisStore::Store& store)
{
    if (p.haveIonoSF1)
    {
        store.putIono(p.ionoStaged);
        p.haveIonoSF1 = false;
    }

    if ((p.haveMask & 0x07) != 0x07)
        return;

    p.k.gnss      = JimmyPaputto::EGnssId::BeiDou;
    p.k.svId      = svId;
    p.k.muRef     = GnssMath::MU_BDS;
    p.k.omegaERef = GnssMath::OMEGA_BDS;
    p.k.valid     = true;
    store.put(p.k);
}

inline bool Decoder::feed(uint8_t svId,
                          const std::vector<uint32_t>& words,
                          EphemerisStore::Store& store)
{
    if (words.size() < 10)
    {
        ++parityFails;
        return false;
    }
    if (isNullFrame(words))
    {
        ++nullFrames;
        return false;
    }

    // Dedupe: u-blox keeps a subframe ring buffer that is never drained,
    // so the same subframe can be delivered many times. Skip if word-1
    // (preamble + rev + FraID + SOW MSB + parity) is identical to the
    // last one we decoded for this SV.
    auto& lastW0 = lastWord0_[svId];
    if (lastW0 == words[0])
        return false;
    lastW0 = words[0];

    uint8_t info[28];
    if (!decodeSubframeBits(words, info))
    {
        ++parityFails;
        return false;
    }
    ++subframeCount;

    // FraID at word 1 bits 16..18 (info bits 15..17)
    const uint32_t fraId = getbitu(info, 15, 3);

    auto& p = partial_[svId];

    switch (fraId)
    {
        case 1: ++sf1Count; decodeSF1(p, info); break;
        case 2: ++sf2Count; decodeSF2(p, info); break;
        case 3: ++sf3Count; decodeSF3(p, info); break;
        default: return false;  // SF4/SF5 almanac/iono — ignored
    }

    const bool wasValid = p.k.valid;
    tryAssemble(svId, p, store);
    return p.k.valid && !wasValid;
}

}  // namespace BeidouD1

#endif  // BEIDOU_D1_HPP_

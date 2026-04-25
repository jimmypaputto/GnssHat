/*
 * Jimmy Paputto 2026
 *
 * Galileo E1-B I/NAV decoder.
 * Ref: Galileo OS SIS ICD Issue 2.1 §4.3 (I/NAV message structure).
 *
 * Signal: gnssId=2, sigId=1 (E1-B). Each SFRBX carries one complete
 * Galileo nominal I/NAV page, packed by u-blox into 8 U4 words = 256
 * bits, big-endian / MSB-first. The page consists of two half-pages
 * (120 bits each) laid out consecutively:
 *
 *   even half-page (bits   0..127 of SFRBX): 120 data + 8 pad
 *     bit 0        even/odd flag = 0
 *     bit 1        page type  (0 = nominal, 1 = alert)
 *     bits 2..113  Data(1/2)  (112 bits)
 *     bits 114..119 tail (6 zeros)
 *     bits 120..127 u-blox padding
 *
 *   odd half-page (bits 128..255 of SFRBX): 120 data + 8 pad
 *     bit 128       even/odd flag = 1
 *     bit 129       page type
 *     bits 130..145 Data(2/2)  (16 bits)
 *     bits 146..185 Reserved1  (40 bits)
 *     bits 186..207 SAR        (22 bits)
 *     bits 208..209 Spare      (2)
 *     bits 210..233 CRC-24Q    (24 bits, over the 196-bit payload)
 *     bits 234..241 Reserved2  (8 bits)
 *     bits 242..247 tail (6 zeros)
 *     bits 248..255 u-blox padding
 *
 * CRC-24Q (poly 0x1864CFB, init 0) input is the 196-bit concatenation:
 *   bits   0..113  (even half: flag + type + Data(1/2))
 *   bits 128..209  (odd  half: flag + type + Data(2/2) + Reserved1
 *                              + SAR + Spare)
 *
 * After CRC passes, the "combined nav data" (128 bits) is:
 *   Data(1/2) (112) || Data(2/2) (16)
 * with word-type at combined bits 0..5. Ephemeris word types (§4.3.5):
 *
 *   WT 1 (Table 41): IODnav, t0e, M0, e, sqrtA
 *   WT 2 (Table 42): IODnav, Omega0, i0, omega, iDot
 *   WT 3 (Table 43): IODnav, OmegaDot, deltaN, Cuc, Cus, Crc, Crs, SISA
 *   WT 4 (Table 44): IODnav, SVID, Cic, Cis, t0c, af0, af1, af2
 *   WT 5 (Table 45): iono (ai0..ai2), BGD(E1,E5a), BGD(E1,E5b), health,
 *                    DVS, WN, TOW
 *
 * A complete ephemeris requires WT 1..4 carrying the same IODnav. The
 * BGD applied as `Kepler::tgd` is BGD(E1,E5b) from WT 5 (used by single-
 * frequency E5b users and a reasonable approximation for E1 users
 * during first-pass SPP bring-up).
 */

#ifndef GALILEO_INAV_HPP_
#define GALILEO_INAV_HPP_

#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

#include "EphemerisStore.hpp"
#include "GnssMath.hpp"

namespace GalileoInav
{

// ── Bit-level helpers over a byte buffer (MSB-first, 0-indexed) ────
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

// ── 8 × U4 words → 32-byte raw buffer (big-endian MSB-first) ───────
inline void wordsToBytes(const std::vector<uint32_t>& w, uint8_t out[32])
{
    for (int i = 0; i < 8; ++i)
    {
        out[i*4 + 0] = (w[i] >> 24) & 0xFFu;
        out[i*4 + 1] = (w[i] >> 16) & 0xFFu;
        out[i*4 + 2] = (w[i] >>  8) & 0xFFu;
        out[i*4 + 3] = (w[i]      ) & 0xFFu;
    }
}

// ── Copy a bit range from src[srcPos..srcPos+n) into dst starting
//    at dst bit offset *outPos, advancing *outPos by n. MSB-first. ──
inline void copyBits(const uint8_t* src, int srcPos, int n,
                     uint8_t* dst, int& outPos)
{
    for (int i = 0; i < n; ++i)
    {
        const uint32_t bit =
            (src[(srcPos + i) >> 3] >> (7 - ((srcPos + i) & 7))) & 1u;
        dst[outPos >> 3] |= static_cast<uint8_t>(bit << (7 - (outPos & 7)));
        ++outPos;
    }
}

// ── Build the 128-bit combined nav data = Data(1/2) || Data(2/2) ───
inline void buildCombined(const uint8_t raw[32], uint8_t combined[16])
{
    std::memset(combined, 0, 16);
    int o = 0;
    copyBits(raw,   2, 112, combined, o);   // Data(1/2)  — 112 bits
    copyBits(raw, 130,  16, combined, o);   // Data(2/2)  —  16 bits
}

// ── CRC-24Q over two concatenated bit ranges from `raw` ────────────
inline uint32_t crc24qRanges(const uint8_t raw[32],
                             int s1, int n1, int s2, int n2)
{
    constexpr uint32_t POLY = 0x1864CFBu;
    uint32_t crc = 0;

    auto feed = [&](int start, int n) {
        for (int i = 0; i < n; ++i)
        {
            const uint32_t bit =
                (raw[(start + i) >> 3] >> (7 - ((start + i) & 7))) & 1u;
            crc ^= (bit << 23);
            if (crc & 0x800000u)
                crc = ((crc << 1) ^ POLY) & 0xFFFFFFu;
            else
                crc = (crc << 1) & 0xFFFFFFu;
        }
    };
    feed(s1, n1);
    feed(s2, n2);
    return crc & 0xFFFFFFu;
}

// ── Scale helper: 2^n ──────────────────────────────────────────────
constexpr double P2(int n) { return std::ldexp(1.0, n); }

class Decoder
{
public:
    bool feed(uint8_t svId,
              const std::vector<uint32_t>& words,
              EphemerisStore::Store& store);

    size_t pageCount    = 0;
    size_t crcFails     = 0;
    size_t shortMsg     = 0;
    size_t halfPageFails = 0;  // even/odd flag check failed

private:
    struct Partial
    {
        uint8_t  haveMask = 0;   // bit i set when WT(i+1) assembled into k
        uint16_t iodnav   = 0;
        bool     haveBgd  = false;
        double   bgd      = 0.0;  // BGD(E1,E5b)
        EphemerisStore::Kepler k{};
    };
    std::unordered_map<uint8_t, Partial> partial_;

    void decodeWT1(Partial& p, const uint8_t combined[16]);
    void decodeWT2(Partial& p, const uint8_t combined[16]);
    void decodeWT3(Partial& p, const uint8_t combined[16]);
    void decodeWT4(Partial& p, const uint8_t combined[16]);
    void decodeWT5(Partial& p, const uint8_t combined[16]);
    void tryAssemble(uint8_t svId, Partial& p,
                     EphemerisStore::Store& store);
};

// ── Word-type decoders ─────────────────────────────────────────────
//
// Offsets below are bit positions within the 128-bit combined nav
// data (= Data(1/2) || Data(2/2), MSB-first).
//
inline void Decoder::decodeWT1(Partial& p, const uint8_t c[16])
{
    const uint16_t iod   = static_cast<uint16_t>(getbitu(c, 6, 10));
    const uint32_t t0e   = getbitu  (c, 16, 14);
    const int64_t  M0    = getbits64(c, 30, 32);
    const uint64_t eRaw  = getbitu64(c, 62, 32);
    const uint64_t sqrtA = getbitu64(c, 94, 32);

    if (p.haveMask == 0 || p.iodnav != iod)
    {
        // New IODnav set: reset partial orbit fields.
        p.haveMask = 0;
        p.iodnav   = iod;
        p.k = EphemerisStore::Kepler{};
    }
    p.k.toe    = t0e * 60.0;
    p.k.M0     = M0    * P2(-31) * GnssMath::PI;
    p.k.ecc    = eRaw  * P2(-33);
    p.k.sqrtA  = sqrtA * P2(-19);
    p.haveMask |= 0x01;
}

inline void Decoder::decodeWT2(Partial& p, const uint8_t c[16])
{
    const uint16_t iod   = static_cast<uint16_t>(getbitu(c, 6, 10));
    const int64_t  Om0   = getbits64(c,  16, 32);
    const int64_t  i0    = getbits64(c,  48, 32);
    const int64_t  omega = getbits64(c,  80, 32);
    const int32_t  iDot  = getbits  (c, 112, 14);

    if (p.haveMask == 0 || p.iodnav != iod)
    {
        p.haveMask = 0;
        p.iodnav   = iod;
        p.k = EphemerisStore::Kepler{};
    }
    p.k.Omega0 = Om0   * P2(-31) * GnssMath::PI;
    p.k.i0     = i0    * P2(-31) * GnssMath::PI;
    p.k.omega  = omega * P2(-31) * GnssMath::PI;
    p.k.iDot   = iDot  * P2(-43) * GnssMath::PI;
    p.haveMask |= 0x02;
}

inline void Decoder::decodeWT3(Partial& p, const uint8_t c[16])
{
    const uint16_t iod   = static_cast<uint16_t>(getbitu(c, 6, 10));
    const int32_t  Omd   = getbits(c,  16, 24);
    const int32_t  dN    = getbits(c,  40, 16);
    const int32_t  cuc   = getbits(c,  56, 16);
    const int32_t  cus   = getbits(c,  72, 16);
    const int32_t  crc   = getbits(c,  88, 16);
    const int32_t  crs   = getbits(c, 104, 16);
    /* SISA(8) at 120 — not used */

    if (p.haveMask == 0 || p.iodnav != iod)
    {
        p.haveMask = 0;
        p.iodnav   = iod;
        p.k = EphemerisStore::Kepler{};
    }
    p.k.OmegaDot = Omd * P2(-43) * GnssMath::PI;
    p.k.deltaN   = dN  * P2(-43) * GnssMath::PI;
    p.k.cuc      = cuc * P2(-29);
    p.k.cus      = cus * P2(-29);
    p.k.crc      = crc * P2(-5);
    p.k.crs      = crs * P2(-5);
    p.haveMask |= 0x04;
}

inline void Decoder::decodeWT4(Partial& p, const uint8_t c[16])
{
    const uint16_t iod   = static_cast<uint16_t>(getbitu(c, 6, 10));
    /* SVID(6) at 16 — sanity, we already have svId from SFRBX */
    const int32_t  cic   = getbits(c,  22, 16);
    const int32_t  cis   = getbits(c,  38, 16);
    const uint32_t t0c   = getbitu(c,  54, 14);
    const int32_t  af0   = getbits(c,  68, 31);
    const int32_t  af1   = getbits(c,  99, 21);
    const int32_t  af2   = getbits(c, 120,  6);

    if (p.haveMask == 0 || p.iodnav != iod)
    {
        p.haveMask = 0;
        p.iodnav   = iod;
        p.k = EphemerisStore::Kepler{};
    }
    p.k.cic = cic * P2(-29);
    p.k.cis = cis * P2(-29);
    p.k.toc = t0c * 60.0;
    p.k.af0 = af0 * P2(-34);
    p.k.af1 = af1 * P2(-46);
    p.k.af2 = af2 * P2(-59);
    p.haveMask |= 0x08;
}

inline void Decoder::decodeWT5(Partial& p, const uint8_t c[16])
{
    /* ai0(11)  at  6 — iono, skip (NeQuick not yet implemented) */
    /* ai1(11)  at 17 */
    /* ai2(14)  at 28 */
    /* Region1..5(5) at 42 */
    /* BGD(E1,E5a)(10) at 47 */
    const int32_t bgdE1E5b = getbits(c, 57, 10);
    /* E5b_HS(2) 67, E1B_HS(2) 69, E5b_DVS(1) 71, E1B_DVS(1) 72 */
    /* WN(12) 73, TOW(20) 85, Spare(23) 105 */

    p.bgd     = bgdE1E5b * P2(-32);
    p.haveBgd = true;
    if (p.haveMask & 0x0F)
        p.k.tgd = p.bgd;  // apply immediately if orbit already in flight
}

inline void Decoder::tryAssemble(uint8_t svId, Partial& p,
                                 EphemerisStore::Store& store)
{
    if ((p.haveMask & 0x0F) != 0x0F)
        return;

    p.k.gnss      = JimmyPaputto::EGnssId::Galileo;
    p.k.svId      = svId;
    p.k.muRef     = GnssMath::MU_GAL;
    p.k.omegaERef = GnssMath::OMEGA_E;
    if (p.haveBgd)
        p.k.tgd = p.bgd;
    p.k.valid     = true;
    store.put(p.k);
}

// ── Page dispatch ──────────────────────────────────────────────────
inline bool Decoder::feed(uint8_t svId,
                          const std::vector<uint32_t>& words,
                          EphemerisStore::Store& store)
{
    if (words.size() < 8)
    {
        ++shortMsg;
        return false;
    }

    uint8_t raw[32];
    wordsToBytes(words, raw);

    // Half-page flags: bit 0 must be 0 (even), bit 128 must be 1 (odd).
    const uint32_t evenFlag = getbitu(raw,   0, 1);
    const uint32_t oddFlag  = getbitu(raw, 128, 1);
    if (evenFlag != 0 || oddFlag != 1)
    {
        ++halfPageFails;
        return false;
    }

    // CRC-24Q over 196 bits (bits 0..113 ++ 128..209); check bits 210..233.
    const uint32_t expected = getbitu(raw, 210, 24);
    const uint32_t actual   = crc24qRanges(raw, 0, 114, 128, 82);
    if (expected != actual)
    {
        ++crcFails;
        return false;
    }

    ++pageCount;

    uint8_t combined[16];
    buildCombined(raw, combined);

    const uint32_t wt = getbitu(combined, 0, 6);
    auto& p = partial_[svId];

    switch (wt)
    {
        case 1:  decodeWT1(p, combined); break;
        case 2:  decodeWT2(p, combined); break;
        case 3:  decodeWT3(p, combined); break;
        case 4:  decodeWT4(p, combined); break;
        case 5:  decodeWT5(p, combined); break;
        default: return false;  // other word types ignored
    }

    const bool wasValid = p.k.valid;
    tryAssemble(svId, p, store);
    return p.k.valid && !wasValid;
}

}  // namespace GalileoInav

#endif  // GALILEO_INAV_HPP_

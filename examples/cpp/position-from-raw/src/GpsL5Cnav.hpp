/*
 * Jimmy Paputto 2026
 *
 * GPS L5 CNAV decoder (message types 10, 11, 30).
 * Ref: IS-GPS-705 §20.3.3 (message structure + MT10/11 ephemeris),
 *      IS-GPS-200 §30.3.3 (CNAV MT30 clock + iono + group delay),
 *      both polynomial CRC-24Q §3.5.3.5 (0x1864CFB, init 0).
 *
 * Signal: gnssId=0, sigId=6 (L5 I). u-blox delivers each CNAV message
 * in SFRBX as 10 U4 words = 320 bits, of which the first 300 bits are
 * the message (left-justified, bit 1 = MSB of word 0).
 *
 * Message frame (IS-GPS-705 Table 20-I):
 *   bits   1..8   preamble  (10001011)
 *   bits   9..14  PRN        (6)
 *   bits  15..20  MT ID      (6)
 *   bits  21..37  TOW count  (17, units 6 s)
 *   bit    38     alert
 *   bits  39..276 payload    (238, per MT)
 *   bits 277..300 CRC-24Q    (over bits 1..276, init 0)
 *
 * A satellite's broadcast ephemeris is complete when MT10, MT11 and
 * MT30 with matching top/toe references have all been received.
 */

#ifndef GPS_L5_CNAV_HPP_
#define GPS_L5_CNAV_HPP_

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include <vector>

#include "EphemerisStore.hpp"
#include "GnssMath.hpp"

namespace GpsL5Cnav
{

// ── Reference values (IS-GPS-705) ──────────────────────────────────
constexpr double A_REF_GPS      = 26559710.0;                    // m
constexpr double OMEGA_DOT_REF  = -2.6e-9 * GnssMath::PI;        // rad/s

// ── Bit reader over message bit vector ─────────────────────────────
//
// Bits are 1-indexed; bits are packed MSB-first across the 10 U4
// words. Word 0's MSB (bit 31) is message bit 1.
//
class BitView
{
public:
    explicit BitView(const std::vector<uint32_t>& w) : w_(w) {}

    // Unsigned extract, 1..32 bits, 1-indexed starting bit.
    uint32_t u(int startBit, int nBits) const
    {
        uint32_t v = 0;
        for (int i = 0; i < nBits; ++i)
        {
            const int b = startBit + i - 1;    // 0-indexed bit
            const int word = b >> 5;
            const int pos  = 31 - (b & 31);
            v = (v << 1) | ((w_[word] >> pos) & 1u);
        }
        return v;
    }

    // Signed extract (two's complement, nBits wide).
    int32_t s(int startBit, int nBits) const
    {
        const uint32_t raw = u(startBit, nBits);
        const uint32_t sign = 1u << (nBits - 1);
        if (raw & sign)
            return static_cast<int32_t>(raw | (~0u << nBits));
        return static_cast<int32_t>(raw);
    }

    // For fields wider than 32 bits (M0, ecc, omega, Omega0, i0 are 33 bits).
    uint64_t u64(int startBit, int nBits) const
    {
        uint64_t v = 0;
        for (int i = 0; i < nBits; ++i)
        {
            const int b = startBit + i - 1;
            const int word = b >> 5;
            const int pos  = 31 - (b & 31);
            v = (v << 1) | ((uint64_t)((w_[word] >> pos) & 1u));
        }
        return v;
    }

    int64_t s64(int startBit, int nBits) const
    {
        const uint64_t raw = u64(startBit, nBits);
        const uint64_t sign = uint64_t{1} << (nBits - 1);
        if (raw & sign)
            return static_cast<int64_t>(raw | (~uint64_t{0} << nBits));
        return static_cast<int64_t>(raw);
    }

private:
    const std::vector<uint32_t>& w_;
};

// ── CRC-24Q over the first 276 bits of the message ─────────────────
//
// Polynomial 0x1864CFB (implicit x^24); init 0; appended big-endian as
// bits 277..300 of the message. We feed 276 data bits MSB-first and
// then compare against u(277, 24).
//
inline uint32_t crc24q(const BitView& bv, int startBit, int nBits)
{
    constexpr uint32_t POLY = 0x1864CFBu;
    uint32_t crc = 0;
    for (int i = 0; i < nBits; ++i)
    {
        const uint32_t bit = bv.u(startBit + i, 1);
        crc ^= (bit << 23);
        for (int k = 0; k < 1; ++k)
        {
            if (crc & 0x800000u)
                crc = ((crc << 1) ^ POLY) & 0xFFFFFFu;
            else
                crc = (crc << 1) & 0xFFFFFFu;
        }
    }
    return crc & 0xFFFFFFu;
}

// ── Scale helpers (semicircles → rad multiplied in one step) ───────
constexpr double P2(int n)  { return std::ldexp(1.0, n); }   // 2^n

class Decoder
{
public:
    bool feed(uint8_t svId,
              const std::vector<uint32_t>& words,
              EphemerisStore::Store& store);

    size_t mt10Count = 0, mt11Count = 0, mt30Count = 0, crcFails = 0;
    size_t preambleFails = 0, shortMsg = 0;

private:
    struct Partial
    {
        bool     have10 = false, have11 = false, have30 = false;
        uint16_t wn     = 0;
        uint32_t top10  = 0;
        uint32_t toe10  = 0;
        uint32_t toe11  = 0;
        uint32_t toc30  = 0;
        EphemerisStore::Kepler k{};
    };
    std::unordered_map<uint8_t, Partial> partial_;

    void decodeMT10(Partial& p, const BitView& bv);
    void decodeMT11(Partial& p, const BitView& bv);
    void decodeMT30(Partial& p, const BitView& bv,
                    EphemerisStore::Store& store);
    void tryAssemble(uint8_t svId, Partial& p,
                     EphemerisStore::Store& store);
};

// ── MT decoders ────────────────────────────────────────────────────
inline void Decoder::decodeMT10(Partial& p, const BitView& bv)
{
    // MT10 payload starts at bit 39 (IS-GPS-705 Table 30-I).
    int b = 39;
    p.wn        = bv.u (b, 13); b += 13;
    /*health L1*/bv.u (b, 1);   b += 1;
    /*health L2*/bv.u (b, 1);   b += 1;
    const uint32_t hL5 = bv.u(b, 1); b += 1;
    p.top10     = bv.u (b, 11); b += 11;
    /*URA_ED*/   bv.s (b, 5);   b += 5;
    p.toe10     = bv.u (b, 11); b += 11;
    const int32_t dA    = bv.s (b, 26); b += 26;
    /*Adot*/     bv.s (b, 25);  b += 25;            // ignored (phase 1)
    const int32_t dn0   = bv.s (b, 17); b += 17;
    /*dn0dot*/   bv.s (b, 23);  b += 23;            // ignored (phase 1)
    const int64_t M0    = bv.s64(b, 33); b += 33;
    const uint64_t eRaw = bv.u64(b, 33); b += 33;
    const int64_t omega = bv.s64(b, 33); b += 33;
    // ISF (1) + L2C Phasing (1) + reserved (3) not needed

    const double A = A_REF_GPS + dA * P2(-9);
    p.k.sqrtA  = std::sqrt(A);
    p.k.toe    = p.toe10 * 300.0;
    p.k.deltaN = dn0 * P2(-44) * GnssMath::PI;        // rad/s
    p.k.M0     = M0    * P2(-32) * GnssMath::PI;      // rad
    p.k.ecc    = eRaw  * P2(-34);
    p.k.omega  = omega * P2(-32) * GnssMath::PI;      // rad

    // Mark unhealthy if L5 Health = 1 (1 = unhealthy)
    p.k.valid &= (hL5 == 0);  // preserve: full validity set on assembly
    p.have10 = true;
}

inline void Decoder::decodeMT11(Partial& p, const BitView& bv)
{
    // MT11 payload starts at bit 39. Field order per IS-GPS-705 Rev D
    // Table 30-II:  toe, Ω0, i0, ΔΩdot, iDOT, Cis, Cic, Crs, Crc, Cus, Cuc.
    // (Note: i0 comes BEFORE ΔΩdot — earlier versions of this decoder
    // had them swapped, which produced physically-impossible inclination
    // values for all GPS SVs and silently broke ~3 SVs per epoch.)
    int b = 39;
    p.toe11           = bv.u(b, 11);  b += 11;
    const int64_t  Om0 = bv.s64(b, 33); b += 33;
    const int64_t  i0  = bv.s64(b, 33); b += 33;
    const int32_t dOmd = bv.s (b, 17); b += 17;
    const int32_t iDot = bv.s (b, 15); b += 15;
    const int32_t cis  = bv.s (b, 16); b += 16;
    const int32_t cic  = bv.s (b, 16); b += 16;
    const int32_t crs  = bv.s (b, 24); b += 24;
    const int32_t crc  = bv.s (b, 24); b += 24;
    const int32_t cus  = bv.s (b, 21); b += 21;
    const int32_t cuc  = bv.s (b, 21); b += 21;

    p.k.Omega0   = Om0  * P2(-32) * GnssMath::PI;
    p.k.OmegaDot = OMEGA_DOT_REF + dOmd * P2(-44) * GnssMath::PI;
    p.k.i0       = i0   * P2(-32) * GnssMath::PI;
    p.k.iDot     = iDot * P2(-44) * GnssMath::PI;
    p.k.cis      = cis  * P2(-30);
    p.k.cic      = cic  * P2(-30);
    p.k.crs      = crs  * P2(-8);
    p.k.crc      = crc  * P2(-8);
    p.k.cus      = cus  * P2(-30);
    p.k.cuc      = cuc  * P2(-30);

    p.have11 = true;
}

inline void Decoder::decodeMT30(Partial& p, const BitView& bv,
                                EphemerisStore::Store& store)
{
    int b = 39;
    /*top*/      bv.u(b, 11); b += 11;
    /*URA_NED0*/ bv.s(b, 5);  b += 5;
    /*URA_NED1*/ bv.u(b, 3);  b += 3;
    /*URA_NED2*/ bv.u(b, 3);  b += 3;
    p.toc30         = bv.u(b, 11); b += 11;
    const int32_t af0       = bv.s(b, 26); b += 26;
    const int32_t af1       = bv.s(b, 20); b += 20;
    const int32_t af2       = bv.s(b, 10); b += 10;
    const int32_t tgd       = bv.s(b, 13); b += 13;
    /*ISC_L1CA*/              bv.s(b, 13); b += 13;
    /*ISC_L2C*/               bv.s(b, 13); b += 13;
    const int32_t iscL5I5   = bv.s(b, 13); b += 13;
    /*ISC_L5Q5*/              bv.s(b, 13); b += 13;
    const int32_t a0 = bv.s(b, 8); b += 8;
    const int32_t a1 = bv.s(b, 8); b += 8;
    const int32_t a2 = bv.s(b, 8); b += 8;
    const int32_t a3 = bv.s(b, 8); b += 8;
    const int32_t B0 = bv.s(b, 8); b += 8;
    const int32_t B1 = bv.s(b, 8); b += 8;
    const int32_t B2 = bv.s(b, 8); b += 8;
    const int32_t B3 = bv.s(b, 8); b += 8;

    p.k.toc = p.toc30 * 300.0;
    p.k.af0 = af0 * P2(-35);
    p.k.af1 = af1 * P2(-48);
    p.k.af2 = af2 * P2(-60);

    // Net group delay to apply for an L5-I user:
    //   dt_L5I = af(t) + Δt_rel - TGD - ISC_L5I5
    // Store pre-summed so SPP can do clockBias - tgd directly.
    p.k.tgd = (tgd + iscL5I5) * P2(-35);

    // Klobuchar iono (shared across satellites — any SV's broadcast is valid)
    EphemerisStore::IonoKlobuchar iono;
    iono.valid = true;
    iono.alpha[0] = a0 * P2(-30);
    iono.alpha[1] = a1 * P2(-27);
    iono.alpha[2] = a2 * P2(-24);
    iono.alpha[3] = a3 * P2(-24);
    iono.beta [0] = B0 * P2(11);
    iono.beta [1] = B1 * P2(14);
    iono.beta [2] = B2 * P2(16);
    iono.beta [3] = B3 * P2(16);
    store.putIono(iono);

    p.have30 = true;
}

inline void Decoder::tryAssemble(uint8_t svId, Partial& p,
                                 EphemerisStore::Store& store)
{
    if (!(p.have10 && p.have11 && p.have30))
        return;
    if (p.toe10 != p.toe11)       // MT10/MT11 must reference same toe
        return;

    // Physical-plausibility check on decoded orbital elements. Some
    // GPS satellites (commissioning Block III, end-of-life Block IIR-M)
    // broadcast L5 CNAV frames that pass CRC but contain garbage values.
    // Reject any ephemeris whose Keplerian elements fall outside the
    // physical envelope of the GPS constellation.
    //   sqrtA  ≈ 5153.7 m^(1/2)  (semi-major axis ≈ 26,560 km)
    //   e      < 0.03            (orbit nearly circular)
    //   |i0|   ∈ [0.84, 1.10] rad (≈48°–63°, real value ≈55°)
    constexpr double GPS_SQRTA_MIN = 5100.0;
    constexpr double GPS_SQRTA_MAX = 5200.0;
    constexpr double GPS_ECC_MAX   = 0.03;
    constexpr double GPS_I0_MIN    = 0.84;   // rad ≈ 48°
    constexpr double GPS_I0_MAX    = 1.10;   // rad ≈ 63°
    const double absI0 = std::fabs(p.k.i0);
    if (p.k.sqrtA < GPS_SQRTA_MIN || p.k.sqrtA > GPS_SQRTA_MAX
        || std::fabs(p.k.ecc) > GPS_ECC_MAX
        || absI0 < GPS_I0_MIN || absI0 > GPS_I0_MAX)
    {
        if (std::getenv("SPP_DEBUG_EPH"))
        {
            std::fprintf(stderr,
                "[CNAV] reject sv=%u sqrtA=%.2f e=%.4f i0=%.3frad — "
                "outside physical envelope\n",
                svId, p.k.sqrtA, p.k.ecc, p.k.i0);
        }
        // Clear the partial so we don't keep retrying the same garbage,
        // and wait for a fresh MT10/MT11 pair (typically 5 minutes).
        p.have10 = p.have11 = false;
        return;
    }

    p.k.gnss      = JimmyPaputto::EGnssId::GPS;
    p.k.svId      = svId;
    p.k.muRef     = GnssMath::MU_GPS;
    p.k.omegaERef = GnssMath::OMEGA_E;
    p.k.valid     = true;
    store.put(p.k);
}

// ── Dispatch ───────────────────────────────────────────────────────
inline bool Decoder::feed(uint8_t svId,
                          const std::vector<uint32_t>& words,
                          EphemerisStore::Store& store)
{
    if (words.size() < 10)
    {
        ++shortMsg;
        return false;
    }

    BitView bv(words);

    // Preamble check (bits 1..8 = 10001011 = 0x8B).
    if (bv.u(1, 8) != 0x8Bu)
    {
        ++preambleFails;
        return false;
    }

    // CRC-24Q over bits 1..276, compare with bits 277..300.
    const uint32_t expected = bv.u(277, 24);
    const uint32_t actual   = crc24q(bv, 1, 276);
    if (expected != actual)
    {
        ++crcFails;
        return false;
    }

    const uint32_t mt = bv.u(15, 6);
    auto& p = partial_[svId];

    // Optional raw-bits dump for forensic decoding (env: SPP_DEBUG_RAW_CNAV).
    // Lets us hand-verify field positions/scales against IS-GPS-705
    // for SVs that produce bad ephemerides downstream.
    if (std::getenv("SPP_DEBUG_RAW_CNAV"))
    {
        std::fprintf(stderr, "[CNAV] sv=%u mt=%u words=", svId, mt);
        for (size_t i = 0; i < 10 && i < words.size(); ++i)
            std::fprintf(stderr, "%08X ", words[i]);
        std::fprintf(stderr, "\n");
    }

    switch (mt)
    {
        case 10:
            decodeMT10(p, bv);
            ++mt10Count;
            break;
        case 11:
            decodeMT11(p, bv);
            ++mt11Count;
            break;
        case 30:
            decodeMT30(p, bv, store);
            ++mt30Count;
            break;
        default:
            return false;  // other MTs (12, 13, 14, 31..37) ignored
    }

    const bool wasValid = p.k.valid;
    tryAssemble(svId, p, store);
    return p.k.valid && !wasValid;
}

}  // namespace GpsL5Cnav

#endif  // GPS_L5_CNAV_HPP_

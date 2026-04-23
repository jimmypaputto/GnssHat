/*
 * Jimmy Paputto 2026
 *
 * Galileo Broadcast Ephemeris Decoder
 *
 * Decodes Galileo I/NAV navigation message pages (from UBX-RXM-SFRBX)
 * into broadcast ephemeris parameters, then computes satellite ECEF
 * positions and clock corrections at any given Galileo System Time (GST).
 *
 * Message structure:
 *   Galileo I/NAV (E1-B signal) transmits 250-bit pages consisting
 *   of an even part (114 data bits) and an odd part (120 data bits).
 *   The u-blox receiver delivers complete pages via SFRBX with the
 *   data bits packed into 32-bit words.
 *
 *   Ephemeris is carried in word types 1–5:
 *     Type 1: toe, M0, e, sqrt(A)
 *     Type 2: Omega0, i0, omega, iDot
 *     Type 3: OmegaDot, deltaN, Cuc, Cus, Crc, Crs
 *     Type 4: Cic, Cis, toc, af0, af1, af2
 *     Type 5: BGD(E1,E5a), BGD(E1,E5b), health/validity, GST week
 *
 * Orbit model:
 *   Galileo uses the same Keplerian orbital model as GPS (IS-GPS-200
 *   Table 20-IV) with identical WGS-84 constants (mu, omega_e).
 *   The satellite position computation is therefore the same algorithm.
 *
 * Reference: Galileo OS SIS ICD Issue 2.1, Section 4.3
 */

#ifndef GALILEO_EPHEMERIS_HPP_
#define GALILEO_EPHEMERIS_HPP_

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include <jimmypaputto/GnssHat.hpp>
#include "GnssMath.hpp"

namespace GalileoEphemeris
{

// ── Galileo orbital constants ───────────────────────────────────────
// Same as GPS (WGS-84 compatible): mu and omega_e are identical.
constexpr double GAL_MU      = 3.986004418e14;    // m³/s² (Galileo value per ICD)
constexpr double GAL_OMEGA_E = 7.2921151467e-5;   // rad/s
constexpr double GAL_F       = -4.442807309e-10;   // Relativistic correction (s/√m)

// ── Broadcast ephemeris parameters ──────────────────────────────────
struct Ephemeris
{
    uint8_t  svId = 0;

    // Word type 5 fields
    uint16_t weekNumber = 0;  // GST week number (12-bit)
    double   bgdE1E5a = 0;   // Group delay E1-E5a (s)
    double   bgdE1E5b = 0;   // Group delay E1-E5b (s)
    uint8_t  e1bDvs = 0;     // Data validity status
    uint8_t  e1bHs = 0;      // Health status (0 = OK)
    uint16_t iodNav = 0;     // Issue of data, navigation

    // Word type 4 fields (clock)
    double   toc = 0;        // Clock reference time (s)
    double   af0 = 0;        // Clock bias (s)
    double   af1 = 0;        // Clock drift (s/s)
    double   af2 = 0;        // Clock drift rate (s/s²)

    // Word type 1 fields (ephemeris part 1)
    double   toe = 0;        // Ephemeris reference time (s)
    double   m0 = 0;         // Mean anomaly at reference time (rad)
    double   e = 0;          // Eccentricity
    double   sqrtA = 0;      // sqrt(semi-major axis) (m^1/2)

    // Word type 2 fields (ephemeris part 2)
    double   omega0 = 0;     // Longitude of ascending node (rad)
    double   i0 = 0;         // Inclination at reference time (rad)
    double   omega = 0;      // Argument of perigee (rad)
    double   iDot = 0;       // Rate of inclination (rad/s)

    // Word type 3 fields (ephemeris part 3)
    double   omegaDot = 0;   // Rate of right ascension (rad/s)
    double   deltaN = 0;     // Mean motion difference (rad/s)
    double   cuc = 0;        // Cosine correction, argument of latitude (rad)
    double   cus = 0;        // Sine correction, argument of latitude (rad)
    double   crc = 0;        // Cosine correction, orbit radius (m)
    double   crs = 0;        // Sine correction, orbit radius (m)
    double   cic = 0;        // Cosine correction, inclination (rad)
    double   cis = 0;        // Sine correction, inclination (rad)

    // Completeness tracking (one flag per word type)
    bool hasType1 = false;
    bool hasType2 = false;
    bool hasType3 = false;
    bool hasType4 = false;
    bool hasType5 = false;

    bool isComplete() const
    {
        return hasType1 && hasType2 && hasType3 && hasType4 && hasType5;
    }

    bool isHealthy() const
    {
        return e1bHs == 0 && e1bDvs == 0;
    }
};

// ── Bit extraction for Galileo I/NAV pages ──────────────────────────
//
// u-blox delivers Galileo I/NAV pages as 8 × 32-bit words (256 bits).
// The even half (114 bits) and odd half (120 bits) of a page are
// concatenated, with the useful data bits packed MSB-first.
//
// We extract bits from the concatenated bit stream. Bit 0 is the MSB
// of word[0]. Total useful bits: 128 data bits + field layouts per ICD.
//
// For simplicity, we treat the 8 words as a flat 256-bit array and
// extract using absolute bit positions (0-based from MSB of word 0).
//

inline uint32_t getBits(const std::vector<uint32_t>& words,
                        int bitOffset, int numBits)
{
    uint32_t result = 0;
    for (int i = 0; i < numBits; ++i)
    {
        const int absBit = bitOffset + i;
        const int wordIdx = absBit / 32;
        const int bitIdx = 31 - (absBit % 32);

        if (wordIdx >= static_cast<int>(words.size())) break;

        if (words[wordIdx] & (1u << bitIdx))
            result |= (1u << (numBits - 1 - i));
    }
    return result;
}

inline int32_t getSignedBits(const std::vector<uint32_t>& words,
                             int bitOffset, int numBits)
{
    uint32_t val = getBits(words, bitOffset, numBits);
    if (val & (1u << (numBits - 1)))
        return static_cast<int32_t>(val | (~0u << numBits));
    return static_cast<int32_t>(val);
}

// ── Word type identification ────────────────────────────────────────
//
// The word type is in the first 6 bits of the page data (after
// the even/odd indicator). In SFRBX, the first 2 bits are the
// even/odd flag, bits 2-7 are the word type.
//
inline uint8_t getWordType(const std::vector<uint32_t>& words)
{
    if (words.empty()) return 255;
    // Bits 0-1: even/odd, bits 2-7: word type (6 bits)
    return static_cast<uint8_t>(getBits(words, 2, 6));
}

// ── Page decoders ───────────────────────────────────────────────────
//
// Galileo I/NAV data layouts per OS SIS ICD Table 39-44.
// Bit offsets are from the start of the page data (bit 0 = MSB of word 0).
// The first 8 bits are the page header (even/odd + word type).
//

inline bool decodeWordType1(const std::vector<uint32_t>& words, Ephemeris& eph)
{
    // IODnav (10 bits), toe (14 bits, scale 60s), M0 (32 bits), e (32 bits), sqrtA (32 bits)
    eph.iodNav = static_cast<uint16_t>(getBits(words, 8, 10));
    eph.toe    = getBits(words, 18, 14) * 60.0;
    eph.m0     = getSignedBits(words, 32, 32) * std::pow(2, -31) * GnssMath::PI;
    eph.e      = getBits(words, 64, 32) * std::pow(2, -33);
    eph.sqrtA  = getBits(words, 96, 32) * std::pow(2, -19);

    eph.hasType1 = true;
    return true;
}

inline bool decodeWordType2(const std::vector<uint32_t>& words, Ephemeris& eph)
{
    // IODnav (10), Omega0 (32), i0 (32), omega (32), iDot (14)
    const uint16_t iod = static_cast<uint16_t>(getBits(words, 8, 10));
    if (eph.hasType1 && iod != eph.iodNav) return false;  // IOD mismatch
    eph.iodNav = iod;

    eph.omega0  = getSignedBits(words, 18, 32) * std::pow(2, -31) * GnssMath::PI;
    eph.i0      = getSignedBits(words, 50, 32) * std::pow(2, -31) * GnssMath::PI;
    eph.omega   = getSignedBits(words, 82, 32) * std::pow(2, -31) * GnssMath::PI;
    eph.iDot    = getSignedBits(words, 114, 14) * std::pow(2, -43) * GnssMath::PI;

    eph.hasType2 = true;
    return true;
}

inline bool decodeWordType3(const std::vector<uint32_t>& words, Ephemeris& eph)
{
    // IODnav (10), OmegaDot (24), deltaN (16), Cuc (16), Cus (16), Crc (16), Crs (16), SISA (8)
    const uint16_t iod = static_cast<uint16_t>(getBits(words, 8, 10));
    if (eph.hasType1 && iod != eph.iodNav) return false;
    eph.iodNav = iod;

    eph.omegaDot = getSignedBits(words, 18, 24) * std::pow(2, -43) * GnssMath::PI;
    eph.deltaN   = getSignedBits(words, 42, 16) * std::pow(2, -43) * GnssMath::PI;
    eph.cuc      = getSignedBits(words, 58, 16) * std::pow(2, -29);
    eph.cus      = getSignedBits(words, 74, 16) * std::pow(2, -29);
    eph.crc      = getSignedBits(words, 90, 16) * std::pow(2, -5);
    eph.crs      = getSignedBits(words, 106, 16) * std::pow(2, -5);
    // SISA at bits 122 (8 bits) — accuracy index, not needed for position

    eph.hasType3 = true;
    return true;
}

inline bool decodeWordType4(const std::vector<uint32_t>& words, Ephemeris& eph)
{
    // IODnav (10), Cic (16), Cis (16), toc (14, scale 60s), af0 (31), af1 (21), af2 (6)
    const uint16_t iod = static_cast<uint16_t>(getBits(words, 8, 10));
    if (eph.hasType1 && iod != eph.iodNav) return false;
    eph.iodNav = iod;

    eph.cic = getSignedBits(words, 18, 16) * std::pow(2, -29);
    eph.cis = getSignedBits(words, 34, 16) * std::pow(2, -29);
    eph.toc = getBits(words, 50, 14) * 60.0;
    eph.af0 = getSignedBits(words, 64, 31) * std::pow(2, -34);
    eph.af1 = getSignedBits(words, 95, 21) * std::pow(2, -46);
    eph.af2 = getSignedBits(words, 116, 6) * std::pow(2, -59);

    eph.hasType4 = true;
    return true;
}

inline bool decodeWordType5(const std::vector<uint32_t>& words, Ephemeris& eph)
{
    // BGD(E1,E5a) (10), BGD(E1,E5b) (10), E1-B health (2), E1-B DVS (1), WN (12)
    eph.bgdE1E5a = getSignedBits(words, 8, 10) * std::pow(2, -32);
    eph.bgdE1E5b = getSignedBits(words, 18, 10) * std::pow(2, -32);
    eph.e1bHs    = static_cast<uint8_t>(getBits(words, 28, 2));
    eph.e1bDvs   = static_cast<uint8_t>(getBits(words, 30, 1));
    eph.weekNumber = static_cast<uint16_t>(getBits(words, 73, 12));

    eph.hasType5 = true;
    return true;
}

// ── Satellite ECEF position (Keplerian, same algorithm as GPS) ──────
inline GnssMath::Ecef computeSvPosition(const Ephemeris& eph, double gstTow)
{
    const double A = eph.sqrtA * eph.sqrtA;
    const double n0 = std::sqrt(GAL_MU / (A * A * A));
    const double n = n0 + eph.deltaN;

    double tk = gstTow - eph.toe;
    if (tk >  302400.0) tk -= 604800.0;
    if (tk < -302400.0) tk += 604800.0;

    const double Mk = eph.m0 + n * tk;

    double Ek = Mk;
    for (int i = 0; i < 15; ++i)
    {
        const double Ek_new = Mk + eph.e * std::sin(Ek);
        if (std::fabs(Ek_new - Ek) < 1e-15) break;
        Ek = Ek_new;
    }

    const double sinEk = std::sin(Ek);
    const double cosEk = std::cos(Ek);
    const double vk = std::atan2(
        std::sqrt(1.0 - eph.e * eph.e) * sinEk,
        cosEk - eph.e
    );

    const double phik = vk + eph.omega;
    const double sin2phi = std::sin(2.0 * phik);
    const double cos2phi = std::cos(2.0 * phik);

    const double deltaUk = eph.cus * sin2phi + eph.cuc * cos2phi;
    const double deltaRk = eph.crs * sin2phi + eph.crc * cos2phi;
    const double deltaIk = eph.cis * sin2phi + eph.cic * cos2phi;

    const double uk = phik + deltaUk;
    const double rk = A * (1.0 - eph.e * cosEk) + deltaRk;
    const double ik = eph.i0 + deltaIk + eph.iDot * tk;

    const double xkp = rk * std::cos(uk);
    const double ykp = rk * std::sin(uk);

    const double omegak = eph.omega0
                        + (eph.omegaDot - GAL_OMEGA_E) * tk
                        - GAL_OMEGA_E * eph.toe;

    const double sinOmk = std::sin(omegak);
    const double cosOmk = std::cos(omegak);
    const double sinIk  = std::sin(ik);
    const double cosIk  = std::cos(ik);

    return {
        xkp * cosOmk - ykp * cosIk * sinOmk,
        xkp * sinOmk + ykp * cosIk * cosOmk,
        ykp * sinIk
    };
}

// ── Satellite clock correction ──────────────────────────────────────
inline double computeSvClockBias(const Ephemeris& eph, double gstTow)
{
    double dt = gstTow - eph.toc;
    if (dt >  302400.0) dt -= 604800.0;
    if (dt < -302400.0) dt += 604800.0;

    // Relativistic correction
    const double A = eph.sqrtA * eph.sqrtA;
    const double n0 = std::sqrt(GAL_MU / (A * A * A));
    const double n = n0 + eph.deltaN;

    double tk = gstTow - eph.toe;
    if (tk >  302400.0) tk -= 604800.0;
    if (tk < -302400.0) tk += 604800.0;

    double Mk = eph.m0 + n * tk;
    double Ek = Mk;
    for (int i = 0; i < 15; ++i)
    {
        double Ek_new = Mk + eph.e * std::sin(Ek);
        if (std::fabs(Ek_new - Ek) < 1e-15) break;
        Ek = Ek_new;
    }

    const double dtr = GAL_F * eph.e * eph.sqrtA * std::sin(Ek);

    // For E1 signal, apply BGD(E1,E5a) group delay
    return eph.af0 + eph.af1 * dt + eph.af2 * dt * dt + dtr - eph.bgdE1E5a;
}

// ── Ephemeris store ─────────────────────────────────────────────────
class EphemerisStore
{
public:
    void processSubframe(const JimmyPaputto::SubframeData& sf)
    {
        if (sf.gnssId != JimmyPaputto::EGnssId::Galileo)
            return;

        if (sf.words.size() < 8)
            return;

        const uint8_t wt = getWordType(sf.words);
        if (wt == 0 || wt > 5) return;  // Only types 1-5 carry ephemeris

        auto& eph = ephemerides_[sf.svId];
        eph.svId = sf.svId;

        switch (wt)
        {
            case 1: decodeWordType1(sf.words, eph); break;
            case 2: decodeWordType2(sf.words, eph); break;
            case 3: decodeWordType3(sf.words, eph); break;
            case 4: decodeWordType4(sf.words, eph); break;
            case 5: decodeWordType5(sf.words, eph); break;
            default: break;
        }
    }

    void processAll(const JimmyPaputto::SubframeBuffer& buffer)
    {
        for (const auto& sf : buffer.subframes)
            processSubframe(sf);
    }

    std::optional<Ephemeris> getEphemeris(uint8_t svId) const
    {
        auto it = ephemerides_.find(svId);
        if (it != ephemerides_.end() && it->second.isComplete())
            return it->second;
        return std::nullopt;
    }

    int completeCount() const
    {
        int count = 0;
        for (const auto& [id, eph] : ephemerides_)
            if (eph.isComplete() && eph.isHealthy())
                ++count;
        return count;
    }

    struct SvEphStatus
    {
        uint8_t svId;
        bool t1, t2, t3, t4, t5;
        bool complete;
        bool healthy;
    };

    std::vector<SvEphStatus> getStatus() const
    {
        std::vector<SvEphStatus> status;
        for (const auto& [id, eph] : ephemerides_)
        {
            status.push_back({
                id,
                eph.hasType1, eph.hasType2, eph.hasType3,
                eph.hasType4, eph.hasType5,
                eph.isComplete(),
                eph.isHealthy()
            });
        }
        std::sort(status.begin(), status.end(),
            [](const auto& a, const auto& b) { return a.svId < b.svId; });
        return status;
    }

private:
    std::unordered_map<uint8_t, Ephemeris> ephemerides_;
};

}  // GalileoEphemeris

#endif  // GALILEO_EPHEMERIS_HPP_

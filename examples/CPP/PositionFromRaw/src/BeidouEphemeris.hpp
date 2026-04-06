/*
 * Jimmy Paputto 2026
 *
 * BeiDou Broadcast Ephemeris Decoder (D1 Navigation Message)
 *
 * Decodes BeiDou D1 navigation message subframes (from UBX-RXM-SFRBX)
 * into broadcast ephemeris parameters for MEO and IGSO satellites.
 *
 * Message structure:
 *   D1 navigation message has the same structure as GPS: 5 subframes
 *   of 10 words × 30 bits each. Subframes 1–3 carry ephemeris and
 *   clock parameters.
 *
 *     Subframe 1: SV health, clock (toc, af0, af1, af2), TGD1, TGD2, AODC
 *     Subframe 2: Ephemeris part 1 (deltaN, Cuc, M0, Cus, e, Crs, sqrtA, toe)
 *     Subframe 3: Ephemeris part 2 (toe, i0, Cic, OmegaDot, Cis, Omega0, omega, iDot)
 *
 * Orbit model:
 *   BeiDou MEO/IGSO satellites use Keplerian orbital elements like GPS.
 *   The propagation algorithm is essentially identical (same Table 20-IV
 *   math) but with CGCS2000/BDS constants:
 *     mu = 3.986004418e14 m³/s²
 *     omega_e = 7.2921150e-5 rad/s
 *
 * Time system:
 *   BeiDou Time (BDT) is 14 seconds behind GPS time:
 *     gpsTow = bdtTow + 14
 *   The receiver's rcvTow is in GPS time, so conversion is needed
 *   before computing SV position from BeiDou ephemeris.
 *
 * Note: GEO satellites use D2 messages with different bit layout.
 *       Only D1 (MEO/IGSO) is supported here.
 *
 * Reference: BDS-SIS-ICD-2.0, Section 5.2
 */

#ifndef BEIDOU_EPHEMERIS_HPP_
#define BEIDOU_EPHEMERIS_HPP_

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include <jimmypaputto/GnssHat.hpp>
#include "GnssMath.hpp"

namespace BeidouEphemeris
{

// ── BeiDou / CGCS2000 constants ─────────────────────────────────────
constexpr double BDS_MU      = 3.986004418e14;    // m³/s²
constexpr double BDS_OMEGA_E = 7.2921150e-5;      // rad/s
constexpr double BDS_F       = -4.442807309e-10;   // Relativistic correction (s/√m)
constexpr double BDS_GPS_OFFSET = 14.0;            // BDT is 14 s behind GPS time

// ── Broadcast ephemeris parameters ──────────────────────────────────
struct Ephemeris
{
    uint8_t  svId = 0;

    // Subframe 1: Clock + health
    uint8_t  satH1 = 0;      // SV health (0 = healthy)
    uint8_t  aodc = 0;       // Age of data, clock
    uint16_t weekNumber = 0;  // BDT week (13-bit)
    double   toc = 0;        // Clock reference time (s, BDT)
    double   tgd1 = 0;       // Group delay B1 (s)
    double   tgd2 = 0;       // Group delay B2 (s)
    double   af0 = 0;        // Clock bias (s)
    double   af1 = 0;        // Clock drift (s/s)
    double   af2 = 0;        // Clock drift rate (s/s²)

    // Subframe 2: Ephemeris part 1
    uint8_t  aode = 0;       // Age of data, ephemeris
    double   deltaN = 0;     // Mean motion difference (rad/s)
    double   cuc = 0;        // Cosine correction, arg of latitude (rad)
    double   m0 = 0;         // Mean anomaly at reference time (rad)
    double   cus = 0;        // Sine correction, arg of latitude (rad)
    double   e = 0;          // Eccentricity
    double   crs = 0;        // Sine correction, orbit radius (m)
    double   sqrtA = 0;      // sqrt(semi-major axis) (m^1/2)
    double   toeMsb = 0;     // toe upper bits (combined in subframe 3)

    // Subframe 3: Ephemeris part 2
    double   toe = 0;        // Ephemeris reference time (s, BDT)
    double   i0 = 0;         // Inclination at reference time (rad)
    double   cic = 0;        // Cosine correction, inclination (rad)
    double   omegaDot = 0;   // Rate of right ascension (rad/s)
    double   cis = 0;        // Sine correction, inclination (rad)
    double   omega0 = 0;     // Longitude of ascending node (rad)
    double   omega = 0;      // Argument of perigee (rad)
    double   iDot = 0;       // Rate of inclination (rad/s)
    double   crc = 0;        // Cosine correction, orbit radius (m)

    // Completeness tracking
    bool hasSubframe1 = false;
    bool hasSubframe2 = false;
    bool hasSubframe3 = false;

    bool isComplete() const
    {
        return hasSubframe1 && hasSubframe2 && hasSubframe3;
    }

    bool isHealthy() const
    {
        return satH1 == 0;
    }
};

// ── Bit extraction for BeiDou D1 navigation words ───────────────────
//
// BeiDou D1 uses the same 30-bit word structure as GPS:
//   24 data bits + 6 BCH parity bits per word, delivered as 32-bit
//   values left-shifted by 2 (same as GPS SFRBX format).
//
// We reuse the same bit-numbering convention: bit 1 = MSB of data,
// bit 24 = LSB of data within each word.
//

inline uint32_t extractBits(uint32_t word, int startBit, int numBits)
{
    int shift = 32 - startBit - numBits + 1;
    if (shift < 0) return 0;
    uint32_t mask = (1u << numBits) - 1;
    return (word >> shift) & mask;
}

inline int32_t signExtend(uint32_t val, int bits)
{
    if (val & (1u << (bits - 1)))
        return static_cast<int32_t>(val | (~0u << bits));
    return static_cast<int32_t>(val);
}

inline uint8_t getSubframeId(const std::vector<uint32_t>& words)
{
    if (words.size() < 2) return 0;
    // Subframe ID is in word 1, bits 16-18 (3 bits) per BDS ICD
    return static_cast<uint8_t>(extractBits(words[0], 16, 3));
}

// ── Subframe decoders (BDS-SIS-ICD-2.0, Tables 5-6 through 5-8) ────

inline bool decodeSubframe1(const std::vector<uint32_t>& words, Ephemeris& eph)
{
    if (words.size() < 10) return false;

    // Word 1: FraID already extracted. SatH1 (bit 19), AODC (bits 20-24)
    eph.satH1 = static_cast<uint8_t>(extractBits(words[0], 19, 1));
    eph.aodc  = static_cast<uint8_t>(extractBits(words[0], 20, 5));

    // Word 2: WN (13 bits, 1-13), toc (bits 14-24 = MSBs, 9 bits)
    eph.weekNumber = static_cast<uint16_t>(extractBits(words[1], 1, 13));
    uint32_t tocMsb = extractBits(words[1], 14, 9);

    // Word 3: toc LSBs (bits 1-8), TGD1 (bits 9-18, signed 10 bits)
    uint32_t tocLsb = extractBits(words[2], 1, 8);
    eph.toc = (static_cast<uint32_t>(tocMsb << 8) | tocLsb) * 8.0;  // Scale factor 2^3
    eph.tgd1 = signExtend(extractBits(words[2], 9, 10), 10) * 1e-10;

    // Word 3 cont: TGD2 (bits 19-24 = MSBs, 6 bits)
    uint32_t tgd2Msb = extractBits(words[2], 19, 6);

    // Word 4: TGD2 LSBs (bits 1-4), alpha0-alpha3 + beta0-beta3 (iono, skip for ephemeris)
    uint32_t tgd2Lsb = extractBits(words[3], 1, 4);
    eph.tgd2 = signExtend((tgd2Msb << 4) | tgd2Lsb, 10) * 1e-10;

    // Word 8: af2 (bits 1-11, signed), af0 MSBs (bits 12-24)
    eph.af2 = signExtend(extractBits(words[7], 1, 11), 11) * std::pow(2, -66);
    uint32_t af0Msb = extractBits(words[7], 12, 13);

    // Word 9: af0 LSBs (bits 1-11), af1 (bits 12-24 = MSBs)
    uint32_t af0Lsb = extractBits(words[8], 1, 11);
    eph.af0 = signExtend((af0Msb << 11) | af0Lsb, 24) * std::pow(2, -33);
    uint32_t af1Msb = extractBits(words[8], 12, 13);

    // Word 10: af1 LSBs (bits 1-9)
    uint32_t af1Lsb = extractBits(words[9], 1, 9);
    eph.af1 = signExtend((af1Msb << 9) | af1Lsb, 22) * std::pow(2, -50);

    eph.hasSubframe1 = true;
    return true;
}

inline bool decodeSubframe2(const std::vector<uint32_t>& words, Ephemeris& eph)
{
    if (words.size() < 10) return false;

    // Word 1: AODE (bits 19-23)
    eph.aode = static_cast<uint8_t>(extractBits(words[0], 19, 5));

    // Word 2: deltaN (bits 1-16, signed), Cuc MSBs (bits 17-24)
    eph.deltaN = signExtend(extractBits(words[1], 1, 16), 16) * std::pow(2, -43) * GnssMath::PI;
    uint32_t cucMsb = extractBits(words[1], 17, 8);

    // Word 3: Cuc LSBs (bits 1-10), M0 MSBs (bits 11-24)
    uint32_t cucLsb = extractBits(words[2], 1, 10);
    eph.cuc = signExtend((cucMsb << 10) | cucLsb, 18) * std::pow(2, -31);
    uint32_t m0Msb = extractBits(words[2], 11, 14);

    // Word 4: M0 LSBs (bits 1-18), Cus MSBs (bits 19-24)
    uint32_t m0Lsb = extractBits(words[3], 1, 18);
    eph.m0 = signExtend((m0Msb << 18) | m0Lsb, 32) * std::pow(2, -31) * GnssMath::PI;
    uint32_t cusMsb = extractBits(words[3], 19, 6);

    // Word 5: Cus LSBs (bits 1-12), e MSBs (bits 13-24)
    uint32_t cusLsb = extractBits(words[4], 1, 12);
    eph.cus = signExtend((cusMsb << 12) | cusLsb, 18) * std::pow(2, -31);
    uint32_t eMsb = extractBits(words[4], 13, 12);

    // Word 6: e LSBs (bits 1-20), sqrtA MSBs (bits 21-24)
    uint32_t eLsb = extractBits(words[5], 1, 20);
    eph.e = ((static_cast<uint64_t>(eMsb) << 20) | eLsb) * std::pow(2, -33);
    uint32_t sqrtAMsb = extractBits(words[5], 21, 4);

    // Word 7: sqrtA continues (bits 1-24)
    uint32_t sqrtAMid = extractBits(words[6], 1, 24);

    // Word 8: sqrtA LSBs (bits 1-4), toe MSBs (bits 5-6)
    uint32_t sqrtALsb = extractBits(words[7], 1, 4);
    eph.sqrtA = ((static_cast<uint64_t>(sqrtAMsb) << 28)
               | (static_cast<uint64_t>(sqrtAMid) << 4) | sqrtALsb) * std::pow(2, -19);

    // toe MSBs stored for combination with subframe 3
    eph.toeMsb = extractBits(words[7], 5, 2) * std::pow(2, 15);

    // Word 9-10: Crs, remaining toe bits handled via toeMsb + sf3
    uint32_t crsMsb = extractBits(words[8], 1, 8);
    uint32_t crsLsb = extractBits(words[9], 1, 10);
    eph.crs = signExtend((crsMsb << 10) | crsLsb, 18) * std::pow(2, -6);

    eph.hasSubframe2 = true;
    return true;
}

inline bool decodeSubframe3(const std::vector<uint32_t>& words, Ephemeris& eph)
{
    if (words.size() < 10) return false;

    // Word 1: toe LSBs (bits 19-24, 5 bits)
    uint32_t toeLsb = extractBits(words[0], 19, 5);
    // Combine with MSBs from subframe 2
    eph.toe = eph.toeMsb + toeLsb * std::pow(2, 3);  // 17-bit toe, scale 2^3 = 8s

    // Word 2: i0 MSBs (bits 1-24)
    uint32_t i0Msb = extractBits(words[1], 1, 24);

    // Word 3: i0 LSBs (bits 1-8), Cic MSBs (bits 9-24)
    uint32_t i0Lsb = extractBits(words[2], 1, 8);
    eph.i0 = signExtend((i0Msb << 8) | i0Lsb, 32) * std::pow(2, -31) * GnssMath::PI;
    uint32_t cicMsb = extractBits(words[2], 9, 16);

    // Word 4: Cic LSBs (bits 1-2), OmegaDot (bits 3-24 = MSBs)
    uint32_t cicLsb = extractBits(words[3], 1, 2);
    eph.cic = signExtend((cicMsb << 2) | cicLsb, 18) * std::pow(2, -31);
    uint32_t omegaDotMsb = extractBits(words[3], 3, 22);

    // Word 5: OmegaDot LSBs (bits 1-2), Cis (bits 3-20), Omega0 MSBs (bits 21-24)
    uint32_t omegaDotLsb = extractBits(words[4], 1, 2);
    eph.omegaDot = signExtend((omegaDotMsb << 2) | omegaDotLsb, 24) * std::pow(2, -43) * GnssMath::PI;
    eph.cis = signExtend(extractBits(words[4], 3, 18), 18) * std::pow(2, -31);
    uint32_t omega0Msb = extractBits(words[4], 21, 4);

    // Word 6: Omega0 continues (bits 1-24)
    uint32_t omega0Mid = extractBits(words[5], 1, 24);

    // Word 7: Omega0 LSBs (bits 1-4), omega MSBs (bits 5-24)
    uint32_t omega0Lsb = extractBits(words[6], 1, 4);
    eph.omega0 = signExtend(
        (static_cast<uint64_t>(omega0Msb) << 28)
        | (static_cast<uint64_t>(omega0Mid) << 4) | omega0Lsb, 32)
        * std::pow(2, -31) * GnssMath::PI;
    uint32_t omegaMsb = extractBits(words[6], 5, 20);

    // Word 8: omega LSBs (bits 1-12), iDot MSBs (bits 13-24)
    uint32_t omegaLsb = extractBits(words[7], 1, 12);
    eph.omega = signExtend((omegaMsb << 12) | omegaLsb, 32) * std::pow(2, -31) * GnssMath::PI;
    uint32_t iDotMsb = extractBits(words[7], 13, 12);

    // Word 9: iDot LSBs (bits 1-2), Crc (bits 3-20)
    uint32_t iDotLsb = extractBits(words[8], 1, 2);
    eph.iDot = signExtend((iDotMsb << 2) | iDotLsb, 14) * std::pow(2, -43) * GnssMath::PI;
    eph.crc = signExtend(extractBits(words[8], 3, 18), 18) * std::pow(2, -6);

    eph.hasSubframe3 = true;
    return true;
}

// ── BDT to GPS time conversion ──────────────────────────────────────
inline double gpsTow2bdtTow(double gpsTow)
{
    return gpsTow - BDS_GPS_OFFSET;
}

// ── Satellite ECEF position (Keplerian, same algorithm as GPS) ──────
inline GnssMath::Ecef computeSvPosition(const Ephemeris& eph, double bdtTow)
{
    const double A = eph.sqrtA * eph.sqrtA;
    const double n0 = std::sqrt(BDS_MU / (A * A * A));
    const double n = n0 + eph.deltaN;

    double tk = bdtTow - eph.toe;
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
                        + (eph.omegaDot - BDS_OMEGA_E) * tk
                        - BDS_OMEGA_E * eph.toe;

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
inline double computeSvClockBias(const Ephemeris& eph, double bdtTow)
{
    double dt = bdtTow - eph.toc;
    if (dt >  302400.0) dt -= 604800.0;
    if (dt < -302400.0) dt += 604800.0;

    const double A = eph.sqrtA * eph.sqrtA;
    const double n0 = std::sqrt(BDS_MU / (A * A * A));
    const double n = n0 + eph.deltaN;

    double tk = bdtTow - eph.toe;
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

    const double dtr = BDS_F * eph.e * eph.sqrtA * std::sin(Ek);

    // For B1 signal, apply TGD1
    return eph.af0 + eph.af1 * dt + eph.af2 * dt * dt + dtr - eph.tgd1;
}

// ── Ephemeris store ─────────────────────────────────────────────────
class EphemerisStore
{
public:
    void processSubframe(const JimmyPaputto::SubframeData& sf)
    {
        if (sf.gnssId != JimmyPaputto::EGnssId::BeiDou)
            return;

        if (sf.numWords < 10 || sf.words.size() < 10)
            return;

        const uint8_t sfId = getSubframeId(sf.words);
        auto& eph = ephemerides_[sf.svId];
        eph.svId = sf.svId;

        switch (sfId)
        {
            case 1: decodeSubframe1(sf.words, eph); break;
            case 2: decodeSubframe2(sf.words, eph); break;
            case 3: decodeSubframe3(sf.words, eph); break;
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
        bool sf1, sf2, sf3;
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
                eph.hasSubframe1, eph.hasSubframe2, eph.hasSubframe3,
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

}  // BeidouEphemeris

#endif  // BEIDOU_EPHEMERIS_HPP_

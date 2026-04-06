/*
 * Jimmy Paputto 2026
 *
 * GPS Broadcast Ephemeris Decoder
 *
 * Decodes GPS navigation message subframes (from UBX-RXM-SFRBX) into
 * broadcast ephemeris parameters, then computes satellite ECEF positions
 * and clock corrections at any given GPS time.
 *
 * Supports:
 *   - GPS L1 C/A subframes 1, 2, 3 (ephemeris + clock)
 *   - Subframe 4 page 18 (Klobuchar ionospheric parameters)
 *   - Satellite position via Keplerian orbit propagation (IS-GPS-200)
 *   - Satellite clock bias (af0, af1, af2 + relativistic correction)
 *   - Group delay correction (TGD)
 *
 * Reference: IS-GPS-200N, Section 20.3.3
 */

#ifndef GPS_EPHEMERIS_HPP_
#define GPS_EPHEMERIS_HPP_

#include <cmath>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include <jimmypaputto/GnssHat.hpp>
#include "GnssMath.hpp"
#include "Ionosphere.hpp"

namespace GpsEphemeris
{

// ── GPS constants (IS-GPS-200) ──────────────────────────────────────
constexpr double GPS_MU      = 3.986005e14;       // Earth gravitational parameter (m^3/s^2)
constexpr double GPS_OMEGA_E = 7.2921151467e-5;   // Earth rotation rate (rad/s)
constexpr double GPS_F       = -4.442807633e-10;   // Relativistic correction constant (s/m^1/2)

// ── Broadcast ephemeris parameters (one per satellite) ──────────────
struct Ephemeris
{
    uint8_t  svId = 0;

    // Subframe 1: Clock + health
    uint16_t weekNumber = 0;  // GPS week (10-bit, mod 1024)
    uint8_t  uraIndex = 0;    // SV accuracy index
    uint8_t  svHealth = 0;    // SV health
    uint8_t  codeOnL2 = 0;
    uint8_t  l2PDataFlag = 0;
    double   tgd = 0;        // Group delay (s)
    uint16_t iodc = 0;       // Issue of data, clock
    double   toc = 0;        // Clock reference time (s)
    double   af2 = 0;        // Clock drift rate (s/s^2)
    double   af1 = 0;        // Clock drift (s/s)
    double   af0 = 0;        // Clock bias (s)

    // Subframe 2: Ephemeris part 1
    uint8_t  iode2 = 0;      // Issue of data, ephemeris (from sf2)
    double   crs = 0;        // Sine correction to orbit radius (m)
    double   deltaN = 0;     // Mean motion difference (rad/s)
    double   m0 = 0;         // Mean anomaly at reference time (rad)
    double   cuc = 0;        // Cosine correction to argument of latitude (rad)
    double   e = 0;          // Eccentricity
    double   cus = 0;        // Sine correction to argument of latitude (rad)
    double   sqrtA = 0;      // Square root of semi-major axis (m^1/2)
    double   toe = 0;        // Ephemeris reference time (s)
    bool     fitIntervalFlag = false;

    // Subframe 3: Ephemeris part 2
    uint8_t  iode3 = 0;      // Issue of data, ephemeris (from sf3)
    double   cic = 0;        // Cosine correction to inclination (rad)
    double   omega0 = 0;     // Longitude of ascending node at weekly epoch (rad)
    double   cis = 0;        // Sine correction to inclination (rad)
    double   i0 = 0;         // Inclination at reference time (rad)
    double   crc = 0;        // Cosine correction to orbit radius (m)
    double   omega = 0;      // Argument of perigee (rad)
    double   omegaDot = 0;   // Rate of right ascension (rad/s)
    double   iDot = 0;       // Rate of inclination (rad/s)

    // Validity tracking
    bool hasSubframe1 = false;
    bool hasSubframe2 = false;
    bool hasSubframe3 = false;

    bool isComplete() const
    {
        return hasSubframe1 && hasSubframe2 && hasSubframe3;
    }

    bool isHealthy() const
    {
        return svHealth == 0;
    }
};

// ── Bit extraction from GPS navigation words ────────────────────────
//
// GPS subframe words are 30-bit, LSB-justified in uint32_t.
// UBX-RXM-SFRBX delivers them as 32-bit words with the 30 data bits
// in the upper 30 bits (bits 31-2), parity in bits 1-0... but the
// u-blox documentation states words are "already inverted" and the
// 24 MSBs of each 32-bit word contain the data bits (bits 31..8),
// with 6 parity bits in bits 7..2 and 2 padding bits.
//
// In practice, u-blox delivers 32-bit words where data bits are
// left-aligned: bits[31:8] = 24 data bits, bits[7:2] = parity.
// But GPS words are 30 bits: 24 data + 6 parity.
// The words from SFRBX are the raw 30-bit words left-shifted by 2.
//

// Extract bits from a single word (bits numbered from MSB=30 down to 1)
inline uint32_t extractBits(uint32_t word, int startBit, int numBits)
{
    // word has 30 data+parity bits in bits [31:2]
    // startBit is 1-based from MSB of the 30-bit word (bit 30 = MSB)
    // So bit 30 → actual bit 31, bit 1 → actual bit 2
    int shift = 32 - startBit - numBits + 1;
    if (shift < 0) return 0;
    uint32_t mask = (1u << numBits) - 1;
    return (word >> shift) & mask;
}

// Extract bits spanning two words
inline uint32_t extractBits2(uint32_t w1, int startBit1, int numBits1,
                             uint32_t w2, int startBit2, int numBits2)
{
    uint32_t upper = extractBits(w1, startBit1, numBits1);
    uint32_t lower = extractBits(w2, startBit2, numBits2);
    return (upper << numBits2) | lower;
}

// Sign-extend a value
inline int32_t signExtend(uint32_t val, int bits)
{
    if (val & (1u << (bits - 1)))
        return static_cast<int32_t>(val | (~0u << bits));
    return static_cast<int32_t>(val);
}

// ── Subframe decoder ────────────────────────────────────────────────
//
// GPS L1 C/A navigation message structure (per IS-GPS-200):
//   Subframe 1 (words 1-10): Week, SV clock, health, accuracy
//   Subframe 2 (words 1-10): Ephemeris parameters (part 1)
//   Subframe 3 (words 1-10): Ephemeris parameters (part 2)
//   Subframes 4-5: Almanac, ionosphere, UTC parameters
//
// Each subframe = 10 words × 30 bits = 300 bits
// Word 1: TLM (preamble), Word 2: HOW (TOW, subframe ID)
//

inline uint8_t getSubframeId(const std::vector<uint32_t>& words)
{
    if (words.size() < 2) return 0;
    // Subframe ID is bits 20-22 of word 2 (3 bits)
    return static_cast<uint8_t>(extractBits(words[1], 9, 3));
}

inline bool decodeSubframe1(const std::vector<uint32_t>& words, Ephemeris& eph)
{
    if (words.size() < 10) return false;

    // Word 3: Week number (bits 1-10), code on L2 (11-12), URA (13-16), SV health (17-22), IODC MSBs (23-24)
    eph.weekNumber = static_cast<uint16_t>(extractBits(words[2], 1, 10));
    eph.codeOnL2   = static_cast<uint8_t>(extractBits(words[2], 11, 2));
    eph.uraIndex   = static_cast<uint8_t>(extractBits(words[2], 13, 4));
    eph.svHealth   = static_cast<uint8_t>(extractBits(words[2], 17, 6));
    uint32_t iodcMsb = extractBits(words[2], 23, 2);

    // Word 4: L2 P data flag (bit 1), reserved
    eph.l2PDataFlag = static_cast<uint8_t>(extractBits(words[3], 1, 1));

    // Word 7: TGD (bits 17-24)
    eph.tgd = signExtend(extractBits(words[6], 17, 8), 8) * std::pow(2, -31);

    // Word 8: IODC LSBs (bits 1-8), toc (bits 9-24)
    uint32_t iodcLsb = extractBits(words[7], 1, 8);
    eph.iodc = static_cast<uint16_t>((iodcMsb << 8) | iodcLsb);
    eph.toc = extractBits(words[7], 9, 16) * std::pow(2, 4);

    // Word 9: af2 (bits 1-8), af1 (bits 9-24)
    eph.af2 = signExtend(extractBits(words[8], 1, 8), 8) * std::pow(2, -55);
    eph.af1 = signExtend(extractBits(words[8], 9, 16), 16) * std::pow(2, -43);

    // Word 10: af0 (bits 1-22)
    eph.af0 = signExtend(extractBits(words[9], 1, 22), 22) * std::pow(2, -31);

    eph.hasSubframe1 = true;
    return true;
}

inline bool decodeSubframe2(const std::vector<uint32_t>& words, Ephemeris& eph)
{
    if (words.size() < 10) return false;

    // Word 3: IODE (bits 1-8), Crs (bits 9-24)
    eph.iode2 = static_cast<uint8_t>(extractBits(words[2], 1, 8));
    eph.crs = signExtend(extractBits(words[2], 9, 16), 16) * std::pow(2, -5);

    // Word 4: Delta n (bits 1-16), M0 MSBs (bits 17-24)
    eph.deltaN = signExtend(extractBits(words[3], 1, 16), 16) * std::pow(2, -43) * GnssMath::PI;
    uint32_t m0Msb = extractBits(words[3], 17, 8);

    // Word 5: M0 LSBs (bits 1-24)
    uint32_t m0Lsb = extractBits(words[4], 1, 24);
    eph.m0 = signExtend((m0Msb << 24) | m0Lsb, 32) * std::pow(2, -31) * GnssMath::PI;

    // Word 6: Cuc (bits 1-16), e MSBs (bits 17-24)
    eph.cuc = signExtend(extractBits(words[5], 1, 16), 16) * std::pow(2, -29);
    uint32_t eMsb = extractBits(words[5], 17, 8);

    // Word 7: e LSBs (bits 1-24)
    uint32_t eLsb = extractBits(words[6], 1, 24);
    eph.e = ((static_cast<uint64_t>(eMsb) << 24) | eLsb) * std::pow(2, -33);

    // Word 8: Cus (bits 1-16), sqrtA MSBs (bits 17-24)
    eph.cus = signExtend(extractBits(words[7], 1, 16), 16) * std::pow(2, -29);
    uint32_t sqrtAMsb = extractBits(words[7], 17, 8);

    // Word 9: sqrtA LSBs (bits 1-24)
    uint32_t sqrtALsb = extractBits(words[8], 1, 24);
    eph.sqrtA = ((static_cast<uint64_t>(sqrtAMsb) << 24) | sqrtALsb) * std::pow(2, -19);

    // Word 10: toe (bits 1-16), fit interval (bit 17)
    eph.toe = extractBits(words[9], 1, 16) * std::pow(2, 4);
    eph.fitIntervalFlag = extractBits(words[9], 17, 1) != 0;

    eph.hasSubframe2 = true;
    return true;
}

inline bool decodeSubframe3(const std::vector<uint32_t>& words, Ephemeris& eph)
{
    if (words.size() < 10) return false;

    // Word 3: Cic (bits 1-16), Omega0 MSBs (bits 17-24)
    eph.cic = signExtend(extractBits(words[2], 1, 16), 16) * std::pow(2, -29);
    uint32_t omega0Msb = extractBits(words[2], 17, 8);

    // Word 4: Omega0 LSBs (bits 1-24)
    uint32_t omega0Lsb = extractBits(words[3], 1, 24);
    eph.omega0 = signExtend((omega0Msb << 24) | omega0Lsb, 32) * std::pow(2, -31) * GnssMath::PI;

    // Word 5: Cis (bits 1-16), i0 MSBs (bits 17-24)
    eph.cis = signExtend(extractBits(words[4], 1, 16), 16) * std::pow(2, -29);
    uint32_t i0Msb = extractBits(words[4], 17, 8);

    // Word 6: i0 LSBs (bits 1-24)
    uint32_t i0Lsb = extractBits(words[5], 1, 24);
    eph.i0 = signExtend((i0Msb << 24) | i0Lsb, 32) * std::pow(2, -31) * GnssMath::PI;

    // Word 7: Crc (bits 1-16), omega MSBs (bits 17-24)
    eph.crc = signExtend(extractBits(words[6], 1, 16), 16) * std::pow(2, -5);
    uint32_t omegaMsb = extractBits(words[6], 17, 8);

    // Word 8: omega LSBs (bits 1-24)
    uint32_t omegaLsb = extractBits(words[7], 1, 24);
    eph.omega = signExtend((omegaMsb << 24) | omegaLsb, 32) * std::pow(2, -31) * GnssMath::PI;

    // Word 9: OmegaDot (bits 1-24)
    eph.omegaDot = signExtend(extractBits(words[8], 1, 24), 24) * std::pow(2, -43) * GnssMath::PI;

    // Word 10: IODE (bits 1-8), iDot (bits 9-22)
    eph.iode3 = static_cast<uint8_t>(extractBits(words[9], 1, 8));
    eph.iDot = signExtend(extractBits(words[9], 9, 14), 14) * std::pow(2, -43) * GnssMath::PI;

    eph.hasSubframe3 = true;
    return true;
}

// ── Satellite ECEF position from ephemeris (IS-GPS-200 Table 20-IV) ─
inline GnssMath::Ecef computeSvPosition(const Ephemeris& eph, double gpsTow)
{
    const double A = eph.sqrtA * eph.sqrtA;       // Semi-major axis
    const double n0 = std::sqrt(GPS_MU / (A * A * A));  // Computed mean motion
    const double n = n0 + eph.deltaN;              // Corrected mean motion

    // Time from ephemeris reference epoch
    double tk = gpsTow - eph.toe;
    if (tk >  302400.0) tk -= 604800.0;
    if (tk < -302400.0) tk += 604800.0;

    // Mean anomaly
    const double Mk = eph.m0 + n * tk;

    // Eccentric anomaly (Kepler's equation, iterative)
    double Ek = Mk;
    for (int i = 0; i < 15; ++i)
    {
        const double Ek_new = Mk + eph.e * std::sin(Ek);
        if (std::fabs(Ek_new - Ek) < 1e-15) break;
        Ek = Ek_new;
    }

    // True anomaly
    const double sinEk = std::sin(Ek);
    const double cosEk = std::cos(Ek);
    const double vk = std::atan2(
        std::sqrt(1.0 - eph.e * eph.e) * sinEk,
        cosEk - eph.e
    );

    // Argument of latitude
    const double phik = vk + eph.omega;
    const double sin2phi = std::sin(2.0 * phik);
    const double cos2phi = std::cos(2.0 * phik);

    // Second harmonic corrections
    const double deltaUk = eph.cus * sin2phi + eph.cuc * cos2phi;
    const double deltaRk = eph.crs * sin2phi + eph.crc * cos2phi;
    const double deltaIk = eph.cis * sin2phi + eph.cic * cos2phi;

    // Corrected argument of latitude, radius, inclination
    const double uk = phik + deltaUk;
    const double rk = A * (1.0 - eph.e * cosEk) + deltaRk;
    const double ik = eph.i0 + deltaIk + eph.iDot * tk;

    // Positions in orbital plane
    const double xkp = rk * std::cos(uk);
    const double ykp = rk * std::sin(uk);

    // Corrected longitude of ascending node
    const double omegak = eph.omega0
                        + (eph.omegaDot - GPS_OMEGA_E) * tk
                        - GPS_OMEGA_E * eph.toe;

    const double sinOmk = std::sin(omegak);
    const double cosOmk = std::cos(omegak);
    const double sinIk  = std::sin(ik);
    const double cosIk  = std::cos(ik);

    // ECEF coordinates
    return {
        xkp * cosOmk - ykp * cosIk * sinOmk,
        xkp * sinOmk + ykp * cosIk * cosOmk,
        ykp * sinIk
    };
}

// ── Satellite clock correction (IS-GPS-200 Section 20.3.3.3.3.1) ───
inline double computeSvClockBias(const Ephemeris& eph, double gpsTow)
{
    double dt = gpsTow - eph.toc;
    if (dt >  302400.0) dt -= 604800.0;
    if (dt < -302400.0) dt += 604800.0;

    // Relativistic correction
    const double A = eph.sqrtA * eph.sqrtA;
    const double n0 = std::sqrt(GPS_MU / (A * A * A));
    const double n = n0 + eph.deltaN;

    double tk = gpsTow - eph.toe;
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

    const double dtr = GPS_F * eph.e * eph.sqrtA * std::sin(Ek);

    // Clock bias = af0 + af1*dt + af2*dt^2 + relativistic - TGD
    return eph.af0 + eph.af1 * dt + eph.af2 * dt * dt + dtr - eph.tgd;
}

// ── Ephemeris store: accumulates subframes per satellite ────────────
class EphemerisStore
{
public:
    // Process a subframe received from UBX-RXM-SFRBX
    void processSubframe(const JimmyPaputto::SubframeData& sf)
    {
        // Only GPS for now
        if (sf.gnssId != JimmyPaputto::EGnssId::GPS)
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

    // Process subframe 4 page 18 for Klobuchar ionospheric parameters.
    // This is called for any GPS SFRBX regardless of svId — all SVs
    // broadcast the same ionospheric parameters.
    void processIonoSubframe(const JimmyPaputto::SubframeData& sf)
    {
        if (sf.gnssId != JimmyPaputto::EGnssId::GPS)
            return;
        if (sf.numWords < 10 || sf.words.size() < 10)
            return;

        const uint8_t sfId = getSubframeId(sf.words);
        if (sfId != 4) return;

        // Subframe 4 page ID is in word 3, bits 1-6 (data ID + page ID)
        // Page 18 has SV ID = 56 in bits 1-6 of word 3
        const uint8_t svIdPage = static_cast<uint8_t>(extractBits(sf.words[2], 1, 6));
        if (svIdPage != 56) return;  // Not page 18

        // Word 3 bits 7-14: alpha0 (8 bits, signed, scale 2^-30)
        // Word 3 bits 15-22: alpha1 (8 bits, signed, scale 2^-27/semi-circle)
        // Word 4 bits 1-8: alpha2 (8 bits, signed, scale 2^-24/semi-circle²)
        // Word 4 bits 9-16: alpha3 (8 bits, signed, scale 2^-24/semi-circle³)
        ionoParams_.alpha[0] = signExtend(extractBits(sf.words[2], 7, 8), 8) * std::pow(2, -30);
        ionoParams_.alpha[1] = signExtend(extractBits(sf.words[2], 15, 8), 8) * std::pow(2, -27);
        ionoParams_.alpha[2] = signExtend(extractBits(sf.words[3], 1, 8), 8) * std::pow(2, -24);
        ionoParams_.alpha[3] = signExtend(extractBits(sf.words[3], 9, 8), 8) * std::pow(2, -24);

        // Word 4 bits 17-24: beta0 (8 bits, signed, scale 2^11)
        // Word 5 bits 1-8: beta1 (8 bits, signed, scale 2^14/semi-circle)
        // Word 5 bits 9-16: beta2 (8 bits, signed, scale 2^16/semi-circle²)
        // Word 5 bits 17-24: beta3 (8 bits, signed, scale 2^16/semi-circle³)
        ionoParams_.beta[0] = signExtend(extractBits(sf.words[3], 17, 8), 8) * std::pow(2, 11);
        ionoParams_.beta[1] = signExtend(extractBits(sf.words[4], 1, 8), 8) * std::pow(2, 14);
        ionoParams_.beta[2] = signExtend(extractBits(sf.words[4], 9, 8), 8) * std::pow(2, 16);
        ionoParams_.beta[3] = signExtend(extractBits(sf.words[4], 17, 8), 8) * std::pow(2, 16);

        hasIonoParams_ = true;
    }

    // Process all new subframes from navigation buffer
    void processAll(const JimmyPaputto::SubframeBuffer& buffer)
    {
        for (const auto& sf : buffer.subframes)
        {
            processSubframe(sf);
            processIonoSubframe(sf);
        }
    }

    // Get Klobuchar ionospheric parameters (if decoded from subframe 4)
    std::optional<Ionosphere::KlobucharParams> getKlobucharParams() const
    {
        if (hasIonoParams_)
            return ionoParams_;
        return std::nullopt;
    }

    // Get complete ephemeris for a satellite (if available)
    std::optional<Ephemeris> getEphemeris(uint8_t svId) const
    {
        auto it = ephemerides_.find(svId);
        if (it != ephemerides_.end() && it->second.isComplete())
            return it->second;
        return std::nullopt;
    }

    // Get count of complete ephemerides
    int completeCount() const
    {
        int count = 0;
        for (const auto& [id, eph] : ephemerides_)
            if (eph.isComplete() && eph.isHealthy())
                ++count;
        return count;
    }

    // Get status string for each tracked satellite
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
                eph.hasSubframe1,
                eph.hasSubframe2,
                eph.hasSubframe3,
                eph.isComplete(),
                eph.isHealthy()
            });
        }
        // Sort by SV ID
        std::sort(status.begin(), status.end(),
            [](const auto& a, const auto& b) { return a.svId < b.svId; });
        return status;
    }

private:
    std::unordered_map<uint8_t, Ephemeris> ephemerides_;
    Ionosphere::KlobucharParams ionoParams_;
    bool hasIonoParams_ = false;
};

}  // GpsEphemeris

#endif  // GPS_EPHEMERIS_HPP_

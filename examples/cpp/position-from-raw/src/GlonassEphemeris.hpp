/*
 * Jimmy Paputto 2026
 *
 * GLONASS Broadcast Ephemeris Decoder
 *
 * Decodes GLONASS navigation message strings (from UBX-RXM-SFRBX)
 * into broadcast ephemeris parameters, then computes satellite ECEF
 * positions via numerical integration.
 *
 * GLONASS is fundamentally different from GPS/Galileo/BeiDou:
 *
 *   1. Ephemeris representation: GLONASS transmits Earth-centred
 *      state vectors (position, velocity, luni-solar acceleration)
 *      at a reference epoch, rather than Keplerian orbital elements.
 *
 *   2. Position computation: requires 4th-order Runge-Kutta numerical
 *      integration of the equations of motion, including J2 zonal
 *      harmonic gravity and the broadcast luni-solar acceleration.
 *
 *   3. Coordinate frame: PZ-90.11 (equivalent to WGS-84 at cm level
 *      for SPP purposes — no explicit transformation needed).
 *
 *   4. Time system: GLONASS time (GLONASST) = UTC(SU) + 3h.
 *      Conversion to GPS time requires the leap second count and
 *      the receiver-broadcast tau_c offset.
 *
 *   5. Signal structure: FDMA (frequency-division). L1 frequency
 *      depends on the satellite's frequency slot k:
 *        f_L1 = 1602.0 + k × 0.5625 MHz
 *      The slot number is the freqId in RawObservation.
 *
 * Navigation message structure:
 *   Each GLONASS string = 100 bits (85 data + 15 parity).
 *   UBX-RXM-SFRBX delivers 4 × 32-bit words per string.
 *   Strings 1–4 of each superframe carry immediate ephemeris:
 *     String 1: time mark tk, velocity Xdot, acceleration Xddot, position X
 *     String 2: health Bn, velocity Ydot, acceleration Yddot, position Y
 *     String 3: health P, gamma_n, velocity Zdot, acceleration Zddot, position Z
 *     String 4: tau_n, delta_tau_n, En, P4, Ft, Nt, M
 *
 * Reference: GLONASS ICD Edition 5.1, Section 4.3
 */

#ifndef GLONASS_EPHEMERIS_HPP_
#define GLONASS_EPHEMERIS_HPP_

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include <jimmypaputto/GnssHat.hpp>
#include "GnssMath.hpp"

namespace GlonassEphemeris
{

// ── PZ-90 / GLONASS constants ───────────────────────────────────────
constexpr double GLO_MU      = 3.9860044e14;      // Earth gravitational parameter (m³/s²)
constexpr double GLO_AE      = 6378136.0;          // Earth equatorial radius (m)
constexpr double GLO_J2      = 1.08262575e-3;      // Second zonal harmonic
constexpr double GLO_OMEGA_E = 7.2921151467e-5;    // Earth rotation rate (rad/s)

// ── Broadcast ephemeris (state vector at reference epoch) ───────────
struct Ephemeris
{
    uint8_t svId = 0;
    int8_t  freqSlot = 0;   // GLONASS frequency slot k (-7..+6)

    // String 1: position X, velocity Xdot, acceleration Xddot
    double x = 0;            // km
    double xDot = 0;         // km/s
    double xDotDot = 0;      // km/s² (luni-solar acceleration)

    // String 2: position Y, velocity Ydot, acceleration Yddot, health
    double y = 0;
    double yDot = 0;
    double yDotDot = 0;
    uint8_t Bn = 0;          // Health flag (0 = healthy)

    // String 3: position Z, velocity Zdot, acceleration Zddot, gamma_n
    double z = 0;
    double zDot = 0;
    double zDotDot = 0;
    double gammaN = 0;       // Relative frequency offset (dimensionless)
    uint8_t P = 0;           // Accuracy flag

    // String 4: clock, time, calendar
    double tauN = 0;         // SV clock bias (s) — note: sign convention is -tauN
    double deltaTauN = 0;    // Inter-frequency bias (s)
    uint8_t En = 0;          // Age of ephemeris (days)
    uint8_t Ft = 0;          // User range accuracy index
    uint16_t Nt = 0;         // Calendar day number (1–1461)
    uint8_t M = 0;           // GLONASS-M satellite type

    // Reference epoch: tb (seconds of day, Moscow time = UTC+3)
    double tb = 0;           // 15-minute intervals, in seconds of day

    // Time mark from string 1 (for timestamp)
    uint8_t  tkHours = 0;
    uint8_t  tkMinutes = 0;
    uint8_t  tkSeconds = 0;  // 30-second intervals

    // Completeness tracking
    bool hasString1 = false;
    bool hasString2 = false;
    bool hasString3 = false;
    bool hasString4 = false;

    bool isComplete() const
    {
        return hasString1 && hasString2 && hasString3 && hasString4;
    }

    bool isHealthy() const
    {
        return Bn == 0;
    }
};

// ── Bit extraction for GLONASS strings ──────────────────────────────
//
// GLONASS strings are 100 bits. u-blox delivers them as 4 × 32-bit
// words (128 bits, with padding). The 85 data bits are MSB-first,
// followed by a Hamming check.
//
// Bit numbering (ICD convention): bit 85 = MSB, bit 1 = LSB.
// In SFRBX words: word[0] bits 31–0 = string bits 85–54 (approx.).
//
// We use 0-based absolute bit position from MSB of word[0].
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
    // GLONASS uses sign-magnitude encoding
    uint32_t val = getBits(words, bitOffset, numBits);
    if (val & (1u << (numBits - 1)))
    {
        // Negative: sign-magnitude → two's complement
        uint32_t magnitude = val & ((1u << (numBits - 1)) - 1);
        return -static_cast<int32_t>(magnitude);
    }
    return static_cast<int32_t>(val);
}

// ── String identification ───────────────────────────────────────────
//
// The string number is in bits 1-4 of the SFRBX data (after the
// idle chip). In the SFRBX word layout, the string begins at bit 0.
// Bits 0-3 of the payload identify the string (m = 1..15).
//
inline uint8_t getStringNumber(const std::vector<uint32_t>& words)
{
    if (words.empty()) return 0;
    return static_cast<uint8_t>(getBits(words, 4, 4));
}

// ── String decoders ─────────────────────────────────────────────────
//
// GLONASS ICD Edition 5.1, Table 4.5.
// Bit positions are given from the ICD (1-based from MSB of string).
// We convert to 0-based offsets from MSB of word[0].
//
// The first 4 bits are the idle chip + string frame start, then:
//   Bits 5-8: string number (4 bits)
//
// String 1: bits 9-13 = unused, 14-18 = tk hours (5), 19 = tk min MSB flag,
//           20-25 = tk 30-second mark, etc.
//

inline bool decodeString1(const std::vector<uint32_t>& words, Ephemeris& eph)
{
    if (words.size() < 4) return false;

    // tk: hours (5 bits at bit 13), minutes (6 bits at bit 18), 30s (1 bit at bit 24)
    eph.tkHours   = static_cast<uint8_t>(getBits(words, 12, 5));
    eph.tkMinutes = static_cast<uint8_t>(getBits(words, 17, 6));
    eph.tkSeconds = static_cast<uint8_t>(getBits(words, 23, 1)) * 30;

    // Xdot: 24 bits, sign-magnitude, scale 2^-20 km/s (bit 25)
    eph.xDot = getSignedBits(words, 24, 24) * std::pow(2, -20);

    // Xddot: 5 bits, sign-magnitude, scale 2^-30 km/s² (bit 49)
    eph.xDotDot = getSignedBits(words, 48, 5) * std::pow(2, -30);

    // X: 27 bits, sign-magnitude, scale 2^-11 km (bit 54)
    eph.x = getSignedBits(words, 53, 27) * std::pow(2, -11);

    eph.hasString1 = true;
    return true;
}

inline bool decodeString2(const std::vector<uint32_t>& words, Ephemeris& eph)
{
    if (words.size() < 4) return false;

    // Bn: 3 bits at bit 9 (0-based: 8)
    eph.Bn = static_cast<uint8_t>(getBits(words, 8, 3));

    // tb: 7 bits at bit 15 (0-based: 14), units = 15 minutes
    uint8_t tbRaw = static_cast<uint8_t>(getBits(words, 14, 7));
    eph.tb = tbRaw * 900.0;  // Convert to seconds of day (Moscow time)

    // Ydot: 24 bits, sign-magnitude, scale 2^-20 km/s (bit 22)
    eph.yDot = getSignedBits(words, 21, 24) * std::pow(2, -20);

    // Yddot: 5 bits, sign-magnitude, scale 2^-30 km/s² (bit 46)
    eph.yDotDot = getSignedBits(words, 45, 5) * std::pow(2, -30);

    // Y: 27 bits, sign-magnitude, scale 2^-11 km (bit 51)
    eph.y = getSignedBits(words, 50, 27) * std::pow(2, -11);

    eph.hasString2 = true;
    return true;
}

inline bool decodeString3(const std::vector<uint32_t>& words, Ephemeris& eph)
{
    if (words.size() < 4) return false;

    // P: 2 bits at bit 9 (0-based: 8)
    eph.P = static_cast<uint8_t>(getBits(words, 8, 2));

    // gamma_n: 11 bits, sign-magnitude, scale 2^-40 (bit 11)
    eph.gammaN = getSignedBits(words, 10, 11) * std::pow(2, -40);

    // Zdot: 24 bits, sign-magnitude, scale 2^-20 km/s (bit 22)
    eph.zDot = getSignedBits(words, 21, 24) * std::pow(2, -20);

    // Zddot: 5 bits, sign-magnitude, scale 2^-30 km/s² (bit 46)
    eph.zDotDot = getSignedBits(words, 45, 5) * std::pow(2, -30);

    // Z: 27 bits, sign-magnitude, scale 2^-11 km (bit 51)
    eph.z = getSignedBits(words, 50, 27) * std::pow(2, -11);

    eph.hasString3 = true;
    return true;
}

inline bool decodeString4(const std::vector<uint32_t>& words, Ephemeris& eph)
{
    if (words.size() < 4) return false;

    // tau_n: 22 bits, sign-magnitude, scale 2^-30 seconds (bit 9)
    eph.tauN = getSignedBits(words, 8, 22) * std::pow(2, -30);

    // delta_tau_n: 5 bits, sign-magnitude, scale 2^-30 seconds (bit 31)
    eph.deltaTauN = getSignedBits(words, 30, 5) * std::pow(2, -30);

    // En: 5 bits at bit 36
    eph.En = static_cast<uint8_t>(getBits(words, 35, 5));

    // Ft: 4 bits at bit 50
    eph.Ft = static_cast<uint8_t>(getBits(words, 49, 4));

    // Nt: 11 bits at bit 59
    eph.Nt = static_cast<uint16_t>(getBits(words, 58, 11));

    // M: 2 bits at bit 70
    eph.M = static_cast<uint8_t>(getBits(words, 69, 2));

    eph.hasString4 = true;
    return true;
}

// ── Equations of motion for GLONASS orbit propagation ───────────────
//
// The force model includes:
//   1. Central gravity with J2 zonal harmonic (PZ-90)
//   2. Luni-solar perturbation (from broadcast acceleration)
//
// State vector: [x, y, z, vx, vy, vz] in km and km/s.
// Accelerations in km/s².
//

struct State6
{
    double x, y, z, vx, vy, vz;
};

inline State6 derivatives(const State6& s,
                          double xddot_ls, double yddot_ls, double zddot_ls)
{
    const double r2 = s.x * s.x + s.y * s.y + s.z * s.z;
    const double r = std::sqrt(r2);
    const double r3 = r2 * r;
    const double r5 = r3 * r2;

    // Convert constants to km units
    constexpr double mu = GLO_MU * 1e-9;     // km³/s²
    constexpr double ae = GLO_AE * 1e-3;     // km
    constexpr double ae2 = ae * ae;
    constexpr double omegaE = GLO_OMEGA_E;

    // J2 perturbation factor
    const double z2_r2 = (s.z * s.z) / r2;
    const double j2factor = 1.5 * GLO_J2 * mu * ae2;

    // Accelerations (km/s²)
    const double ax = -mu * s.x / r3
                    + j2factor * s.x / r5 * (5.0 * z2_r2 - 1.0)
                    + xddot_ls
                    + omegaE * omegaE * s.x
                    + 2.0 * omegaE * s.vy;

    const double ay = -mu * s.y / r3
                    + j2factor * s.y / r5 * (5.0 * z2_r2 - 1.0)
                    + yddot_ls
                    + omegaE * omegaE * s.y
                    - 2.0 * omegaE * s.vx;

    const double az = -mu * s.z / r3
                    + j2factor * s.z / r5 * (5.0 * z2_r2 - 3.0)
                    + zddot_ls;

    return {s.vx, s.vy, s.vz, ax, ay, az};
}

// ── 4th-order Runge-Kutta integrator ────────────────────────────────
//
// Propagates the GLONASS state vector from reference epoch tb to
// target time t using RK4 with adaptive step sizing (max 60 s).
//
inline State6 rk4Step(const State6& s, double h,
                      double xddot_ls, double yddot_ls, double zddot_ls)
{
    auto addState = [](const State6& a, const State6& b, double scale) -> State6 {
        return {
            a.x  + b.x  * scale,  a.y  + b.y  * scale,  a.z  + b.z  * scale,
            a.vx + b.vx * scale, a.vy + b.vy * scale, a.vz + b.vz * scale
        };
    };

    const State6 k1 = derivatives(s, xddot_ls, yddot_ls, zddot_ls);
    const State6 s2 = addState(s, k1, h * 0.5);
    const State6 k2 = derivatives(s2, xddot_ls, yddot_ls, zddot_ls);
    const State6 s3 = addState(s, k2, h * 0.5);
    const State6 k3 = derivatives(s3, xddot_ls, yddot_ls, zddot_ls);
    const State6 s4 = addState(s, k3, h);
    const State6 k4 = derivatives(s4, xddot_ls, yddot_ls, zddot_ls);

    return {
        s.x  + h / 6.0 * (k1.x  + 2.0 * k2.x  + 2.0 * k3.x  + k4.x),
        s.y  + h / 6.0 * (k1.y  + 2.0 * k2.y  + 2.0 * k3.y  + k4.y),
        s.z  + h / 6.0 * (k1.z  + 2.0 * k2.z  + 2.0 * k3.z  + k4.z),
        s.vx + h / 6.0 * (k1.vx + 2.0 * k2.vx + 2.0 * k3.vx + k4.vx),
        s.vy + h / 6.0 * (k1.vy + 2.0 * k2.vy + 2.0 * k3.vy + k4.vy),
        s.vz + h / 6.0 * (k1.vz + 2.0 * k2.vz + 2.0 * k3.vz + k4.vz)
    };
}

// ── Satellite ECEF position via numerical integration ───────────────
//
// Integrates the GLONASS equations of motion from the broadcast
// reference epoch (tb) to the target time (tTarget), both in
// seconds-of-day (Moscow time = UTC+3).
//
// Returns position in meters (converted from km).
//
inline GnssMath::Ecef computeSvPosition(const Ephemeris& eph, double tTargetSod)
{
    // Time delta from reference epoch (seconds)
    double dt = tTargetSod - eph.tb;

    // Wrap around midnight
    if (dt >  43200.0) dt -= 86400.0;
    if (dt < -43200.0) dt += 86400.0;

    // Initial state in km and km/s
    State6 state = {eph.x, eph.y, eph.z, eph.xDot, eph.yDot, eph.zDot};

    // Luni-solar accelerations (km/s²) — constant over interval
    const double xdd = eph.xDotDot;
    const double ydd = eph.yDotDot;
    const double zdd = eph.zDotDot;

    // Integration step: max 60 seconds, sign follows dt
    constexpr double MAX_STEP = 60.0;
    const double sign = (dt >= 0) ? 1.0 : -1.0;
    double remaining = std::fabs(dt);

    while (remaining > 1e-6)
    {
        const double h = sign * std::min(remaining, MAX_STEP);
        state = rk4Step(state, h, xdd, ydd, zdd);
        remaining -= std::fabs(h);
    }

    // Convert km to meters
    return {state.x * 1000.0, state.y * 1000.0, state.z * 1000.0};
}

// ── Satellite clock correction ──────────────────────────────────────
//
// GLONASS clock: delta_t_sv = -tau_n + gamma_n × (t - tb)
// Note the negative sign on tau_n per ICD convention.
//
inline double computeSvClockBias(const Ephemeris& eph, double tTargetSod)
{
    double dt = tTargetSod - eph.tb;
    if (dt >  43200.0) dt -= 86400.0;
    if (dt < -43200.0) dt += 86400.0;

    return -eph.tauN + eph.gammaN * dt;
}

// ── Time conversion helpers ─────────────────────────────────────────
//
// GLONASS time = UTC(SU) + 3 hours (Moscow decree time).
// GPS time = UTC + leap seconds.
//
// To convert GPS TOW to GLONASS seconds-of-day:
//   1. gpsTow → UTC seconds of week: utcSow = gpsTow - leapSeconds
//   2. UTC seconds of day: utcSod = utcSow mod 86400
//   3. Moscow time: mskSod = utcSod + 3*3600 (mod 86400)
//
inline double gpsTow2GloSod(double gpsTow, int leapSeconds)
{
    double utcSow = gpsTow - leapSeconds;
    double utcSod = std::fmod(utcSow, 86400.0);
    if (utcSod < 0) utcSod += 86400.0;

    double mskSod = utcSod + 3.0 * 3600.0;
    if (mskSod >= 86400.0) mskSod -= 86400.0;

    return mskSod;
}

// ── Ephemeris store ─────────────────────────────────────────────────
class EphemerisStore
{
public:
    void processSubframe(const JimmyPaputto::SubframeData& sf)
    {
        if (sf.gnssId != JimmyPaputto::EGnssId::GLONASS)
            return;

        if (sf.words.size() < 4)
            return;

        const uint8_t strNum = getStringNumber(sf.words);
        if (strNum < 1 || strNum > 4) return;

        auto& eph = ephemerides_[sf.svId];
        eph.svId = sf.svId;

        // Store frequency slot from SFRBX metadata
        // freqId in SFRBX is the slot + 7 (range 0..13 for slots -7..+6)
        eph.freqSlot = static_cast<int8_t>(sf.freqId) - 7;

        switch (strNum)
        {
            case 1: decodeString1(sf.words, eph); break;
            case 2: decodeString2(sf.words, eph); break;
            case 3: decodeString3(sf.words, eph); break;
            case 4: decodeString4(sf.words, eph); break;
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
        int8_t  freqSlot;
        bool s1, s2, s3, s4;
        bool complete;
        bool healthy;
    };

    std::vector<SvEphStatus> getStatus() const
    {
        std::vector<SvEphStatus> status;
        for (const auto& [id, eph] : ephemerides_)
        {
            status.push_back({
                id, eph.freqSlot,
                eph.hasString1, eph.hasString2,
                eph.hasString3, eph.hasString4,
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

}  // GlonassEphemeris

#endif  // GLONASS_EPHEMERIS_HPP_

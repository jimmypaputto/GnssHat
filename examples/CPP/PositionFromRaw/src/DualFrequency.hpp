/*
 * Jimmy Paputto 2026
 *
 * Dual-frequency ionosphere-free pseudorange combination
 *
 * When a receiver tracks a satellite on two frequencies (e.g. L1 + L5),
 * the first-order ionospheric delay can be eliminated entirely via a
 * linear combination of pseudoranges:
 *
 *     P_IF = (f1² · P1 - f2² · P2) / (f1² - f2²)
 *
 * This is the "iono-free" combination. It removes the dominant 1/f²
 * ionospheric error (~5–30 m) at the cost of amplified noise (~2.6×
 * for L1/L5).
 *
 * On single-frequency receivers (NEO-M9N), no L5/E5a/B2a observations
 * exist, so pairObservations() returns all L1 observations as unpaired
 * and the solver automatically falls back to the Klobuchar model.
 * On multi-frequency receivers (ZED-F9P, NEO-F10N), dual-freq pairs
 * are formed and the iono-free pseudorange is used instead.
 *
 * Supported frequency pairs:
 *   GPS:     L1CA (1575.42 MHz)  +  L5I (1176.45 MHz)
 *   Galileo: E1B  (1575.42 MHz)  +  E5aI (1176.45 MHz)
 *   BeiDou:  B1I  (1561.098 MHz) +  B2aI (1176.45 MHz)
 *   GLONASS: L1OF (FDMA)         +  L2OF (FDMA)
 *
 * Note: GLONASS uses FDMA, so L1/L2 frequencies depend on the
 * satellite's frequency slot k:
 *   f_L1 = 1602.0 + k × 0.5625 MHz
 *   f_L2 = 1246.0 + k × 0.4375 MHz
 */

#ifndef DUAL_FREQUENCY_HPP_
#define DUAL_FREQUENCY_HPP_

#include <cmath>
#include <optional>
#include <unordered_map>
#include <vector>

#include <jimmypaputto/GnssHat.hpp>

namespace DualFrequency
{

// ── Carrier frequencies (Hz) ────────────────────────────────────────
constexpr double GPS_L1  = 1575.42e6;
constexpr double GPS_L5  = 1176.45e6;
constexpr double GAL_E1  = 1575.42e6;
constexpr double GAL_E5a = 1176.45e6;
constexpr double BDS_B1I = 1561.098e6;
constexpr double BDS_B2a = 1176.45e6;
constexpr double GLO_L1_BASE = 1602.0e6;
constexpr double GLO_L1_STEP = 0.5625e6;
constexpr double GLO_L2_BASE = 1246.0e6;
constexpr double GLO_L2_STEP = 0.4375e6;

// ── Iono-free combination ───────────────────────────────────────────
//
// P_IF = (f1² · P1 - f2² · P2) / (f1² - f2²)
//
// The noise amplification factor for P1 is: f1²/(f1²-f2²) ≈ 2.26
// for L1/L5, which is acceptable because the eliminated ionospheric
// error (5–30 m) far exceeds the added noise (~0.5 m).
//
inline double ionoFreeRange(double f1, double f2, double pr1, double pr2)
{
    const double f1sq = f1 * f1;
    const double f2sq = f2 * f2;
    return (f1sq * pr1 - f2sq * pr2) / (f1sq - f2sq);
}

// ── Signal classification ───────────────────────────────────────────

inline bool isL1Signal(JimmyPaputto::EGnssId gnssId, uint8_t sigId)
{
    using namespace JimmyPaputto;
    switch (gnssId)
    {
        case EGnssId::GPS:     return sigId == 0;                // L1CA
        case EGnssId::Galileo: return sigId <= 1;                // E1C, E1B
        case EGnssId::BeiDou:  return sigId <= 1;                // B1ID1, B1ID2
        case EGnssId::GLONASS: return sigId == 0;                // L1OF
        default: return false;
    }
}

inline bool isL5Signal(JimmyPaputto::EGnssId gnssId, uint8_t sigId)
{
    using namespace JimmyPaputto;
    switch (gnssId)
    {
        case EGnssId::GPS:     return sigId == 6 || sigId == 7;  // L5I, L5Q
        case EGnssId::Galileo: return sigId == 3 || sigId == 4;  // E5aI, E5aQ
        case EGnssId::BeiDou:  return sigId == 7 || sigId == 8;  // B2ap, B2ad
        case EGnssId::GLONASS: return sigId == 2;                // L2OF
        default: return false;
    }
}

// ── Frequency lookup ────────────────────────────────────────────────
//
// Returns the carrier frequency in Hz for a given signal.
// For GLONASS FDMA, freqId encodes the frequency slot (slot = freqId - 7).
//
inline double carrierFrequency(JimmyPaputto::EGnssId gnssId,
                               uint8_t sigId, uint8_t freqId)
{
    using namespace JimmyPaputto;
    switch (gnssId)
    {
        case EGnssId::GPS:
            if (sigId == 0)                return GPS_L1;   // L1CA
            if (sigId == 6 || sigId == 7)  return GPS_L5;   // L5I/Q
            break;

        case EGnssId::Galileo:
            if (sigId <= 1)                return GAL_E1;   // E1C/B
            if (sigId == 3 || sigId == 4)  return GAL_E5a;  // E5aI/Q
            break;

        case EGnssId::BeiDou:
            if (sigId <= 1)                return BDS_B1I;   // B1I
            if (sigId == 7 || sigId == 8)  return BDS_B2a;   // B2a
            break;

        case EGnssId::GLONASS:
        {
            const int k = static_cast<int>(freqId) - 7;  // Frequency slot
            if (sigId == 0) return GLO_L1_BASE + k * GLO_L1_STEP;  // L1OF
            if (sigId == 2) return GLO_L2_BASE + k * GLO_L2_STEP;  // L2OF
            break;
        }
        default: break;
    }
    return 0.0;
}

// ── Pairing result ──────────────────────────────────────────────────
struct DualFreqPair
{
    JimmyPaputto::RawObservation l1;    // Primary (L1/E1/B1I/L1OF)
    JimmyPaputto::RawObservation l2;    // Secondary (L5/E5a/B2a/L2OF)
    double ionoFreePr;                  // Iono-free pseudorange (m)
};

struct PairingResult
{
    std::vector<DualFreqPair> paired;                    // Dual-freq matches
    std::vector<JimmyPaputto::RawObservation> unpaired;  // L1-only (no L5 match)
};

// ── Pair L1+L5 observations per satellite ───────────────────────────
//
// For each (gnssId, svId) that has both an L1 and an L5 observation
// with valid pseudoranges and sufficient C/N0, compute the iono-free
// combination. Unpaired L1 observations are returned separately for
// Klobuchar fallback.
//
inline PairingResult pairObservations(
    const std::vector<JimmyPaputto::RawObservation>& observations,
    double minCno = 15.0)
{
    using namespace JimmyPaputto;

    // Key: (gnssId << 8) | svId
    auto key = [](const RawObservation& obs) -> uint16_t {
        return (static_cast<uint16_t>(obs.gnssId) << 8) | obs.svId;
    };

    // Collect best L1 and L5 per satellite
    std::unordered_map<uint16_t, RawObservation> l1Map, l5Map;

    for (const auto& obs : observations)
    {
        if (!obs.prValid || obs.cno < minCno)
            continue;

        const uint16_t k = key(obs);

        if (isL1Signal(obs.gnssId, obs.sigId))
        {
            auto it = l1Map.find(k);
            if (it == l1Map.end() || obs.cno > it->second.cno)
                l1Map[k] = obs;
        }
        else if (isL5Signal(obs.gnssId, obs.sigId))
        {
            auto it = l5Map.find(k);
            if (it == l5Map.end() || obs.cno > it->second.cno)
                l5Map[k] = obs;
        }
    }

    PairingResult result;

    for (const auto& [k, l1Obs] : l1Map)
    {
        auto it = l5Map.find(k);
        if (it != l5Map.end())
        {
            // Dual-freq pair found — compute iono-free combination
            const auto& l5Obs = it->second;
            const double f1 = carrierFrequency(l1Obs.gnssId, l1Obs.sigId, l1Obs.freqId);
            const double f2 = carrierFrequency(l5Obs.gnssId, l5Obs.sigId, l5Obs.freqId);

            if (f1 > 0 && f2 > 0)
            {
                const double prIF = ionoFreeRange(f1, f2, l1Obs.prMes, l5Obs.prMes);
                result.paired.push_back({l1Obs, l5Obs, prIF});
                continue;
            }
        }

        // No L5 match or invalid frequency — single-freq fallback
        result.unpaired.push_back(l1Obs);
    }

    return result;
}

// ── Melbourne-Wübbena wide-lane combination ─────────────────────────
//
// The wide-lane combination of carrier phase observations has a much
// longer wavelength than L1 or L5 individually, making its integer
// ambiguity easy to resolve (often by simple rounding):
//
//   φ_WL = φ_1 - φ_2  (in cycles)
//   λ_WL = c / (f1 - f2)
//
//   L1/L5:  λ_WL = c / (1575.42 - 1176.45) MHz ≈ 0.751 m
//   L1/L2:  λ_WL = c / (1575.42 - 1227.60) MHz ≈ 0.862 m (GLONASS)
//
// The code-based Melbourne-Wübbena observable is geometry-free and
// can be averaged over time to resolve the wide-lane ambiguity N_WL:
//
//   MW = φ_WL - (f1·P1 + f2·P2) / (f1 + f2) / λ_WL
//      ≈ N_WL + noise
//
// After averaging, round MW to the nearest integer to get N_WL.
// This resolved wide-lane constrains the LAMBDA search space.
//
struct WideLanePair
{
    uint16_t svKey;             // (gnssId << 8) | svId
    double mwObservable;        // Melbourne-Wübbena value (cycles)
    double wideLaneWavelength;  // λ_WL (m)
    int    nWideLane;           // Resolved integer (0 = not yet resolved)
    int    epochCount;          // Number of epochs averaged
    double mwSum;               // Running sum for averaging
};

// Compute the Melbourne-Wübbena observable for a dual-freq pair.
// Requires valid carrier phase on both frequencies.
//
// Returns MW in wide-lane cycles, or nullopt if carrier phase is invalid.
//
inline std::optional<double> melbourneWubbena(
    const JimmyPaputto::RawObservation& l1,
    const JimmyPaputto::RawObservation& l2,
    double f1, double f2)
{
    if (!l1.cpValid || !l2.cpValid || l1.halfCyc || l2.halfCyc)
        return std::nullopt;

    constexpr double c = 299792458.0;
    const double lambdaWL = c / (f1 - f2);

    // Wide-lane phase in meters then cycles
    const double lambda1 = c / f1;
    const double lambda2 = c / f2;
    const double phiWL_m = l1.cpMes * lambda1 - l2.cpMes * lambda2;
    const double phiWL_cyc = phiWL_m / lambdaWL;

    // Narrow-lane pseudorange (meters)
    const double prNL = (f1 * l1.prMes + f2 * l2.prMes) / (f1 + f2);

    // MW = φ_WL(cycles) - P_NL / λ_WL
    return phiWL_cyc - prNL / lambdaWL;
}

// Wide-lane accumulator: averages MW observations over time per satellite.
// After enough epochs, resolves N_WL by rounding the average.
//
class WideLaneResolver
{
public:
    // Update with a new MW observation for a satellite.
    // Returns true if the wide-lane is resolved.
    bool update(uint16_t svKey, double mwValue, double wavelength)
    {
        auto it = accum_.find(svKey);
        if (it == accum_.end())
        {
            accum_[svKey] = {svKey, mwValue, wavelength, 0, 1, mwValue};
            return false;
        }

        auto& wl = it->second;
        wl.epochCount++;
        wl.mwSum += mwValue;
        wl.mwObservable = wl.mwSum / wl.epochCount;

        // Resolve after sufficient averaging (≥10 epochs)
        if (wl.epochCount >= 10 && wl.nWideLane == 0)
        {
            double avg = wl.mwObservable;
            int rounded = static_cast<int>(std::round(avg));
            double frac = std::fabs(avg - rounded);

            // Accept if fractional part < 0.25 cycles
            if (frac < 0.25)
                wl.nWideLane = rounded;
        }

        return wl.nWideLane != 0;
    }

    // Get resolved wide-lane ambiguity for a satellite (0 = not resolved)
    int getWideLane(uint16_t svKey) const
    {
        auto it = accum_.find(svKey);
        return (it != accum_.end()) ? it->second.nWideLane : 0;
    }

    // Reset tracking for a satellite (e.g., after cycle slip)
    void reset(uint16_t svKey)
    {
        accum_.erase(svKey);
    }

    int resolvedCount() const
    {
        int count = 0;
        for (const auto& [k, wl] : accum_)
            if (wl.nWideLane != 0) ++count;
        return count;
    }

private:
    std::unordered_map<uint16_t, WideLanePair> accum_;
};

}  // DualFrequency

#endif  // DUAL_FREQUENCY_HPP_

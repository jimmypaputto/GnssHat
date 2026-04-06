/*
 * Jimmy Paputto 2026
 *
 * Carrier Phase Processing Utilities
 *
 * Provides the foundation for carrier-phase-based positioning:
 *   - Conversion from receiver-reported cycles to meters
 *   - Carrier phase observation validation (halfCyc, cpValid)
 *   - Cycle slip detection using Doppler-predicted phase
 *   - Per-satellite phase arc tracking with automatic reset on slip
 *
 * Carrier phase measurement model:
 *
 *   φ (meters) = ρ + c·δt_rx − c·δt_sv + T − I + λ·N + ε
 *
 * where:
 *   ρ       = geometric range (m)
 *   δt_rx   = receiver clock bias (s)
 *   δt_sv   = satellite clock bias (s)
 *   T       = tropospheric delay (m, same sign as code)
 *   I       = ionospheric delay (m, NEGATIVE for phase vs positive for code)
 *   λ       = carrier wavelength (m)
 *   N       = integer ambiguity (cycles) — the unknown we want to resolve
 *   ε       = noise + multipath (~2 mm for carrier phase)
 *
 * Key difference from pseudorange: the ionospheric delay has opposite
 * sign (phase is advanced, code is delayed). When using the iono-free
 * combination, this doesn't matter since iono cancels.
 *
 * Cycle slip detection: if the receiver loses phase lock, the integer
 * ambiguity N changes discontinuously. We detect this by comparing
 * the actual phase change between epochs with the Doppler-predicted
 * change. A discrepancy > 0.5 cycles indicates a slip.
 */

#ifndef CARRIER_PHASE_HPP_
#define CARRIER_PHASE_HPP_

#include <cmath>
#include <unordered_map>

#include <jimmypaputto/GnssHat.hpp>
#include "DualFrequency.hpp"

namespace CarrierPhase
{

// ── Carrier phase observation (validated + converted) ───────────────
struct CpObservation
{
    double cpMeters;       // Carrier phase in meters (cpMes × λ)
    double wavelength;     // λ = c/f (m)
    double frequency;      // Carrier frequency (Hz)
    uint16_t locktime;     // Continuous lock indicator (ms)
    uint8_t cno;           // Signal strength
    bool valid;            // Passed all quality checks
};

// ── Validate and convert a raw observation's carrier phase ──────────
//
// Returns nullopt if the phase is unusable:
//   - cpValid must be true (receiver reports valid phase)
//   - halfCyc must be false (half-cycle ambiguity resolved)
//   - Carrier frequency must be known
//   - Minimum C/N0 threshold
//
inline std::optional<CpObservation> validateCarrierPhase(
    const JimmyPaputto::RawObservation& obs,
    double minCno = 20.0)
{
    // Reject if phase measurement is invalid
    if (!obs.cpValid)
        return std::nullopt;

    // Reject if half-cycle ambiguity is not resolved — the receiver
    // hasn't determined whether the phase is aligned to a full cycle
    if (obs.halfCyc)
        return std::nullopt;

    // Higher C/N0 threshold for phase than code (needs clean signal)
    if (obs.cno < minCno)
        return std::nullopt;

    // Get carrier frequency
    const double freq = DualFrequency::carrierFrequency(
        obs.gnssId, obs.sigId, obs.freqId);
    if (freq <= 0.0)
        return std::nullopt;

    constexpr double C = 299792458.0;
    const double wavelength = C / freq;

    return CpObservation {
        .cpMeters   = obs.cpMes * wavelength,
        .wavelength = wavelength,
        .frequency  = freq,
        .locktime   = obs.locktime,
        .cno        = obs.cno,
        .valid      = true
    };
}

// ── Cycle slip detection ────────────────────────────────────────────
//
// Tracks the previous epoch's carrier phase and Doppler for each
// satellite. Detects cycle slips by comparing the measured phase
// change with the Doppler-predicted phase change.
//
// Doppler gives the rate of change of range in Hz (cycles/s).
// Over interval dt:
//   predicted Δφ = −Doppler × dt     (cycles)
//   measured  Δφ = φ_k − φ_{k-1}    (cycles)
//
// If |measured − predicted| > threshold → cycle slip detected.
//
class SlipDetector
{
public:
    // Returns true if the arc is continuous (no slip detected).
    // Returns false on first observation or after a slip.
    bool checkAndUpdate(JimmyPaputto::EGnssId gnssId, uint8_t svId,
                        uint8_t sigId, double cpCycles, float doppler,
                        uint16_t locktime, double epochTime)
    {
        const uint32_t key = makeKey(gnssId, svId, sigId);
        auto it = history_.find(key);

        if (it == history_.end())
        {
            // First observation for this SV — start new arc
            history_[key] = {cpCycles, doppler, locktime, epochTime, 1};
            return false;
        }

        auto& prev = it->second;

        // Check 1: locktime decrease indicates receiver lost lock
        if (locktime < prev.locktime && prev.locktime > 500)
        {
            prev = {cpCycles, doppler, locktime, epochTime, 1};
            return false;
        }

        // Check 2: Doppler-phase consistency
        const double dt = epochTime - prev.epochTime;
        if (dt > 0 && dt < 10.0)  // Reasonable epoch interval
        {
            // Predicted phase change (cycles) from average Doppler
            const double avgDoppler = (doppler + prev.doppler) * 0.5;
            const double predictedDelta = -avgDoppler * dt;
            const double measuredDelta = cpCycles - prev.cpCycles;

            const double discrepancy = std::fabs(measuredDelta - predictedDelta);

            if (discrepancy > SLIP_THRESHOLD_CYCLES)
            {
                // Cycle slip detected — reset arc
                prev = {cpCycles, doppler, locktime, epochTime, 1};
                return false;
            }
        }

        // Arc continues
        prev.cpCycles = cpCycles;
        prev.doppler = doppler;
        prev.locktime = locktime;
        prev.epochTime = epochTime;
        prev.arcLength++;

        return true;
    }

    // Get arc length (number of continuous epochs) for a satellite
    int getArcLength(JimmyPaputto::EGnssId gnssId, uint8_t svId,
                     uint8_t sigId) const
    {
        auto it = history_.find(makeKey(gnssId, svId, sigId));
        return (it != history_.end()) ? it->second.arcLength : 0;
    }

    // Reset all tracking (e.g. after long gap)
    void reset() { history_.clear(); }

private:
    static constexpr double SLIP_THRESHOLD_CYCLES = 0.5;

    struct PhaseHistory
    {
        double cpCycles;
        float doppler;
        uint16_t locktime;
        double epochTime;
        int arcLength;
    };

    static uint32_t makeKey(JimmyPaputto::EGnssId gnssId,
                            uint8_t svId, uint8_t sigId)
    {
        return (static_cast<uint32_t>(gnssId) << 16)
             | (static_cast<uint32_t>(svId) << 8)
             | sigId;
    }

    std::unordered_map<uint32_t, PhaseHistory> history_;
};

}  // CarrierPhase

#endif  // CARRIER_PHASE_HPP_

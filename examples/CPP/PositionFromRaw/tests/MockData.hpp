/*
 * Mock GNSS observation data for algorithm testing.
 *
 * Provides a consistent set of satellite positions, pseudoranges,
 * carrier phase, and Doppler observations at a known receiver location
 * so that solver outputs can be validated against ground truth.
 *
 * Reference location: Somewhere in Europe (roughly 47°N, 15°E, 200 m)
 *   WGS-84 ECEF: X = 4195059.970, Y = 1158663.544, Z = 4647215.064
 *   LLA:         lat = 47.07°, lon = 15.44°, alt = 200 m
 *
 * The satellite positions and pseudoranges are synthetic but geometrically
 * consistent — each pseudorange equals the geometric range to the SV plus
 * a constant receiver clock bias (100 km ≈ 333 μs).
 */

#ifndef MOCK_DATA_HPP_
#define MOCK_DATA_HPP_

#include <cmath>
#include <vector>

#include <jimmypaputto/GnssHat.hpp>
#include "GnssMath.hpp"
#include "DualFrequency.hpp"

namespace MockData
{

// ── Ground truth ────────────────────────────────────────────────────
constexpr double TRUE_X = 4195059.970;
constexpr double TRUE_Y = 1158663.544;
constexpr double TRUE_Z = 4647215.064;
constexpr double TRUE_LAT_DEG = 47.07;
constexpr double TRUE_LON_DEG = 15.44;
constexpr double TRUE_ALT     = 200.0;

constexpr double CLOCK_BIAS_M = 100000.0;  // 100 km (≈ 333 μs)
constexpr double GPS_TOW      = 388800.0;  // Monday 12:00:00 GPS time
constexpr uint16_t GPS_WEEK   = 2361;

// Speed of light (consistent with GnssMath::C)
constexpr double C = 299792458.0;

// ── Satellite positions in ECEF ─────────────────────────────────────
//
// 8 satellites placed at ~26560 km orbital radius with good sky distribution.
// Mix of GPS, Galileo, GLONASS, BeiDou — all above the horizon from the
// reference receiver position.
//
struct MockSatellite
{
    JimmyPaputto::EGnssId gnssId;
    uint8_t svId;
    uint8_t sigId;
    uint8_t freqId;
    double x, y, z;           // ECEF position (m)
    double clockBias;          // SV clock bias (seconds)
};

inline std::vector<MockSatellite> satellites()
{
    return {
        // GPS satellites
        {JimmyPaputto::EGnssId::GPS, 2, 0, 0,
         13485496.0,  7590664.0, 21586033.0,  3.5e-5},   // az=45° el=75°
        {JimmyPaputto::EGnssId::GPS, 5, 0, 0,
         19203078.0, 15374735.0, 10014635.0,  1.2e-5},   // az=135° el=50°
        {JimmyPaputto::EGnssId::GPS, 10, 0, 0,
         25223002.0, -6482188.0,  5216800.0, -2.1e-5},   // az=225° el=35°
        {JimmyPaputto::EGnssId::GPS, 15, 0, 0,
         12813984.0, -4112295.0, 22898132.0,  5.7e-5},   // az=315° el=60°

        // Galileo satellites
        {JimmyPaputto::EGnssId::Galileo, 1, 1, 0,
         10176525.0, 18710076.0, 15868365.0, -1.8e-5},   // az=90° el=45°
        {JimmyPaputto::EGnssId::Galileo, 8, 1, 0,
         25487742.0,  7039641.0, -2500407.0,  4.3e-5},   // az=180° el=25°

        // GLONASS satellites (freqId encodes frequency slot: slot = freqId - 7)
        {JimmyPaputto::EGnssId::GLONASS, 3, 0, 8,
         17975798.0,-12531654.0, 15008728.0,  8.9e-6},   // slot +1, az=270° el=40°

        // BeiDou satellite
        {JimmyPaputto::EGnssId::BeiDou, 11, 0, 0,
          7001303.0,  1933740.0, 25547525.0, -3.4e-5},   // az=0° el=55°
    };
}

// ── Compute geometric range from true receiver position to satellite ─
inline double trueRange(const MockSatellite& sv)
{
    const double dx = sv.x - TRUE_X;
    const double dy = sv.y - TRUE_Y;
    const double dz = sv.z - TRUE_Z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// ── Generate mock RawObservation (code + carrier phase) ──────────────
//
// Pseudorange = geometric_range + clock_bias + sv_clock*c
// Carrier phase (cycles) = (geometric_range + clock_bias - sv_clock*c + λ*N) / λ
//
// Ambiguity N is set to a known integer for each satellite.
//
inline JimmyPaputto::RawObservation makeObservation(
    const MockSatellite& sv,
    double noisePr = 0.0,         // Pseudorange noise (m)
    double noiseCp = 0.0,         // Carrier phase noise (cycles)
    int ambiguity = 1000000,      // Integer ambiguity (cycles)
    uint8_t cno = 42,
    bool cpValid = true)
{
    const double range = trueRange(sv);
    const double pr = range + CLOCK_BIAS_M - sv.clockBias * C + noisePr;

    // Carrier frequency and wavelength
    double freq = DualFrequency::carrierFrequency(sv.gnssId, sv.sigId, sv.freqId);
    if (freq <= 0) freq = 1575.42e6;  // Fallback to GPS L1
    const double lambda = C / freq;

    // Phase (cycles) = (range + clockBias - svClock*c + λ*N) / λ
    const double phaseMeters = range + CLOCK_BIAS_M - sv.clockBias * C + lambda * ambiguity;
    const double cpCycles = phaseMeters / lambda + noiseCp;

    // Doppler: approximately -range_rate/λ; static scenario → 0. for testing
    const float doppler = 0.0f;

    JimmyPaputto::RawObservation obs{};
    obs.prMes     = pr;
    obs.cpMes     = cpCycles;
    obs.doMes     = doppler;
    obs.gnssId    = sv.gnssId;
    obs.svId      = sv.svId;
    obs.sigId     = sv.sigId;
    obs.freqId    = sv.freqId;
    obs.locktime  = 5000;    // 5 seconds of lock
    obs.cno       = cno;
    obs.prStdev   = 2;
    obs.cpStdev   = 1;
    obs.doStdev   = 1;
    obs.prValid   = true;
    obs.cpValid   = cpValid;
    obs.halfCyc   = false;
    obs.subHalfCyc = false;

    return obs;
}

// ── Generate a full set of paired observations for the SPP solver ────
inline std::vector<GnssMath::PairedObservation> makePairedObservations(
    double prNoise = 0.0)
{
    std::vector<GnssMath::PairedObservation> paired;
    const auto svs = satellites();

    for (const auto& sv : svs)
    {
        auto obs = makeObservation(sv, prNoise);

        paired.push_back({
            .obs = obs,
            .sv  = GnssMath::SatelliteState{
                .gnssId    = sv.gnssId,
                .svId      = sv.svId,
                .position  = {sv.x, sv.y, sv.z},
                .clockBias = sv.clockBias
            },
            .ionoFreePr = std::nullopt,
            .cpMeters   = std::nullopt,
            .wavelength = 0.0,
            .cpUsable   = false
        });
    }
    return paired;
}

// ── Generate paired observations with carrier phase ──────────────────
inline std::vector<GnssMath::PairedObservation> makePairedWithPhase(
    double prNoise = 0.0, int baseAmbiguity = 1000000)
{
    auto paired = makePairedObservations(prNoise);
    const auto svs = satellites();

    for (size_t i = 0; i < paired.size(); ++i)
    {
        const auto& sv = svs[i];
        double freq = DualFrequency::carrierFrequency(
            sv.gnssId, sv.sigId, sv.freqId);
        if (freq <= 0) freq = 1575.42e6;
        double lambda = C / freq;

        double range = trueRange(sv);
        double phaseMeters = range + CLOCK_BIAS_M - sv.clockBias * C
                           + lambda * (baseAmbiguity + static_cast<int>(i));

        paired[i].cpMeters = phaseMeters;
        paired[i].wavelength = lambda;
        paired[i].cpUsable = true;
    }
    return paired;
}

// ── Known integer ambiguities per satellite (for validation) ─────────
inline std::vector<int> trueAmbiguities(int baseAmbiguity = 1000000)
{
    const int n = static_cast<int>(satellites().size());
    std::vector<int> amb(n);
    for (int i = 0; i < n; ++i)
        amb[i] = baseAmbiguity + i;
    return amb;
}

// ── Zero atmospheric corrections (matching mock pseudoranges) ────────
//
// Mock pseudoranges contain no atmospheric delays, so tests must use
// a correction model that returns zero for tropo + iono.
//
inline GnssMath::CorrectionModel noCorrections()
{
    GnssMath::CorrectionModel m;
    m.troposphere = [](const GnssMath::Lla&, double, double, double) { return 0.0; };
    m.ionosphere  = [](const GnssMath::Lla&, double, double, double) { return 0.0; };
    return m;
}

}  // MockData

#endif  // MOCK_DATA_HPP_

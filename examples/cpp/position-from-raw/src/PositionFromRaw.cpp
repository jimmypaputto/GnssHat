/*
 * Jimmy Paputto 2026
 *
 * PositionFromRaw — Multi-constellation standalone position computation
 * from raw GNSS observations with RTCM3 generation and correction ingestion.
 *
 * This example demonstrates:
 *  1. Reading UBX-RXM-RAWX (raw pseudorange/carrier observations)
 *  2. Decoding broadcast ephemeris from UBX-RXM-SFRBX for 4 constellations:
 *     - GPS:     Keplerian elements from subframes 1-3 (IS-GPS-200)
 *     - Galileo:  Keplerian elements from I/NAV word types 1-5
 *     - BeiDou:   Keplerian elements from D1 subframes 1-3 (MEO/IGSO)
 *     - GLONASS:  State-vector propagation from strings 1-4 (ICD 5.1)
 *  3. Computing a single-point position (SPP) via weighted least-squares
 *     with atmospheric corrections:
 *     - Troposphere: UNB3m model with Neill mapping functions
 *     - Ionosphere:  Dual-frequency iono-free combination (L1/L5) when
 *       both frequencies are available; Klobuchar single-frequency model
 *       (GPS broadcast) as fallback for single-freq receivers or SVs
 *       without a second measurement
 *  4. Carrier phase integer ambiguity resolution (IAR):
 *     - Cycle slip detection via Doppler-phase consistency
 *     - Float ambiguity estimation (extended WLS: [x,y,z,cdt,N1..Nm])
 *     - LAMBDA decorrelation + integer search + ratio test
 *     - Melbourne-Wübbena wide-lane bootstrapping for dual-freq
 *  5. Encoding raw observations as RTCM3 MSM4 messages (1074/1084/1094/1124)
 *  6. Serving generated RTCM3 frames over TCP (port 2102)
 *  7. Accepting RTCM3 corrections from an external source (port 2101)
 *
 * Architecture:
 *  ┌──────────────────────────────────────────────────────────────────┐
 *  │  GNSS Receiver (u-blox)                                         │
 *  │    ├─ UBX-RXM-RAWX  → raw pseudoranges (L1 all constellations)  │
 *  │    ├─ UBX-RXM-SFRBX → broadcast ephemeris subframes             │
 *  │    ├─ UBX-NAV-PVT   → receiver position (reference)             │
 *  │    └─ RTCM3 input   ← rover corrections (if RTK HAT)            │
 *  └──────────────────────────────────────────────────────────────────┘
 *          │                                     ▲
 *          ▼                                     │
 *  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────┐
 *  │ SPP Solver       │  │ RTCM3 Encoder   │  │ RTCM3 Receiver     │
 *  │ WLS + UNB3m      │  │ → TCP :2102     │  │ TCP :2101          │
 *  │ + Iono-free/Klob │  │ MSM4 broadcast  │  │ → applyCorrections │
 *  │ GPS/GAL/BDS/GLO  │  │                 │  │                    │
 *  └─────────────────┘  └─────────────────┘  └─────────────────────┘
 *
 * Satellite positions: computed from real broadcast ephemeris for all
 * 4 constellations. Satellites without a complete decoded ephemeris
 * are skipped (no elevation/azimuth fallback).
 */

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#include <signal.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <jimmypaputto/GnssHat.hpp>

#include "GnssMath.hpp"
#include "GpsEphemeris.hpp"
#include "GalileoEphemeris.hpp"
#include "BeidouEphemeris.hpp"
#include "GlonassEphemeris.hpp"
#include "Troposphere.hpp"
#include "Ionosphere.hpp"
#include "DualFrequency.hpp"
#include "CarrierPhase.hpp"
#include "AmbiguityFloat.hpp"
#include "AmbiguityLambda.hpp"
#include "Rtcm3Encoder.hpp"
#include "Rtcm3Tcp.hpp"


using namespace JimmyPaputto;

// ── Configuration ───────────────────────────────────────────────────
static constexpr uint16_t RTCM3_RECEIVE_PORT = 2101;   // Incoming corrections
static constexpr uint16_t RTCM3_SEND_PORT    = 2102;   // Outgoing RTCM3 stream
static constexpr uint16_t STATION_ID         = 1;       // RTCM3 station ID
static constexpr double   MIN_CNO            = 15.0;    // Minimum C/N0 filter
static constexpr int      MEASUREMENT_RATE   = 1;       // Hz
static constexpr int      CURRENT_LEAP_S     = 18;      // GPS-UTC leap seconds (2017+)

// ── Global state ────────────────────────────────────────────────────
static std::atomic<bool> running{true};
static std::atomic<int>  rtcm3FramesReceived{0};
static std::atomic<int>  rtcm3FramesSent{0};

void signalHandler(int sig)
{
    if (sig == SIGINT)
        running = false;
}

// ── GNSS config ─────────────────────────────────────────────────────
GnssConfig createConfig()
{
    return GnssConfig {
        .measurementRate_Hz = MEASUREMENT_RATE,
        .dynamicModel = EDynamicModel::Stationary,
        .timepulsePinConfig = TimepulsePinConfig {
            .active = true,
            .fixedPulse = TimepulsePinConfig::Pulse { 1, 0.1 },
            .pulseWhenNoFix = std::nullopt,
            .polarity = ETimepulsePinPolarity::RisingEdgeAtTopOfSecond
        },
        .geofencing = std::nullopt,
        .rtk = std::nullopt
        // Note: Set .rtk = RtkConfig{ .mode = ERtkMode::Rover }
        // on an RTK HAT to enable correction ingestion via
        // rtk()->rover()->applyCorrections()
    };
}

// ── Terminal helpers ────────────────────────────────────────────────
#define RST   "\033[0m"
#define BOLD  "\033[1m"
#define GRN   "\033[32m"
#define YEL   "\033[33m"
#define BLU   "\033[34m"
#define CYN   "\033[36m"
#define RED   "\033[31m"
#define MAG   "\033[35m"

void clearScreen() { printf("\033[2J\033[H"); }
void hideCursor()  { printf("\033[?25l"); }
void showCursor()  { printf("\033[?25h"); }

int getTermWidth()
{
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_col;
}

void hline(int width, char c = '-')
{
    for (int i = 0; i < width; ++i) putchar(c);
}

void printRow(const char* label, const char* fmt, ...)
{
    printf("  " CYN "%-28s" RST, label);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

// ── Day-of-year from GPS week and TOW ───────────────────────────────
int gpsDayOfYear(uint16_t gpsWeek, double gpsTow)
{
    // GPS epoch: January 6, 1980
    constexpr int GPS_EPOCH_JD = 2444245;  // Julian day of GPS epoch
    const int daysSinceEpoch = gpsWeek * 7 + static_cast<int>(gpsTow / 86400.0);
    const int jd = GPS_EPOCH_JD + daysSinceEpoch;

    // Julian day to calendar date (Meeus algorithm)
    int a = jd + 32044;
    int b = (4 * a + 3) / 146097;
    int c = a - (146097 * b) / 4;
    int d = (4 * c + 3) / 1461;
    int e = c - (1461 * d) / 4;
    int m = (5 * e + 2) / 153;
    int day = e - (153 * m + 2) / 5 + 1;
    int month = m + 3 - 12 * (m / 10);
    int year = 100 * b + d - 4800 + m / 10;

    // Day of year
    constexpr int daysInMonth[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    int doy = day;
    for (int i = 1; i < month; ++i)
        doy += daysInMonth[i] + (i == 2 && leap ? 1 : 0);
    return doy;
}

// ── Main display ────────────────────────────────────────────────────
void printStatus(
    const Navigation& nav,
    const std::optional<GnssMath::PositionSolution>& spp,
    int rtcm3ClientCount,
    const Rtcm3Encode::Rtcm3Output& rtcm3Out,
    const GpsEphemeris::EphemerisStore& gpsEphStore,
    const GalileoEphemeris::EphemerisStore& galEphStore,
    const BeidouEphemeris::EphemerisStore& bdsEphStore,
    const GlonassEphemeris::EphemerisStore& gloEphStore,
    bool hasKlobuchar,
    int dualFreqCount, int singleFreqCount,
    int cpUsableCount, int wlResolvedCount,
    int pairedSvCount)
{
    const int tw = getTermWidth();

    clearScreen();

    // Title
    printf(BOLD GRN "+");
    hline(tw - 2, '-');
    printf("+\n");
    const char* title = "Position From Raw Observations + RTCM3";
    int pad = (tw - (int)strlen(title) - 2) / 2;
    printf("|%*s%s%*s|\n", pad, "", title, tw - (int)strlen(title) - pad - 2, "");
    printf("+");
    hline(tw - 2, '-');
    printf("+" RST "\n\n");

    // Receiver PVT (reference)
    const auto& pvt = nav.pvt;
    printf(BOLD " Receiver PVT (reference):" RST "\n");
    printRow("Fix Type",      YEL "%s" RST, Utils::eFixType2string(pvt.fixType).c_str());
    printRow("Fix Quality",   YEL "%s" RST, Utils::eFixQuality2string(pvt.fixQuality).c_str());
    printRow("Latitude",      YEL "%.8f deg" RST, pvt.latitude);
    printRow("Longitude",     YEL "%.8f deg" RST, pvt.longitude);
    printRow("Altitude (MSL)",YEL "%.2f m" RST, pvt.altitudeMSL);
    printRow("Accuracy (H/V)",YEL "%.2f m / %.2f m" RST,
             pvt.horizontalAccuracy, pvt.verticalAccuracy);
    printRow("Visible SVs",   YEL "%u" RST, pvt.visibleSatellites);
    printf("\n");

    // Raw observations summary
    const auto& raw = nav.rawMeasurements;
    const auto l1Obs = GnssMath::filterL1Observations(raw.observations, MIN_CNO);
    printf(BOLD " Raw Observations:" RST "\n");
    printRow("GPS Week",      YEL "%u" RST, raw.week);
    printRow("Receiver TOW",  YEL "%.3f s" RST, raw.rcvTow);
    printRow("Total Meas",    YEL "%u" RST, raw.numMeas);
    printRow("L1 (filtered)", YEL "%zu" RST, l1Obs.size());
    printRow("Leap Seconds",  YEL "%d" RST, raw.leapS);

    // Count per constellation
    int gpsCount = 0, galCount = 0, gloCount = 0, bdsCount = 0;
    for (const auto& o : l1Obs)
    {
        switch (o.gnssId)
        {
            case EGnssId::GPS:     ++gpsCount; break;
            case EGnssId::Galileo: ++galCount; break;
            case EGnssId::GLONASS: ++gloCount; break;
            case EGnssId::BeiDou:  ++bdsCount; break;
            default: break;
        }
    }
    printRow("  GPS / GAL / GLO / BDS",
             YEL "%d / %d / %d / %d" RST,
             gpsCount, galCount, gloCount, bdsCount);
    printf("\n");

    // SPP solution
    const char* modeStr = "SPP";
    const char* modeColor = YEL;
    if (spp)
    {
        switch (spp->mode)
        {
            case GnssMath::SolutionMode::Fixed:
                modeStr = "FIXED"; modeColor = GRN; break;
            case GnssMath::SolutionMode::Float:
                modeStr = "FLOAT"; modeColor = CYN; break;
            default: break;
        }
    }
    printf(BOLD " Position Solution (%s%s" RST BOLD "):" RST "\n", modeColor, modeStr);
    if (spp && spp->converged)
    {
        printRow("ECEF X",       YEL "%.3f m" RST, spp->ecef.x);
        printRow("ECEF Y",       YEL "%.3f m" RST, spp->ecef.y);
        printRow("ECEF Z",       YEL "%.3f m" RST, spp->ecef.z);
        printRow("Latitude",     GRN "%.8f deg" RST, spp->lla.lat_rad * GnssMath::RAD2DEG);
        printRow("Longitude",    GRN "%.8f deg" RST, spp->lla.lon_rad * GnssMath::RAD2DEG);
        printRow("Altitude",     GRN "%.2f m" RST, spp->lla.alt);
        printRow("Clock Bias",   YEL "%.3f m (%.1f ns)" RST,
                 spp->receiverClockBias_m,
                 spp->receiverClockBias_m / GnssMath::C * 1e9);
        printRow("HDOP / VDOP",  YEL "%.2f / %.2f" RST, spp->hdop, spp->vdop);
        printRow("PDOP",         YEL "%.2f" RST, spp->pdop);
        printRow("Used SVs",     YEL "%d" RST, spp->usedSatellites);
        printRow("Iterations",   YEL "%d" RST, spp->iterations);
        printRow("Residual RMS", YEL "%.2f m" RST, spp->residualRms);

        // Ambiguity resolution info
        if (spp->mode != GnssMath::SolutionMode::SPP)
        {
            printRow("Solution Mode",
                     spp->mode == GnssMath::SolutionMode::Fixed
                         ? GRN "FIXED (%d ambiguities, ratio %.1f)" RST
                         : CYN "FLOAT (%d ambiguities)" RST,
                     spp->fixedAmbiguities, spp->ratioTest);
        }

        // Delta to receiver PVT
        const GnssMath::Ecef pvtEcef = GnssMath::lla2ecef({
            pvt.latitude * GnssMath::DEG2RAD,
            pvt.longitude * GnssMath::DEG2RAD,
            static_cast<double>(pvt.altitudeMSL)
        });
        const auto enu = GnssMath::ecef2enu(spp->ecef, pvtEcef);
        const double hDelta = std::sqrt(enu.e * enu.e + enu.n * enu.n);
        printRow("Delta to PVT (H)",
                 MAG "%.2f m (E:%.1f N:%.1f U:%.1f)" RST,
                 hDelta, enu.e, enu.n, enu.u);
    }
    else if (spp)
    {
        printRow("Status", RED "Did not converge (%d iterations)" RST,
                 spp->iterations);
    }
    else
    {
        const auto& raw = nav.rawMeasurements;
        const auto l1 = GnssMath::filterL1Observations(raw.observations, MIN_CNO);
        int ephCount = gpsEphStore.completeCount() + galEphStore.completeCount()
                     + bdsEphStore.completeCount() + gloEphStore.completeCount();
        if (nav.pvt.fixType == EFixType::NoFix)
            printRow("Status", YEL "Waiting for receiver PVT fix..." RST);
        else if (l1.size() < 4)
            printRow("Status", YEL "Need >= 4 L1 observations (have %zu)" RST,
                     l1.size());
        else if (ephCount < 4)
            printRow("Status", YEL "Decoding ephemeris... (%d complete, need >= 4 with obs)" RST,
                     ephCount);
        else if (pairedSvCount < 4)
            printRow("Status", YEL "Need >= 4 SVs with ephemeris (have %d)" RST,
                     pairedSvCount);
        else
            printRow("Status", RED "Solver returned no solution" RST);
    }
    printf("\n");

    // RTCM3 status
    printf(BOLD " RTCM3 Output (port %u):" RST "\n", RTCM3_SEND_PORT);
    printRow("Connected clients", YEL "%d" RST, rtcm3ClientCount);
    printRow("Frames this epoch", YEL "%zu" RST, rtcm3Out.frames.size());
    for (const auto& frame : rtcm3Out.frames)
    {
        uint16_t msgId = Rtcm3Tcp::StreamParser::getMessageId(frame);
        printRow("  Frame", YEL "MSG %u, %zu bytes" RST,
                 msgId, frame.size());
    }
    printRow("Total frames sent", YEL "%d" RST, rtcm3FramesSent.load());
    printf("\n");

    printf(BOLD " RTCM3 Input (port %u):" RST "\n", RTCM3_RECEIVE_PORT);
    printRow("Frames received", YEL "%d" RST, rtcm3FramesReceived.load());
    printf("\n");

    // Atmospheric corrections status
    printf(BOLD " Corrections:" RST "\n");
    printRow("Troposphere",   GRN "UNB3m + Neill mapping" RST);
    if (dualFreqCount > 0)
        printRow("Ionosphere",
                 GRN "Iono-free L1/L5: %d SVs" RST " + " YEL "Klobuchar: %d SVs" RST,
                 dualFreqCount, singleFreqCount);
    else
        printRow("Ionosphere",    hasKlobuchar
                 ? YEL "Klobuchar (single-freq, %d SVs)" RST
                 : YEL "Klobuchar (waiting for SF4 page 18)" RST,
                 singleFreqCount);
    printRow("Carrier Phase",
             cpUsableCount > 0
                 ? GRN "%d SVs usable" RST : YEL "0 SVs usable" RST,
             cpUsableCount);
    if (wlResolvedCount > 0)
        printRow("Wide-Lane",
                 GRN "%d resolved" RST, wlResolvedCount);
    printf("\n");

    // Ephemeris status — GPS
    const auto gpsStatus = gpsEphStore.getStatus();
    printf(BOLD " GPS Ephemeris (%d complete):" RST "\n", gpsEphStore.completeCount());
    if (!gpsStatus.empty())
    {
        printf("  " BOLD "%-6s %-5s %-5s %-5s %-10s %-8s" RST "\n",
               "SV", "SF1", "SF2", "SF3", "Complete", "Healthy");
        printf("  "); hline(48, '-'); printf("\n");
        for (const auto& s : gpsStatus)
        {
            const char* color = s.complete ? (s.healthy ? GRN : RED) : YEL;
            printf("  %s%-6u %-5s %-5s %-5s %-10s %-8s" RST "\n", color,
                   s.svId, s.sf1?"Y":"-", s.sf2?"Y":"-", s.sf3?"Y":"-",
                   s.complete?"YES":"...", s.complete?(s.healthy?"OK":"BAD"):"");
        }
    }
    printf("\n");

    // Ephemeris status — Galileo
    const auto galStatus = galEphStore.getStatus();
    printf(BOLD " Galileo Ephemeris (%d complete):" RST "\n", galEphStore.completeCount());
    if (!galStatus.empty())
    {
        printf("  " BOLD "%-6s %-5s %-5s %-5s %-5s %-5s %-10s %-8s" RST "\n",
               "SV", "W1", "W2", "W3", "W4", "W5", "Complete", "Healthy");
        printf("  "); hline(62, '-'); printf("\n");
        for (const auto& s : galStatus)
        {
            const char* color = s.complete ? (s.healthy ? GRN : RED) : YEL;
            printf("  %s%-6u %-5s %-5s %-5s %-5s %-5s %-10s %-8s" RST "\n", color,
                   s.svId, s.t1?"Y":"-", s.t2?"Y":"-", s.t3?"Y":"-",
                   s.t4?"Y":"-", s.t5?"Y":"-",
                   s.complete?"YES":"...", s.complete?(s.healthy?"OK":"BAD"):"");
        }
    }
    printf("\n");

    // Ephemeris status — BeiDou
    const auto bdsStatus = bdsEphStore.getStatus();
    printf(BOLD " BeiDou Ephemeris (%d complete):" RST "\n", bdsEphStore.completeCount());
    if (!bdsStatus.empty())
    {
        printf("  " BOLD "%-6s %-5s %-5s %-5s %-10s %-8s" RST "\n",
               "SV", "SF1", "SF2", "SF3", "Complete", "Healthy");
        printf("  "); hline(48, '-'); printf("\n");
        for (const auto& s : bdsStatus)
        {
            const char* color = s.complete ? (s.healthy ? GRN : RED) : YEL;
            printf("  %s%-6u %-5s %-5s %-5s %-10s %-8s" RST "\n", color,
                   s.svId, s.sf1?"Y":"-", s.sf2?"Y":"-", s.sf3?"Y":"-",
                   s.complete?"YES":"...", s.complete?(s.healthy?"OK":"BAD"):"");
        }
    }
    printf("\n");

    // Ephemeris status — GLONASS
    const auto gloStatus = gloEphStore.getStatus();
    printf(BOLD " GLONASS Ephemeris (%d complete):" RST "\n", gloEphStore.completeCount());
    if (!gloStatus.empty())
    {
        printf("  " BOLD "%-6s %-5s %-5s %-5s %-5s %-5s %-10s %-8s" RST "\n",
               "SV", "Slot", "S1", "S2", "S3", "S4", "Complete", "Healthy");
        printf("  "); hline(58, '-'); printf("\n");
        for (const auto& s : gloStatus)
        {
            const char* color = s.complete ? (s.healthy ? GRN : RED) : YEL;
            printf("  %s%-6u %-+5d %-5s %-5s %-5s %-5s %-10s %-8s" RST "\n", color,
                   s.svId, s.freqSlot,
                   s.s1?"Y":"-", s.s2?"Y":"-", s.s3?"Y":"-", s.s4?"Y":"-",
                   s.complete?"YES":"...", s.complete?(s.healthy?"OK":"BAD"):"");
        }
    }
    printf("\n");

    // Observation table (compact)
    printf(BOLD " L1 Observations:" RST "\n");
    printf("  " BOLD "%-8s %-4s %-8s %6s %16s %14s %4s" RST "\n",
           "System", "SV", "Signal", "C/N0", "Pseudorange(m)", "Doppler(Hz)", "PR?");
    printf("  ");
    hline(70, '-');
    printf("\n");

    for (const auto& obs : l1Obs)
    {
        const char* color = (obs.cno >= 35) ? GRN :
                            (obs.cno >= 20) ? YEL : RED;
        printf("  %s%-8s %-4u %-8s %4u dB %16.3f %14.3f  %s" RST "\n",
               color,
               Utils::gnssId2string(obs.gnssId).c_str(),
               obs.svId,
               Utils::gnssSignalId2string(obs.gnssId, obs.sigId).c_str(),
               obs.cno,
               obs.prMes,
               static_cast<double>(obs.doMes),
               obs.prValid ? "Y" : "N");
    }

    printf("\n  Press Ctrl+C to exit\n");
    fflush(stdout);
}

// ── Main ────────────────────────────────────────────────────────────
auto main() -> int
{
    signal(SIGINT, signalHandler);

    // Create and start GNSS HAT
    auto* ubxHat = IGnssHat::create();
    if (!ubxHat)
    {
        fprintf(stderr, "Failed to create GNSS HAT instance\n");
        return 1;
    }

    ubxHat->softResetUbloxSom_HotStart();
    if (!ubxHat->start(createConfig()))
    {
        fprintf(stderr, "Failed to start GNSS\n");
        return 1;
    }
    printf("GNSS started. Waiting for observations...\n");

    // ── RTCM3 TCP sender (outgoing generated corrections) ───────────
    Rtcm3Tcp::Sender rtcm3Sender(RTCM3_SEND_PORT);
    rtcm3Sender.start();

    // ── RTCM3 TCP receiver (incoming external corrections) ──────────
    // If this HAT supports RTK rover mode, corrections will be applied.
    // Otherwise they are just counted for display.
    IRtk* rtk = ubxHat->rtk();

    Rtcm3Tcp::Receiver rtcm3Receiver(RTCM3_RECEIVE_PORT,
        [&](const std::vector<std::vector<uint8_t>>& frames)
        {
            rtcm3FramesReceived += static_cast<int>(frames.size());

            // If RTK rover is available, forward corrections to receiver
            if (rtk && rtk->rover())
                rtk->rover()->applyCorrections(frames);
        });
    rtcm3Receiver.start();

    // ── Ephemeris stores (decoded from UBX-RXM-SFRBX) ──────────────
    GpsEphemeris::EphemerisStore     gpsEphStore;
    GalileoEphemeris::EphemerisStore galEphStore;
    BeidouEphemeris::EphemerisStore  bdsEphStore;
    GlonassEphemeris::EphemerisStore gloEphStore;

    // ── Previous SPP solution for warm-starting the solver ──────────
    GnssMath::Ecef lastSppEcef = {0, 0, 0};

    // ── Carrier phase processing state (persistent across epochs) ───
    CarrierPhase::SlipDetector slipDetector;
    DualFrequency::WideLaneResolver wideLaneResolver;

    hideCursor();

    while (running)
    {
        const auto navigation = ubxHat->waitAndGetFreshNavigation();
        const auto& raw = navigation.rawMeasurements;

        if (raw.observations.empty())
            continue;

        // ── 0. Process any new subframes for ephemeris ──────────────
        gpsEphStore.processAll(navigation.subframeBuffer);
        galEphStore.processAll(navigation.subframeBuffer);
        bdsEphStore.processAll(navigation.subframeBuffer);
        gloEphStore.processAll(navigation.subframeBuffer);

        // ── 1. Pair observations (dual-freq where possible) ───────
        //
        // On multi-frequency receivers (F9P, F10N), L1+L5 pairs are
        // formed and the iono-free pseudorange eliminates the first-
        // order ionospheric delay. On single-frequency receivers
        // (M9N), all observations fall through as unpaired L1 and
        // the Klobuchar model is used instead.
        //
        const auto pairing = DualFrequency::pairObservations(
            raw.observations, MIN_CNO);

        // Build a unified L1 observation list (for display and RTCM3)
        std::vector<JimmyPaputto::RawObservation> l1Obs;
        l1Obs.reserve(pairing.paired.size() + pairing.unpaired.size());
        for (const auto& dp : pairing.paired)
            l1Obs.push_back(dp.l1);
        for (const auto& obs : pairing.unpaired)
            l1Obs.push_back(obs);

        // Iono-free lookup: (gnssId << 8 | svId) → IF pseudorange
        std::unordered_map<uint16_t, double> ionoFreeMap;
        for (const auto& dp : pairing.paired)
            ionoFreeMap[(static_cast<uint16_t>(dp.l1.gnssId) << 8) | dp.l1.svId]
                = dp.ionoFreePr;

        // ── 2. SPP: attempt position solution ───────────────────────
        //
        // For each L1 observation, look up the decoded broadcast
        // ephemeris for that constellation. If a complete, healthy
        // ephemeris exists, compute the satellite's ECEF position and
        // clock bias at the signal transmission time. Observations
        // without ephemeris are skipped (no fallback approximation).
        //
        // Atmospheric corrections:
        //   - Troposphere: UNB3m (latitude/altitude/DOY interpolation)
        //   - Ionosphere:  Iono-free L1/L5 combination when dual-freq
        //                  available; Klobuchar fallback otherwise
        //
        std::optional<GnssMath::PositionSolution> sppSolution;
        std::vector<GnssMath::PairedObservation> paired;
        int dualFreqUsed = 0;
        int singleFreqUsed = 0;

        // Leap seconds: prefer decoded, fall back to configured constant
        const int leapS = raw.leapSecDetermined ? raw.leapS : CURRENT_LEAP_S;

        if (l1Obs.size() >= 4 && navigation.pvt.fixType != EFixType::NoFix)
        {

            for (const auto& obs : l1Obs)
            {
                GnssMath::Ecef svPos;
                double clockBias = 0.0;
                bool hasSv = false;

                switch (obs.gnssId)
                {
                // ── GPS ───────────────────────────────────────────
                case EGnssId::GPS:
                {
                    auto eph = gpsEphStore.getEphemeris(obs.svId);
                    if (eph && eph->isHealthy())
                    {
                        const double transit = obs.prMes / GnssMath::C;
                        const double txTime = raw.rcvTow - transit;
                        svPos = GpsEphemeris::computeSvPosition(*eph, txTime);
                        svPos = GnssMath::earthRotationCorrection(svPos, transit);
                        clockBias = GpsEphemeris::computeSvClockBias(*eph, txTime);
                        hasSv = true;
                    }
                    break;
                }
                // ── Galileo ───────────────────────────────────────
                case EGnssId::Galileo:
                {
                    auto eph = galEphStore.getEphemeris(obs.svId);
                    if (eph && eph->isHealthy())
                    {
                        const double transit = obs.prMes / GnssMath::C;
                        // Galileo System Time ≈ GPS time (no offset)
                        const double txTime = raw.rcvTow - transit;
                        svPos = GalileoEphemeris::computeSvPosition(*eph, txTime);
                        svPos = GnssMath::earthRotationCorrection(svPos, transit);
                        clockBias = GalileoEphemeris::computeSvClockBias(*eph, txTime);
                        hasSv = true;
                    }
                    break;
                }
                // ── BeiDou ────────────────────────────────────────
                case EGnssId::BeiDou:
                {
                    auto eph = bdsEphStore.getEphemeris(obs.svId);
                    if (eph && eph->isHealthy())
                    {
                        const double transit = obs.prMes / GnssMath::C;
                        const double bdtTow = BeidouEphemeris::gpsTow2bdtTow(
                            raw.rcvTow - transit);
                        svPos = BeidouEphemeris::computeSvPosition(*eph, bdtTow);
                        svPos = GnssMath::earthRotationCorrection(svPos, transit);
                        clockBias = BeidouEphemeris::computeSvClockBias(*eph, bdtTow);
                        hasSv = true;
                    }
                    break;
                }
                // ── GLONASS ───────────────────────────────────────
                case EGnssId::GLONASS:
                {
                    auto eph = gloEphStore.getEphemeris(obs.svId);
                    if (eph && eph->isComplete() && eph->isHealthy())
                    {
                        const double transit = obs.prMes / GnssMath::C;
                        // Convert GPS reception time to GLONASS SOD
                        const double gloSod = GlonassEphemeris::gpsTow2GloSod(
                            raw.rcvTow - transit, leapS);
                        svPos = GlonassEphemeris::computeSvPosition(*eph, gloSod);
                        svPos = GnssMath::earthRotationCorrection(svPos, transit);
                        clockBias = GlonassEphemeris::computeSvClockBias(*eph, gloSod);
                        hasSv = true;
                    }
                    break;
                }
                default:
                    break;
                }

                if (hasSv)
                {
                    // Check if we have a dual-freq iono-free pseudorange
                    const uint16_t svKey =
                        (static_cast<uint16_t>(obs.gnssId) << 8) | obs.svId;
                    auto ifIt = ionoFreeMap.find(svKey);
                    std::optional<double> ionoFreePr;
                    if (ifIt != ionoFreeMap.end())
                    {
                        ionoFreePr = ifIt->second;
                        ++dualFreqUsed;
                    }
                    else
                    {
                        ++singleFreqUsed;
                    }

                    paired.push_back({
                        .obs = obs,
                        .sv  = GnssMath::SatelliteState {
                            .gnssId    = obs.gnssId,
                            .svId      = obs.svId,
                            .position  = svPos,
                            .clockBias = clockBias
                        },
                        .ionoFreePr = ionoFreePr,
                        .cpMeters   = std::nullopt,
                        .wavelength = 0.0,
                        .cpUsable   = false
                    });

                    // ── Validate carrier phase for this observation ──
                    auto& p = paired.back();
                    auto cpObs = CarrierPhase::validateCarrierPhase(obs);
                    if (cpObs)
                    {
                        bool arcOk = slipDetector.checkAndUpdate(
                            obs.gnssId, obs.svId, obs.sigId,
                            obs.cpMes, obs.doMes, obs.locktime,
                            raw.rcvTow);
                        if (arcOk)
                        {
                            p.cpMeters = cpObs->cpMeters;
                            p.wavelength = cpObs->wavelength;
                            p.cpUsable = true;
                        }
                        else
                        {
                            // Cycle slip detected — reset wide-lane for this SV
                            const uint16_t wlKey =
                                (static_cast<uint16_t>(obs.gnssId) << 8) | obs.svId;
                            wideLaneResolver.reset(wlKey);
                        }
                    }
                }
            }

            if (paired.size() >= 4)
            {
                GnssMath::Ecef guess = (lastSppEcef.x != 0)
                    ? lastSppEcef
                    : GnssMath::lla2ecef({
                          navigation.pvt.latitude * GnssMath::DEG2RAD,
                          navigation.pvt.longitude * GnssMath::DEG2RAD,
                          static_cast<double>(navigation.pvt.altitudeMSL)
                      });

                // Build atmospheric correction model
                const int doy = gpsDayOfYear(raw.week, raw.rcvTow);
                GnssMath::CorrectionModel corrections;

                // Troposphere: UNB3m
                corrections.troposphere =
                    [doy](const GnssMath::Lla& lla, double el, double, double) {
                        return Troposphere::troposphereDelay(
                            lla.lat_rad, lla.alt, el, doy);
                    };

                // Ionosphere: Klobuchar (if parameters decoded)
                auto ionoParams = gpsEphStore.getKlobucharParams();
                if (ionoParams)
                {
                    corrections.ionosphere =
                        [params = *ionoParams](const GnssMath::Lla& lla,
                                               double el, double az, double tow) {
                            return Ionosphere::klobucharDelay(
                                params, lla.lat_rad, lla.lon_rad, el, az, tow);
                        };
                }

                sppSolution = GnssMath::solvePosition(
                    paired, guess, 20, 1e-4, raw.rcvTow, corrections);

                if (sppSolution && sppSolution->converged)
                    lastSppEcef = sppSolution->ecef;

                // ── 2b. Carrier phase: float + fixed ambiguity resolution ─
                //
                // Build DualObservation list from paired observations
                // that have usable carrier phase. Then attempt float
                // solution followed by LAMBDA integer fixing.
                //
                if (sppSolution && sppSolution->converged)
                {
                    // Update Melbourne-Wübbena for dual-freq pairs
                    for (const auto& dp : pairing.paired)
                    {
                        const double f1 = DualFrequency::carrierFrequency(
                            dp.l1.gnssId, dp.l1.sigId, dp.l1.freqId);
                        const double f2 = DualFrequency::carrierFrequency(
                            dp.l2.gnssId, dp.l2.sigId, dp.l2.freqId);

                        auto mw = DualFrequency::melbourneWubbena(
                            dp.l1, dp.l2, f1, f2);
                        if (mw)
                        {
                            const uint16_t svKey =
                                (static_cast<uint16_t>(dp.l1.gnssId) << 8)
                                | dp.l1.svId;
                            constexpr double c = 299792458.0;
                            wideLaneResolver.update(svKey, *mw, c / (f1 - f2));
                        }
                    }

                    // Collect observations with usable carrier phase
                    std::vector<AmbiguityFloat::DualObservation> cpObs;
                    int ambIdx = 0;
                    for (const auto& p : paired)
                    {
                        if (!p.cpUsable || !p.cpMeters.has_value())
                            continue;

                        CarrierPhase::CpObservation cpData;
                        cpData.cpMeters = *p.cpMeters;
                        cpData.wavelength = p.wavelength;
                        cpData.frequency = GnssMath::C / p.wavelength;
                        cpData.locktime = p.obs.locktime;
                        cpData.cno = p.obs.cno;
                        cpData.valid = true;

                        cpObs.push_back({
                            .code = p,
                            .phase = cpData,
                            .ambiguityIndex = ambIdx++
                        });
                    }

                    // Need at least 5 SVs with carrier phase
                    // (4 unknowns + at least 1 ambiguity to resolve)
                    if (cpObs.size() >= 5)
                    {
                        auto floatSol = AmbiguityFloat::solveFloat(
                            cpObs, sppSolution->ecef, 20, 1e-4,
                            raw.rcvTow, corrections);

                        if (floatSol && floatSol->converged)
                        {
                            // Attempt LAMBDA integer fixing
                            auto fixedSol = AmbiguityLambda::fixAmbiguities(
                                *floatSol, 2.5);

                            if (fixedSol && fixedSol->accepted)
                            {
                                // Fixed solution accepted
                                sppSolution->ecef = fixedSol->ecef;
                                sppSolution->lla = fixedSol->lla;
                                sppSolution->receiverClockBias_m =
                                    fixedSol->receiverClockBias_m;
                                sppSolution->mode = GnssMath::SolutionMode::Fixed;
                                sppSolution->fixedAmbiguities = fixedSol->numFixed;
                                sppSolution->ratioTest = fixedSol->ratioTest;
                                lastSppEcef = fixedSol->ecef;
                            }
                            else
                            {
                                // Fall back to float solution
                                sppSolution->ecef = floatSol->ecef;
                                sppSolution->lla = floatSol->lla;
                                sppSolution->receiverClockBias_m =
                                    floatSol->receiverClockBias_m;
                                sppSolution->mode = GnssMath::SolutionMode::Float;
                                sppSolution->fixedAmbiguities =
                                    static_cast<int>(floatSol->ambiguities.size());
                                sppSolution->ratioTest =
                                    fixedSol ? fixedSol->ratioTest : 0.0;
                                lastSppEcef = floatSol->ecef;
                            }
                            sppSolution->hdop = floatSol->hdop;
                            sppSolution->vdop = floatSol->vdop;
                            sppSolution->pdop = floatSol->pdop;
                        }
                    }
                }
            }
        }

        // ── 3. Generate RTCM3 from raw observations ────────────────
        GnssMath::Ecef stationPos = {0, 0, 0};
        bool hasStationPos = false;

        if (sppSolution && sppSolution->converged)
        {
            stationPos = sppSolution->ecef;
            hasStationPos = true;
        }
        else if (navigation.pvt.fixType != EFixType::NoFix)
        {
            stationPos = GnssMath::lla2ecef({
                navigation.pvt.latitude * GnssMath::DEG2RAD,
                navigation.pvt.longitude * GnssMath::DEG2RAD,
                static_cast<double>(navigation.pvt.altitudeMSL)
            });
            hasStationPos = true;
        }

        auto rtcm3Out = Rtcm3Encode::encodeAllMsm4(
            STATION_ID, raw,
            hasStationPos ? &stationPos : nullptr);

        // ── 4. Broadcast RTCM3 to connected TCP clients ────────────
        if (!rtcm3Out.frames.empty())
        {
            rtcm3Sender.sendFrames(rtcm3Out.frames);
            rtcm3FramesSent += static_cast<int>(rtcm3Out.frames.size());
        }

        // ── 5. Display ─────────────────────────────────────────────
        int cpUsableCount = 0;
        for (const auto& p : paired)
            if (p.cpUsable) ++cpUsableCount;

        printStatus(navigation, sppSolution,
                    rtcm3Sender.clientCount(), rtcm3Out,
                    gpsEphStore, galEphStore, bdsEphStore, gloEphStore,
                    gpsEphStore.getKlobucharParams().has_value(),
                    dualFreqUsed, singleFreqUsed,
                    cpUsableCount, wideLaneResolver.resolvedCount(),
                    static_cast<int>(paired.size()));
    }

    showCursor();
    printf("\nShutting down...\n");

    rtcm3Receiver.stop();
    rtcm3Sender.stop();
    delete ubxHat;

    return 0;
}

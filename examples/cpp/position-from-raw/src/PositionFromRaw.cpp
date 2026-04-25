/*
 * Jimmy Paputto 2026
 *
 * PositionFromRaw — standalone GNSS position from raw UBX observations
 * and broadcast ephemeris decoded from UBX-RXM-SFRBX.
 *
 * Architecture (phase 1 scaffold):
 *
 *   GnssHat lib (Navigation) ──SFRBX──▶ per-GNSS decoders
 *                                        GpsL5Cnav | GalileoInav
 *                                        BeidouD1  | GlonassL1of
 *                                                │ write
 *                                                ▼
 *                                        EphemerisStore
 *                              RAWX ──▶  (pair for SPP — next phase)
 *                                   ──▶  RTCM3 TCP :2102
 *
 * Decoder bodies are stubbed (see GpsL5Cnav.hpp etc). This build
 * streams RTCM3, reports live SFRBX/RAWX stats, and will show per-GNSS
 * ephemeris counts once decoders are filled in.
 */

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <signal.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <jimmypaputto/GnssHat.hpp>

#include "GnssMath.hpp"
#include "EphemerisStore.hpp"
#include "GpsL5Cnav.hpp"
#include "GalileoInav.hpp"
#include "BeidouD1.hpp"
#include "GlonassL1of.hpp"
#include "Solver.hpp"
#include "Rtcm3Encoder.hpp"
#include "Rtcm3Tcp.hpp"

using namespace JimmyPaputto;

// ── Configuration ───────────────────────────────────────────────────
static constexpr uint16_t RTCM3_SEND_PORT = 2102;
static constexpr uint16_t STATION_ID      = 1;
static constexpr double   MIN_CNO         = 15.0;
static constexpr int      MEASUREMENT_RATE = 1;  // Hz

// ── Global state ────────────────────────────────────────────────────
static std::atomic<bool> running{true};
static std::atomic<int>  rtcm3FramesSent{0};

static void signalHandler(int) { running = false; }

// ── Terminal helpers ────────────────────────────────────────────────
#define RST   "\033[0m"
#define BOLD  "\033[1m"
#define GRN   "\033[32m"
#define YEL   "\033[33m"
#define CYN   "\033[36m"
#define RED   "\033[31m"

static void clearScreen() { printf("\033[2J\033[H"); }
static void hideCursor()  { printf("\033[?25l"); }
static void showCursor()  { printf("\033[?25h"); }

static int getTermWidth()
{
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_col ? w.ws_col : 80;
}

static void hline(int width, char c = '-')
{
    for (int i = 0; i < width; ++i) putchar(c);
}

static void printRow(const char* label, const char* fmt, ...)
{
    printf("  " CYN "%-30s" RST, label);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

// ── GNSS config ─────────────────────────────────────────────────────
static GnssConfig createConfig()
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
        .rtk = std::nullopt,
        .timing = std::nullopt,
        .saveToFlash = true,
        // Disable every chatty SPI message we do not consume
        // (NAV-SAT, NAV-DOP, NAV-GEOFENCE, MON-SPAN, MON-RF) so the
        // F9P TX-Ready threshold is only tripped by NAV-PVT,
        // RXM-RAWX and RXM-SFRBX. Reduces ~600 B/s of bus traffic
        // and prevents intermittent receiver overruns / dropped
        // frames during long raw-data sessions.
        .rawObservationsOnly = true,
    };
}

// ── Subframe → decoder dispatch ─────────────────────────────────────
//
// Signal IDs per UBX spec (u-blox F9 HPG L1L5 1.40 §1.5.4 p.20):
//   GPS:      L1C/A=0, L5 I=6, L5 Q=7
//   Galileo:  E1 B=1,  E5 bI=5
//   BeiDou:   B1I D1=0, B1Cd=6, B2ad=8
//   GLONASS:  L1OF=0
//
// F9P HPG L1L5 emits GPS CNAV on L5 (sigId=6), not LNAV on L1CA —
// we wire the L5 CNAV decoder for (gnssId=0, sigId=6).
//
struct Decoders
{
    GpsL5Cnav::Decoder   gps;
    GalileoInav::Decoder gal;
    BeidouD1::Decoder    bds;
    GlonassL1of::Decoder glo;
};

static void dispatchSubframe(const SubframeData& sf,
                             Decoders& dec,
                             EphemerisStore::Store& store)
{
    switch (sf.gnssId)
    {
        case EGnssId::GPS:
            if (sf.sigId == 6)
                dec.gps.feed(sf.svId, sf.words, store);
            break;
        case EGnssId::Galileo:
            if (sf.sigId == 1)
                dec.gal.feed(sf.svId, sf.words, store);
            break;
        case EGnssId::BeiDou:
            if (sf.sigId == 0)
                dec.bds.feed(sf.svId, sf.words, store);
            break;
        case EGnssId::GLONASS:
            if (sf.sigId == 0)
                dec.glo.feed(sf.svId, sf.freqId, sf.words, store);
            break;
        default:
            break;
    }
}

// ── Per-constellation observation count (for TUI) ───────────────────
struct ObsCount { int gps=0, gal=0, bds=0, glo=0, sbas=0; };

static ObsCount countObs(const RawMeasurements& raw)
{
    ObsCount c;
    for (const auto& o : raw.observations)
    {
        if (o.cno < MIN_CNO) continue;
        switch (o.gnssId)
        {
            case EGnssId::GPS:     ++c.gps;  break;
            case EGnssId::Galileo: ++c.gal;  break;
            case EGnssId::BeiDou:  ++c.bds;  break;
            case EGnssId::GLONASS: ++c.glo;  break;
            case EGnssId::SBAS:    ++c.sbas; break;
            default: break;
        }
    }
    return c;
}

// ── Display ────────────────────────────────────────────────────────
static void printStatus(const Navigation& nav,
                        const Decoders& dec,
                        const EphemerisStore::Store& store,
                        const Spp::Solution& spp,
                        int sfrbxTotalThisEpoch,
                        int rtcmClients)
{
    const int tw = getTermWidth();
    clearScreen();

    printf(BOLD GRN "+");
    hline(tw - 2, '-');
    printf("+\n");
    const char* title = "Position From Raw - SPP solver";
    int pad = (tw - (int)strlen(title) - 2) / 2;
    if (pad < 0) pad = 0;
    printf("|%*s%s%*s|\n", pad, "", title,
           tw - (int)strlen(title) - pad - 2, "");
    printf("+");
    hline(tw - 2, '-');
    printf("+" RST "\n\n");

    const auto& pvt = nav.pvt;
    printf(BOLD " Receiver PVT (reference):" RST "\n");
    printRow("Fix Type",     YEL "%s" RST,
             Utils::eFixType2string(pvt.fixType).c_str());
    printRow("Latitude",     YEL "%.8f deg" RST, pvt.latitude);
    printRow("Longitude",    YEL "%.8f deg" RST, pvt.longitude);
    printRow("Altitude MSL", YEL "%.2f m" RST, pvt.altitudeMSL);
    printRow("Visible SVs",  YEL "%u" RST, pvt.visibleSatellites);
    printf("\n");

    const auto& raw = nav.rawMeasurements;
    const auto c = countObs(raw);
    printf(BOLD " Raw Observations:" RST "\n");
    printRow("GPS Week",     YEL "%u" RST, raw.week);
    printRow("Receiver TOW", YEL "%.3f s" RST, raw.rcvTow);
    printRow("Total Meas",   YEL "%u" RST, raw.numMeas);
    printRow("Leap Seconds", YEL "%d" RST, raw.leapS);
    printRow("G / E / C / R / S",
             YEL "%d / %d / %d / %d / %d" RST,
             c.gps, c.gal, c.bds, c.glo, c.sbas);
    printf("\n");

    printf(BOLD " Subframe Decoders:" RST "\n");
    printRow("SFRBX this epoch", YEL "%d" RST, sfrbxTotalThisEpoch);

    // Diagnostic: histogram of (gnss, sig, numWords) in subframe buffer
    {
        struct Key { uint8_t g, s, nw; bool operator==(const Key& o) const { return g==o.g && s==o.s && nw==o.nw; } };
        std::vector<std::pair<Key,int>> hist;
        for (const auto& sf : nav.subframeBuffer.subframes)
        {
            Key k{static_cast<uint8_t>(sf.gnssId), sf.sigId, sf.numWords};
            bool found = false;
            for (auto& p : hist) if (p.first == k) { ++p.second; found = true; break; }
            if (!found) hist.push_back({k, 1});
        }
        std::string s;
        for (auto& p : hist)
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "g%u/s%u/nw%u=%d ",
                     p.first.g, p.first.s, p.first.nw, p.second);
            s += buf;
        }
        printRow("SFRBX buffer mix", YEL "%s" RST, s.c_str());
    }

    printRow("GPS  L5 CNAV  MT10/11/30",
             YEL "%zu / %zu / %zu  crc-fail %zu  pre-fail %zu" RST,
             dec.gps.mt10Count, dec.gps.mt11Count, dec.gps.mt30Count,
             dec.gps.crcFails, dec.gps.preambleFails);
    printRow("GAL  E1-B I/NAV pages",
             YEL "%zu  crc-fail %zu" RST,
             dec.gal.pageCount, dec.gal.crcFails);
    printRow("BDS  B1I D1 subframes",
             YEL "%zu  SF1/2/3 %zu/%zu/%zu  null %zu  parity-fail %zu" RST,
             dec.bds.subframeCount,
             dec.bds.sf1Count, dec.bds.sf2Count, dec.bds.sf3Count,
             dec.bds.nullFrames, dec.bds.parityFails);
    printRow("GLO  L1OF strings",
             YEL "%zu  S1/2/3/4 %zu/%zu/%zu/%zu  parity-fail %zu" RST,
             dec.glo.stringCount,
             dec.glo.s1Count, dec.glo.s2Count, dec.glo.s3Count, dec.glo.s4Count,
             dec.glo.parityFails);
    printRow("Ephemerides (valid)",
             YEL "G=%zu  E=%zu  C=%zu  R=%zu  total=%zu" RST,
             store.countValid(EGnssId::GPS),
             store.countValid(EGnssId::Galileo),
             store.countValid(EGnssId::BeiDou),
             store.countValid(EGnssId::GLONASS),
             store.countValid());
    printf("\n");

    printf(BOLD " SPP Solution:" RST "\n");
    if (spp.valid)
    {
        printRow("Status",
                 GRN "FIX  used %d  iter %d  rms %.2f m" RST,
                 spp.nUsed, spp.iterations, spp.rmsResidual);
        printRow("Latitude  (SPP)", YEL "%.8f deg" RST,
                 spp.lla.lat_rad * GnssMath::RAD2DEG);
        printRow("Longitude (SPP)", YEL "%.8f deg" RST,
                 spp.lla.lon_rad * GnssMath::RAD2DEG);
        printRow("Altitude  (SPP)", YEL "%.2f m (HAE)" RST, spp.lla.alt);
        printRow("Rx clock G/E/C/R",
                 YEL "%.0f / %.0f / %.0f / %.0f m" RST,
                 spp.clkG, spp.clkE, spp.clkC, spp.clkR);
    }
    else
    {
        printRow("Status",
                 RED "no fix" RST " — cand %d  used %d  dropped %d",
                 spp.nCandidates, spp.nUsed, spp.nDropped);
    }
    printf("\n");

    printf(BOLD " RTCM3 Output (port %u):" RST "\n", RTCM3_SEND_PORT);
    printRow("Connected clients", YEL "%d" RST, rtcmClients);
    printRow("Total frames sent", YEL "%d" RST, rtcm3FramesSent.load());
    printf("\n");

    printf("  Press Ctrl+C to exit\n");
}

// ── Main ───────────────────────────────────────────────────────────
int main()
{
    signal(SIGINT, signalHandler);

    auto* hat = IGnssHat::create();
    if (!hat)
    {
        fprintf(stderr, "Failed to create GNSS HAT\n");
        return 1;
    }

    if (!hat->start(createConfig()))
    {
        fprintf(stderr, "Failed to start receiver\n");
        delete hat;
        return 1;
    }

    Decoders              decoders;
    EphemerisStore::Store store;
    GnssMath::Ecef        lastSppEcef{0,0,0};

    Rtcm3Tcp::Sender rtcm3Sender(RTCM3_SEND_PORT);
    rtcm3Sender.start();

    hideCursor();

    while (running)
    {
        const auto nav = hat->waitAndGetFreshNavigation();
        const auto& raw = nav.rawMeasurements;
        if (raw.observations.empty())
            continue;

        const int nSf = static_cast<int>(nav.subframeBuffer.subframes.size());
        for (const auto& sf : nav.subframeBuffer.subframes)
            dispatchSubframe(sf, decoders, store);

        // SPP fix from raw + ephemerides. Seed each call from the
        // freshest available position estimate: prefer the receiver's
        // own PVT (always near-truth and fresh each epoch); fall back
        // to the previous SPP solution; finally to origin (which the
        // solver tolerates after the first PVT-seeded success).
        GnssMath::Ecef seed = lastSppEcef;
        if (nav.pvt.fixType != EFixType::NoFix)
        {
            seed = GnssMath::lla2ecef({
                nav.pvt.latitude  * GnssMath::DEG2RAD,
                nav.pvt.longitude * GnssMath::DEG2RAD,
                static_cast<double>(nav.pvt.altitudeMSL)
            });
        }
        const Spp::Solution spp = Spp::solve(raw, store, seed);
        if (spp.valid) lastSppEcef = spp.ecef;

        GnssMath::Ecef stationPos {0, 0, 0};
        bool haveStation = false;
        if (spp.valid)
        {
            stationPos = spp.ecef;
            haveStation = true;
        }
        else if (nav.pvt.fixType != EFixType::NoFix)
        {
            stationPos = GnssMath::lla2ecef({
                nav.pvt.latitude  * GnssMath::DEG2RAD,
                nav.pvt.longitude * GnssMath::DEG2RAD,
                static_cast<double>(nav.pvt.altitudeMSL)
            });
            haveStation = true;
        }
        auto rtcm = Rtcm3Encode::encodeAllMsm4(
            STATION_ID, raw, haveStation ? &stationPos : nullptr);
        if (!rtcm.frames.empty())
        {
            rtcm3Sender.sendFrames(rtcm.frames);
            rtcm3FramesSent += static_cast<int>(rtcm.frames.size());
        }

        printStatus(nav, decoders, store, spp, nSf, rtcm3Sender.clientCount());
    }

    showCursor();
    printf("\nShutting down...\n");

    rtcm3Sender.stop();
    delete hat;
    return 0;
}

/*
 * Jimmy Paputto 2026
 */

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <signal.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <jimmypaputto/GnssHat.hpp>


using namespace JimmyPaputto;

std::atomic<bool> running{true};

void signalHandler(int signal)
{
    if (signal == SIGINT)
        running = false;
}

void clearScreen()
{
    printf("\033[2J\033[H");
}

void hideCursor()
{
    printf("\033[?25l");
}

void showCursor()
{
    printf("\033[?25h");
}

void moveCursor(const int row, int col)
{
    printf("\033[%d;%dH", row, col);
}

#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_DIM     "\033[2m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_RED     "\033[31m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_WHITE   "\033[37m"

#define BG_RED        "\033[41m"
#define BG_GREEN      "\033[42m"
#define BG_YELLOW     "\033[43m"
#define BG_BLUE       "\033[44m"
#define BG_MAGENTA    "\033[45m"
#define BG_CYAN       "\033[46m"

void getTerminalSize(int& width, int& height)
{
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    width = w.ws_col;
    height = w.ws_row;
}

void drawHorizontalLine(int width, char c = '-')
{
    for (int i = 0; i < width; i++)
        printf("%c", c);
}

struct ConstellationStyle
{
    const char* color;
    const char* abbrev;
    const char* name;
};

ConstellationStyle getConstellationStyle(EGnssId id)
{
    switch (id)
    {
        case EGnssId::GPS:     return { COLOR_GREEN,   "GP", "GPS"     };
        case EGnssId::SBAS:    return { COLOR_YELLOW,  "SB", "SBAS"    };
        case EGnssId::Galileo: return { COLOR_BLUE,    "GA", "Galileo" };
        case EGnssId::BeiDou:  return { COLOR_RED,     "BD", "BeiDou"  };
        case EGnssId::IMES:    return { COLOR_WHITE,   "IM", "IMES"    };
        case EGnssId::QZSS:    return { COLOR_MAGENTA, "QZ", "QZSS"    };
        case EGnssId::GLONASS: return { COLOR_CYAN,    "GL", "GLONASS" };
        default:               return { COLOR_WHITE,   "??", "Unknown" };
    }
}

const char* svQualityToString(ESvQuality quality)
{
    switch (quality)
    {
        case ESvQuality::NoSignal:                      return "No Signal";
        case ESvQuality::Searching:                     return "Searching";
        case ESvQuality::SignalAcquired:                 return "Acquired";
        case ESvQuality::SignalDetectedButUnusable:      return "Unusable";
        case ESvQuality::CodeLockedAndTimeSynchronized:  return "Code Lock";
        case ESvQuality::CodeAndCarrierLocked1:          return "Carrier 1";
        case ESvQuality::CodeAndCarrierLocked2:          return "Carrier 2";
        case ESvQuality::CodeAndCarrierLocked3:          return "Carrier 3";
        default:                                         return "Unknown";
    }
}

void printSignalBar(uint8_t cno, int barWidth = 20)
{
    // C/N0 typically ranges 0-55 dBHz
    constexpr int maxCno = 50;
    int filled = static_cast<int>((static_cast<float>(
        std::min<int>(cno, maxCno)) / maxCno) * barWidth);

    const char* barColor;
    if (cno >= 40)
        barColor = COLOR_GREEN;
    else if (cno >= 25)
        barColor = COLOR_YELLOW;
    else if (cno >= 10)
        barColor = COLOR_RED;
    else
        barColor = COLOR_DIM;

    printf("%s", barColor);
    for (int i = 0; i < barWidth; i++)
    {
        if (i < filled)
            printf("\u2588");  // Full block
        else
            printf("\u2591");  // Light shade
    }
    printf(COLOR_RESET);
}

struct ConstellationStats
{
    uint8_t total = 0;
    uint8_t used = 0;
    uint8_t healthy = 0;
    float avgCno = 0.0f;
};

void printSatelliteTable(const Navigation& navigation)
{
    int termWidth, termHeight;
    getTerminalSize(termWidth, termHeight);

    clearScreen();

    // --- Header ---
    printf(COLOR_BOLD COLOR_GREEN);
    printf("+");
    drawHorizontalLine(termWidth - 2, '-');
    printf("+\n");

    std::string title = "GNSS HAT Satellite Monitor";
    int padding = (termWidth - (int)title.length() - 2) / 2;
    printf("|%*s%s%*s|\n", padding, "", title.c_str(),
           termWidth - (int)title.length() - padding - 2, "");

    std::string timestamp = "Updated: " +
        Utils::utcTimeFromGnss_ISO8601(navigation.pvt);
    padding = (termWidth - (int)timestamp.length() - 2) / 2;
    printf("|%*s%s%*s|\n", padding, "", timestamp.c_str(),
           termWidth - (int)timestamp.length() - padding - 2, "");

    printf("+");
    drawHorizontalLine(termWidth - 2, '-');
    printf("+\n");
    printf(COLOR_RESET);

    const auto& satellites = navigation.satellites;

    // --- Summary bar ---
    uint8_t totalUsed = 0;
    std::map<EGnssId, ConstellationStats> statsMap;
    for (const auto& sat : satellites)
    {
        auto& stats = statsMap[sat.gnssId];
        stats.total++;
        if (sat.usedInFix) { stats.used++; totalUsed++; }
        if (sat.healthy)   stats.healthy++;
        stats.avgCno += sat.cno;
    }
    for (auto& [id, stats] : statsMap)
    {
        if (stats.total > 0)
            stats.avgCno /= stats.total;
    }

    printf(" " COLOR_BOLD "Fix: " COLOR_RESET "%s" COLOR_RESET
           "  |  " COLOR_BOLD "Satellites: " COLOR_RESET "%d tracked, "
           COLOR_GREEN "%d used in fix" COLOR_RESET "\n\n",
        Utils::eFixQuality2string(navigation.pvt.fixQuality).c_str(),
        (int)satellites.size(), totalUsed);

    // --- Constellation summary ---
    printf(" " COLOR_BOLD "Constellation Summary:" COLOR_RESET "\n");
    printf(" ");
    for (const auto& [id, stats] : statsMap)
    {
        auto style = getConstellationStyle(id);
        printf(" %s%s%-8s%s %d/%d",
            COLOR_BOLD, style.color, style.name, COLOR_RESET,
            stats.used, stats.total);
    }
    printf("\n\n");

    // --- Satellite table header ---
    printf(" " COLOR_BOLD
        "%-4s %-8s %4s  %6s  %5s  %6s  %-11s  %-4s  %-4s  %-22s"
        COLOR_RESET "\n",
        "SYS", "SV", "C/N0", "Elev", "Azim", "Status", "Quality",
        "Fix", "Hlth", "Signal Strength");

    printf(" ");
    drawHorizontalLine(termWidth - 3, '-');
    printf("\n");

    // --- Sort satellites: used first, then by constellation, then by C/N0 descending ---
    auto sortedSats = satellites;
    std::sort(sortedSats.begin(), sortedSats.end(),
        [](const SatelliteInfo& a, const SatelliteInfo& b)
        {
            if (a.usedInFix != b.usedInFix)
                return a.usedInFix > b.usedInFix;
            if (a.gnssId != b.gnssId)
                return static_cast<uint8_t>(a.gnssId) <
                       static_cast<uint8_t>(b.gnssId);
            return a.cno > b.cno;
        });

    for (const auto& sat : sortedSats)
    {
        auto style = getConstellationStyle(sat.gnssId);

        // Satellite identifier, e.g. "G12", "E05", "R24"
        char svLabel[8];
        snprintf(svLabel, sizeof(svLabel), "%s%03d", style.abbrev, sat.svId);

        // Status flags
        const char* fixMark  = sat.usedInFix ?
                               (COLOR_GREEN "YES " COLOR_RESET) :
                               (COLOR_DIM "no  " COLOR_RESET);
        const char* healthMark = sat.healthy ?
                                 (COLOR_GREEN "OK  " COLOR_RESET) :
                                 (COLOR_RED "BAD " COLOR_RESET);

        printf(
            " %s%-4s" COLOR_RESET
            " %s%-8s" COLOR_RESET
            " %3d"
            "   %4d°"
            "  %4d°"
            "  %s%-6s" COLOR_RESET
            "  %-11s"
            "  %s  %s  ",
            style.color, style.abbrev,
            COLOR_BOLD, svLabel,
            sat.cno,
            sat.elevation,
            sat.azimuth,
            sat.usedInFix ? COLOR_GREEN : COLOR_DIM,
            sat.usedInFix ? "USED" : "---",
            svQualityToString(sat.quality),
            fixMark,
            healthMark
        );

        printSignalBar(sat.cno, 20);
        printf(" %d dBHz\n", sat.cno);
    }

    // --- Footer ---
    printf("\n ");
    drawHorizontalLine(termWidth - 3, '-');
    printf("\n");

    // --- Signal quality legend ---
    printf(" " COLOR_BOLD "Signal: " COLOR_RESET);
    printf(COLOR_GREEN  "\u2588\u2588 >40" COLOR_RESET "  ");
    printf(COLOR_YELLOW "\u2588\u2588 25-40" COLOR_RESET "  ");
    printf(COLOR_RED    "\u2588\u2588 10-25" COLOR_RESET "  ");
    printf(COLOR_DIM    "\u2588\u2588 <10 dBHz" COLOR_RESET);

    // --- Feature flags ---
    uint8_t ephCount = 0, almCount = 0, diffCount = 0;
    for (const auto& sat : satellites)
    {
        if (sat.ephAvail) ephCount++;
        if (sat.almAvail) almCount++;
        if (sat.diffCorr) diffCount++;
    }
    printf("  |  EPH: %d  ALM: %d  DGPS: %d", ephCount, almCount, diffCount);
    printf("\n");

    printf(COLOR_BOLD " Press Ctrl+C to exit\n" COLOR_RESET);

    fflush(stdout);
}

GnssConfig createDefaultConfig()
{
    return GnssConfig {
        .measurementRate_Hz = 1,
        .dynamicModel = EDynamicModel::Stationary,
        .timepulsePinConfig = TimepulsePinConfig {
            .active = true,
            .fixedPulse = TimepulsePinConfig::Pulse { 1, 0.1 },
            .pulseWhenNoFix = std::nullopt,
            .polarity = ETimepulsePinPolarity::RisingEdgeAtTopOfSecond
        },
        .geofencing = std::nullopt,
        .rtk = std::nullopt
    };
}

auto main() -> int
{
    signal(SIGINT, signalHandler);

    hideCursor();

    auto* ubxHat = IGnssHat::create();
    if (!ubxHat)
    {
        showCursor();
        printf("Failed to create GNSS HAT instance\r\n");
        return -1;
    }

    ubxHat->softResetUbloxSom_HotStart();

    clearScreen();
    printf(COLOR_BOLD COLOR_GREEN "Initializing GNSS...\n" COLOR_RESET);
    printf("Please wait for startup to complete...\n");

    const bool isStartupDone = ubxHat->start(createDefaultConfig());
    if (!isStartupDone)
    {
        showCursor();
        printf(COLOR_RED "Startup failed, exit\n" COLOR_RESET);
        return -1;
    }

    while (running)
    {
        const auto navigation = ubxHat->navigation();
        printSatelliteTable(navigation);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    showCursor();
    clearScreen();
    printf(COLOR_RED "Satellite monitoring stopped gracefully.\n" COLOR_RESET);
    delete ubxHat;

    return 0;
}

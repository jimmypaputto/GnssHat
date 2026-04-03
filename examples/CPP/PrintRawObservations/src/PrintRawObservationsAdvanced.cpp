/*
 * Jimmy Paputto 2026
 */

#include <atomic>
#include <chrono>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>

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
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_RED     "\033[31m"
#define COLOR_MAGENTA "\033[35m"

void getTerminalSize(int& width, int& height)
{
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    width = w.ws_col;
    height = w.ws_row;
}

std::string formatDouble(double value, int precision = 6)
{
    std::stringstream ss;
    ss << std::fixed << std::setprecision(precision) << value;
    return ss.str();
}

std::size_t getVisualLength(const std::string& str)
{
    size_t length = 0;
    for (size_t i = 0; i < str.length(); )
    {
        unsigned char c = str[i];
        if (c < 0x80)
        {
            length++;
            i++;
        }
        else if ((c & 0xE0) == 0xC0)
        {
            length++;
            i += 2;
        }
        else if ((c & 0xF0) == 0xE0)
        {
            length++;
            i += 3;
        }
        else if ((c & 0xF8) == 0xF0)
        {
            length++;
            i += 4;
        }
        else
        {
            length++;
            i++;
        }
    }
    return length;
}

void drawHorizontalLine(int width, char c = '-')
{
    for (int i = 0; i < width; i++)
        printf("%c", c);
}

void printTableRow(const std::string& label, const std::string& value, int labelWidth = 25)
{
    size_t valueVisualLength = getVisualLength(value);
    size_t valuePadding = 40;

    const int extraBytes = value.length() - valueVisualLength;

    printf("| " COLOR_CYAN "%-*s" COLOR_RESET " | " COLOR_YELLOW "%-*s" COLOR_RESET " |\n",
           labelWidth, label.c_str(),
           (int)(valuePadding + extraBytes), value.c_str());
}

void printObservationHeader()
{
    printf("| " COLOR_BOLD
           "%-10s %-4s %-4s %-4s %-6s %-16s %-16s %-14s %-6s %-4s %-4s %-5s"
           COLOR_RESET " |\n",
           "System", "SV", "Sig", "Freq", "C/N0", "PR (m)", "CP (cyc)",
           "Doppler (Hz)", "Lock", "PR?", "CP?", "HlfCy");
}

void printObservationSeparator()
{
    printf("|");
    drawHorizontalLine(99, '-');
    printf("|\n");
}

void printObservationRow(const RawObservation& obs)
{
    const char* color;
    if (obs.cno >= 35)
        color = COLOR_GREEN;
    else if (obs.cno >= 20)
        color = COLOR_YELLOW;
    else
        color = COLOR_RED;

    printf("| %s%-10s %-4u %-4u %-4u %-4u dB %-16.3f %-16.3f %-14.3f %-6u %-4s %-4s %-5s" COLOR_RESET " |\n",
           color,
           Utils::gnssId2string(obs.gnssId).c_str(),
           obs.svId,
           obs.sigId,
           obs.freqId,
           obs.cno,
           obs.prMes,
           obs.cpMes,
           static_cast<double>(obs.doMes),
           obs.locktime,
           obs.prValid ? "Y" : "N",
           obs.cpValid ? "Y" : "N",
           obs.halfCyc ? "Y" : "N");
}

void printRawObservationsTable(const Navigation& navigation)
{
    int termWidth, termHeight;
    getTerminalSize(termWidth, termHeight);

    clearScreen();

    const auto& raw = navigation.rawMeasurements;
    const auto& observations = raw.observations;

    printf(COLOR_BOLD COLOR_GREEN);
    printf("+");
    drawHorizontalLine(termWidth - 2, '-');
    printf("+\n");

    std::string title = "u-blox NEO-M9N Raw Observations (UBX-RXM-RAWX)";
    int padding = (termWidth - title.length() - 2) / 2;
    printf("|%*s%s%*s|\n", padding, "", title.c_str(),
           termWidth - (int)title.length() - padding - 2, "");

    std::string timestamp = "Updated: " + Utils::utcTimeFromGnss_ISO8601(navigation.pvt);
    padding = (termWidth - timestamp.length() - 2) / 2;
    printf("|%*s%s%*s|\n", padding, "", timestamp.c_str(),
           termWidth - (int)timestamp.length() - padding - 2, "");

    printf("+");
    drawHorizontalLine(termWidth - 2, '-');
    printf("+\n");
    printf(COLOR_RESET);

    printf("| " COLOR_BOLD "Parameter" COLOR_RESET "%*s | " COLOR_BOLD "Value" COLOR_RESET "%*s |\n",
           16, "", 35, "");
    printf("+");
    drawHorizontalLine(27, '-');
    printf("+");
    drawHorizontalLine(42, '-');
    printf("+\n");

    printf("| " COLOR_BOLD COLOR_MAGENTA "RECEIVER STATUS" COLOR_RESET "%*s |%*s|\n", 10, "", 42, "");
    printf("+");
    drawHorizontalLine(27, '-');
    printf("+");
    drawHorizontalLine(42, '-');
    printf("+\n");

    printTableRow("Receiver TOW", formatDouble(raw.rcvTow, 3) + " s");
    printTableRow("GPS Week", std::to_string(raw.week));
    printTableRow("Leap Seconds", std::to_string(raw.leapS));
    printTableRow("Num Measurements", std::to_string(raw.numMeas));
    printTableRow("Message Version", std::to_string(raw.version));
    printTableRow("Leap Sec Determined", raw.leapSecDetermined ? "Yes" : "No");
    printTableRow("Clock Reset", raw.clkReset ? "Yes" : "No");
    printTableRow("Observations", std::to_string(observations.size()));

    printf("+");
    drawHorizontalLine(termWidth - 2, '-');
    printf("+\n");
    printf("| " COLOR_BOLD COLOR_MAGENTA "OBSERVATIONS" COLOR_RESET "%*s|\n",
           termWidth - 16, "");
    printf("+");
    drawHorizontalLine(termWidth - 2, '-');
    printf("+\n");

    printObservationHeader();
    printObservationSeparator();

    for (const auto& obs : observations)
    {
        printObservationRow(obs);
    }

    printObservationSeparator();

    printf("+");
    drawHorizontalLine(termWidth - 2, '-');
    printf("+\n");

    printf(COLOR_BOLD "Press Ctrl+C to exit\n" COLOR_RESET);

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
    ubxHat->softResetUbloxSom_HotStart();

    clearScreen();
    printf(COLOR_BOLD COLOR_GREEN "Initializing u-blox NEO-M9N GNSS...\n" COLOR_RESET);
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
        printRawObservationsTable(navigation);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    showCursor();
    clearScreen();
    printf(COLOR_RED "GNSS monitoring stopped gracefully.\n" COLOR_RESET);
    delete ubxHat;

    return 0;
}

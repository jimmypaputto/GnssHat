/*
 * Jimmy Paputto 2025
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
            // ASCII character
            length++;
            i++;
        }
        else if ((c & 0xE0) == 0xC0)
        {
            // 2-byte UTF-8 character
            length++;
            i += 2;
        }
        else if ((c & 0xF0) == 0xE0)
        {
            // 3-byte UTF-8 character (like °)
            length++;
            i += 3;
        }
        else if ((c & 0xF8) == 0xF0)
        {
            // 4-byte UTF-8 character
            length++;
            i += 4;
        }
        else
        {
            // Invalid UTF-8, treat as single byte
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

void printNavigationTable(const Navigation& navigation)
{
    int termWidth, termHeight;
    getTerminalSize(termWidth, termHeight);

    clearScreen();
  
    printf(COLOR_BOLD COLOR_GREEN);
    printf("+");
    drawHorizontalLine(termWidth - 2, '-');
    printf("+\n");

    std::string title = "u-blox NEO-M9N GNSS Navigation Data";
    int padding = (termWidth - title.length() - 2) / 2;
    printf("|%*s%s%*s|\n", padding, "", title.c_str(), 
           termWidth - title.length() - padding - 2, "");

    std::string timestamp = "Updated: " + Utils::utcTimeFromGnss_ISO8601(navigation.pvt);
    padding = (termWidth - timestamp.length() - 2) / 2;
    printf("|%*s%s%*s|\n", padding, "", timestamp.c_str(), 
           termWidth - timestamp.length() - padding - 2, "");

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
    
    printf("| " COLOR_BOLD COLOR_MAGENTA "FIX STATUS" COLOR_RESET "%*s |%*s|\n", 15, "", 42, "");
    printf("+");
    drawHorizontalLine(27, '-');
    printf("+");
    drawHorizontalLine(42, '-');
    printf("+\n");
    
    printTableRow("Fix Quality", Utils::eFixQuality2string(navigation.pvt.fixQuality));
    printTableRow("Fix Status", Utils::eFixStatus2string(navigation.pvt.fixStatus));
    printTableRow("Fix Type", Utils::eFixType2string(navigation.pvt.fixType));
    printTableRow("Visible Satellites", std::to_string(navigation.pvt.visibleSatellites));
    printTableRow("Time Accuracy", std::to_string(navigation.pvt.utc.accuracy) + " nanoseconds");
    
    printf("+");
    drawHorizontalLine(27, '-');
    printf("+");
    drawHorizontalLine(42, '-');
    printf("+\n");
    printf("| " COLOR_BOLD COLOR_MAGENTA "POSITION" COLOR_RESET "%*s |%*s|\n", 17, "", 42, "");
    printf("+");
    drawHorizontalLine(27, '-');
    printf("+");
    drawHorizontalLine(42, '-');
    printf("+\n");
    
    printTableRow("Latitude", formatDouble(navigation.pvt.latitude, 8) + "°");
    printTableRow("Longitude", formatDouble(navigation.pvt.longitude, 8) + "°");
    printTableRow("Altitude", formatDouble(navigation.pvt.altitude, 2) + " m");
    printTableRow("Altitude MSL", formatDouble(navigation.pvt.altitudeMSL, 2) + " m");
    printTableRow("Horizontal Accuracy", formatDouble(navigation.pvt.horizontalAccuracy, 2) + " m");
    printTableRow("Vertical Accuracy", formatDouble(navigation.pvt.verticalAccuracy, 2) + " m");
    
    printf("+");
    drawHorizontalLine(27, '-');
    printf("+");
    drawHorizontalLine(42, '-');
    printf("+\n");
    printf("| " COLOR_BOLD COLOR_MAGENTA "MOTION" COLOR_RESET "%*s |%*s|\n", 19, "", 42, "");
    printf("+");
    drawHorizontalLine(27, '-');
    printf("+");
    drawHorizontalLine(42, '-');
    printf("+\n");
    
    printTableRow("Speed Over Ground", formatDouble(navigation.pvt.speedOverGround, 2) + " m/s");
    printTableRow("Speed Accuracy", formatDouble(navigation.pvt.speedAccuracy, 2) + " m/s");
    printTableRow("Heading", formatDouble(navigation.pvt.heading, 2) + "°");
    printTableRow("Heading Accuracy", formatDouble(navigation.pvt.headingAccuracy, 2) + "°");
    
    printf("+");
    drawHorizontalLine(27, '-');
    printf("+");
    drawHorizontalLine(42, '-');
    printf("+\n");
    printf("| " COLOR_BOLD COLOR_MAGENTA "DILUTION OF PRECISION" COLOR_RESET "%*s |%*s|\n", 4, "", 42, "");
    printf("+");
    drawHorizontalLine(27, '-');
    printf("+");
    drawHorizontalLine(42, '-');
    printf("+\n");
    
    printTableRow("Geometric DOP", formatDouble(navigation.dop.geometric, 2));
    printTableRow("Position DOP", formatDouble(navigation.dop.position, 2));
    printTableRow("Time DOP", formatDouble(navigation.dop.time, 2));
    printTableRow("Vertical DOP", formatDouble(navigation.dop.vertical, 2));
    printTableRow("Horizontal DOP", formatDouble(navigation.dop.horizontal, 2));
    printTableRow("Northing DOP", formatDouble(navigation.dop.northing, 2));
    printTableRow("Easting DOP", formatDouble(navigation.dop.easting, 2));
    
    printf("+");
    drawHorizontalLine(27, '-');
    printf("+");
    drawHorizontalLine(42, '-');
    printf("+\n");
    printf("| " COLOR_BOLD COLOR_MAGENTA "RF MONITOR (L1 BAND)" COLOR_RESET "%*s |%*s|\n", 5, "", 42, "");
    printf("+");
    drawHorizontalLine(27, '-');
    printf("+");
    drawHorizontalLine(42, '-');
    printf("+\n");
    
    if (!navigation.rfBlocks.empty())
    {
        printTableRow("Noise Level", std::to_string(navigation.rfBlocks[0].noisePerMS) + " counts/ms");
        printTableRow("AGC Monitor", std::to_string(navigation.rfBlocks[0].agcMonitor) + "%");
        printTableRow("Jamming State", Utils::jammingState2string(navigation.rfBlocks[0].jammingState));
        printTableRow("Antenna Status", Utils::antennaStatus2string(navigation.rfBlocks[0].antennaStatus));
        printTableRow("CW Interference", std::to_string(navigation.rfBlocks[0].cwInterferenceSuppressionLevel) + "%");
    }
    else
    {
        printTableRow("Status", "No RF blocks available");
    }

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
        .geofencing = std::nullopt
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
        printNavigationTable(navigation);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    showCursor();
    clearScreen();
    printf(COLOR_RED "GNSS monitoring stopped gracefully.\n" COLOR_RESET);
    delete ubxHat;

    return 0;
}

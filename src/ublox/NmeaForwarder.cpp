/*
 * Jimmy Paputto 2025
 */

#include "ublox/NmeaForwarder.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <chrono>


namespace JimmyPaputto
{

NmeaForwarder::NmeaForwarder()
:   masterFd_(-1),
    slaveFd_(-1)
{
}

NmeaForwarder::~NmeaForwarder()
{
    stopForwarding();

    if (masterFd_ >= 0)
        close(masterFd_);
    if (slaveFd_ >= 0)
        close(slaveFd_);
}

bool NmeaForwarder::createVirtualTty()
{
    char slaveName[256];
    if (openpty(&masterFd_, &slaveFd_, slaveName, nullptr, nullptr) < 0)
    {
        fprintf(
            stderr,
            "[NmeaForwarder] Failed to create PTY: %s\r\n",
            strerror(errno)
        );
        return false;
    }

    struct termios tty;
    if (tcgetattr(slaveFd_, &tty) < 0)
    {
        fprintf(
            stderr,
            "[NmeaForwarder] Failed to get terminal attributes: %s\r\n",
            strerror(errno)
        );
        close(masterFd_);
        close(slaveFd_);
        return false;
    }

    cfsetispeed(&tty, B9600);
    cfsetospeed(&tty, B9600);
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag &= ~IGNBRK;
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 5;
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr(slaveFd_, TCSANOW, &tty) < 0)
    {
        fprintf(
            stderr,
            "[NmeaForwarder] Failed to set terminal attributes: %s\r\n",
            strerror(errno)
        );
        close(masterFd_);
        close(slaveFd_);
        return false;
    }

    tcgetattr(masterFd_, &tty);
    cfmakeraw(&tty);
    if (tcsetattr(masterFd_, TCSANOW, &tty) < 0)
    {
        fprintf(
            stderr,
            "[NmeaForwarder] Failed to set terminal attributes: %s\r\n",
            strerror(errno)
        );
        close(masterFd_);
        close(slaveFd_);
        return false;
    }

    int flags = fcntl(masterFd_, F_GETFL);
    if (flags >= 0)
    {
        fcntl(masterFd_, F_SETFL, flags | O_NONBLOCK);
    }

    devicePath_ = "/dev/jimmypaputto/gnss";
    unlink(devicePath_.c_str());

    if (mkdir("/dev/jimmypaputto", 0755) != 0 && errno != EEXIST)
    {
        fprintf(
            stderr,
            "[NmeaForwarder] FATAL: Failed to create "
                "/dev/jimmypaputto directory: %s\r\n"
            "[NmeaForwarder] SOLUTION: Run with sudo privileges:\r\n"
            "[NmeaForwarder]   sudo ./your_program\r\n"
            "[NmeaForwarder] Or create directory manually:\r\n"
            "[NmeaForwarder]   sudo mkdir -p /dev/jimmypaputto\r\n"
            "[NmeaForwarder]   sudo chmod 755 /dev/jimmypaputto\r\n",
            strerror(errno)
        );
        close(masterFd_);
        close(slaveFd_);
        return false;
    }
    
    if (symlink(slaveName, devicePath_.c_str()) != 0)
    {
        fprintf(
            stderr,
            "[NmeaForwarder] FATAL: Failed to create symlink %s: %s\r\n"
            "[NmeaForwarder] SOLUTION: Check permissions and run with sudo\r\n",
            devicePath_.c_str(),
            strerror(errno)
        );
        close(masterFd_);
        close(slaveFd_);
        return false;
    }

    chmod(slaveName, 0666);
    chmod(devicePath_.c_str(), 0666);

    return true;
}

void NmeaForwarder::startForwarding(const Gnss& gnss)
{
    if (forwardingEnabled_.load())
    {
        printf("[NmeaForwarder] Already forwarding\n");
        return;
    }

    if (masterFd_ < 0)
    {
        fprintf(
            stderr,
            "[NmeaForwarder] FATAL: Virtual TTY not created\r\n"
        );
        return;
    }

    forwardingEnabled_.store(true);
    forwardingThread_ = std::jthread(
        [this, &gnss](std::stop_token stoken) {
            forwardingThread(gnss, stoken);
        }
    );
}

void NmeaForwarder::stopForwarding()
{
    if (!forwardingEnabled_.load())
        return;

    forwardingEnabled_.store(false);

    if (!devicePath_.empty())
        unlink(devicePath_.c_str());
}

void NmeaForwarder::joinForwarding()
{
    if (forwardingThread_.joinable())
        forwardingThread_.join();
}

void NmeaForwarder::forwardingThread(const Gnss& gnss, std::stop_token stoken)
{    
    while (!stoken.stop_requested() && forwardingEnabled_.load())
    {
        if (!gnss.lock())
            continue;

        const auto nav = gnss.navigation();
        gnss.unlock();

        const std::string gga = generateNmeaGGA(nav);
        const std::string rmc = generateNmeaRMC(nav);
        const std::string gsa = generateNmeaGSA(nav);

        const std::string combined = gga + rmc + gsa;

        if (masterFd_ >= 0)
        {
            const ssize_t result = write(
                masterFd_, combined.c_str(), combined.length()
            );

            if (result < 0)
            {
                if (errno != EAGAIN && errno != EWOULDBLOCK)
                {
                    fprintf(
                        stderr,
                        "[NmeaForwarder] Write error: %s\r\n",
                        strerror(errno)
                    );
                }
            }
            else
            {
                // TODO: check for that, is this taking too much CPU?
                fsync(masterFd_);
            }
        }

        std::this_thread::sleep_for(updateInterval_);
    }
}

std::string NmeaForwarder::generateNmeaGGA(const Navigation& navigation)
{
    std::ostringstream oss;
    
    std::string sentence = "GPGGA,";
    
    sentence += formatTime(navigation) + ",";
    
    sentence += formatLatitude(navigation.pvt.latitude) + ",";
    sentence += (navigation.pvt.latitude >= 0 ? "N," : "S,");
    
    sentence += formatLongitude(navigation.pvt.longitude) + ",";
    sentence += (navigation.pvt.longitude >= 0 ? "E," : "W,");

    const int quality = static_cast<int>(navigation.pvt.fixQuality);
    sentence += std::to_string(quality) + ",";

    sentence += std::to_string(navigation.pvt.visibleSatellites) + ",";

    oss.str("");
    oss << std::fixed << std::setprecision(1) << navigation.dop.horizontal;
    sentence += oss.str() + ",";
 
    oss.str("");
    oss << std::fixed << std::setprecision(1) << navigation.pvt.altitudeMSL;
    sentence += oss.str() + ",M,";

    const float geoidSep = navigation.pvt.altitude - navigation.pvt.altitudeMSL;
    oss.str("");
    oss << std::fixed << std::setprecision(1) << geoidSep;
    sentence += oss.str() + ",M,,";

    const std::string checksum = calculateNmeaChecksum(sentence);
    return "$" + sentence + "*" + checksum + "\r\n";
}

std::string NmeaForwarder::generateNmeaRMC(const Navigation& navigation)
{
    std::ostringstream oss;

    std::string sentence = "GPRMC,";

    sentence += formatTime(navigation) + ",";
    sentence += (navigation.pvt.fixStatus == EFixStatus::Active ? "A," : "V,");

    sentence += formatLatitude(navigation.pvt.latitude) + ",";
    sentence += (navigation.pvt.latitude >= 0 ? "N," : "S,");

    sentence += formatLongitude(navigation.pvt.longitude) + ",";
    sentence += (navigation.pvt.longitude >= 0 ? "E," : "W,");

    // Speed m/s to knots
    // 1 m/s = 1.94384 knots
    constexpr float knotsConversionFactor = 1.94384f;
    const float speedKnots =
        navigation.pvt.speedOverGround * knotsConversionFactor;
    oss.str("");
    oss << std::fixed << std::setprecision(1) << speedKnots;
    sentence += oss.str() + ",";

    oss.str("");
    oss << std::fixed << std::setprecision(1) << navigation.pvt.heading;
    sentence += oss.str() + ",";

    sentence += formatDate(navigation) + ",,,";

    const std::string checksum = calculateNmeaChecksum(sentence);
    return "$" + sentence + "*" + checksum + "\r\n";
}

std::string NmeaForwarder::generateNmeaGSA(const Navigation& navigation)
{
    std::string sentence = "GPGSA,A,";
    
    int fixType = 1;
    switch (navigation.pvt.fixType)
    {
        case EFixType::Fix2D: fixType = 2; break;
        case EFixType::Fix3D: 
        case EFixType::GnssWithDeadReckoning: fixType = 3; break;
        default: fixType = 1; break;
    }
    sentence += std::to_string(fixType) + ",";

    sentence += ",,,,,,,,,,,,,";

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << navigation.dop.position;
    sentence += oss.str() + ",";

    oss.str("");
    oss << std::fixed << std::setprecision(1) << navigation.dop.horizontal;
    sentence += oss.str() + ",";

    oss.str("");
    oss << std::fixed << std::setprecision(1) << navigation.dop.vertical;
    sentence += oss.str();

    const std::string checksum = calculateNmeaChecksum(sentence);
    return "$" + sentence + "*" + checksum + "\r\n";
}

std::string NmeaForwarder::calculateNmeaChecksum(const std::string& sentence)
{
    uint8_t checksum = 0;
    for (const char c : sentence)
        checksum ^= c;

    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setfill('0')
        << std::setw(2) << (int)checksum;
    return oss.str();
}

std::string NmeaForwarder::formatLatitude(const double lat)
{
    const double absLat = std::abs(lat);
    const int degrees = (int)absLat;
    const double minutes = (absLat - degrees) * 60.0;

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << degrees;
    oss << std::fixed << std::setprecision(5) << std::setfill('0')
        << std::setw(8) << minutes;
    return oss.str();
}

std::string NmeaForwarder::formatLongitude(double lon)
{
    const double absLon = std::abs(lon);
    const int degrees = (int)absLon;
    const double minutes = (absLon - degrees) * 60.0;

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(3) << degrees;
    oss << std::fixed << std::setprecision(5) << std::setfill('0')
        << std::setw(8) << minutes;
    return oss.str();
}

std::string NmeaForwarder::formatTime(const Navigation& navigation)
{
    const auto& utc = navigation.pvt.utc;
    if (!utc.valid)
        return "";

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << (int)utc.hh;
    oss << std::setfill('0') << std::setw(2) << (int)utc.mm;
    oss << std::setfill('0') << std::setw(2) << (int)utc.ss;
    return oss.str();
}

std::string NmeaForwarder::formatDate(const Navigation& navigation)
{
    const auto& date = navigation.pvt.date;
    if (!date.valid)
        return "";
    
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << (int)date.day;
    oss << std::setfill('0') << std::setw(2) << (int)date.month;
    oss << std::setfill('0') << std::setw(2) << (date.year % 100);
    return oss.str();
}

}  // JimmyPaputto

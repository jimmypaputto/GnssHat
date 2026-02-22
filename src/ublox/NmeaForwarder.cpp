/*
 * Jimmy Paputto 2025
 */

#include "ublox/NmeaForwarder.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>
#include <cstring>
#include <chrono>


namespace JimmyPaputto
{

namespace
{

const char* gnssIdToGsvTalkerId(EGnssId id)
{
    switch (id)
    {
        case EGnssId::GPS:
        case EGnssId::SBAS:    return "GP";
        case EGnssId::GLONASS: return "GL";
        case EGnssId::Galileo: return "GA";
        case EGnssId::BeiDou:  return "GB";
        case EGnssId::QZSS:    return "GQ";
        default:               return "GP";
    }
}

uint8_t gnssIdToNmeaSystemId(EGnssId id)
{
    switch (id)
    {
        case EGnssId::GPS:
        case EGnssId::SBAS:    return 1;
        case EGnssId::GLONASS: return 2;
        case EGnssId::Galileo: return 3;
        case EGnssId::BeiDou:  return 4;
        case EGnssId::QZSS:    return 5;
        default:               return 1;
    }
}

int satelliteToNmeaPrn(const SatelliteInfo& sat)
{
    switch (sat.gnssId)
    {
        case EGnssId::GLONASS: return sat.svId + 64;
        default:               return sat.svId;
    }
}

}  // anonymous namespace

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
        const std::string gsa = generateNmeaGSA(nav);
        const std::string gsv = generateNmeaGSV(nav);
        const std::string rmc = generateNmeaRMC(nav);
        const std::string zda = generateNmeaZDA(nav);

        const std::string combined = gga + gsa + gsv + rmc + zda;

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
    
    std::string sentence = "GNGGA,";
    
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

    std::string sentence = "GNRMC,";

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
    int fixType = 1;
    switch (navigation.pvt.fixType)
    {
        case EFixType::Fix2D: fixType = 2; break;
        case EFixType::Fix3D:
        case EFixType::GnssWithDeadReckoning: fixType = 3; break;
        default: fixType = 1; break;
    }

    std::map<uint8_t, std::vector<int>> usedPrnsBySystem;

    for (const auto& sat : navigation.satellites)
    {
        if (!sat.usedInFix)
            continue;

        if (sat.gnssId == EGnssId::IMES)
            continue;

        const uint8_t sysId = gnssIdToNmeaSystemId(sat.gnssId);
        const int prn = satelliteToNmeaPrn(sat);
        usedPrnsBySystem[sysId].push_back(prn);
    }

    std::ostringstream ossPdop, ossHdop, ossVdop;
    ossPdop << std::fixed << std::setprecision(1) << navigation.dop.position;
    ossHdop << std::fixed << std::setprecision(1) << navigation.dop.horizontal;
    ossVdop << std::fixed << std::setprecision(1) << navigation.dop.vertical;

    std::string result;

    if (usedPrnsBySystem.empty())
    {
        std::string sentence = "GNGSA,A,"
            + std::to_string(fixType) + ",";

        for (int i = 0; i < 12; i++)
            sentence += ",";

        sentence += ossPdop.str() + ","
                  + ossHdop.str() + ","
                  + ossVdop.str() + ",1";

        result += "$" + sentence + "*"
               + calculateNmeaChecksum(sentence) + "\r\n";
    }
    else
    {
        for (const auto& [sysId, prns] : usedPrnsBySystem)
        {
            std::string sentence = "GNGSA,A,"
                + std::to_string(fixType) + ",";

            for (int i = 0; i < 12; i++)
            {
                if (i < static_cast<int>(prns.size()))
                    sentence += std::to_string(prns[i]);
                sentence += ",";
            }

            sentence += ossPdop.str() + ","
                      + ossHdop.str() + ","
                      + ossVdop.str() + ","
                      + std::to_string(sysId);

            result += "$" + sentence + "*"
                   + calculateNmeaChecksum(sentence) + "\r\n";
        }
    }

    return result;
}

std::string NmeaForwarder::generateNmeaGSV(const Navigation& navigation)
{
    std::map<uint8_t, std::vector<const SatelliteInfo*>> groups;

    for (const auto& sat : navigation.satellites)
    {
        if (sat.gnssId == EGnssId::IMES)
            continue;

        const uint8_t sysId = gnssIdToNmeaSystemId(sat.gnssId);
        groups[sysId].push_back(&sat);
    }

    std::string result;

    for (const auto& [sysId, sats] : groups)
    {
        if (sats.empty())
            continue;

        const char* talkerId = gnssIdToGsvTalkerId(sats[0]->gnssId);
        const int totalSvs = static_cast<int>(sats.size());
        const int totalMsgs = (totalSvs + 3) / 4;

        for (int msgNum = 1; msgNum <= totalMsgs; msgNum++)
        {
            std::string sentence = std::string(talkerId) + "GSV,"
                + std::to_string(totalMsgs) + ","
                + std::to_string(msgNum) + ","
                + std::to_string(totalSvs);

            const int startIdx = (msgNum - 1) * 4;
            const int endIdx = std::min(startIdx + 4, totalSvs);

            for (int i = startIdx; i < endIdx; i++)
            {
                const auto& sat = *sats[i];
                const int prn = satelliteToNmeaPrn(sat);

                std::ostringstream oss;
                oss << std::setfill('0') << std::setw(2) << prn;
                sentence += "," + oss.str();

                sentence += "," + std::to_string(
                    static_cast<int>(sat.elevation));

                oss.str("");
                oss << std::setfill('0') << std::setw(3)
                    << sat.azimuth;
                sentence += "," + oss.str();

                sentence += ",";
                if (sat.cno > 0)
                {
                    oss.str("");
                    oss << std::setfill('0') << std::setw(2)
                        << static_cast<int>(sat.cno);
                    sentence += oss.str();
                }
            }

            const std::string checksum =
                calculateNmeaChecksum(sentence);
            result += "$" + sentence + "*" + checksum + "\r\n";
        }
    }

    return result;
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

std::string NmeaForwarder::generateNmeaZDA(const Navigation& navigation)
{
    const auto& date = navigation.pvt.date;

    std::string sentence = "GNZDA,";

    sentence += formatTime(navigation) + ",";

    if (date.valid)
    {
        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(2) << (int)date.day;
        sentence += oss.str() + ",";

        oss.str("");
        oss << std::setfill('0') << std::setw(2) << (int)date.month;
        sentence += oss.str() + ",";

        sentence += std::to_string(date.year) + ",";
    }
    else
    {
        sentence += ",,,,";
    }

    // Local zone hours and minutes (UTC = 00,00)
    sentence += "00,00";

    const std::string checksum = calculateNmeaChecksum(sentence);
    return "$" + sentence + "*" + checksum + "\r\n";
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

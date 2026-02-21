/*
 * Jimmy Paputto 2025
 */

#ifndef JIMMY_PAPUTTO_NMEA_FORWARDER_HPP_
#define JIMMY_PAPUTTO_NMEA_FORWARDER_HPP_

#include <atomic>
#include <string>
#include <thread>
#include <memory>
#include <pty.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/stat.h>

#include "Gnss.hpp"
#include "Navigation.hpp"
#include "common/Notifier.hpp"


namespace JimmyPaputto
{

class NmeaForwarder
{
public:
    explicit NmeaForwarder();
    ~NmeaForwarder();

    bool createVirtualTty();
    void startForwarding(const Gnss& gnss);
    void stopForwarding();
    void joinForwarding();
    
    std::string getDevicePath() const { return devicePath_; }
    bool isRunning() const { return forwardingEnabled_.load(); }

private:
    void forwardingThread(const Gnss& gnss, std::stop_token stoken);
    std::string generateNmeaGGA(const Navigation& navigation);
    std::string generateNmeaRMC(const Navigation& navigation);
    std::string generateNmeaGSA(const Navigation& navigation);
    std::string calculateNmeaChecksum(const std::string& sentence);
    std::string formatLatitude(const double lat);
    std::string formatLongitude(const double lon);
    std::string formatTime(const Navigation& navigation);
    std::string formatDate(const Navigation& navigation);

    int masterFd_;
    int slaveFd_;
    std::string devicePath_;
    std::atomic<bool> forwardingEnabled_{false};
    std::jthread forwardingThread_;

    std::chrono::milliseconds updateInterval_{1000};
};

}  // JimmyPaputto

#endif  // JIMMY_PAPUTTO_NMEA_FORWARDER_HPP_

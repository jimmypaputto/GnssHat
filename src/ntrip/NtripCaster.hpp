/*
 * Jimmy Paputto 2026
 *
 * Simplified single-mountpoint NTRIP v2.0 caster for GnssHat.
 * Accepts NTRIP client connections over TCP and broadcasts
 * RTCM3 correction frames from the RTK base station.
 */

#ifndef NTRIP_CASTER_HPP_
#define NTRIP_CASTER_HPP_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace JimmyPaputto
{

    class NtripCaster
    {
    public:
        NtripCaster(std::string host, uint16_t port,
                    std::string mountpoint, size_t maxClients = 10);
        ~NtripCaster();

        NtripCaster(const NtripCaster &) = delete;
        NtripCaster &operator=(const NtripCaster &) = delete;

        bool start();
        void stop();

        void feed(const std::vector<std::vector<uint8_t>> &frames);

        size_t clientCount() const;
        void updatePosition(double lat, double lon);

    private:
        void acceptLoop(std::stop_token stoken);
        void handleClient(int clientFd, std::string clientAddr);
        void registerClient(int fd, const std::string &addr);
        void removeClient(int fd, const std::string &addr);

        void sendSourcetable(int fd);
        void sendResponse(int fd, const char *status, const char *body);
        bool sendAll(int fd, const void *data, size_t len);

        std::string host_;
        uint16_t port_;
        std::string mountpoint_;
        size_t maxClients_;

        int serverFd_ = -1;
        std::atomic<bool> running_{false};
        std::jthread acceptThread_;

        mutable std::mutex clientsMutex_;
        std::vector<int> clients_;

        std::mutex positionMutex_;
        double latitude_ = 0.0;
        double longitude_ = 0.0;
    };

}
#endif // NTRIP_CASTER_HPP_

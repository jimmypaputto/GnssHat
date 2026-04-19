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
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "NtripLog.hpp"
#include "NtripStats.hpp"
#include "NtripTls.hpp"

namespace JimmyPaputto
{

    class NtripCaster : public NtripLoggable, public NtripStatsTracker
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

        /// Set credentials for Basic auth.  Empty = accept all (default).
        void setCredentials(std::string username, std::string password);

        /// Enable server-side TLS.  Must be called before start().
        /// certFile and keyFile are paths to PEM files.
        bool setTls(const std::string &certFile, const std::string &keyFile);

        /// Check if TLS support was compiled in.
        static bool isTlsAvailable();

    private:
        void acceptLoop(std::stop_token stoken);
        void handleClient(int clientFd, std::string clientAddr);
        void registerClient(int fd, const std::string &addr);
        void removeClient(int fd, const std::string &addr);

        void sendSourcetable(int fd);
        void sendResponse(int fd, const char *status, const char *body);
        bool sendAll(int fd, const void *data, size_t len);

        ssize_t netRecv(int fd, void *buf, size_t len);
        void closeClientTls(int fd);

        std::string host_;
        uint16_t port_;
        std::string mountpoint_;
        size_t maxClients_;

        int serverFd_ = -1;
        std::atomic<bool> running_{false};
        std::jthread acceptThread_;

        mutable std::mutex clientsMutex_;
        std::vector<int> clients_;
        std::vector<int> sources_;   // POST (source/server push) fds

        std::mutex positionMutex_;
        double latitude_ = 0.0;
        double longitude_ = 0.0;

        std::mutex authMutex_;
        std::string authUsername_;
        std::string authPassword_;

        struct HandlerThread {
            std::thread thread;
            std::atomic<bool> finished{false};
        };
        std::mutex threadsMutex_;
        std::vector<std::unique_ptr<HandlerThread>> clientThreads_;

        // Server-side TLS
        NtripTlsServerContext tlsCtx_;
        mutable std::mutex tlsMapMutex_;
        std::vector<std::pair<int, void *>> tlsHandles_; // fd → SSL*
        void *getTlsHandle(int fd) const;
        void setTlsHandle(int fd, void *handle);
        void removeTlsHandle(int fd);
    };

}
#endif // NTRIP_CASTER_HPP_

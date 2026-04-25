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
#include "RtcmAnalyzer.hpp"

namespace JimmyPaputto
{

    class NtripCaster : public NtripLoggable, public NtripStatsTracker
    {
    public:
        NtripCaster(std::string host, uint16_t port,
                    size_t maxClients = 64);
        ~NtripCaster();

        NtripCaster(const NtripCaster &) = delete;
        NtripCaster &operator=(const NtripCaster &) = delete;

        bool start();
        void stop();

        void feed(const std::vector<std::vector<uint8_t>> &frames);

        size_t clientCount() const;

        /// Currently advertised mountpoint name, or empty string if no
        /// source is connected.
        std::string mountpoint() const;

        /// Manually override the advertised position.  Normally the
        /// caster auto-updates this by decoding RTCM 1005/1006 frames
        /// from the source stream.
        void updatePosition(double lat, double lon);

        /// Set credentials for Basic auth.  Empty = accept all (default).
        void setCredentials(std::string username, std::string password);

        /// Snapshot of the live RTCM3 stream feeding the active mountpoint.
        /// Empty (default-constructed) when no source is connected.
        RtcmSnapshot rtcmSnapshot() const;

        /// Description of every currently connected source-side socket
        /// (POST connections).
        struct SourceInfo
        {
            int         fd          = -1;
            std::string peer;        // "ip:port"
            std::string mountpoint;  // empty unless this fd owns the active mount
            uint64_t    connectedUnixMs = 0;
        };
        std::vector<SourceInfo> connectedSources() const;

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
        size_t maxClients_;

        // Active mountpoint claimed by the connected source (empty when
        // no source is connected).
        mutable std::mutex mountMutex_;
        std::string activeMountpoint_;
        int activeSourceFd_ = -1;

        int serverFd_ = -1;
        std::atomic<bool> running_{false};
        std::jthread acceptThread_;

        mutable std::mutex clientsMutex_;
        std::vector<int> clients_;
        std::vector<int> sources_;   // POST (source/server push) fds

        std::mutex positionMutex_;
        double latitude_ = 0.0;
        double longitude_ = 0.0;

        // Live RTCM3 stream analyzer fed by the active source thread.
        mutable std::mutex   analyzerMutex_;
        RtcmAnalyzer         analyzer_;

        // Per-source metadata for status page.
        mutable std::mutex   sourceInfoMutex_;
        std::vector<SourceInfo> sourceInfo_;

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

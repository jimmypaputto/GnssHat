/*
 * Jimmy Paputto 2026
 *
 * NTRIP v2.0 server (source) for GnssHat.
 * Pushes RTCM3 corrections to a remote NTRIP caster
 * via HTTP POST, solving NAT/firewall issues.
 */

#ifndef NTRIP_SERVER_HPP_
#define NTRIP_SERVER_HPP_

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "NtripLog.hpp"
#include "NtripStats.hpp"
#include "NtripTls.hpp"

namespace JimmyPaputto
{

    class NtripServer : public NtripLoggable, public NtripStatsTracker
    {
    public:
        NtripServer(std::string host, uint16_t port,
                    std::string mountpoint,
                    std::string username = {},
                    std::string password = {});
        ~NtripServer();

        NtripServer(const NtripServer &) = delete;
        NtripServer &operator=(const NtripServer &) = delete;

        bool connect();
        void disconnect();
        bool isConnected() const;

        /// Send RTCM3 frames to the remote caster.
        void feed(const std::vector<std::vector<uint8_t>> &frames);

        /// Enable/disable auto-reconnect with exponential backoff.
        void setAutoReconnect(bool enable,
                              uint32_t initialDelayMs = 1000,
                              uint32_t maxDelayMs = 30000);

        /// Number of reconnect attempts since last connect().
        uint32_t reconnectCount() const;

        /// Enable TLS (disabled by default). Must be called before connect().
        void setUseTls(bool enable, bool verifyPeer = true);

        /// Check if TLS support was compiled in.
        static bool isTlsAvailable();

    private:
        bool connectInternal(std::stop_token stoken = {});
        void monitorLoop(std::stop_token stoken);
        bool sendAll(const void *data, size_t len);

        ssize_t netSend(const void *buf, size_t len);
        ssize_t netRecv(void *buf, size_t len);

        std::string host_;
        uint16_t port_;
        std::string mountpoint_;
        std::string username_;
        std::string password_;

        int sockFd_ = -1;
        std::atomic<bool> connected_{false};
        std::jthread monitorThread_;

        mutable std::mutex sendMutex_;

        // Auto-reconnect state
        std::atomic<bool> autoReconnect_{false};
        uint32_t reconnectInitialMs_ = 1000;
        uint32_t reconnectMaxMs_ = 30000;
        std::atomic<uint32_t> reconnectCount_{0};
        std::mutex reconnectMutex_;
        std::condition_variable reconnectCv_;

        // TLS state
        bool useTls_ = false;
        bool tlsVerifyPeer_ = true;
        NtripTlsSocket tls_;
    };

}

#endif // NTRIP_SERVER_HPP_

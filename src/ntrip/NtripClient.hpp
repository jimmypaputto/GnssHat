/*
 * Jimmy Paputto 2026
 *
 * Simplified NTRIP v2.0 client for GnssHat.
 * Connects to an NTRIP caster and receives RTCM3 correction
 * frames that can be applied to an RTK rover receiver.
 */

#ifndef NTRIP_CLIENT_HPP_
#define NTRIP_CLIENT_HPP_

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

    /// Entry from an NTRIP caster sourcetable (STR record).
    struct NtripSourcetableEntry
    {
        std::string mountpoint;
        std::string identifier;
        std::string format;
        std::string formatDetails;
        std::string carrier;
        std::string navSystem;
        double latitude = 0.0;
        double longitude = 0.0;
    };

    class NtripClient : public NtripLoggable, public NtripStatsTracker
    {
    public:
        NtripClient(std::string host, uint16_t port,
                    std::string mountpoint,
                    std::string username = {},
                    std::string password = {});
        ~NtripClient();

        NtripClient(const NtripClient &) = delete;
        NtripClient &operator=(const NtripClient &) = delete;

        bool connect();
        void disconnect();
        bool isConnected() const;

        /// Drain all received frames since the last call.
        std::vector<std::vector<uint8_t>> receiveFrames();

        /// Send GGA position to the caster (for VRS / nearest base).
        void sendPosition(double lat, double lon, double alt);

        /// Enable/disable auto-reconnect with exponential backoff.
        void setAutoReconnect(bool enable,
                              uint32_t initialDelayMs = 1000,
                              uint32_t maxDelayMs = 30000);

        /// Number of reconnect attempts since last connect().
        uint32_t reconnectCount() const;

        /// Store a position that auto-GGA will periodically send.
        void updatePosition(double lat, double lon, double alt);

        /// Enable periodic GGA sending.  0 = disabled (default).
        void setAutoGGA(uint32_t intervalMs);

        /// Enable TLS (disabled by default). Must be called before connect().
        void setUseTls(bool enable, bool verifyPeer = true);

        /// Check if TLS support was compiled in.
        static bool isTlsAvailable();

        /// Fetch the sourcetable from an NTRIP caster (static utility).
        static std::vector<NtripSourcetableEntry> fetchSourcetable(
            const std::string &host, uint16_t port,
            const std::string &username = {},
            const std::string &password = {},
            uint32_t timeoutMs = 10000,
            bool useTls = false,
            bool tlsVerifyPeer = true);

    private:
        bool connectInternal(std::stop_token stoken = {});
        void receiveLoop(std::stop_token stoken);
        void extractFrames(const uint8_t *data, size_t len);
        void autoGgaLoop(std::stop_token stoken);

        ssize_t netSend(const void *buf, size_t len);
        ssize_t netRecv(void *buf, size_t len);

        std::string host_;
        uint16_t port_;
        std::string mountpoint_;
        std::string username_;
        std::string password_;

        int sockFd_ = -1;
        std::atomic<bool> connected_{false};
        std::jthread recvThread_;

        mutable std::mutex framesMutex_;
        std::vector<std::vector<uint8_t>> pendingFrames_;
        std::vector<uint8_t> parseBuffer_;

        // Auto-reconnect state
        std::atomic<bool> autoReconnect_{false};
        uint32_t reconnectInitialMs_ = 1000;
        uint32_t reconnectMaxMs_ = 30000;
        std::atomic<uint32_t> reconnectCount_{0};

        // Auto-GGA state
        std::mutex ggaMutex_;
        std::condition_variable ggaCv_;
        std::jthread autoGgaThread_;
        std::atomic<uint32_t> autoGgaIntervalMs_{0};
        double ggaLat_ = 0.0, ggaLon_ = 0.0, ggaAlt_ = 0.0;
        std::mutex reconnectMutex_;
        std::condition_variable reconnectCv_;

        // TLS state
        bool useTls_ = false;
        bool tlsVerifyPeer_ = true;
        NtripTlsSocket tls_;
    };

}
#endif // NTRIP_CLIENT_HPP_

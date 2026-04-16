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
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace JimmyPaputto
{

    class NtripClient
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

    private:
        void receiveLoop(std::stop_token stoken);
        void extractFrames(const uint8_t *data, size_t len);

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
    };

}
#endif // NTRIP_CLIENT_HPP_

/*
 * Jimmy Paputto 2026
 */

#include "NtripClient.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <sstream>

#include "common/Utils.hpp"
#include "ublox/Rtcm3Parser.hpp"

namespace JimmyPaputto
{
    // -----------------------------------------------------------------------
    // Public
    // -----------------------------------------------------------------------

    NtripClient::NtripClient(std::string host, uint16_t port,
                             std::string mountpoint,
                             std::string username,
                             std::string password)
        : host_(std::move(host)), port_(port),
          mountpoint_(std::move(mountpoint)),
          username_(std::move(username)),
          password_(std::move(password))
    {
    }

    NtripClient::~NtripClient()
    {
        disconnect();
    }

    bool NtripClient::connect()
    {
        if (connected_)
            return true;

        reconnectCount_ = 0;

        if (!connectInternal())
            return false;

        recvThread_ = std::jthread([this](std::stop_token st)
                                   { receiveLoop(st); });
        return true;
    }

    bool NtripClient::connectInternal(std::stop_token stoken)
    {
        // Resolve hostname
        struct addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        std::string portStr = std::to_string(port_);
        struct addrinfo *res = nullptr;
        int rc = ::getaddrinfo(host_.c_str(), portStr.c_str(), &hints, &res);
        if (rc != 0 || !res)
        {
            log(ENtripLogLevel::Error, "[NtripClient] Failed to resolve %s: %s",
                host_.c_str(), gai_strerror(rc));
            return false;
        }

        if (stoken.stop_requested())
        {
            ::freeaddrinfo(res);
            return false;
        }

        sockFd_ = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sockFd_ < 0)
        {
            log(ENtripLogLevel::Error, "[NtripClient] Failed to create socket: %s",
                strerror(errno));
            ::freeaddrinfo(res);
            return false;
        }

        // Set receive timeout (10s)
        struct timeval tv{};
        tv.tv_sec = 10;
        ::setsockopt(sockFd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        // Non-blocking connect with 5s timeout so disconnect() can interrupt it
        int flags = ::fcntl(sockFd_, F_GETFL, 0);
        ::fcntl(sockFd_, F_SETFL, flags | O_NONBLOCK);

        int crc = ::connect(sockFd_, res->ai_addr, res->ai_addrlen);
        if (crc < 0 && errno != EINPROGRESS)
        {
            log(ENtripLogLevel::Error, "[NtripClient] Failed to connect to %s:%u: %s",
                host_.c_str(), port_, strerror(errno));
            ::close(sockFd_);
            sockFd_ = -1;
            ::freeaddrinfo(res);
            return false;
        }

        if (crc < 0) // EINPROGRESS — wait for completion
        {
            struct pollfd pfd{};
            pfd.fd = sockFd_;
            pfd.events = POLLOUT;

            // Poll in 500ms slices so disconnect() can interrupt via stop_token
            int pr = 0;
            for (int elapsed = 0; elapsed < 5000; elapsed += 500)
            {
                pr = ::poll(&pfd, 1, 500);
                if (pr != 0 || stoken.stop_requested())
                    break;
            }

            if (stoken.stop_requested())
            {
                ::close(sockFd_);
                sockFd_ = -1;
                ::freeaddrinfo(res);
                return false;
            }
            if (pr <= 0)
            {
                log(ENtripLogLevel::Error, "[NtripClient] Connect to %s:%u timed out",
                    host_.c_str(), port_);
                ::close(sockFd_);
                sockFd_ = -1;
                ::freeaddrinfo(res);
                return false;
            }
            int sockerr = 0;
            socklen_t errlen = sizeof(sockerr);
            ::getsockopt(sockFd_, SOL_SOCKET, SO_ERROR, &sockerr, &errlen);
            if (sockerr != 0)
            {
                log(ENtripLogLevel::Error, "[NtripClient] Failed to connect to %s:%u: %s",
                    host_.c_str(), port_, strerror(sockerr));
                ::close(sockFd_);
                sockFd_ = -1;
                ::freeaddrinfo(res);
                return false;
            }
        }

        // Restore blocking mode for recv/send
        ::fcntl(sockFd_, F_SETFL, flags);
        ::freeaddrinfo(res);

        // Build NTRIP v2.0 GET request
        std::ostringstream req;
        req << "GET /" << mountpoint_ << " HTTP/1.1\r\n";
        req << "Host: " << host_ << ":" << port_ << "\r\n";
        req << "Ntrip-Version: Ntrip/2.0\r\n";
        req << "User-Agent: GnssHat/1.0\r\n";

        if (!username_.empty())
        {
            std::string cred = username_ + ":" + password_;
            req << "Authorization: Basic " << base64Encode(cred) << "\r\n";
        }

        req << "Accept: */*\r\n";
        req << "\r\n";

        std::string reqStr = req.str();
        ssize_t sent = ::send(sockFd_, reqStr.data(), reqStr.size(), MSG_NOSIGNAL);
        if (sent < 0 || static_cast<size_t>(sent) != reqStr.size())
        {
            log(ENtripLogLevel::Error, "[NtripClient] Failed to send request: %s",
                strerror(errno));
            ::close(sockFd_);
            sockFd_ = -1;
            return false;
        }

        // Read response header
        char hdrBuf[4096]{};
        size_t hdrLen = 0;
        bool headerComplete = false;

        while (hdrLen < sizeof(hdrBuf) - 1)
        {
            ssize_t n = ::recv(sockFd_, hdrBuf + hdrLen,
                               sizeof(hdrBuf) - 1 - hdrLen, 0);
            if (n <= 0)
            {
                log(ENtripLogLevel::Error, "[NtripClient] Connection closed during header read");
                ::close(sockFd_);
                sockFd_ = -1;
                return false;
            }
            hdrLen += static_cast<size_t>(n);
            hdrBuf[hdrLen] = '\0';

            if (strstr(hdrBuf, "\r\n\r\n"))
            {
                headerComplete = true;
                break;
            }
        }

        if (!headerComplete)
        {
            log(ENtripLogLevel::Error, "[NtripClient] Incomplete response header");
            ::close(sockFd_);
            sockFd_ = -1;
            return false;
        }

        // Check for ICY 200 OK or HTTP/1.1 200 OK
        bool ok = (strstr(hdrBuf, "ICY 200 OK") != nullptr) ||
                  (strstr(hdrBuf, "200 OK") != nullptr);

        if (!ok)
        {
            // Extract first line for error message
            char *eol = strstr(hdrBuf, "\r\n");
            std::string firstLine;
            if (eol)
                firstLine.assign(hdrBuf, static_cast<size_t>(eol - hdrBuf));
            else
                firstLine = hdrBuf;

            log(ENtripLogLevel::Error, "[NtripClient] Caster rejected connection: %s",
                firstLine.c_str());
            ::close(sockFd_);
            sockFd_ = -1;
            return false;
        }

        // Feed any trailing data after headers into the parse buffer
        const char *bodyStart = strstr(hdrBuf, "\r\n\r\n") + 4;
        size_t trailing = hdrLen - static_cast<size_t>(bodyStart - hdrBuf);
        if (trailing > 0)
        {
            std::lock_guard lock(framesMutex_);
            extractFrames(reinterpret_cast<const uint8_t *>(bodyStart),
                          trailing);
        }

        connected_ = true;
        statsStart();

        log(ENtripLogLevel::Info, "[NtripClient] Connected to %s:%u/%s",
            host_.c_str(), port_, mountpoint_.c_str());
        return true;
    }

    void NtripClient::disconnect()
    {
        autoReconnect_ = false;

        if (!connected_.exchange(false))
        {
            // Wake any sleeping reconnect loop so it exits
            if (recvThread_.joinable())
                recvThread_.request_stop();
            reconnectCv_.notify_all();
            if (recvThread_.joinable())
                recvThread_.join();
            return;
        }

        if (recvThread_.joinable())
            recvThread_.request_stop();

        reconnectCv_.notify_all();

        if (sockFd_ >= 0)
        {
            ::shutdown(sockFd_, SHUT_RDWR);
            ::close(sockFd_);
            sockFd_ = -1;
        }

        if (recvThread_.joinable())
            recvThread_.join();

        std::lock_guard lock(framesMutex_);
        pendingFrames_.clear();
        parseBuffer_.clear();
        statsReset();

        log(ENtripLogLevel::Info, "[NtripClient] Disconnected.");
    }

    bool NtripClient::isConnected() const
    {
        return connected_;
    }

    std::vector<std::vector<uint8_t>> NtripClient::receiveFrames()
    {
        std::lock_guard lock(framesMutex_);
        std::vector<std::vector<uint8_t>> out;
        out.swap(pendingFrames_);
        return out;
    }

    void NtripClient::setAutoReconnect(bool enable,
                                        uint32_t initialDelayMs,
                                        uint32_t maxDelayMs)
    {
        autoReconnect_ = enable;
        reconnectInitialMs_ = initialDelayMs;
        reconnectMaxMs_ = maxDelayMs;
    }

    uint32_t NtripClient::reconnectCount() const
    {
        return reconnectCount_;
    }

    void NtripClient::sendPosition(double lat, double lon, double alt)
    {
        if (!connected_ || sockFd_ < 0)
            return;

        // Build NMEA GGA sentence
        char ns = lat >= 0 ? 'N' : 'S';
        char ew = lon >= 0 ? 'E' : 'W';
        double absLat = std::fabs(lat);
        double absLon = std::fabs(lon);

        int latDeg = static_cast<int>(absLat);
        double latMin = (absLat - latDeg) * 60.0;
        int lonDeg = static_cast<int>(absLon);
        double lonMin = (absLon - lonDeg) * 60.0;

        char gga[256];
        snprintf(gga, sizeof(gga),
                 "$GPGGA,000000.00,%02d%010.7f,%c,%03d%010.7f,%c,"
                 "1,12,1.0,%.2f,M,0.0,M,,",
                 latDeg, latMin, ns, lonDeg, lonMin, ew, alt);

        // Compute NMEA checksum
        uint8_t cksum = 0;
        for (const char *p = gga + 1; *p; ++p)
            cksum ^= static_cast<uint8_t>(*p);

        char sentence[300];
        snprintf(sentence, sizeof(sentence), "%s*%02X\r\n", gga, cksum);

        ::send(sockFd_, sentence, strlen(sentence), MSG_NOSIGNAL);
    }

    // -----------------------------------------------------------------------
    // Private
    // -----------------------------------------------------------------------

    void NtripClient::receiveLoop(std::stop_token stoken)
    {
        uint8_t buf[8192];

    receive_loop_start:
        while (!stoken.stop_requested() && connected_)
        {
            ssize_t n = ::recv(sockFd_, buf, sizeof(buf), 0);
            if (n <= 0)
            {
                if (n == 0)
                    log(ENtripLogLevel::Warning, "[NtripClient] Server closed connection");
                else if (errno == EAGAIN || errno == EWOULDBLOCK)
                    continue; // Timeout, retry
                else
                    log(ENtripLogLevel::Error, "[NtripClient] recv error: %s",
                        strerror(errno));

                connected_ = false;
                break;
            }

            std::lock_guard lock(framesMutex_);
            statsRecordRx(static_cast<size_t>(n));
            extractFrames(buf, static_cast<size_t>(n));
        }

        // Auto-reconnect with exponential backoff
        if (autoReconnect_ && !stoken.stop_requested())
        {
            uint32_t delayMs = reconnectInitialMs_;
            while (autoReconnect_ && !stoken.stop_requested())
            {
                ++reconnectCount_;
                log(ENtripLogLevel::Info,
                    "[NtripClient] Reconnect attempt %u in %u ms",
                    reconnectCount_.load(), delayMs);

                {
                    std::unique_lock lk(reconnectMutex_);
                    reconnectCv_.wait_for(lk, std::chrono::milliseconds(delayMs),
                                          [&] { return stoken.stop_requested() || !autoReconnect_.load(); });
                }

                if (stoken.stop_requested() || !autoReconnect_)
                    break;

                // Close old socket
                if (sockFd_ >= 0)
                {
                    ::close(sockFd_);
                    sockFd_ = -1;
                }

                if (connectInternal(stoken))
                {
                    log(ENtripLogLevel::Info, "[NtripClient] Reconnected after %u attempts",
                        reconnectCount_.load());
                    // Re-enter receive loop
                    goto receive_loop_start;
                }

                delayMs = std::min(delayMs * 2, reconnectMaxMs_);
            }
        }
    }

    void NtripClient::extractFrames(const uint8_t *data, size_t len)
    {
        // Append new data to parse buffer
        parseBuffer_.insert(parseBuffer_.end(), data, data + len);

        while (parseBuffer_.size() >= 6)
        {
            // Find RTCM3 preamble (0xD3)
            auto it = std::find(parseBuffer_.begin(), parseBuffer_.end(), 0xD3);
            if (it == parseBuffer_.end())
            {
                parseBuffer_.clear();
                return;
            }

            // Discard bytes before preamble
            if (it != parseBuffer_.begin())
                parseBuffer_.erase(parseBuffer_.begin(), it);

            if (parseBuffer_.size() < 6)
                return; // Need more data

            // Check reserved bits (byte 1 upper 6 bits should be 0)
            if ((parseBuffer_[1] & 0xFC) != 0)
            {
                parseBuffer_.erase(parseBuffer_.begin());
                continue;
            }

            // Extract payload length from bytes 1-2 (10 bits)
            uint16_t payloadLen = (static_cast<uint16_t>(parseBuffer_[1] & 0x03) << 8)
                                | static_cast<uint16_t>(parseBuffer_[2]);

            // Total frame: 3 (header) + payload + 3 (CRC)
            size_t frameLen = 3u + payloadLen + 3u;
            if (parseBuffer_.size() < frameLen)
                return; // Need more data

            // Verify CRC-24Q over header + payload
            uint32_t computed = crc24q(parseBuffer_.data(), 3u + payloadLen);
            uint32_t received =
                (static_cast<uint32_t>(parseBuffer_[3 + payloadLen]) << 16) |
                (static_cast<uint32_t>(parseBuffer_[4 + payloadLen]) << 8) |
                 static_cast<uint32_t>(parseBuffer_[5 + payloadLen]);

            if (computed != received)
            {
                // Bad CRC — skip this preamble byte
                parseBuffer_.erase(parseBuffer_.begin());
                continue;
            }

            // Valid frame
            statsRecordFrame(parseBuffer_.data(), frameLen);
            pendingFrames_.emplace_back(
                parseBuffer_.begin(),
                parseBuffer_.begin() + static_cast<ptrdiff_t>(frameLen));
            parseBuffer_.erase(
                parseBuffer_.begin(),
                parseBuffer_.begin() + static_cast<ptrdiff_t>(frameLen));
        }
    }

}

/*
 * Jimmy Paputto 2026
 */

#include "NtripServer.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <sstream>

#include "common/Utils.hpp"

namespace JimmyPaputto
{
    // -----------------------------------------------------------------------
    // Public
    // -----------------------------------------------------------------------

    NtripServer::NtripServer(std::string host, uint16_t port,
                             std::string mountpoint,
                             std::string username,
                             std::string password)
        : host_(std::move(host)), port_(port),
          mountpoint_(std::move(mountpoint)),
          username_(std::move(username)),
          password_(std::move(password))
    {
    }

    NtripServer::~NtripServer()
    {
        disconnect();
    }

    void NtripServer::setUseTls(bool enable, bool verifyPeer)
    {
        useTls_ = enable;
        tlsVerifyPeer_ = verifyPeer;
    }

    bool NtripServer::isTlsAvailable()
    {
        return NtripTlsSocket::isAvailable();
    }

    bool NtripServer::connect()
    {
        if (connected_)
            return true;

        reconnectCount_ = 0;

        if (!connectInternal())
            return false;

        monitorThread_ = std::jthread([this](std::stop_token st)
                                      { monitorLoop(st); });
        return true;
    }

    void NtripServer::disconnect()
    {
        autoReconnect_ = false;

        if (!connected_.exchange(false))
        {
            if (monitorThread_.joinable())
                monitorThread_.request_stop();
            reconnectCv_.notify_all();
            if (monitorThread_.joinable())
                monitorThread_.join();
            return;
        }

        if (monitorThread_.joinable())
            monitorThread_.request_stop();

        reconnectCv_.notify_all();

        tls_.close();

        if (sockFd_ >= 0)
        {
            ::shutdown(sockFd_, SHUT_RDWR);
            ::close(sockFd_);
            sockFd_ = -1;
        }

        if (monitorThread_.joinable())
            monitorThread_.join();

        statsReset();

        log(ENtripLogLevel::Info, "[NtripServer] Disconnected.");
    }

    bool NtripServer::isConnected() const
    {
        return connected_;
    }

    void NtripServer::feed(const std::vector<std::vector<uint8_t>> &frames)
    {
        if (!connected_ || sockFd_ < 0)
            return;

        statsRecordTxFrames(frames);

        // Concatenate all frames
        size_t totalSize = 0;
        for (const auto &f : frames)
            totalSize += f.size();

        if (totalSize == 0)
            return;

        std::vector<uint8_t> buf;
        buf.reserve(totalSize);
        for (const auto &f : frames)
            buf.insert(buf.end(), f.begin(), f.end());

        if (!sendAll(buf.data(), buf.size()))
        {
            connected_ = false;
            log(ENtripLogLevel::Warning,
                "[NtripServer] Send failed, connection lost");
        }
    }

    void NtripServer::setAutoReconnect(bool enable,
                                       uint32_t initialDelayMs,
                                       uint32_t maxDelayMs)
    {
        autoReconnect_ = enable;
        reconnectInitialMs_ = initialDelayMs;
        reconnectMaxMs_ = maxDelayMs;
    }

    uint32_t NtripServer::reconnectCount() const
    {
        return reconnectCount_;
    }

    // -----------------------------------------------------------------------
    // Private
    // -----------------------------------------------------------------------

    bool NtripServer::connectInternal(std::stop_token stoken)
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
            log(ENtripLogLevel::Error, "[NtripServer] Failed to resolve %s: %s",
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
            log(ENtripLogLevel::Error, "[NtripServer] Failed to create socket: %s",
                strerror(errno));
            ::freeaddrinfo(res);
            return false;
        }

        // Set receive timeout for header handshake
        struct timeval tv{};
        tv.tv_sec = 3;
        ::setsockopt(sockFd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        // Non-blocking connect with 5s timeout
        int flags = ::fcntl(sockFd_, F_GETFL, 0);
        ::fcntl(sockFd_, F_SETFL, flags | O_NONBLOCK);

        int crc = ::connect(sockFd_, res->ai_addr, res->ai_addrlen);
        if (crc < 0 && errno != EINPROGRESS)
        {
            log(ENtripLogLevel::Error, "[NtripServer] Failed to connect to %s:%u: %s",
                host_.c_str(), port_, strerror(errno));
            ::close(sockFd_);
            sockFd_ = -1;
            ::freeaddrinfo(res);
            return false;
        }

        if (crc < 0)
        {
            struct pollfd pfd{};
            pfd.fd = sockFd_;
            pfd.events = POLLOUT;

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
                log(ENtripLogLevel::Error, "[NtripServer] Connect to %s:%u timed out",
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
                log(ENtripLogLevel::Error, "[NtripServer] Failed to connect to %s:%u: %s",
                    host_.c_str(), port_, strerror(sockerr));
                ::close(sockFd_);
                sockFd_ = -1;
                ::freeaddrinfo(res);
                return false;
            }
        }

        // Restore blocking mode
        ::fcntl(sockFd_, F_SETFL, flags);
        ::freeaddrinfo(res);

        // TLS handshake (if enabled)
        if (useTls_)
        {
            tls_.close();
            if (!tls_.wrap(sockFd_, host_, tlsVerifyPeer_))
            {
                log(ENtripLogLevel::Error,
                    "[NtripServer] TLS handshake failed to %s:%u",
                    host_.c_str(), port_);
                ::close(sockFd_);
                sockFd_ = -1;
                return false;
            }
            log(ENtripLogLevel::Info, "[NtripServer] TLS established to %s:%u",
                host_.c_str(), port_);
        }

        // Build NTRIP v2.0 POST request (server/source push)
        std::ostringstream req;
        req << "POST /" << mountpoint_ << " HTTP/1.1\r\n";
        req << "Host: " << host_ << ":" << port_ << "\r\n";
        req << "Ntrip-Version: Ntrip/2.0\r\n";
        req << "User-Agent: GnssHat/1.0\r\n";
        req << "Content-Type: gnss/data\r\n";

        // NTRIP server auth: base64("username:password")
        if (!password_.empty() || !username_.empty())
        {
            std::string cred = username_ + ":" + password_;
            req << "Authorization: Basic " << base64Encode(cred) << "\r\n";
        }

        req << "\r\n";

        std::string reqStr = req.str();
        ssize_t sent = netSend(reqStr.data(), reqStr.size());
        if (sent < 0 || static_cast<size_t>(sent) != reqStr.size())
        {
            log(ENtripLogLevel::Error, "[NtripServer] Failed to send request: %s",
                strerror(errno));
            tls_.close();
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
            ssize_t n = netRecv(hdrBuf + hdrLen,
                               sizeof(hdrBuf) - 1 - hdrLen);
            if (n <= 0)
            {
                log(ENtripLogLevel::Error, "[NtripServer] Connection closed during header read");
                tls_.close();
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
            log(ENtripLogLevel::Error, "[NtripServer] Incomplete response header");
            tls_.close();
            ::close(sockFd_);
            sockFd_ = -1;
            return false;
        }

        // Check for ICY 200 OK or HTTP/1.1 200 OK
        bool ok = (strstr(hdrBuf, "ICY 200 OK") != nullptr) ||
                  (strstr(hdrBuf, "200 OK") != nullptr);

        if (!ok)
        {
            char *eol = strstr(hdrBuf, "\r\n");
            std::string firstLine;
            if (eol)
                firstLine.assign(hdrBuf, static_cast<size_t>(eol - hdrBuf));
            else
                firstLine = hdrBuf;

            log(ENtripLogLevel::Error, "[NtripServer] Caster rejected connection: %s",
                firstLine.c_str());
            tls_.close();
            ::close(sockFd_);
            sockFd_ = -1;
            return false;
        }

        connected_ = true;
        statsStart();

        log(ENtripLogLevel::Info, "[NtripServer] Connected to %s:%u/%s",
            host_.c_str(), port_, mountpoint_.c_str());
        return true;
    }

    void NtripServer::monitorLoop(std::stop_token stoken)
    {
        // Monitor connection by polling for POLLIN (server disconnect)
    monitor_loop_start:
        while (!stoken.stop_requested() && connected_)
        {
            struct pollfd pfd{};
            pfd.fd = sockFd_;
            pfd.events = POLLIN;

            int pr = ::poll(&pfd, 1, 500);
            if (pr > 0 && (pfd.revents & (POLLIN | POLLHUP | POLLERR)))
            {
                // Server sent something (likely disconnect) or error
                char buf[256];
                ssize_t n = netRecv(buf, sizeof(buf));
                if (n <= 0)
                {
                    log(ENtripLogLevel::Warning, "[NtripServer] Server closed connection");
                    connected_ = false;
                    break;
                }
                // Discard any data from server
            }
        }

        // Auto-reconnect with exponential backoff
        if (autoReconnect_ && !stoken.stop_requested())
        {
            uint32_t delayMs = reconnectInitialMs_;
            while (autoReconnect_ && !stoken.stop_requested())
            {
                ++reconnectCount_;
                log(ENtripLogLevel::Info,
                    "[NtripServer] Reconnect attempt %u in %u ms",
                    reconnectCount_.load(), delayMs);

                {
                    std::unique_lock lk(reconnectMutex_);
                    reconnectCv_.wait_for(lk, std::chrono::milliseconds(delayMs),
                                          [&] { return stoken.stop_requested() || !autoReconnect_.load(); });
                }

                if (stoken.stop_requested() || !autoReconnect_)
                    break;

                tls_.close();
                if (sockFd_ >= 0)
                {
                    ::close(sockFd_);
                    sockFd_ = -1;
                }

                if (connectInternal(stoken))
                {
                    log(ENtripLogLevel::Info, "[NtripServer] Reconnected after %u attempts",
                        reconnectCount_.load());
                    reconnectCount_ = 0;
                    goto monitor_loop_start;
                }

                delayMs = std::min(delayMs * 2, reconnectMaxMs_);
            }
        }
    }

    bool NtripServer::sendAll(const void *data, size_t len)
    {
        std::lock_guard lock(sendMutex_);
        const uint8_t *ptr = static_cast<const uint8_t *>(data);
        size_t remaining = len;

        while (remaining > 0)
        {
            ssize_t sent = netSend(ptr, remaining);
            if (sent < 0)
                return false;
            ptr += sent;
            remaining -= static_cast<size_t>(sent);
        }
        return true;
    }

    ssize_t NtripServer::netSend(const void *buf, size_t len)
    {
        if (tls_.isActive())
            return tls_.write(buf, len);
        return ::send(sockFd_, buf, len, MSG_NOSIGNAL);
    }

    ssize_t NtripServer::netRecv(void *buf, size_t len)
    {
        if (tls_.isActive())
            return tls_.read(buf, len);
        return ::recv(sockFd_, buf, len, 0);
    }

}

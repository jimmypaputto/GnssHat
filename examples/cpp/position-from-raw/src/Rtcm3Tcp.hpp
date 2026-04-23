/*
 * Jimmy Paputto 2026
 *
 * Simple TCP server that listens for incoming RTCM3 correction data
 * from external sources (e.g. an NTRIP caster, another base station)
 * and provides parsed frames to the application.
 *
 * Also supports simple TCP client output: sending locally-generated
 * RTCM3 frames to a connected client (for acting as a base station).
 */

#ifndef RTCM3_TCP_HPP_
#define RTCM3_TCP_HPP_

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace Rtcm3Tcp
{

// ── Simple RTCM3 stream parser (standalone, no library dependency) ───
class StreamParser
{
public:
    // Feed raw bytes; returns complete RTCM3 frames found
    std::vector<std::vector<uint8_t>> feed(const uint8_t* data, size_t len)
    {
        std::vector<std::vector<uint8_t>> frames;
        buffer_.insert(buffer_.end(), data, data + len);

        while (buffer_.size() >= 6)
        {
            // Find preamble
            auto it = std::find(buffer_.begin(), buffer_.end(), 0xD3);
            if (it == buffer_.end())
            {
                buffer_.clear();
                break;
            }
            if (it != buffer_.begin())
                buffer_.erase(buffer_.begin(), it);

            if (buffer_.size() < 3) break;

            const uint16_t payloadLen =
                ((static_cast<uint16_t>(buffer_[1]) & 0x03) << 8) | buffer_[2];
            const size_t frameLen = 3 + payloadLen + 3;

            if (buffer_.size() < frameLen) break;

            // Extract and validate frame
            std::vector<uint8_t> frame(buffer_.begin(),
                                       buffer_.begin() + frameLen);

            // Verify CRC24Q
            if (verifyCrc24(frame))
            {
                frames.push_back(std::move(frame));
                buffer_.erase(buffer_.begin(), buffer_.begin() + frameLen);
            }
            else
            {
                // Skip this byte and try again
                buffer_.erase(buffer_.begin());
            }
        }

        return frames;
    }

    static uint16_t getMessageId(const std::vector<uint8_t>& frame)
    {
        if (frame.size() < 5) return 0;
        return (static_cast<uint16_t>(frame[3]) << 4) | (frame[4] >> 4);
    }

private:
    static bool verifyCrc24(const std::vector<uint8_t>& frame)
    {
        if (frame.size() < 6) return false;
        const size_t dataLen = frame.size() - 3;
        constexpr uint32_t poly = 0x1864CFB;
        uint32_t crc = 0;
        for (size_t i = 0; i < dataLen; ++i)
        {
            crc ^= static_cast<uint32_t>(frame[i]) << 16;
            for (int bit = 0; bit < 8; ++bit)
            {
                crc <<= 1;
                if (crc & 0x1000000) crc ^= poly;
            }
        }
        crc &= 0xFFFFFF;
        const uint32_t frameCrc =
            (static_cast<uint32_t>(frame[dataLen]) << 16) |
            (static_cast<uint32_t>(frame[dataLen + 1]) << 8) |
            static_cast<uint32_t>(frame[dataLen + 2]);
        return crc == frameCrc;
    }

    std::vector<uint8_t> buffer_;
};

// ── RTCM3 TCP Receiver (accepts corrections from external source) ───
class Receiver
{
public:
    using FrameCallback = std::function<void(
        const std::vector<std::vector<uint8_t>>&)>;

    explicit Receiver(uint16_t port, FrameCallback callback)
        : port_(port), callback_(std::move(callback)) {}

    ~Receiver() { stop(); }

    void start()
    {
        running_ = true;
        thread_ = std::thread([this]() { acceptLoop(); });
    }

    void stop()
    {
        running_ = false;
        if (listenFd_ >= 0)
        {
            shutdown(listenFd_, SHUT_RDWR);
            close(listenFd_);
            listenFd_ = -1;
        }
        if (thread_.joinable())
            thread_.join();
    }

    bool isRunning() const { return running_; }

private:
    void acceptLoop()
    {
        listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (listenFd_ < 0)
        {
            perror("socket");
            return;
        }

        int opt = 1;
        setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port_);

        if (bind(listenFd_, reinterpret_cast<struct sockaddr*>(&addr),
                 sizeof(addr)) < 0)
        {
            perror("bind");
            close(listenFd_);
            listenFd_ = -1;
            return;
        }

        if (listen(listenFd_, 2) < 0)
        {
            perror("listen");
            close(listenFd_);
            listenFd_ = -1;
            return;
        }

        printf("[RTCM3 Receiver] Listening on port %u\n", port_);

        while (running_)
        {
            struct sockaddr_in clientAddr{};
            socklen_t clientLen = sizeof(clientAddr);
            int clientFd = accept(listenFd_,
                reinterpret_cast<struct sockaddr*>(&clientAddr), &clientLen);

            if (clientFd < 0)
            {
                if (running_)
                    perror("accept");
                break;
            }

            char clientIp[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, sizeof(clientIp));
            printf("[RTCM3 Receiver] Client connected from %s:%d\n",
                   clientIp, ntohs(clientAddr.sin_port));

            handleClient(clientFd);
            close(clientFd);
            printf("[RTCM3 Receiver] Client disconnected\n");
        }
    }

    void handleClient(int clientFd)
    {
        StreamParser parser;
        uint8_t buf[4096];

        while (running_)
        {
            const ssize_t n = recv(clientFd, buf, sizeof(buf), 0);
            if (n <= 0) break;

            auto frames = parser.feed(buf, static_cast<size_t>(n));
            if (!frames.empty() && callback_)
                callback_(frames);
        }
    }

    uint16_t port_;
    FrameCallback callback_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    int listenFd_ = -1;
};

// ── RTCM3 TCP Sender (broadcasts generated RTCM3 to connected clients) ─
class Sender
{
public:
    explicit Sender(uint16_t port) : port_(port) {}
    ~Sender() { stop(); }

    void start()
    {
        running_ = true;
        thread_ = std::thread([this]() { acceptLoop(); });
    }

    void stop()
    {
        running_ = false;
        if (listenFd_ >= 0)
        {
            shutdown(listenFd_, SHUT_RDWR);
            close(listenFd_);
            listenFd_ = -1;
        }
        {
            std::lock_guard lock(clientsMutex_);
            for (int fd : clientFds_)
                close(fd);
            clientFds_.clear();
        }
        if (thread_.joinable())
            thread_.join();
    }

    void sendFrames(const std::vector<std::vector<uint8_t>>& frames)
    {
        std::lock_guard lock(clientsMutex_);
        std::vector<int> deadClients;

        for (const auto& frame : frames)
        {
            for (int fd : clientFds_)
            {
                ssize_t sent = send(fd, frame.data(), frame.size(), MSG_NOSIGNAL);
                if (sent < 0)
                    deadClients.push_back(fd);
            }
        }

        // Remove dead clients
        for (int fd : deadClients)
        {
            close(fd);
            clientFds_.erase(
                std::remove(clientFds_.begin(), clientFds_.end(), fd),
                clientFds_.end());
            printf("[RTCM3 Sender] Client disconnected\n");
        }
    }

    int clientCount() const
    {
        std::lock_guard lock(clientsMutex_);
        return static_cast<int>(clientFds_.size());
    }

private:
    void acceptLoop()
    {
        listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (listenFd_ < 0) { perror("socket"); return; }

        int opt = 1;
        setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port_);

        if (bind(listenFd_, reinterpret_cast<struct sockaddr*>(&addr),
                 sizeof(addr)) < 0)
        { perror("bind"); close(listenFd_); listenFd_ = -1; return; }

        if (listen(listenFd_, 5) < 0)
        { perror("listen"); close(listenFd_); listenFd_ = -1; return; }

        printf("[RTCM3 Sender] Listening on port %u for RTCM3 clients\n", port_);

        while (running_)
        {
            struct sockaddr_in clientAddr{};
            socklen_t clientLen = sizeof(clientAddr);
            int clientFd = accept(listenFd_,
                reinterpret_cast<struct sockaddr*>(&clientAddr), &clientLen);

            if (clientFd < 0)
            {
                if (running_) perror("accept");
                break;
            }

            char clientIp[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, sizeof(clientIp));
            printf("[RTCM3 Sender] Client connected from %s:%d\n",
                   clientIp, ntohs(clientAddr.sin_port));

            std::lock_guard lock(clientsMutex_);
            clientFds_.push_back(clientFd);
        }
    }

    uint16_t port_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    int listenFd_ = -1;
    mutable std::mutex clientsMutex_;
    std::vector<int> clientFds_;
};

}  // Rtcm3Tcp

#endif  // RTCM3_TCP_HPP_

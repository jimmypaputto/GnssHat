/*
 * Jimmy Paputto 2026
 */

#include "NtripCaster.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <sstream>

#include "common/Utils.hpp"

namespace JimmyPaputto
{
    NtripCaster::NtripCaster(std::string host, uint16_t port,
                             std::string mountpoint, size_t maxClients)
        : host_(std::move(host)), port_(port), mountpoint_(std::move(mountpoint)), maxClients_(maxClients)
    {
    }

    NtripCaster::~NtripCaster()
    {
        stop();
    }

    bool NtripCaster::start()
    {
        serverFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (serverFd_ < 0)
        {
            log(ENtripLogLevel::Error, "[NtripCaster] Failed to create socket: %s",
                strerror(errno));
            return false;
        }

        int opt = 1;
        ::setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port_);
        if (host_ == "0.0.0.0")
            addr.sin_addr.s_addr = INADDR_ANY;
        else
            ::inet_pton(AF_INET, host_.c_str(), &addr.sin_addr);

        if (::bind(serverFd_, reinterpret_cast<sockaddr *>(&addr),
                   sizeof(addr)) < 0)
        {
            log(ENtripLogLevel::Error, "[NtripCaster] Failed to bind %s:%u: %s",
                host_.c_str(), port_, strerror(errno));
            ::close(serverFd_);
            serverFd_ = -1;
            return false;
        }

        if (::listen(serverFd_, 5) < 0)
        {
            log(ENtripLogLevel::Error, "[NtripCaster] Failed to listen: %s", strerror(errno));
            ::close(serverFd_);
            serverFd_ = -1;
            return false;
        }

        running_ = true;
        statsStart();
        acceptThread_ = std::jthread([this](std::stop_token st)
                                     { acceptLoop(st); });

        log(ENtripLogLevel::Info, "[NtripCaster] Listening on %s:%u/%s (max %zu clients)",
            host_.c_str(), port_, mountpoint_.c_str(), maxClients_);
        return true;
    }

    void NtripCaster::stop()
    {
        if (!running_.exchange(false))
            return;

        if (acceptThread_.joinable())
            acceptThread_.request_stop();

        // Close server socket to unblock accept()
        if (serverFd_ >= 0)
        {
            ::shutdown(serverFd_, SHUT_RDWR);
            ::close(serverFd_);
            serverFd_ = -1;
        }

        if (acceptThread_.joinable())
            acceptThread_.join();

        // Close all client connections
        std::lock_guard lock(clientsMutex_);
        for (int fd : clients_)
        {
            ::shutdown(fd, SHUT_RDWR);
            ::close(fd);
        }
        clients_.clear();
        statsReset();
        log(ENtripLogLevel::Info, "[NtripCaster] Stopped.");
    }

    void NtripCaster::feed(const std::vector<std::vector<uint8_t>> &frames)
    {
        // Track statistics
        statsRecordTxFrames(frames);

        // Concatenate all frames into a single buffer
        size_t totalSize = 0;
        for (const auto &f : frames)
            totalSize += f.size();

        if (totalSize == 0)
            return;

        std::vector<uint8_t> buf;
        buf.reserve(totalSize);
        for (const auto &f : frames)
            buf.insert(buf.end(), f.begin(), f.end());

        std::lock_guard lock(clientsMutex_);
        std::vector<int> dead;

        for (int fd : clients_)
        {
            if (!sendAll(fd, buf.data(), buf.size()))
                dead.push_back(fd);
        }

        for (int fd : dead)
        {
            clients_.erase(
                std::remove(clients_.begin(), clients_.end(), fd),
                clients_.end());
            ::close(fd);
            log(ENtripLogLevel::Debug, "[NtripCaster] Client fd=%d disconnected during feed "
                "(total: %zu)",
                fd, clients_.size());
        }
    }

    size_t NtripCaster::clientCount() const
    {
        std::lock_guard lock(clientsMutex_);
        return clients_.size();
    }

    void NtripCaster::updatePosition(double lat, double lon)
    {
        std::lock_guard lock(positionMutex_);
        latitude_ = lat;
        longitude_ = lon;
    }

    void NtripCaster::setCredentials(std::string username,
                                     std::string password)
    {
        std::lock_guard lock(authMutex_);
        authUsername_ = std::move(username);
        authPassword_ = std::move(password);
    }

    // ---------------------------------------------------------------------------
    // Private
    // ---------------------------------------------------------------------------

    void NtripCaster::acceptLoop(std::stop_token stoken)
    {
        while (!stoken.stop_requested() && running_)
        {
            sockaddr_in clientAddr{};
            socklen_t addrLen = sizeof(clientAddr);
            int clientFd = ::accept(serverFd_,
                                    reinterpret_cast<sockaddr *>(&clientAddr),
                                    &addrLen);
            if (clientFd < 0)
            {
                if (running_)
                    log(ENtripLogLevel::Error, "[NtripCaster] accept() error: %s",
                        strerror(errno));
                break;
            }

            char addrStr[INET_ADDRSTRLEN]{};
            ::inet_ntop(AF_INET, &clientAddr.sin_addr, addrStr, sizeof(addrStr));
            std::string addr = std::string(addrStr) + ":" + std::to_string(ntohs(clientAddr.sin_port));

            // Spawn a detached thread per client
            std::thread([this, clientFd, addr = std::move(addr)]()
                        { handleClient(clientFd, addr); })
                .detach();
        }
    }

    void NtripCaster::handleClient(int clientFd, std::string clientAddr)
    {
        char reqBuf[4096]{};
        ssize_t n = ::recv(clientFd, reqBuf, sizeof(reqBuf) - 1, 0);
        if (n <= 0)
        {
            ::close(clientFd);
            return;
        }
        reqBuf[n] = '\0';

        // Parse first line: "GET /path HTTP/1.1"
        std::string firstLine;
        {
            const char *eol = strstr(reqBuf, "\r\n");
            if (eol)
                firstLine.assign(reqBuf, static_cast<size_t>(eol - reqBuf));
            else
                firstLine = reqBuf;
        }

        // Tokenise: METHOD PATH VERSION
        std::istringstream iss(firstLine);
        std::string method, path, version;
        iss >> method >> path >> version;

        if (method != "GET")
        {
            sendResponse(clientFd, "405 Method Not Allowed",
                         "Only GET is supported.\r\n");
            ::close(clientFd);
            return;
        }

        // Strip leading '/'
        std::string mount = path;
        while (!mount.empty() && mount.front() == '/')
            mount.erase(mount.begin());

        // Empty path → sourcetable
        if (mount.empty())
        {
            sendSourcetable(clientFd);
            ::close(clientFd);
            return;
        }

        // Wrong mountpoint -> 404
        if (mount != mountpoint_)
        {
            std::string body = "Mountpoint '" + mount + "' not found.\r\n";
            sendResponse(clientFd, "404 Not Found", body.c_str());
            ::close(clientFd);
            return;
        }

        // Check authentication (if credentials are set)
        {
            std::lock_guard lock(authMutex_);
            if (!authUsername_.empty())
            {
                // Look for "Authorization: Basic <b64>" header
                const char *authHdr = strcasestr(reqBuf, "Authorization: Basic ");
                std::string decoded;
                if (authHdr)
                {
                    authHdr += 21; // skip "Authorization: Basic "
                    const char *eol = strstr(authHdr, "\r\n");
                    std::string b64(authHdr,
                                    eol ? static_cast<size_t>(eol - authHdr)
                                        : strlen(authHdr));
                    decoded = base64Decode(b64);
                }

                std::string expected = authUsername_ + ":" + authPassword_;
                if (decoded != expected)
                {
                    const char *resp =
                        "HTTP/1.1 401 Unauthorized\r\n"
                        "WWW-Authenticate: Basic realm=\"NTRIP Caster\"\r\n"
                        "Content-Length: 0\r\n"
                        "\r\n";
                    sendAll(clientFd, resp, strlen(resp));
                    ::close(clientFd);
                    log(ENtripLogLevel::Warning,
                        "[NtripCaster] Rejected %s — auth failed",
                        clientAddr.c_str());
                    return;
                }
            }
        }

        // Check max clients
        {
            std::lock_guard lock(clientsMutex_);
            if (clients_.size() >= maxClients_)
            {
                sendResponse(clientFd, "503 Service Unavailable",
                             "Too many clients connected.\r\n");
                ::close(clientFd);
                log(ENtripLogLevel::Warning, "[NtripCaster] Rejected %s — max clients reached (%zu)",
                    clientAddr.c_str(), maxClients_);
                return;
            }
        }

        // Accept: send ICY 200 OK (NTRIP v2.0)
        const char *icy =
            "ICY 200 OK\r\n"
            "Content-Type: gnss/data\r\n"
            "Cache-Control: no-store\r\n"
            "\r\n";

        if (!sendAll(clientFd, icy, strlen(icy)))
        {
            ::close(clientFd);
            return;
        }

        registerClient(clientFd, clientAddr);

        // Block until client disconnects — data is pushed via feed()
        char discardBuf[1024];
        while (running_)
        {
            ssize_t r = ::recv(clientFd, discardBuf, sizeof(discardBuf), 0);
            if (r <= 0)
                break;
        }

        removeClient(clientFd, clientAddr);
        ::close(clientFd);
    }

    void NtripCaster::registerClient(int fd, const std::string &addr)
    {
        std::lock_guard lock(clientsMutex_);
        clients_.push_back(fd);
        log(ENtripLogLevel::Info, "[NtripCaster] Client %s connected (total: %zu)",
            addr.c_str(), clients_.size());
    }

    void NtripCaster::removeClient(int fd, const std::string &addr)
    {
        std::lock_guard lock(clientsMutex_);
        clients_.erase(
            std::remove(clients_.begin(), clients_.end(), fd),
            clients_.end());
        log(ENtripLogLevel::Info, "[NtripCaster] Client %s disconnected (total: %zu)",
            addr.c_str(), clients_.size());
    }

    void NtripCaster::sendSourcetable(int fd)
    {
        double lat, lon;
        {
            std::lock_guard lock(positionMutex_);
            lat = latitude_;
            lon = longitude_;
        }

        char entry[512];
        snprintf(entry, sizeof(entry),
                 "STR;%s;%s;RTCM 3.3;"
                 "1005(31),1077(1),1087(1),1097(1),1127(1),1230(10);"
                 "2;GPS+GLO+GAL+BDS;NONE;POL;%.6f;%.6f;"
                 "0;0;GnssHat NEO-F9P;none;N;N;0;\r\n",
                 mountpoint_.c_str(), mountpoint_.c_str(), lat, lon);

        std::string body = std::string(entry) + "ENDSOURCETABLE\r\n";

        char header[256];
        snprintf(header, sizeof(header),
                 "ICY 200 OK\r\n"
                 "Content-Type: gnss/sourcetable\r\n"
                 "Content-Length: %zu\r\n"
                 "\r\n",
                 body.size());

        std::string resp = std::string(header) + body;
        sendAll(fd, resp.data(), resp.size());
    }

    void NtripCaster::sendResponse(int fd, const char *status, const char *body)
    {
        size_t bodyLen = strlen(body);
        char header[256];
        snprintf(header, sizeof(header),
                 "HTTP/1.1 %s\r\n"
                 "Content-Type: text/plain\r\n"
                 "Content-Length: %zu\r\n"
                 "\r\n",
                 status, bodyLen);

        std::string resp = std::string(header) + body;
        sendAll(fd, resp.data(), resp.size());
    }

    bool NtripCaster::sendAll(int fd, const void *data, size_t len)
    {
        const uint8_t *ptr = static_cast<const uint8_t *>(data);
        size_t remaining = len;

        while (remaining > 0)
        {
            ssize_t sent = ::send(fd, ptr, remaining, MSG_NOSIGNAL);
            if (sent < 0)
                return false;
            ptr += sent;
            remaining -= static_cast<size_t>(sent);
        }
        return true;
    }
}
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
#include <chrono>
#include <sstream>

#include "Base64.hpp"
#include "RtcmArp.hpp"

namespace JimmyPaputto
{
    NtripCaster::NtripCaster(std::string host, uint16_t port,
                             size_t maxClients)
        : host_(std::move(host)), port_(port), maxClients_(maxClients)
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

        log(ENtripLogLevel::Info, "[NtripCaster] Listening on %s:%u (max %zu clients, mountpoint claimed by source)",
            host_.c_str(), port_, maxClients_);
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

        // Shutdown (but don't close) client/source sockets to unblock
        // handler threads — they close their own fds on exit.
        {
            std::lock_guard lock(clientsMutex_);
            for (int fd : clients_)
                ::shutdown(fd, SHUT_RDWR);
            for (int fd : sources_)
                ::shutdown(fd, SHUT_RDWR);
        }

        // Wait for all handler threads to finish
        {
            std::lock_guard tlock(threadsMutex_);
            for (auto &ht : clientThreads_)
            {
                if (ht->thread.joinable())
                    ht->thread.join();
            }
            clientThreads_.clear();
        }

        // Clear remaining tracking vectors
        {
            std::lock_guard lock(clientsMutex_);
            clients_.clear();
            sources_.clear();
        }

        // Destroy TLS context
        tlsCtx_.destroy();

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
            closeClientTls(fd);
            ::shutdown(fd, SHUT_RDWR);
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

    std::string NtripCaster::mountpoint() const
    {
        std::lock_guard lock(mountMutex_);
        return activeMountpoint_;
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

    RtcmSnapshot NtripCaster::rtcmSnapshot() const
    {
        std::lock_guard lock(analyzerMutex_);
        return analyzer_.snapshot();
    }

    std::vector<NtripCaster::SourceInfo>
    NtripCaster::connectedSources() const
    {
        std::lock_guard lock(sourceInfoMutex_);
        return sourceInfo_;
    }

    bool NtripCaster::setTls(const std::string &certFile,
                             const std::string &keyFile)
    {
        return tlsCtx_.init(certFile, keyFile);
    }

    bool NtripCaster::isTlsAvailable()
    {
        return NtripTlsSocket::isAvailable();
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

            // Clean up finished handler threads
            {
                std::lock_guard tlock(threadsMutex_);
                auto it = clientThreads_.begin();
                while (it != clientThreads_.end())
                {
                    if ((*it)->finished.load(std::memory_order_acquire))
                    {
                        (*it)->thread.join();
                        it = clientThreads_.erase(it);
                    }
                    else
                        ++it;
                }
            }

            // Spawn tracked handler thread
            auto ht = std::make_unique<HandlerThread>();
            auto *htPtr = ht.get();
            ht->thread = std::thread([this, clientFd, addr = std::move(addr), htPtr]()
                                     {
                                         handleClient(clientFd, addr);
                                         htPtr->finished.store(true, std::memory_order_release);
                                     });
            {
                std::lock_guard tlock(threadsMutex_);
                clientThreads_.push_back(std::move(ht));
            }
        }
    }

    void NtripCaster::handleClient(int clientFd, std::string clientAddr)
    {
        // TLS handshake (if enabled)
        if (tlsCtx_.isActive())
        {
            void *handle = tlsCtx_.accept(clientFd);
            if (!handle)
            {
                log(ENtripLogLevel::Warning,
                    "[NtripCaster] TLS handshake failed for %s",
                    clientAddr.c_str());
                ::close(clientFd);
                return;
            }
            setTlsHandle(clientFd, handle);
        }

        char reqBuf[4096]{};
        ssize_t n = netRecv(clientFd, reqBuf, sizeof(reqBuf) - 1);
        if (n <= 0)
        {
            closeClientTls(clientFd);
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

        if (method != "GET" && method != "POST")
        {
            sendResponse(clientFd, "405 Method Not Allowed",
                         "Only GET and POST are supported.\r\n");
            closeClientTls(clientFd);
            ::close(clientFd);
            return;
        }

        // Strip leading '/'
        std::string mount = path;
        while (!mount.empty() && mount.front() == '/')
            mount.erase(mount.begin());

        // GET with empty path → sourcetable
        if (method == "GET" && mount.empty())
        {
            sendSourcetable(clientFd);
            closeClientTls(clientFd);
            ::close(clientFd);
            return;
        }

        if (method == "GET")
        {
            // Rovers may only join the mountpoint currently claimed by a source.
            std::string current;
            {
                std::lock_guard lock(mountMutex_);
                current = activeMountpoint_;
            }
            if (current.empty() || mount != current)
            {
                std::string body = "Mountpoint '" + mount + "' not found.\r\n";
                sendResponse(clientFd, "404 Not Found", body.c_str());
                closeClientTls(clientFd);
                ::close(clientFd);
                return;
            }
        }
        else // POST
        {
            // Refuse a second source while one is already connected.
            std::lock_guard lock(mountMutex_);
            if (activeSourceFd_ >= 0)
            {
                sendResponse(clientFd, "409 Conflict",
                             "A source is already connected.\r\n");
                closeClientTls(clientFd);
                ::close(clientFd);
                log(ENtripLogLevel::Warning,
                    "[NtripCaster] Rejected source %s — mountpoint '%s' busy",
                    clientAddr.c_str(), activeMountpoint_.c_str());
                return;
            }
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
                    closeClientTls(clientFd);
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
                closeClientTls(clientFd);
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
            closeClientTls(clientFd);
            ::close(clientFd);
            return;
        }

        if (method == "POST")
        {
            // Claim the mountpoint for this source.
            {
                std::lock_guard lock(mountMutex_);
                activeMountpoint_ = mount;
                activeSourceFd_   = clientFd;
            }

            // Source/server push: read RTCM3 data and broadcast to clients
            {
                std::lock_guard lock(clientsMutex_);
                sources_.push_back(clientFd);
            }

            // Reset the analyzer for the new source — the snapshot now
            // describes only the currently active stream.
            {
                std::lock_guard alock(analyzerMutex_);
                analyzer_.reset();
            }

            // Track this source for the status page.
            {
                using namespace std::chrono;
                std::lock_guard lk(sourceInfoMutex_);
                SourceInfo si;
                si.fd = clientFd;
                si.peer = clientAddr;
                si.mountpoint = mount;
                si.connectedUnixMs = static_cast<uint64_t>(
                    duration_cast<milliseconds>(
                        system_clock::now().time_since_epoch()).count());
                sourceInfo_.push_back(std::move(si));
            }

            log(ENtripLogLevel::Info,
                "[NtripCaster] Source %s connected, claimed mountpoint '%s'",
                clientAddr.c_str(), mount.c_str());

            uint8_t readBuf[8192];
            while (running_)
            {
                ssize_t r = netRecv(clientFd, readBuf, sizeof(readBuf));
                if (r <= 0)
                    break;

                // Track relay statistics and extract RTCM3 message types.
                statsRecordTxRaw(readBuf, static_cast<size_t>(r));

                // Feed the analyzer for the status page (validates CRC,
                // decodes 1005/1006 ARP and MSM headers).
                {
                    std::lock_guard alock(analyzerMutex_);
                    analyzer_.feed(readBuf, static_cast<size_t>(r));
                    if (auto snap = analyzer_.snapshot(); snap.arp)
                    {
                        std::lock_guard plock(positionMutex_);
                        latitude_  = snap.arp->latitudeDeg;
                        longitude_ = snap.arp->longitudeDeg;
                    }
                }

                // Broadcast raw data to all GET clients
                std::lock_guard lock(clientsMutex_);
                std::vector<int> dead;
                for (int fd : clients_)
                {
                    if (!sendAll(fd, readBuf, static_cast<size_t>(r)))
                        dead.push_back(fd);
                }
                for (int fd : dead)
                {
                    clients_.erase(
                        std::remove(clients_.begin(), clients_.end(), fd),
                        clients_.end());
                    ::shutdown(fd, SHUT_RDWR);
                }
            }

            {
                std::lock_guard lock(clientsMutex_);
                sources_.erase(
                    std::remove(sources_.begin(), sources_.end(), clientFd),
                    sources_.end());
            }
            {
                std::lock_guard lk(sourceInfoMutex_);
                sourceInfo_.erase(
                    std::remove_if(sourceInfo_.begin(), sourceInfo_.end(),
                                   [clientFd](const SourceInfo& s) {
                                       return s.fd == clientFd;
                                   }),
                    sourceInfo_.end());
            }
            {
                std::lock_guard lock(mountMutex_);
                if (activeSourceFd_ == clientFd)
                {
                    activeSourceFd_ = -1;
                    activeMountpoint_.clear();
                }
            }
            log(ENtripLogLevel::Info, "[NtripCaster] Source %s disconnected",
                clientAddr.c_str());
            closeClientTls(clientFd);
            ::close(clientFd);
            return;
        }

        registerClient(clientFd, clientAddr);

        // Block until client disconnects — data is pushed via feed()
        char discardBuf[1024];
        while (running_)
        {
            ssize_t r = netRecv(clientFd, discardBuf, sizeof(discardBuf));
            if (r <= 0)
                break;
        }

        removeClient(clientFd, clientAddr);
        closeClientTls(clientFd);
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
        std::string mount;
        {
            std::lock_guard lock(mountMutex_);
            mount = activeMountpoint_;
        }

        double lat, lon;
        {
            std::lock_guard lock(positionMutex_);
            lat = latitude_;
            lon = longitude_;
        }

        std::string body;
        if (!mount.empty())
        {
            char entry[512];
            snprintf(entry, sizeof(entry),
                     "STR;%s;%s;RTCM 3.3;"
                     "1005(31),1077(1),1087(1),1097(1),1127(1),1230(10);"
                     "2;GPS+GLO+GAL+BDS;NONE;XXX;%.6f;%.6f;"
                     "0;0;NTRIP Caster;none;N;N;0;\r\n",
                     mount.c_str(), mount.c_str(), lat, lon);
            body = entry;
        }
        body += "ENDSOURCETABLE\r\n";

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

        void *handle = getTlsHandle(fd);

        while (remaining > 0)
        {
            ssize_t sent;
            if (handle)
                sent = NtripTlsServerContext::write(handle, ptr, remaining);
            else
                sent = ::send(fd, ptr, remaining, MSG_NOSIGNAL);
            if (sent < 0)
                return false;
            ptr += sent;
            remaining -= static_cast<size_t>(sent);
        }
        return true;
    }

    ssize_t NtripCaster::netRecv(int fd, void *buf, size_t len)
    {
        void *handle = getTlsHandle(fd);
        if (handle)
            return NtripTlsServerContext::read(handle, buf, len);
        return ::recv(fd, buf, len, 0);
    }

    void NtripCaster::closeClientTls(int fd)
    {
        void *handle = nullptr;
        {
            std::lock_guard lock(tlsMapMutex_);
            for (auto it = tlsHandles_.begin(); it != tlsHandles_.end(); ++it)
            {
                if (it->first == fd)
                {
                    handle = it->second;
                    tlsHandles_.erase(it);
                    break;
                }
            }
        }
        NtripTlsServerContext::closeSsl(handle);
    }

    void *NtripCaster::getTlsHandle(int fd) const
    {
        std::lock_guard lock(tlsMapMutex_);
        for (const auto &p : tlsHandles_)
        {
            if (p.first == fd)
                return p.second;
        }
        return nullptr;
    }

    void NtripCaster::setTlsHandle(int fd, void *handle)
    {
        std::lock_guard lock(tlsMapMutex_);
        tlsHandles_.push_back({fd, handle});
    }

    void NtripCaster::removeTlsHandle(int fd)
    {
        std::lock_guard lock(tlsMapMutex_);
        for (auto it = tlsHandles_.begin(); it != tlsHandles_.end(); ++it)
        {
            if (it->first == fd)
            {
                tlsHandles_.erase(it);
                return;
            }
        }
    }

}
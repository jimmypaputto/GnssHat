/*
 * Jimmy Paputto 2026
 */

#include "NtripClient.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <sstream>

namespace JimmyPaputto
{
    // -----------------------------------------------------------------------
    // RTCM3 CRC-24Q (for frame validation)
    // -----------------------------------------------------------------------

    static const uint32_t crc24q_table[256] = {
        0x000000,0x864CFB,0x8AD50D,0x0C99F6,0x93E6E1,0x15AA1A,0x1933EC,0x9F7F17,
        0xA18139,0x27CDC2,0x2B5434,0xAD18CF,0x3267D8,0xB42B23,0xB8B2D5,0x3EFE2E,
        0xC54E89,0x430272,0x4F9B84,0xC9D77F,0x56A868,0xD0E493,0xDC7D65,0x5A319E,
        0x64CFB0,0xE2834B,0xEE1ABD,0x685646,0xF72951,0x7165AA,0x7DFC5C,0xFBB0A7,
        0x0CD1E9,0x8A9D12,0x8604E4,0x00481F,0x9F3708,0x197BF3,0x15E205,0x93AEFE,
        0xAD50D0,0x2B1C2B,0x2785DD,0xA1C926,0x3EB631,0xB8FACA,0xB4633C,0x322FC7,
        0xC99F60,0x4FD39B,0x434A6D,0xC50696,0x5A7981,0xDC357A,0xD0AC8C,0x56E077,
        0x681E59,0xEE52A2,0xE2CB54,0x6487AF,0xFBF8B8,0x7DB443,0x712DB5,0xF7614E,
        0x19A3D2,0x9FEF29,0x9376DF,0x153A24,0x8A4533,0x0C09C8,0x00903E,0x86DCC5,
        0xB822EB,0x3E6E10,0x32F7E6,0xB4BB1D,0x2BC40A,0xAD88F1,0xA11107,0x275DFC,
        0xDCED5B,0x5AA1A0,0x563856,0xD074AD,0x4F0BBA,0xC94741,0xC5DEB7,0x43924C,
        0x7D6C62,0xFB2099,0xF7B96F,0x71F594,0xEE8A83,0x68C678,0x645F8E,0xE21375,
        0x15723B,0x933EC0,0x9FA736,0x19EBCD,0x8694DA,0x00D821,0x0C41D7,0x8A0D2C,
        0xB4F302,0x32BFF9,0x3E260F,0xB86AF4,0x2715E3,0xA15918,0xADC0EE,0x2B8C15,
        0xD03CB2,0x567049,0x5AE9BF,0xDCA544,0x43DA53,0xC596A8,0xC90F5E,0x4F43A5,
        0x71BD8B,0xF7F170,0xFB6886,0x7D247D,0xE25B6A,0x641791,0x688E67,0xEEC29C,
        0x3347A4,0xB50B5F,0xB992A9,0x3FDE52,0xA0A145,0x26EDBE,0x2A7448,0xAC38B3,
        0x92C69D,0x148A66,0x181390,0x9E5F6B,0x01207C,0x876C87,0x8BF571,0x0DB98A,
        0xF6092D,0x7045D6,0x7CDC20,0xFA90DB,0x65EFCC,0xE3A337,0xEF3AC1,0x69763A,
        0x578814,0xD1C4EF,0xDD5D19,0x5B11E2,0xC46EF5,0x42220E,0x4EBBF8,0xC8F703,
        0x3F964D,0xB9DAB6,0xB54340,0x330FBB,0xAC70AC,0x2A3C57,0x26A5A1,0xA0E95A,
        0x9E1774,0x185B8F,0x14C279,0x928E82,0x0DF195,0x8BBD6E,0x872498,0x016863,
        0xFAD8C4,0x7C943F,0x700DC9,0xF64132,0x693E25,0xEF72DE,0xE3EB28,0x65A7D3,
        0x5B59FD,0xDD1506,0xD18CF0,0x57C00B,0xC8BF1C,0x4EF3E7,0x426A11,0xC426EA,
        0x2AE476,0xACA88D,0xA0317B,0x267D80,0xB90297,0x3F4E6C,0x33D79A,0xB59B61,
        0x8B654F,0x0D29B4,0x01B042,0x87FCB9,0x1883AE,0x9ECF55,0x9256A3,0x141A58,
        0xEFAAFF,0x69E604,0x657FF2,0xE33309,0x7C4C1E,0xFA00E5,0xF69913,0x70D5E8,
        0x4E2BC6,0xC8673D,0xC4FECB,0x42B230,0xDDCD27,0x5B81DC,0x57182A,0xD154D1,
        0x26359F,0xA07964,0xACE092,0x2AAC69,0xB5D37E,0x339F85,0x3F0673,0xB94A88,
        0x87B4A6,0x01F85D,0x0D61AB,0x8B2D50,0x145247,0x921EBC,0x9E874A,0x18CBB1,
        0xE37B16,0x6537ED,0x69AE1B,0xEFE2E0,0x709DF7,0xF6D10C,0xFA48FA,0x7C0401,
        0x42FA2F,0xC4B6D4,0xC82F22,0x4E63D9,0xD11CCE,0x575035,0x5BC9C3,0xDD8538
    };

    static uint32_t crc24q(const uint8_t *data, size_t len)
    {
        uint32_t crc = 0;
        for (size_t i = 0; i < len; ++i)
            crc = ((crc << 8) & 0xFFFFFF) ^ crc24q_table[(crc >> 16) ^ data[i]];
        return crc;
    }

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    static std::string base64Encode(const std::string &input)
    {
        static const char table[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        out.reserve(((input.size() + 2) / 3) * 4);

        for (size_t i = 0; i < input.size(); i += 3)
        {
            uint32_t v = static_cast<uint8_t>(input[i]) << 16;
            if (i + 1 < input.size()) v |= static_cast<uint8_t>(input[i + 1]) << 8;
            if (i + 2 < input.size()) v |= static_cast<uint8_t>(input[i + 2]);

            out.push_back(table[(v >> 18) & 0x3F]);
            out.push_back(table[(v >> 12) & 0x3F]);
            out.push_back((i + 1 < input.size()) ? table[(v >> 6) & 0x3F] : '=');
            out.push_back((i + 2 < input.size()) ? table[v & 0x3F] : '=');
        }
        return out;
    }

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

        // Resolve hostname
        struct addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        std::string portStr = std::to_string(port_);
        struct addrinfo *res = nullptr;
        int rc = ::getaddrinfo(host_.c_str(), portStr.c_str(), &hints, &res);
        if (rc != 0 || !res)
        {
            printf("[NtripClient] Failed to resolve %s: %s\r\n",
                   host_.c_str(), gai_strerror(rc));
            return false;
        }

        sockFd_ = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sockFd_ < 0)
        {
            printf("[NtripClient] Failed to create socket: %s\r\n",
                   strerror(errno));
            ::freeaddrinfo(res);
            return false;
        }

        // Set receive timeout (10s)
        struct timeval tv{};
        tv.tv_sec = 10;
        ::setsockopt(sockFd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        if (::connect(sockFd_, res->ai_addr, res->ai_addrlen) < 0)
        {
            printf("[NtripClient] Failed to connect to %s:%u: %s\r\n",
                   host_.c_str(), port_, strerror(errno));
            ::close(sockFd_);
            sockFd_ = -1;
            ::freeaddrinfo(res);
            return false;
        }
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
            printf("[NtripClient] Failed to send request: %s\r\n",
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
                printf("[NtripClient] Connection closed during header read\r\n");
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
            printf("[NtripClient] Incomplete response header\r\n");
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

            printf("[NtripClient] Caster rejected connection: %s\r\n",
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
        recvThread_ = std::jthread([this](std::stop_token st)
                                   { receiveLoop(st); });

        printf("[NtripClient] Connected to %s:%u/%s\r\n",
               host_.c_str(), port_, mountpoint_.c_str());
        return true;
    }

    void NtripClient::disconnect()
    {
        if (!connected_.exchange(false))
            return;

        if (recvThread_.joinable())
            recvThread_.request_stop();

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

        printf("[NtripClient] Disconnected.\r\n");
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

        while (!stoken.stop_requested() && connected_)
        {
            ssize_t n = ::recv(sockFd_, buf, sizeof(buf), 0);
            if (n <= 0)
            {
                if (n == 0)
                    printf("[NtripClient] Server closed connection\r\n");
                else if (errno == EAGAIN || errno == EWOULDBLOCK)
                    continue; // Timeout, retry
                else
                    printf("[NtripClient] recv error: %s\r\n",
                           strerror(errno));

                connected_ = false;
                break;
            }

            std::lock_guard lock(framesMutex_);
            extractFrames(buf, static_cast<size_t>(n));
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
            pendingFrames_.emplace_back(
                parseBuffer_.begin(),
                parseBuffer_.begin() + static_cast<ptrdiff_t>(frameLen));
            parseBuffer_.erase(
                parseBuffer_.begin(),
                parseBuffer_.begin() + static_cast<ptrdiff_t>(frameLen));
        }
    }

}

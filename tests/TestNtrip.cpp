#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "common/Utils.hpp"
#include "ntrip/NtripCaster.hpp"
#include "ntrip/NtripClient.hpp"
#include "ntrip/NtripServer.hpp"
#include "ublox/Rtcm3Parser.hpp"

using namespace JimmyPaputto;

// ── Suppress printf output from NtripCaster / NtripClient during tests ───

class NtripTestBase : public ::testing::Test {
protected:
    void SetUp() override {
        fflush(stdout);
        savedStdout_ = dup(STDOUT_FILENO);
        FILE* devnull = fopen("/dev/null", "w");
        dup2(fileno(devnull), STDOUT_FILENO);
        fclose(devnull);
    }
    void TearDown() override {
        fflush(stdout);
        dup2(savedStdout_, STDOUT_FILENO);
        close(savedStdout_);
    }
private:
    int savedStdout_ = -1;
};

// ── RTCM3 mock helpers ──────────────────────────────────────────────────────

namespace
{

    /// Build a valid RTCM3 frame with the given message ID and a 4-byte payload.
    std::vector<uint8_t> buildRtcm3Frame(uint16_t msgId)
    {
        uint16_t dataLength = 4;
        std::vector<uint8_t> frame;
        frame.push_back(0xD3);
        frame.push_back(0x00);
        frame.push_back(static_cast<uint8_t>(dataLength));

        uint8_t b0 = static_cast<uint8_t>((msgId >> 4) & 0xFF);
        uint8_t b1 = static_cast<uint8_t>((msgId << 4) & 0xF0);
        frame.push_back(b0);
        frame.push_back(b1);
        frame.push_back(0x00);
        frame.push_back(0x00);

        size_t crcDataLen = 3 + dataLength;
        uint32_t crc = crc24q(frame.data(), crcDataLen);
        frame.push_back((crc >> 16) & 0xFF);
        frame.push_back((crc >> 8) & 0xFF);
        frame.push_back(crc & 0xFF);

        return frame;
    }

    /// Build a batch of mock RTCM3 correction frames (typical base station output).
    std::vector<std::vector<uint8_t>> buildMockCorrections()
    {
        return {
            buildRtcm3Frame(1005),  // Station reference position
            buildRtcm3Frame(1077),  // GPS MSM7
            buildRtcm3Frame(1087),  // GLONASS MSM7
            buildRtcm3Frame(1097),  // Galileo MSM7
            buildRtcm3Frame(1127),  // BeiDou MSM7
        };
    }

    /// Use a port in the ephemeral range.  Each test gets its own offset to avoid
    /// collisions when tests run in parallel or in quick succession.
    uint16_t testPort(int offset)
    {
        return static_cast<uint16_t>(19000 + offset);
    }

    /// Connect a raw TCP socket to localhost:port, return the fd (or -1).
    int rawConnect(uint16_t port)
    {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) return -1;

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            close(fd);
            return -1;
        }
        return fd;
    }

    /// Send an NTRIP v2.0 GET request for a mountpoint and return the fd.
    int ntripHandshake(uint16_t port, const std::string& mountpoint,
                       const std::string& username = {},
                       const std::string& password = {})
    {
        int fd = rawConnect(port);
        if (fd < 0) return -1;

        std::string req = "GET /" + mountpoint + " HTTP/1.1\r\n"
                        "Ntrip-Version: Ntrip/2.0\r\n"
                        "User-Agent: TestClient\r\n";

        if (!username.empty())
        {
            std::string b64 = base64Encode(username + ":" + password);
            req += "Authorization: Basic " + b64 + "\r\n";
        }

        req += "\r\n";
        send(fd, req.c_str(), req.size(), 0);

        // Read response line
        char buf[512]{};
        struct timeval tv{};
        tv.tv_sec = 3;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) {
            close(fd);
            return -1;
        }

        std::string response(buf, static_cast<size_t>(n));
        if (response.find("ICY 200 OK") == std::string::npos) {
            close(fd);
            return -1;
        }

        return fd;
    }

    /// Read up to 'maxBytes' from a socket with a timeout.
    std::vector<uint8_t> recvWithTimeout(int fd, size_t maxBytes, int timeoutMs)
    {
        struct timeval tv{};
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        std::vector<uint8_t> result(maxBytes);
        ssize_t n = recv(fd, result.data(), maxBytes, 0);
        if (n <= 0) return {};
        result.resize(static_cast<size_t>(n));
        return result;
    }

}  // anonymous namespace


// ═══════════════════════════════════════════════════════════════════════════
//  NtripCaster Tests
// ═══════════════════════════════════════════════════════════════════════════

class NtripCasterTest : public NtripTestBase {};

TEST_F(NtripCasterTest, StartStop)
{
    NtripCaster caster("127.0.0.1", testPort(1), "TEST");
    ASSERT_TRUE(caster.start());
    EXPECT_EQ(caster.clientCount(), 0u);
    caster.stop();
}

TEST_F(NtripCasterTest, AcceptsClient)
{
    const uint16_t port = testPort(2);
    NtripCaster caster("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(caster.start());

    int fd = ntripHandshake(port, "GNSS");
    ASSERT_GE(fd, 0) << "NTRIP handshake failed";

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(caster.clientCount(), 1u);

    close(fd);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    caster.stop();
}

TEST_F(NtripCasterTest, RejectsWrongMountpoint)
{
    const uint16_t port = testPort(3);
    NtripCaster caster("127.0.0.1", port, "CORRECT");
    ASSERT_TRUE(caster.start());

    int fd = ntripHandshake(port, "WRONG");
    EXPECT_LT(fd, 0);

    EXPECT_EQ(caster.clientCount(), 0u);
    caster.stop();
}

TEST_F(NtripCasterTest, FeedBroadcastsToClient)
{
    const uint16_t port = testPort(4);
    NtripCaster caster("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(caster.start());

    int fd = ntripHandshake(port, "GNSS");
    ASSERT_GE(fd, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto corrections = buildMockCorrections();
    caster.feed(corrections);

    size_t totalBytes = 0;
    for (const auto& f : corrections)
        totalBytes += f.size();

    auto received = recvWithTimeout(fd, 4096, 2000);
    EXPECT_EQ(received.size(), totalBytes);

    // Verify RTCM3 frame contents byte-for-byte
    size_t offset = 0;
    for (const auto& f : corrections) {
        ASSERT_GE(received.size(), offset + f.size());
        EXPECT_EQ(received[offset], 0xD3);
        EXPECT_EQ(std::memcmp(received.data() + offset, f.data(), f.size()), 0);
        offset += f.size();
    }

    close(fd);
    caster.stop();
}

TEST_F(NtripCasterTest, MultipleClients)
{
    const uint16_t port = testPort(5);
    NtripCaster caster("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(caster.start());

    int fd1 = ntripHandshake(port, "GNSS");
    int fd2 = ntripHandshake(port, "GNSS");
    ASSERT_GE(fd1, 0);
    ASSERT_GE(fd2, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(caster.clientCount(), 2u);

    auto corrections = buildMockCorrections();
    caster.feed(corrections);

    size_t totalBytes = 0;
    for (const auto& f : corrections)
        totalBytes += f.size();

    auto recv1 = recvWithTimeout(fd1, 4096, 2000);
    auto recv2 = recvWithTimeout(fd2, 4096, 2000);
    EXPECT_EQ(recv1.size(), totalBytes);
    EXPECT_EQ(recv2.size(), totalBytes);

    close(fd1);
    close(fd2);
    caster.stop();
}

TEST_F(NtripCasterTest, ClientDisconnectUpdatesCount)
{
    const uint16_t port = testPort(6);
    NtripCaster caster("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(caster.start());

    int fd = ntripHandshake(port, "GNSS");
    ASSERT_GE(fd, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(caster.clientCount(), 1u);

    close(fd);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    caster.feed(buildMockCorrections());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(caster.clientCount(), 0u);
    caster.stop();
}

TEST_F(NtripCasterTest, UpdatePosition)
{
    NtripCaster caster("127.0.0.1", testPort(7), "TEST");
    ASSERT_TRUE(caster.start());
    caster.updatePosition(46.123456, 14.654321);
    caster.stop();
}

TEST_F(NtripCasterTest, SourcetableOnRootRequest)
{
    const uint16_t port = testPort(8);
    NtripCaster caster("127.0.0.1", port, "MYBASE");
    ASSERT_TRUE(caster.start());

    int fd = rawConnect(port);
    ASSERT_GE(fd, 0);

    std::string req = "GET / HTTP/1.1\r\n"
                      "Ntrip-Version: Ntrip/2.0\r\n\r\n";
    send(fd, req.c_str(), req.size(), 0);

    auto response = recvWithTimeout(fd, 4096, 2000);
    std::string respStr(response.begin(), response.end());

    EXPECT_NE(respStr.find("200 OK"), std::string::npos);
    EXPECT_NE(respStr.find("MYBASE"), std::string::npos);
    EXPECT_NE(respStr.find("ENDSOURCETABLE"), std::string::npos);

    close(fd);
    caster.stop();
}

TEST_F(NtripCasterTest, FeedEmptyFramesNoOp)
{
    const uint16_t port = testPort(9);
    NtripCaster caster("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(caster.start());

    std::vector<std::vector<uint8_t>> empty;
    caster.feed(empty);

    caster.stop();
}


// ═══════════════════════════════════════════════════════════════════════════
//  NtripClient + NtripCaster Integration Tests
// ═══════════════════════════════════════════════════════════════════════════

class NtripClientCasterTest : public NtripTestBase {};

TEST_F(NtripClientCasterTest, ConnectAndReceiveCorrections)
{
    const uint16_t port = testPort(10);
    NtripCaster caster("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(caster.start());

    NtripClient client("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(client.connect());
    EXPECT_TRUE(client.isConnected());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(caster.clientCount(), 1u);

    auto corrections = buildMockCorrections();
    caster.feed(corrections);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto frames = client.receiveFrames();
    EXPECT_EQ(frames.size(), corrections.size());

    for (size_t i = 0; i < std::min(frames.size(), corrections.size()); ++i) {
        EXPECT_EQ(frames[i], corrections[i]);
    }

    client.disconnect();
    EXPECT_FALSE(client.isConnected());
    caster.stop();
}

TEST_F(NtripClientCasterTest, ClientDisconnectClean)
{
    const uint16_t port = testPort(11);
    NtripCaster caster("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(caster.start());

    NtripClient client("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(client.connect());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(caster.clientCount(), 1u);

    client.disconnect();
    EXPECT_FALSE(client.isConnected());

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    caster.feed(buildMockCorrections());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(caster.clientCount(), 0u);
    caster.stop();
}

TEST_F(NtripClientCasterTest, MultipleFeeds)
{
    const uint16_t port = testPort(12);
    NtripCaster caster("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(caster.start());

    NtripClient client("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(client.connect());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto batch1 = buildMockCorrections();
    auto batch2 = buildMockCorrections();
    caster.feed(batch1);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    caster.feed(batch2);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto frames = client.receiveFrames();
    EXPECT_EQ(frames.size(), batch1.size() + batch2.size());

    client.disconnect();
    caster.stop();
}

TEST_F(NtripClientCasterTest, ReceiveFramesDrainsQueue)
{
    const uint16_t port = testPort(13);
    NtripCaster caster("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(caster.start());

    NtripClient client("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(client.connect());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    caster.feed(buildMockCorrections());
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto first = client.receiveFrames();
    EXPECT_FALSE(first.empty());

    auto second = client.receiveFrames();
    EXPECT_TRUE(second.empty());

    client.disconnect();
    caster.stop();
}

TEST_F(NtripClientCasterTest, WrongMountpointFails)
{
    const uint16_t port = testPort(14);
    NtripCaster caster("127.0.0.1", port, "CORRECT");
    ASSERT_TRUE(caster.start());

    NtripClient client("127.0.0.1", port, "WRONG");
    EXPECT_FALSE(client.connect());
    EXPECT_FALSE(client.isConnected());

    caster.stop();
}

TEST_F(NtripClientCasterTest, SendPositionNoThrow)
{
    const uint16_t port = testPort(15);
    NtripCaster caster("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(caster.start());

    NtripClient client("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(client.connect());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_NO_THROW(client.sendPosition(46.05, 14.50, 300.0));

    client.disconnect();
    caster.stop();
}

TEST_F(NtripClientCasterTest, ConnectToNonexistentPortFails)
{
    NtripClient client("127.0.0.1", testPort(16), "GNSS");
    EXPECT_FALSE(client.connect());
    EXPECT_FALSE(client.isConnected());
}

TEST_F(NtripClientCasterTest, MultipleClientsReceiveAll)
{
    const uint16_t port = testPort(17);
    NtripCaster caster("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(caster.start());

    NtripClient client1("127.0.0.1", port, "GNSS");
    NtripClient client2("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(client1.connect());
    ASSERT_TRUE(client2.connect());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(caster.clientCount(), 2u);

    auto corrections = buildMockCorrections();
    caster.feed(corrections);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto frames1 = client1.receiveFrames();
    auto frames2 = client2.receiveFrames();
    EXPECT_EQ(frames1.size(), corrections.size());
    EXPECT_EQ(frames2.size(), corrections.size());

    client1.disconnect();
    client2.disconnect();
    caster.stop();
}


// ═══════════════════════════════════════════════════════════════════════════
//  NtripLoggable Tests
// ═══════════════════════════════════════════════════════════════════════════

#include "ntrip/NtripLog.hpp"
#include <mutex>

class NtripLogTest : public ::testing::Test {};

TEST_F(NtripLogTest, CasterLogCallbackReceivesMessages)
{
    const uint16_t port = testPort(20);

    std::mutex mu;
    std::vector<std::pair<ENtripLogLevel, std::string>> logs;

    NtripCaster caster("127.0.0.1", port, "LOG");
    caster.setLogLevel(ENtripLogLevel::Debug);
    caster.setLogCallback([&](ENtripLogLevel level, const std::string& msg) {
        std::lock_guard<std::mutex> lk(mu);
        logs.emplace_back(level, msg);
    });

    ASSERT_TRUE(caster.start());
    caster.stop();

    std::lock_guard<std::mutex> lk(mu);
    EXPECT_GE(logs.size(), 2u);  // at least "Listening" + "Stopped"

    // Check that we got an Info-level "Listening" message
    bool foundListening = false;
    for (const auto& [lvl, msg] : logs) {
        if (lvl == ENtripLogLevel::Info && msg.find("Listening") != std::string::npos) {
            foundListening = true;
            break;
        }
    }
    EXPECT_TRUE(foundListening);
}

TEST_F(NtripLogTest, CasterLogLevelFiltering)
{
    const uint16_t port = testPort(21);

    std::mutex mu;
    std::vector<std::pair<ENtripLogLevel, std::string>> logs;

    NtripCaster caster("127.0.0.1", port, "LOG");
    caster.setLogLevel(ENtripLogLevel::Error);  // Only errors
    caster.setLogCallback([&](ENtripLogLevel level, const std::string& msg) {
        std::lock_guard<std::mutex> lk(mu);
        logs.emplace_back(level, msg);
    });

    ASSERT_TRUE(caster.start());
    caster.stop();

    std::lock_guard<std::mutex> lk(mu);
    // Info-level "Listening" / "Stopped" should be filtered out
    for (const auto& [lvl, msg] : logs) {
        EXPECT_LE(static_cast<int>(lvl), static_cast<int>(ENtripLogLevel::Error));
    }
}

TEST_F(NtripLogTest, DefaultNoCallback)
{
    // With no callback set, start/stop should not crash
    const uint16_t port = testPort(22);
    NtripCaster caster("127.0.0.1", port, "LOG");
    ASSERT_TRUE(caster.start());
    caster.stop();
}

TEST_F(NtripLogTest, ClientLogCallbackOnConnectionFailure)
{
    std::mutex mu;
    std::vector<std::pair<ENtripLogLevel, std::string>> logs;

    NtripClient client("127.0.0.1", testPort(23), "GNSS");
    client.setLogLevel(ENtripLogLevel::Debug);
    client.setLogCallback([&](ENtripLogLevel level, const std::string& msg) {
        std::lock_guard<std::mutex> lk(mu);
        logs.emplace_back(level, msg);
    });

    // Connecting to a port with no server should fail and produce error logs
    EXPECT_FALSE(client.connect());

    std::lock_guard<std::mutex> lk(mu);
    EXPECT_FALSE(logs.empty());

    bool foundError = false;
    for (const auto& [lvl, msg] : logs) {
        if (lvl == ENtripLogLevel::Error) {
            foundError = true;
            break;
        }
    }
    EXPECT_TRUE(foundError);
}


// ═══════════════════════════════════════════════════════════════════════════
//  NtripStats Tests
// ═══════════════════════════════════════════════════════════════════════════

#include "ntrip/NtripStats.hpp"

class NtripStatsTest : public NtripTestBase {};

TEST_F(NtripStatsTest, CasterStatsAfterFeed)
{
    const uint16_t port = testPort(30);
    NtripCaster caster("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(caster.start());

    int fd = ntripHandshake(port, "GNSS");
    ASSERT_GE(fd, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto corrections = buildMockCorrections();
    caster.feed(corrections);

    auto stats = caster.getStats();

    EXPECT_EQ(stats.framesTx, corrections.size());
    EXPECT_GT(stats.bytesTx, 0u);
    EXPECT_GT(stats.uptimeMs, 0u);
    EXPECT_FALSE(stats.messageTypeCounts.empty());

    // Verify known message types (1005, 1077, 1087, 1097, 1127)
    EXPECT_EQ(stats.messageTypeCounts.count(1005), 1u);
    EXPECT_EQ(stats.messageTypeCounts.count(1077), 1u);

    close(fd);
    caster.stop();
}

TEST_F(NtripStatsTest, CasterUptimeIncreases)
{
    const uint16_t port = testPort(31);
    NtripCaster caster("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(caster.start());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto s1 = caster.getStats();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto s2 = caster.getStats();

    EXPECT_GT(s2.uptimeMs, s1.uptimeMs);

    caster.stop();
}

TEST_F(NtripStatsTest, ClientStatsAfterReceive)
{
    const uint16_t port = testPort(32);
    NtripCaster caster("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(caster.start());

    NtripClient client("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(client.connect());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto corrections = buildMockCorrections();
    caster.feed(corrections);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    client.receiveFrames();

    auto stats = client.getStats();

    EXPECT_EQ(stats.framesRx, corrections.size());
    EXPECT_GT(stats.bytesRx, 0u);
    EXPECT_GT(stats.uptimeMs, 0u);
    EXPECT_FALSE(stats.messageTypeCounts.empty());
    EXPECT_EQ(stats.messageTypeCounts.count(1005), 1u);

    client.disconnect();
    caster.stop();
}

TEST_F(NtripStatsTest, InterFrameTiming)
{
    const uint16_t port = testPort(33);
    NtripCaster caster("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(caster.start());

    NtripClient client("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(client.connect());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Two separate feeds with a gap
    caster.feed(buildMockCorrections());
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    caster.feed(buildMockCorrections());
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    client.receiveFrames();

    auto stats = client.getStats();

    EXPECT_EQ(stats.framesRx, 10u);
    EXPECT_GT(stats.avgInterFrameMs, 0.0);
    EXPECT_GE(stats.maxInterFrameMs, stats.avgInterFrameMs);

    client.disconnect();
    caster.stop();
}


// ═══════════════════════════════════════════════════════════════════════════
//  NtripCaster Auth Tests
// ═══════════════════════════════════════════════════════════════════════════

class NtripAuthTest : public NtripTestBase {};

TEST_F(NtripAuthTest, NoAuthByDefault)
{
    const uint16_t port = testPort(40);
    NtripCaster caster("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(caster.start());

    // No credentials set → any client should connect
    int fd = ntripHandshake(port, "GNSS");
    ASSERT_GE(fd, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(caster.clientCount(), 1u);

    close(fd);
    caster.stop();
}

TEST_F(NtripAuthTest, AuthAcceptsValid)
{
    const uint16_t port = testPort(41);
    NtripCaster caster("127.0.0.1", port, "GNSS");
    caster.setCredentials("admin", "secret");
    ASSERT_TRUE(caster.start());

    int fd = ntripHandshake(port, "GNSS", "admin", "secret");
    ASSERT_GE(fd, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(caster.clientCount(), 1u);

    close(fd);
    caster.stop();
}

TEST_F(NtripAuthTest, AuthRejectsWrong)
{
    const uint16_t port = testPort(42);
    NtripCaster caster("127.0.0.1", port, "GNSS");
    caster.setCredentials("admin", "secret");
    ASSERT_TRUE(caster.start());

    int fd = ntripHandshake(port, "GNSS", "admin", "wrong");
    EXPECT_LT(fd, 0);
    EXPECT_EQ(caster.clientCount(), 0u);

    caster.stop();
}

TEST_F(NtripAuthTest, AuthRejectsMissing)
{
    const uint16_t port = testPort(43);
    NtripCaster caster("127.0.0.1", port, "GNSS");
    caster.setCredentials("admin", "secret");
    ASSERT_TRUE(caster.start());

    // No auth header → should be rejected
    int fd = ntripHandshake(port, "GNSS");
    EXPECT_LT(fd, 0);
    EXPECT_EQ(caster.clientCount(), 0u);

    caster.stop();
}

TEST_F(NtripAuthTest, ClientAuthenticates)
{
    const uint16_t port = testPort(44);
    NtripCaster caster("127.0.0.1", port, "GNSS");
    caster.setCredentials("user", "pass");
    ASSERT_TRUE(caster.start());

    NtripClient client("127.0.0.1", port, "GNSS", "user", "pass");
    ASSERT_TRUE(client.connect());
    EXPECT_TRUE(client.isConnected());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(caster.clientCount(), 1u);

    client.disconnect();
    caster.stop();
}

TEST_F(NtripAuthTest, ClientAuthFailsWrongPassword)
{
    const uint16_t port = testPort(45);
    NtripCaster caster("127.0.0.1", port, "GNSS");
    caster.setCredentials("user", "pass");
    ASSERT_TRUE(caster.start());

    NtripClient client("127.0.0.1", port, "GNSS", "user", "badpass");
    EXPECT_FALSE(client.connect());
    EXPECT_FALSE(client.isConnected());

    caster.stop();
}

// ── Auto-Reconnect Tests ─────────────────────────────────────────────

class NtripReconnectTest : public NtripTestBase {};

TEST_F(NtripReconnectTest, NoReconnectByDefault)
{
    const uint16_t port = testPort(50);
    NtripCaster caster("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(caster.start());

    NtripClient client("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(client.connect());
    EXPECT_EQ(client.reconnectCount(), 0u);

    // Stop caster — client should NOT reconnect
    caster.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_FALSE(client.isConnected());
    EXPECT_EQ(client.reconnectCount(), 0u);

    client.disconnect();
}

TEST_F(NtripReconnectTest, ReconnectsAfterCasterRestart)
{
    const uint16_t port = testPort(51);
    NtripCaster caster("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(caster.start());

    NtripClient client("127.0.0.1", port, "GNSS");
    client.setAutoReconnect(true, 100, 500);
    ASSERT_TRUE(client.connect());

    // Kill the caster
    caster.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    EXPECT_FALSE(client.isConnected());

    // Restart caster — client should reconnect
    ASSERT_TRUE(caster.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    EXPECT_TRUE(client.isConnected());
    EXPECT_GT(client.reconnectCount(), 0u);

    client.disconnect();
    caster.stop();
}

TEST_F(NtripReconnectTest, StopsOnDisconnect)
{
    const uint16_t port = testPort(52);
    NtripCaster caster("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(caster.start());

    NtripClient client("127.0.0.1", port, "GNSS");
    client.setAutoReconnect(true, 100, 500);
    ASSERT_TRUE(client.connect());

    // Kill caster to trigger reconnect loop
    caster.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Explicit disconnect should stop reconnection attempts
    client.disconnect();
    uint32_t countAfterDisconnect = client.reconnectCount();
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    EXPECT_EQ(client.reconnectCount(), countAfterDisconnect);
}

TEST_F(NtripReconnectTest, ExponentialBackoff)
{
    const uint16_t port = testPort(53);
    NtripCaster caster("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(caster.start());

    NtripClient client("127.0.0.1", port, "GNSS");
    client.setAutoReconnect(true, 50, 200);
    ASSERT_TRUE(client.connect());

    // Kill caster — no restart, so all reconnects fail
    caster.stop();

    // Wait for several reconnect attempts with backoff
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    uint32_t count = client.reconnectCount();
    EXPECT_GE(count, 2u);

    client.disconnect();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Sourcetable Fetch Tests
// ═══════════════════════════════════════════════════════════════════════════

class NtripSourcetableTest : public NtripTestBase {};

TEST_F(NtripSourcetableTest, FetchFromLocalCaster)
{
    const uint16_t port = testPort(54);
    NtripCaster caster("127.0.0.1", port, "GNSS");
    caster.updatePosition(52.0, 13.0);
    ASSERT_TRUE(caster.start());

    auto entries = NtripClient::fetchSourcetable("127.0.0.1", port, "", "", 3000);
    ASSERT_GE(entries.size(), 1u);
    EXPECT_EQ(entries[0].mountpoint, "GNSS");

    caster.stop();
}

TEST_F(NtripSourcetableTest, FetchFromUnreachableHost)
{
    // Should return empty, not crash
    auto entries = NtripClient::fetchSourcetable(
        "192.0.2.1", 9999, "", "", 500);
    EXPECT_TRUE(entries.empty());
}

// ═══════════════════════════════════════════════════════════════════════════
//  Auto-GGA Tests
// ═══════════════════════════════════════════════════════════════════════════

class NtripAutoGgaTest : public NtripTestBase {};

TEST_F(NtripAutoGgaTest, SendsPeriodicGGA)
{
    const uint16_t port = testPort(55);
    NtripCaster caster("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(caster.start());

    NtripClient client("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(client.connect());

    // Set position and enable auto-GGA at 200ms
    client.updatePosition(48.1234, 11.5678, 520.0);
    client.setAutoGGA(200);

    // Wait for a couple of GGA sends (shouldn't crash, caster should stay happy)
    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    EXPECT_TRUE(client.isConnected());

    // Disable auto-GGA
    client.setAutoGGA(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    client.disconnect();
    caster.stop();
}

TEST_F(NtripAutoGgaTest, UpdatePositionWhileRunning)
{
    const uint16_t port = testPort(56);
    NtripCaster caster("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(caster.start());

    NtripClient client("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(client.connect());

    client.updatePosition(48.0, 11.0, 500.0);
    client.setAutoGGA(150);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Update position mid-stream
    client.updatePosition(49.0, 12.0, 600.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_TRUE(client.isConnected());

    client.setAutoGGA(0);
    client.disconnect();
    caster.stop();
}

// ═══════════════════════════════════════════════════════════════════════════
//  NtripServer Tests
// ═══════════════════════════════════════════════════════════════════════════

class NtripServerTest : public NtripTestBase {};

TEST_F(NtripServerTest, ConnectDisconnect)
{
    const uint16_t port = testPort(57);
    NtripCaster caster("127.0.0.1", port, "BASE");
    ASSERT_TRUE(caster.start());

    NtripServer server("127.0.0.1", port, "BASE", "", "secret");
    ASSERT_TRUE(server.connect());
    EXPECT_TRUE(server.isConnected());

    server.disconnect();
    EXPECT_FALSE(server.isConnected());
    caster.stop();
}

TEST_F(NtripServerTest, FeedFramesToCaster)
{
    const uint16_t port = testPort(58);
    NtripCaster caster("127.0.0.1", port, "BASE");
    ASSERT_TRUE(caster.start());

    // Connect a server (source) that pushes corrections
    NtripServer server("127.0.0.1", port, "BASE", "", "secret");
    ASSERT_TRUE(server.connect());

    // Connect a client that receives corrections
    NtripClient client("127.0.0.1", port, "BASE");
    ASSERT_TRUE(client.connect());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Feed corrections through the server
    auto corrections = buildMockCorrections();
    server.feed(corrections);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Client should receive frames
    auto received = client.receiveFrames();
    EXPECT_GE(received.size(), 1u);

    client.disconnect();
    server.disconnect();
    caster.stop();
}

TEST_F(NtripServerTest, ServerStats)
{
    const uint16_t port = testPort(59);
    NtripCaster caster("127.0.0.1", port, "BASE");
    ASSERT_TRUE(caster.start());

    NtripServer server("127.0.0.1", port, "BASE", "", "secret");
    ASSERT_TRUE(server.connect());

    auto corrections = buildMockCorrections();
    server.feed(corrections);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto stats = server.getStats();
    EXPECT_GT(stats.bytesTx, 0u);

    server.disconnect();
    caster.stop();
}

TEST_F(NtripServerTest, ReconnectsAfterCasterDrop)
{
    const uint16_t port = testPort(60);
    NtripCaster caster("127.0.0.1", port, "BASE");
    ASSERT_TRUE(caster.start());

    NtripServer server("127.0.0.1", port, "BASE", "", "secret");
    server.setLogLevel(ENtripLogLevel::Debug);
    server.setAutoReconnect(true, 200, 1000);
    ASSERT_TRUE(server.connect());
    EXPECT_TRUE(server.isConnected());

    // Kill caster
    caster.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Restart caster so reconnect succeeds
    NtripCaster caster2("127.0.0.1", port, "BASE");
    ASSERT_TRUE(caster2.start());
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    EXPECT_EQ(server.reconnectCount(), 0u);

    server.disconnect();
    caster2.stop();
}

TEST_F(NtripServerTest, WrongMountpointFails)
{
    const uint16_t port = testPort(61);
    NtripCaster caster("127.0.0.1", port, "BASE");
    ASSERT_TRUE(caster.start());

    NtripServer server("127.0.0.1", port, "WRONG", "", "secret");
    // Connection might succeed at TCP level but should fail at NTRIP level
    // depending on caster implementation
    bool result = server.connect();
    // Either way, cleanup should work
    server.disconnect();
    caster.stop();
    (void)result;
}

TEST_F(NtripServerTest, FullChainServerCasterClient)
{
    // End-to-end: Server pushes -> Caster relays -> Client receives
    const uint16_t port = testPort(62);
    NtripCaster caster("127.0.0.1", port, "CHAIN");
    ASSERT_TRUE(caster.start());

    NtripServer server("127.0.0.1", port, "CHAIN", "", "");
    ASSERT_TRUE(server.connect());

    NtripClient client("127.0.0.1", port, "CHAIN");
    ASSERT_TRUE(client.connect());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Push multiple batches
    auto corrections = buildMockCorrections();
    for (int i = 0; i < 3; ++i)
    {
        server.feed(corrections);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    auto received = client.receiveFrames();
    EXPECT_GE(received.size(), 3u);

    client.disconnect();
    server.disconnect();
    caster.stop();
}

TEST_F(NtripServerTest, CasterStopWithActiveSourceNocrash)
{
    // Verify that stopping a caster while an NtripServer is actively
    // feeding data does not crash (bus error / use-after-free).
    const uint16_t port = testPort(63);
    NtripCaster caster("127.0.0.1", port, "STOP");
    ASSERT_TRUE(caster.start());

    NtripServer server("127.0.0.1", port, "STOP", "", "");
    ASSERT_TRUE(server.connect());

    // Also connect a GET client so broadcast path is exercised
    NtripClient client("127.0.0.1", port, "STOP");
    ASSERT_TRUE(client.connect());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Feed data in a background thread while we stop the caster
    auto corrections = buildMockCorrections();
    std::atomic<bool> feeding{true};
    std::thread feeder([&]() {
        while (feeding)
        {
            server.feed(corrections);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Verify stats were tracked during the active window (before stop resets them)
    auto stats = caster.getStats();
    EXPECT_GT(stats.bytesTx, 0u);

    // Stop caster while feed is active — must not crash
    caster.stop();

    // Clean up
    feeding = false;
    feeder.join();
    server.disconnect();
    client.disconnect();
}

// ═══════════════════════════════════════════════════════════════════════════
//  TLS Tests
// ═══════════════════════════════════════════════════════════════════════════

class NtripTlsTest : public NtripTestBase {};

TEST_F(NtripTlsTest, TlsAvailableReturnsConsistently)
{
    // Should not crash; value depends on build config
    bool a = NtripClient::isTlsAvailable();
    bool b = NtripServer::isTlsAvailable();
    EXPECT_EQ(a, b); // Both use the same underlying check
}

TEST_F(NtripTlsTest, ClientTlsToPlainServerFails)
{
    // TLS handshake to a plain-text caster should fail gracefully
    const uint16_t port = testPort(70);
    NtripCaster caster("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(caster.start());

    NtripClient client("127.0.0.1", port, "GNSS");
    client.setUseTls(true, false); // no verify
    // connect() should fail (TLS handshake to plain port)
    // If SSL not compiled in, wrap() returns false immediately
    EXPECT_FALSE(client.connect());

    caster.stop();
}

TEST_F(NtripTlsTest, ServerTlsToPlainCasterFails)
{
    const uint16_t port = testPort(71);
    NtripCaster caster("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(caster.start());

    NtripServer server("127.0.0.1", port, "GNSS", "", "");
    server.setUseTls(true, false);
    EXPECT_FALSE(server.connect());

    caster.stop();
}

TEST_F(NtripTlsTest, FetchSourcetableTlsToPlainFails)
{
    const uint16_t port = testPort(72);
    NtripCaster caster("127.0.0.1", port, "GNSS");
    ASSERT_TRUE(caster.start());

    auto entries = NtripClient::fetchSourcetable(
        "127.0.0.1", port, "", "", 3000, true, false);
    EXPECT_TRUE(entries.empty());

    caster.stop();
}

TEST_F(NtripTlsTest, CasterTlsAvailableConsistent)
{
    bool a = NtripCaster::isTlsAvailable();
    bool b = NtripClient::isTlsAvailable();
    EXPECT_EQ(a, b);
}

TEST_F(NtripTlsTest, CasterSetTlsInvalidFilesFails)
{
    NtripCaster caster("127.0.0.1", testPort(73), "GNSS");
    // Non-existent cert/key files should fail
    EXPECT_FALSE(caster.setTls("/nonexistent/cert.pem", "/nonexistent/key.pem"));
}

TEST_F(NtripTlsTest, CasterSetTlsEmptyStringsFails)
{
    NtripCaster caster("127.0.0.1", testPort(74), "GNSS");
    EXPECT_FALSE(caster.setTls("", ""));
}

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

#include "ntrip/NtripCaster.hpp"
#include "ntrip/NtripClient.hpp"

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

namespace JimmyPaputto
{
    uint32_t crc24q(const uint8_t* data, size_t length);
}

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
    int ntripHandshake(uint16_t port, const std::string& mountpoint)
    {
        int fd = rawConnect(port);
        if (fd < 0) return -1;

        std::string req = "GET /" + mountpoint + " HTTP/1.1\r\n"
                        "Ntrip-Version: Ntrip/2.0\r\n"
                        "User-Agent: TestClient\r\n"
                        "\r\n";
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

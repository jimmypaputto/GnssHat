#include <gtest/gtest.h>
#include <vector>
#include <cstring>

#include "ublox/Rtcm3Parser.hpp"
#include "ublox/Rtcm3Store.hpp"


using namespace JimmyPaputto;

namespace JimmyPaputto
{
uint32_t crc24q(const uint8_t* data, size_t length);
}

namespace
{

std::vector<uint8_t> buildValidRtcm3Frame(uint16_t msgId)
{
    uint16_t dataLength = 4;
    std::vector<uint8_t> frame;
    frame.push_back(0xD3);
    frame.push_back(0x00);
    frame.push_back(dataLength);

    // msgId w 12 MSB payload
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

}  // namespace


TEST(Crc24q, KnownValues)
{
    uint8_t data[] = { 0xD3, 0x00, 0x00 };
    uint32_t crc = crc24q(data, 3);
    EXPECT_NE(crc, 0u);
    EXPECT_EQ(crc & 0xFF000000, 0u);
}

TEST(Crc24q, Deterministic)
{
    uint8_t data[] = { 0xD3, 0x00, 0x04, 0x01, 0x02, 0x03, 0x04 };
    uint32_t crc1 = crc24q(data, sizeof(data));
    uint32_t crc2 = crc24q(data, sizeof(data));
    EXPECT_EQ(crc1, crc2);
}

TEST(Crc24q, DifferentInputsDifferentCrc)
{
    uint8_t data1[] = { 0xD3, 0x00, 0x01, 0xAA };
    uint8_t data2[] = { 0xD3, 0x00, 0x01, 0xBB };
    EXPECT_NE(crc24q(data1, 4), crc24q(data2, 4));
}


TEST(Rtcm3Parser, ParsesValidFrame)
{
    auto frame = buildValidRtcm3Frame(1005);
    Rtcm3Store store;
    Rtcm3Parser parser(store);

    std::vector<uint8_t> unfinished;
    parser.parse(frame, unfinished);

    EXPECT_TRUE(unfinished.empty());
}

TEST(Rtcm3Parser, StoresFrameInStore)
{
    auto frame = buildValidRtcm3Frame(1077);
    Rtcm3Store store;
    Rtcm3Parser parser(store);

    std::vector<uint8_t> unfinished;
    parser.parse(frame, unfinished);

    auto stored = store.getFrame(1077);
    EXPECT_FALSE(stored.empty());
}

TEST(Rtcm3Parser, RejectsCorruptedCrc)
{
    auto frame = buildValidRtcm3Frame(1005);
    frame.back() ^= 0xFF; // corrupt CRC

    Rtcm3Store store;
    Rtcm3Parser parser(store);

    std::vector<uint8_t> unfinished;
    parser.parse(frame, unfinished);

    auto stored = store.getFrame(1005);
    EXPECT_TRUE(stored.empty());
}

TEST(Rtcm3Parser, HandlesIncompleteFrame)
{
    auto frame = buildValidRtcm3Frame(1005);
    frame.resize(frame.size() / 2);

    Rtcm3Store store;
    Rtcm3Parser parser(store);

    std::vector<uint8_t> unfinished;
    parser.parse(frame, unfinished);

    EXPECT_FALSE(unfinished.empty());
}

TEST(Rtcm3Parser, ParsesMultipleFrames)
{
    auto frame1 = buildValidRtcm3Frame(1005);
    auto frame2 = buildValidRtcm3Frame(1077);

    std::vector<uint8_t> buffer;
    buffer.insert(buffer.end(), frame1.begin(), frame1.end());
    buffer.insert(buffer.end(), frame2.begin(), frame2.end());

    Rtcm3Store store;
    Rtcm3Parser parser(store);

    std::vector<uint8_t> unfinished;
    parser.parse(buffer, unfinished);

    EXPECT_FALSE(store.getFrame(1005).empty());
    EXPECT_FALSE(store.getFrame(1077).empty());
}


TEST(Rtcm3Store, UpdateAndGetFrame)
{
    Rtcm3Store store;
    std::vector<uint8_t> data = { 0x01, 0x02, 0x03 };
    store.updateFrame(1005, data);

    auto result = store.getFrame(1005);
    EXPECT_EQ(result, data);
}

TEST(Rtcm3Store, GetNonExistentFrame)
{
    Rtcm3Store store;
    auto result = store.getFrame(9999);
    EXPECT_TRUE(result.empty());
}

TEST(Rtcm3Store, OverwriteExistingFrame)
{
    Rtcm3Store store;
    store.updateFrame(1005, { 0x01 });
    store.updateFrame(1005, { 0x02, 0x03 });

    auto result = store.getFrame(1005);
    EXPECT_EQ(result, (std::vector<uint8_t>{ 0x02, 0x03 }));
}

TEST(Rtcm3Store, GetFramesMultipleIds)
{
    Rtcm3Store store;
    store.updateFrame(1005, { 0xAA });
    store.updateFrame(1077, { 0xBB });
    store.updateFrame(1087, { 0xCC });

    std::vector<uint16_t> ids = { 1005, 1087 };
    auto frames = store.getFrames(ids);

    ASSERT_EQ(frames.size(), 2u);
}

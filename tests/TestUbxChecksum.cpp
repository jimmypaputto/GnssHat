#include <gtest/gtest.h>

#include "ublox/UbxParser.hpp"


TEST(UbxChecksum, KnownFrame)
{
    std::vector<uint8_t> frame = { 0xB5, 0x62, 0x01, 0x07, 0x00, 0x00 };
    JimmyPaputto::UbxParser::addChecksum(frame);

    EXPECT_EQ(frame.size(), 8u);
    EXPECT_TRUE(JimmyPaputto::UbxParser::checkFrame(frame));

    auto ck = JimmyPaputto::UbxParser::checksum(frame);
    EXPECT_EQ(frame[6], ck[0]);
    EXPECT_EQ(frame[7], ck[1]);
}

TEST(UbxChecksum, SingleBytePayload)
{
    // class=0x06 id=0x01 len=1 payload=0x42
    std::vector<uint8_t> frame = { 0xB5, 0x62, 0x06, 0x01, 0x01, 0x00, 0x42 };
    JimmyPaputto::UbxParser::addChecksum(frame);

    EXPECT_EQ(frame.size(), 9u);
    EXPECT_TRUE(JimmyPaputto::UbxParser::checkFrame(frame));
}

TEST(UbxChecksum, VerifyFletcherAlgorithm)
{
    // Fletcher checksum od bajtu 2 do size-2
    // {B5, 62, 01, 04, 00, 00, CK_A, CK_B} - po addChecksum
    std::vector<uint8_t> frame = { 0xB5, 0x62, 0x01, 0x04, 0x00, 0x00 };
    // addChecksum liczy checksum(frame, 0) → offset=0, iteruje [2..size)
    // i=2: cka=0x01, ckb=0x01
    // i=3: cka=0x05, ckb=0x06
    // i=4: cka=0x05, ckb=0x0B
    // i=5: cka=0x05, ckb=0x10
    JimmyPaputto::UbxParser::addChecksum(frame);
    EXPECT_EQ(frame[6], 0x05);
    EXPECT_EQ(frame[7], 0x10);
}

TEST(UbxCheckFrame, ValidFrame)
{
    std::vector<uint8_t> frame = { 0xB5, 0x62, 0x01, 0x04, 0x00, 0x00 };
    JimmyPaputto::UbxParser::addChecksum(frame);
    EXPECT_TRUE(JimmyPaputto::UbxParser::checkFrame(frame));
}

TEST(UbxCheckFrame, CorruptedChecksum)
{
    std::vector<uint8_t> frame = { 0xB5, 0x62, 0x01, 0x04, 0x00, 0x00 };
    JimmyPaputto::UbxParser::addChecksum(frame);
    frame.back() ^= 0xFF;
    EXPECT_FALSE(JimmyPaputto::UbxParser::checkFrame(frame));
}

TEST(UbxCheckFrame, CorruptedPayload)
{
    std::vector<uint8_t> frame = { 0xB5, 0x62, 0x06, 0x01, 0x02, 0x00, 0xAA, 0xBB };
    JimmyPaputto::UbxParser::addChecksum(frame);
    frame[6] = 0x00;
    EXPECT_FALSE(JimmyPaputto::UbxParser::checkFrame(frame));
}

TEST(UbxAddChecksum, InvalidFrameReturnsMarker)
{
    // buildFrame wewnętrznie sprawdza checkFrame po dodaniu - jeśli ramka byłaby
    // uszkodzona, addChecksum zmienia ją na {0xFF}. Ale normalnie addChecksum powinien działać.
    std::vector<uint8_t> frame = { 0xB5, 0x62, 0x0A, 0x38, 0x00, 0x00 };
    JimmyPaputto::UbxParser::addChecksum(frame);
    EXPECT_NE(frame[0], 0xFF);
    EXPECT_TRUE(JimmyPaputto::UbxParser::checkFrame(frame));
}

TEST(UbxChecksum, MultiplePayloadBytes)
{
    std::vector<uint8_t> frame = {
        0xB5, 0x62, 0x06, 0x8A, 0x05, 0x00,
        0x01, 0x02, 0x03, 0x04, 0x05
    };
    JimmyPaputto::UbxParser::addChecksum(frame);
    EXPECT_TRUE(JimmyPaputto::UbxParser::checkFrame(frame));

    std::vector<uint8_t> frame2 = {
        0xB5, 0x62, 0x06, 0x8A, 0x05, 0x00,
        0x01, 0x02, 0x03, 0x04, 0x05
    };
    JimmyPaputto::UbxParser::addChecksum(frame2);
    EXPECT_EQ(frame, frame2);
}

TEST(UbxChecksum, LargerPayload)
{
    std::vector<uint8_t> frame = { 0xB5, 0x62, 0x01, 0x07, 0x10, 0x00 };
    for (int i = 0; i < 16; i++)
        frame.push_back(static_cast<uint8_t>(i));

    JimmyPaputto::UbxParser::addChecksum(frame);
    EXPECT_TRUE(JimmyPaputto::UbxParser::checkFrame(frame));
}

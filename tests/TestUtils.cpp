#include <gtest/gtest.h>

#include "common/Utils.hpp"

#include "ublox/EFixType.hpp"
#include "ublox/EDynamicModel.hpp"


TEST(ReadLE, Uint16FromBuffer)
{
    std::vector<uint8_t> buf = { 0x00, 0x00, 0x34, 0x12 };
    auto val = readLE<uint16_t>(std::span<const uint8_t>(buf), 2);
    EXPECT_EQ(val, 0x1234);
}

TEST(ReadLE, Int32FromBuffer)
{
    std::vector<uint8_t> buf = { 0xFF, 0xFF, 0xFF, 0xFF };
    auto val = readLE<int32_t>(std::span<const uint8_t>(buf), 0);
    EXPECT_EQ(val, -1);
}

TEST(ReadLE, Uint32FromPointer)
{
    uint8_t data[] = { 0x78, 0x56, 0x34, 0x12 };
    auto val = readLE<uint32_t>(data);
    EXPECT_EQ(val, 0x12345678u);
}

TEST(AppendLE, Uint16)
{
    std::vector<uint8_t> out;
    appendLE<uint16_t>(0x1234, out);
    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out[0], 0x34);
    EXPECT_EQ(out[1], 0x12);
}

TEST(AppendLE, Uint32)
{
    std::vector<uint8_t> out;
    appendLE<uint32_t>(0xDEADBEEF, out);
    ASSERT_EQ(out.size(), 4u);
    EXPECT_EQ(out[0], 0xEF);
    EXPECT_EQ(out[1], 0xBE);
    EXPECT_EQ(out[2], 0xAD);
    EXPECT_EQ(out[3], 0xDE);
}

TEST(AppendLE, AppendsToExistingData)
{
    std::vector<uint8_t> out = { 0xAA, 0xBB };
    appendLE<uint16_t>(0x0102, out);
    ASSERT_EQ(out.size(), 4u);
    EXPECT_EQ(out[0], 0xAA);
    EXPECT_EQ(out[1], 0xBB);
    EXPECT_EQ(out[2], 0x02);
    EXPECT_EQ(out[3], 0x01);
}

TEST(Serialize, WritesAtOffset)
{
    std::vector<uint8_t> buf(8, 0x00);
    uint32_t val = 0xCAFEBABE;
    serialize(val, buf, 2);
    EXPECT_EQ(buf[2], 0xBE);
    EXPECT_EQ(buf[3], 0xBA);
    EXPECT_EQ(buf[4], 0xFE);
    EXPECT_EQ(buf[5], 0xCA);
}

TEST(Serialize, IgnoresIfBufferTooSmall)
{
    std::vector<uint8_t> buf(2, 0xFF);
    uint32_t val = 0x12345678;
    serialize(val, buf, 0);
    EXPECT_EQ(buf[0], 0xFF);
    EXPECT_EQ(buf[1], 0xFF);
}

TEST(SerializeInt2LE, Uint16)
{
    auto bytes = serializeInt2LittleEndian<uint16_t>(0xABCD);
    ASSERT_EQ(bytes.size(), 2u);
    EXPECT_EQ(bytes[0], 0xCD);
    EXPECT_EQ(bytes[1], 0xAB);
}

TEST(SerializeInt2LE, Uint32)
{
    auto bytes = serializeInt2LittleEndian<uint32_t>(0x01020304);
    ASSERT_EQ(bytes.size(), 4u);
    EXPECT_EQ(bytes[0], 0x04);
    EXPECT_EQ(bytes[1], 0x03);
    EXPECT_EQ(bytes[2], 0x02);
    EXPECT_EQ(bytes[3], 0x01);
}

TEST(GetBit, ReturnsCorrectBits)
{
    uint8_t val = 0b10100101;
    EXPECT_TRUE(getBit(val, 0));
    EXPECT_FALSE(getBit(val, 1));
    EXPECT_TRUE(getBit(val, 2));
    EXPECT_FALSE(getBit(val, 3));
    EXPECT_FALSE(getBit(val, 4));
    EXPECT_TRUE(getBit(val, 5));
    EXPECT_FALSE(getBit(val, 6));
    EXPECT_TRUE(getBit(val, 7));
}

TEST(SetBit, SetsSingleBit)
{
    uint32_t val = 0;
    setBit(val, 3, true);
    EXPECT_EQ(val, 0b1000u);
}

TEST(SetBit, ClearsSingleBit)
{
    uint32_t val = 0xFF;
    setBit(val, 0, false);
    EXPECT_EQ(val, 0xFEu);
}

TEST(SetBit, SetAndClearMultipleBits)
{
    uint32_t val = 0;
    setBit(val, 0, true);
    setBit(val, 7, true);
    setBit(val, 0, false);
    EXPECT_EQ(val, 0b10000000u);
}

TEST(CountEnum, CorrectSize)
{
    using JimmyPaputto::EFixType;
    constexpr auto count = countEnum<EFixType,
        EFixType::NoFix, EFixType::TimeOnlyFix>();
    EXPECT_EQ(count, 6);
}

TEST(ToUnderlying, ReturnsBaseValue)
{
    using JimmyPaputto::EDynamicModel;
    EXPECT_EQ(to_underlying(EDynamicModel::Portable), 0);
    EXPECT_EQ(to_underlying(EDynamicModel::Stationary), 2);
    EXPECT_EQ(to_underlying(EDynamicModel::Bike), 10);
}

TEST(IsLittleEndian, ArmIsLE)
{
    EXPECT_TRUE(isLittleEndian());
}

TEST(FloatingToLE, Float)
{
    float val = 1.0f;
    auto bytes = floatingToLittleEndian(val);
    ASSERT_EQ(bytes.size(), sizeof(float));

    float restored;
    std::memcpy(&restored, bytes.data(), sizeof(float));
    EXPECT_FLOAT_EQ(restored, 1.0f);
}

TEST(FloatingToLE, Double)
{
    double val = 3.14;
    auto bytes = floatingToLittleEndian(val);
    ASSERT_EQ(bytes.size(), sizeof(double));

    double restored;
    std::memcpy(&restored, bytes.data(), sizeof(double));
    EXPECT_DOUBLE_EQ(restored, 3.14);
}

TEST(VectorConcat, PlusOperator)
{
    std::vector<uint8_t> a = { 1, 2 };
    std::vector<uint8_t> b = { 3, 4 };
    auto c = a + b;
    EXPECT_EQ(c, (std::vector<uint8_t>{ 1, 2, 3, 4 }));
}

TEST(VectorConcat, PlusEqualsOperator)
{
    std::vector<uint8_t> a = { 1, 2 };
    std::vector<uint8_t> b = { 3, 4 };
    a += b;
    EXPECT_EQ(a, (std::vector<uint8_t>{ 1, 2, 3, 4 }));
}

TEST(ToHex, FormatsCorrectly)
{
    std::vector<uint8_t> data = { 0xDE, 0xAD, 0xBE, 0xEF };
    EXPECT_EQ(toHex(data), "DEADBEEF");
}

TEST(ToHex, EmptyVector)
{
    std::vector<uint8_t> data;
    EXPECT_EQ(toHex(data), "(empty)");
}

#include <gtest/gtest.h>

#include "ublox/UbxClassMsgId.hpp"
#include "ublox/EUbxMsg.hpp"


using namespace JimmyPaputto;

TEST(UbxClassMsgId, TranslateEnumToBytes)
{
    auto& map = UbxClassMsgId::instance();

    auto ackAck = map.translate(EUbxMsg::UBX_ACK_ACK);
    EXPECT_EQ(ackAck.first, 0x05);
    EXPECT_EQ(ackAck.second, 0x01);

    auto navPvt = map.translate(EUbxMsg::UBX_NAV_PVT);
    EXPECT_EQ(navPvt.first, 0x01);
    EXPECT_EQ(navPvt.second, 0x07);

    auto navDop = map.translate(EUbxMsg::UBX_NAV_DOP);
    EXPECT_EQ(navDop.first, 0x01);
    EXPECT_EQ(navDop.second, 0x04);
}

TEST(UbxClassMsgId, TranslateBytesToEnum)
{
    auto& map = UbxClassMsgId::instance();

    EXPECT_EQ(map.translate({ 0x05, 0x01 }), EUbxMsg::UBX_ACK_ACK);
    EXPECT_EQ(map.translate({ 0x05, 0x00 }), EUbxMsg::UBX_ACK_NAK);
    EXPECT_EQ(map.translate({ 0x01, 0x07 }), EUbxMsg::UBX_NAV_PVT);
    EXPECT_EQ(map.translate({ 0x01, 0x04 }), EUbxMsg::UBX_NAV_DOP);
    EXPECT_EQ(map.translate({ 0x01, 0x35 }), EUbxMsg::UBX_NAV_SAT);
    EXPECT_EQ(map.translate({ 0x0A, 0x38 }), EUbxMsg::UBX_MON_RF);
    EXPECT_EQ(map.translate({ 0x0A, 0x39 }), EUbxMsg::UBX_MON_SPAN);
    EXPECT_EQ(map.translate({ 0x06, 0x8A }), EUbxMsg::UBX_CFG_VALSET);
}

TEST(UbxClassMsgId, UnknownBytesReturnEndUbx)
{
    auto& map = UbxClassMsgId::instance();
    EXPECT_EQ(map.translate({ 0xFF, 0xFF }), EUbxMsg::END_UBX);
    EXPECT_EQ(map.translate({ 0x00, 0x00 }), EUbxMsg::END_UBX);
}

TEST(UbxClassMsgId, RoundTripAllMessages)
{
    auto& map = UbxClassMsgId::instance();

    for (uint8_t i = 0; i < static_cast<uint8_t>(EUbxMsg::END_UBX); i++)
    {
        auto eMsg = static_cast<EUbxMsg>(i);
        auto bytes = map.translate(eMsg);
        auto roundTrip = map.translate(bytes);
        EXPECT_EQ(roundTrip, eMsg);
    }
}

TEST(UbxClassMsgId, CfgGeofenceMapping)
{
    auto& map = UbxClassMsgId::instance();

    auto bytes = map.translate(EUbxMsg::UBX_CFG_GEOFENCE);
    EXPECT_EQ(bytes.first, 0x06);
    EXPECT_EQ(bytes.second, 0x69);
}

TEST(UbxClassMsgId, NavGeofenceMapping)
{
    auto& map = UbxClassMsgId::instance();

    auto bytes = map.translate(EUbxMsg::UBX_NAV_GEOFENCE);
    EXPECT_EQ(bytes.first, 0x01);
    EXPECT_EQ(bytes.second, 0x39);
}

TEST(UbxClassMsgId, CfgValgetMapping)
{
    auto& map = UbxClassMsgId::instance();

    auto bytes = map.translate(EUbxMsg::UBX_CFG_VALGET);
    EXPECT_EQ(bytes.first, 0x06);
    EXPECT_EQ(bytes.second, 0x8B);
}

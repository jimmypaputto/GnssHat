/*
 * Jimmy Paputto 2022
 */

#ifndef UBX_CLASS_MSG_ID_HPP_
#define UBX_CLASS_MSG_ID_HPP_

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>

#include "EUbxMsg.hpp"
#include "common/GenericSingleton.hpp"
#include "common/Utils.hpp"


namespace JimmyPaputto
{

class UbxClassMsgId: public GenericSingleton<UbxClassMsgId>
{
public:
    explicit UbxClassMsgId()
    {
        using enum EUbxMsg;
        ubxClassMsgIdMap_[to_underlying(UBX_ACK_ACK)] = { 0x05, 0x01 };
        ubxClassMsgIdMap_[to_underlying(UBX_ACK_NAK)] = { 0x05, 0x00 };
        ubxClassMsgIdMap_[to_underlying(UBX_CFG_CFG)] = { 0x06, 0x09 };
        ubxClassMsgIdMap_[to_underlying(UBX_CFG_GEOFENCE)] = { 0x06, 0x69 };
        ubxClassMsgIdMap_[to_underlying(UBX_CFG_MSG)] = { 0x06, 0x01 };
        ubxClassMsgIdMap_[to_underlying(UBX_CFG_NAV5)] = { 0x06, 0x24 };
        ubxClassMsgIdMap_[to_underlying(UBX_CFG_PRT)] = { 0x06, 0x00 };
        ubxClassMsgIdMap_[to_underlying(UBX_CFG_RATE)] = { 0x06, 0x08 };
        ubxClassMsgIdMap_[to_underlying(UBX_CFG_TP5)] = { 0x06, 0x31 };
        ubxClassMsgIdMap_[to_underlying(UBX_CFG_VALSET)] = { 0x06, 0x8A };
        ubxClassMsgIdMap_[to_underlying(UBX_CFG_VALGET)] = { 0x06, 0x8B };
        ubxClassMsgIdMap_[to_underlying(UBX_MON_RF)] = { 0x0A, 0x38 };
        ubxClassMsgIdMap_[to_underlying(UBX_NAV_DOP)] = { 0x01, 0x04 };
        ubxClassMsgIdMap_[to_underlying(UBX_NAV_GEOFENCE)] = { 0x01, 0x39 };
        ubxClassMsgIdMap_[to_underlying(UBX_NAV_PVT)] = { 0x01, 0x07 };
    }

    std::pair<uint8_t, uint8_t> translate(const EUbxMsg& eUbxMsg) const
    {
        return ubxClassMsgIdMap_[to_underlying(eUbxMsg)];
    }

    EUbxMsg translate(const std::pair<uint8_t, uint8_t>& rawUbxClassMsgid) const
    {
        const auto ubxClassMsgIdIt = std::find(
            ubxClassMsgIdMap_.begin(),
            ubxClassMsgIdMap_.end(),
            rawUbxClassMsgid
        );
        if (ubxClassMsgIdIt != ubxClassMsgIdMap_.end())
        {
            return static_cast<EUbxMsg>(std::distance(ubxClassMsgIdMap_.begin(),
                ubxClassMsgIdIt));
        }

        return EUbxMsg::END_UBX;
    }

private:
    std::array<std::pair<uint8_t, uint8_t>, numberOfUbxMsgs> ubxClassMsgIdMap_;
};

}  // JimmyPaputto

#endif  // UBX_CLASS_MSG_ID_HPP_

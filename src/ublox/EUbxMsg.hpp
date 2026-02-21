/*
 * Jimmy Paputto 2022
 */

#ifndef E_UBX_MSG_HPP_
#define E_UBX_MSG_HPP_

#include <cstdint>

#include "common/Utils.hpp"


namespace JimmyPaputto
{

enum class EUbxMsg: std::uint8_t
{
    UBX_ACK_ACK      = 0x00,
    UBX_ACK_NAK      = 0x01,
    UBX_CFG_CFG      = 0x02,
    UBX_CFG_GEOFENCE = 0x03,
    UBX_CFG_MSG      = 0x04,
    UBX_CFG_NAV5     = 0x05,
    UBX_CFG_PRT      = 0x06,
    UBX_CFG_RATE     = 0x07,
    UBX_CFG_TP5      = 0x08,
    UBX_CFG_VALSET   = 0x09,
    UBX_CFG_VALGET   = 0x0A,
    UBX_MON_RF       = 0x0B,
    UBX_NAV_DOP      = 0x0C,
    UBX_NAV_GEOFENCE = 0x0D,
    UBX_NAV_PVT      = 0x0E,
    END_UBX
};

constexpr uint8_t numberOfUbxMsgs = 
    countEnum<EUbxMsg, EUbxMsg::UBX_ACK_ACK, EUbxMsg::END_UBX>() - 1;

}  // JimmyPaputto

#endif  // E_UBX_MSG_HPP_

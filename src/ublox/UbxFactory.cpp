/*
 * Jimmy Paputto 2022
 */

#include "UbxFactory.hpp"

#include "ubxmsg/UBX_ACK_ACK.hpp"
#include "ubxmsg/UBX_ACK_NAK.hpp"
#include "ubxmsg/UBX_CFG_CFG.hpp"
#include "ubxmsg/UBX_CFG_GEOFENCE.hpp"
#include "ubxmsg/UBX_CFG_MSG.hpp"
#include "ubxmsg/UBX_CFG_NAV5.hpp"
#include "ubxmsg/UBX_CFG_PRT.hpp"
#include "ubxmsg/UBX_CFG_RATE.hpp"
#include "ubxmsg/UBX_CFG_TP5.hpp"
#include "ubxmsg/UBX_CFG_VALSET.hpp"
#include "ubxmsg/UBX_CFG_VALGET.hpp"
#include "ubxmsg/UBX_MON_RF.hpp"
#include "ubxmsg/UBX_NAV_DOP.hpp"
#include "ubxmsg/UBX_NAV_GEOFENCE.hpp"
#include "ubxmsg/UBX_NAV_PVT.hpp"


namespace JimmyPaputto
{

ubxmsg::IUbxMsg& UbxFactory::create(const EUbxMsg& eUbxMsg,
    std::span<const uint8_t> frame)
{
    static UbxFactory factory;
    auto* ubxMsg = factory.create_[to_underlying(eUbxMsg)];
    ubxMsg->deserialize(frame);
    return *ubxMsg;
}

UbxFactory::UbxFactory()
{
    using enum EUbxMsg;
    static ubxmsg::UBX_ACK_ACK ack_ack;
    create_[to_underlying(UBX_ACK_ACK)] = &ack_ack;
    static ubxmsg::UBX_ACK_NAK ack_nak;
    create_[to_underlying(UBX_ACK_NAK)] = &ack_nak;
    static ubxmsg::UBX_CFG_CFG cfg_cfg;
    create_[to_underlying(UBX_CFG_CFG)] = &cfg_cfg;
    static ubxmsg::UBX_CFG_GEOFENCE cfg_geofence;
    create_[to_underlying(UBX_CFG_GEOFENCE)] = &cfg_geofence;
    static ubxmsg::UBX_CFG_MSG cfg_msg;
    create_[to_underlying(UBX_CFG_MSG)] = &cfg_msg;
    static ubxmsg::UBX_CFG_NAV5 cfg_nav5;
    create_[to_underlying(UBX_CFG_NAV5)] = &cfg_nav5;
    static ubxmsg::UBX_CFG_PRT cfg_prt;
    create_[to_underlying(UBX_CFG_PRT)] = &cfg_prt;
    static ubxmsg::UBX_CFG_RATE cfg_rate;
    create_[to_underlying(UBX_CFG_RATE)] = &cfg_rate;
    static ubxmsg::UBX_CFG_TP5 cfg_tp5;
    create_[to_underlying(UBX_CFG_TP5)] = &cfg_tp5;
    static ubxmsg::UBX_CFG_VALSET cfg_valset;
    create_[to_underlying(UBX_CFG_VALSET)] = &cfg_valset;
    static ubxmsg::UBX_CFG_VALGET cfg_valget;
    create_[to_underlying(UBX_CFG_VALGET)] = &cfg_valget;
    static ubxmsg::UBX_MON_RF mon_rf;
    create_[to_underlying(UBX_MON_RF)] = &mon_rf;
    static ubxmsg::UBX_NAV_DOP nav_dop;
    create_[to_underlying(UBX_NAV_DOP)] = &nav_dop;
    static ubxmsg::UBX_NAV_GEOFENCE nav_geofence;
    create_[to_underlying(UBX_NAV_GEOFENCE)] = &nav_geofence;
    static ubxmsg::UBX_NAV_PVT nav_pvt;
    create_[to_underlying(UBX_NAV_PVT)] = &nav_pvt;
}

}  // JimmyPaputto

/*
 * Jimmy Paputto 2022
 */

#include "UbxCallbacks.hpp"

#include "ublox/Gnss.hpp"
#include "ublox/Ublox.hpp"
#include "common/Utils.hpp"
#include "GnssHat.hpp"

#include "ubxmsg/UBX_ACK_ACK.hpp"
#include "ubxmsg/UBX_ACK_NAK.hpp"
#include "ubxmsg/UBX_CFG_VALSET.hpp"
#include "ubxmsg/UBX_CFG_VALGET.hpp"
#include "ubxmsg/UBX_MON_RF.hpp"
#include "ubxmsg/UBX_NAV_DOP.hpp"
#include "ubxmsg/UBX_NAV_GEOFENCE.hpp"
#include "ubxmsg/UBX_NAV_PVT.hpp"
#include "ubxmsg/UBX_NAV_SAT.hpp"


namespace JimmyPaputto
{

UbxCallbacks::UbxCallbacks(IUbloxConfigRegistry& configRegistry,
    Notifier& navigationNotifier, bool callbackNotificationEnabled)
:	navigationNotifier_(navigationNotifier),
    callbackNotificationEnabled_(callbackNotificationEnabled)
{
    using enum EUbxMsg;
    callbacks_[to_underlying(UBX_ACK_ACK)] = std::bind(
        &UbxCallbacks::ackNakCb,
        this,
        std::placeholders::_1,
        EUbxMsg::UBX_ACK_ACK,
        std::ref(configRegistry)
    );

    callbacks_[to_underlying(UBX_ACK_NAK)] = std::bind(
        &UbxCallbacks::ackNakCb,
        this,
        std::placeholders::_1,
        EUbxMsg::UBX_ACK_NAK,
        std::ref(configRegistry)
    );

    callbacks_[to_underlying(UBX_CFG_CFG)] = [](ubxmsg::IUbxMsg&) -> void {
        // empty callback
    };

    callbacks_[to_underlying(UBX_CFG_GEOFENCE)] =
        [](ubxmsg::IUbxMsg&) -> void {
            // Deprecated: geofencing migrated to CFG-GEOFENCE-* via VALSET
        };

    callbacks_[to_underlying(UBX_CFG_MSG)] = [](ubxmsg::IUbxMsg&) -> void {
        // Deprecated: message output migrated to CFG-MSGOUT-* via VALSET
    };

    callbacks_[to_underlying(UBX_CFG_NAV5)] = [](ubxmsg::IUbxMsg&) -> void {
        // Deprecated: dynamic model migrated to CFG-NAVSPG-DYNMODEL via VALSET
    };

    callbacks_[to_underlying(UBX_CFG_PRT)] = [](ubxmsg::IUbxMsg&) -> void {
        // Deprecated: port configuration migrated to CFG-VALSET/VALGET
    };

    callbacks_[to_underlying(UBX_CFG_RATE)] = [](ubxmsg::IUbxMsg&) -> void {
        // Deprecated: rate configuration migrated to CFG-RATE-* via VALSET
    };

    callbacks_[to_underlying(UBX_CFG_TP5)] = [](ubxmsg::IUbxMsg&) -> void {
        // Deprecated: timepulse configuration migrated to CFG-TP-* via VALSET
    };

    callbacks_[to_underlying(UBX_CFG_VALSET)] = [](ubxmsg::IUbxMsg&) -> void {
        // empty callback
    };

    callbacks_[to_underlying(UBX_CFG_VALGET)] = [&configRegistry](ubxmsg::IUbxMsg& ubxMsg) -> void {
        const auto& ubxCfgValget = static_cast<ubxmsg::UBX_CFG_VALGET&>(ubxMsg);
        for (const auto& config : ubxCfgValget.configData())
            configRegistry.storeConfigValue(config.key, config.value);
    };

    callbacks_[to_underlying(UBX_MON_RF)] = [](ubxmsg::IUbxMsg& ubxMsg) -> void {
        const auto& ubxMonRf = static_cast<ubxmsg::UBX_MON_RF&>(ubxMsg);
        Gnss::instance().rfBlocks(ubxMonRf.rfBlocks());
    };

    callbacks_[to_underlying(UBX_NAV_DOP)] = [](ubxmsg::IUbxMsg& ubxMsg) -> void {
        const auto& ubxNavDop = static_cast<ubxmsg::UBX_NAV_DOP&>(ubxMsg);
        Gnss::instance().dop(ubxNavDop.dop());
    };

    callbacks_[to_underlying(UBX_NAV_GEOFENCE)] = [](ubxmsg::IUbxMsg& ubxMsg) -> void {
        const auto& ubxNavGeofence =
            static_cast<ubxmsg::UBX_NAV_GEOFENCE&>(ubxMsg);
        Gnss::instance().geofencingNav(ubxNavGeofence.nav());
    };

    callbacks_[to_underlying(UBX_NAV_PVT)] = [this](ubxmsg::IUbxMsg& ubxMsg) -> void {
        const auto& ubxNavPvt = static_cast<ubxmsg::UBX_NAV_PVT&>(ubxMsg);
        Gnss::instance().pvt(ubxNavPvt.pvt());
        if (callbackNotificationEnabled_)
            navigationNotifier_.notify();
    };

    callbacks_[to_underlying(UBX_NAV_SAT)] = [](ubxmsg::IUbxMsg& ubxMsg) -> void {
        const auto& ubxNavSat = static_cast<ubxmsg::UBX_NAV_SAT&>(ubxMsg);
        Gnss::instance().satellites(ubxNavSat.satellites());
    };
}

void UbxCallbacks::ackNakCb(ubxmsg::IUbxMsg& ubxMsg, EUbxMsg eUbxMsg,
    IUbloxConfigRegistry& configRegistry)
{
    if (eUbxMsg == EUbxMsg::UBX_ACK_ACK)
    {
        const auto& ackMsg = static_cast<ubxmsg::UBX_ACK_ACK&>(ubxMsg);
        const auto eUbxMsgFromNakOrAck =
            UbxClassMsgId::instance().translate(ackMsg.classMsgId());
        if (eUbxMsgFromNakOrAck != EUbxMsg::END_UBX)
        {
            configRegistry.ack(eUbxMsgFromNakOrAck);
        }
    }
    else if (eUbxMsg == EUbxMsg::UBX_ACK_NAK)
    {
        const auto& ackMsg = static_cast<ubxmsg::UBX_ACK_NAK&>(ubxMsg);
        const auto eUbxMsgFromNakOrAck =
            UbxClassMsgId::instance().translate(ackMsg.classMsgId());
        if (eUbxMsgFromNakOrAck != EUbxMsg::END_UBX)
        {
            configRegistry.nak(eUbxMsgFromNakOrAck);
        }
    }
}

void UbxCallbacks::run(ubxmsg::IUbxMsg& ubxMsg, const EUbxMsg& eUbxMsg)
{
    callbacks_[to_underlying(eUbxMsg)](ubxMsg);
}

}  // JimmyPaputto

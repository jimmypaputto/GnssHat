/*
 * Jimmy Paputto 2025
 */

#include "UbloxConfigRegistry.hpp"

#include "ublox/SpiDriver.hpp"
#include "ublox/UartDriver.hpp"
#include "ublox/ubxmsg/UBX_MON_RF.hpp"
#include "ublox/ubxmsg/UBX_NAV_DOP.hpp"
#include "ublox/ubxmsg/UBX_NAV_GEOFENCE.hpp"
#include "ublox/ubxmsg/UBX_NAV_PVT.hpp"
#include "ublox/ubxmsg/UBX_NAV_SAT.hpp"


namespace JimmyPaputto
{

UbloxConfigRegistry::UbloxConfigRegistry(const GnssConfig& config,
    const EUbxPrt portId)
:   config_(config),
    portId_(portId)
{
    std::array<uint8_t, numberOfUbxPrts> cfgMsg{};
    cfgMsg[static_cast<uint8_t>(portId_)] = 1;

    for (uint8_t i = 0; i < numberOfUbxMsgs; i++)
    {
        if (static_cast<EUbxMsg>(i) == EUbxMsg::UBX_MON_RF)
        {
            cfgMsgs_[i] = std::make_unique<ubxmsg::UBX_CFG_MSG>(EUbxMsg::UBX_MON_RF,
                cfgMsg);
        }
        else if (static_cast<EUbxMsg>(i) == EUbxMsg::UBX_NAV_DOP)
        {
            cfgMsgs_[i] = std::make_unique<ubxmsg::UBX_CFG_MSG>(EUbxMsg::UBX_NAV_DOP,
                cfgMsg);
        }
        else if (static_cast<EUbxMsg>(i) == EUbxMsg::UBX_NAV_GEOFENCE)
        {
            cfgMsgs_[i] = std::make_unique<ubxmsg::UBX_CFG_MSG>(
                EUbxMsg::UBX_NAV_GEOFENCE,
                cfgMsg
            );
        }
        else if (static_cast<EUbxMsg>(i) == EUbxMsg::UBX_NAV_PVT)
        {
            cfgMsgs_[i] = std::make_unique<ubxmsg::UBX_CFG_MSG>(EUbxMsg::UBX_NAV_PVT,
                cfgMsg);
        }
        else if (static_cast<EUbxMsg>(i) == EUbxMsg::UBX_NAV_SAT)
        {
            cfgMsgs_[i] = std::make_unique<ubxmsg::UBX_CFG_MSG>(EUbxMsg::UBX_NAV_SAT,
                cfgMsg);
        }
        else
        {
            cfgMsgs_[i] =
                std::make_unique<ubxmsg::UBX_CFG_MSG>(static_cast<EUbxMsg>(i));
        }
    }
}

void UbloxConfigRegistry::isPortConfigured(const bool isPortConfigured)
{
    isPortConfigured_ = isPortConfigured;
}

void UbloxConfigRegistry::isMsgSendrateCorrect(const bool isMsgSendrateCorrect)
{
    isMsgSendrateCorrect_ = isMsgSendrateCorrect;
}

void UbloxConfigRegistry::isRateCorrect(const bool isRateCorrect)
{
    isRateCorrect_ = isRateCorrect;
}

void UbloxConfigRegistry::isTimepulseConfigCorrect(
    const bool isTimepulseConfigCorrect)
{
    isTimepulseConfigCorrect_ = isTimepulseConfigCorrect;
}

void UbloxConfigRegistry::isDynamicModelCorrect(
    const bool isDynamicModelCorrect)
{
    isDynamicModelCorrect_ = isDynamicModelCorrect;
}

void UbloxConfigRegistry::shouldSaveConfigToFlash(const bool shouldSave)
{
    shouldSaveConfigToFlash_ = shouldSave;
}

EDynamicModel UbloxConfigRegistry::dynamicModel() const
{
    return config_.dynamicModel;
}

const ubxmsg::IUbxMsg& UbloxConfigRegistry::portConfig() const
{
    static const auto spiConfig = SpiDriver::portConfig();
    return  static_cast<const ubxmsg::IUbxMsg&>(spiConfig);
}

ubxmsg::UBX_CFG_RATE UbloxConfigRegistry::rateConfig() const
{
    const uint16_t measurementRate_ms = 1000U / config_.measurementRate_Hz;
    ubxmsg::UBX_CFG_RATE rate(ubxmsg::Rate { measurementRate_ms, 1, 0 });
    return rate;
}

ubxmsg::UBX_CFG_TP5 UbloxConfigRegistry::timepulseConfig() const
{
    return ubxmsg::UBX_CFG_TP5(config_.timepulsePinConfig);
}

void UbloxConfigRegistry::ack(const EUbxMsg& eUbxMsg)
{
    ack_[to_underlying(eUbxMsg)] = true;
}

void UbloxConfigRegistry::nak(const EUbxMsg& eUbxMsg)
{
    nak_[to_underlying(eUbxMsg)] = true;
}

std::array<uint8_t, numberOfUbxPrts> UbloxConfigRegistry::getMsgSendrates(
    const EUbxMsg& eUbxMsg) const
{
    return cfgMsgs_[to_underlying(eUbxMsg)]->sendRates();
}

void UbloxConfigRegistry::checkGeofencing(
    const ubxmsg::UBX_CFG_GEOFENCE& geofencingCfgFromSom)
{
    isGeofencingCorrect_ = geofencingCfgFromSom.serialize() ==
        geofencingConfig().serialize();
}

bool UbloxConfigRegistry::isGeofencingCorrect() const
{
    return isGeofencingCorrect_;
}

ubxmsg::UBX_CFG_GEOFENCE UbloxConfigRegistry::geofencingConfig() const
{
    constexpr uint8_t pioPinNumber = 6;
    EPioPinPolarity pioPinPolarity = EPioPinPolarity::LowMeansInside;
    bool pioEnabled = false;
    uint8_t confidenceLevel = 0;
    std::vector<Geofence> geofences;

    const auto& geoConfig = config_.geofencing;
    if (geoConfig.has_value())
    {
        if (geoConfig->pioPinPolarity.has_value())
        {
            pioPinPolarity = geoConfig->pioPinPolarity.value();
            pioEnabled = true;
        }
        confidenceLevel = geoConfig->confidenceLevel;
        geofences = geoConfig->geofences;
    }
    
    return ubxmsg::UBX_CFG_GEOFENCE(
        pioPinNumber,
        pioPinPolarity,
        pioEnabled,
        confidenceLevel,
        geofences
    );
}

void UbloxConfigRegistry::storeConfigValue(uint32_t key, const std::vector<uint8_t>& value)
{
    storedConfigValues_[key] = value;
}

std::vector<uint8_t> UbloxConfigRegistry::getStoredConfigValue(uint32_t key) const
{
    auto it = storedConfigValues_.find(key);
    if (it != storedConfigValues_.end())
        return it->second;

    return {};
}

bool UbloxConfigRegistry::hasStoredConfigValue(uint32_t key) const
{
    return storedConfigValues_.find(key) != storedConfigValues_.end();
}

void UbloxConfigRegistry::clearStoredConfigValues()
{
    storedConfigValues_.clear();
}

bool UbloxConfigRegistry::compareConfigValue(uint32_t key, const std::vector<uint8_t>& expectedValue) const
{
    auto actualValue = getStoredConfigValue(key);
    return !actualValue.empty() && actualValue == expectedValue;
}

bool UbloxConfigRegistry::compareConfigValue(uint32_t key, uint8_t expectedValue) const
{
    auto actualValue = getStoredConfigValue(key);
    return actualValue.size() == 1 && actualValue[0] == expectedValue;
}

bool UbloxConfigRegistry::compareConfigValue(uint32_t key, uint16_t expectedValue) const
{
    auto actualValue = getStoredConfigValue(key);
    if (actualValue.size() != 2)
        return false;

    uint16_t actualU16 = actualValue[0] | (actualValue[1] << 8);
    return actualU16 == expectedValue;
}

bool UbloxConfigRegistry::compareConfigValue(uint32_t key, uint32_t expectedValue) const
{
    auto actualValue = getStoredConfigValue(key);
    if (actualValue.size() != 4)
        return false;

    uint32_t actualU32 = actualValue[0] | (actualValue[1] << 8) | 
                        (actualValue[2] << 16) | (actualValue[3] << 24);
    return actualU32 == expectedValue;
}

}  // JimmyPaputto

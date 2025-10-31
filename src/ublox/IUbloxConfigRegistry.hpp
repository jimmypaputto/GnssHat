/*
 * Jimmy Paputto 2025
 */

#ifndef I_UBLOX_CONFIG_REGISTRY_HPP_
#define I_UBLOX_CONFIG_REGISTRY_HPP_

#include <array>
#include <vector>

#include "EUbxMsg.hpp"
#include "EUbxPrt.hpp"
#include "EDynamicModel.hpp"
#include "GnssConfig.hpp"
#include "ubxmsg/IUbxMsg.hpp"
#include "ubxmsg/UBX_CFG_GEOFENCE.hpp"
#include "ubxmsg/UBX_CFG_MSG.hpp"
#include "ubxmsg/UBX_CFG_PRT.hpp"
#include "ubxmsg/UBX_CFG_RATE.hpp"
#include "ubxmsg/UBX_CFG_TP5.hpp"
#include "ubxmsg/UBX_CFG_VALSET.hpp"  // For ConfigKeyValue struct


namespace JimmyPaputto
{

class IUbloxConfigRegistry
{
public:
    virtual ~IUbloxConfigRegistry() = default;

    virtual const GnssConfig& getGnssConfig() const = 0;

    virtual void isPortConfigured(const bool isPortConfigured) = 0;
    virtual void isMsgSendrateCorrect(const bool isMsgSendrateCorrect) = 0;
    virtual void isRateCorrect(const bool isRateCorrect) = 0;
    virtual void isTimepulseConfigCorrect(
        const bool isTimepulseConfigCorrect) = 0;
    virtual void isDynamicModelCorrect(const bool isDynamicModelCorrect) = 0;
    virtual void shouldSaveConfigToFlash(const bool shouldSave) = 0;

    virtual bool isPortConfigured() const = 0;
    virtual bool isMsgSendrateCorrect() const = 0;
    virtual bool isRateCorrect() const = 0;
    virtual bool isTimepulseConfigCorrect() const = 0;
    virtual bool isDynamicModelCorrect() const = 0;
    virtual bool shouldSaveConfigToFlash() const = 0;

    virtual EDynamicModel dynamicModel() const = 0;
    virtual const ubxmsg::IUbxMsg& portConfig() const = 0;
    virtual ubxmsg::UBX_CFG_RATE rateConfig() const = 0;
    virtual ubxmsg::UBX_CFG_TP5 timepulseConfig() const = 0;
    virtual void ack(const EUbxMsg& eUbxMsg) = 0;
    virtual void nak(const EUbxMsg& eUbxMsg) = 0;
    virtual ubxmsg::UBX_CFG_MSG& cfgMsg(const EUbxMsg& eUbxMsg) = 0;
    virtual std::array<uint8_t, numberOfUbxPrts> getMsgSendrates(
        const EUbxMsg& eUbxMsg) const = 0;

    virtual std::array<bool, numberOfUbxMsgs>& ack() = 0;
    virtual std::array<bool, numberOfUbxMsgs>& nak() = 0;

    virtual void checkGeofencing(
        const ubxmsg::UBX_CFG_GEOFENCE& geofencingCfgFromSom) = 0;
    virtual bool isGeofencingCorrect() const = 0;
    virtual ubxmsg::UBX_CFG_GEOFENCE geofencingConfig() const = 0;
    
    // VALGET Configuration Storage Methods
    virtual void storeConfigValue(uint32_t key, const std::vector<uint8_t>& value) = 0;
    virtual std::vector<uint8_t> getStoredConfigValue(uint32_t key) const = 0;
    virtual bool hasStoredConfigValue(uint32_t key) const = 0;
    virtual void clearStoredConfigValues() = 0;
    
    // Configuration Comparison Methods
    virtual bool compareConfigValue(uint32_t key, const std::vector<uint8_t>& expectedValue) const = 0;
    virtual bool compareConfigValue(uint32_t key, uint8_t expectedValue) const = 0;
    virtual bool compareConfigValue(uint32_t key, uint16_t expectedValue) const = 0;
    virtual bool compareConfigValue(uint32_t key, uint32_t expectedValue) const = 0;
};

}  // JimmyPaputto

#endif  // I_UBLOX_CONFIG_REGISTRY_HPP_

/*
 * Jimmy Paputto 2025
 */

#ifndef UBLOX_CONFIG_REGISTRY_HPP_
#define UBLOX_CONFIG_REGISTRY_HPP_

#include <array>
#include <memory>
#include <unordered_map>

#include "IUbloxConfigRegistry.hpp"
#include "EDynamicModel.hpp"
#include "EUbxMsg.hpp"
#include "GnssConfig.hpp"

#include "ubxmsg/IUbxMsg.hpp"
#include "ubxmsg/UBX_CFG_MSG.hpp"
#include "ubxmsg/UBX_CFG_PRT.hpp"
#include "ubxmsg/UBX_CFG_RATE.hpp"
#include "ubxmsg/UBX_CFG_TP5.hpp"


namespace JimmyPaputto
{

class UbloxConfigRegistry : public IUbloxConfigRegistry
{
public:
    explicit UbloxConfigRegistry(const GnssConfig& config,
        const EUbxPrt portId);
    ~UbloxConfigRegistry() override = default;

    const GnssConfig& getGnssConfig() const override { return config_; }

    void isPortConfigured(const bool isPortConfigured) override;
    void isMsgSendrateCorrect(const bool isMsgSendrateCorrect) override;
    void isRateCorrect(const bool isRateCorrect) override;
    void isTimepulseConfigCorrect(const bool isTimepulseConfigCorrect) override;
    void isDynamicModelCorrect(const bool isDynamicModelCorrect) override;
    void shouldSaveConfigToFlash(const bool shouldSave) override;

    bool isPortConfigured() const override { return isPortConfigured_; }
    bool isMsgSendrateCorrect() const override { return isMsgSendrateCorrect_; }
    bool isRateCorrect() const override { return isRateCorrect_; }
    bool isTimepulseConfigCorrect() const override { return isTimepulseConfigCorrect_; }
    bool isDynamicModelCorrect() const override { return isDynamicModelCorrect_; }
    bool shouldSaveConfigToFlash() const override { return shouldSaveConfigToFlash_; }

    EDynamicModel dynamicModel() const override;
    const ubxmsg::IUbxMsg& portConfig() const override;
    ubxmsg::UBX_CFG_RATE rateConfig() const override;
    ubxmsg::UBX_CFG_TP5 timepulseConfig() const override;
    void ack(const EUbxMsg& eUbxMsg) override;
    void nak(const EUbxMsg& eUbxMsg) override;
    ubxmsg::UBX_CFG_MSG& cfgMsg(const EUbxMsg& eUbxMsg) override
    {
        return *cfgMsgs_[eUbxMsg];
    }
    std::array<uint8_t, numberOfUbxPrts> getMsgSendrates(
        const EUbxMsg& eUbxMsg) const override;

    std::array<bool, numberOfUbxMsgs>& ack() override { return ack_; }
    std::array<bool, numberOfUbxMsgs>& nak() override { return nak_; }

    void checkGeofencing(
        const ubxmsg::UBX_CFG_GEOFENCE& geofencingCfgFromSom) override;
    bool isGeofencingCorrect() const override;
    ubxmsg::UBX_CFG_GEOFENCE geofencingConfig() const override;
    
    // VALGET Configuration Storage Methods
    void storeConfigValue(uint32_t key, const std::vector<uint8_t>& value) override;
    std::vector<uint8_t> getStoredConfigValue(uint32_t key) const override;
    bool hasStoredConfigValue(uint32_t key) const override;
    void clearStoredConfigValues() override;
    
    // Configuration Comparison Methods
    bool compareConfigValue(uint32_t key, const std::vector<uint8_t>& expectedValue) const override;
    bool compareConfigValue(uint32_t key, uint8_t expectedValue) const override;
    bool compareConfigValue(uint32_t key, uint16_t expectedValue) const override;
    bool compareConfigValue(uint32_t key, uint32_t expectedValue) const override;

private:
    bool isMsgSendrateCorrect_;
    bool isPortConfigured_;
    bool isRateCorrect_;
    bool isTimepulseConfigCorrect_;
    bool isDynamicModelCorrect_;
    bool isConfigured_;
    bool shouldSaveConfigToFlash_;

    bool isGeofencingCorrect_;

    std::array<bool, numberOfUbxMsgs> ack_;
    std::array<bool, numberOfUbxMsgs> nak_;

    std::array<std::unique_ptr<ubxmsg::UBX_CFG_MSG>, numberOfUbxMsgs> cfgMsgs_;

    GnssConfig config_;
    EUbxPrt portId_;
    
    // Storage for actual configuration values retrieved via VALGET
    std::unordered_map<uint32_t, std::vector<uint8_t>> storedConfigValues_;
};

}  // JimmyPaputto

#endif  // UBLOX_CONFIG_REGISTRY_HPP_

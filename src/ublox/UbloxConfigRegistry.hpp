/*
 * Jimmy Paputto 2025
 */

#ifndef UBLOX_CONFIG_REGISTRY_HPP_
#define UBLOX_CONFIG_REGISTRY_HPP_

#include <array>
#include <unordered_map>

#include "IUbloxConfigRegistry.hpp"
#include "EUbxMsg.hpp"
#include "GnssConfig.hpp"


namespace JimmyPaputto
{

class UbloxConfigRegistry : public IUbloxConfigRegistry
{
public:
    explicit UbloxConfigRegistry(const GnssConfig& config);
    ~UbloxConfigRegistry() override = default;

    const GnssConfig& getGnssConfig() const override { return config_; }

    void shouldSaveConfigToFlash(const bool shouldSave) override;
    bool shouldSaveConfigToFlash() const override { return shouldSaveConfigToFlash_; }

    void ack(const EUbxMsg& eUbxMsg) override;
    void nak(const EUbxMsg& eUbxMsg) override;
    std::array<bool, numberOfUbxMsgs>& ack() override { return ack_; }
    std::array<bool, numberOfUbxMsgs>& nak() override { return nak_; }

    void storeConfigValue(uint32_t key,
        const std::vector<uint8_t>& value) override;
    std::vector<uint8_t> getStoredConfigValue(uint32_t key) const override;
    void clearStoredConfigValues() override;

private:
    bool shouldSaveConfigToFlash_;

    std::array<bool, numberOfUbxMsgs> ack_;
    std::array<bool, numberOfUbxMsgs> nak_;

    GnssConfig config_;

    // Storage for actual configuration values retrieved via VALGET
    std::unordered_map<uint32_t, std::vector<uint8_t>> storedConfigValues_;
};

}  // JimmyPaputto

#endif  // UBLOX_CONFIG_REGISTRY_HPP_

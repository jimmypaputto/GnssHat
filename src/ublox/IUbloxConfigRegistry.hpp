/*
 * Jimmy Paputto 2025
 */

#ifndef I_UBLOX_CONFIG_REGISTRY_HPP_
#define I_UBLOX_CONFIG_REGISTRY_HPP_

#include <array>
#include <vector>

#include "EUbxMsg.hpp"
#include "GnssConfig.hpp"
#include "ubxmsg/UBX_CFG_VALSET.hpp"


namespace JimmyPaputto
{

class IUbloxConfigRegistry
{
public:
    virtual ~IUbloxConfigRegistry() = default;

    virtual const GnssConfig& getGnssConfig() const = 0;

    virtual void shouldSaveConfigToFlash(const bool shouldSave) = 0;
    virtual bool shouldSaveConfigToFlash() const = 0;

    virtual void ack(const EUbxMsg& eUbxMsg) = 0;
    virtual void nak(const EUbxMsg& eUbxMsg) = 0;
    virtual std::array<bool, numberOfUbxMsgs>& ack() = 0;
    virtual std::array<bool, numberOfUbxMsgs>& nak() = 0;

    // VALGET Configuration Storage
    virtual void storeConfigValue(uint32_t key,
        const std::vector<uint8_t>& value) = 0;
    virtual std::vector<uint8_t> getStoredConfigValue(uint32_t key) const = 0;
    virtual bool hasStoredConfigValue(uint32_t key) const = 0;
    virtual void clearStoredConfigValues() = 0;

    // Configuration Comparison
    virtual bool compareConfigValue(uint32_t key,
        const std::vector<uint8_t>& expectedValue) const = 0;
    virtual bool compareConfigValue(uint32_t key,
        uint8_t expectedValue) const = 0;
    virtual bool compareConfigValue(uint32_t key,
        uint16_t expectedValue) const = 0;
    virtual bool compareConfigValue(uint32_t key,
        uint32_t expectedValue) const = 0;
};

}  // JimmyPaputto

#endif  // I_UBLOX_CONFIG_REGISTRY_HPP_

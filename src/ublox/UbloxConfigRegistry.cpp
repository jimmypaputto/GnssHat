/*
 * Jimmy Paputto 2025
 */

#include "UbloxConfigRegistry.hpp"


namespace JimmyPaputto
{

UbloxConfigRegistry::UbloxConfigRegistry(const GnssConfig& config)
:   shouldSaveConfigToFlash_(false),
    ack_{},
    nak_{},
    config_(config)
{
}

void UbloxConfigRegistry::shouldSaveConfigToFlash(const bool shouldSave)
{
    shouldSaveConfigToFlash_ = shouldSave;
}

void UbloxConfigRegistry::ack(const EUbxMsg& eUbxMsg)
{
    ack_[to_underlying(eUbxMsg)] = true;
}

void UbloxConfigRegistry::nak(const EUbxMsg& eUbxMsg)
{
    nak_[to_underlying(eUbxMsg)] = true;
}

void UbloxConfigRegistry::storeConfigValue(uint32_t key,
    const std::vector<uint8_t>& value)
{
    storedConfigValues_[key] = value;
}

std::vector<uint8_t> UbloxConfigRegistry::getStoredConfigValue(
    uint32_t key) const
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

bool UbloxConfigRegistry::compareConfigValue(uint32_t key,
    const std::vector<uint8_t>& expectedValue) const
{
    auto actualValue = getStoredConfigValue(key);
    return !actualValue.empty() && actualValue == expectedValue;
}

bool UbloxConfigRegistry::compareConfigValue(uint32_t key,
    uint8_t expectedValue) const
{
    auto actualValue = getStoredConfigValue(key);
    return actualValue.size() == 1 && actualValue[0] == expectedValue;
}

bool UbloxConfigRegistry::compareConfigValue(uint32_t key,
    uint16_t expectedValue) const
{
    auto actualValue = getStoredConfigValue(key);
    if (actualValue.size() != 2)
        return false;

    uint16_t actualU16 = actualValue[0] | (actualValue[1] << 8);
    return actualU16 == expectedValue;
}

bool UbloxConfigRegistry::compareConfigValue(uint32_t key,
    uint32_t expectedValue) const
{
    auto actualValue = getStoredConfigValue(key);
    if (actualValue.size() != 4)
        return false;

    uint32_t actualU32 = actualValue[0] | (actualValue[1] << 8) | 
                        (actualValue[2] << 16) | (actualValue[3] << 24);
    return actualU32 == expectedValue;
}

}  // JimmyPaputto

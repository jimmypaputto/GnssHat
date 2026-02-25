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

void UbloxConfigRegistry::clearStoredConfigValues()
{
    storedConfigValues_.clear();
}

}  // JimmyPaputto

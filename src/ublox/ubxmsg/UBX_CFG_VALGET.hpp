/*
 * Jimmy Paputto 2025
 */

#ifndef UBX_CFG_VALGET_HPP_
#define UBX_CFG_VALGET_HPP_

#include <vector>
#include <cstdint>
#include <iomanip>

#include <sstream>
#include <stdexcept>

#include <unordered_map>

#include "IUbxMsg.hpp"
#include "UBX_CFG_VALSET.hpp"  // For ConfigKeyValue struct
#include "ublox/EUbxMemoryLayer.hpp"
#include "common/Utils.hpp"


namespace JimmyPaputto::ubxmsg
{

class ConfigKeySizeMap 
{
public:
    static uint8_t getKeySize(uint32_t key) 
    {
        auto it = keySizes_.find(key);
        if (it != keySizes_.end())
            return it->second;

        const std::string hexStr =
            (std::stringstream{} << "0x" << std::hex << std::uppercase << key).str();
        throw std::runtime_error(
            "Unknown configuration key size for key: " +  hexStr
        );
    }

private:
    static std::unordered_map<uint32_t, uint8_t> keySizes_;
};

/**
 * UBX-CFG-VALGET message - Get configuration values
 * Used to retrieve configuration values from the receiver's configuration database
 * 
 * Two distinct use cases:
 * 1. REQUEST: Use poll() static methods to create query messages (keys only)
 * 2. RESPONSE: Use deserialize() to parse incoming responses (key+value pairs)
 */
class UBX_CFG_VALGET : public IUbxMsg
{
public:
    explicit UBX_CFG_VALGET() = default;

    /**
     * Constructor for polling specific configuration keys (request)
     * @param version Protocol version (0x00 for current)
     * @param layer Configuration layer
     * @param position Position in the message (0x0000 for first/only)
     * @param keys Vector of configuration key IDs to retrieve
     */
    explicit UBX_CFG_VALGET(uint8_t version, EUbxMemoryLayer layer, uint16_t position, 
                            std::span<const uint32_t> keys)
    :   version_(version),
        layer_(layer),
        position_(position),
        keys_(keys.begin(), keys.end())
    {}

    /**
     * Constructor for deserializing received message (response only)
     * Only used by UBX parser when receiving VALGET responses from device
     */
    explicit UBX_CFG_VALGET(std::span<const uint8_t> frame)
    {
        deserialize(frame);
    }



    /**
     * Create a VALGET poll request for a single configuration key
     * @param key Configuration key ID to retrieve
     * @param layer Configuration layer (default: RAM)
     * @return Serialized UBX frame ready to send
     */
    static std::vector<uint8_t> poll(uint32_t key)
    {
        const std::array<uint32_t, 1> keys = { key };
        return UBX_CFG_VALGET(0x00, EUbxMemoryLayer::None, 0x0000, keys).serialize();
    }

    /**
     * Create a VALGET poll request for multiple configuration keys
     * @param keys Vector of configuration key IDs to retrieve
     * @param layer Configuration layer (default: RAM)
     * @return Serialized UBX frame ready to send
     */
    static std::vector<uint8_t> poll(std::span<const uint32_t> keys)
    {
        return UBX_CFG_VALGET(0x00, EUbxMemoryLayer::None, 0x0000, keys).serialize();
    }

    std::vector<uint8_t> serialize() const override
    {
        // Build payload first
        std::vector<uint8_t> payload;
        payload.push_back(version_);
        payload.push_back(static_cast<uint8_t>(layer_));
        
        // Add position using serializeInt (little-endian)
        const auto positionBytes = serializeInt2LittleEndian(position_);
        payload.insert(payload.end(), positionBytes.begin(), positionBytes.end());
        
        // Add configuration keys using serializeInt (little-endian)
        for (const auto& key : keys_) {
            const auto keyBytes = serializeInt2LittleEndian(key);
            payload.insert(payload.end(), keyBytes.begin(), keyBytes.end());
        }

        // Build frame: UBX header + length + payload
        std::vector<uint8_t> frame = { 0xB5, 0x62, 0x06, 0x8B };
        const auto lengthBytes = serializeInt2LittleEndian(static_cast<uint16_t>(payload.size()));
        frame.insert(frame.end(), lengthBytes.begin(), lengthBytes.end());
        frame.insert(frame.end(), payload.begin(), payload.end());

        return buildFrame(frame);
    }

    void deserialize(std::span<const uint8_t> frame) override
    {
        if (frame.size() < 10) {
            throw std::runtime_error("Invalid frame size for UBX_CFG_VALGET");
        }

        // Parse from offset 6 (after UBX header and length)
        version_ = frame[6];
        layer_ = static_cast<EUbxMemoryLayer>(frame[7]);
        position_ = static_cast<uint16_t>(frame[8] | (frame[9] << 8));

        keys_.clear();
        configData_.clear();
        
        // Deserialize is always called for responses containing key+value pairs
        // Requests are sent via poll() method and don't use deserialize()
        parseAsResponse(frame, 10);
    }

private:
    /**
     * Parse frame as response containing key+value pairs
     * Only called for incoming responses, never for outgoing requests
     */
    void parseAsResponse(std::span<const uint8_t> frame, size_t startOffset) 
    {
        size_t offset = startOffset;
        
        while (offset + 4 <= frame.size()) {
            // Read configuration key (4 bytes, little-endian)
            uint32_t key = static_cast<uint32_t>(
                frame[offset] | 
                (frame[offset + 1] << 8) | 
                (frame[offset + 2] << 16) | 
                (frame[offset + 3] << 24)
            );
            offset += 4;
            
            // Get expected value size for this key
            uint8_t valueSize;
            valueSize = ConfigKeySizeMap::getKeySize(key);

            
            // Check if we have enough bytes for the value
            if (offset + valueSize > frame.size()) {
                throw std::runtime_error("Insufficient data for config key value");
            }
            
            // Read value bytes
            std::vector<uint8_t> value(frame.begin() + offset, 
                                     frame.begin() + offset + valueSize);
            offset += valueSize;
            
            // Store key+value pair
            configData_.push_back({key, value});
            keys_.push_back(key);  // Also add to keys for compatibility
        }
        
        if (configData_.empty()) {
            throw std::runtime_error("No valid key+value pairs found in response");
        }
    }

public:
    // Getters
    inline uint8_t version() const { return version_; }
    inline EUbxMemoryLayer layer() const { return layer_; }
    inline uint16_t position() const { return position_; }
    inline const std::vector<uint32_t>& keys() const { return keys_; }
    inline const std::vector<ConfigKeyValue>& configData() const { return configData_; }

    /**
     * Get configuration value for a specific key (for responses only)
     * @param key Configuration key ID to look for
     * @return Configuration value as bytes, or empty vector if not found
     */
    std::vector<uint8_t> getConfigValue(uint32_t key) const 
    {
        for (const auto& config : configData_) {
            if (config.key == key) {
                return config.value;
            }
        }
        return {};  // Empty if not found
    }

    /**
     * Get configuration value as specific type (for responses only)
     * @param key Configuration key ID
     * @return Typed value
     */
    template<typename T>
    T getConfigValueAs(uint32_t key) const 
    {
        auto value = getConfigValue(key);
        if (value.empty() || value.size() != sizeof(T)) {
            throw std::runtime_error("Invalid value size for config key");
        }
        
        T result = 0;
        for (size_t i = 0; i < sizeof(T); ++i) {
            result |= (static_cast<T>(value[i]) << (i * 8));
        }
        return result;
    }

private:
    uint8_t version_{0x00};                         // Message version
    EUbxMemoryLayer layer_{EUbxMemoryLayer::None};   // Configuration layer
    uint16_t position_{0x0000};                     // Position in the configuration database
    std::vector<uint32_t> keys_;                    // Configuration key IDs
    std::vector<ConfigKeyValue> configData_;        // Configuration key+value pairs (for responses)
};

}  // JimmyPaputto::ubxmsg

#endif  // UBX_CFG_VALGET_HPP_

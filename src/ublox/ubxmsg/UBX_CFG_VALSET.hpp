/*
 * Jimmy Paputto 2025
 */

#ifndef UBX_CFG_VALSET_HPP_
#define UBX_CFG_VALSET_HPP_

#include <vector>
#include <cstdint>
#include <stdexcept>

#include "IUbxMsg.hpp"
#include "ublox/EUbxMemoryLayer.hpp"
#include "common/Utils.hpp"


namespace JimmyPaputto::ubxmsg
{

/**
 * Simple configuration key-value pair for basic types
 */
struct ConfigKeyValue
{
    uint32_t key;
    std::vector<uint8_t> value;  // Raw bytes of the value
};

/**
 * UBX-CFG-VALSET message - Set configuration values
 * Used to set configuration values in the receiver's configuration database
 */
class UBX_CFG_VALSET : public IUbxMsg
{
public:
    explicit UBX_CFG_VALSET() = default;

    /**
     * Constructor for setting configuration values
     * @param version Protocol version (0x00 for current)
     * @param layer Configuration layer
     * @param cfgData Vector of configuration key-value pairs
     */
    explicit UBX_CFG_VALSET(uint8_t version, EUbxMemoryLayer layer, 
                            const std::vector<ConfigKeyValue>& cfgData)
    :   version_(version),
        layer_(layer),
        cfgData_(cfgData)
    {}

    /**
     * Constructor for deserializing received message
     */
    explicit UBX_CFG_VALSET(const std::vector<uint8_t>& frame)
    {
        deserialize(frame);
    }

    /**
     * Create a VALSET message for setting a single uint8_t value
     */
    static std::vector<uint8_t> setU1(uint32_t key, uint8_t value, EUbxMemoryLayer layer = EUbxMemoryLayer::RAM)
    {
        ConfigKeyValue cfg = {key, {value}};
        return UBX_CFG_VALSET(0x00, layer, {cfg}).serialize();
    }

    /**
     * Create a VALSET message for setting a single uint16_t value
     */
    static std::vector<uint8_t> setU2(uint32_t key, uint16_t value, EUbxMemoryLayer layer = EUbxMemoryLayer::RAM)
    {
        ConfigKeyValue cfg = {key, serializeInt2LittleEndian(value)};
        return UBX_CFG_VALSET(0x00, layer, {cfg}).serialize();
    }

    /**
     * Create a VALSET message for setting a single uint32_t value
     */
    static std::vector<uint8_t> setU4(uint32_t key, uint32_t value, EUbxMemoryLayer layer = EUbxMemoryLayer::RAM)
    {
        ConfigKeyValue cfg = {key, serializeInt2LittleEndian(value)};
        return UBX_CFG_VALSET(0x00, layer, {cfg}).serialize();
    }

    std::vector<uint8_t> serialize() const override
    {
        // Build payload first
        std::vector<uint8_t> payload;
        payload.push_back(version_);
        payload.push_back(static_cast<uint8_t>(layer_));
        payload.push_back(0x00); // Reserved byte 1
        payload.push_back(0x00); // Reserved byte 2
        
        // Add configuration key-value pairs
        for (const auto& cfg : cfgData_) {
            // Add key using serializeInt2LittleEndian (little-endian)
            const auto keyBytes = serializeInt2LittleEndian(cfg.key);
            payload.insert(payload.end(), keyBytes.begin(), keyBytes.end());
            
            // Add value bytes
            payload.insert(payload.end(), cfg.value.begin(), cfg.value.end());
        }
        
        // Build frame: UBX header + length + payload
        std::vector<uint8_t> frame = { 0xB5, 0x62, 0x06, 0x8A };
        const auto lengthBytes = serializeInt2LittleEndian(static_cast<uint16_t>(payload.size()));
        frame.insert(frame.end(), lengthBytes.begin(), lengthBytes.end());
        frame.insert(frame.end(), payload.begin(), payload.end());

        return buildFrame(frame);
    }

    void deserialize(const std::vector<uint8_t>& frame) override
    {
        if (frame.size() < 10) {
            throw std::runtime_error("Invalid frame size for UBX_CFG_VALSET");
        }

        // Parse from offset 6 (after UBX header and length)
        version_ = frame[6];
        layer_ = static_cast<EUbxMemoryLayer>(frame[7]);
        // frame[8] and frame[9] are reserved
        
        cfgData_.clear();
        
        // Parse configuration data starting from offset 10
        size_t offset = 10;
        while (offset + 4 < frame.size()) {
            // Read key (4 bytes, little-endian)
            uint32_t key = static_cast<uint32_t>(
                frame[offset] | 
                (frame[offset + 1] << 8) | 
                (frame[offset + 2] << 16) | 
                (frame[offset + 3] << 24)
            );
            offset += 4;
            
            // For simplification, assume 4-byte values (most common)
            // In real implementation, value size would be determined by key type
            std::vector<uint8_t> value;
            size_t remainingBytes = frame.size() - offset;
            if (remainingBytes >= 4) {
                // Assume 4-byte value for now
                value = {frame[offset], frame[offset+1], frame[offset+2], frame[offset+3]};
                offset += 4;
            } else if (remainingBytes > 0) {
                // Take remaining bytes
                value.assign(frame.begin() + offset, frame.end());
                offset = frame.size();
            }
            
            if (!value.empty()) {
                cfgData_.push_back({key, value});
            }
        }
    }

    // Getters
    inline uint8_t version() const { return version_; }
    inline EUbxMemoryLayer layer() const { return layer_; }
    inline const std::vector<ConfigKeyValue>& cfgData() const { return cfgData_; }

private:
    uint8_t version_{0x00};                         // Message version
    EUbxMemoryLayer layer_{EUbxMemoryLayer::RAM};   // Configuration layer
    std::vector<ConfigKeyValue> cfgData_;           // Configuration data
};

}  // JimmyPaputto::ubxmsg

#endif  // UBX_CFG_VALSET_HPP_

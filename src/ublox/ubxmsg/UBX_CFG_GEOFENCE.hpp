/*
 * Jimmy Paputto 2022
 */

#ifndef UBX_CFG_GEOFENCE_HPP_
#define UBX_CFG_GEOFENCE_HPP_

#include "ublox/ubxmsg/IUbxMsg.hpp"
#include "ublox/Geofence.hpp"
#include "ublox/Geofencing.hpp"
#include "common/Utils.hpp"


namespace JimmyPaputto::ubxmsg
{

class UBX_CFG_GEOFENCE: public IUbxMsg
{
public:
    explicit UBX_CFG_GEOFENCE() = default;

    explicit UBX_CFG_GEOFENCE(
        const uint8_t pioPinNumber,
        const EPioPinPolarity pinPolarity,
        const bool pioEnabled,
        const uint8_t confidenceLevel,
        const std::vector<Geofence>& geofences)
    :   cfg_ {
            .pioPinNumber = pioPinNumber,
            .pinPolarity = pinPolarity,
            .pioEnabled = pioEnabled,
            .confidenceLevel = confidenceLevel,
            .geofences = geofences
        }
    {}

    explicit UBX_CFG_GEOFENCE(std::span<const uint8_t> frame)
    {
        cfg_.geofences.reserve(4);
        deserialize(frame);
    }

    std::vector<uint8_t> serialize() const override
    {
        const std::vector<uint8_t> begining = { 0xB5, 0x62, 0x06, 0x69 };
        uint16_t length = 8 + cfg_.geofences.size() * 12;
        uint8_t lenLE[2];
        std::memcpy(lenLE, &length, sizeof(length));

        const auto serialized =
            begining +
            std::vector<uint8_t> { lenLE[0], lenLE[1] } +
            std::vector<uint8_t> { 0x00, static_cast<uint8_t>(cfg_.geofences.size()),
                cfg_.confidenceLevel, 0x00, static_cast<uint8_t>(cfg_.pioEnabled),
                static_cast<uint8_t>(cfg_.pinPolarity), cfg_.pioPinNumber, 0x00 };

        std::vector<uint8_t> serializedFences(cfg_.geofences.size() * 12);
        for (uint8_t i = 0; i < cfg_.geofences.size(); i++)
        {
            uint32_t lat = cfg_.geofences[i].lat * 10000000;
            uint32_t lon = cfg_.geofences[i].lon * 10000000;
            uint32_t radius = cfg_.geofences[i].radius * 100;

            std::memcpy(serializedFences.data() + i * 12, &lat, sizeof(lat));
            std::memcpy(serializedFences.data() + 4 + i * 12, &lon, sizeof(lon));
            std::memcpy(serializedFences.data() + 8 + i * 12, &radius, sizeof(radius));
        }

        return buildFrame(serialized + serializedFences);
    }

    void deserialize(std::span<const uint8_t> serialized) override
    {
        const uint8_t numberOfGeofences = serialized[7];
        cfg_.confidenceLevel = serialized[8];
        cfg_.pioEnabled = serialized[10];
        cfg_.pinPolarity = static_cast<EPioPinPolarity>(serialized[11]);
        cfg_.pioPinNumber = serialized[12];
        cfg_.geofences.clear();
        for (uint8_t i = 0; i < numberOfGeofences; i++)
        {
            const auto geofence = Geofence {
                .lat = static_cast<float>(
                    readLE<uint32_t>(serialized, 14 + i*12) / 10000000.0),
                .lon = static_cast<float>(
                    readLE<uint32_t>(serialized, 18 + i*12) / 10000000.0),
                .radius = static_cast<float>(
                    readLE<uint32_t>(serialized, 22 + i*12) / 100.0)
            };
            cfg_.geofences.push_back(geofence);
        }
    }

    inline Geofencing::Cfg cfg() const
    {
        return cfg_;
    }

    static std::vector<uint8_t> poll()
    {
        return buildFrame({ 0xB5, 0x62, 0x06, 0x69, 0x00, 0x00 });
    }

private:
    Geofencing::Cfg cfg_;
};

}  // JimmyPaputto::ubxmsg

#endif  // UBX_CFG_GEOFENCE_HPP_

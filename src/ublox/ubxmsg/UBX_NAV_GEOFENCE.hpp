/*
 * Jimmy Paputto 2022
 */

#ifndef UBX_NAV_GEOFENCE_HPP_
#define UBX_NAV_GEOFENCE_HPP_

#include "ublox/Geofencing.hpp"

#include "IUbxMsg.hpp"


namespace JimmyPaputto::ubxmsg
{

class UBX_NAV_GEOFENCE: public IUbxMsg
{
public:
    explicit UBX_NAV_GEOFENCE() = default;

    explicit UBX_NAV_GEOFENCE(const std::vector<uint8_t>& frame)
    {
        deserialize(frame);
    }

    std::vector<uint8_t> serialize() const override
    {
        return {};
    }

    void deserialize(const std::vector<uint8_t>& serialized) override
    {
        nav_.iTOW = readLE<uint32_t>(serialized, 6);
        nav_.geofencingStatus = static_cast<EGeofencingStatus>(serialized[11]);
        if (nav_.geofencingStatus == EGeofencingStatus::Active)
        {
            nav_.numberOfGeofences = serialized[12];
            nav_.combinedState = static_cast<EGeofenceStatus>(serialized[13]);
            for (uint8_t i = 0; i < nav_.numberOfGeofences; i++)
            {
                nav_.geofencesStatus[i] =
                    static_cast<EGeofenceStatus>(serialized[14 + 2*i]);
            }
        }
    }

    inline Geofencing::Nav nav() const
    {
        return nav_;
    }

    static std::vector<uint8_t> poll()
    {
        return buildFrame({ 0xB5, 0x62, 0x01, 0x39, 0x00, 0x00 });
    }

private:
    Geofencing::Nav nav_;
};

}  // JimmyPaputto::ubxmsg

#endif  // UBX_NAV_GEOFENCE_HPP_

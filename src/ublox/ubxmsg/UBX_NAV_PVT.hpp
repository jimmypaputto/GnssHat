/*
 * Jimmy Paputto 2022
 */

#ifndef UBX_NAV_PVT_HPP_
#define UBX_NAV_PVT_HPP_

#include "IUbxMsg.hpp"

#include "ublox/PositionVelocityTime.hpp"
#include "common/Utils.hpp"


namespace JimmyPaputto::ubxmsg
{

class UBX_NAV_PVT: public IUbxMsg
{
public:
    explicit UBX_NAV_PVT() = default;

    explicit UBX_NAV_PVT(const std::vector<uint8_t>& frame)
    {
        deserialize(frame);
    }

    std::vector<uint8_t> serialize() const override
    {
        return {};
    }

    void deserialize(const std::vector<uint8_t>& serialized) override
    {
        pvt_.date.day = serialized[13];
        pvt_.date.month = serialized[12];
        pvt_.date.year = readLE<uint16_t>(serialized, 10);
        pvt_.date.valid = getBit(serialized[17], 1);
        
        pvt_.utc.hh = serialized[14];
        pvt_.utc.mm = serialized[15];
        pvt_.utc.ss = serialized[16];
        pvt_.utc.valid = getBit(serialized[17], 0);
        pvt_.utc.accuracy = readLE<int32_t>(serialized, 18);

        const auto fixType = EFixType(serialized[26]);
        pvt_.fixType = fixType;
        pvt_.fixStatus = EFixStatus(serialized[27] & (1 << 0));

        auto fixQuality = EFixQuality::Invalid;
        if (fixType == EFixType::Fix2D || fixType == EFixType::Fix3D)
        {
            fixQuality = EFixQuality::GpsFix2D3D;
        }
        else if (fixType == EFixType::DeadReckoningOnly)
        {
            fixQuality = EFixQuality::DeadReckoning;
        }
        if (getBit(serialized[27], 1))
        {
            fixQuality = EFixQuality::DGNSS;
        }
        if (getBit(serialized[27], 6))
        {
            fixQuality = EFixQuality::FloatRtk;
        }
        else if (getBit(serialized[27], 7))
        {
            fixQuality = EFixQuality::FixedRTK;
        }
        pvt_.fixQuality = fixQuality;

        pvt_.visibleSatellites = serialized[29];
        pvt_.longitude = readLE<uint32_t>(serialized, 30) / 10000000.0;
        pvt_.latitude = readLE<uint32_t>(serialized, 34) / 10000000.0;
        pvt_.altitude = readLE<uint32_t>(serialized, 38) / 1000.0;
        pvt_.altitudeMSL = readLE<uint32_t>(serialized, 42) / 1000.0;
        pvt_.horizontalAccuracy = readLE<uint32_t>(serialized, 46) / 1000.0;
        pvt_.verticalAccuracy = readLE<uint32_t>(serialized, 50) / 1000.0;
        pvt_.speedOverGround = readLE<uint32_t>(serialized, 66) / 1000.0;
        pvt_.heading = readLE<uint32_t>(serialized, 70) / 100000.0;
        pvt_.speedAccuracy = readLE<uint32_t>(serialized, 74) / 1000.0;
        pvt_.headingAccuracy = readLE<uint32_t>(serialized, 78) / 100000.0;
    }

    PositionVelocityTime pvt() const
    {
        return pvt_;
    }

private:
    PositionVelocityTime pvt_;
};

}  // JimmyPaputto::ubxmsg

#endif  // UBX_NAV_PVT_HPP_

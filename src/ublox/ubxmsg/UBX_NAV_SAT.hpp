/*
 * Jimmy Paputto 2026
 */

#ifndef UBX_NAV_SAT_HPP_
#define UBX_NAV_SAT_HPP_

#include "IUbxMsg.hpp"

#include "ublox/SatelliteInfo.hpp"
#include "common/Utils.hpp"


namespace JimmyPaputto::ubxmsg
{

class UBX_NAV_SAT: public IUbxMsg
{
public:
    explicit UBX_NAV_SAT() = default;

    explicit UBX_NAV_SAT(std::span<const uint8_t> frame)
    {
        satellites_.reserve(SatelliteInfo::maxNumberOfSatellites);
        deserialize(frame);
    }

    std::vector<uint8_t> serialize() const override
    {
        return {};
    }

    void deserialize(std::span<const uint8_t> serialized) override
    {
        const uint8_t numSvs = serialized[11];

        satellites_.clear();

        constexpr uint8_t headerSize = 14;
        constexpr uint8_t blockSize = 12;

        for (uint8_t i = 0; i < numSvs; i++)
        {
            const size_t offset = headerSize + i * blockSize;

            if (offset + blockSize > serialized.size())
                break;

            SatelliteInfo sat;

            sat.gnssId = static_cast<EGnssId>(serialized[offset + 0]);
            sat.svId = serialized[offset + 1];
            sat.cno = serialized[offset + 2];
            sat.elevation = static_cast<int8_t>(serialized[offset + 3]);
            sat.azimuth = readLE<int16_t>(serialized, offset + 4);

            const uint32_t flags = readLE<uint32_t>(serialized, offset + 8);
            sat.quality = static_cast<ESvQuality>(flags & 0x07);
            sat.usedInFix = (flags >> 3) & 0x01;
            const uint8_t health = (flags >> 4) & 0x03;
            sat.healthy = (health == 1);
            sat.diffCorr = (flags >> 6) & 0x01;
            sat.ephAvail = (flags >> 8) & 0x01;
            sat.almAvail = (flags >> 9) & 0x01;

            satellites_.push_back(sat);
        }
    }

    const std::vector<SatelliteInfo>& satellites() const
    {
        return satellites_;
    }

private:
    std::vector<SatelliteInfo> satellites_;
};

}  // JimmyPaputto::ubxmsg

#endif  // UBX_NAV_SAT_HPP_

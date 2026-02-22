/*
 * Jimmy Paputto 2025
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
        // UBX-NAV-SAT payload starts at offset 6
        // Byte 6+0: iTOW (U4) - GPS time of week
        // Byte 6+4: version (U1)
        // Byte 6+5: numSvs (U1)
        // Byte 6+6: reserved1 (U2)
        // Repeated block starts at offset 6+8, each block is 12 bytes
        //   +0: gnssId (U1)
        //   +1: svId (U1)
        //   +2: cno (U1)
        //   +3: elev (I1)
        //   +4: azim (I2)
        //   +6: prRes (I2) - pseudorange residual (x0.1 m)
        //   +8: flags (X4)
        //     bits 2..0: qualityInd
        //     bit 3: svUsed
        //     bits 5..4: health (0=unknown, 1=healthy, 2=unhealthy)
        //     bit 6: diffCorr
        //     bit 8: ephAvail
        //     bit 9: almAvail

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

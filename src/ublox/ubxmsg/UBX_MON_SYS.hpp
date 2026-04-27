/*
 * Jimmy Paputto 2026
 */

#ifndef UBX_MON_SYS_HPP_
#define UBX_MON_SYS_HPP_

#include "IUbxMsg.hpp"
#include "ublox/SystemHealth.hpp"
#include "common/Utils.hpp"


namespace JimmyPaputto::ubxmsg
{

class UBX_MON_SYS : public IUbxMsg
{
public:
    explicit UBX_MON_SYS() = default;

    explicit UBX_MON_SYS(std::span<const uint8_t> frame)
    {
        deserialize(frame);
    }

    std::vector<uint8_t> serialize() const override
    {
        return { };
    }

    // Frame layout: [0-1] sync, [2] class, [3] msgId, [4-5] length, [6..] payload.
    // Payload (24 B) per UBX-23006991 §3.14.13:
    //   off 0  U1   msgVer       (0x01)
    //   off 1  U1   bootType
    //   off 2  U1   cpuLoad      [%]
    //   off 3  U1   cpuLoadMax   [%]
    //   off 4  U1   memUsage     [%]
    //   off 5  U1   memUsageMax  [%]
    //   off 6  U1   ioUsage      [%]
    //   off 7  U1   ioUsageMax   [%]
    //   off 8  U4   runTime      [s]
    //   off 12 U2   noticeCount
    //   off 14 U2   warnCount
    //   off 16 U2   errorCount
    //   off 18 I1   tempValue    [°C]
    //   off 19 U1[5] reserved0
    void deserialize(std::span<const uint8_t> serialized) override
    {
        constexpr size_t hdr = 6;          // sync(2)+class+id+length(2)
        constexpr size_t payloadLen = 24;
        if (serialized.size() < hdr + payloadLen)
            return;

        systemHealth_.msgVersion   = serialized[hdr + 0];
        systemHealth_.bootType     = static_cast<EBootType>(serialized[hdr + 1]);
        systemHealth_.cpuLoad      = serialized[hdr + 2];
        systemHealth_.cpuLoadMax   = serialized[hdr + 3];
        systemHealth_.memUsage     = serialized[hdr + 4];
        systemHealth_.memUsageMax  = serialized[hdr + 5];
        systemHealth_.ioUsage      = serialized[hdr + 6];
        systemHealth_.ioUsageMax   = serialized[hdr + 7];
        systemHealth_.runTime      = readLE<uint32_t>(serialized, hdr + 8);
        systemHealth_.noticeCount  = readLE<uint16_t>(serialized, hdr + 12);
        systemHealth_.warnCount    = readLE<uint16_t>(serialized, hdr + 14);
        systemHealth_.errorCount   = readLE<uint16_t>(serialized, hdr + 16);
        systemHealth_.temperatureC = static_cast<int8_t>(serialized[hdr + 18]);
        systemHealth_.valid        = true;
    }

    static std::vector<uint8_t> poll()
    {
        return buildFrame({0xB5, 0x62, 0x0A, 0x39, 0x00, 0x00});
    }

    const SystemHealth& systemHealth() const { return systemHealth_; }

private:
    SystemHealth systemHealth_;
};

}  // JimmyPaputto::ubxmsg

#endif  // UBX_MON_SYS_HPP_

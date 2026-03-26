/*
 * Jimmy Paputto 2026
 */

#ifndef UBX_TIM_TM2_HPP_
#define UBX_TIM_TM2_HPP_

#include "IUbxMsg.hpp"

#include "ublox/TimeMark.hpp"
#include "common/Utils.hpp"


namespace JimmyPaputto::ubxmsg
{

class UBX_TIM_TM2: public IUbxMsg
{
public:
    explicit UBX_TIM_TM2() = default;

    explicit UBX_TIM_TM2(std::span<const uint8_t> frame)
    {
        deserialize(frame);
    }

    std::vector<uint8_t> serialize() const override
    {
        return {};
    }

    void deserialize(std::span<const uint8_t> serialized) override
    {
        timeMark_.channel = serialized[6];

        const uint8_t flags = serialized[7];
        timeMark_.mode = static_cast<ETimeMarkMode>(flags & 0x01);
        timeMark_.run = static_cast<ETimeMarkRun>((flags >> 1) & 0x01);
        timeMark_.newFallingEdge = getBit(flags, 2);
        timeMark_.timeBase = static_cast<ETimeMarkTimeBase>((flags >> 3) & 0x03);
        timeMark_.utcAvailable = getBit(flags, 5);
        timeMark_.timeValid = getBit(flags, 6);
        timeMark_.newRisingEdge = getBit(flags, 7);

        timeMark_.count = readLE<uint16_t>(serialized, 8);
        timeMark_.weekNumberRising = readLE<uint16_t>(serialized, 10);
        timeMark_.weekNumberFalling = readLE<uint16_t>(serialized, 12);
        timeMark_.towRising_ms = readLE<uint32_t>(serialized, 14);
        timeMark_.towSubRising_ns = readLE<uint32_t>(serialized, 18);
        timeMark_.towFalling_ms = readLE<uint32_t>(serialized, 22);
        timeMark_.towSubFalling_ns = readLE<uint32_t>(serialized, 26);
        timeMark_.accuracyEstimate_ns = readLE<uint32_t>(serialized, 30);
    }

    TimeMark timeMark() const
    {
        return timeMark_;
    }

private:
    TimeMark timeMark_;
};

}  // JimmyPaputto::ubxmsg

#endif  // UBX_TIM_TM2_HPP_

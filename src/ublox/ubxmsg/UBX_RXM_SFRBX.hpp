/*
 * Jimmy Paputto 2026
 */

#ifndef UBX_RXM_SFRBX_HPP_
#define UBX_RXM_SFRBX_HPP_

#include "IUbxMsg.hpp"

#include "ublox/SubframeData.hpp"
#include "common/Utils.hpp"


namespace JimmyPaputto::ubxmsg
{

/*
 * UBX-RXM-SFRBX: Broadcast Navigation Data Subframe
 *
 * Class: 0x02  Msg: 0x13
 *
 * Payload (header = 8 bytes + numWords * 4 bytes):
 *   Byte 0: gnssId
 *   Byte 1: svId
 *   Byte 2: sigId (reserved on protocol < 27)
 *   Byte 3: freqId
 *   Byte 4: numWords
 *   Byte 5: chn
 *   Byte 6: version
 *   Byte 7: reserved
 *   Bytes 8..: dwrd[0..numWords-1] (uint32_t LE each)
 */
class UBX_RXM_SFRBX: public IUbxMsg
{
public:
    explicit UBX_RXM_SFRBX() = default;

    explicit UBX_RXM_SFRBX(std::span<const uint8_t> frame)
    {
        deserialize(frame);
    }

    std::vector<uint8_t> serialize() const override
    {
        return {};
    }

    void deserialize(std::span<const uint8_t> serialized) override
    {
        // UBX frame: [sync1 sync2 class msg len_lo len_hi payload... ck_a ck_b]
        // Payload starts at offset 6
        constexpr size_t payloadOffset = 6;

        if (serialized.size() < payloadOffset + 8)
            return;

        subframe_.gnssId   = static_cast<EGnssId>(serialized[payloadOffset + 0]);
        subframe_.svId     = serialized[payloadOffset + 1];
        subframe_.sigId    = serialized[payloadOffset + 2];
        subframe_.freqId   = serialized[payloadOffset + 3];
        subframe_.numWords = serialized[payloadOffset + 4];
        subframe_.chn      = serialized[payloadOffset + 5];
        subframe_.version  = serialized[payloadOffset + 6];
        // byte 7 reserved

        subframe_.words.clear();
        subframe_.words.reserve(subframe_.numWords);

        constexpr size_t wordsOffset = payloadOffset + 8;
        for (uint8_t i = 0; i < subframe_.numWords; ++i)
        {
            const size_t offset = wordsOffset + i * 4;
            if (offset + 4 > serialized.size())
                break;

            subframe_.words.push_back(
                readLE<uint32_t>(serialized, offset));
        }
    }

    const SubframeData& subframe() const
    {
        return subframe_;
    }

private:
    SubframeData subframe_;
};

}  // JimmyPaputto::ubxmsg

#endif  // UBX_RXM_SFRBX_HPP_

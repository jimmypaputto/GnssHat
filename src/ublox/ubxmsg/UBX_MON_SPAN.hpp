/*
 * Jimmy Paputto 2026
 */

#ifndef UBX_MON_SPAN_HPP_
#define UBX_MON_SPAN_HPP_

#include <cmath>

#include "IUbxMsg.hpp"
#include "ublox/RFBlockSpectrumData.hpp"


namespace JimmyPaputto::ubxmsg
{

class UBX_MON_SPAN : public IUbxMsg
{
public:
    explicit UBX_MON_SPAN() = default;

    explicit UBX_MON_SPAN(std::span<const uint8_t> frame)
    {
        rfBlocksSpectrumData_.reserve(RfBlockSpectrumData::maxNumberOfRfBlocks);
        deserialize(frame);
    }

    std::vector<uint8_t> serialize() const override
    {
        return { };
    }

    void deserialize(std::span<const uint8_t> serialized) override
    {
        uint8_t numberOfRfBlocks = serialized[7];

        rfBlocksSpectrumData_.clear();

        for (uint8_t i = 0; i < numberOfRfBlocks; i++)
        {
            RfBlockSpectrumData rfBlockSpectrumData{};

            rfBlockSpectrumData.id = i;
            rfBlockSpectrumData.data.resize(256);
            std::copy(serialized.begin() + 10 + (i * 272), serialized.begin() + 10 + 256 + (i * 272), rfBlockSpectrumData.data.begin());

            rfBlockSpectrumData.span = readLE<uint32_t>(serialized,(10 + 256 + (i * 272)));
            rfBlockSpectrumData.resolution = readLE<uint32_t>(serialized,(14 + 256 + (i * 272)));
            rfBlockSpectrumData.centerFreq = readLE<uint32_t>(serialized,(18 + 256 + (i * 272)));
            rfBlockSpectrumData.gain = serialized[(22 + 256 + (i * 272))];

            rfBlocksSpectrumData_.push_back(rfBlockSpectrumData);
        }
    }

    static std::vector<uint8_t> poll()
    {
        return buildFrame({ 0xB5, 0x62, 0x0A, 0x31, 0x00, 0x00 });
    }

    const std::vector<RfBlockSpectrumData>& rfBlocksSpectrumData() const
    {
        return rfBlocksSpectrumData_;
    }

private:
    std::vector<RfBlockSpectrumData> rfBlocksSpectrumData_;
};

}  // JimmyPaputto::ubxmsg

#endif  // UBX_MON_SPAN_HPP_

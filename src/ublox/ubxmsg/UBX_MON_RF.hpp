/*
 * Jimmy Paputto 2022
 */

#ifndef UBX_MON_RF_HPP_
#define UBX_MON_RF_HPP_

#include <cmath>

#include "IUbxMsg.hpp"
#include "ublox/RFBlock.hpp"


namespace JimmyPaputto::ubxmsg
{

class UBX_MON_RF : public IUbxMsg
{
public:
    explicit UBX_MON_RF() = default;

    explicit UBX_MON_RF(const std::vector<uint8_t>& frame)
    {
        rfBlocks_.reserve(RfBlock::maxNumberOfRfBlocks);
        deserialize(frame);
    }

    std::vector<uint8_t> serialize() const override
    {
        return { };
    }

    void deserialize(const std::vector<uint8_t>& serialized) override
    {
        uint8_t numberOfRfBlocks = serialized[7];

        rfBlocks_.clear();

        for (uint8_t i = 0; i < numberOfRfBlocks; i++)
        {
            RfBlock rfBlock;

            rfBlock.id = static_cast<EBand>(serialized[10 + i * 24]);
            rfBlock.jammingState = static_cast<EJammingState>(
                0b11 & serialized[11 + i * 24]);
            rfBlock.antennaStatus = static_cast<EAntennaStatus>(
                serialized[12 + i * 24]);
            rfBlock.antennaPower = static_cast<EAntennaPower>(
                serialized[13 + i * 24]);
            rfBlock.postStatus = readLE<uint32_t>(serialized, 14 + i * 24);
            rfBlock.noisePerMS = readLE<uint16_t>(serialized, 22 + i * 24);
            rfBlock.agcMonitor =
                readLE<uint16_t>(serialized, 24 + i * 24) * 100.0 / 8191.0;
            rfBlock.cwInterferenceSuppressionLevel =
                serialized[26 + i * 24] * 100.0 / 255.0;
            rfBlock.ofsI = serialized[27 + i * 24];
            rfBlock.magI = serialized[28 + i * 24];
            rfBlock.ofsQ = serialized[29 + i * 24];
            rfBlock.magQ = serialized[30 + i * 24];

            rfBlocks_.push_back(rfBlock);
        }
    }

    const std::vector<RfBlock>& rfBlocks() const
    {
        return rfBlocks_;
    }

private:
    std::vector<RfBlock> rfBlocks_;
};

}  // JimmyPaputto::ubxmsg

#endif  // UBX_MON_RF_HPP_

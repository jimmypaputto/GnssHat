/*
 * Jimmy Paputto 2026
 */

#ifndef UBX_RXM_RAWX_HPP_
#define UBX_RXM_RAWX_HPP_

#include "IUbxMsg.hpp"

#include "ublox/RawObservation.hpp"
#include "common/Utils.hpp"


namespace JimmyPaputto::ubxmsg
{

class UBX_RXM_RAWX: public IUbxMsg
{
public:
    explicit UBX_RXM_RAWX() = default;

    explicit UBX_RXM_RAWX(std::span<const uint8_t> frame)
    {
        rawMeasurements_.observations.reserve(
            RawMeasurements::maxNumberOfMeasurements);
        deserialize(frame);
    }

    std::vector<uint8_t> serialize() const override
    {
        return {};
    }

    void deserialize(std::span<const uint8_t> serialized) override
    {
        rawMeasurements_.rcvTow = readLE<double>(serialized, 6);
        rawMeasurements_.week = readLE<uint16_t>(serialized, 14);
        rawMeasurements_.leapS = static_cast<int8_t>(serialized[16]);
        rawMeasurements_.numMeas = serialized[17];

        const uint8_t recStat = serialized[18];
        rawMeasurements_.leapSecDetermined = recStat & 0x01;
        rawMeasurements_.clkReset = (recStat >> 1) & 0x01;

        rawMeasurements_.version = serialized[19];

        rawMeasurements_.observations.clear();

        constexpr size_t headerSize = 22;
        constexpr size_t blockSize = 32;

        for (uint8_t i = 0; i < rawMeasurements_.numMeas; i++)
        {
            const size_t offset = headerSize + i * blockSize;

            if (offset + blockSize > serialized.size())
                break;

            RawObservation obs;

            obs.prMes = readLE<double>(serialized, offset + 0);
            obs.cpMes = readLE<double>(serialized, offset + 8);
            obs.doMes = readLE<float>(serialized, offset + 16);
            obs.gnssId = static_cast<EGnssId>(serialized[offset + 20]);
            obs.svId = serialized[offset + 21];
            obs.sigId = serialized[offset + 22];
            obs.freqId = serialized[offset + 23];
            obs.locktime = readLE<uint16_t>(serialized, offset + 24);
            obs.cno = serialized[offset + 26];
            obs.prStdev = serialized[offset + 27] & 0x0F;
            obs.cpStdev = serialized[offset + 28] & 0x0F;
            obs.doStdev = serialized[offset + 29] & 0x0F;

            const uint8_t trkStat = serialized[offset + 30];
            obs.prValid = trkStat & 0x01;
            obs.cpValid = (trkStat >> 1) & 0x01;
            obs.halfCyc = (trkStat >> 2) & 0x01;
            obs.subHalfCyc = (trkStat >> 3) & 0x01;

            rawMeasurements_.observations.push_back(obs);
        }
    }

    const RawMeasurements& rawMeasurements() const
    {
        return rawMeasurements_;
    }

private:
    RawMeasurements rawMeasurements_;
};

}  // JimmyPaputto::ubxmsg

#endif  // UBX_RXM_RAWX_HPP_

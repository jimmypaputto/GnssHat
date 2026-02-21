/*
 * Jimmy Paputto 2025
 */

#ifndef UBX_CFG_TP5_HPP_
#define UBX_CFG_TP5_HPP_

#include <vector>
#include <stdexcept>

#include "common/Utils.hpp"
#include "ublox/TimepulsePinConfig.hpp"

#include "IUbxMsg.hpp"


namespace JimmyPaputto::ubxmsg
{

enum class ETimepulseIdx : uint8_t
{
    TIMEPULSE0 = 0x00,
    TIMEPULSE1 = 0x01
};

enum class EGridUtcGps : uint8_t
{
    UTC = 0x0,
    GPS = 0x1
};

struct Timepulse5
{
    ETimepulseIdx tpIdx;
    int16_t antCableDelay;
    int16_t rfGroupDelay;
    uint32_t freqPeriod;
    uint32_t freqPeriodLock;
    uint32_t pulseLenRatio;
    uint32_t pulseLenRatioLock;
    int32_t userConfigDelay;

    bool active;
    bool lockGpsFreq;
    bool lockedOtherSet;
    bool isFreq;
    bool isLength;
    bool allignToTow;
    ETimepulsePinPolarity polarity;
    EGridUtcGps gridUtcGps;

    static Timepulse5 create(const TimepulsePinConfig timepulsePinConfig)
    {
        constexpr uint32_t pulseWidthConstant = 0xFFFFFFFF;
        Timepulse5 timepulse;
        timepulse.tpIdx = ETimepulseIdx::TIMEPULSE0;
        timepulse.antCableDelay = 0;
        timepulse.rfGroupDelay = 0;
        timepulse.freqPeriod = timepulsePinConfig.pulseWhenNoFix.has_value() ?
            timepulsePinConfig.pulseWhenNoFix->frequency : 0;
        timepulse.freqPeriodLock = timepulsePinConfig.fixedPulse.frequency;
        timepulse.pulseLenRatio = timepulsePinConfig.pulseWhenNoFix.has_value() ?
            timepulsePinConfig.pulseWhenNoFix->pulseWidth * pulseWidthConstant : 0;
        timepulse.pulseLenRatioLock =
            timepulsePinConfig.fixedPulse.pulseWidth * pulseWidthConstant;
        timepulse.userConfigDelay = 0;

        timepulse.active = timepulsePinConfig.active;
        timepulse.lockGpsFreq = true;
        timepulse.lockedOtherSet = !timepulsePinConfig.pulseWhenNoFix.has_value();
        timepulse.isFreq = true;
        timepulse.isLength = false;
        timepulse.allignToTow = true;
        timepulse.polarity = timepulsePinConfig.polarity;
        timepulse.gridUtcGps = EGridUtcGps::UTC;

        return timepulse;
    }

    std::vector<uint8_t> serialize()
    {
        std::vector<uint8_t> serialized;
        serialized.reserve(32);
        serialized.push_back(static_cast<uint8_t>(tpIdx));
        serialized.push_back(0x00);
        serialized.push_back(0x00);
        serialized.push_back(0x00);

        appendLE(antCableDelay, serialized);
        appendLE(rfGroupDelay, serialized);
        appendLE(freqPeriod, serialized);
        appendLE(freqPeriodLock, serialized);
        appendLE(pulseLenRatio, serialized);
        appendLE(pulseLenRatioLock, serialized);
        appendLE(userConfigDelay, serialized);

        uint32_t flags = 0;
        setBit(flags, 0, active);
        setBit(flags, 1, lockGpsFreq);
        setBit(flags, 2, lockedOtherSet);
        setBit(flags, 3, isFreq);
        setBit(flags, 4, isLength);
        setBit(flags, 5, allignToTow);
        setBit(flags, 6, static_cast<bool>(polarity));
        setBit(flags, 7, static_cast<bool>(gridUtcGps));

        appendLE(flags, serialized);

        return serialized;
    }

    static Timepulse5 deserialize(std::span<const uint8_t> serialized)
    {
        Timepulse5 timepulse;
        timepulse.tpIdx = static_cast<ETimepulseIdx>(serialized[0]);
        timepulse.antCableDelay = readLE<int16_t>(serialized, 4);
        timepulse.rfGroupDelay = readLE<int16_t>(serialized, 6);
        timepulse.freqPeriod = readLE<uint32_t>(serialized, 8);
        timepulse.freqPeriodLock = readLE<uint32_t>(serialized, 12);
        timepulse.pulseLenRatio = readLE<uint32_t>(serialized, 16);
        timepulse.pulseLenRatioLock = readLE<uint32_t>(serialized, 20);
        timepulse.userConfigDelay = readLE<int32_t>(serialized, 24);

        uint32_t flags = readLE<uint32_t>(serialized, 28);
        timepulse.active = getBit(flags, 0);
        timepulse.lockGpsFreq = getBit(flags, 1);
        timepulse.lockedOtherSet = getBit(flags, 2);
        timepulse.isFreq = getBit(flags, 3);
        timepulse.isLength = getBit(flags, 4);
        timepulse.allignToTow = getBit(flags, 5);
        timepulse.polarity = static_cast<ETimepulsePinPolarity>(getBit(flags, 6));
        timepulse.gridUtcGps = static_cast<EGridUtcGps>(getBit(flags, 7));

        return timepulse;
    }
};


class UBX_CFG_TP5 : public IUbxMsg
{
public:
    explicit UBX_CFG_TP5() = default;

    explicit UBX_CFG_TP5(const TimepulsePinConfig& timepulsePinConfig)
    :   timepulse_(Timepulse5::create(timepulsePinConfig))
    {}

    explicit UBX_CFG_TP5(std::span<const uint8_t> frame)
    {
        deserialize(frame);
    }

    std::vector<uint8_t> serialize() const override
    {
        std::vector<uint8_t> payload = timepulse_.serialize();
        std::vector<uint8_t> frame = {
            0xB5, 0x62, 0x06, 0x31, static_cast<uint8_t>(payload.size()), 0x00
        };
        frame.insert(frame.end(), payload.begin(), payload.end());
        return buildFrame(frame);
    }

    void deserialize(std::span<const uint8_t> frame) override
    {
        if (frame.size() < 34)
        {
            throw std::runtime_error("Invalid frame size for UBX_CFG_TP5");
        }
        timepulse_ = Timepulse5::deserialize(
            frame.subspan(6, frame.size() - 8)
        );
    }

    static std::vector<uint8_t> poll()
    {
        const auto tpIdx = ETimepulseIdx::TIMEPULSE0;
        return poll(tpIdx);
    }

    Timepulse5 timepulse() const { return timepulse_; }

private:
    static std::vector<uint8_t> poll(const ETimepulseIdx tpIdx)
    {
        return buildFrame({
            0xB5, 0x62, 0x06, 0x31, 0x01, 0x00, static_cast<uint8_t>(tpIdx)
        });
    }

    mutable Timepulse5 timepulse_;
};

}  // JimmyPaputto::ubxmsg

#endif  // UBX_CFG_TP5_HPP_

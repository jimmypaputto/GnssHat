/*
 * Jimmy Paputto 2025
 */

#ifndef UBX_CFG_RATE_HPP_
#define UBX_CFG_RATE_HPP_

#include <vector>
#include <stdexcept>

#include "IUbxMsg.hpp"


namespace JimmyPaputto::ubxmsg
{

struct Rate
{
    uint16_t measRate; // Measurement Rate, GPS measurements are taken every measRate milliseconds
    uint16_t navRate;  // Navigation Rate, in number of measurement cycles. This parameter cannot be changed, and must be set to 1.
    uint16_t timeRef;  // Time system to which measurements are aligned
};

class UBX_CFG_RATE : public IUbxMsg
{
public:
    explicit UBX_CFG_RATE() = default;

    explicit UBX_CFG_RATE(const Rate& rate)
    : rate_(rate)
    {}

    explicit UBX_CFG_RATE(std::span<const uint8_t> frame)
    {
        deserialize(frame);
    }

    static UBX_CFG_RATE createDefault()
    {
        return UBX_CFG_RATE(Rate { .measRate = 1000, .navRate = 1, .timeRef = 0 });
    }

    std::vector<uint8_t> serialize() const override
    {
        std::vector<uint8_t> payload;
        payload.push_back(static_cast<uint8_t>(rate_.measRate & 0xFF));
        payload.push_back(static_cast<uint8_t>((rate_.measRate >> 8) & 0xFF));
        payload.push_back(static_cast<uint8_t>(rate_.navRate & 0xFF));
        payload.push_back(static_cast<uint8_t>((rate_.navRate >> 8) & 0xFF));
        payload.push_back(static_cast<uint8_t>(rate_.timeRef & 0xFF));
        payload.push_back(static_cast<uint8_t>((rate_.timeRef >> 8) & 0xFF));

        std::vector<uint8_t> frame = { 0xB5, 0x62, 0x06, 0x08, 0x06, 0x00 };
        frame.insert(frame.end(), payload.begin(), payload.end());
        return buildFrame(frame);
    }

    void deserialize(std::span<const uint8_t> frame) override
    {
        if (frame.size() < 14)
        {
            throw std::runtime_error("Invalid frame size for UBX_CFG_RATE");
        }
        rate_.measRate = static_cast<uint16_t>(frame[6] | (frame[7] << 8));
        rate_.navRate = static_cast<uint16_t>(frame[8] | (frame[9] << 8));
        rate_.timeRef = static_cast<uint16_t>(frame[10] | (frame[11] << 8));
    }

    static std::vector<uint8_t> poll()
    {
        return buildFrame({ 0xB5, 0x62, 0x06, 0x08, 0x00, 0x00 });
    }

private:
    Rate rate_;
};

}  // JimmyPaputto::ubxmsg

#endif  // UBX_CFG_RATE_HPP_

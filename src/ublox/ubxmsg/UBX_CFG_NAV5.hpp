/*
 * Jimmy Paputto 2022
 */

#ifndef UBX_CFG_NAV5_HPP_
#define UBX_CFG_NAV5_HPP_

#include "ublox/EDynamicModel.hpp"
#include "IUbxMsg.hpp"
#include "common/Utils.hpp"


namespace JimmyPaputto::ubxmsg
{

class UBX_CFG_NAV5 : public IUbxMsg  // only dynamic platform model supported
{
public:
    explicit UBX_CFG_NAV5() = default;

    explicit UBX_CFG_NAV5(const EDynamicModel& dynamicModel)
    :	mask_(dynamicModelBitForMask_),
        dynamicModel_(dynamicModel)
    {}

    explicit UBX_CFG_NAV5(const std::vector<uint8_t>& serialized)
    {
        deserialize(serialized);
    }

    std::vector<uint8_t> serialize() const override
    {
        const std::vector<uint8_t> begining = {
            0xB5, 0x62, 0x06, 0x24, 0x24, 0x00
        };
        uint8_t maskLE[2];
        std::memcpy(maskLE, &mask_, sizeof(mask_));
        const auto serialized =
            begining +
            std::vector<uint8_t> {
                maskLE[0], maskLE[1],
                static_cast<uint8_t>(dynamicModel_)
            } +
            std::vector<uint8_t>(33, 0);

        return buildFrame(serialized);
    }

    void deserialize(const std::vector<uint8_t>& serialized) override
    {
        mask_ = readLE<uint16_t>(serialized, 6);
        dynamicModel_ = EDynamicModel(serialized[8]);
    }

    EDynamicModel dynamicModel() const
    {
        return dynamicModel_;
    }

    static std::vector<uint8_t> poll()
    {
        return buildFrame({
            0xB5, 0x62, 0x06, 0x24, 0x00, 0x00
        });
    }

private:
    mutable uint16_t mask_;
    EDynamicModel dynamicModel_;
    static constexpr const uint16_t dynamicModelBitForMask_ = 0x0001;
};

}  // JimmyPaputto::ubxmsg

#endif  // UBX_CFG_NAV5_HPP_

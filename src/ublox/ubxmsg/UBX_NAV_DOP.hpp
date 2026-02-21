/*
 * Jimmy Paputto 2022
 */

#ifndef UBX_NAV_DOP_HPP_
#define UBX_NAV_DOP_HPP_

#include "IUbxMsg.hpp"

#include "ublox/DilutionOverPrecision.hpp"


namespace JimmyPaputto::ubxmsg
{

class UBX_NAV_DOP: public IUbxMsg
{
public:
    explicit UBX_NAV_DOP() = default;

    explicit UBX_NAV_DOP(const std::vector<uint8_t>& frame)
    {
        deserialize(frame);
    }

    std::vector<uint8_t> serialize() const override
    {
        return { };
    }

    void deserialize(const std::vector<uint8_t>& serialized) override
    {
        dop_.geometric = readLE<uint16_t>(serialized, 10) * 0.01;
        dop_.position = readLE<uint16_t>(serialized, 12) * 0.01;
        dop_.time = readLE<uint16_t>(serialized, 14) * 0.01;
        dop_.vertical = readLE<uint16_t>(serialized, 16) * 0.01;
        dop_.horizontal = readLE<uint16_t>(serialized, 18) * 0.01;
        dop_.northing = readLE<uint16_t>(serialized, 20) * 0.01;
        dop_.easting = readLE<uint16_t>(serialized, 22) * 0.01;
    }

    DilutionOverPrecision dop() const
    {
        return dop_;
    }

private:
    DilutionOverPrecision dop_;
};

}  // JimmyPaputto::ubxmsg

#endif  // UBX_NAV_DOP_HPP_

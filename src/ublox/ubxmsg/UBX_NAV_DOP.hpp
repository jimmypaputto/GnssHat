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
        dop_.geometric = (*(uint16_t*)&serialized[10]) * 0.01;
        dop_.position = (*(uint16_t*)&serialized[12]) * 0.01;
        dop_.time = (*(uint16_t*)&serialized[14]) * 0.01;
        dop_.vertical = (*(uint16_t*)&serialized[16]) * 0.01;
        dop_.horizontal = (*(uint16_t*)&serialized[18]) * 0.01;
        dop_.northing = (*(uint16_t*)&serialized[20]) * 0.01;
        dop_.easting = (*(uint16_t*)&serialized[22]) * 0.01;
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

/*
 * Jimmy Paputto 2023
 */

#ifndef UBX_CFG_CFG_HPP_
#define UBX_CFG_CFG_HPP_

#include "IUbxMsg.hpp"


namespace JimmyPaputto::ubxmsg
{

class UBX_CFG_CFG: public IUbxMsg  // not fully supported, just used for save cfg
{
public:
    explicit UBX_CFG_CFG() = default;

    std::vector<uint8_t> serialize() const override
    {
        return { };
    }

    void deserialize(std::span<const uint8_t>) override
    {

    }

    static std::vector<uint8_t> poll()
    {
        return buildFrame( {
            0xB5, 0x62, 0x06, 0x09, 0x00, 0x00
        });
    }

    static std::vector<uint8_t> saveToFlash()
    {
        return buildFrame( {
            0xB5, 0x62, 0x06, 0x09, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03
        });
    }
};

}  // JimmyPaputto::ubxmsg

#endif  // UBX_CFG_CFG_HPP_

/*
 * Jimmy Paputto 2022
 */

#ifndef UBX_ACK_NAK_HPP_
#define UBX_ACK_NAK_HPP_

#include "UBX_ACK_ACK.hpp"


namespace JimmyPaputto::ubxmsg
{

class UBX_ACK_NAK : public UBX_ACK_ACK
{
public:
    explicit UBX_ACK_NAK() = default;

    explicit UBX_ACK_NAK(std::span<const uint8_t> frame)
    :	UBX_ACK_ACK(frame)
    {}

    std::vector<uint8_t> serialize() const override
    {
        return buildFrame({
            0xB5, 0x62, 0x05, 0x00, 0x02, 0x00, classId_, msgId_
        });
    }
};

}  // JimmyPaputto::ubxmsg

#endif  // UBX_ACK_NAK_HPP_

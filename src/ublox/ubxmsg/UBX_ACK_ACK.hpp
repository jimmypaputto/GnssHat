/*
 * Jimmy Paputto 2022
 */

#ifndef UBX_ACK_ACK_HPP_
#define UBX_ACK_ACK_HPP_

#include "IUbxMsg.hpp"


namespace JimmyPaputto::ubxmsg
{

class UBX_ACK_ACK: public IUbxMsg
{
public:
    explicit UBX_ACK_ACK() = default;

    explicit UBX_ACK_ACK(const std::vector<uint8_t>& frame)
    {
        deserialize(frame);
    }

    std::vector<uint8_t> serialize() const override
    {
        return buildFrame({ 0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, classId_, msgId_ });
    }

    void deserialize(const std::vector<uint8_t>& serialized) override
    {
        classId_ = serialized[6];
        msgId_ = serialized[7];
    }

    std::pair<uint8_t, uint8_t> classMsgId() const noexcept
    {
        return { classId_, msgId_ };
    }

protected:
    uint8_t classId_;
    uint8_t msgId_;
};

}  // JimmyPaputto::ubxmsg

#endif  // UBX_ACK_ACK_HPP_

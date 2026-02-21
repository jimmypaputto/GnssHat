/*
 * Jimmy Paputto 2022
 */

#ifndef UBX_CFG_MSG_HPP_
#define UBX_CFG_MSG_HPP_

#include "common/Utils.hpp"

#include "ublox/EUbxMsg.hpp"
#include "ublox/EUbxPrt.hpp"
#include "ublox/UbxClassMsgId.hpp"

#include "IUbxMsg.hpp"


namespace JimmyPaputto::ubxmsg
{

class UBX_CFG_MSG: public IUbxMsg
{
public:
    explicit UBX_CFG_MSG() = default;

    explicit UBX_CFG_MSG(const EUbxMsg& eUbxMsg)
    {
        sendRateOnPort_.fill(0x00);
        const auto classMsgId = UbxClassMsgId::instance().translate(eUbxMsg);
        classId_ = classMsgId.first;
        msgId_ = classMsgId.second;
    }

    explicit UBX_CFG_MSG(const EUbxMsg& eUbxMsg,
        const std::array<uint8_t, numberOfUbxPrts>& sendRateOnPort)
    :	sendRateOnPort_(sendRateOnPort)
    {
        const auto classMsgId = UbxClassMsgId::instance().translate(eUbxMsg);
        classId_ = classMsgId.first;
        msgId_ = classMsgId.second;
    }

    explicit UBX_CFG_MSG(const uint8_t& classId, const uint8_t& msgId,
        const std::array<uint8_t, numberOfUbxPrts>& sendRateOnPort)
    :	classId_(classId),
        msgId_(msgId),
        sendRateOnPort_(sendRateOnPort)
    {}

    explicit UBX_CFG_MSG(std::span<const uint8_t> frame)
    {
        deserialize(frame);
    }

    std::vector<uint8_t> serialize() const override
    {
        const std::vector<uint8_t> begining = {
            0xB5, 0x62, 0x06, 0x01, 0x08, 0x00
        };

        auto serialized = 
            begining +
            std::vector<uint8_t> { classId_, msgId_ } +
            std::vector<uint8_t> (sendRateOnPort_.begin(), sendRateOnPort_.end());

        return buildFrame(serialized);
    }

    void deserialize(std::span<const uint8_t> serialized) override
    {
        classId_ = serialized[6];
        msgId_ = serialized[7];
        std::copy(serialized.begin() + 8, serialized.begin() + 14, sendRateOnPort_.begin());
    }

    static std::vector<uint8_t> poll(const EUbxMsg& eUbxMsg)
    {
        const auto classMsgId = UbxClassMsgId::instance().translate(eUbxMsg);
        return buildFrame({
            0xB5, 0x62, 0x06, 0x01, 0x02, 0x00, classMsgId.first, classMsgId.second
        });
    }

    inline const EUbxMsg eUbxMsg() const
    {
        return UbxClassMsgId::instance().translate({ classId_, msgId_ });
    }

    inline const std::array<uint8_t, numberOfUbxPrts> sendRates() const
    {
        return sendRateOnPort_;
    }

private:
    uint8_t classId_;
    uint8_t msgId_;
    std::array<uint8_t, numberOfUbxPrts> sendRateOnPort_;
};

}  // JimmyPaputto::ubxmsg

#endif  // UBX_CFG_MSG_HPP_

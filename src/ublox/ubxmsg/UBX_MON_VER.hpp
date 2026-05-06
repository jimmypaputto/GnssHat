/*
 * Jimmy Paputto 2026
 */

#ifndef UBX_MON_VER_HPP_
#define UBX_MON_VER_HPP_

#include <algorithm>
#include <string>
#include <vector>

#include "IUbxMsg.hpp"


namespace JimmyPaputto::ubxmsg
{

class UBX_MON_VER : public IUbxMsg
{
public:
    explicit UBX_MON_VER() = default;

    explicit UBX_MON_VER(std::span<const uint8_t> frame)
    {
        deserialize(frame);
    }

    std::vector<uint8_t> serialize() const override
    {
        return {};
    }

    void deserialize(std::span<const uint8_t> serialized) override
    {
        // Frame: [0-1] sync, [2] class, [3] msg, [4-5] length (LE), [6..] payload
        const uint16_t payloadLen =
            static_cast<uint16_t>(serialized[4]) |
            (static_cast<uint16_t>(serialized[5]) << 8);

        // Payload: bytes 0-29 = swVersion (30 chars), 30-39 = hwVersion (10 chars)
        constexpr size_t payloadOffset = 6;
        constexpr size_t swLen = 30;
        constexpr size_t hwLen = 10;
        constexpr size_t extLen = 30;

        if (payloadLen < swLen + hwLen)
            return;

        swVersion_ = extractString(serialized, payloadOffset, swLen);
        hwVersion_ = extractString(serialized, payloadOffset + swLen, hwLen);

        extensions_.clear();
        const size_t extensionBytes = payloadLen - swLen - hwLen;
        const size_t numExtensions = extensionBytes / extLen;

        for (size_t i = 0; i < numExtensions; i++)
        {
            std::string ext = extractString(
                serialized,
                payloadOffset + swLen + hwLen + i * extLen,
                extLen);
            if (!ext.empty())
                extensions_.push_back(std::move(ext));
        }
    }

    static std::vector<uint8_t> poll()
    {
        return buildFrame({0xB5, 0x62, 0x0A, 0x04, 0x00, 0x00});
    }

    const std::string &swVersion() const { return swVersion_; }
    const std::string &hwVersion() const { return hwVersion_; }
    const std::vector<std::string> &extensions() const { return extensions_; }

private:
    static std::string extractString(
        std::span<const uint8_t> data, size_t offset, size_t maxLen)
    {
        if (offset >= data.size())
            return {};
        auto slice = data.subspan(offset, std::min(maxLen, data.size() - offset));
        auto end = std::find(slice.begin(), slice.end(), uint8_t{0});
        return {slice.begin(), end};
    }

    std::string swVersion_;
    std::string hwVersion_;
    std::vector<std::string> extensions_;
};

}  // JimmyPaputto::ubxmsg

#endif  // UBX_MON_VER_HPP_

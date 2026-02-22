/*
 * Jimmy Paputto 2022
 */

#include "UbxParser.hpp"

#include <algorithm>

#include "EUbxMsg.hpp"
#include "Gnss.hpp"
#include "UbxFactory.hpp"
#include "Ublox.hpp"

#include "ubxmsg/UBX_ACK_ACK.hpp"
#include "ubxmsg/UBX_ACK_NAK.hpp"
#include "ubxmsg/UBX_CFG_MSG.hpp"
#include "ubxmsg/UBX_CFG_PRT.hpp"
#include "ubxmsg/UBX_NAV_DOP.hpp"
#include "ubxmsg/UBX_NAV_PVT.hpp"

#include "common/Utils.hpp"


namespace JimmyPaputto
{

UbxParser::UbxParser(IUbloxConfigRegistry& configRegistry,
    Notifier& navigationNotifier, bool callbackNotificationEnabled)
:   endFrameIt_(frames_.end()),
    configRegistry_(configRegistry),
    ubxCallbacks_(configRegistry, navigationNotifier, callbackNotificationEnabled)
{
    constexpr uint16_t maxFrameSize = 1024;
    unfinishedFrameFromBuffer_.reserve(maxFrameSize);
    for (auto& frame: frames_)
    {
        frame.reserve(maxFrameSize);
    }
}

std::vector<uint8_t> UbxParser::parse(std::span<const uint8_t> buffer)
{
    unfinishedFrameFromBuffer_.clear();
    extractFrames(buffer);
    for (auto frameIt = frames_.begin(); frameIt != endFrameIt_; frameIt++)
    {
        if (frameIt->empty())
            continue;

        const auto& frame = *frameIt;
        const auto eUbxMsg = UbxClassMsgId::instance().translate(
            { frame[2], frame[3] }
        );
        if (eUbxMsg != EUbxMsg::END_UBX)
        {
            ubxCallbacks_.run(UbxFactory::create(eUbxMsg, frame), eUbxMsg);
        }
    }
    return unfinishedFrameFromBuffer_;
}

void UbxParser::extractFrames(std::span<const uint8_t> buffer)
{
    std::array<uint8_t, 2> pattern {0xB5, 0x62};
    auto bufferBegin = buffer.begin();
    for (auto frameIt = frames_.begin(); frameIt != endFrameIt_; frameIt++)
    {
        frameIt->clear();
    }

    endFrameIt_ = frames_.begin();

    for (uint16_t frameIndex = 0; frameIndex < maxNumberOfFrames_; frameIndex++)
    {
        const auto beginFrameIterator = std::search(
            bufferBegin,
            buffer.end(),
            pattern.begin(),
            pattern.end()
        );

        if (beginFrameIterator != buffer.end() &&
            std::distance(beginFrameIterator, buffer.end()) > 5)
        {
            auto endFrameIterator = beginFrameIterator + 4;
            uint16_t dataLength;
            std::memcpy(&dataLength, &(*endFrameIterator), sizeof(uint16_t));
            if (std::distance(++endFrameIterator, buffer.end()) < dataLength + 3)
            {
                std::copy(
                    beginFrameIterator,
                    buffer.end(),
                    std::back_inserter(unfinishedFrameFromBuffer_)
                );
                break;
            }
            else
            {
                endFrameIterator += dataLength + 3;
                std::copy(
                    beginFrameIterator,
                    endFrameIterator,
                    std::back_inserter(frames_[frameIndex])
                );
                if (!checkFrame(frames_[frameIndex]))
                {
                    frames_[frameIndex].clear();
                }
                endFrameIt_++;
                bufferBegin = endFrameIterator;
            }
        }
        else if (
            std::distance(beginFrameIterator, buffer.end()) <= 5 &&
            beginFrameIterator != buffer.end()
        )
        {
            std::copy(
                beginFrameIterator,
                buffer.end(),
                std::back_inserter(unfinishedFrameFromBuffer_)
            );
            break;
        }
        else
        {
            break;
        }
    }
}

void UbxParser::addChecksum(std::vector<uint8_t>& frame)
{
    const auto ck = UbxParser::checksum(frame, 0);
    frame.insert(frame.end(), ck.begin(), ck.end());

    if (!UbxParser::checkFrame(frame))
    {
        frame = { 0xFF };
    }
}

std::array<uint8_t, 2> UbxParser::checksum(std::span<const uint8_t> frame,
    uint8_t offset)
{
    uint8_t cka = 0;
    uint8_t ckb = 0;

    for (std::size_t i = 2; i < frame.size() - offset; i++)
    {
        cka += frame[i];
        ckb += cka;
    }

    return { cka, ckb };
}

bool UbxParser::checkFrame(std::span<const uint8_t> frame)
{
    uint8_t cka = 0;
    uint8_t ckb = 0;

    for (std::size_t i = 2; i < frame.size() - 2; i++)
    {
        cka += frame[i];
        ckb += cka;
    }

    if (cka == frame[frame.size() - 2] && ckb == frame[frame.size() - 1])
    {
        return true;
    }

    return false;
}

}  // JimmyPaputto

/*
 * Jimmy Paputto 2025
 */

#include "Rtcm3Parser.hpp"

#include <algorithm>

#include "common/Utils.hpp"


namespace JimmyPaputto
{

Rtcm3Parser::Rtcm3Parser(Rtcm3Store& rtcm3Store)
:   endFrameIt_(frames_.end()),
    rtcm3Store_(rtcm3Store)
{
    for (auto& frame: frames_)
    {
        frame.reserve(1024);
    }
}

void Rtcm3Parser::parse(std::span<uint8_t> buffer,
    std::vector<uint8_t>& unfinishedFrame)
{
    extractFrames(buffer, unfinishedFrame);
    for (auto frameIt = frames_.begin(); frameIt != endFrameIt_; frameIt++)
    {
        if (frameIt->empty())
            continue;

        const uint16_t frameId = getFrameId(*frameIt);
        rtcm3Store_.updateFrame(frameId, *frameIt);
    }
}

void Rtcm3Parser::extractFrames(std::span<uint8_t> buffer,
    std::vector<uint8_t>& unfinishedFrame)
{
    constexpr uint8_t pattern {0xD3};
    auto bufferBegin = buffer.begin();

    for (auto frameIt = frames_.begin(); frameIt != endFrameIt_; frameIt++)
    {
        frameIt->clear();
    }
    endFrameIt_ = frames_.begin();

    for (uint16_t frameIndex = 0; frameIndex < maxNumberOfFrames_; frameIndex++)
    {
        const auto beginFrameIterator = std::find(
            bufferBegin, buffer.end(), pattern
        );

        if (beginFrameIterator != buffer.end() &&
            std::distance(beginFrameIterator, buffer.end()) > 2)
        {
            auto endFrameIterator = beginFrameIterator + 1;
            const uint8_t b1 = *(endFrameIterator);
            const uint8_t b2 = *(endFrameIterator + 1);
            const uint16_t dataLength = ((b1 & 0x03) << 8) | b2;
            if (
                std::distance(++endFrameIterator, buffer.end()) < dataLength + 4
            )
            {
                std::copy(
                    beginFrameIterator,
                    buffer.end(),
                    std::back_inserter(unfinishedFrame)
                );
                break;
            }
            else
            {
                endFrameIterator += dataLength + 4;
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
            std::distance(beginFrameIterator, buffer.end()) <= 2 &&
            beginFrameIterator != buffer.end()
        )
        {
            std::copy(
                beginFrameIterator,
                buffer.end(),
                std::back_inserter(unfinishedFrame)
            );
            break;
        }
        else
        {
            break;
        }
    }
}

uint16_t Rtcm3Parser::getFrameId(const std::vector<uint8_t>& frame)
{
    if (frame.size() < 6 || frame[0] != 0xD3)
        return 0;

    const uint8_t b0 = frame[3];
    const uint8_t b1 = frame[4];
    const uint16_t msgId = (uint16_t(b0) << 4) | (b1 >> 4);
    return msgId;
}

bool Rtcm3Parser::checkFrame(const std::vector<uint8_t>& frame)
{
    return true;
}

}  // JimmyPaputto

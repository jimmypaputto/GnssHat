/*
 * Jimmy Paputto 2025
 */

#include "Rtcm3Store.hpp"


#define SEMAPHORE_TIMEOUT 100

namespace JimmyPaputto
{

Rtcm3Store::Rtcm3Store()
{
    incomingFrames_.reserve(10);
}

void Rtcm3Store::updateFrame(const uint16_t id,
    const std::vector<uint8_t>& newFrame)
{
    if (xSemaphore_.takeResource(SEMAPHORE_TIMEOUT))
    {
        auto frameIt = frames_.find(id);
        if (frameIt == frames_.end())
        {
            frames_[id] = newFrame;
            xSemaphore_.releaseResource();
            return;
        }

        auto& frame = frameIt->second;

        if (frame.size() != newFrame.size())
        {
            frame = newFrame;
            xSemaphore_.releaseResource();
            return;
        }

        std::copy(newFrame.begin(), newFrame.end(), frame.begin());
        xSemaphore_.releaseResource();
    }
}

void Rtcm3Store::updateFramesAndNotify(
    const std::vector<std::vector<uint8_t>>& newFrames
)
{
    if (xSemaphore_.takeResource(SEMAPHORE_TIMEOUT))
    {
        incomingFrames_ = newFrames;
        xSemaphore_.releaseResource();
        roverNotifier_.notify();
    }
}

std::vector<uint8_t> Rtcm3Store::getFrame(const uint16_t id) const
{
    if (xSemaphore_.takeResource(SEMAPHORE_TIMEOUT))
    {
        auto frameIt = frames_.find(id);
        if (frameIt == frames_.end())
        {
            xSemaphore_.releaseResource();
            return {};
        }

        const auto result = frameIt->second;
        xSemaphore_.releaseResource();
        return result;
    }
    return {};
}

std::vector<std::vector<uint8_t>> Rtcm3Store::getFrames(
    const std::vector<uint16_t>& ids) const
{
    std::vector<std::vector<uint8_t>> result;
    if (xSemaphore_.takeResource(SEMAPHORE_TIMEOUT))
    {
        for (const auto& id : ids)
        {
            auto frameIt = frames_.find(id);
            if (frameIt != frames_.end())
            {
                result.push_back(frameIt->second);
            }
        }
        xSemaphore_.releaseResource();
    }
    return result;
}

std::vector<std::vector<uint8_t>> Rtcm3Store::waitForFrames()
{
    roverNotifier_.wait();
    if (xSemaphore_.takeResource(SEMAPHORE_TIMEOUT))
    {
        const auto result = incomingFrames_;
        incomingFrames_.clear();
        xSemaphore_.releaseResource();
        return result;
    }
    return {};
}

}  // JimmyPaputto

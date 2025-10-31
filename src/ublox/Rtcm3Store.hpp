/*
 * Jimmy Paputto 2025
 */

#ifndef JP_RTCM3_STORE_HPP_
#define JP_RTCM3_STORE_HPP_

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "common/JPGuard.hpp"
#include "common/Notifier.hpp"


namespace JimmyPaputto
{

class Rtcm3Store final
{
public:
    explicit Rtcm3Store();

    void updateFrame(const uint16_t id, const std::vector<uint8_t>& newFrame);
    void updateFramesAndNotify(
        const std::vector<std::vector<uint8_t>>& newFrames
    );

    std::vector<uint8_t> getFrame(const uint16_t id) const;
    std::vector<std::vector<uint8_t>> getFrames(
        const std::vector<uint16_t>& ids) const;

    std::vector<std::vector<uint8_t>> waitForFrames();

private:
    std::unordered_map<uint16_t, std::vector<uint8_t>> frames_;
    std::vector<std::vector<uint8_t>> incomingFrames_;
    mutable JPGuard xSemaphore_;
    Notifier roverNotifier_;
};

}  // JimmyPaputto

#endif  // JP_RTCM3_STORE_HPP_

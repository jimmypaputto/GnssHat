/*
 * Jimmy Paputto 2025
 */

#ifndef JP_RTCM3_PARSER_HPP_
#define JP_RTCM3_PARSER_HPP_

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "ublox/Rtcm3Store.hpp"


namespace JimmyPaputto
{

class Rtcm3Parser final
{
public:
    explicit Rtcm3Parser(Rtcm3Store& rtcm3Store);

    void parse(
        std::span<uint8_t> buffer,
        std::vector<uint8_t>& unfinishedFrame
    );

private:
    void extractFrames(
        std::span<uint8_t> buffer,
        std::vector<uint8_t>& unfinishedFrame
    );
    uint16_t getFrameId(const std::vector<uint8_t>& frame);
    bool checkFrame(const std::vector<uint8_t>& frame);

    constexpr static uint16_t maxNumberOfFrames_ = 30;
    std::array<std::vector<uint8_t>, maxNumberOfFrames_> frames_;
    std::array<std::vector<uint8_t>, maxNumberOfFrames_>::iterator endFrameIt_;
    Rtcm3Store& rtcm3Store_;
};

}  // JimmyPaputto

#endif  // JP_RTCM3_PARSER_HPP_

/*
 * Jimmy Paputto 2026
 */

#ifndef SUBFRAME_DATA_HPP_
#define SUBFRAME_DATA_HPP_

#include <cstdint>
#include <vector>

#include "SatelliteInfo.hpp"


namespace JimmyPaputto
{

struct SubframeData
{
    EGnssId gnssId;
    uint8_t svId;
    uint8_t sigId;
    uint8_t freqId;
    uint8_t numWords;
    uint8_t chn;
    uint8_t version;
    std::vector<uint32_t> words;     // Subframe payload (numWords x 32-bit)
};

struct SubframeBuffer
{
    std::vector<SubframeData> subframes;

    static constexpr size_t maxSubframes = 128;
};

}  // JimmyPaputto

#endif  // SUBFRAME_DATA_HPP_

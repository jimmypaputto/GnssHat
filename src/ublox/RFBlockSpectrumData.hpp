/*
 * Jimmy Paputto 2026
 */

#ifndef RF_BLOCK_SPECTRUM_DATA_HPP_
#define RF_BLOCK_SPECTRUM_DATA_HPP_

#include <cstdint>


namespace JimmyPaputto
{

struct RfBlockSpectrumData
{
    uint8_t id;
    std::vector<uint8_t> data;
    uint32_t span;
    uint32_t resolution;
    uint32_t centerFreq;
    uint8_t gain;
   
    static constexpr uint8_t maxNumberOfRfBlocks = 2;
};

}  // JimmyPaputto

#endif  // RF_BLOCK_SPECTRUM_DATA_HPP_

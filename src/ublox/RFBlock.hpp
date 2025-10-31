/*
 * Jimmy Paputto 2023
 */

#ifndef RF_BLOCK_HPP_
#define RF_BLOCK_HPP_

#include <cstdint>


namespace JimmyPaputto
{

enum class EBand : std::uint8_t
{
    L1     = 0x00,
    L2orL5 = 0x01
};

enum class EJammingState : std::uint8_t
{
    Unknown                           = 0x00,
    Ok_NoSignifantJamming             = 0x01,
    Warning_InferenceVisibleButFixOk  = 0x02,
    Critical_InferenceVisibleAndNoFix = 0x03
};

enum class EAntennaStatus : std::uint8_t
{
    Init     = 0x00,
    DontKnow = 0x01,
    Ok       = 0x02,
    Short    = 0x03,
    Open     = 0x04
};

enum class EAntennaPower : std::uint8_t
{
    Off      = 0x00,
    On       = 0x01,
    DontKnow = 0x02
};

struct RfBlock
{
    EBand id;
    EJammingState jammingState;
    EAntennaStatus antennaStatus;
    EAntennaPower antennaPower;
    uint32_t postStatus;
    uint16_t noisePerMS;
    float agcMonitor;
    float cwInterferenceSuppressionLevel;

    int8_t ofsI;
    uint8_t magI;
    int8_t ofsQ;
    uint8_t magQ;

    static constexpr uint8_t maxNumberOfRfBlocks = 2;
};

}  // JimmyPaputto

#endif  // RF_BLOCK_HPP_

/*
 * Jimmy Paputto 2026
 */

#ifndef TIMEMARK_HPP_
#define TIMEMARK_HPP_

#include <cstdint>


namespace JimmyPaputto
{

enum class ETimeMarkMode : uint8_t
{
    Single  = 0x00,
    Running = 0x01
};

enum class ETimeMarkRun : uint8_t
{
    Armed   = 0x00,
    Stopped = 0x01
};

enum class ETimeMarkTimeBase : uint8_t
{
    ReceiverTime = 0x00,
    GnssTime     = 0x01,
    UTC          = 0x02
};

enum class ETimeMarkTriggerEdge : uint8_t
{
    Rising  = 0x00,
    Falling = 0x01,
    Toggle  = 0x02
};

struct TimeMark
{
    uint8_t channel;
    ETimeMarkMode mode;
    ETimeMarkRun run;
    bool newFallingEdge;
    ETimeMarkTimeBase timeBase;
    bool utcAvailable;
    bool timeValid;
    bool newRisingEdge;
    uint16_t count;
    uint16_t weekNumberRising;
    uint16_t weekNumberFalling;
    uint32_t towRising_ms;
    uint32_t towSubRising_ns;
    uint32_t towFalling_ms;
    uint32_t towSubFalling_ns;
    uint32_t accuracyEstimate_ns;
};

}  // JimmyPaputto

#endif  // TIMEMARK_HPP_

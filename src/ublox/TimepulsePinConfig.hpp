/* 
 * Jimmy Paputto 2025
 */

#ifndef TIMEPULSE_PIN_CONFIG_HPP_
#define TIMEPULSE_PIN_CONFIG_HPP_

#include <cstdint>
#include <optional>


namespace JimmyPaputto
{

enum class ETimepulsePinPolarity : uint8_t
{
    FallingEdgeAtTopOfSecond = 0x0,
    RisingEdgeAtTopOfSecond  = 0x1
};

struct TimepulsePinConfig
{
    bool active;

    struct Pulse
    {
        uint32_t frequency;
        float pulseWidth;
    };
    Pulse fixedPulse;
    std::optional<Pulse> pulseWhenNoFix;
    ETimepulsePinPolarity polarity;
};

}  // JimmyPaputto

#endif  // TIMEPULSE_PIN_CONFIG_HPP_

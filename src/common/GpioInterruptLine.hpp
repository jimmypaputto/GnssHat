/*
 * Jimmy Paputto 2025
 */

#ifndef JIMMY_PAPUTTO_GPIO_INTERRUPT_LINE_HPP_
#define JIMMY_PAPUTTO_GPIO_INTERRUPT_LINE_HPP_

#include <cstdint>


struct gpiod_chip;
#if LIBGPIO_VERSION >= 2
struct gpiod_line_request;
struct gpiod_edge_event_buffer;
#else
struct gpiod_line;
#endif

namespace JimmyPaputto
{

class GpioInterruptLine
{
public:
    enum class Edge : uint8_t
    {
        Rising,
        Falling,
        Both
    };

    enum class EventType : uint8_t
    {
        Rising,
        Falling,
        Timeout,
        Error
    };

    GpioInterruptLine(unsigned int pin, Edge edge, const char* consumer);
    ~GpioInterruptLine();

    GpioInterruptLine(const GpioInterruptLine&) = delete;
    GpioInterruptLine& operator=(const GpioInterruptLine&) = delete;
    GpioInterruptLine(GpioInterruptLine&&) = delete;
    GpioInterruptLine& operator=(GpioInterruptLine&&) = delete;

    EventType waitEvent(int64_t timeoutNs = -1);

    int getValue() const;

private:
    unsigned int pin_;
    struct gpiod_chip* chip_ = nullptr;
#if LIBGPIO_VERSION >= 2
    struct gpiod_line_request* lineReq_ = nullptr;
    mutable struct gpiod_edge_event_buffer* eventBuffer_ = nullptr;
#else
    struct gpiod_line* line_ = nullptr;
#endif
};

}  // JimmyPaputto

#endif  // JIMMY_PAPUTTO_GPIO_INTERRUPT_LINE_HPP_

/*
 * Jimmy Paputto 2025
 */

#include "GpioInterruptLine.hpp"

#include <cstdio>
#include <exception>

#include <gpiod.h>

#include "Utils.hpp"


namespace JimmyPaputto
{

GpioInterruptLine::GpioInterruptLine(
    unsigned int pin, Edge edge, const char* consumer)
:   pin_(pin)
{
#if LIBGPIO_VERSION >= 2
    chip_ = gpiod_chip_open(CHIP_NAME);
    if (!chip_)
    {
        fprintf(
            stderr,
            "[GpioInterruptLine] Failed to open gpiochip %s\r\n",
            CHIP_NAME
        );
        std::terminate();
    }

    struct gpiod_line_settings* settings = gpiod_line_settings_new();
    if (!settings)
    {
        gpiod_chip_close(chip_);
        std::terminate();
    }

    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);

    gpiod_line_edge edgeType = GPIOD_LINE_EDGE_RISING;
    switch (edge)
    {
        case Edge::Rising:  edgeType = GPIOD_LINE_EDGE_RISING;  break;
        case Edge::Falling: edgeType = GPIOD_LINE_EDGE_FALLING; break;
        case Edge::Both:    edgeType = GPIOD_LINE_EDGE_BOTH;    break;
    }
    gpiod_line_settings_set_edge_detection(settings, edgeType);

    struct gpiod_line_config* config = gpiod_line_config_new();
    if (!config)
    {
        gpiod_line_settings_free(settings);
        gpiod_chip_close(chip_);
        std::terminate();
    }

    int rc = gpiod_line_config_add_line_settings(config, &pin_, 1, settings);
    if (rc)
    {
        fprintf(
            stderr,
            "[GpioInterruptLine] Failed to add settings for GPIO line %u\r\n",
            pin_
        );
        gpiod_line_settings_free(settings);
        gpiod_line_config_free(config);
        gpiod_chip_close(chip_);
        std::terminate();
    }

    struct gpiod_request_config* reqCfg = gpiod_request_config_new();
    if (reqCfg && consumer)
    {
        gpiod_request_config_set_consumer(reqCfg, consumer);
    }

    lineReq_ = gpiod_chip_request_lines(chip_, reqCfg, config);

    gpiod_request_config_free(reqCfg);
    gpiod_line_settings_free(settings);
    gpiod_line_config_free(config);

    if (!lineReq_)
    {
        fprintf(
            stderr,
            "[GpioInterruptLine] Failed to request GPIO line %u\r\n",
            pin_
        );
        gpiod_chip_close(chip_);
        std::terminate();
    }

    eventBuffer_ = gpiod_edge_event_buffer_new(1);
    if (!eventBuffer_)
    {
        gpiod_line_request_release(lineReq_);
        gpiod_chip_close(chip_);
        std::terminate();
    }
#else
    chip_ = gpiod_chip_open_by_name(CHIP_NAME);
    if (!chip_)
    {
        fprintf(
            stderr,
            "[GpioInterruptLine] Failed to open gpiochip %s\r\n",
            CHIP_NAME
        );
        std::terminate();
    }

    line_ = gpiod_chip_get_line(chip_, pin_);
    if (!line_)
    {
        fprintf(
            stderr,
            "[GpioInterruptLine] Failed to get GPIO line %u\r\n",
            pin_
        );
        gpiod_chip_close(chip_);
        std::terminate();
    }

    int ret = -1;
    switch (edge)
    {
        case Edge::Rising:
            ret = gpiod_line_request_rising_edge_events(line_, consumer);
            break;
        case Edge::Falling:
            ret = gpiod_line_request_falling_edge_events(line_, consumer);
            break;
        case Edge::Both:
            ret = gpiod_line_request_both_edges_events_flags(
                line_, consumer, 0);
            break;
    }

    if (ret < 0)
    {
        fprintf(
            stderr,
            "[GpioInterruptLine] Failed to request edge events on pin %u\r\n",
            pin_
        );
        gpiod_chip_close(chip_);
        std::terminate();
    }
#endif
}

GpioInterruptLine::~GpioInterruptLine()
{
#if LIBGPIO_VERSION >= 2
    if (eventBuffer_)
        gpiod_edge_event_buffer_free(eventBuffer_);
    if (lineReq_)
        gpiod_line_request_release(lineReq_);
#else
    if (line_)
        gpiod_line_release(line_);
#endif
    if (chip_)
        gpiod_chip_close(chip_);
}

GpioInterruptLine::EventType GpioInterruptLine::waitEvent(int64_t timeoutNs)
{
#if LIBGPIO_VERSION >= 2
    int ret = gpiod_line_request_wait_edge_events(lineReq_, timeoutNs);
    if (ret < 0)
        return EventType::Error;
    if (ret == 0)
        return EventType::Timeout;

    ret = gpiod_line_request_read_edge_events(lineReq_, eventBuffer_, 1);
    if (ret <= 0)
        return EventType::Error;

    struct gpiod_edge_event* event =
        gpiod_edge_event_buffer_get_event(eventBuffer_, 0);
    if (!event)
        return EventType::Error;

    auto type = gpiod_edge_event_get_event_type(event);
    if (type == GPIOD_EDGE_EVENT_RISING_EDGE)
        return EventType::Rising;
    if (type == GPIOD_EDGE_EVENT_FALLING_EDGE)
        return EventType::Falling;

    return EventType::Error;
#else
    const struct timespec* tsPtr = nullptr;
    struct timespec ts{};
    if (timeoutNs >= 0)
    {
        ts.tv_sec  = static_cast<time_t>(timeoutNs / 1'000'000'000);
        ts.tv_nsec = static_cast<long>(timeoutNs % 1'000'000'000);
        tsPtr = &ts;
    }

    int ret = gpiod_line_event_wait(line_, tsPtr);
    if (ret < 0)
        return EventType::Error;
    if (ret == 0)
        return EventType::Timeout;

    struct gpiod_line_event event;
    ret = gpiod_line_event_read(line_, &event);
    if (ret < 0)
        return EventType::Error;

    if (event.event_type == GPIOD_LINE_EVENT_RISING_EDGE)
        return EventType::Rising;
    if (event.event_type == GPIOD_LINE_EVENT_FALLING_EDGE)
        return EventType::Falling;

    return EventType::Error;
#endif
}

int GpioInterruptLine::getValue() const
{
#if LIBGPIO_VERSION >= 2
    auto val = gpiod_line_request_get_value(lineReq_, pin_);
    return val == GPIOD_LINE_VALUE_ACTIVE ? 1 : 0;
#else
    return gpiod_line_get_value(line_);
#endif
}

}  // JimmyPaputto

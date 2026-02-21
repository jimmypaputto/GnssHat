/*
 * Jimmy Paputto 2025
 */

#ifndef JIMMY_PAPUTTO_TIMEPULSE_HPP_
#define JIMMY_PAPUTTO_TIMEPULSE_HPP_

#include <cstdio>

#include <gpiod.h>

#include "common/Notifier.hpp"
#include "common/Utils.hpp"


#define TIMEPULSE_PIN 5

namespace JimmyPaputto
{

class Timepulse
{
public:
    explicit Timepulse(Notifier& notifier)
    :   notifier_(notifier)
    {
#if LIBGPIO_VERSION >= 2
        chip = gpiod_chip_open(CHIP_NAME);
        if (!chip)
        {
            fprintf(
                stderr,
                "[Timepulse] Failed to open gpiochip %s\r\n",
                CHIP_NAME
            );
        }

        unsigned int timepulsePin = TIMEPULSE_PIN;
        struct gpiod_line_config *config = gpiod_line_config_new();
        struct gpiod_line_settings *lineSettings = gpiod_line_config_get_line_settings(config, timepulsePin);
        
        if(!lineSettings)
        {
            lineSettings = gpiod_line_settings_new();
            if(!lineSettings)
            {
                fprintf(
                    stderr,
                    "[Timepulse] Failed to get line settings.\n"
                );
                std::terminate();
            }
        }
        int rc = gpiod_line_settings_set_direction(lineSettings, GPIOD_LINE_DIRECTION_INPUT);
        rc += gpiod_line_settings_set_edge_detection(lineSettings, gpiod_line_edge::GPIOD_LINE_EDGE_RISING);
        if(rc)
        {
            fprintf(
                stderr,
                "[Timepulse] Failed to set GPIO line %d\r\n",
                TIMEPULSE_PIN
            );
            gpiod_line_settings_free(lineSettings);
            gpiod_line_config_free(config);
            std::terminate();
        }

        rc = gpiod_line_config_add_line_settings(
            config,
            (unsigned int *) &timepulsePin,
            1,
            lineSettings
        );
        if(rc)
        {
            fprintf(
                stderr,
                "[Timepulse] Failed to add line settings to GPIO line %d\r\n",
                TIMEPULSE_PIN
            );
            gpiod_line_settings_free(lineSettings);
            gpiod_line_config_free(config);
            std::terminate();
        }

        lineReq = gpiod_chip_request_lines(chip, NULL, config);

        if(!lineReq)
        {
            fprintf(
                stderr,
                "[Timepulse] Failed to request GPIO line %d\r\n",
                TIMEPULSE_PIN
            );
        }

        gpiod_line_settings_free(lineSettings);
        gpiod_line_config_free(config);

#else
        chip = gpiod_chip_open_by_name(CHIP_NAME);
        if (!chip)
        {
            fprintf(
                stderr,
                "[Timepulse] Failed to open gpiochip %s\r\n",
                CHIP_NAME
            );
        }

        line = gpiod_chip_get_line(chip, TIMEPULSE_PIN);
        if (!line)
        {
            fprintf(
                stderr,
                "[Timepulse] Failed to get GPIO line %d\r\n",
                TIMEPULSE_PIN
            );
            gpiod_chip_close(chip);
        }

        int ret = gpiod_line_request_rising_edge_events(line, "ublox_timepulse");
        if (ret < 0)
        {
            fprintf(
                stderr,
                "[Timepulse] Failed to request rising edge events on pin %d\r\n",
                TIMEPULSE_PIN
            );
            gpiod_chip_close(chip);
        }
#endif
    }

    ~Timepulse()
    {
        threadRunning_ = false;
        if (interruptHandler_.joinable())
            interruptHandler_.join();
#if LIBGPIO_VERSION >= 2
        gpiod_line_request_release(lineReq);
#else
        gpiod_line_release(line);       
#endif
        gpiod_chip_close(chip);
    }

    void run()
    {
        interruptHandler_ = std::thread(&Timepulse::interruptHandler, this);
    }

private:
    void interruptHandler()
    {
        int ret;
#if LIBGPIO_VERSION >= 2
        struct gpiod_edge_event_buffer *eventBuffer = gpiod_edge_event_buffer_new(1);
#endif
        while (threadRunning_)
        {
#if LIBGPIO_VERSION >= 2
            int64_t timeout_ns = 0;
            ret = gpiod_line_request_wait_edge_events(lineReq, timeout_ns);
            if(ret < 0)
            {
                fprintf(stderr, "[Timepulse] Error waiting for event: %d\r\n", ret);
                break;
            }
            if(ret > 0)
            {
                ret = gpiod_line_request_read_edge_events(lineReq, eventBuffer, 1);
                if(ret <= 0)
                {
                    fprintf(stderr, "[Timepulse] Error reading events: %d\r\n", ret);
                    break;
                }
                struct gpiod_edge_event *event = gpiod_edge_event_buffer_get_event(eventBuffer, 0);
                if(!event)
                {
                    fprintf(stderr, "[Timepulse] Failed to read event from buffer: %d\r\n", ret);
                    break;
                }
                if(gpiod_edge_event_get_event_type(event) == gpiod_edge_event_type::GPIOD_EDGE_EVENT_RISING_EDGE)
                {
                    notifier_.notify();
                }
                else
                {
                    fprintf(stderr, "[Timepulse] Unrecognized event type: %d\r\n", ret);
                    break;
                }
                
            }
#else
            struct gpiod_line_event event;
            ret = gpiod_line_event_wait(line, NULL);
            if (ret < 0)
            {
                fprintf(stderr, "[Timepulse] Error waiting for event: %d\r\n", ret);
                break;
            }

            ret = gpiod_line_event_read(line, &event);
            if (ret < 0)
            {
                fprintf(stderr, "[Timepulse] Error reading event: %d\r\n", ret);
                break;
            }

            if (event.event_type == GPIOD_LINE_EVENT_RISING_EDGE)
            {
                notifier_.notify();
            }
#endif
        }
#if LIBGPIO_VERSION >= 2
        gpiod_edge_event_buffer_free(eventBuffer);
#endif
    }

    struct gpiod_chip *chip;
    struct gpiod_line *line;
    struct gpiod_line_request *lineReq;
    Notifier& notifier_;
    std::thread interruptHandler_;
    std::atomic<bool> threadRunning_{true};
};

}  // JimmyPaputto

#endif  // JIMMY_PAPUTTO_TIMEPULSE_HPP_

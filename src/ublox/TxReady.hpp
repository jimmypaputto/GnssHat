/*
 * Jimmy Paputto 2023
 */

#ifndef TX_READY_HPP_
#define TX_READY_HPP_

#include <cstdlib>
#include <stdexcept>
#include <thread>

#include <gpiod.h>

#include "common/Utils.hpp"

#define TX_READY_PIN 17

namespace JimmyPaputto
{

class TxReadyInterrupt
{
public:
    explicit TxReadyInterrupt(Notifier& notifier, const uint8_t navigationFrequency)
    :   notifier_(notifier)
    {
        timeoutNs_ = navigationFrequency > 2 ? 2'000'000'000 / navigationFrequency : 0;
        timeoutS_ = navigationFrequency > 2 ? 0 : 2 / navigationFrequency;
        int ret = 0;

#if LIBGPIO_VERSION >= 2
        chip = gpiod_chip_open(CHIP_NAME);
        if (!chip)
        {
            fprintf(stderr, "[TxReady] Failed to open gpiochip %s\r\n", CHIP_NAME);
            exit(EXIT_FAILURE);
        }

        unsigned int txReadyPin = TX_READY_PIN;
        struct gpiod_line_config *config = gpiod_line_config_new();
        struct gpiod_line_settings *lineSettings = gpiod_line_config_get_line_settings(config, txReadyPin);
              
        if(!lineSettings)
        {
            lineSettings = gpiod_line_settings_new();
            if(!lineSettings)
            {
                fprintf(
                    stderr,
                    "[TxReady] Failed to get line settings. Cannot continue.\n"
                );
                std::terminate();
            }
        }
        int rc = gpiod_line_settings_set_direction(lineSettings, GPIOD_LINE_DIRECTION_INPUT);
        rc += gpiod_line_settings_set_edge_detection(lineSettings, gpiod_line_edge::GPIOD_LINE_EDGE_BOTH);
        
        if(rc)
        {
            fprintf(
                stderr,
                "[TxReady] Failed to get GPIO line %d\r\n",
                TX_READY_PIN
            );
            gpiod_line_settings_free(lineSettings);
            gpiod_line_config_free(config);
        }

        rc = gpiod_line_config_add_line_settings(config, (unsigned int *) &txReadyPin, 1, lineSettings);
        if(rc)
        {
            fprintf(
                stderr,
                "[TxReady] Failed to add line settings on  GPIO line %d\r\n",
                TX_READY_PIN
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
                "[TxReady] Failed to request GPIO line %d\r\n",
                TX_READY_PIN
            );
        }

        gpiod_line_settings_free(lineSettings);
        gpiod_line_config_free(config);
#else
        chip = gpiod_chip_open_by_name(CHIP_NAME);
        if (!chip)
        {
            fprintf(stderr, "[TxReady] Failed to open gpiochip %s\r\n", CHIP_NAME);
            exit(EXIT_FAILURE);
        }

        line = gpiod_chip_get_line(chip, TX_READY_PIN);
        if (!line)
        {
            fprintf(stderr, "[TxReady] Failed to get GPIO line %d\r\n", TX_READY_PIN);
            exit(EXIT_FAILURE);
        }

        ret = gpiod_line_request_both_edges_events_flags(line, "ublox_txready", 0);
#endif
        if (ret < 0)
        {
            fprintf(stderr, "[TxReady] Failed to request both edge events on pin %d\r\n",
                TX_READY_PIN);
            exit(EXIT_FAILURE);
        }
    }

    ~TxReadyInterrupt()
    {
        interruptHandler_.request_stop();
        if (interruptHandler_.joinable())
            interruptHandler_.join();
#if LIBGPIO_VERSION >= 2
        gpiod_line_request_release(lineReq);
#else       
        gpiod_line_release(line);
        gpiod_chip_close(chip);
#endif
    }

    void run()
    {
        interruptHandler_ = std::jthread([this](std::stop_token stoken) {
            interruptHandler(stoken);
        });
    }

private:
    void interruptHandler(std::stop_token stoken)
    {
        const struct timespec ts = { timeoutS_, timeoutNs_ };
        int ret;
#if LIBGPIO_VERSION >= 2
        struct gpiod_edge_event_buffer *eventBuffer = gpiod_edge_event_buffer_new(1);
#endif
        while (!stoken.stop_requested())
        {
#if LIBGPIO_VERSION >= 2
            int64_t timeout_ns = ts.tv_sec *1000000000 + ts.tv_nsec;
            ret = gpiod_line_request_wait_edge_events(lineReq, timeout_ns);
            if(ret < 0)
            {
                fprintf(stderr, "[TxReady] Error waiting for event: %d\r\n", ret);
                break;
            }
            else if(ret == 0)
            {
                if (gpiod_line_request_get_value(lineReq, TX_READY_PIN) == GPIOD_LINE_VALUE_ACTIVE)
                {
                    notifier_.notify();
                    notifier_.setFlag(false);
                    continue;
                }
            }
            else if(ret > 0)
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
                else if(gpiod_edge_event_get_event_type(event) == gpiod_edge_event_type::GPIOD_EDGE_EVENT_FALLING_EDGE)
                {
                    notifier_.setFlag(true);
                }      
            }
#else
            struct gpiod_line_event event;
            int ret = gpiod_line_event_wait(line, &ts);
            if (ret == 0)
            {
                int lineValue = gpiod_line_get_value(line);
                if (lineValue == 1)
                {
                    notifier_.notify();
                    notifier_.setFlag(false);
                    continue;
                }
            }
            else if (ret < 0)
            {
                fprintf(stderr, "[TxReady] Error waiting for event: %d\r\n", ret);
                perror("gpiod_line_event_wait");
                exit(EXIT_FAILURE);
            }

            ret = gpiod_line_event_read(line, &event);
            if (ret < 0)
            {
                fprintf(stderr, "[TxReady] Error reading event: %d\r\n", ret);
                perror("gpiod_line_event_wait");
                exit(EXIT_FAILURE);
            }

            if (event.event_type == GPIOD_LINE_EVENT_RISING_EDGE)
            {
                notifier_.notify();
            }
            else if (event.event_type == GPIOD_LINE_EVENT_FALLING_EDGE)
            {
                notifier_.setFlag(true);
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
    uint32_t timeoutNs_;
    uint32_t timeoutS_;
    std::jthread interruptHandler_;
};

}  // JimmyPaputto

#endif  // TX_READY_HPP_

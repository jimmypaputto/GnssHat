/*
 * Jimmy Paputto 2023
 */

#ifndef TX_READY_HPP_
#define TX_READY_HPP_

#include <cstdio>
#include <cstdlib>
#include <thread>

#include "common/GpioInterruptLine.hpp"
#include "common/Notifier.hpp"

#define TX_READY_PIN 17

namespace JimmyPaputto
{

class TxReadyInterrupt
{
public:
    explicit TxReadyInterrupt(Notifier& notifier, const uint8_t navigationFrequency)
    :   notifier_(notifier),
        gpioLine_(TX_READY_PIN,
            GpioInterruptLine::Edge::Both, "ublox_txready"),
        timeoutNs_(navigationFrequency > 0
            ? 2'000'000'000LL / navigationFrequency : 2'000'000'000LL)
    {
    }

    ~TxReadyInterrupt()
    {
        interruptHandler_.request_stop();
        if (interruptHandler_.joinable())
            interruptHandler_.join();
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
        while (!stoken.stop_requested())
        {
            auto event = gpioLine_.waitEvent(timeoutNs_);

            if (event == GpioInterruptLine::EventType::Timeout)
            {
                if (gpioLine_.getValue() == 1)
                {
                    notifier_.notify();
                    notifier_.setFlag(false);
                }
                continue;
            }

            if (event == GpioInterruptLine::EventType::Rising)
            {
                notifier_.notify();
            }
            else if (event == GpioInterruptLine::EventType::Falling)
            {
                notifier_.setFlag(true);
            }
            else if (event == GpioInterruptLine::EventType::Error)
            {
                fprintf(stderr, "[TxReady] GPIO event error\r\n");
            }
        }
    }

    Notifier& notifier_;
    GpioInterruptLine gpioLine_;
    int64_t timeoutNs_;
    std::jthread interruptHandler_;
};

}  // JimmyPaputto

#endif  // TX_READY_HPP_

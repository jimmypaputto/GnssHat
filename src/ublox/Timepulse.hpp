/*
 * Jimmy Paputto 2025
 */

#ifndef JIMMY_PAPUTTO_TIMEPULSE_HPP_
#define JIMMY_PAPUTTO_TIMEPULSE_HPP_

#include <cstdio>
#include <thread>

#include "common/GpioInterruptLine.hpp"
#include "common/Notifier.hpp"


#define TIMEPULSE_PIN 5

namespace JimmyPaputto
{

class Timepulse
{
public:
    explicit Timepulse(Notifier& notifier)
    :   notifier_(notifier),
        gpioLine_(TIMEPULSE_PIN,
            GpioInterruptLine::Edge::Rising, "ublox_timepulse")
    {
    }

    ~Timepulse()
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
            auto event = gpioLine_.waitEvent(0);
            if (event == GpioInterruptLine::EventType::Rising)
            {
                notifier_.notify();
            }
            else if (event == GpioInterruptLine::EventType::Error)
            {
                fprintf(stderr, "[Timepulse] GPIO event error\r\n");
            }
        }
    }

    Notifier& notifier_;
    GpioInterruptLine gpioLine_;
    std::jthread interruptHandler_;
};

}  // JimmyPaputto

#endif  // JIMMY_PAPUTTO_TIMEPULSE_HPP_

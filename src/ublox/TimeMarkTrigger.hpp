/*
 * Jimmy Paputto 2026
 */

#ifndef JIMMY_PAPUTTO_TIMEMARK_TRIGGER_HPP_
#define JIMMY_PAPUTTO_TIMEMARK_TRIGGER_HPP_

#include <cstdio>

#include "common/Utils.hpp"


#define EXTINT_PIN 17

namespace JimmyPaputto
{

class TimeMarkTrigger
{
public:
    explicit TimeMarkTrigger()
    :   state_(getGpio(CHIP_NAME, EXTINT_PIN) == 1)
    {
    }

    ~TimeMarkTrigger() = default;

    TimeMarkTrigger(const TimeMarkTrigger&) = delete;
    TimeMarkTrigger& operator=(const TimeMarkTrigger&) = delete;
    TimeMarkTrigger(TimeMarkTrigger&&) = delete;
    TimeMarkTrigger& operator=(TimeMarkTrigger&&) = delete;

    void raise()
    {
        setGpio(CHIP_NAME, EXTINT_PIN, 1);
        state_ = true;
    }

    void fall()
    {
        setGpio(CHIP_NAME, EXTINT_PIN, 0);
        state_ = false;
    }

    void toggle()
    {
        state_ = !state_;
        setGpio(CHIP_NAME, EXTINT_PIN, state_ ? 1 : 0);
    }

private:
    bool state_;
};

}  // JimmyPaputto

#endif  // JIMMY_PAPUTTO_TIMEMARK_TRIGGER_HPP_

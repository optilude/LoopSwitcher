#pragma once

#include <stdint.h>

class Clock {
public:
    virtual void delayMs(uint32_t ms) = 0;
    virtual ~Clock() = default;
};

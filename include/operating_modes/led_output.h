#pragma once

#include <stdint.h>

class LedOutput {
public:
    // Bit n = LED n lit. Returns false if the write failed.
    virtual bool setLeds(uint8_t mask) = 0;
    virtual ~LedOutput() = default;
};

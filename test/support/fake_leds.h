#pragma once

#include <stdint.h>

#include "operating_modes/led_output.h"

class FakeLeds : public LedOutput {
public:
    uint8_t lastMask = 0;
    int writes = 0;
    bool failing = false;

    bool setLeds(uint8_t mask) override {
        ++writes;
        if (failing) return false;
        lastMask = mask;
        return true;
    }
};

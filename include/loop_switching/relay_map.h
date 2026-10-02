#pragma once

#include <stdint.h>

constexpr uint8_t kLoopCount = 8;

// Position of a loop's relay coils on MCP23017 U3 (port 0 = A, 1 = B).
struct CoilPin {
    uint8_t port;
    uint8_t rstBit;
    uint8_t setBit;
};

struct RelayDrive {
    uint8_t setMask;  // bit n: pulse SET on loop n
    uint8_t rstMask;  // bit n: pulse RST on loop n
};

constexpr CoilPin coilPin(uint8_t loop) {
    return CoilPin{static_cast<uint8_t>(loop / 4), static_cast<uint8_t>(2 * (loop % 4)),
                   static_cast<uint8_t>(2 * (loop % 4) + 1)};
}

constexpr RelayDrive computeDrive(uint8_t current, uint8_t target) {
    const uint8_t changed = static_cast<uint8_t>(current ^ target);
    return RelayDrive{static_cast<uint8_t>(changed & target),
                      static_cast<uint8_t>(changed & ~target)};
}

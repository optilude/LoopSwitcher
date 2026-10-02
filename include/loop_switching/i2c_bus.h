#pragma once

#include <stdint.h>

class I2cBus {
public:
    // Sends `length` bytes to `address` as one transfer; returns false if it was not acknowledged.
    virtual bool write(uint8_t address, const uint8_t* data, uint8_t length) = 0;
    // Sends `outLength` bytes (typically a register number), then reads `inLength` bytes after a
    // repeated start. Returns false if either part was not acknowledged.
    virtual bool writeRead(uint8_t address, const uint8_t* out, uint8_t outLength, uint8_t* in, uint8_t inLength) = 0;
    virtual ~I2cBus() = default;
};

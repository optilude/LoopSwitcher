#pragma once

#include <stdint.h>

#include <vector>

#include "loop_switching/i2c_bus.h"

struct Transfer {
    uint8_t address;
    std::vector<uint8_t> bytes;
};

// Records every write; can be told to fail the nth write (1-based).
class FakeI2cBus : public I2cBus {
public:
    std::vector<Transfer> transfers;
    int failOnCall = 0;

    bool write(uint8_t address, const uint8_t* data, uint8_t length) override {
        transfers.push_back({address, std::vector<uint8_t>(data, data + length)});
        return static_cast<int>(transfers.size()) != failOnCall;
    }

    bool writeRead(uint8_t, const uint8_t*, uint8_t, uint8_t*, uint8_t) override { return false; }  // not needed here
};

#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "loop_switching/clock.h"
#include "loop_switching/i2c_bus.h"

class WireI2cBus : public I2cBus {
public:
    void begin(uint32_t clockHz = 400000) {
        Wire.begin();
        Wire.setClock(clockHz);
    }

    bool write(uint8_t address, const uint8_t* data, uint8_t length) override {
        Wire.beginTransmission(address);
        Wire.write(data, length);
        return Wire.endTransmission() == 0;
    }

    bool writeRead(uint8_t address, const uint8_t* out, uint8_t outLength, uint8_t* in, uint8_t inLength) override {
        Wire.beginTransmission(address);
        Wire.write(out, outLength);
        if (Wire.endTransmission(false) != 0) return false;  // keep the bus for the repeated start
        if (Wire.requestFrom(address, inLength) != inLength) return false;
        for (uint8_t i = 0; i < inLength; ++i) in[i] = static_cast<uint8_t>(Wire.read());
        return true;
    }
};

class ArduinoClock : public Clock {
public:
    void delayMs(uint32_t ms) override { delay(ms); }
};

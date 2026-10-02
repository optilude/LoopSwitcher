#pragma once

#include <stdint.h>

#include <vector>

#include "fake_i2c_bus.h"
#include "loop_switching/i2c_bus.h"

// A model of one MCP23017 (IOCON.BANK = 0, sequential addressing): a write sets the register
// pointer and stores the following bytes; reads return registers. The GPIO registers read the
// external pin levels (inverted where IPOL says so) for inputs and the latch for outputs.
class FakeMcpBus : public I2cBus {
public:
    explicit FakeMcpBus(uint8_t address) : address_(address) { regs_[0x00] = regs_[0x01] = 0xFF; }

    std::vector<Transfer> writes;  // every write, in order
    int reads = 0;
    int failOnCall = 0;            // fail the nth transfer (writes and reads together), 1-based
    uint8_t pinsA = 0xFF;          // external levels; 0xFF = every pin pulled up, nothing pressed
    uint8_t pinsB = 0xFF;

    bool write(uint8_t address, const uint8_t* data, uint8_t length) override {
        if (!accept(address)) return false;
        writes.push_back({address, std::vector<uint8_t>(data, data + length)});
        if (length == 0) return true;
        pointer_ = data[0];
        for (uint8_t i = 1; i < length; ++i) store(pointer_++, data[i]);
        return true;
    }

    bool writeRead(uint8_t address, const uint8_t* out, uint8_t outLength, uint8_t* in, uint8_t inLength) override {
        if (!accept(address)) return false;
        ++reads;
        pointer_ = outLength > 0 ? out[0] : pointer_;
        for (uint8_t i = 0; i < inLength; ++i) in[i] = load(pointer_++);
        return true;
    }

    uint8_t reg(uint8_t r) const { return regs_[r]; }
    int calls() const { return calls_; }
    void failNext() { failOnCall = calls_ + 1; }

    static constexpr uint8_t kIodirA = 0x00, kIodirB = 0x01, kIpolA = 0x02, kGpintenA = 0x04, kIntconA = 0x08,
                             kGppuA = 0x0C, kGpioA = 0x12, kGpioB = 0x13, kOlatA = 0x14, kOlatB = 0x15;

private:
    bool accept(uint8_t address) {
        ++calls_;
        return address == address_ && calls_ != failOnCall;
    }
    void store(uint8_t r, uint8_t value) {
        if (r < sizeof(regs_)) regs_[r] = value;
    }
    uint8_t load(uint8_t r) const {
        if (r == kGpioA) return static_cast<uint8_t>((regs_[kIodirA] & (pinsA ^ regs_[kIpolA])) | (~regs_[kIodirA] & regs_[kOlatA]));
        if (r == kGpioB) return static_cast<uint8_t>((regs_[kIodirB] & pinsB) | (~regs_[kIodirB] & regs_[kOlatB]));
        return r < sizeof(regs_) ? regs_[r] : 0xFF;
    }

    uint8_t address_;
    uint8_t regs_[0x16] = {};
    uint8_t pointer_ = 0;
    int calls_ = 0;
};

#pragma once

#include <stdint.h>

#include "loop_switching/i2c_bus.h"
#include "loop_switching/mcp23017.h"
#include "operating_modes/led_output.h"

// The footswitches and LEDs on MCP23017 U2. Loop n corresponds to footswitch SW(n+1) on GPA(7-n)
// and LED(n+1) on GPB(7-n); masks use bit n for loop n. A pressed switch reads high because the
// input polarity is inverted, and a LED is lit by driving its pin high.
class ControlExpander : public LedOutput {
public:
    explicit ControlExpander(I2cBus& bus, uint8_t address = kControlExpanderAddress)
        : bus_(bus), address_(address) {}

    // Switches the LEDs off, then makes GPA inputs with pull-ups and interrupt-on-change and GPB
    // outputs. Stops at the first bus error and returns false.
    bool begin();

    bool setLeds(uint8_t mask) override;

    // Bit n of `pressed` is set while footswitch n is down. Reading also clears the expander's
    // interrupt. Returns false, leaving `pressed` alone, on a bus error.
    bool readSwitches(uint8_t& pressed);

private:
    bool writeRegisters(uint8_t firstRegister, uint8_t first, uint8_t second, uint8_t count);

    I2cBus& bus_;
    uint8_t address_;
};

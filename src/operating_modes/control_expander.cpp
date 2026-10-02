#include "operating_modes/control_expander.h"

namespace {

// Loop n sits on pin 7-n, so the pin byte is the loop mask with its bits reversed.
uint8_t reverseBits(uint8_t value) {
    uint8_t out = 0;
    for (uint8_t i = 0; i < 8; ++i) {
        if (value & (1u << i)) out |= static_cast<uint8_t>(1u << (7 - i));
    }
    return out;
}

}  // namespace

bool ControlExpander::writeRegisters(uint8_t firstRegister, uint8_t first, uint8_t second, uint8_t count) {
    const uint8_t bytes[3] = {firstRegister, first, second};
    return bus_.write(address_, bytes, static_cast<uint8_t>(count + 1));
}

bool ControlExpander::begin() {
    if (!writeRegisters(kMcpOlatB, 0x00, 0, 1)) return false;                // LEDs off first
    if (!writeRegisters(kMcpIodirA, 0xFF, 0x00, 2)) return false;            // GPA in, GPB out
    if (!writeRegisters(kMcpIpolA, 0xFF, 0, 1)) return false;                // pressed reads 1
    if (!writeRegisters(kMcpGppuA, 0xFF, 0, 1)) return false;                // switches pull to ground
    if (!writeRegisters(kMcpIntconA, 0x00, 0, 1)) return false;              // interrupt on any change
    if (!writeRegisters(kMcpGpintenA, 0xFF, 0, 1)) return false;
    uint8_t pending = 0;
    return readSwitches(pending);  // clears a stale interrupt
}

bool ControlExpander::setLeds(uint8_t mask) { return writeRegisters(kMcpOlatB, reverseBits(mask), 0, 1); }

bool ControlExpander::readSwitches(uint8_t& pressed) {
    const uint8_t reg = kMcpGpioA;
    uint8_t raw = 0;
    if (!bus_.writeRead(address_, &reg, 1, &raw, 1)) return false;
    pressed = reverseBits(raw);
    return true;
}

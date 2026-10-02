#pragma once

#include <stdint.h>

#include "loop_switching/i2c_bus.h"
#include "loop_switching/mcp23017.h"
#include "loop_switching/relay_port.h"

// Drives the eight latching relays through the MCP23017 U3. A coil is energized by driving its
// pin high; both ports are written in one transfer.
class Mcp23017RelayPort : public RelayPort {
public:
    explicit Mcp23017RelayPort(I2cBus& bus, uint8_t address = kRelayExpanderAddress)
        : bus_(bus), address_(address) {}

    // Clears the output latches first, then makes every pin an output, so no coil can be
    // energized by a stale latch value after a reset. Returns false on a bus error.
    bool begin();

    // Refuses (returns false, writes nothing) a drive that has SET and RST on the same loop.
    bool energize(RelayDrive drive) override;
    bool releaseAll() override;

private:
    bool writePair(uint8_t firstRegister, uint8_t portA, uint8_t portB);

    I2cBus& bus_;
    uint8_t address_;
};

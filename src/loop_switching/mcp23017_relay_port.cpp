#include "loop_switching/mcp23017_relay_port.h"

bool Mcp23017RelayPort::writePair(uint8_t firstRegister, uint8_t portA, uint8_t portB) {
    const uint8_t bytes[3] = {firstRegister, portA, portB};
    return bus_.write(address_, bytes, sizeof(bytes));
}

bool Mcp23017RelayPort::begin() {
    if (!writePair(kMcpOlatA, 0x00, 0x00)) return false;
    return writePair(kMcpIodirA, 0x00, 0x00);
}

bool Mcp23017RelayPort::energize(RelayDrive drive) {
    if (drive.setMask & drive.rstMask) return false;

    uint8_t port[2] = {0, 0};
    for (uint8_t loop = 0; loop < kLoopCount; ++loop) {
        const uint8_t bit = static_cast<uint8_t>(1u << loop);
        const CoilPin pin = coilPin(loop);
        if (drive.setMask & bit) port[pin.port] |= static_cast<uint8_t>(1u << pin.setBit);
        if (drive.rstMask & bit) port[pin.port] |= static_cast<uint8_t>(1u << pin.rstBit);
    }
    if (port[0] == 0 && port[1] == 0) return true;
    return writePair(kMcpOlatA, port[0], port[1]);
}

bool Mcp23017RelayPort::releaseAll() { return writePair(kMcpOlatA, 0x00, 0x00); }

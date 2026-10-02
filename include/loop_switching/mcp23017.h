#pragma once

#include <stdint.h>

// MCP23017 registers with IOCON.BANK = 0 (the power-up default): ports A and B alternate, and
// a write of several bytes continues into the next register.
constexpr uint8_t kMcpIodirA = 0x00;
constexpr uint8_t kMcpIpolA = 0x02;
constexpr uint8_t kMcpGpintenA = 0x04;
constexpr uint8_t kMcpIntconA = 0x08;
constexpr uint8_t kMcpGppuA = 0x0C;
constexpr uint8_t kMcpGpioA = 0x12;
constexpr uint8_t kMcpOlatA = 0x14;
constexpr uint8_t kMcpOlatB = 0x15;

// U2, the footswitch and LED expander on the audio PCB: A0 = A1 = A2 = GND.
constexpr uint8_t kControlExpanderAddress = 0x20;

// U3, the relay driver on the audio PCB: A0 = +5V, A1 = A2 = GND.
constexpr uint8_t kRelayExpanderAddress = 0x21;

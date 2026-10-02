#pragma once

#include <stdint.h>

#include "loop_switching/clock.h"
#include "loop_switching/relay_map.h"
#include "loop_switching/relay_port.h"

constexpr uint32_t kRelayPulseMs = 10;   // datasheet maximum set/reset time is 3 ms
constexpr uint32_t kRelayGroupGapMs = 5;
// Raise only after measuring the 5 V rail with that many relays pulsing together.
constexpr uint8_t kMaxSimultaneousRelays = 4;

class LoopSwitching {
public:
    LoopSwitching(RelayPort& port, Clock& clock) : port_(port), clock_(clock) {}

    // Latching relays keep their position through power loss, so every relay is pulsed.
    // On failure the tracked state is left as it was.
    bool begin(uint8_t initialMask);

    // bit n of `mask` = loop n engaged. Returns false if a bus write failed; loops in
    // groups that completed before the failure keep their new state.
    bool apply(uint8_t mask);

    uint8_t stateMask() const { return state_; }

private:
    RelayPort& port_;
    Clock& clock_;
    uint8_t state_ = 0;
};

#pragma once

#include <stdint.h>

#include "operating_modes/control_expander.h"

constexpr uint32_t kFootswitchLockoutMs = 30;
constexpr uint32_t kFootswitchPollMs = 50;

// Turns footswitch levels into presses. A press is reported at the moment the switch is first seen
// down; after any accepted change the same switch is ignored for kFootswitchLockoutMs to ride out
// contact bounce. A change that arrives inside the lockout is not lost: it is re-checked as soon
// as the lockout ends. The expander is read when INT_SWITCH fired, and at least every
// kFootswitchPollMs in case an interrupt was missed.
class FootswitchInput {
public:
    explicit FootswitchInput(ControlExpander& expander) : expander_(expander) {}

    // Takes the current switch state as the starting point, so a switch held at power-up is not a
    // press. Returns false on a bus error.
    bool begin(uint32_t nowMs);

    // `interrupt` is true when INT_SWITCH fired since the last call. Returns the presses accepted
    // by this call, bit n for footswitch n.
    uint8_t poll(bool interrupt, uint32_t nowMs);

    // Releases accepted by the latest poll() call, bit n for footswitch n.
    uint8_t releases() const { return releases_; }

    uint32_t readFailures() const { return readFailures_; }

private:
    ControlExpander& expander_;
    uint8_t stable_ = 0;  // accepted state, bit n set while footswitch n is down
    uint8_t locked_ = 0;  // switches inside their lockout
    uint8_t releases_ = 0;
    uint32_t lockedUntilMs_[8] = {};
    bool recheck_ = false;
    uint32_t recheckAtMs_ = 0;
    uint32_t lastReadMs_ = 0;
    uint32_t readFailures_ = 0;
};

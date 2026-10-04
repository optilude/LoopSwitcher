#include "operating_modes/footswitch_input.h"

namespace {
bool reached(uint32_t nowMs, uint32_t atMs) { return static_cast<int32_t>(nowMs - atMs) >= 0; }
}  // namespace

bool FootswitchInput::begin(uint32_t nowMs) {
    uint8_t raw = 0;
    if (!expander_.readSwitches(raw)) return false;
    stable_ = raw;
    locked_ = 0;
    recheck_ = false;
    lastReadMs_ = nowMs;
    readFailures_ = 0;
    return true;
}

uint8_t FootswitchInput::poll(bool interrupt, uint32_t nowMs) {
    releases_ = 0;

    // Clear expired lockouts on every call, so a stale one can never look like the future once the
    // millisecond counter has run for 24 days.
    for (uint8_t i = 0; i < 8; ++i) {
        if ((locked_ & (1u << i)) && reached(nowMs, lockedUntilMs_[i])) locked_ &= static_cast<uint8_t>(~(1u << i));
    }

    const bool due = interrupt || nowMs - lastReadMs_ >= kFootswitchPollMs || (recheck_ && reached(nowMs, recheckAtMs_));
    if (!due) return 0;
    lastReadMs_ = nowMs;

    uint8_t raw = 0;
    if (!expander_.readSwitches(raw)) {
        ++readFailures_;
        return 0;
    }

    recheck_ = false;
    uint8_t presses = 0;
    const uint8_t changed = static_cast<uint8_t>(raw ^ stable_);
    for (uint8_t i = 0; i < 8; ++i) {
        const uint8_t bit = static_cast<uint8_t>(1u << i);
        if (!(changed & bit)) continue;

        if (locked_ & bit) {
            // Look again the moment this switch's lockout ends.
            if (!recheck_ || static_cast<int32_t>(lockedUntilMs_[i] - recheckAtMs_) < 0) recheckAtMs_ = lockedUntilMs_[i];
            recheck_ = true;
            continue;
        }

        stable_ ^= bit;
        locked_ |= bit;
        lockedUntilMs_[i] = nowMs + kFootswitchLockoutMs;
        if (raw & bit) {
            presses |= bit;
        } else {
            releases_ |= bit;
        }
    }
    return presses;
}

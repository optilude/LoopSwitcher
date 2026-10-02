#pragma once

#include <stdint.h>

struct SavedState {
    uint8_t loopMask;      // bit n = loop n engaged
    bool presetMode;
    uint8_t activePreset;  // 0-7
};

inline bool operator==(const SavedState& a, const SavedState& b) {
    return a.loopMask == b.loopMask && a.presetMode == b.presetMode && a.activePreset == b.activePreset;
}
inline bool operator!=(const SavedState& a, const SavedState& b) { return !(a == b); }

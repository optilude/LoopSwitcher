#pragma once

#include <stdint.h>

struct SavedState {
    uint8_t loopMask;      // bit n = loop n engaged
    bool presetMode;
    uint8_t activePreset;  // 0-7
    bool performMode = false;  // Perform mode; never set together with presetMode
};

inline bool operator==(const SavedState& a, const SavedState& b) {
    return a.loopMask == b.loopMask && a.presetMode == b.presetMode && a.activePreset == b.activePreset &&
           a.performMode == b.performMode;
}
inline bool operator!=(const SavedState& a, const SavedState& b) { return !(a == b); }

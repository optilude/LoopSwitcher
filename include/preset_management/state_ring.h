#pragma once

#include <stdint.h>

#include "preset_management/eeprom.h"
#include "preset_management/layout.h"
#include "preset_management/saved_state.h"

// A ring of kRingRecords 4-byte records, so every save lands in the next cell group and each
// cell is written once per kRingRecords saves. A record is [seq, loopMask, modePreset, check].
class StateRing {
public:
    explicit StateRing(Eeprom& eeprom) : eeprom_(eeprom) {}

    // Finds the newest valid record; call once at boot before load() or append().
    void scan();
    // Returns false and leaves `out` untouched when no valid record exists.
    bool load(SavedState& out) const;
    void append(const SavedState& state);

private:
    bool readRecord(uint8_t slot, uint8_t& seq, SavedState& state) const;

    Eeprom& eeprom_;
    bool hasRecord_ = false;
    uint8_t newestSlot_ = 0;
    uint8_t newestSeq_ = 0;
    SavedState newest_{};
};

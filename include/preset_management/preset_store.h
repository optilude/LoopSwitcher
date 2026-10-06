#pragma once

#include <stdint.h>

#include "preset_management/eeprom.h"
#include "preset_management/layout.h"
#include "preset_management/saved_state.h"
#include "preset_management/state_ring.h"

constexpr uint32_t kIdleSaveMs = 2000;

struct PresetMidi {
    uint8_t channel = 0;  // 0-15, displayed as MIDI channels 1-16
    bool bankSelectEnabled = false;
    bool programChangeEnabled = false;
    bool effectCcEnabled = false;
    uint8_t bankMsb = 0;
    uint8_t bankLsb = 0;
    uint8_t program = 0;
    uint8_t effectCc = 0;
    uint8_t effectValue = 0;
};

class PresetStore {
public:
    explicit PresetStore(Eeprom& eeprom) : eeprom_(eeprom), ring_(eeprom) {}

    // Validates the stored configuration. When it is blank or damaged the store is reset to
    // defaults (all labels default, all presets empty) and false is returned.
    bool begin();

    // `out` receives the label, or the default "Loop n" when none is set; empty for a bad index.
    void loopLabel(uint8_t loop, char out[kNameMax + 1]) const;
    // Printable ASCII, at most kNameMax characters, trailing spaces trimmed. An empty result
    // restores the default name. Returns false and changes nothing for invalid input.
    bool setLoopLabel(uint8_t loop, const char* name);
    // True when the loop has its own label, false when it shows the default name.
    bool hasLoopLabel(uint8_t loop) const;

    // A slot is empty or holds a name (1 to kNameMax characters) and a loop mask (bit n = loop n).
    // Name rules are the same as for labels, except that a preset name may not be empty.
    bool presetUsed(uint8_t slot) const;
    void presetName(uint8_t slot, char out[kNameMax + 1]) const;  // empty for an empty or bad slot
    uint8_t presetMask(uint8_t slot) const;                         // 0 for an empty or bad slot
    // Creates or overwrites a preset.
    bool savePreset(uint8_t slot, const char* name, uint8_t mask);
    // These three fail on an empty slot as well as on bad input.
    bool renamePreset(uint8_t slot, const char* name);
    bool setPresetMask(uint8_t slot, uint8_t mask);
    bool deletePreset(uint8_t slot);

    PresetMidi presetMidi(uint8_t slot) const;
    bool setPresetMidi(uint8_t slot, const PresetMidi& midi);

    // The last saved state, or all loops bypassed, manual mode, preset 0 when none was saved.
    // A state passed to setState() but not yet saved is not included.
    SavedState savedState() const;

    // Records the current state; it is written kIdleSaveMs after the last change, and only if it
    // differs from the last saved state. Call tick() regularly with the current time.
    void setState(const SavedState& state, uint32_t nowMs);
    void tick(uint32_t nowMs);
    // Writes a pending state immediately.
    void flush();

private:
    uint16_t computeConfigCrc() const;
    void writeConfigCrc();
    void resetConfiguration();

    Eeprom& eeprom_;
    StateRing ring_;
    SavedState lastSaved_{0, false, 0};
    SavedState pending_{0, false, 0};
    bool hasPending_ = false;
    uint32_t changedAtMs_ = 0;
};

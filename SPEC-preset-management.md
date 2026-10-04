# Spec: preset-management

Module of [CAPABILITY-MAP.md](CAPABILITY-MAP.md). Depends on: `loop-switching` (loop mask format only). Consumed by: `operating-modes`.

## Objective

Own all user data that must survive power-off: eight loop labels, eight fixed preset slots (name plus loop combination), and the last operating state. Provide a small, UI-agnostic API to read, create, rename, modify and delete this data, and a wear-aware way to save the last state.

User: a guitarist who names loops ("TS808"), builds up to eight presets, and expects the unit to come back exactly as it was left.

Success: data survives power cycles; corrupted or never-initialized storage is detected and replaced with defaults; frequent footswitch use does not wear out the EEPROM.

## Behavior Requirements

1. **Loop labels:** 8 labels, 0 to 10 characters each. An empty label means "use the default name" `Loop n` (1-based); the default is generated, not stored.
2. **Presets:** 8 fixed slots. A slot is either empty or holds a name (1 to 10 characters) and a loop mask (`uint8_t`, bit n = loop n engaged, same as `loop-switching`).
3. **Operations:** set loop label; save preset (create or overwrite name and mask); rename preset; set preset mask; delete preset (empties the slot). Operating on an empty slot is allowed only for save (create); rename and set-mask on an empty slot fail.
4. **Name rules:** printable ASCII (0x20 to 0x7E), at most 10 characters, trailing spaces trimmed. A preset name that is empty after trimming is rejected. Invalid input returns false and changes nothing. (The UI restricts the character set further; this layer only enforces storage-safe input.)
5. **Last state:** `SavedState { loopMask, presetMode, activePreset }`. Saved with an idle delay `kIdleSaveMs` = 2000 ms after the last change, only when it differs from the last saved state. `flush()` saves immediately.
6. **Wear levelling:** state records rotate through a ring of 20 slots in EEPROM, so each cell is written once per 20 saves.
7. **Integrity:** the configuration region (labels and presets) is protected by a version byte and CRC-16. If either is invalid at `begin()`, the store resets to defaults (labels empty, all presets empty) and `begin()` returns false so the caller can tell the user.
8. **State records** carry a checksum; an invalid or erased record is ignored. With no valid record, `savedState()` returns all loops bypassed, manual mode, preset 0.
9. **Minimal writes:** only bytes that actually change are written (`EEPROM.update` semantics).
10. All storage access goes through an `Eeprom` interface so logic runs in native tests.

## EEPROM Layout (256 bytes, ATmega4809)

| Offset | Size | Content |
|---|---|---|
| 0 | 1 | Layout version (`0x01`) |
| 1 | 2 | CRC-16/CCITT-FALSE of bytes 3 to 170 |
| 3 | 80 | 8 loop labels, 10 bytes each, NUL-padded |
| 83 | 88 | 8 presets: 10-byte name (NUL-padded) + 1-byte mask; empty slot = first name byte `0x00` |
| 171 | 5 | Reserved |
| 176 | 80 | State ring: 20 records of 4 bytes |

State record: `[seq, loopMask, modePreset, check]`, where `modePreset` bit 7 = preset mode, bit 6 = perform mode (never together with bit 7) and bits 0 to 2 = active preset, and `check = seq ^ loopMask ^ modePreset ^ 0xA5`. The newest valid record is the one that is ahead of the others in modulo-256 sequence order (`(a - b) mod 256` in 1 to 127). The next record is written to the slot after the newest, with `seq + 1`. Erased cells (`0xFF`) fail the check by construction.

## Tech Stack

- C++17, Arduino framework, PlatformIO, board `nano_every`
- `EEPROM.h` from the Arduino megaAVR core (no extra library); `EEPROM.length()` verified to be 256 on hardware

## Commands

```
Build:       pio run -e nano_every
Unit tests:  pio test -e native
Format:      clang-format -i src/**/*.cpp src/**/*.h include/**/*.h
```

## Project Structure

```
src/preset_management/   → PresetStore, CRC, state ring, ArduinoEeprom
include/preset_management/ → public headers (PresetStore, Eeprom, SavedState)
test/test_preset_management/ → native tests with FakeEeprom
```

## Code Style

```cpp
constexpr uint8_t kNameMax = 10;

struct SavedState {
    uint8_t loopMask;     // bit n = loop n engaged
    bool presetMode;
    uint8_t activePreset; // 0-7
};

class Eeprom {
public:
    virtual uint8_t read(uint16_t addr) const = 0;
    virtual void update(uint16_t addr, uint8_t value) = 0;   // writes only if different
    virtual ~Eeprom() = default;
};

class PresetStore {
public:
    explicit PresetStore(Eeprom& eeprom);
    bool begin();   // false when defaults were loaded

    void loopLabel(uint8_t loop, char out[kNameMax + 1]) const;   // default "Loop n" if empty
    bool setLoopLabel(uint8_t loop, const char* name);

    bool presetUsed(uint8_t slot) const;
    void presetName(uint8_t slot, char out[kNameMax + 1]) const;
    uint8_t presetMask(uint8_t slot) const;
    bool savePreset(uint8_t slot, const char* name, uint8_t mask);
    bool renamePreset(uint8_t slot, const char* name);
    bool setPresetMask(uint8_t slot, uint8_t mask);
    bool deletePreset(uint8_t slot);

    SavedState savedState() const;
    void setState(const SavedState& state, uint32_t nowMs);
    void tick(uint32_t nowMs);   // performs the idle save
    void flush();
};
```

Conventions as in `SPEC-loop-switching.md`: `snake_case` files, `PascalCase` types, `camelCase` functions, `kPascalCase` constants; loops and slots are 0-indexed in code and 1-indexed when shown.

## Testing Strategy

Native tests (Unity) with a `FakeEeprom` that counts writes per cell:
- defaults on a blank (`0xFF`) EEPROM and on a corrupted CRC or wrong version; `begin()` returns false
- label and preset round trips across a simulated power cycle (new `PresetStore` over the same fake EEPROM)
- name validation: length, character range, trailing-space trim, empty preset name rejected
- operations on empty slots behave per requirement 3
- delete empties a slot; other slots unaffected
- idle save: no write before 2000 ms; one write after; none if unchanged; `flush()` saves immediately
- ring: records rotate through 20 slots; newest chosen correctly across sequence wraparound (`seq` 255 to 0); a record with a bad checksum is skipped
- write-count test: 1000 state saves write no single cell more than 50 times
- only changed bytes are written

Hardware checks (manual): edit data, power-cycle, confirm contents; `EEPROM.length()` equals 256.

## Boundaries

- Always: validate input at the API boundary; go through the `Eeprom` interface; run native tests before commits.
- Ask first: changing the EEPROM layout or version, changing `kNameMax`, adding a library.
- Never: write to EEPROM from interrupt context; change layout without bumping the version byte; apply a preset's loop mask (that is `operating-modes`, which calls `loop-switching`).

## Success Criteria

- All native tests pass; `pio run -e nano_every` builds.
- On the unit: labels, presets and the last state survive 20 power cycles.
- Erased or corrupted EEPROM yields defaults, not garbage, and `begin()` reports it.
- A simulated hour of footswitching (about 2000 state changes) causes at most 100 EEPROM cell writes to any single cell.

## Open Questions

None open. Accepted by the user: a power loss during a configuration write resets labels and presets to defaults on the next boot (no shadow copy fits in 256 bytes; edits are rare), and a footswitch change within 2 s of power-off is not saved.

Hardware results (Nano Every, `tools/verify_eeprom.py`, 44 checks, 2026-10-02):
- `EEPROM.length()` is 256.
- A wiped (all `0xFF`) EEPROM is detected: `begin()` reports defaults, the state ring reads as empty, and labels and presets come back as defaults.
- Labels, presets and the saved state survive 20 reset cycles (opening the serial port resets the board).
- Re-uploading the firmware keeps the stored data, so a firmware update does not erase labels and presets.
- The idle save works (state restored after a reset 3 s after the change); a change reset within the 2 s delay is not saved, as accepted.
- After 45 saves, so the ring wrapped twice, the newest record is restored and the configuration is unchanged.
- Delete works, and a second delete of an empty slot is rejected.

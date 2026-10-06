# Spec: preset-management

Module of [CAPABILITY-MAP.md](CAPABILITY-MAP.md). Depends on: `loop-switching` (loop mask format only). Consumed by: `operating-modes`.

## Objective

Own all user data that must survive power-off: eight loop labels, eight fixed preset slots (name, loop combination, and optional MIDI commands), and the last operating state. Provide a small, UI-agnostic API to read, create, rename, modify and delete this data, and a wear-aware way to save the last state.

User: a guitarist who names loops ("TS808"), builds up to eight presets, and expects the unit to come back exactly as it was left.

Success: data survives power cycles; corrupted or never-initialized storage is detected and replaced with defaults; frequent footswitch use does not wear out the EEPROM.

## Behavior Requirements

1. **Loop labels:** 8 labels, 0 to 10 characters each. An empty label means "use the default name" `Loop n` (1-based); the default is generated, not stored.
2. **Presets:** 8 fixed slots. A slot is either empty or holds a name (1 to 10 characters), a loop mask (`uint8_t`, bit n = loop n engaged, same as `loop-switching`), and `PresetMidi` settings. MIDI settings default to channel 1 with Bank Select, Program Change and effect CC disabled; all message values default to 0.
3. **Preset MIDI:** each used preset can store a channel (1-16), optional Bank Select MSB/LSB (0-127), optional Program Change (MIDI value 0-127; displayed as 1-128), and one optional effect CC number/value (0-127). MIDI settings are independent of preset name and loop-mask edits. Setting MIDI data on an empty slot fails; deleting a preset clears its MIDI data.
4. **Operations:** set loop label; save preset (create or overwrite name and mask); rename preset; set preset mask; get/set preset MIDI settings; delete preset (empties the slot). Operating on an empty slot is allowed only for save (create); rename, set-mask and set-MIDI on an empty slot fail.
5. **Name rules:** printable ASCII (0x20 to 0x7E), at most 10 characters, trailing spaces trimmed. A preset name that is empty after trimming is rejected. Invalid input returns false and changes nothing. (The UI restricts the character set further; this layer only enforces storage-safe input.)
6. **Last state:** `SavedState { loopMask, presetMode, activePreset }`. Saved with an idle delay `kIdleSaveMs` = 2000 ms after the last change, only when it differs from the last saved state. `flush()` saves immediately.
7. **Wear levelling:** state records rotate through a ring of 9 slots in EEPROM, so each cell is written once per 9 saves.
8. **Integrity:** the configuration region (labels, presets and MIDI settings) is protected by a version byte and CRC-16. If either is invalid at `begin()`, the store resets configuration to defaults and returns false. On a recognized v1-to-v2 upgrade, saved state is also reset; blank or CRC-damaged v2 configuration does not erase a separately valid state ring.
9. **State records** carry a checksum; an invalid or erased record is ignored. With no valid record, `savedState()` returns all loops bypassed, manual mode, preset 0.
10. **Minimal writes:** only bytes that actually change are written (`EEPROM.update` semantics).
11. All storage access goes through an `Eeprom` interface so logic runs in native tests.

## EEPROM Layout (256 bytes, ATmega4809)

| Offset | Size | Content |
|---|---|---|
| 0 | 1 | Layout version (`0x02`) |
| 1 | 2 | CRC-16/CCITT-FALSE of bytes 3 to 218 |
| 3 | 80 | 8 loop labels, 10 bytes each, NUL-padded |
| 83 | 136 | 8 presets: 10-byte name + 1-byte loop mask + 1-byte MIDI flags/channel + 5 MIDI values; empty slot = first name byte `0x00` |
| 219 | 1 | Reserved |
| 220 | 36 | State ring: 9 records of 4 bytes |

The MIDI flags byte stores the 0-based channel in bits 0-3, Bank Select enabled in bit 4, Program Change enabled in bit 5, and effect CC enabled in bit 6. The following values are Bank MSB, Bank LSB, Program, effect CC number, and effect CC value, each 0-127.

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
    PresetMidi presetMidi(uint8_t slot) const;
    bool setPresetMidi(uint8_t slot, const PresetMidi& midi);

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
- label, preset and MIDI-setting round trips across a simulated power cycle (new `PresetStore` over the same fake EEPROM)
- MIDI channel and values outside channel 1-16 / value 0-127 are rejected without writes; MIDI settings on an empty slot are rejected
- name validation: length, character range, trailing-space trim, empty preset name rejected
- operations on empty slots behave per requirement 3
- delete empties a slot; other slots unaffected
- idle save: no write before 2000 ms; one write after; none if unchanged; `flush()` saves immediately
- ring: records rotate through 9 slots; newest chosen correctly across sequence wraparound (`seq` 255 to 0); a record with a bad checksum is skipped
- write-count test: 1000 state saves write no single cell more than 112 times
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
- A simulated hour of footswitching (about 2000 state changes) causes at most 223 EEPROM cell writes to any single state-ring cell.

## Open Questions

Accepted by the user: a v1-to-v2 firmware upgrade resets labels, presets, MIDI settings and saved operating state to defaults; a power loss during a configuration write resets configuration to defaults on the next boot (no shadow copy fits in 256 bytes; edits are rare); and a footswitch change within 2 s of power-off is not saved.

Hardware results below were recorded against layout v1 (Nano Every, `tools/verify_eeprom.py`, 44 checks, 2026-10-02); the v2 migration intentionally resets that stored data once:
- `EEPROM.length()` is 256.
- A wiped (all `0xFF`) EEPROM is detected: `begin()` reports defaults, the state ring reads as empty, and labels and presets come back as defaults.
- Labels, presets and the saved state survive 20 reset cycles (opening the serial port resets the board).
- Re-uploading the firmware keeps the stored data, so a firmware update does not erase labels and presets.
- The idle save works (state restored after a reset 3 s after the change); a change reset within the 2 s delay is not saved, as accepted.
- After 45 saves, so the ring wrapped twice, the newest record is restored and the configuration is unchanged.
- Delete works, and a second delete of an empty slot is rejected.

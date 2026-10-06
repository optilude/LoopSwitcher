# Tasks: preset-management

Plan: [plan-preset-management.md](plan-preset-management.md). Spec: [SPEC-preset-management.md](../SPEC-preset-management.md).

Prerequisite: `loop-switching` Task 1 (PlatformIO scaffold with `native` and `nano_every` environments).

## Phase 1: Foundation

- [x] **Task 1: Eeprom interface, fake and CRC-16** (S)
  - Description: `Eeprom` interface, `FakeEeprom` (256 bytes initialised to `0xFF`, per-cell write counter, counts only real changes), and `crc16` (CCITT-FALSE) plus the layout constants with `static_assert`s.
  - Acceptance:
    - [ ] `crc16("123456789")` is `0x29B1`
    - [ ] `FakeEeprom::update` writes and counts only when the value changes
    - [ ] Layout offsets and sizes sum to at most 256 and regions do not overlap (compile-time)
  - Verify: `pio test -e native`
  - Dependencies: `loop-switching` Task 1
  - Files: `include/preset_management/eeprom.h`, `include/preset_management/layout.h`, `src/preset_management/crc16.cpp`, `test/test_preset_management/fake_eeprom.h`, `test/test_preset_management/test_crc.cpp`

## Phase 2: Core behavior

- [x] **Task 2: Configuration region and loop labels** (M)
  - Description: `PresetStore::begin()` validates version and CRC (resets to defaults when invalid and returns false), plus `loopLabel` and `setLoopLabel` with name validation (printable ASCII, 10 characters at most, trailing spaces trimmed, empty label means `Loop n`).
  - Acceptance:
    - [ ] Blank or corrupted EEPROM yields defaults and `begin()` returns false; valid data returns true
    - [ ] Label round trip survives a new `PresetStore` over the same EEPROM
    - [ ] Invalid names (too long, non-printable) are rejected without changing storage
    - [ ] Empty label reads as `Loop 1` to `Loop 8`
    - [ ] The CRC is updated after every configuration write
  - Verify: `pio test -e native`
  - Dependencies: Task 1
  - Files: `include/preset_management/preset_store.h`, `src/preset_management/preset_store.cpp`, `test/test_preset_management/test_labels.cpp`

- [x] **Task 3: Preset slots** (M)
  - Description: `presetUsed`, `presetName`, `presetMask`, `savePreset`, `renamePreset`, `setPresetMask`, `deletePreset` with the empty-slot rules from the spec.
  - Acceptance:
    - [ ] Save creates or overwrites; rename and set-mask on an empty slot return false
    - [ ] A preset name empty after trimming is rejected
    - [ ] Delete empties only that slot
    - [ ] Only changed bytes are written
    - [ ] Data survives a simulated power cycle
  - Verify: `pio test -e native`
  - Dependencies: Task 2
  - Files: `src/preset_management/preset_store.cpp`, `test/test_preset_management/test_presets.cpp`

- [x] **Task 4: State ring** (M)
  - Description: State record encoding with checksum, selection of the newest valid record using modulo-256 sequence comparison, `savedState()` and an internal write of the next record.
  - Acceptance:
    - [ ] No valid record returns all loops bypassed, manual mode, preset 0
    - [ ] Newest record is chosen correctly across `seq` wraparound (255 to 0)
    - [ ] A record with a bad checksum is skipped
    - [ ] Successive writes advance through all 9 slots and wrap
  - Verify: `pio test -e native`
  - Dependencies: Task 1
  - Files: `src/preset_management/state_ring.cpp`, `include/preset_management/state_ring.h`, `test/test_preset_management/test_state_ring.cpp`

- [x] **Task 5: Idle save and flush** (S)
  - Description: `setState(state, nowMs)`, `tick(nowMs)` writing after `kIdleSaveMs` of no change and only if different from the last saved state, and `flush()`.
  - Acceptance:
    - [ ] No write before 2000 ms; exactly one write after
    - [ ] No write when the state equals the last saved state
    - [ ] `flush()` saves immediately
    - [ ] 1000 state saves write no single EEPROM cell more than 50 times
  - Verify: `pio test -e native`
  - Dependencies: Task 4
  - Files: `src/preset_management/preset_store.cpp`, `test/test_preset_management/test_idle_save.cpp`

### Checkpoint: Core behavior
- [x] All native tests pass
- [ ] Human reviews the public `PresetStore` API before `operating-modes` is specified against it

## Phase 3: Hardware

- [x] **Task 6: Arduino EEPROM adapter and hardware verification** (S)
  - Description: `ArduinoEeprom` wrapping `EEPROM.read` and `EEPROM.update`, and a test sketch (build-time option) that prints and edits labels and presets over serial.
  - Acceptance:
    - [x] Builds for the Nano Every
    - [x] `EEPROM.length()` is 256
    - [x] Blank EEPROM yields defaults and `begin()` reports it
    - [x] Data survives 20 power cycles
    - [x] Result of re-uploading firmware on stored data is recorded in the spec
  - Verify: `pio run -e nano_every`; manual checklist on the unit
  - Dependencies: Tasks 3, 5
  - Files: `include/preset_management/arduino_eeprom.h` (header-only), `src/eeprom_demo.cpp` (build with `pio run -e eeprom_demo`), `tools/verify_eeprom.py` (repeatable hardware test)

### Checkpoint: Complete
- [x] All spec success criteria met
- [x] Open Questions in the spec updated with the hardware result

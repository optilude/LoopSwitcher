# Implementation Plan: preset-management

Implements [SPEC-preset-management.md](../SPEC-preset-management.md), module 2 of [CAPABILITY-MAP.md](../CAPABILITY-MAP.md). Tasks are in [todo-preset-management.md](todo-preset-management.md).

## Overview

A UI-agnostic store for loop labels, eight preset slots and the last operating state, persisted in the Nano Every's 256-byte EEPROM. All logic sits behind an `Eeprom` interface so it is built and tested natively; the Arduino adapter comes last.

## Architecture Decisions

- **One `Eeprom` interface** (`read`, `update`) with `FakeEeprom` for tests. The fake counts writes per cell, which lets wear levelling be asserted instead of assumed.
- **Fixed byte layout from the spec**, with offsets as named constants and one CRC-16 over labels and presets. A layout version byte guards future changes.
- **State ring separate from configuration**, so frequent state saves never touch the configuration region or its CRC.
- **Newest ring record by modulo-256 sequence comparison**, which stays correct across wraparound and with partially erased cells.
- **Time passed in** (`nowMs`), no clock inside the store, which keeps the idle-save logic deterministic in tests.
- **No dependency on hardware or other modules** except the mask convention (`uint8_t`, bit n = loop n).

## Dependency Graph

```
loop-switching Task 1 (scaffold: platformio.ini, native env)
    └── Task 1 Eeprom, FakeEeprom, CRC-16
            ├── Task 2 config region + loop labels
            │       └── Task 3 presets
            └── Task 4 state ring
                    └── Task 5 idle save and flush
Tasks 3, 5 ── Task 6 Arduino adapter + verification
```

Tasks 2-3 and Task 4-5 are independent and can proceed in parallel.

## Task List

See [todo-preset-management.md](todo-preset-management.md).

### Phase 1: Foundation
- [x] Task 1: Eeprom interface, fake and CRC-16

### Phase 2: Core behavior
- [x] Task 2: Configuration region and loop labels
- [x] Task 3: Preset slots
- [x] Task 4: State ring
- [x] Task 5: Idle save and flush

### Checkpoint: Core behavior

### Phase 3: Hardware
- [x] Task 6: Arduino EEPROM adapter and hardware verification

### Checkpoint: Complete

## Risks and Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| Layout offsets overlap or overflow 256 bytes | High | Compile-time `static_assert`s on offsets and sizes; a test that writes every region and checks nothing overlaps |
| Torn write during configuration edit resets data | Low (accepted) | Documented tradeoff; only changed bytes are written to minimize the window |
| Upload flow erases EEPROM or leaves unexpected contents | Med | Check in Task 6 that blank EEPROM yields defaults and that data survives a re-upload (or note that it does not) |
| Wear levelling bug writes one cell repeatedly | High | Per-cell write counter in `FakeEeprom`; a 1000-save test asserts at most 50 writes per cell |
| Megaavr `EEPROM` library differs from the AVR one | Low | Adapter is 10 lines; verify `EEPROM.length()` is 256 on hardware |

## Open Questions

- Resolved in Task 6: an erased EEPROM is detected, and re-uploading the firmware keeps the stored data.

# Implementation Plan: operating-modes

Implements [SPEC-operating-modes.md](../SPEC-operating-modes.md), module 4 of [CAPABILITY-MAP.md](../CAPABILITY-MAP.md). Tasks are in [todo-operating-modes.md](todo-operating-modes.md).

## Overview

The application layer: footswitch and LED handling on U2, the two operating modes, the play screen, the menus for loop names and presets, and the boot sequence. It is built against the public interfaces of `loop-switching`, `preset-management` and `ui-framework`, using their fakes in native tests, then wired up in `main.cpp`.

## Architecture Decisions

- **`PerformanceController` holds the behavior** (modes, footswitch handling, last-changed loop, state saving) and knows nothing about the OLED or menus. This is the part that must be right on stage, so it is tested the most.
- **Screens call the controller and `PresetStore`.** The play screen and menu screens are thin; they read state and call methods.
- **Hardware behind two small interfaces:** `FootswitchInput` (reads pressed-switch edges from U2) and `LedOutput` (writes the LED mask to U2). Both have fakes; the MCP23017 versions share the `I2cBus` from `loop-switching`.
- **Footswitch ISR only sets a flag.** The I2C read happens in the main loop; leading-edge action with a 30 ms per-switch lockout gives instant response without bounce.
- **Integration-style native tests** with real `LoopSwitching`, `PresetStore` and `ui-framework` classes over fakes, so interactions between modules are tested, not just each module alone.
- **All time is passed in** (`nowMs`), so timeouts and lockouts are deterministic in tests.

## Dependency Graph

```
loop-switching (apply, begin, I2cBus)     preset-management      ui-framework
              \                                  |                   /
               └──── Task 1 PerformanceController core ─────────────┘
                          ├── Task 2 Modes and preset behavior
                          ├── Task 3 Footswitch input and LED driver
                          │       └── (bit mapping, lockout, MCP23017 U2 setup)
                          ├── Task 4 Play screen
                          └── Task 5 Menus (loop names, presets)
                                  └── Task 6 Idle timeout
Tasks 3, 4, 5, 6 ── Task 7 Boot sequence and main.cpp wiring
Task 7 ── Task 8 Hardware verification
```

Tasks 3, 4 and 5 are independent once Task 2 exists.

## Task List

See [todo-operating-modes.md](todo-operating-modes.md).

### Phase 1: Behavior core
- [x] Task 1: PerformanceController, manual mode
- [x] Task 2: Preset mode and mode toggle

### Checkpoint: Behavior core

### Phase 2: Input, output and screens
- [x] Task 3: Footswitch input and LED driver
- [x] Task 4: Play screen
- [x] Task 5: Menus
- [x] Task 6: Menu idle timeout

### Checkpoint: Application logic complete

### Phase 3: Integration
- [x] Task 7: Boot sequence and wiring
- [ ] Task 8: Hardware verification

### Checkpoint: Complete

## Risks and Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| LED shows a state that differs from the relays | High | LEDs are written only from the tracked state after a successful change; a test asserts LED mask equals `stateMask()` after every operation, including failures |
| Footswitch noise (long wires, internal pull-ups only) gives phantom presses | Med | Lockout debounce; verify on the unit; add external pull-ups as a hardware fix if needed |
| A stuck I2C line (damaged display, bad cable) freezes the firmware: seen on the bench, the TWI hardware hung mid-scan | High | Enable the watchdog with a short timeout so a hang resets the unit; add an I2C bus-clear attempt at boot; keep footswitch handling out of any blocking display call. Add a task for this before Task 8 |
| `INT_SWITCH` behaves differently than assumed (polarity, latching until read) | Med | Read `GPIOA` on every interrupt and also poll at a low rate (50 ms) as a fallback; verify in Task 8 |
| Footswitch response delayed by display redraw or relay groups | High | Budgets from the other modules; end-to-end 50 ms press-to-click measured in Task 8 |
| Menu left open on stage | Med | Footswitches always live; 60 s idle timeout |
| Deleting or overwriting a preset by accident | Med | Confirm dialogs, default highlight on NO |
| RAM or flash overflow with everything linked | Med | Report RAM and flash use at every build; success criterion under 80% RAM |

## Open Questions

- `INT_SWITCH` behavior, external pull-ups on the footswitch lines and tuning of the timeouts are settled in Task 8.

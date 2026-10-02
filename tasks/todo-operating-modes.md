# Tasks: operating-modes

Plan: [plan-operating-modes.md](plan-operating-modes.md). Spec: [SPEC-operating-modes.md](../SPEC-operating-modes.md).

Prerequisites: `loop-switching` (Tasks 1-5: `LoopSwitching`, `RelayPort`, `I2cBus`), `preset-management` (Tasks 1-5), `ui-framework` (Tasks 1-6 for logic; Tasks 7-8 for hardware).

## Phase 1: Behavior core

- [x] **Task 1: PerformanceController, manual mode** (M)
  - Description: `PerformanceController` with `begin`, `onFootswitch`, `lastChangedLoop`/`lastChangedState`, an `LedOutput` interface with a fake, and the mapping footswitch/LED n to loop n. Manual mode only.
  - Acceptance:
    - [ ] Footswitch n toggles loop n through `LoopSwitching::apply`
    - [ ] LEDs equal `stateMask()` after each change
    - [ ] A failed change leaves LEDs and state unchanged and reports an error
    - [ ] The last-changed loop and its state are reported; none before the first change
    - [ ] State is passed to `PresetStore::setState`
  - Verify: `pio test -e native`
  - Dependencies: `loop-switching` Task 3, `preset-management` Task 5
  - Files: `include/operating_modes/performance_controller.h`, `include/operating_modes/led_output.h`, `src/operating_modes/performance_controller.cpp`, `test/test_operating_modes/test_manual_mode.cpp`

- [x] **Task 2: Preset mode and mode toggle** (M)
  - Description: Preset mode behavior and `toggleMode()` per the spec: preset footswitch applies the slot's mask (including re-applying the active preset), empty slots ignored with an "EMPTY" notice, simultaneous presses handled in order with the highest winning in preset mode.
  - Acceptance:
    - [ ] Footswitch n in preset mode applies preset n's mask and sets it active
    - [ ] Pressing the active preset's footswitch re-applies its loops
    - [ ] An empty slot does nothing and reports "EMPTY"
    - [ ] Entering preset mode applies the active preset (or keeps loops if empty); leaving keeps loops
    - [ ] LEDs show the actual loops in both modes
    - [ ] Mode and active preset reach `PresetStore::setState`
  - Verify: `pio test -e native`
  - Dependencies: Task 1
  - Files: `src/operating_modes/performance_controller.cpp`, `test/test_operating_modes/test_preset_mode.cpp`

### Checkpoint: Behavior core
- [x] All native tests pass
- [ ] Human reviews the mode and LED behavior against the spec

## Phase 2: Input, output and screens

- [x] **Task 3: Footswitch input and LED driver** (M)
  - Description: `FootswitchInput` (edge detection, 30 ms per-switch lockout, bit mapping SWn = GPA(8-n)), and MCP23017 implementations for U2 (0x20): `GPPUA` pull-ups enabled, interrupt-on-change on GPA, LEDs on GPB (LEDn = GPB(8-n)) driven high = on; a flag set by the `INT_SWITCH` ISR on D2 with a 50 ms polling fallback.
  - Acceptance:
    - [x] Bit mapping: SW1 = GPA7 and SW8 = GPA0; LED1 = GPB7 and LED8 = GPB0
    - [x] A press fires on the leading edge; changes within 30 ms on the same switch are ignored
    - [x] Register writes during U2 setup are exactly as designed (fake bus)
    - [x] Builds for the Nano Every
  - Verify: `pio test -e native`; `pio run -e nano_every`
  - Dependencies: Task 1, `loop-switching` Task 5 (`I2cBus`, which has `write` only: add a register read for the footswitch state here)
  - Files: `include/operating_modes/control_expander.h` + `src/operating_modes/control_expander.cpp` (U2 setup, LEDs, switch read; implements `LedOutput`), `include/operating_modes/footswitch_input.h` + `src/operating_modes/footswitch_input.cpp` (edge detection, lockout, polling fallback), `test/support/fake_mcp_bus.h` (a register-level MCP23017 model), `test/test_om_control_expander`, `test/test_om_footswitch`. `I2cBus` gained `writeRead`; the `INT_SWITCH` ISR on D2 belongs to `main.cpp` (Task 7)

- [x] **Task 4: Play screen** (S)
  - Description: Play screen showing the mode name; in manual mode the last-changed loop's name and ON/OFF; in preset mode the active preset's slot number and name, or "EMPTY". Uses the large status font. Toasts for "EMPTY", "ERROR", "DATA RESET".
  - Acceptance:
    - [ ] Manual mode shows the loop name (default `Loop n` when no label) and ON/OFF
    - [ ] Preset mode shows slot number and name, or "EMPTY"
    - [ ] The screen redraws only when the state changes
  - Verify: `pio test -e native` (FakeDisplay)
  - Dependencies: Task 2, `ui-framework` Task 3
  - Files: `include/operating_modes/play_screen.h`, `src/operating_modes/play_screen.cpp`, `test/test_operating_modes/test_play_screen.cpp`

- [x] **Task 5: Menus** (L, split if needed)
  - Description: Main menu (`PRESETS`, `LOOP NAMES`), loop-label editing through text entry, the preset slot list and slot menu (`SAVE LOOPS`, `RENAME`, `DELETE`) with confirm dialogs and result toasts, per requirements 11 to 15. The encoder press opens the menu from the play screen; `Mode` is ignored in menus.
  - Acceptance:
    - [ ] Label a loop; an empty text restores the default; cancel changes nothing
    - [ ] Save the current loops into an empty slot (name prompt, empty name rejected)
    - [ ] Overwrite a used slot keeps the name and replaces the mask after confirmation
    - [ ] Rename and delete work only on used slots; delete asks for confirmation (default NO)
    - [ ] Deleting the active preset leaves loops unchanged and the slot reads "EMPTY"
    - [ ] Toasts: "SAVED", "RENAMED", "DELETED"
  - Verify: `pio test -e native` (simulated event sequences with real `PresetStore` over `FakeEeprom`)
  - Dependencies: Task 4, `preset-management` Task 3, `ui-framework` Tasks 4-6
  - Files: `include/operating_modes/menu_flow.h`, `src/operating_modes/menu_flow.cpp`, `test/test_om_menu/test_om_menu.cpp`; small additions elsewhere: `PresetStore::hasLoopLabel`, `Menu::resetHighlight`, `PerformanceController::loopMask`
  - Note: if this exceeds about 5 files in practice, split into "loop names" and "presets" tasks.

- [x] **Task 6: Menu idle timeout** (S)
  - Description: After `kMenuIdleMs` = 60 s without input, close all menus and edit screens and return to the play screen, discarding an unconfirmed text entry.
  - Acceptance:
    - [ ] No change before 60 s; closed at 60 s
    - [ ] Each input resets the timer
    - [ ] An unconfirmed text entry is discarded
  - Verify: `pio test -e native`
  - Dependencies: Task 5
  - Files: `src/operating_modes/menu_flow.cpp` (`MenuFlow::tick`), `ScreenStack::popToRoot`, `test/test_om_timeout/test_om_timeout.cpp`. The main loop must call `MenuFlow::noteInput` for every encoder, button and footswitch event, and `tick` regularly

### Checkpoint: Application logic complete
- [x] All native tests pass
- [ ] Human walks through the menu flows against the spec using the native test output

## Phase 3: Integration

- [x] **Task 7: Boot sequence and wiring** (M)
  - Description: `main.cpp` constructing all hardware objects and modules; boot sequence per requirement 10 (initialise store, compute initial mask by mode, `begin()` loops, then LEDs, "DATA RESET" toast if needed); the main loop (`ui-framework` input tick, footswitch handling, `PresetStore::tick`, idle timeout, rendering).
  - Acceptance:
    - [x] Boot in each mode restores the right loops and LEDs
    - [x] "DATA RESET" shown for 2 s when `PresetStore::begin()` returns false
    - [x] The firmware builds and uploads; RAM use reported and under 80%
  - Verify: `pio run -e nano_every`; native boot tests with fakes
  - Dependencies: Tasks 3, 4, 5, 6; `ui-framework` Task 8
  - Files: `include/operating_modes/pedal.h` + `src/operating_modes/pedal.cpp` (all the wiring and the boot sequence behind `I2cBus`, `Clock` and `Eeprom`, tested end to end in `test/test_om_pedal`), `src/main.cpp` (the Arduino shell: INT_SWITCH interrupt on D2, display, encoder, watchdog), `include/arduino_watchdog.h`. Measured: 27.7% RAM, 69.9% flash
  - Added beyond the plan: a 2 s watchdog (a stuck I2C line resets the unit; verified on the board with a deliberate hang), and a boot report on serial

- [ ] **Task 8: Hardware verification** (manual)
  - Description: Run the full firmware on the built unit and record results.
  - Acceptance:
    - [ ] Every footswitch behaves per spec in both modes; LEDs match relays
    - [ ] Press-to-relay-click within 50 ms, also with a menu open
    - [ ] No phantom footswitch presses with the unit in the enclosure and cables connected
    - [ ] A deliberately unplugged audio PCB or a stuck I2C line recovers (watchdog) without a power cycle
    - [ ] `INT_SWITCH` behaves as assumed (or the polling fallback is adequate)
    - [ ] Names, presets and the last mode survive 20 power cycles
    - [ ] A new user can name a loop and save a preset using only the encoder, Back and Mode
  - Verify: Manual checklist; results written into the spec's Open Questions
  - Dependencies: Task 7
  - Files: `SPEC-operating-modes.md` (results)

### Checkpoint: Complete
- [ ] All spec success criteria met
- [ ] Open Questions in the spec updated with hardware results
- [ ] Review with human before release

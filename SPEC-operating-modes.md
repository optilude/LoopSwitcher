# Spec: operating-modes

Module of [CAPABILITY-MAP.md](CAPABILITY-MAP.md). Depends on: `loop-switching`, `preset-management`, `ui-framework`. Consumed by: nothing (top-level application).

## Objective

The application layer. It turns footswitch presses into loop changes in two operating modes, drives the LEDs, shows the right thing on the OLED, and provides the menus to name loops and to save, rename and delete presets.

User: a guitarist who stomps footswitches on stage and occasionally edits names and presets at home.

Success: the pedal behaves instantly and predictably on stage (footswitches always work, LEDs always tell the truth), and every edit the user wants (loop names, presets) can be done with the encoder, Back and Mode alone.

## Hardware

See the Hardware Reference in [SPEC-loop-switching.md](SPEC-loop-switching.md). In summary:
- U2 (MCP23017, 0x20): footswitch SWn on GPA(8-n) (SW1 = GPA7, SW8 = GPA0); LED n on GPB(8-n) (LED1 = GPB7, LED8 = GPB0); INTA to `INT_SWITCH` on Nano Every D2.
- Loop i (0-based) corresponds to footswitch SW(i+1) and LED(i+1): GPA bit `7 - i` and GPB bit `7 - i`.
- Footswitches are momentary SPST switches between the MCP23017 pin and ground (confirmed by the user), so a press reads low. Firmware enables the internal pull-ups (`GPPUA`); if the control PCB also has external pull-ups, that is harmless.
- Each LED anode is on the MCP23017 pin and its other side goes to ground, with a current-limiting resistor built into the LED ring (confirmed by the user). A LED is lit when its pin is driven high.
- MIDI OUT uses Nano Every D1/TX at 31,250 baud. In MIDI-enabled builds this UART is not available for text diagnostics; non-MIDI builds retain the 115200 baud boot report. The Type-A TRS output circuit is documented in `README.md`.

## Behavior Requirements

**Modes**
1. Three modes: **Manual**, **Preset** and **Perform**. The `Mode` button (short press) cycles Manual, Preset, Perform, Manual. The mode and active preset are saved through `preset-management`.
2. **Manual mode:** footswitch n toggles loop n. The OLED shows the name of the loop that was last changed and its new state (ON or OFF). Before any change since boot, it shows the mode name only; it also shows only the mode name again once a preset or the mode change has moved the loops, because the last changed loop is then no longer true.
3. **Preset mode:** footswitch n selects preset slot n; its saved loop mask is applied through `loop-switching`. Pressing the footswitch of the already active preset re-applies its loops. The OLED shows the active preset: its slot number and name. An empty slot is ignored by the footswitch and the OLED shows "EMPTY" briefly (toast); loops do not change.
3a. **MIDI output:** after a successful preset activation or reactivation, send the preset's enabled MIDI commands on its configured channel: optional Bank Select CC0 then CC32, optional Program Change, then optional effect-level CC. Empty slots and relay-application failures send nothing. Entering Preset or Perform mode sends the active preset's MIDI commands only when its loop application succeeds. Boot restoration never sends MIDI.
4. **Entering Preset or Perform mode** applies the active preset's loops straight away. If the active slot is empty, loops stay as they are and the OLED shows "EMPTY". **Entering Manual mode** leaves loops unchanged.
4a. **Perform mode:** a short press of footswitch n toggles loop n, as in Manual mode; holding it for `kPerformHoldMs` = 2000 ms selects preset slot n, as in Preset mode (empty slot: "EMPTY" toast, loops unchanged). The preset is applied when the hold time is reached, while the switch is still down; releasing afterwards does nothing. A short press acts on release, so the relay moves when the foot comes off. Of several switches held together, the highest-numbered one selects the preset. Changing mode while a switch is held discards that press. The OLED shows `PERFORM n`, the preset name, and the last stomped loop with its state. At boot in Perform mode the saved loops are restored (not the preset's own mask), because stomps change them after the preset is selected.
5. **LEDs** always show the loops that are actually on, in both modes: LED n is lit exactly when loop n is engaged. LEDs update after each successful loop change, never before.
6. A loop change that fails (I2C error) leaves the LEDs and tracked state unchanged and shows an "ERROR" toast.
7. Presses of more than one footswitch at once are handled in switch-number order; in Preset mode the highest-numbered press wins.

**Footswitches**
8. Footswitches are read from U2 after an `INT_SWITCH` interrupt (ISR only sets a flag; the I2C read happens in the main loop). A press is acted on at its leading edge; further changes on that switch are ignored for `kFootswitchLockoutMs` = 30 ms (lockout debounce).
9. Footswitches work in every screen, including menus, so a pedal left in a menu on stage never stops responding. After `kMenuIdleMs` = 60 s without any input (encoder, buttons or footswitches), any open menu or edit screen closes and the play screen returns (an unconfirmed text entry is discarded).

**Boot**
10. At boot: initialise `preset-management`; build the initial mask (in Manual mode the saved mask; in Preset mode the active preset's mask if the slot is used, otherwise the saved mask); call `loop-switching` `begin(initialMask)`; then set the LEDs. If `begin()` of `preset-management` returned false, show "DATA RESET" for 2 s. If either expander does not answer, the UI still comes up and shows "ERROR"; a relay or LED write that fails later is retried or reported the same way. A 2 s watchdog resets the unit if the main loop stops (for example a stuck I2C line), and the boot report on serial says so.

**Menu** (opened with the encoder press on the play screen; `Back` at the root returns to the play screen; `Mode` is ignored inside menus)
11. Main menu: `PRESETS`, `LOOP NAMES`.
12. `LOOP NAMES`: a list of the eight loops (current label or default). Selecting one opens text entry, starting empty when the label is default and otherwise containing the current label. Confirming an empty text restores the default; cancel changes nothing.
13. `PRESETS`: a list of the eight slots, shown as `n NAME` or `n EMPTY`. Selecting a slot opens its menu. An empty slot's menu has only `SAVE LOOPS`; `RENAME`, `DELETE` and (when MIDI is compiled in) `MIDI` are not shown until a preset has been saved into the slot (and disappear again when it is deleted):
    - `SAVE LOOPS`: stores the current loops into the slot. For an empty slot, text entry for the name opens first, starting empty; the preset is saved with the name and the current loop mask when confirmed (an empty name is not accepted; a "NAME REQUIRED" toast is shown). For a used slot, a confirm dialog "OVERWRITE?" is shown; on YES, only the mask is replaced and the name is kept.
    - `RENAME`: used slots only; text entry starting with the current name.
    - `DELETE`: used slots only; confirm dialog "DELETE?"; on YES the slot is emptied. Deleting the active preset leaves loops unchanged and the active slot reads "EMPTY".
    - `MIDI`: used slots only; edit channel 1-16, toggle Bank Select / Program Change / effect CC, edit Bank MSB/LSB (0-127), Program Change number (1-128), effect CC number and value (0-127). The data is stored with the preset.
14. After a successful save, rename or delete, a toast confirms it ("SAVED", "RENAMED", "DELETED").
15. Presets are created and modified only by capturing the current loops (`SAVE LOOPS`); there is no loop-picker screen. To change a preset's loops, set them in Manual mode and save them into the slot.

**Display**
16. The play screen uses the large status font from `ui-framework`; names that are default (`Loop n`) appear as stored. The OLED never obscures a toast.

## Tech Stack

- C++17, Arduino framework, PlatformIO, board `nano_every`
- Uses `loop-switching`, `preset-management` and `ui-framework` through their public headers; no new libraries.

## Commands

```
Build:       pio run -e nano_every
Upload:      pio run -e nano_every -t upload
Unit tests:  pio test -e native
Format:      clang-format -i src/**/*.cpp src/**/*.h include/**/*.h
```

## Project Structure

```
src/operating_modes/      → PerformanceController, FootswitchInput, LedDriver, screens
include/operating_modes/  → public headers
src/main.cpp              → wiring of all modules (the only place that includes Arduino hardware objects)
test/test_operating_modes/ → native tests with fakes for all hardware
```

## Code Style

```cpp
class PerformanceController {
public:
    static constexpr uint8_t kNone = 0xFF;
    PerformanceController(LoopSwitching& loops, PresetStore& store, LedOutput& leds, MidiOutput* midi = nullptr);

    bool begin(bool dataReset);                          // after PresetStore::begin(); false if the relays failed
    void onFootswitch(uint8_t index, uint32_t nowMs);    // 0-7
    void onFootswitches(uint8_t pressedMask, uint32_t nowMs);   // several at once
    void toggleMode(uint32_t nowMs);
    void tick(uint32_t nowMs);                           // idle save, LED retry

    Mode mode() const;
    uint8_t loopMask() const;
    uint8_t activePreset() const;
    uint8_t lastChangedLoop() const;                     // kNone when not meaningful
    bool lastChangedState() const;
    Notice takeNotice();                                 // Error, DataReset, EmptyPreset; once
};
```

Conventions as in the other specs. The controller knows nothing about the OLED or menus, and screens call the controller and `PresetStore` through plain method calls.

## Testing Strategy

Native tests (Unity), using real `LoopSwitching`, `PresetStore` and `ui-framework` classes over fakes (`FakeRelayPort`, `FakeEeprom`, `FakeDisplay`, `FakeClock`, fake LED output and fake MIDI output):
- manual mode: footswitch n toggles loop n; the last-changed loop and state are reported
- preset mode: footswitch n applies the preset mask; an empty slot does nothing
- MIDI output: exact message order/channel/value, mode-entry resend, and no output on boot, empty slot or relay failure
- mode toggle: entering Preset mode applies the active preset; leaving it leaves loops unchanged
- LEDs equal the actual loop mask after every change; unchanged after a failed change
- simultaneous presses: order and "highest wins" in Preset mode
- lockout debounce: second edge within 30 ms ignored; accepted after
- footswitch/LED bit mapping against the spec (SW1 = GPA7, LED8 = GPB0)
- boot: initial mask in each mode, "DATA RESET" toast when data was reset
- menu flows end to end with simulated events: label a loop, save a preset into an empty slot, overwrite, rename, delete, delete the active preset
- MIDI menu: edit channel/values and toggle message types; the editor is absent when `LOOPSWITCHER_ENABLE_MIDI=0`
- idle timeout closes menus and discards an unconfirmed text entry
- state saving: mode, active preset and mask reach `PresetStore::setState`

Hardware checks (manual, on the built unit): stomp every footswitch in both modes; check LEDs and relays agree; edit names and presets through the real menu; power-cycle and confirm everything is restored.

## Boundaries

- Always: keep LEDs consistent with the real loop state; keep footswitches live in every screen; run native tests before commits; do not send MIDI until loop application succeeds.
- Ask first: changing footswitch or menu semantics, adding menu items, changing timeouts.
- Never: switch relays directly (go through `loop-switching`); write EEPROM directly (go through `preset-management`); block the main loop beyond the budgets in the other modules.

## Success Criteria

- All native tests pass; `pio run -e nano_every` builds.
- On the unit, in both modes: every footswitch does what the spec says, LEDs match the loops, and the OLED shows the right loop name or preset.
- A footswitch press switches its relay within 50 ms (measured from the press to the relay click), including with a menu open.
- MIDI messages are emitted at 31,250 baud in the documented order, while preserving the 50 ms footswitch response target.
- Loop names, presets and the last mode survive power cycles.
- A new user can name a loop and save a preset using only the encoder, Back and Mode.

## Open Questions

1. `INT_SWITCH`: assumed active-low push-pull from INTA with interrupt-on-change; confirm the electrical behaviour on D2 during bring-up.
2. Timeouts: `kMenuIdleMs` = 60 s and `kFootswitchLockoutMs` = 30 ms are defaults accepted by the user; tune on the unit.
3. Do any footswitch lines have external pull-ups on the control PCB, or is the internal 100 kΩ pull-up enough? Confirm during bring-up (long wires could pick up noise).

Resolved by the user: footswitch and LED wiring; the 60 s and 30 ms defaults; pressing the footswitch of the active preset re-applies its loops.

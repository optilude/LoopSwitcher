# Tasks: ui-framework

Plan: [plan-ui-framework.md](plan-ui-framework.md). Spec: [SPEC-ui-framework.md](../SPEC-ui-framework.md).

Prerequisite: `loop-switching` Task 1 (PlatformIO scaffold with `native` and `nano_every` environments).

## Phase 1: Input

- [x] **Task 1: Event type and quadrature decoder** (S)
  - Description: `Event` enum and a pure `decodeQuadrature(prevState, newState)` returning -1, 0 or +1, with an accumulator that yields one step per detent (detent divisor is a named constant).
  - Acceptance:
    - [ ] A full clockwise detent sequence yields exactly one `+1` step; counter-clockwise yields `-1`
    - [ ] Invalid transitions (bounce, skipped states) yield no step
    - [ ] The accumulator does not drift after a back-and-forth wiggle
  - Verify: `pio test -e native`
  - Dependencies: `loop-switching` Task 1
  - Files: `include/ui_framework/event.h`, `include/ui_framework/quadrature.h`, `src/ui_framework/quadrature.cpp`, `test/test_ui_framework/test_quadrature.cpp`

- [x] **Task 2: Button handling and event queue** (M)
  - Description: Debounce, short press on release, long press fired once at `kLongPressMs`, for encoder push, Back and Mode; a ring queue of 8 events that drops the oldest when full; `FakeClock`.
  - Acceptance:
    - [ ] Bounces shorter than 20 ms produce no event
    - [ ] A press under 600 ms emits the short event on release
    - [ ] A press held 600 ms emits the long event once and no short event on release
    - [ ] Queue overflow drops the oldest event
  - Verify: `pio test -e native`
  - Dependencies: Task 1 (Event type)
  - Files: `include/ui_framework/buttons.h`, `src/ui_framework/buttons.cpp`, `include/ui_framework/event_queue.h`, `test/test_ui_framework/test_buttons.cpp`

## Phase 2: Screens and widgets

- [x] **Task 3: Display interface, screen stack and toast** (M)
  - Description: `Display` interface, `FakeDisplay` recording draws, `Screen` interface, `ScreenStack` (push, pop on `Back`, dirty flag) and a toast that shows for `kToastMs` and expires.
  - Acceptance:
    - [ ] `Back` pops; pushing and popping keeps the previous screen state
    - [ ] A screen is redrawn only when dirty, or when a toast appears or expires
    - [ ] The toast shows for 1000 ms then clears
    - [ ] The stack never allocates dynamically (fixed depth, rejects push when full)
  - Verify: `pio test -e native`
  - Dependencies: `loop-switching` Task 1
  - Files: `include/ui_framework/display.h`, `include/ui_framework/screen.h`, `src/ui_framework/screen_stack.cpp`, `test/test_ui_framework/fake_display.h`, `test/test_ui_framework/test_screen_stack.cpp`

- [x] **Task 4: Menu widget** (M)
  - Description: List of items (label, optional value text, enabled flag, callback) with highlight movement, scrolling window of 4 rows and no wrap.
  - Acceptance:
    - [ ] `Left`/`Right` move the highlight and stop at the ends
    - [ ] The window scrolls so the highlight is always visible
    - [ ] Disabled items are skipped when moving the highlight and not activated by `Select`
    - [ ] `Select` calls the item callback; `Back` pops
  - Verify: `pio test -e native`
  - Dependencies: Task 3
  - Files: `include/ui_framework/menu.h`, `src/ui_framework/menu.cpp`, `test/test_ui_framework/test_menu.cpp`

- [x] **Task 5: Text entry widget** (M)
  - Description: Text entry with the character set `A-Z 0-9 space - . + / #`, cursor, and the control mapping from the spec (rotate cycles through a "no character" position `_` and the set, `Select` accepts and advances, `SelectLong` confirms including the character under the cursor, `Back` erases or cancels, `BackLong` cancels).
  - Acceptance:
    - [ ] Cycling wraps around the character set
    - [ ] Maximum 10 characters; `Select` at the last position stays there
    - [ ] `Back` erases the last character, and cancels when text is empty
    - [ ] Confirm returns the text through the callback; cancel returns nothing
  - Verify: `pio test -e native`
  - Dependencies: Task 3
  - Files: `include/ui_framework/text_entry.h`, `src/ui_framework/text_entry.cpp`, `test/test_ui_framework/test_text_entry.cpp`

- [x] **Task 6: Confirm dialog** (S)
  - Description: `YES`/`NO` dialog; `Select` chooses the highlighted option, `Back` means NO. Default highlight is NO.
  - Acceptance:
    - [ ] Default highlight is NO
    - [ ] `Select` returns the highlighted choice through the callback; `Back` returns NO
  - Verify: `pio test -e native`
  - Dependencies: Task 3
  - Files: `include/ui_framework/confirm.h`, `src/ui_framework/confirm.cpp`, `test/test_ui_framework/test_confirm.cpp`

### Checkpoint: Logic complete
- [ ] All native tests pass
- [ ] Human reviews the `Display` interface and widget APIs before `operating-modes` is specified against them

## Phase 3: Hardware

- [x] **Task 7: U8g2 display and fonts** (M)
  - Description: Add U8g2 to `lib_deps`; implement `U8g2Display` for the SH1106 128x64 I2C in page-buffer mode; pick the fonts (list font, large status font where 10 characters fit one line); a font preview in the demo sketch.
  - Acceptance:
    - [ ] Builds for the Nano Every; RAM use reported and under 80%
    - [ ] Chosen list font: 10 characters within 128 px, rows at least 14 px
    - [ ] Chosen status font: the largest where 10 characters fit on one line
  - Verify: `pio run -e nano_every`; manual check of the font preview on the unit
  - Dependencies: Task 3
  - Files: `platformio.ini`, `include/ui_framework/u8g2_display.h`, `src/ui_framework/u8g2_display.cpp`, `src/main.cpp`

- [x] **Task 8: Arduino input adapters and demo sketch** (M)
  - Description: Pin setup (D3/D4/D5/D6/D7 as inputs with pull-ups), a pin-change ISR for the encoder that runs the decoder, polling of buttons, and a demo sketch exercising the menu, text entry, confirm dialog and toast.
  - Acceptance:
    - [ ] ISR only updates a step counter
    - [ ] Events reach the active screen via `tick()`
    - [ ] The demo covers all widgets; builds for the Nano Every
  - Verify: `pio run -e nano_every`
  - Dependencies: Tasks 1, 2, 4, 5, 6, 7
  - Files: `include/ui_framework/arduino_input.h`, `src/ui_framework/arduino_input.cpp`, `src/main.cpp`

- [x] **Task 9: Hardware verification** (manual)
  - Description: Run the demo on the built unit and record results.
  - Acceptance:
    - [x] Display shows correctly with the SH1106 driver at the assumed I2C address (flash `pio run -e ui_demo -t upload`, open the FONTS screen)
    - [x] Font preview legible: the 10 widest characters (`WWWWWWWWWW`) fit on one line in both fonts, text readable from about 2 m; if not, change the fonts in `u8g2_display.cpp` and `kStatusCharWidth`
    - [x] Encoder detent count matches the divisor; fast spinning (about 120 detents per second) loses no steps
    - [x] Mode and Back polarity and pull-ups confirmed
    - [x] Worst-case render step measured at 30 ms or less (measured 21 ms; whole frame 122 ms over 8 steps)
    - [x] Text readable from about 2 m
  - Verify: Manual checklist; results written into the spec's Open Questions
  - Dependencies: Task 8
  - Files: `SPEC-ui-framework.md` (results)

### Checkpoint: Complete
- [x] All spec success criteria met
- [x] Open Questions in the spec updated with hardware results

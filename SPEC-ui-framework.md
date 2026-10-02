# Spec: ui-framework

Module of [CAPABILITY-MAP.md](CAPABILITY-MAP.md). Depends on: nothing (hardware pins in [SPEC-loop-switching.md](SPEC-loop-switching.md), Hardware Reference). Consumed by: `operating-modes`.

## Objective

Provide the generic on-device UI toolkit: input events from the encoder and two buttons, a screen stack, a menu widget, a text-entry widget operated by the encoder, a confirmation dialog, and rendering to the 128x64 OLED in fonts readable from a distance. It has no knowledge of loops or presets.

User: a guitarist on a dark stage who edits names and presets with one encoder and two buttons.

Success: every widget can be operated with the encoder (rotate, press, long press), Back and Mode only; text is readable at a glance; UI work never delays a footswitch action noticeably.

## Hardware

See the Hardware Reference in [SPEC-loop-switching.md](SPEC-loop-switching.md).
- OLED: HS13L03W2C01, 1.3" "I2C IIC OLED Display 128x64 Pixels SSH1106" (SH1106 controller, confirmed by the user), I2C (A4/A5), shared bus with U2 and U3. Address assumed 0x3C. I2C at 400 kHz is assumed reliable (confirmed by the user).
- Encoder with push: Bourns PEC11 series (schematic part PEC11R-4220K-S0024) on D3 (A), D4 (B), D5 (push). Detents and pulses per revolution assumed 24; verify on hardware.
- Mode button (SW2) on D6, Back button (SW3) on D7. Assumed active-low with `INPUT_PULLUP`; to be verified against the schematic.

## Behavior Requirements

**Input events**
1. Events: `Left`, `Right` (one per detent), `Select` (encoder press), `SelectLong`, `Back`, `BackLong`, `Mode`, `ModeLong`.
2. A short press fires on release; a long press fires once when the button has been held `kLongPressMs` = 600 ms and does not also fire a short press on release.
3. Buttons are debounced (`kDebounceMs` = 20 ms).
4. The encoder is decoded with a quadrature state table; invalid transitions (bounce) produce no step. Decoding runs from pin-change interrupts so that steps are not lost while the main loop is blocked by a relay pulse or screen update. The ISR only updates a small step counter; events are produced in `tick()`.
5. Events queue in a ring of 8; if full, the oldest event is dropped.

**Screens and widgets**
6. A `ScreenStack` holds screens; `Back` pops, a screen may push another. Each screen implements `handle(Event)` and `draw(Display&)`.
7. `Menu`: a list of items (label, optional value text, enabled flag, on-select callback). `Left`/`Right` move the highlight (no wrap), `Select` activates, `Back` pops. The list scrolls so the highlight is always visible. A visible window of at most 4 rows.
8. `TextEntry`: edits a string of at most `kNameMax` (10) characters. Character set: `A-Z`, `0-9`, space, `-`, `.`, `+`, `/`, `#`. The cursor sits after the accepted text and shows either "no character" (an inverted `_`) or one character of the set. `Left`/`Right` rotate it through `_`, the set and back to `_` (wrapping). `Select` accepts the character under the cursor and moves on, leaving the cursor on `_` (nothing happens on `_`). `SelectLong` confirms the text including the character under the cursor, unless the cursor is on `_`, so the last character needs no short press. `Back` clears the character under the cursor, then erases the last accepted character, and cancels when there is neither. `BackLong` cancels. Returns the text through a callback; cancel returns nothing.
9. `Confirm`: a two-choice dialog (`YES`/`NO`) for destructive actions; `Select` chooses, `Back` means NO.
10. A transient message (for example "SAVED") can be shown for `kToastMs` = 1000 ms over any screen.

**Rendering**
11. Text sizes: lists and the text entry use a font where 10 characters fit in 128 px (at most 12 px advance per glyph) and rows are at least 14 px high; the main status text uses the largest font in which a 10-character name fits on one line.
12. A screen is redrawn only when marked dirty or when a toast appears or expires.
13. Rendering is incremental: the main loop draws at most one display page (one eighth of the screen) per iteration with `ScreenStack::renderStep()`, so input and footswitch handling are delayed by at most one step. One step must take 30 ms or less; a whole frame may span several iterations. Measured on the unit (SH1106, 400 kHz): longest step 21 ms, full menu frame 122 ms, of which 48 ms is sending (about 6 ms per page) and the rest is drawing text into the page buffer. A change made during a frame is drawn in the next frame. The display adapter skips text outside the current page.
14. All drawing goes through a `Display` interface (text at position, font choice, invert/highlight rows, clear, flush) so widgets run in native tests; `U8g2Display` is the Arduino implementation.

## Tech Stack

- C++17, Arduino framework, PlatformIO, board `nano_every`
- U8g2 (new dependency; approved for the SH1106 display), configured in page-buffer mode to save RAM
- Wire at 400 kHz

## Commands

```
Build:       pio run -e nano_every
Unit tests:  pio test -e native
Format:      clang-format -i src/**/*.cpp src/**/*.h include/**/*.h
```

## Project Structure

```
src/ui_framework/      → input decoding, screen stack, widgets, U8g2Display
include/ui_framework/  → Event, Display, Screen, widget headers
test/test_ui_framework/ → native tests with FakeDisplay and FakeClock
```

## Code Style

```cpp
enum class Event : uint8_t {
    None, Left, Right, Select, SelectLong, Back, BackLong, Mode, ModeLong
};

class Display {
public:
    // Page-buffer rendering: draw everything between firstPage() and the last nextPage().
    virtual void firstPage() = 0;
    virtual bool nextPage() = 0;   // true while another pass is needed
    virtual void text(uint8_t x, uint8_t y, const char* s, Font font, bool inverted) = 0;
    virtual ~Display() = default;
};

class Screen {
public:
    virtual bool handle(Event event) = 0;   // true if used; an unused Back pops the screen
    virtual void draw(Display& display) = 0;   // runs once per display pass; no state changes
    virtual ~Screen() = default;
};
```

Widgets that finish an interaction (`TextEntry`, `Confirm`) report the outcome through callbacks (function pointer plus context, since the AVR has no `std::function`); the callback is responsible for popping the screen. Text entry saves what is on screen: the accepted text plus the character under the cursor (see requirement 8).

Conventions as in the other specs. No dynamic allocation after `begin()`; screens are statically allocated.

## Testing Strategy

Native tests (Unity):
- quadrature decoder: clockwise/counter-clockwise sequences produce the right step counts; bounce and skipped states produce none
- button debounce, short press on release, long press fired once and no short press afterwards, using `FakeClock`
- event queue overflow drops the oldest
- `Menu`: highlight movement, scrolling window, disabled items, selection callback, Back
- `TextEntry`: cycling and wrap through the `_` position, accept, long press saving the pending character, erase (pending first), confirm, cancel, 10-character limit, empty text cancel
- `Confirm`: choose and Back
- toast shows then expires
- widget draw output checked through `FakeDisplay`

Hardware checks (manual, a UI demo sketch): operate each widget with the real encoder and buttons; fast spinning (about 120 detents per second) loses no steps; longest render step measured at 30 ms or less; text readable from about 2 m.

## Boundaries

- Always: keep rendering independent of loop and preset logic; keep ISRs minimal; run native tests before commits.
- Ask first: adding libraries other than U8g2, changing the character set, changing event semantics or `kNameMax`.
- Never: block for more than the render budget; allocate memory dynamically in the main loop; touch relay or EEPROM code.

## Success Criteria

- All native tests pass; `pio run -e nano_every` builds with RAM use under 80% of 6 KB.
- Every widget operable using only encoder, Back and Mode.
- A 10-character name fits on one line in the lists, the text entry and the main status font.
- The longest render step takes 30 ms or less on hardware, and fast encoder spinning loses no steps.

## Open Questions

None open. Resolved on the built control board (2026-10-01/02):
- Display: SH1106 at I2C address 0x3C; the menu, text entry and confirm screens work.
- Buttons: Mode, Back and the encoder push are active low with the Nano's internal pull-ups; the encoder turns clockwise as `Right` with one event per click.
- Text entry as proposed (upper case only); 400 kHz I2C works (bench: 992 us per 31-byte transfer at 400 kHz, 591 us at 800 kHz and 1 MHz, no failures).
- Mode events are produced but no demo screen uses them; the mode toggle belongs to `operating-modes`.

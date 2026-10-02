# Implementation Plan: ui-framework

Implements [SPEC-ui-framework.md](../SPEC-ui-framework.md), module 3 of [CAPABILITY-MAP.md](../CAPABILITY-MAP.md). Tasks are in [todo-ui-framework.md](todo-ui-framework.md).

## Overview

A generic toolkit for the 128x64 SH1106 OLED, the Bourns PEC11 encoder and the Mode/Back buttons: input events, a screen stack, a menu, a text-entry widget and a confirm dialog. Everything is built against `Display` and a clock interface and tested natively; U8g2, the interrupt-driven encoder and the hardware demo come last.

## Architecture Decisions

- **Pure decoder and state machines first.** The quadrature decoder (`prev, next` → step) and the button state machine take pin states and time as input and run natively.
- **ISR does almost nothing.** The pin-change ISR runs the decoder and updates a step counter; `tick()` turns that into `Left`/`Right` events. This keeps encoder steps from being lost while a relay pulse or screen update blocks the loop.
- **`Display` interface with `FakeDisplay`.** Widgets draw only through it, and tests assert on recorded text and highlighted rows. `U8g2Display` is the only code that includes U8g2.
- **Statically allocated screens, no heap** after `begin()`, because the ATmega4809 has 6 KB of RAM.
- **Dirty-flag, incremental rendering:** one display page per main-loop iteration, with a 30 ms budget per step, checked on hardware.
- **Widgets stay generic:** callbacks and strings in, no loop or preset types, so `operating-modes` composes them.

## Dependency Graph

```
loop-switching Task 1 (scaffold)
    ├── Task 1 Event + quadrature decoder
    ├── Task 2 Button handling + event queue
    └── Task 3 Display interface, ScreenStack, toast
            ├── Task 4 Menu
            ├── Task 5 TextEntry
            └── Task 6 Confirm
Task 3 ── Task 7 U8g2Display and fonts
Tasks 1, 2 ── Task 8 Arduino input adapters + demo sketch (needs Tasks 4-7)
Task 8 ── Task 9 Hardware verification
```

Tasks 1-2 are independent of Tasks 3-6 and can proceed in parallel; Tasks 4-6 are independent of each other.

## Task List

See [todo-ui-framework.md](todo-ui-framework.md).

### Phase 1: Input
- [x] Task 1: Event type and quadrature decoder
- [x] Task 2: Button handling and event queue

### Phase 2: Screens and widgets
- [x] Task 3: Display interface, screen stack and toast
- [x] Task 4: Menu widget
- [x] Task 5: Text entry widget
- [x] Task 6: Confirm dialog

### Checkpoint: Logic complete

### Phase 3: Hardware
- [x] Task 7: U8g2 display and fonts (code complete; on-device check in Task 9)
- [x] Task 8: Arduino input adapters and demo sketch (code complete; on-device check in Task 9)
- [x] Task 9: Hardware verification

### Checkpoint: Complete

## Risks and Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| A slow redraw delays footswitch response | High | Incremental rendering (`renderStep`, one page per loop iteration), text outside the current page skipped. Measured: whole frame 122 ms, longest step 21 ms, sending 6 ms per page. Raising the I2C clock to 800 kHz to 1 MHz (591 us per 31 bytes against 992 us) would roughly halve sending time if ever needed |
| No 10-character font within 12 px advance and 14 px rows reads well | Med | Choose from U8g2 fonts in Task 7 with a font preview sketch; fall back to a condensed font |
| SH1106 shows a 2 px column offset with an SSD1306 driver | Low | Use the SH1106 driver in U8g2; confirmed by the user |
| RAM exhaustion with U8g2 + widgets (6 KB) | Med | Page buffer (128 bytes), static screens, report RAM use at each build; success criterion under 80%. Measured with the demo: RAM 19%, **flash 52.7% (25.6 KB of 48.6 KB)**, mostly U8g2 and its fonts, so flash headroom for the rest of the application should be re-checked at each integration step |
| Encoder bounce or wrong detent count | Med | State-table decoder rejects invalid transitions; detent divisor is one constant, verified in Task 9 |
| Button polarity or pull-ups differ from assumption | Low | Single adapter function; checked in Task 9 |
| Shared I2C bus contention between display and expanders | Low | Single-threaded access, no interrupts on `Wire`; 400 kHz assumed reliable by the user |

## Open Questions

- Display I2C address, encoder detent count and button pull-ups are confirmed in Task 9.

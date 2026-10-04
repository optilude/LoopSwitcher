# Spec: loop-switching

Module of [CAPABILITY-MAP.md](CAPABILITY-MAP.md). Depends on: nothing. Consumed by: `preset-management`, `operating-modes`.

## Objective

Provide the firmware layer that sets each of the eight guitar-effect loops in or out of the signal path by driving the latching relays on the (already manufactured) audio PCB.

User: a guitarist, indirectly, via higher modules. Success: any requested loop-state change results in the correct relays being physically switched, with both coils of a relay never energized together and all coils released afterward.

## Hardware Reference

Sources: `Circuit/SCH_Audio Circuit_2026-09-19.pdf`, `Circuit/SCH_Control Circuit_2026-09-19.pdf`, user-confirmed pin maps. The `ui-framework` and `operating-modes` specs cite this section.

**Controller:** Arduino Nano Every (ABX00028, ATMega4809). I2C on A4 (SDA) / A5 (SCL), 4.7 kΩ pull-ups on the audio PCB.

**Control PCB pins (Nano Every):**

| Signal | Pin |
|---|---|
| `INT_SWITCH` (from U2 INTA, via CTRL header pin 1) | D2 |
| Encoder A / B / push | D3 / D4 / D5 |
| Momentary switch SW2 ("Mode") | D6 |
| Momentary switch SW3 ("Back") | D7 |
| OLED (HS13L03W2C01, I2C) | A4 / A5 |

CTRL header: pin 1 = `INT_SWITCH`, pin 2 = SCL, pin 3 = SDA. Pins read from the schematic image; SW2/SW3 roles confirmed by the user.

**U2: MCP23017 at 0x20** (A0=A1=A2=GND) — footswitches and LEDs (used by `operating-modes`):
- Footswitch SWn on GPA(8−n): SW1=GPA7 … SW8=GPA0.
- LED n on GPB(8−n): LED1=GPB7 … LED8=GPB0.
- INTA → `INT_SWITCH`.

**U3: MCP23017 at 0x21** (A0=+5V, A1=A2=GND) — relay coils (this module):

| Loop (1-based) | RST coil | SET coil |
|---|---|---|
| 1 | GPA0 | GPA1 |
| 2 | GPA2 | GPA3 |
| 3 | GPA4 | GPA5 |
| 4 | GPA6 | GPA7 |
| 5 | GPB0 | GPB1 |
| 6 | GPB2 | GPB3 |
| 7 | GPB4 | GPB5 |
| 8 | GPB6 | GPB7 |

For 0-based loop `i`: port = `i / 4` (0 = A, 1 = B); RST bit = `2 * (i % 4)`; SET bit = RST bit + 1. Each coil is wired: MCP23017 pin → 1 kΩ → transistor → relay coil pin, with a flyback diode across the coil (confirmed by the user). No pull-down on the transistor base. A coil is energized by driving its pin high.

**Relays:** Panasonic TQ2-L2-5V, 2-coil latching. After RST the loop is bypassed; after SET it is engaged. Loop order in the signal chain is fixed by wiring.

**Relay timing/current (datasheet, TQ2 series):** set time max 3 ms, reset time max 3 ms (nominal voltage). 3 V coil: 45 Ω, 200 mW per coil. The 5 V row was not read; about 40 mA per coil is derived (200 mW / 5 V) and must be confirmed.

**Errata found on the first built unit (control PCB, 2026-10-01):**
- The OLED footprint has VCC and GND swapped relative to the real HS13L03W2C01 module (the product image and library footprint say GND, VCC, SCL, SDA; the delivered module is VCC, GND, SCL, SDA). Powered the wrong way round, the first display held SDA and SCL low and stopped working. Fixed on this unit by crossing the two wires; fix the footprint in the next PCB revision.
- The Nano's GND was not connected to the PCB ground net, so the display, switches and encoder had no common ground. Fixed on this unit with a wire from the Nano's GND pin to the ground pad; fix the netlist in the next revision.
- The I2C pins are `SDA`/`SCL` (PA2/PA3) in the Arduino core, while the header pins are labelled A4/A5. Measured: A4 (PF2) and SDA (PA2) read the same level, so they share the header pin. Use `Wire` as normal.

**Errata found on the assembled pedal (audio PCB, 2026-10-03):**
- In the schematic, C5 (U2) and C6 (U3) sit in series between the +5V flag and the MCP23017 VDD pin instead of between VDD and GND, so neither chip's VDD is connected to the +5V rail. Measured with the I2C lines connected: +5V side 5.04 V, VDD 4.7 V (U2) and 1.3 V (U3). The chips were only back-fed through input protection diodes (RESET#, A0, SDA/SCL), which is why U3 answered and U2 did not. Fix on the unit: wire VDD to +5V and add a 100 nF cap across VDD and VSS; fix the schematic and netlist in the next revision.

## Tech Stack

- C++17, Arduino framework, PlatformIO
- Platform `atmelmegaavr`, board `nano_every`
- No third-party libraries: a minimal MCP23017 driver over `Wire` (a new dependency requires approval)

## Commands

```
Build:          pio run -e nano_every
Upload:         pio run -e nano_every -t upload
Unit tests:     pio test -e native
Hardware test:  manual checklist on the built unit
Format:         clang-format -i src/**/*.cpp src/**/*.h include/**/*.h
```

## Project Structure

```
platformio.ini        → envs: nano_every, native
src/loop_switching/   → LoopSwitching logic and Mcp23017RelayPort
include/              → shared headers (RelayPort, pin/address constants)
test/                 → native unit tests with a fake RelayPort
CAPABILITY-MAP.md     → module index
SPEC-<module>.md      → per-module specs
```

## Code Style

Hardware access goes through an interface so logic is testable off-device.

```cpp
struct RelayDrive {
    uint8_t setMask;   // bit n: pulse SET on loop n
    uint8_t rstMask;   // bit n: pulse RST on loop n
};

class RelayPort {
public:
    virtual bool energize(RelayDrive drive) = 0;   // writes coils high; false on I2C error
    virtual bool releaseAll() = 0;                 // writes all coils low
    virtual ~RelayPort() = default;
};

class LoopSwitching {
public:
    explicit LoopSwitching(RelayPort& port);
    bool begin(uint8_t initialMask);       // drives every relay to initialMask
    bool apply(uint8_t mask);              // bit n = loop n engaged
    uint8_t stateMask() const;
};
```

Conventions: `snake_case` files, `PascalCase` types, `camelCase` functions, constants `kPascalCase`. Loops are 0-indexed in code and 1-indexed in anything shown to users.

## Testing Strategy

Native unit tests (PlatformIO `native`, Unity) against a fake `RelayPort`:
- a relay never gets SET and RST in the same drive
- an unchanged loop gets no pulse once state is known
- `begin()` pulses all eight relays (state is unknown at boot)
- no more than `kMaxSimultaneousRelays` relays are energized in one drive; larger changes are split into sequential groups
- coils are released after every pulse group
- bit mapping matches the loop/coil table (loop 1 → GPA0/GPA1, loop 8 → GPB6/GPB7)
- a failed write leaves tracked state unchanged and returns false

Hardware checklist (manual, on the built unit): each loop clicks and passes/bypasses audio; relay coils are not warm after switching; the supply rail stays stable when many relays switch at once.

## Behavior Requirements

1. `apply(mask)` computes which loops differ from the tracked state and pulses SET for newly engaged loops and RST for newly bypassed loops.
2. A pulse lasts `kRelayPulseMs` = 10 ms (datasheet maximum set/reset time is 3 ms), then all coils are released.
3. SET and RST of one relay are never energized together.
4. At most `kMaxSimultaneousRelays` relays are pulsed in one group (default 4 until hardware testing validates a higher value; 8 is the upper bound). Groups run sequentially with `kRelayGroupGapMs` between them.
5. Within a group, coils are written with one register write per U3 port, so a group's pulses are simultaneous.
6. `begin(initialMask)` runs once at boot: write the output latches of U3 to 0x00 first, then configure the pins as outputs (avoids spurious coil energizing), then drive every relay to `initialMask`, because latching relay positions are unknown to firmware.
7. Tracked state is updated only after a group's pulse completes.
8. If any I2C write to U3 fails, tracked state is not updated and the failure is returned to the caller.
9. The initial mask comes from `preset-management` (last saved loop state, or all bypassed if none).

## Boundaries

- Always: keep coils released when idle; run native tests before commits; keep hardware access behind `RelayPort`.
- Ask first: changing `kRelayPulseMs` or `kMaxSimultaneousRelays`, adding a library, changing pin or I2C address mapping.
- Never: energize SET and RST of one relay together; commit secrets; modify the manufactured hardware design files.

## Success Criteria

- All native unit tests pass; `pio run -e nano_every` builds cleanly.
- On the built unit, each loop can be engaged and bypassed repeatedly with an audible/visible change, and coils are released afterward.
- Power-cycling the unit returns loops to the saved state (or all bypassed if none).
- Switching all eight loops at once (all off to all on) does not reset the Nano Every or glitch the I2C bus.
- Power-up test: over repeated power cycles, no relay clicks or changes state before firmware starts (see Open Questions 1).

## Open Questions

1. Power-up behavior is a hardware risk to test: with no base pull-downs, U3's pins are high-impedance (inputs) at reset, so a base could float until `begin()` runs. If any relay switches at power-up, firmware can't prevent it; the fix would be a hardware change (a pull-down per base, such as 10 kΩ), outside the scope of this spec. A power-cycle test on the built unit decides whether this matters.
2. Hardware test needed to set `kMaxSimultaneousRelays`: measure the 5 V rail with 4 and then 8 relays pulsing together, and confirm the 5 V coil current (derived about 40 mA per coil, not read from the datasheet). Until then the default is 4.

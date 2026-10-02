# Implementation Plan: loop-switching

Implements [SPEC-loop-switching.md](../SPEC-loop-switching.md), module 1 of [CAPABILITY-MAP.md](../CAPABILITY-MAP.md). Tasks are in [todo-loop-switching.md](todo-loop-switching.md). Plan and tasks approved 2026-10-01.

## Overview

Firmware layer that sets each of eight loops engaged or bypassed by pulsing the SET or RST coil of a latching relay through MCP23017 U3 (0x21) on a Nano Every. Logic is written against a hardware-free interface and tested natively; the I2C driver and a bring-up sketch come last, then are verified on the built unit.

## Architecture Decisions

- **Three seams: `Clock`, `I2cBus`, `RelayPort`.** `LoopSwitching` only sees `RelayPort` and `Clock`; `Mcp23017RelayPort` only sees `I2cBus`. Native tests fake all three, so pulse timing, grouping and register writes are all verified off-device.
- **Plain `uint8_t` masks.** A loop state is one byte (bit n = loop n), and a relay drive is a SET mask plus a RST mask. This keeps the interface small for `preset-management` and `operating-modes`.
- **Pin mapping lives in one pure function** (loop index → port, RST bit, SET bit), tested against the spec table.
- **Blocking pulses.** Switching blocks for about 10 ms per group, which is below the perceptible threshold for a footswitch. Non-blocking switching adds state-machine complexity that nothing needs yet.
- **No new library dependencies.** The MCP23017 needs only a handful of registers over `Wire`.
- **`begin()` always pulses all eight relays**, since latching relay positions are unknown at boot.

## Dependency Graph

```
Task 1 scaffold
    └── Task 2 pin mapping + diff logic
            └── Task 3 grouping, timing, errors
                    └── Task 4 begin() boot sequence
Task 1 ── Task 5 MCP23017 driver (needs Task 2 mapping)
                    └── Task 6 bring-up sketch (needs Tasks 4, 5)
                            └── Task 7 hardware verification
```

Tasks 3-4 and Task 5 can proceed in parallel after Task 2.

## Task List

See [todo-loop-switching.md](todo-loop-switching.md) for acceptance criteria and verification.

### Phase 1: Foundation
- [x] Task 1: Project scaffold
- [x] Task 2: Pin mapping and relay drive computation

### Checkpoint: Foundation

### Phase 2: Core behavior
- [x] Task 3: Grouped, timed pulses with error handling
- [x] Task 4: Boot sequence

### Checkpoint: Core behavior

### Phase 3: Hardware
- [x] Task 5: MCP23017 relay port
- [x] Task 6: Bring-up sketch
- [ ] Task 7: Hardware verification

### Checkpoint: Complete

## Risks and Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| Relays switch at power-up (no base pull-downs) | High | Test on hardware in Task 7; if it happens, the fix is a hardware pull-down, outside this module |
| 5 V rail sags when many relays pulse together | High | Default `kMaxSimultaneousRelays` = 4; measure with 4 and 8 in Task 7 before raising it |
| PlatformIO or the `atmelmegaavr` platform isn't installed or builds differently than assumed | Med | Task 1 verifies the toolchain first, before any logic |
| Wrong MCP23017 register or bit behavior | Med | Driver tested with a fake bus asserting exact register writes; confirmed against the datasheet; verified on hardware in Task 7 |
| Pin map errors (loop to coil) | High | One tested function, checked against the spec table, then confirmed physically in Task 7 (loop n clicks relay n) |

## Open Questions

- Pull-down risk and final `kMaxSimultaneousRelays` are resolved only by hardware tests (Task 7).
- Confirm the MCP23017 register layout (IOCON.BANK = 0 default) against the datasheet during Task 5.

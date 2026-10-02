# Tasks: loop-switching

Plan: [plan-loop-switching.md](plan-loop-switching.md). Spec: [SPEC-loop-switching.md](../SPEC-loop-switching.md).

## Phase 1: Foundation

- [x] **Task 1: Project scaffold** (S)
  - Description: PlatformIO project with a `nano_every` env and a `native` env (Unity), and a trivial passing test.
  - Acceptance:
    - [ ] `platformio.ini` defines `nano_every` (board `nano_every`, Arduino framework) and `native` (test only)
    - [ ] A placeholder sketch compiles for the Nano Every
    - [ ] One trivial native test runs
  - Verify: `pio run -e nano_every` and `pio test -e native` both succeed
  - Dependencies: None
  - Files: `platformio.ini`, `src/main.cpp`, `test/test_smoke/test_smoke.cpp`, `.gitignore`

- [x] **Task 2: Pin mapping and relay drive computation** (S)
  - Description: Pure function mapping loop index to U3 port and coil bits, plus a function turning (current mask, target mask) into a `RelayDrive` (SET and RST masks). Tests first.
  - Acceptance:
    - [ ] Loop 1 maps to GPA0 (RST) / GPA1 (SET); loop 4 to GPA6/GPA7; loop 5 to GPB0/GPB1; loop 8 to GPB6/GPB7
    - [ ] Newly engaged loops appear only in SET, newly bypassed only in RST, unchanged in neither
    - [ ] SET and RST masks never overlap
  - Verify: `pio test -e native`
  - Dependencies: Task 1
  - Files: `include/loop_switching/relay_map.h` (header-only, constexpr; no `.cpp` needed), `test/test_relay_map/test_relay_map.cpp`

### Checkpoint: Foundation
- [x] Native tests pass; `pio run -e nano_every` builds
- [x] Mapping table reviewed by a human against the spec (confirmed by the user 2026-10-01: SET engages, RST bypasses)

## Phase 2: Core behavior

- [x] **Task 3: Grouped, timed pulses with error handling** (M)
  - Description: `LoopSwitching::apply(mask)` using `RelayPort` and `Clock`: split the drive into groups of at most `kMaxSimultaneousRelays`, pulse each for `kRelayPulseMs`, release, wait `kRelayGroupGapMs`, update tracked state only after a successful pulse.
  - Acceptance:
    - [ ] Group size never exceeds `kMaxSimultaneousRelays` (default 4)
    - [ ] Coils are released after every group, in order energize → delay → release
    - [ ] Unchanged state causes no port calls
    - [ ] A failed energize or release returns false and leaves tracked state unchanged for the loops not yet completed
  - Verify: `pio test -e native` with fake `RelayPort` and fake `Clock`
  - Dependencies: Task 2
  - Files: `include/loop_switching/loop_switching.h`, `include/loop_switching/relay_port.h`, `include/loop_switching/clock.h`, `src/loop_switching/loop_switching.cpp`, `test/test_loop_switching/test_loop_switching.cpp`

- [x] **Task 4: Boot sequence** (S)
  - Description: `begin(initialMask)` treats relay state as unknown and drives all eight relays to `initialMask` through the same grouped path.
  - Acceptance:
    - [ ] All eight relays are pulsed at boot, even when `initialMask` is 0x00
    - [ ] Tracked state equals `initialMask` after success; unchanged on failure
  - Verify: `pio test -e native`
  - Dependencies: Task 3
  - Files: `src/loop_switching/loop_switching.cpp`, `test/test_loop_switching/test_loop_switching.cpp`

### Checkpoint: Core behavior
- [x] All native tests pass
- [ ] Human reviews `RelayPort` / `Clock` interfaces before the driver is built on them

## Phase 3: Hardware

- [x] **Task 5: MCP23017 relay port** (M)
  - Description: `I2cBus` interface, a `Wire`-backed implementation, and `Mcp23017RelayPort` for U3 (0x21): init (write OLAT 0x00 first, then IODIR = outputs), `energize` (one OLATA/OLATB write per port), `releaseAll`.
  - Acceptance:
    - [x] Init writes latches to 0x00 before setting IODIR to output
    - [x] `energize` writes exact OLATA/OLATB bytes for the given SET/RST masks
    - [x] I2C write failures are returned as false
    - [x] Compiles for the Nano Every
  - Verify: `pio test -e native` (fake bus asserts the exact register sequence); `pio run -e nano_every`
  - Dependencies: Task 2 (mapping)
  - Files: `include/loop_switching/i2c_bus.h`, `include/loop_switching/mcp23017.h`, `include/loop_switching/mcp23017_relay_port.h`, `include/loop_switching/arduino_bus.h` (header-only `WireI2cBus` and `ArduinoClock`), `src/loop_switching/mcp23017_relay_port.cpp`, `test/test_mcp23017/test_mcp23017.cpp`, `test/support/fake_i2c_bus.h`

- [x] **Task 6: Bring-up sketch** (S)
  - Description: `main.cpp` that wires `Wire`, `Mcp23017RelayPort`, `LoopSwitching` and the Arduino clock; boots with all loops bypassed and cycles loops one by one, then in groups, over serial or a fixed pattern, for manual testing.
  - Acceptance:
    - [x] Sketch builds and uploads
    - [x] A build-time option selects the test pattern (loop walk, all-on/all-off)
  - Verify: `pio run -e nano_every`
  - Dependencies: Tasks 4, 5
  - Files: `src/bringup.cpp` (build with `pio run -e bringup`; serial commands listed at the top of the file)

- [ ] **Task 7: Hardware verification** (manual)
  - Description: Run the bring-up sketch on the built unit and record results.
  - Acceptance:
    - [ ] Loop walk: loop n clicks relay n and passes/bypasses audio for all eight loops
    - [ ] Power-cycle test: over at least 20 power cycles, no relay clicks or changes before firmware starts
    - [ ] 5 V rail measured with 4 and with 8 relays pulsing together; `kMaxSimultaneousRelays` set from the result
    - [ ] All-off to all-on does not reset the Nano Every or glitch I2C
    - [ ] Coils not warm after switching
  - Verify: Manual checklist; results written into the spec's Open Questions
  - Dependencies: Task 6
  - Files: `SPEC-loop-switching.md` (results)

### Checkpoint: Complete
- [ ] All spec success criteria met
- [ ] Open Questions in the spec updated with hardware results
- [ ] Review with human before starting `preset-management`

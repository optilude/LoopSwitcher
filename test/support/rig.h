#pragma once

#include "fake_eeprom.h"
#include "fake_hardware.h"
#include "fake_leds.h"
#include "fake_midi_output.h"
#include "loop_switching/loop_switching.h"
#include "operating_modes/performance_controller.h"
#include "preset_management/preset_store.h"

// The real LoopSwitching, PresetStore and PerformanceController over fakes. Several rigs can share
// one FakeEeprom to simulate a reboot.
struct Rig {
    explicit Rig(FakeEeprom& eeprom, MidiOutput* midi = nullptr)
        : store(eeprom), loops(hw, hw), controller(loops, store, leds, midi) {}

    // What main does at startup.
    bool boot() {
        const bool dataValid = store.begin();
        return controller.begin(!dataValid);
    }

    FakeHardware hw;
    PresetStore store;
    LoopSwitching loops;
    FakeLeds leds;
    PerformanceController controller;
};

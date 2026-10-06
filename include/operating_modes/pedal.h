#pragma once

#include <stdint.h>

#include "loop_switching/clock.h"
#include "loop_switching/i2c_bus.h"
#include "loop_switching/loop_switching.h"
#include "loop_switching/mcp23017_relay_port.h"
#include "operating_modes/control_expander.h"
#include "operating_modes/footswitch_input.h"
#include "operating_modes/menu_flow.h"
#include "operating_modes/performance_controller.h"
#include "operating_modes/play_screen.h"
#include "preset_management/eeprom.h"
#include "preset_management/preset_store.h"
#include "ui_framework/event_queue.h"
#include "ui_framework/screen_stack.h"

// The whole pedal behind three interfaces (the I2C bus, a clock and the EEPROM), so that it runs
// the same on the Nano Every and in native tests. The display and the encoder and buttons are
// outside: main passes their events in and renders the screen stack.
class Pedal {
public:
    struct BootStatus {
        bool storageValid;  // false when the EEPROM was blank or damaged and defaults were loaded
        bool relays;        // U3 answered and every relay was driven to its saved state
        bool controls;      // U2 answered, so the footswitches and LEDs work
    };

    Pedal(I2cBus& bus, Clock& clock, Eeprom& eeprom, MidiOutput* midi = nullptr)
        : relays_(bus),
          loops_(relays_, clock),
          expander_(bus),
          switches_(expander_),
          store_(eeprom),
          controller_(loops_, store_, expander_, midi),
          flow_(stack_, store_, controller_),
          play_(controller_, store_, &MenuFlow::openCallback, &flow_) {}

    // Spec requirement 10. The UI comes up even if one of the boards does not answer.
    BootStatus begin(uint32_t nowMs);

    // One main-loop iteration. `interrupt` is true when INT_SWITCH fired. Footswitch presses are
    // handled before button events so that the relays move first.
    void update(bool interrupt, EventQueue& events, uint32_t nowMs);

    ScreenStack& stack() { return stack_; }
    PerformanceController& controller() { return controller_; }
    PresetStore& store() { return store_; }

private:
    void showNotice(uint32_t nowMs);

    Mcp23017RelayPort relays_;
    LoopSwitching loops_;
    ControlExpander expander_;
    FootswitchInput switches_;
    PresetStore store_;
    PerformanceController controller_;
    ScreenStack stack_;
    MenuFlow flow_;
    PlayScreen play_;
};

#pragma once

#include <stdint.h>

#include "loop_switching/loop_switching.h"
#include "operating_modes/led_output.h"
#include "preset_management/preset_store.h"

constexpr uint32_t kLedRetryMs = 100;

enum class Mode : uint8_t { Manual, Preset };

// Something the user should be told about; the main loop turns it into a toast.
enum class Notice : uint8_t { None, Error, DataReset, EmptyPreset };

// The behavior of the pedal: footswitches, modes and LEDs. It knows nothing about the display
// or the menus. LEDs are only ever set from the loop state that LoopSwitching actually tracks.
class PerformanceController {
public:
    static constexpr uint8_t kNone = 0xFF;

    PerformanceController(LoopSwitching& loops, PresetStore& store, LedOutput& leds)
        : loops_(loops), store_(store), leds_(leds) {}

    // Call once after PresetStore::begin(); `dataReset` is true when that call reported defaults.
    // Returns false if the relays could not be driven.
    bool begin(bool dataReset);

    // Footswitch index 0-7 (SW1-SW8).
    void onFootswitch(uint8_t index, uint32_t nowMs);
    // Several footswitches pressed together (bit n = SW(n+1)). In manual mode each loop is toggled
    // in switch order; in preset mode only the highest-numbered press is used.
    void onFootswitches(uint8_t pressedMask, uint32_t nowMs);

    // Switches between manual and preset mode. Entering preset mode applies the active preset.
    void toggleMode(uint32_t nowMs);

    // Call often: saves the state after the idle delay and retries LED writes that failed.
    void tick(uint32_t nowMs);

    Mode mode() const { return mode_; }
    uint8_t loopMask() const { return loops_.stateMask(); }
    uint8_t activePreset() const { return activePreset_; }
    uint8_t lastChangedLoop() const { return lastChangedLoop_; }  // kNone before the first change
    bool lastChangedState() const { return lastChangedState_; }

    // Returns the latest notice once, then None.
    Notice takeNotice();

private:
    // Applies `mask`; the LEDs follow whatever relays actually moved. Reports an error on failure.
    bool applyMask(uint8_t mask, uint32_t nowMs);
    void toggleLoop(uint8_t loop, uint32_t nowMs);
    void selectPreset(uint8_t slot, uint32_t nowMs);
    void updateLeds(uint32_t nowMs);
    void saveState(uint32_t nowMs);

    LoopSwitching& loops_;
    PresetStore& store_;
    LedOutput& leds_;

    Mode mode_ = Mode::Manual;
    uint8_t activePreset_ = 0;
    uint8_t lastChangedLoop_ = kNone;
    bool lastChangedState_ = false;
    Notice notice_ = Notice::None;
    bool ledsStale_ = false;
    uint32_t lastLedTryMs_ = 0;
};

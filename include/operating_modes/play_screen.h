#pragma once

#include "operating_modes/performance_controller.h"
#include "preset_management/preset_store.h"
#include "ui_framework/screen.h"

// The screen shown during play: the mode, and either the last changed loop (manual mode) or the
// active preset (preset mode). The encoder press opens the menu.
class PlayScreen : public Screen {
public:
    using OpenMenuFn = void (*)(void* context);

    PlayScreen(PerformanceController& controller, PresetStore& store, OpenMenuFn openMenu, void* context)
        : controller_(controller), store_(store), openMenu_(openMenu), context_(context) {}

    bool handle(Event event) override;
    void draw(Display& display) override;

private:
    PerformanceController& controller_;
    PresetStore& store_;
    OpenMenuFn openMenu_;
    void* context_;
};

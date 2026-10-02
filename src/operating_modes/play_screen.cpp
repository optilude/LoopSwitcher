#include "operating_modes/play_screen.h"

namespace {
constexpr uint8_t kModeY = 0;
constexpr uint8_t kNameY = 20;
constexpr uint8_t kStateY = 42;
}  // namespace

bool PlayScreen::handle(Event event) {
    if (event != Event::Select) return false;
    openMenu_(context_);
    return true;
}

void PlayScreen::draw(Display& display) {
    char name[kNameMax + 1];

    if (controller_.mode() == Mode::Manual) {
        display.text(0, kModeY, "MANUAL", Font::List, false);
        const uint8_t loop = controller_.lastChangedLoop();
        if (loop == PerformanceController::kNone) return;

        store_.loopLabel(loop, name);
        display.text(0, kNameY, name, Font::Status, false);
        display.text(0, kStateY, controller_.lastChangedState() ? "ON" : "OFF", Font::Status, false);
        return;
    }

    const uint8_t slot = controller_.activePreset();
    const char title[] = {'P', 'R', 'E', 'S', 'E', 'T', ' ', static_cast<char>('1' + slot), '\0'};
    display.text(0, kModeY, title, Font::List, false);
    if (store_.presetUsed(slot)) {
        store_.presetName(slot, name);
        display.text(0, kNameY, name, Font::Status, false);
    } else {
        display.text(0, kNameY, "EMPTY", Font::Status, false);
    }
}

#include "operating_modes/pedal.h"

namespace {
constexpr uint32_t kDataResetToastMs = 2000;
}

void Pedal::showNotice(uint32_t nowMs) {
    switch (controller_.takeNotice()) {
        case Notice::Error: stack_.showToast("ERROR", nowMs); break;
        case Notice::DataReset: stack_.showToast("DATA RESET", nowMs, kDataResetToastMs); break;
        case Notice::EmptyPreset: stack_.showToast("EMPTY", nowMs); break;
        case Notice::None: break;
    }
}

Pedal::BootStatus Pedal::begin(uint32_t nowMs) {
    BootStatus status{};
    status.storageValid = store_.begin();
    status.relays = relays_.begin();
    status.controls = expander_.begin() && switches_.begin(nowMs);

    // Always started: it restores the saved mode and preset even when a board is missing, and it
    // reports a relay failure itself.
    const bool relaysDriven = controller_.begin(!status.storageValid);
    status.relays = status.relays && relaysDriven;

    stack_.push(play_);
    showNotice(nowMs);
    if (!status.controls) stack_.showToast("ERROR", nowMs);
    return status;
}

void Pedal::update(bool interrupt, EventQueue& events, uint32_t nowMs) {
    const uint8_t pressed = switches_.poll(interrupt, nowMs);
    const uint8_t released = switches_.releases();
    if (pressed != 0 || released != 0) {
        flow_.noteInput(nowMs);
        if (pressed != 0) controller_.onFootswitches(pressed, nowMs);
        if (released != 0) controller_.onFootswitchesReleased(released, nowMs);
        stack_.markDirty();
        showNotice(nowMs);
    }

    for (Event e = events.pop(); e != Event::None; e = events.pop()) {
        flow_.noteInput(nowMs);
        if (e == Event::Mode) {
            if (stack_.depth() == 1) controller_.toggleMode(nowMs);  // the menus ignore Mode
            stack_.markDirty();
        } else {
            stack_.handle(e);
        }
        showNotice(nowMs);
    }

    if (controller_.tick(nowMs)) {
        stack_.markDirty();
        showNotice(nowMs);
    }
    flow_.tick(nowMs);
    stack_.tick(nowMs);
}

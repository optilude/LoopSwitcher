#include "operating_modes/performance_controller.h"

void PerformanceController::updateLeds(uint32_t nowMs) {
    ledsStale_ = !leds_.setLeds(loops_.stateMask());
    lastLedTryMs_ = nowMs;
}

void PerformanceController::saveState(uint32_t nowMs) {
    store_.setState(SavedState{loops_.stateMask(), mode_ == Mode::Preset, activePreset_}, nowMs);
}

bool PerformanceController::begin(bool dataReset) {
    const SavedState saved = store_.savedState();
    mode_ = saved.presetMode ? Mode::Preset : Mode::Manual;
    activePreset_ = saved.activePreset;

    uint8_t initial = saved.loopMask;
    if (mode_ == Mode::Preset && store_.presetUsed(activePreset_)) initial = store_.presetMask(activePreset_);
    const bool ok = loops_.begin(initial);

    notice_ = dataReset ? Notice::DataReset : Notice::None;
    if (!ok) {
        notice_ = Notice::Error;
        return false;
    }
    updateLeds(0);
    return true;
}

bool PerformanceController::applyMask(uint8_t mask, uint32_t nowMs) {
    const uint8_t before = loops_.stateMask();
    const bool ok = loops_.apply(mask);

    // The tracked state is the truth even after a partial failure.
    if (loops_.stateMask() != before) {
        updateLeds(nowMs);
        lastChangedLoop_ = kNone;  // the loop shown as "last changed" may no longer be true
    }
    if (!ok) notice_ = Notice::Error;
    return ok;
}

void PerformanceController::toggleLoop(uint8_t loop, uint32_t nowMs) {
    const uint8_t target = static_cast<uint8_t>(loops_.stateMask() ^ (1u << loop));
    if (!applyMask(target, nowMs)) return;

    lastChangedLoop_ = loop;
    lastChangedState_ = (target >> loop) & 1;
    saveState(nowMs);
}

void PerformanceController::selectPreset(uint8_t slot, uint32_t nowMs) {
    if (!store_.presetUsed(slot)) {
        notice_ = Notice::EmptyPreset;
        return;
    }
    if (!applyMask(store_.presetMask(slot), nowMs)) return;

    activePreset_ = slot;
    saveState(nowMs);
}

void PerformanceController::onFootswitch(uint8_t index, uint32_t nowMs) {
    if (index >= kLoopCount) return;
    onFootswitches(static_cast<uint8_t>(1u << index), nowMs);
}

void PerformanceController::onFootswitches(uint8_t pressedMask, uint32_t nowMs) {
    if (mode_ == Mode::Manual) {
        for (uint8_t loop = 0; loop < kLoopCount; ++loop) {
            if (pressedMask & (1u << loop)) toggleLoop(loop, nowMs);
        }
        return;
    }

    for (int slot = kLoopCount - 1; slot >= 0; --slot) {
        if (pressedMask & (1u << slot)) {
            selectPreset(static_cast<uint8_t>(slot), nowMs);
            return;
        }
    }
}

void PerformanceController::toggleMode(uint32_t nowMs) {
    if (mode_ == Mode::Manual) {
        mode_ = Mode::Preset;
        if (store_.presetUsed(activePreset_)) {
            applyMask(store_.presetMask(activePreset_), nowMs);
        } else {
            notice_ = Notice::EmptyPreset;
        }
    } else {
        mode_ = Mode::Manual;
    }
    saveState(nowMs);
}

void PerformanceController::tick(uint32_t nowMs) {
    store_.tick(nowMs);
    if (ledsStale_ && nowMs - lastLedTryMs_ >= kLedRetryMs) updateLeds(nowMs);
}

Notice PerformanceController::takeNotice() {
    const Notice notice = notice_;
    notice_ = Notice::None;
    return notice;
}

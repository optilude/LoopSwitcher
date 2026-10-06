#include "operating_modes/performance_controller.h"

void PerformanceController::updateLeds(uint32_t nowMs) {
    ledsStale_ = !leds_.setLeds(loops_.stateMask());
    lastLedTryMs_ = nowMs;
}

void PerformanceController::saveState(uint32_t nowMs) {
    store_.setState(SavedState{loops_.stateMask(), mode_ == Mode::Preset, activePreset_, mode_ == Mode::Perform}, nowMs);
}

bool PerformanceController::begin(bool dataReset) {
    const SavedState saved = store_.savedState();
    mode_ = saved.performMode ? Mode::Perform : saved.presetMode ? Mode::Preset : Mode::Manual;
    activePreset_ = saved.activePreset;

    // Perform mode keeps the saved loops: they were changed by stomps after the preset was selected.
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

#if LOOPSWITCHER_ENABLE_MIDI
void PerformanceController::sendPresetMidi(uint8_t slot) {
    if (midi_ == nullptr) return;

    const PresetMidi midi = store_.presetMidi(slot);
    if (midi.bankSelectEnabled) {
        midi_->sendControlChange(midi.channel, 0, midi.bankMsb);
        midi_->sendControlChange(midi.channel, 32, midi.bankLsb);
    }
    if (midi.programChangeEnabled) midi_->sendProgramChange(midi.channel, midi.program);
    if (midi.effectCcEnabled) midi_->sendControlChange(midi.channel, midi.effectCc, midi.effectValue);
}
#endif

void PerformanceController::selectPreset(uint8_t slot, uint32_t nowMs) {
    if (!store_.presetUsed(slot)) {
        notice_ = Notice::EmptyPreset;
        return;
    }
    if (!applyMask(store_.presetMask(slot), nowMs)) return;

#if LOOPSWITCHER_ENABLE_MIDI
    sendPresetMidi(slot);
#endif
    activePreset_ = slot;
    saveState(nowMs);
}

void PerformanceController::onFootswitch(uint8_t index, uint32_t nowMs) {
    if (index >= kLoopCount) return;
    onFootswitches(static_cast<uint8_t>(1u << index), nowMs);
}

void PerformanceController::onFootswitches(uint8_t pressedMask, uint32_t nowMs) {
    if (mode_ == Mode::Perform) {
        for (uint8_t loop = 0; loop < kLoopCount; ++loop) {
            if (!(pressedMask & (1u << loop))) continue;
            pending_ |= static_cast<uint8_t>(1u << loop);
            pressedAtMs_[loop] = nowMs;
        }
        return;
    }

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

void PerformanceController::longPress(uint8_t mask, uint32_t nowMs) {
    for (int slot = kLoopCount - 1; slot >= 0; --slot) {
        if (mask & (1u << slot)) {
            selectPreset(static_cast<uint8_t>(slot), nowMs);
            return;
        }
    }
}

void PerformanceController::onFootswitchesReleased(uint8_t releasedMask, uint32_t nowMs) {
    if (mode_ != Mode::Perform) return;

    uint8_t longMask = 0;
    for (uint8_t loop = 0; loop < kLoopCount; ++loop) {
        const uint8_t bit = static_cast<uint8_t>(1u << loop);
        if (!(releasedMask & bit) || !(pending_ & bit)) continue;

        pending_ &= static_cast<uint8_t>(~bit);
        // tick() may not have run since the threshold passed.
        if (nowMs - pressedAtMs_[loop] >= kPerformHoldMs) {
            longMask |= bit;
        } else {
            toggleLoop(loop, nowMs);
        }
    }
    longPress(longMask, nowMs);
}

void PerformanceController::toggleMode(uint32_t nowMs) {
    pending_ = 0;
    if (mode_ == Mode::Perform) {
        mode_ = Mode::Manual;
    } else {
        mode_ = mode_ == Mode::Manual ? Mode::Preset : Mode::Perform;
        if (store_.presetUsed(activePreset_)) {
            if (applyMask(store_.presetMask(activePreset_), nowMs)) {
#if LOOPSWITCHER_ENABLE_MIDI
                sendPresetMidi(activePreset_);
#endif
            }
        } else {
            notice_ = Notice::EmptyPreset;
        }
    }
    saveState(nowMs);
}

bool PerformanceController::tick(uint32_t nowMs) {
    uint8_t longMask = 0;
    if (mode_ == Mode::Perform) {
        for (uint8_t loop = 0; loop < kLoopCount; ++loop) {
            const uint8_t bit = static_cast<uint8_t>(1u << loop);
            if ((pending_ & bit) && nowMs - pressedAtMs_[loop] >= kPerformHoldMs) longMask |= bit;
        }
        pending_ &= static_cast<uint8_t>(~longMask);
        longPress(longMask, nowMs);
    }

    store_.tick(nowMs);
    if (ledsStale_ && nowMs - lastLedTryMs_ >= kLedRetryMs) updateLeds(nowMs);
    return longMask != 0;
}

Notice PerformanceController::takeNotice() {
    const Notice notice = notice_;
    notice_ = Notice::None;
    return notice;
}

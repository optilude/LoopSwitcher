#pragma once

#include <stdint.h>

#include "ui_framework/screen.h"

constexpr uint32_t kToastMs = 1000;
constexpr uint8_t kToastMaxChars = 16;

// Statically sized: screens live elsewhere and are pushed by reference. The first screen
// pushed is the root and is never popped.
class ScreenStack {
public:
    static constexpr uint8_t kMaxDepth = 6;

    bool push(Screen& screen);
    void pop();
    void popToRoot();
    uint8_t depth() const { return depth_; }

    void handle(Event event);

    void showToast(const char* message, uint32_t nowMs, uint32_t durationMs = kToastMs);
    void tick(uint32_t nowMs);

    void markDirty() { dirty_ = true; }
    bool needsRender() const { return dirty_ || frameActive_; }

    // Draws at most one display pass (one page of a page-buffer display) and returns true while
    // the frame is unfinished. Call it once per main-loop iteration so a slow display never
    // blocks input handling. A change made mid-frame is drawn in the next frame.
    bool renderStep(Display& display);

    // Draws one complete frame without returning in between.
    void render(Display& display);

private:
    Screen* screens_[kMaxDepth] = {};
    uint8_t depth_ = 0;
    bool dirty_ = true;
    bool frameActive_ = false;
    bool toastActive_ = false;
    uint32_t toastUntil_ = 0;
    char toast_[kToastMaxChars + 1] = {};
};

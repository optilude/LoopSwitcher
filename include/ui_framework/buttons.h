#pragma once

#include <stdint.h>

#include "ui_framework/event_queue.h"

constexpr uint32_t kDebounceMs = 20;
constexpr uint32_t kLongPressMs = 600;

// One debounced button. A short press is reported on release, a long press once while held.
class Button {
public:
    enum class Result : uint8_t { None, Short, Long };

    Result update(bool pressed, uint32_t nowMs);

private:
    bool raw_ = false;
    bool stable_ = false;
    bool longFired_ = false;
    uint32_t rawSince_ = 0;
    uint32_t pressedAt_ = 0;
};

// The encoder push button, Back and Mode.
class Buttons {
public:
    void update(bool select, bool back, bool mode, uint32_t nowMs, EventQueue& queue);

private:
    Button select_;
    Button back_;
    Button mode_;
};

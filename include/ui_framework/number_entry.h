#pragma once

#include <stdint.h>

#include "ui_framework/screen.h"

// Picks a number from min to max with the encoder: Right counts up, Left counts down, and both
// wrap at the ends. Select (short or long) confirms; Back (short or long) cancels.
// The callbacks decide what happens next, for example popping the screen.
class NumberEntry : public Screen {
public:
    using DoneFn = void (*)(void* context, uint8_t value);
    using CancelFn = void (*)(void* context);

    NumberEntry(DoneFn onDone, CancelFn onCancel, void* context)
        : onDone_(onDone), onCancel_(onCancel), context_(context) {}

    // `value` is clamped to [min, max]; `title` must outlive the screen's use.
    void reset(const char* title, uint8_t value, uint8_t min, uint8_t max);

    uint8_t value() const { return value_; }

    bool handle(Event event) override;
    void draw(Display& display) override;

private:
    DoneFn onDone_;
    CancelFn onCancel_;
    void* context_;
    const char* title_ = "";
    uint8_t value_ = 0;
    uint8_t min_ = 0;
    uint8_t max_ = 0;
};

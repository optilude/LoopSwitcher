#include "ui_framework/buttons.h"

Button::Result Button::update(bool pressed, uint32_t nowMs) {
    if (pressed != raw_) {
        raw_ = pressed;
        rawSince_ = nowMs;
    }

    Result result = Result::None;
    if (raw_ != stable_ && nowMs - rawSince_ >= kDebounceMs) {
        stable_ = raw_;
        if (stable_) {
            pressedAt_ = rawSince_;
            longFired_ = false;
        } else if (!longFired_) {
            result = Result::Short;
        }
    }

    if (stable_ && !longFired_ && nowMs - pressedAt_ >= kLongPressMs) {
        longFired_ = true;
        result = Result::Long;
    }
    return result;
}

namespace {
void push(EventQueue& queue, Button::Result result, Event shortEvent, Event longEvent) {
    if (result == Button::Result::Short) queue.push(shortEvent);
    if (result == Button::Result::Long) queue.push(longEvent);
}
}  // namespace

void Buttons::update(bool select, bool back, bool mode, uint32_t nowMs, EventQueue& queue) {
    push(queue, select_.update(select, nowMs), Event::Select, Event::SelectLong);
    push(queue, back_.update(back, nowMs), Event::Back, Event::BackLong);
    push(queue, mode_.update(mode, nowMs), Event::Mode, Event::ModeLong);
}

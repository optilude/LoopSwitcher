#include "ui_framework/number_entry.h"

#include <stdio.h>

namespace {
constexpr uint8_t kValueY = 20;
constexpr uint8_t kHintY = 48;
}  // namespace

void NumberEntry::reset(const char* title, uint8_t value, uint8_t min, uint8_t max) {
    title_ = title;
    min_ = min;
    max_ = max;
    value_ = value < min ? min : value > max ? max : value;
}

bool NumberEntry::handle(Event event) {
    switch (event) {
        case Event::Right:
            value_ = value_ >= max_ ? min_ : static_cast<uint8_t>(value_ + 1);
            return true;
        case Event::Left:
            value_ = value_ <= min_ ? max_ : static_cast<uint8_t>(value_ - 1);
            return true;
        case Event::Select:
        case Event::SelectLong:
            onDone_(context_, value_);
            return true;
        case Event::Back:
        case Event::BackLong:
            onCancel_(context_);
            return true;
        default:
            return false;
    }
}

void NumberEntry::draw(Display& display) {
    char text[4];
    snprintf(text, sizeof(text), "%u", static_cast<unsigned>(value_));
    display.text(0, 0, title_, Font::List, false);
    display.text(0, kValueY, text, Font::Status, true);
    display.text(0, kHintY, "PRESS=OK", Font::List, false);
}

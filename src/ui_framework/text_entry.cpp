#include "ui_framework/text_entry.h"

namespace {
const char kCharset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -.+/#";
constexpr uint8_t kCharsetSize = sizeof(kCharset) - 1;
constexpr uint8_t kTextY = 8;
constexpr uint8_t kHintY = 48;
}  // namespace

void TextEntry::reset(const char* initial) {
    length_ = 0;
    while (length_ < kTextMax && initial[length_] != '\0') {
        text_[length_] = initial[length_];
        ++length_;
    }
    text_[length_] = '\0';
    position_ = 0;
}

char TextEntry::workingChar() const {
    return position_ == 0 ? '\0' : kCharset[position_ - 1];
}

bool TextEntry::handle(Event event) {
    switch (event) {
        case Event::Right:
            if (length_ < kTextMax) {
                const uint8_t positions = static_cast<uint8_t>(kCharsetSize + 1);
                position_ = static_cast<uint8_t>((position_ + 1) % positions);
            }
            return true;
        case Event::Left:
            if (length_ < kTextMax) {
                const uint8_t positions = static_cast<uint8_t>(kCharsetSize + 1);
                position_ = static_cast<uint8_t>((position_ + positions - 1) % positions);
            }
            return true;
        case Event::Select:
            if (position_ != 0 && length_ < kTextMax) {
                text_[length_++] = workingChar();
                text_[length_] = '\0';
                position_ = 0;
            }
            return true;
        case Event::SelectLong: {
            // The state stays as it is, so editing can go on if the callback refuses the text.
            char confirmed[kTextMax + 1];
            uint8_t n = 0;
            for (; n < length_; ++n) confirmed[n] = text_[n];
            if (position_ != 0 && n < kTextMax) confirmed[n++] = workingChar();
            confirmed[n] = '\0';
            onDone_(context_, confirmed);
            return true;
        }
        case Event::Back:
            if (position_ != 0) {
                position_ = 0;
            } else if (length_ > 0) {
                text_[--length_] = '\0';
            } else {
                onCancel_(context_);
            }
            return true;
        case Event::BackLong:
            onCancel_(context_);
            return true;
        default:
            return false;
    }
}

void TextEntry::draw(Display& display) {
    if (length_ > 0) display.text(0, kTextY, text_, Font::Status, false);
    if (length_ < kTextMax) {
        const char cursor[2] = {position_ == 0 ? '_' : workingChar(), '\0'};
        display.text(static_cast<uint8_t>(length_ * kStatusCharWidth), kTextY, cursor, Font::Status, true);
    }
    display.text(0, kHintY, "HOLD=OK", Font::List, false);
}

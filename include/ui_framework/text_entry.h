#pragma once

#include <stdint.h>

#include "ui_framework/screen.h"

constexpr uint8_t kTextMax = 10;

// Edits up to kTextMax characters with the encoder. The cursor sits after the accepted text and
// shows either "no character" (an inverted `_`) or a character from the set. Rotating moves it
// through `_`, A-Z, 0-9, space, `- . + / #` and back to `_`.
//   Select      accepts the character under the cursor and moves on (nothing on `_`)
//   SelectLong  confirms the text, including the character under the cursor unless it is `_`
//   Back        clears the character under the cursor, then erases the last accepted character,
//               and cancels when there is neither
//   BackLong    cancels
// The callbacks decide what happens next, for example popping the screen.
class TextEntry : public Screen {
public:
    using DoneFn = void (*)(void* context, const char* text);
    using CancelFn = void (*)(void* context);

    TextEntry(DoneFn onDone, CancelFn onCancel, void* context)
        : onDone_(onDone), onCancel_(onCancel), context_(context) {
        reset("");
    }

    // Starts editing `initial` (truncated to kTextMax) with the cursor on "no character".
    void reset(const char* initial);

    // The accepted text only.
    const char* text() const { return text_; }
    bool hasPendingChar() const { return position_ != 0; }
    char workingChar() const;  // '\0' when the cursor shows "no character"

    bool handle(Event event) override;
    void draw(Display& display) override;

private:
    DoneFn onDone_;
    CancelFn onCancel_;
    void* context_;
    char text_[kTextMax + 1] = {};
    uint8_t length_ = 0;
    uint8_t position_ = 0;  // 0 = no character, 1.. = a character of the set
};

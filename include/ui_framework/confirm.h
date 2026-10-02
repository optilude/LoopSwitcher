#pragma once

#include <stdint.h>

#include "ui_framework/screen.h"

// A YES/NO question that defaults to NO. Left highlights YES, Right highlights NO, Select
// chooses, Back means NO. The callback decides what happens next, for example popping the screen.
class Confirm : public Screen {
public:
    using ChoiceFn = void (*)(void* context, bool yes);

    Confirm(const char* question, ChoiceFn onChoice, void* context)
        : question_(question), onChoice_(onChoice), context_(context) {}

    void reset() { yes_ = false; }

    bool handle(Event event) override;
    void draw(Display& display) override;

private:
    const char* question_;
    ChoiceFn onChoice_;
    void* context_;
    bool yes_ = false;
};

#pragma once

#include "ui_framework/display.h"
#include "ui_framework/event.h"

class Screen {
public:
    // Returns true if the event was used. An unused Back pops the screen.
    virtual bool handle(Event event) = 0;
    // May run once per display pass; must not change state.
    virtual void draw(Display& display) = 0;
    virtual ~Screen() = default;
};

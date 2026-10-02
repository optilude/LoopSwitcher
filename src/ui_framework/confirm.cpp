#include "ui_framework/confirm.h"

bool Confirm::handle(Event event) {
    switch (event) {
        case Event::Left:
            yes_ = true;
            return true;
        case Event::Right:
            yes_ = false;
            return true;
        case Event::Select:
            onChoice_(context_, yes_);
            return true;
        case Event::Back:
            onChoice_(context_, false);
            return true;
        default:
            return false;
    }
}

void Confirm::draw(Display& display) {
    display.text(0, 0, question_, Font::List, false);
    display.text(0, 2 * kRowHeight, "YES", Font::List, yes_);
    display.text(0, 3 * kRowHeight, "NO", Font::List, !yes_);
}

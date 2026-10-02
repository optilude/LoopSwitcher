#pragma once

#include <stdint.h>

#include "ui_framework/screen.h"

struct MenuItem {
    const char* label;
    bool enabled;
    void (*onSelect)(void* context);
    void* context;
};

// A scrolling list. The caller owns the item array and the label strings, which must outlive
// the menu; call setItems() after changing them. Back is left to the screen stack.
class Menu : public Screen {
public:
    static constexpr uint8_t kVisibleRows = 4;

    Menu(MenuItem* items, uint8_t count) { setItems(items, count); }

    void setItems(MenuItem* items, uint8_t count);
    uint8_t highlight() const { return highlight_; }
    // Back to the first enabled item with the list scrolled to the top.
    void resetHighlight();

    bool handle(Event event) override;
    void draw(Display& display) override;

private:
    void ensureHighlightVisible();

    MenuItem* items_ = nullptr;
    uint8_t count_ = 0;
    uint8_t highlight_ = 0;
    uint8_t top_ = 0;
};

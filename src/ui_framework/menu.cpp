#include "ui_framework/menu.h"

void Menu::setItems(MenuItem* items, uint8_t count) {
    items_ = items;
    count_ = count;
    if (highlight_ >= count_) highlight_ = count_ == 0 ? 0 : static_cast<uint8_t>(count_ - 1);
    if (count_ > 0 && !items_[highlight_].enabled) {
        for (uint8_t i = 0; i < count_; ++i) {
            if (items_[i].enabled) {
                highlight_ = i;
                break;
            }
        }
    }
    ensureHighlightVisible();
}

void Menu::resetHighlight() {
    highlight_ = 0;
    top_ = 0;
    setItems(items_, count_);
}

bool Menu::handle(Event event) {
    if (event == Event::Right) {
        for (uint8_t i = static_cast<uint8_t>(highlight_ + 1); i < count_; ++i) {
            if (items_[i].enabled) {
                highlight_ = i;
                break;
            }
        }
        ensureHighlightVisible();
        return true;
    }
    if (event == Event::Left) {
        for (int i = highlight_ - 1; i >= 0; --i) {
            if (items_[i].enabled) {
                highlight_ = static_cast<uint8_t>(i);
                break;
            }
        }
        ensureHighlightVisible();
        return true;
    }
    if (event == Event::Select) {
        if (count_ > 0 && items_[highlight_].enabled) {
            items_[highlight_].onSelect(items_[highlight_].context);
        }
        return true;
    }
    return false;
}

void Menu::draw(Display& display) {
    for (uint8_t row = 0; row < kVisibleRows; ++row) {
        const uint8_t index = static_cast<uint8_t>(top_ + row);
        if (index >= count_) break;
        display.text(0, static_cast<uint8_t>(row * kRowHeight), items_[index].label, Font::List,
                     index == highlight_);
    }
}

void Menu::ensureHighlightVisible() {
    if (highlight_ < top_) top_ = highlight_;
    if (highlight_ >= top_ + kVisibleRows) top_ = static_cast<uint8_t>(highlight_ - kVisibleRows + 1);
}

#include "ui_framework/u8g2_display.h"

namespace {
// Both are monospaced: 10 characters take 90 and 100 px. Chosen from the U8g2 font list;
// confirm legibility on the unit (Task 9).
const uint8_t* const kListFont = u8g2_font_9x15_mf;
const uint8_t* const kStatusFont = u8g2_font_10x20_mf;
static_assert(kStatusCharWidth == 10, "kStatusCharWidth must match the status font advance");
}  // namespace

void U8g2Display::begin() {
    u8g2_.begin();
    u8g2_.setBusClock(400000);
    u8g2_.setFontPosTop();
}

void U8g2Display::text(uint8_t x, uint8_t y, const char* s, Font font, bool inverted) {
    u8g2_.setFont(font == Font::Status ? kStatusFont : kListFont);

    // Page-buffer mode redraws every call on all 8 passes and decodes each glyph even when it is
    // clipped away, which dominated the frame time. Skip text outside the current 8 px page.
    const uint8_t height = u8g2_.getMaxCharHeight();
    const uint8_t pageTop = static_cast<uint8_t>(u8g2_.getBufferCurrTileRow() * 8);
    if (y + height <= pageTop || y >= pageTop + 8) return;

    if (inverted) {
        u8g2_.setDrawColor(1);
        u8g2_.drawBox(x, y, u8g2_.getStrWidth(s), u8g2_.getMaxCharHeight());
        u8g2_.setDrawColor(0);
        u8g2_.drawStr(x, y, s);
        u8g2_.setDrawColor(1);
    } else {
        u8g2_.drawStr(x, y, s);
    }
}

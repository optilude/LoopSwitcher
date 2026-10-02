#pragma once

#include <stdint.h>

constexpr uint8_t kScreenWidth = 128;
constexpr uint8_t kScreenHeight = 64;
constexpr uint8_t kRowHeight = 16;
// Advance of the monospaced status font; text entry places its cursor with it.
constexpr uint8_t kStatusCharWidth = 10;

enum class Font : uint8_t { List, Status };

// With a page-buffer display one frame is drawn in several passes, so everything drawn between
// firstPage() and the last nextPage() must come out identical on every pass.
class Display {
public:
    virtual void firstPage() = 0;
    // Returns true while another pass is needed.
    virtual bool nextPage() = 0;
    // (x, y) is the top-left corner of the text; an inverted row is drawn light on dark.
    virtual void text(uint8_t x, uint8_t y, const char* s, Font font, bool inverted) = 0;
    virtual ~Display() = default;
};

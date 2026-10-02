#pragma once

#include <U8g2lib.h>

#include "ui_framework/display.h"

// SH1106 128x64 on I2C (address 0x3C), drawn in page-buffer mode to save RAM.
class U8g2Display : public Display {
public:
    void begin();

    void firstPage() override { u8g2_.firstPage(); }
    bool nextPage() override {
        const uint32_t start = micros();
        const bool more = u8g2_.nextPage();
        sendUs += micros() - start;
        return more;
    }
    void text(uint8_t x, uint8_t y, const char* s, Font font, bool inverted) override;

    // Time spent sending pages to the display since it was last cleared (for benchmarking).
    uint32_t sendUs = 0;

private:
    U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2_{U8G2_R0, U8X8_PIN_NONE};
};

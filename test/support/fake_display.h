#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "ui_framework/display.h"

struct Drawn {
    uint8_t x, y;
    std::string text;
    Font font;
    bool inverted;
};

// Records what the last frame drew (only the final pass is kept) and how many passes it took.
class FakeDisplay : public Display {
public:
    int passesPerFrame = 1;
    int framesStarted = 0;
    int lastFramePasses = 0;
    std::vector<Drawn> drawn;
    std::vector<std::vector<Drawn>> finishedPasses;  // what each pass of the current frame drew

    void firstPage() override {
        ++framesStarted;
        pass_ = 0;
        lastFramePasses = 1;
        drawn.clear();
        finishedPasses.clear();
    }

    bool nextPage() override {
        finishedPasses.push_back(drawn);
        if (++pass_ < passesPerFrame) {
            ++lastFramePasses;
            drawn.clear();
            return true;
        }
        return false;
    }

    bool passShows(size_t pass, const char* s) const {
        if (pass >= finishedPasses.size()) return false;
        for (const Drawn& d : finishedPasses[pass]) {
            if (d.text == s) return true;
        }
        return false;
    }

    void text(uint8_t x, uint8_t y, const char* s, Font font, bool inverted) override {
        drawn.push_back({x, y, s, font, inverted});
    }

    bool shows(const char* s) const {
        for (const Drawn& d : drawn) {
            if (d.text == s) return true;
        }
        return false;
    }

private:
    int pass_ = 0;
};

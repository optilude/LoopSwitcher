#pragma once

#include <stdint.h>

// Decodes one detent per call to update() that lands on the rest state.
// state = (A << 1) | B with pins read as 1 = high; the detent rests at 0b11.
// A falling first counts as +1; swap A and B at the call site if the encoder turns the wrong way.
class QuadratureDecoder {
public:
    // Fewer than this many valid transitions between rests counts as bounce or a half turn.
    static constexpr int8_t kMinTransitionsPerStep = 2;

    // Returns +1 or -1 when a detent was completed, otherwise 0.
    int8_t update(uint8_t state) {
        state &= 0b11;
        if (state == prev_) return 0;

        acc_ = static_cast<int8_t>(acc_ + kTransition[(prev_ << 2) | state]);
        prev_ = state;
        if (state != kRest) return 0;

        int8_t step = 0;
        if (acc_ >= kMinTransitionsPerStep) step = 1;
        if (acc_ <= -kMinTransitionsPerStep) step = -1;
        acc_ = 0;
        return step;
    }

private:
    static constexpr uint8_t kRest = 0b11;
    // Indexed by (previous << 2) | next; invalid (skipped or bounced) transitions are 0.
    static constexpr int8_t kTransition[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

    uint8_t prev_ = kRest;
    int8_t acc_ = 0;
};

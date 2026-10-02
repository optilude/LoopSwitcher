#include "ui_framework/arduino_input.h"

#include "ui_framework/quadrature.h"

namespace {
QuadratureDecoder gDecoder;
volatile int8_t gPendingSteps = 0;

// Runs on every edge of A or B; only decodes and counts, the rest happens in poll().
void onEncoderChange() {
    const uint8_t state = static_cast<uint8_t>((digitalRead(kEncoderAPin) << 1) | digitalRead(kEncoderBPin));
    gPendingSteps = static_cast<int8_t>(gPendingSteps + gDecoder.update(state));
}
}  // namespace

void ArduinoInput::begin() {
    pinMode(kEncoderAPin, INPUT_PULLUP);
    pinMode(kEncoderBPin, INPUT_PULLUP);
    pinMode(kSelectPin, INPUT_PULLUP);
    pinMode(kModePin, INPUT_PULLUP);
    pinMode(kBackPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(kEncoderAPin), onEncoderChange, CHANGE);
    attachInterrupt(digitalPinToInterrupt(kEncoderBPin), onEncoderChange, CHANGE);
}

void ArduinoInput::poll(uint32_t nowMs, EventQueue& queue) {
    noInterrupts();
    const int8_t steps = gPendingSteps;
    gPendingSteps = 0;
    interrupts();

    for (int8_t i = 0; i < steps; ++i) queue.push(Event::Right);
    for (int8_t i = 0; i > steps; --i) queue.push(Event::Left);

    buttons_.update(digitalRead(kSelectPin) == LOW, digitalRead(kBackPin) == LOW,
                    digitalRead(kModePin) == LOW, nowMs, queue);
}

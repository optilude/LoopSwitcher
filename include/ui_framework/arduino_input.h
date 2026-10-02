#pragma once

#include <Arduino.h>

#include "ui_framework/buttons.h"
#include "ui_framework/event_queue.h"

// Control PCB pins (see SPEC-loop-switching.md, Hardware Reference). Buttons are active low.
constexpr uint8_t kEncoderAPin = 3;
constexpr uint8_t kEncoderBPin = 4;
constexpr uint8_t kSelectPin = 5;  // encoder push
constexpr uint8_t kModePin = 6;
constexpr uint8_t kBackPin = 7;

// If the encoder turns the wrong way, swap kEncoderAPin and kEncoderBPin.
class ArduinoInput {
public:
    void begin();
    // Turns encoder steps and button presses into events; call it often from loop().
    void poll(uint32_t nowMs, EventQueue& queue);

private:
    Buttons buttons_;
};

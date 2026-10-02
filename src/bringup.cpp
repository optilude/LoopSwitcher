// Bring-up sketch for the audio PCB's relays (115200 baud, one command per line):
//   1-8      toggle that loop
//   a, o     all loops on, all loops off (a big current step: watch the 5 V rail)
//   w        walk: engage each loop in turn for 1.5 s
//   g        groups: alternate masks 0xAA and 0x55 five times, so many relays switch at once
//   m <hex>  apply a loop mask, e.g. "m 81"
//   s        print the tracked state
// Every command prints whether the I2C writes succeeded and how long the switching took.
#include <Arduino.h>
#include <stdlib.h>

#include "loop_switching/arduino_bus.h"
#include "loop_switching/loop_switching.h"
#include "loop_switching/mcp23017_relay_port.h"

namespace {

WireI2cBus bus;
ArduinoClock clock;
Mcp23017RelayPort port(bus);
LoopSwitching loops(port, clock);

void apply(uint8_t mask) {
    const uint32_t start = millis();
    const bool ok = loops.apply(mask);
    Serial.print(ok ? F("ok") : F("FAILED"));
    Serial.print(F("  state 0x"));
    Serial.print(loops.stateMask(), HEX);
    Serial.print(F("  took "));
    Serial.print(millis() - start);
    Serial.println(F(" ms"));
}

void handle(char* line) {
    switch (line[0]) {
        case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8':
            apply(loops.stateMask() ^ static_cast<uint8_t>(1u << (line[0] - '1')));
            break;
        case 'a':
            apply(0xFF);
            break;
        case 'o':
            apply(0x00);
            break;
        case 'm':
            apply(static_cast<uint8_t>(strtol(line + 1, nullptr, 16)));
            break;
        case 'w':
            for (uint8_t i = 0; i < kLoopCount; ++i) {
                Serial.print(F("loop "));
                Serial.print(i + 1);
                Serial.print(F(": "));
                apply(static_cast<uint8_t>(1u << i));
                delay(1500);
            }
            apply(0x00);
            break;
        case 'g':
            for (uint8_t i = 0; i < 5; ++i) {
                apply(0xAA);
                delay(800);
                apply(0x55);
                delay(800);
            }
            apply(0x00);
            break;
        case 's':
            Serial.print(F("state 0x"));
            Serial.println(loops.stateMask(), HEX);
            break;
        default:
            Serial.println(F("unknown command"));
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(1500);  // let a serial monitor attach before the boot messages
    bus.begin();

    if (port.begin()) {
        Serial.println(F("U3 (0x21) answered; latches cleared, pins are outputs"));
    } else {
        Serial.println(F("U3 (0x21) did not answer: check the audio PCB connection and power"));
    }
    Serial.println(loops.begin(0x00) ? F("boot: every relay driven to bypass") : F("boot: FAILED"));
}

void loop() {
    static char line[24];
    static uint8_t length = 0;
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\r') continue;
        if (c == '\n') {
            line[length] = '\0';
            if (length > 0) handle(line);
            length = 0;
        } else if (length < sizeof(line) - 1) {
            line[length++] = c;
        }
    }
}

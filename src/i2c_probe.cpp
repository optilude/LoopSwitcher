// Live bring-up tool for the audio PCB's two expanders and the display (115200 baud). It rescans
// the I2C bus twice a second, prints whenever the set of devices changes, and once U2 (0x20)
// answers it shows the footswitches live and drives the LEDs:
//   1-8   toggle that LED (LED n = loop n)
//   a, o  all LEDs on, all LEDs off
// Handy while checking power, address pins and solder joints with a multimeter.
#include <Arduino.h>
#include <stdlib.h>

#include "arduino_watchdog.h"
#include "loop_switching/arduino_bus.h"
#include "operating_modes/control_expander.h"

namespace {

constexpr uint8_t kIntSwitchPin = 2;

WireI2cBus bus;
ControlExpander expander(bus);

uint8_t foundCount = 0;
uint8_t found[16];
bool u2Ready = false;
uint8_t leds = 0;
int lastIntLevel = -1;

bool has(uint8_t address) {
    for (uint8_t i = 0; i < foundCount; ++i) {
        if (found[i] == address) return true;
    }
    return false;
}

void scan(uint8_t* list, uint8_t& count) {
    count = 0;
    for (uint8_t address = 0x08; address < 0x78 && count < 16; ++address) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0) list[count++] = address;
    }
}

void printHex(uint8_t value) {
    Serial.print(F("0x"));
    if (value < 0x10) Serial.print('0');
    Serial.print(value, HEX);
}

void report() {
    Serial.print(F("devices:"));
    if (foundCount == 0) Serial.print(F(" none"));
    for (uint8_t i = 0; i < foundCount; ++i) {
        Serial.print(' ');
        printHex(found[i]);
    }
    Serial.println();
    Serial.println(has(0x20) ? F("  0x20 U2 footswitches and LEDs: answers")
                             : F("  0x20 U2 footswitches and LEDs: NO ANSWER"));
    Serial.println(has(0x21) ? F("  0x21 U3 relays: answers") : F("  0x21 U3 relays: NO ANSWER"));
    Serial.println(has(0x3C) ? F("  0x3C display: answers") : F("  0x3C display: no answer"));
}

void printLeds() {
    Serial.print(F("LEDs: "));
    for (uint8_t i = 0; i < 8; ++i) {
        Serial.print((leds >> i) & 1 ? static_cast<char>('1' + i) : '-');
        Serial.print(' ');
    }
    Serial.println();
}

void setLeds(uint8_t mask) {
    leds = mask;
    if (!u2Ready) {
        Serial.println(F("U2 is not answering"));
        return;
    }
    Serial.println(expander.setLeds(leds) ? F("ok") : F("LED write FAILED"));
    printLeds();
}

}  // namespace

void setup() {
    watchdogStart();
    Serial.begin(115200);
    delay(1500);
    bus.begin(100000);  // slow and forgiving while looking for faults
    pinMode(kIntSwitchPin, INPUT_PULLUP);
    Serial.println(F("i2c_probe: rescanning every 500 ms"));
}

void loop() {
    watchdogPet();

    static uint32_t lastScanMs = 0;
    if (millis() - lastScanMs >= 500) {
        lastScanMs = millis();
        uint8_t list[16];
        uint8_t count;
        scan(list, count);

        bool changed = count != foundCount;
        for (uint8_t i = 0; !changed && i < count; ++i) changed = list[i] != found[i];
        for (uint8_t i = 0; i < count; ++i) found[i] = list[i];
        foundCount = count;
        if (changed || lastScanMs < 600) report();

        const bool u2Answers = has(0x20);
        if (u2Answers && !u2Ready) {
            u2Ready = expander.begin();
            Serial.println(u2Ready ? F("U2 configured: press a footswitch") : F("U2 answers but configuring it FAILED"));
            leds = 0;
        } else if (!u2Answers && u2Ready) {
            u2Ready = false;
            Serial.println(F("U2 stopped answering"));
        }
    }

    if (u2Ready) {
        static uint8_t lastPressed = 0xFF;
        static uint32_t lastReadMs = 0;
        if (millis() - lastReadMs >= 20) {
            lastReadMs = millis();
            uint8_t pressed = 0;
            if (expander.readSwitches(pressed) && pressed != lastPressed) {
                lastPressed = pressed;
                Serial.print(F("footswitches down: "));
                for (uint8_t i = 0; i < 8; ++i) {
                    Serial.print((pressed >> i) & 1 ? static_cast<char>('1' + i) : '.');
                    Serial.print(' ');
                }
                Serial.println();
            }
        }
    }

    const int intLevel = digitalRead(kIntSwitchPin);
    if (intLevel != lastIntLevel) {
        lastIntLevel = intLevel;
        Serial.print(F("INT_SWITCH (D2): "));
        Serial.println(intLevel ? F("high (idle)") : F("LOW (U2 is signalling a change)"));
    }

    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c >= '1' && c <= '8') setLeds(leds ^ static_cast<uint8_t>(1u << (c - '1')));
        if (c == 'a') setLeds(0xFF);
        if (c == 'o') setLeds(0x00);
    }
}

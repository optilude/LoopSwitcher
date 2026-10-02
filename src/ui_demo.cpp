// Hardware demo for the control board: exercises the menu, text entry, confirm dialog, toast and
// font preview, and prints events and render times to serial (115200 baud).
#include <Arduino.h>
#include <Wire.h>

#include "ui_framework/arduino_input.h"
#include "ui_framework/confirm.h"
#include "ui_framework/menu.h"
#include "ui_framework/screen_stack.h"
#include "ui_framework/text_entry.h"
#include "ui_framework/u8g2_display.h"

namespace {

U8g2Display display;
ArduinoInput input;
EventQueue queue;
ScreenStack stack;

class FontScreen : public Screen {
public:
    bool handle(Event) override { return false; }
    void draw(Display& d) override {
        d.text(0, 0, "WWWWWWWWWW", Font::List, false);
        d.text(0, kRowHeight, "0123456789", Font::List, false);
        d.text(0, 36, "WWWWWWWWWW", Font::Status, false);
    }
};

void onTextDone(void*, const char* text);
void onTextCancel(void*);
void onChoice(void*, bool yes);

FontScreen fontScreen;
TextEntry textEntry(onTextDone, onTextCancel, nullptr);
Confirm confirm("DELETE?", onChoice, nullptr);

void onTextDone(void*, const char* text) {
    stack.pop();
    stack.showToast(text[0] ? text : "EMPTY", millis());
}
void onTextCancel(void*) {
    stack.pop();
    stack.showToast("CANCELLED", millis());
}
void onChoice(void*, bool yes) {
    stack.pop();
    stack.showToast(yes ? "YES" : "NO", millis());
}

void openText(void*) {
    textEntry.reset("");
    stack.push(textEntry);
}
void openConfirm(void*) {
    confirm.reset();
    stack.push(confirm);
}
void openFonts(void*) { stack.push(fontScreen); }
void showToast(void*) { stack.showToast("HELLO", millis()); }

MenuItem items[] = {
    {"TEXT ENTRY", true, openText, nullptr},
    {"CONFIRM", true, openConfirm, nullptr},
    {"FONTS", true, openFonts, nullptr},
    {"TOAST", true, showToast, nullptr},
    {"DISABLED", false, showToast, nullptr},
};
Menu menu(items, sizeof(items) / sizeof(items[0]));

const char* eventName(Event e) {
    switch (e) {
        case Event::Left: return "Left";
        case Event::Right: return "Right";
        case Event::Select: return "Select";
        case Event::SelectLong: return "SelectLong";
        case Event::Back: return "Back";
        case Event::BackLong: return "BackLong";
        case Event::Mode: return "Mode";
        case Event::ModeLong: return "ModeLong";
        default: return "None";
    }
}

}  // namespace

bool i2cOk = false;

// Reads a pin without and then with the internal pull-up. A healthy unconnected pin reads
// 0 or 1 floating and 1 pulled up; a pin held low reads 0 both ways.
void printLevels(const char* name, uint8_t pin) {
    pinMode(pin, INPUT);
    delay(2);
    const int floating = digitalRead(pin);
    pinMode(pin, INPUT_PULLUP);
    delay(2);
    const int pulledUp = digitalRead(pin);
    Serial.print(name);
    Serial.print(F(": floating="));
    Serial.print(floating);
    Serial.print(F(" pullup="));
    Serial.println(pulledUp);
}

// Returns true when at least one device answers. Progress is printed, so a hang is visible.
bool scanI2c() {
    // The I2C pins are SDA/SCL (PA2/PA3); A4/A5 are different MCU pins (PF2/PF3). Show all four.
    printLevels("SDA (PA2)", SDA);
    printLevels("SCL (PA3)", SCL);
    printLevels("A4  (PF2)", A4);
    printLevels("A5  (PF3)", A5);

    Serial.println(F("I2C scan starting"));
    Wire.begin();
    Wire.setClock(100000);
    uint8_t found = 0;
    for (uint8_t address = 0x08; address < 0x78; ++address) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0) {
            Serial.print(F("I2C device at 0x"));
            Serial.println(address, HEX);
            ++found;
        }
    }
    Serial.print(F("I2C scan done, devices found: "));
    Serial.println(found);
    return found > 0;
}

// Times 50 transfers of 31 bytes (a control byte plus 30 no-op commands, harmless to the display)
// at several bus clocks, to see what speed the Wire library really delivers.
void benchI2c(uint8_t address) {
    const uint32_t clocks[] = {100000, 400000, 800000, 1000000};
    for (uint8_t c = 0; c < sizeof(clocks) / sizeof(clocks[0]); ++c) {
        Wire.setClock(clocks[c]);
        uint8_t failures = 0;
        const uint32_t start = micros();
        for (uint8_t i = 0; i < 50; ++i) {
            Wire.beginTransmission(address);
            Wire.write(static_cast<uint8_t>(0x00));  // control byte: command stream
            for (uint8_t b = 0; b < 30; ++b) Wire.write(static_cast<uint8_t>(0xE3));  // NOP
            if (Wire.endTransmission() != 0) ++failures;
        }
        const uint32_t perTransfer = (micros() - start) / 50;
        Serial.print(F("bench "));
        Serial.print(clocks[c] / 1000);
        Serial.print(F(" kHz: "));
        Serial.print(perTransfer);
        Serial.print(F(" us per 31-byte transfer, failures "));
        Serial.println(failures);
    }
}

// Every usable pin with a pull-up, so a press shows up even if a control is not on the pin we expect.
const uint8_t kScanPins[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, A0, A1, A2, A3};
uint16_t lastScan = 0xFFFF;

void printPinName(uint8_t pin) {
    if (pin >= A0) {
        Serial.print('A');
        Serial.print(pin - A0);
    } else {
        Serial.print('D');
        Serial.print(pin);
    }
}

void initPinScan() {
    for (uint8_t i = 0; i < sizeof(kScanPins); ++i) pinMode(kScanPins[i], INPUT_PULLUP);
}

void logRawPins() {
    uint16_t now = 0;
    for (uint8_t i = 0; i < sizeof(kScanPins); ++i) now |= static_cast<uint16_t>(digitalRead(kScanPins[i])) << i;
    if (now == lastScan) return;
    for (uint8_t i = 0; i < sizeof(kScanPins); ++i) {
        const bool before = (lastScan >> i) & 1;
        const bool after = (now >> i) & 1;
        if (lastScan != 0xFFFF && before == after) continue;
        printPinName(kScanPins[i]);
        Serial.print('=');
        Serial.print(after);
        Serial.print(' ');
    }
    Serial.println();
    lastScan = now;
}

void setup() {
    Serial.begin(115200);
    delay(1500);  // let the serial monitor attach before the boot messages
    input.begin();
    initPinScan();
    i2cOk = scanI2c();
    if (i2cOk) {
        benchI2c(0x3C);
        display.begin();
    }
    stack.push(menu);
    Serial.println(F("ui_demo ready; first line lists every pin's idle level, then only changes"));
}

void loop() {
    const uint32_t now = millis();
    logRawPins();
    input.poll(now, queue);

    for (Event e = queue.pop(); e != Event::None; e = queue.pop()) {
        Serial.println(eventName(e));
        stack.handle(e);
    }
    stack.tick(now);

    // One display page per loop iteration, so a slow bus never blocks the inputs above.
    static uint32_t frameUs = 0;
    static uint32_t longestStepUs = 0;
    static uint8_t steps = 0;
    if (i2cOk && stack.needsRender()) {
        const uint32_t start = micros();
        const bool more = stack.renderStep(display);
        const uint32_t took = micros() - start;
        frameUs += took;
        ++steps;
        if (took > longestStepUs) longestStepUs = took;
        if (!more) {
            Serial.print(F("frame us: "));
            Serial.print(frameUs);
            Serial.print(F(" in "));
            Serial.print(steps);
            Serial.print(F(" steps, longest step us: "));
            Serial.print(longestStepUs);
            Serial.print(F(", of which sending us: "));
            Serial.println(display.sendUs);
            display.sendUs = 0;
            frameUs = longestStepUs = 0;
            steps = 0;
        }
    }
}

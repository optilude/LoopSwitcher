// The finished UI on the control board alone: real OLED, encoder, Mode/Back buttons and EEPROM, with
// the audio PCB simulated. Open a serial monitor (115200 baud) and type:
//   1-8   a footswitch press (SW1-SW8); no Enter needed
//   s     print the mode, active preset and loop state
// The relays always "succeed" and the LEDs are printed on the serial line.
#include <Arduino.h>

#include "loop_switching/loop_switching.h"
#include "operating_modes/menu_flow.h"
#include "operating_modes/performance_controller.h"
#include "operating_modes/play_screen.h"
#include "preset_management/arduino_eeprom.h"
#include "preset_management/preset_store.h"
#include "ui_framework/arduino_input.h"
#include "ui_framework/screen_stack.h"
#include "ui_framework/u8g2_display.h"

namespace {

class SimulatedRelays : public RelayPort, public Clock {
public:
    bool energize(RelayDrive) override { return true; }
    bool releaseAll() override { return true; }
    void delayMs(uint32_t) override {}
};

class SerialLeds : public LedOutput {
public:
    bool setLeds(uint8_t mask) override {
        Serial.print(F("LEDs: "));
        for (uint8_t i = 0; i < 8; ++i) {
            Serial.print((mask >> i) & 1 ? static_cast<char>('1' + i) : '-');
            Serial.print(' ');
        }
        Serial.println();
        return true;
    }
};

U8g2Display display;
ArduinoInput input;
EventQueue queue;
ScreenStack stack;
ArduinoEeprom eeprom;
PresetStore store(eeprom);
SimulatedRelays relays;
LoopSwitching loops(relays, relays);
SerialLeds leds;
PerformanceController controller(loops, store, leds);
MenuFlow flow(stack, store, controller);
PlayScreen play(controller, store, &MenuFlow::openCallback, &flow);

void showNotice() {
    switch (controller.takeNotice()) {
        case Notice::Error: stack.showToast("ERROR", millis()); break;
        case Notice::DataReset: stack.showToast("DATA RESET", millis()); break;
        case Notice::EmptyPreset: stack.showToast("EMPTY", millis()); break;
        default: break;
    }
}

void printState() {
    Serial.print(controller.mode() == Mode::Manual ? F("manual") : controller.mode() == Mode::Preset ? F("preset") : F("perform"));
    Serial.print(F(" mode, active preset "));
    Serial.print(controller.activePreset() + 1);
    Serial.print(F(", loops 0x"));
    Serial.println(controller.loopMask(), HEX);
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(1500);  // let a serial monitor attach before the boot messages
    display.begin();
    input.begin();

    const bool dataValid = store.begin();
    controller.begin(!dataValid);
    stack.push(play);
    showNotice();
    Serial.println(dataValid ? F("stored data is valid") : F("storage was blank: defaults loaded"));
    printState();
}

void loop() {
    const uint32_t now = millis();

    input.poll(now, queue);
    for (Event e = queue.pop(); e != Event::None; e = queue.pop()) {
        flow.noteInput(now);
        if (e == Event::Mode) {
            if (stack.depth() == 1) controller.toggleMode(now);  // the menus ignore Mode
            stack.markDirty();
        } else {
            stack.handle(e);
        }
        showNotice();
    }

    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c >= '1' && c <= '8') {
            flow.noteInput(now);
            controller.onFootswitch(static_cast<uint8_t>(c - '1'), now);
            stack.markDirty();
            showNotice();
        } else if (c == 's') {
            printState();
        }
    }

    controller.tick(now);
    flow.tick(now);
    stack.tick(now);
    stack.renderStep(display);
}

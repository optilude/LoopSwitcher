// The loop switcher firmware: wires the pedal's logic (see operating_modes/pedal.h) to the Nano
// Every's hardware. MIDI builds use the UART TX pin for MIDI; non-MIDI builds report boot status.
#include <Arduino.h>

#include "arduino_watchdog.h"
#include "loop_switching/arduino_bus.h"
#include "operating_modes/pedal.h"
#include "operating_modes/midi_output.h"
#include "preset_management/arduino_eeprom.h"
#include "ui_framework/arduino_input.h"
#include "ui_framework/u8g2_display.h"

namespace {

constexpr uint8_t kIntSwitchPin = 2;  // INT_SWITCH, active low from U2's INTA

#if LOOPSWITCHER_ENABLE_MIDI
class SerialMidiOutput final : public MidiOutput {
public:
    void sendControlChange(uint8_t channel, uint8_t controller, uint8_t value) override {
        Serial.write(static_cast<uint8_t>(0xB0 | channel));
        Serial.write(controller);
        Serial.write(value);
    }

    void sendProgramChange(uint8_t channel, uint8_t program) override {
        Serial.write(static_cast<uint8_t>(0xC0 | channel));
        Serial.write(program);
    }
};

SerialMidiOutput midiOutput;
#endif

WireI2cBus bus;
ArduinoClock clock;
ArduinoEeprom eeprom;
#if LOOPSWITCHER_ENABLE_MIDI
Pedal pedal(bus, clock, eeprom, &midiOutput);
#else
Pedal pedal(bus, clock, eeprom);
#endif
U8g2Display display;
ArduinoInput input;
EventQueue events;

volatile bool switchInterrupt = false;
void onSwitchInterrupt() { switchInterrupt = true; }

bool takeSwitchInterrupt() {
    noInterrupts();
    const bool fired = switchInterrupt;
    switchInterrupt = false;
    interrupts();
    return fired;
}

Pedal::BootStatus bootStatus{};
#if !LOOPSWITCHER_ENABLE_MIDI
bool bootReported = false;
bool resetByWatchdog = false;

void reportBoot() {
    Serial.println(resetByWatchdog ? F("restarted by the watchdog: an I2C line was stuck") : F("started"));
    Serial.println(bootStatus.storageValid ? F("storage: valid") : F("storage: blank or damaged, defaults loaded"));
    Serial.println(bootStatus.relays ? F("relays (U3): ok") : F("relays (U3): not driven; check the audio PCB"));
    Serial.println(bootStatus.controls ? F("footswitches and LEDs (U2): ok") : F("footswitches and LEDs (U2): no answer; check the audio PCB"));
}
#endif

}  // namespace

void setup() {
#if !LOOPSWITCHER_ENABLE_MIDI
    resetByWatchdog = watchdogCausedLastReset();
#endif
    watchdogStart();  // from here a stuck bus resets the unit instead of freezing it

#if LOOPSWITCHER_ENABLE_MIDI
    Serial.begin(31250);
#else
    Serial.begin(115200);
#endif
    bus.begin();
    display.begin();
    input.begin();
    pinMode(kIntSwitchPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(kIntSwitchPin), onSwitchInterrupt, FALLING);

    bootStatus = pedal.begin(millis());
}

void loop() {
    watchdogPet();
    const uint32_t now = millis();

    input.poll(now, events);
    pedal.update(takeSwitchInterrupt(), events, now);
    pedal.stack().renderStep(display);

#if !LOOPSWITCHER_ENABLE_MIDI
    if (!bootReported && now > 1500) {  // by now a serial monitor has had time to attach
        bootReported = true;
        reportBoot();
    }
#endif
}

// The loop switcher firmware: wires the pedal's logic (see operating_modes/pedal.h) to the Nano
// Every's hardware. Serial (115200 baud) only reports the boot result and watchdog resets.
#include <Arduino.h>

#include "arduino_watchdog.h"
#include "loop_switching/arduino_bus.h"
#include "operating_modes/pedal.h"
#include "preset_management/arduino_eeprom.h"
#include "ui_framework/arduino_input.h"
#include "ui_framework/u8g2_display.h"

namespace {

constexpr uint8_t kIntSwitchPin = 2;  // INT_SWITCH, active low from U2's INTA

WireI2cBus bus;
ArduinoClock clock;
ArduinoEeprom eeprom;
Pedal pedal(bus, clock, eeprom);
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
bool bootReported = false;
bool resetByWatchdog = false;

void reportBoot() {
    Serial.println(resetByWatchdog ? F("restarted by the watchdog: an I2C line was stuck") : F("started"));
    Serial.println(bootStatus.storageValid ? F("storage: valid") : F("storage: blank or damaged, defaults loaded"));
    Serial.println(bootStatus.relays ? F("relays (U3): ok") : F("relays (U3): not driven; check the audio PCB"));
    Serial.println(bootStatus.controls ? F("footswitches and LEDs (U2): ok") : F("footswitches and LEDs (U2): no answer; check the audio PCB"));
}

}  // namespace

void setup() {
    resetByWatchdog = watchdogCausedLastReset();
    watchdogStart();  // from here a stuck bus resets the unit instead of freezing it

    Serial.begin(115200);
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

    if (!bootReported && now > 1500) {  // by now a serial monitor has had time to attach
        bootReported = true;
        reportBoot();
    }
}

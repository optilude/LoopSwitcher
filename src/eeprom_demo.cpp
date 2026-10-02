// Serial test sketch for preset-management on the real EEPROM (115200 baud, one command per line):
//   d                     dump the EEPROM length, labels, presets and saved state
//   l <1-8> <name>        set a loop label ("l 3" with no name clears it)
//   p <1-8> <hex> <name>  save a preset with a hex loop mask, e.g. "p 1 05 CLEAN"
//   n <1-8> <name>        rename a preset
//   m <1-8> <hex>         set a preset's loop mask
//   x <1-8>               delete a preset
//   s <hex> <0|1> <0-7>   set the state (mask, preset mode, preset); saved after the idle delay
//   f                     flush a pending state now
//   w                     wipe the EEPROM to 0xFF, then run begin() again
#include <Arduino.h>
#include <stdio.h>
#include <stdlib.h>

#include "preset_management/arduino_eeprom.h"
#include "preset_management/preset_store.h"

namespace {

ArduinoEeprom eeprom;
PresetStore store(eeprom);

void report(bool ok) { Serial.println(ok ? F("ok") : F("rejected")); }

void dump() {
    Serial.print(F("EEPROM length: "));
    Serial.println(EEPROM.length());

    char name[kNameMax + 1];
    for (uint8_t i = 0; i < kLabelCount; ++i) {
        store.loopLabel(i, name);
        Serial.print(F("loop "));
        Serial.print(i + 1);
        Serial.print(F(": "));
        Serial.println(name);
    }
    for (uint8_t slot = 0; slot < kPresetCount; ++slot) {
        Serial.print(F("preset "));
        Serial.print(slot + 1);
        Serial.print(F(": "));
        if (!store.presetUsed(slot)) {
            Serial.println(F("(empty)"));
            continue;
        }
        store.presetName(slot, name);
        Serial.print(name);
        Serial.print(F("  mask 0x"));
        Serial.println(store.presetMask(slot), HEX);
    }
    const SavedState state = store.savedState();
    Serial.print(F("saved state: mask 0x"));
    Serial.print(state.loopMask, HEX);
    Serial.print(state.presetMode ? F(" preset mode, preset ") : F(" manual mode, preset "));
    Serial.println(state.activePreset + 1);
}

void begin() {
    Serial.println(store.begin() ? F("begin: stored data is valid") : F("begin: data was blank or damaged, defaults loaded"));
}

// Parses " <text>" after a number; returns the text (possibly empty) with the leading space skipped.
const char* rest(const char* p) {
    while (*p == ' ') ++p;
    return p;
}

void handle(char* line) {
    const char cmd = line[0];
    char* p = line + 1;
    char* end = nullptr;

    switch (cmd) {
        case 'd':
            dump();
            break;
        case 'l': {
            const long loop = strtol(p, &end, 10);
            report(store.setLoopLabel(static_cast<uint8_t>(loop - 1), rest(end)));
            break;
        }
        case 'p': {
            const long slot = strtol(p, &end, 10);
            const long mask = strtol(end, &end, 16);
            report(store.savePreset(static_cast<uint8_t>(slot - 1), rest(end), static_cast<uint8_t>(mask)));
            break;
        }
        case 'n': {
            const long slot = strtol(p, &end, 10);
            report(store.renamePreset(static_cast<uint8_t>(slot - 1), rest(end)));
            break;
        }
        case 'm': {
            const long slot = strtol(p, &end, 10);
            const long mask = strtol(end, &end, 16);
            report(store.setPresetMask(static_cast<uint8_t>(slot - 1), static_cast<uint8_t>(mask)));
            break;
        }
        case 'x': {
            const long slot = strtol(p, &end, 10);
            report(store.deletePreset(static_cast<uint8_t>(slot - 1)));
            break;
        }
        case 's': {
            const long mask = strtol(p, &end, 16);
            const long mode = strtol(end, &end, 10);
            const long preset = strtol(end, &end, 10);
            store.setState(SavedState{static_cast<uint8_t>(mask), mode != 0, static_cast<uint8_t>(preset)}, millis());
            Serial.println(F("pending; saved after the idle delay, or send f"));
            break;
        }
        case 'f':
            store.flush();
            Serial.println(F("flushed"));
            break;
        case 'w':
            for (uint16_t a = 0; a < EEPROM.length(); ++a) EEPROM.update(a, 0xFF);
            Serial.println(F("EEPROM wiped"));
            begin();
            break;
        default:
            Serial.println(F("unknown command"));
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(1500);  // let a serial monitor attach before the boot messages
    begin();
    dump();
}

void loop() {
    store.tick(millis());

    static char line[40];
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

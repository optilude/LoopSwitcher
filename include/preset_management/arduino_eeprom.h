#pragma once

#include <EEPROM.h>

#include "preset_management/eeprom.h"

class ArduinoEeprom : public Eeprom {
public:
    uint8_t read(uint16_t addr) const override { return EEPROM.read(addr); }
    void update(uint16_t addr, uint8_t value) override { EEPROM.update(addr, value); }
};

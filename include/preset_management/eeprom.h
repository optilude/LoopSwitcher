#pragma once

#include <stdint.h>

class Eeprom {
public:
    virtual uint8_t read(uint16_t addr) const = 0;
    // Writes only if the stored value differs, so unchanged bytes cost no wear.
    virtual void update(uint16_t addr, uint8_t value) = 0;
    virtual ~Eeprom() = default;
};

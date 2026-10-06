#pragma once

#include <stdint.h>

#ifndef LOOPSWITCHER_ENABLE_MIDI
#define LOOPSWITCHER_ENABLE_MIDI 1
#endif

class MidiOutput {
public:
    virtual void sendControlChange(uint8_t channel, uint8_t controller, uint8_t value) = 0;
    virtual void sendProgramChange(uint8_t channel, uint8_t program) = 0;
    virtual ~MidiOutput() = default;
};
#pragma once

#include <stdint.h>

#include <vector>

#include "operating_modes/midi_output.h"

enum class MidiMessageType : uint8_t { ControlChange, ProgramChange };

struct MidiMessage {
    MidiMessageType type;
    uint8_t channel;
    uint8_t data1;
    uint8_t data2;
};

class FakeMidiOutput : public MidiOutput {
public:
    void sendControlChange(uint8_t channel, uint8_t controller, uint8_t value) override {
        messages.push_back({MidiMessageType::ControlChange, channel, controller, value});
    }

    void sendProgramChange(uint8_t channel, uint8_t program) override {
        messages.push_back({MidiMessageType::ProgramChange, channel, program, 0});
    }

    std::vector<MidiMessage> messages;
};
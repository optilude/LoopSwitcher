#pragma once

#include <stdint.h>

// EEPROM layout of the ATmega4809's 256 bytes; see SPEC-preset-management.md.
constexpr uint16_t kEepromSize = 256;
constexpr uint8_t kLayoutVersion = 0x02;

constexpr uint8_t kNameMax = 10;
constexpr uint8_t kLabelCount = 8;
constexpr uint8_t kPresetCount = 8;
constexpr uint8_t kPresetMidiDataSize = 5;
constexpr uint8_t kPresetRecordSize = kNameMax + 1 + 1 + kPresetMidiDataSize;  // name, mask, flags, MIDI data

constexpr uint16_t kVersionAddr = 0;
constexpr uint16_t kCrcAddr = 1;
constexpr uint16_t kLabelsAddr = 3;
constexpr uint16_t kPresetsAddr = kLabelsAddr + kLabelCount * kNameMax;
constexpr uint16_t kConfigEnd = kPresetsAddr + kPresetCount * kPresetRecordSize;  // CRC covers [kLabelsAddr, kConfigEnd)

constexpr uint8_t kRingRecords = 9;
constexpr uint8_t kRingRecordSize = 4;  // seq, loop mask, mode and preset, check
constexpr uint16_t kRingAddr = 220;

static_assert(kPresetsAddr == 83, "labels occupy 80 bytes from offset 3");
static_assert(kConfigEnd == 219, "presets occupy 136 bytes from offset 83");
static_assert(kConfigEnd <= kRingAddr, "configuration must not overlap the state ring");
static_assert(kConfigEnd + 1 == kRingAddr, "one reserved byte separates configuration and state");
static_assert(kRingAddr + kRingRecords * kRingRecordSize == kEepromSize, "the state ring ends at the last byte");

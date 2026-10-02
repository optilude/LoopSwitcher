#pragma once

#include <stdint.h>

// EEPROM layout of the ATmega4809's 256 bytes; see SPEC-preset-management.md.
constexpr uint16_t kEepromSize = 256;
constexpr uint8_t kLayoutVersion = 0x01;

constexpr uint8_t kNameMax = 10;
constexpr uint8_t kLabelCount = 8;
constexpr uint8_t kPresetCount = 8;
constexpr uint8_t kPresetRecordSize = kNameMax + 1;  // name + loop mask

constexpr uint16_t kVersionAddr = 0;
constexpr uint16_t kCrcAddr = 1;
constexpr uint16_t kLabelsAddr = 3;
constexpr uint16_t kPresetsAddr = kLabelsAddr + kLabelCount * kNameMax;
constexpr uint16_t kConfigEnd = kPresetsAddr + kPresetCount * kPresetRecordSize;  // CRC covers [kLabelsAddr, kConfigEnd)

constexpr uint8_t kRingRecords = 20;
constexpr uint8_t kRingRecordSize = 4;  // seq, loop mask, mode and preset, check
constexpr uint16_t kRingAddr = 176;

static_assert(kPresetsAddr == 83, "labels occupy 80 bytes from offset 3");
static_assert(kConfigEnd == 171, "presets occupy 88 bytes from offset 83");
static_assert(kConfigEnd <= kRingAddr, "configuration must not overlap the state ring");
static_assert(kRingAddr + kRingRecords * kRingRecordSize == kEepromSize, "the state ring ends at the last byte");

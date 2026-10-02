#include "preset_management/preset_store.h"

#include "preset_management/crc16.h"

namespace {

// Copies `name` into `out` without trailing spaces. Returns false if it is too long or has a
// character outside printable ASCII. `length` receives the trimmed length.
bool validateName(const char* name, char out[kNameMax + 1], uint8_t& length) {
    if (name == nullptr) return false;

    uint8_t count = 0;
    for (; name[count] != '\0'; ++count) {
        if (count == kNameMax) return false;
        const uint8_t c = static_cast<uint8_t>(name[count]);
        if (c < 0x20 || c > 0x7E) return false;
        out[count] = name[count];
    }
    while (count > 0 && out[count - 1] == ' ') --count;
    out[count] = '\0';
    length = count;
    return true;
}

}  // namespace

uint16_t PresetStore::computeConfigCrc() const {
    uint16_t crc = kCrcInit;
    for (uint16_t addr = kLabelsAddr; addr < kConfigEnd; ++addr) crc = crc16Update(crc, eeprom_.read(addr));
    return crc;
}

void PresetStore::writeConfigCrc() {
    const uint16_t crc = computeConfigCrc();
    eeprom_.update(kCrcAddr, static_cast<uint8_t>(crc & 0xFF));
    eeprom_.update(kCrcAddr + 1, static_cast<uint8_t>(crc >> 8));
}

void PresetStore::resetConfiguration() {
    for (uint16_t addr = kLabelsAddr; addr < kConfigEnd; ++addr) eeprom_.update(addr, 0);
    eeprom_.update(kVersionAddr, kLayoutVersion);
    writeConfigCrc();
}

bool PresetStore::begin() {
    ring_.scan();
    lastSaved_ = savedState();
    hasPending_ = false;
    const uint16_t stored = static_cast<uint16_t>(eeprom_.read(kCrcAddr) | (eeprom_.read(kCrcAddr + 1) << 8));
    if (eeprom_.read(kVersionAddr) == kLayoutVersion && stored == computeConfigCrc()) return true;

    resetConfiguration();
    return false;
}

void PresetStore::loopLabel(uint8_t loop, char out[kNameMax + 1]) const {
    if (loop >= kLabelCount) {
        out[0] = '\0';
        return;
    }

    const uint16_t base = kLabelsAddr + loop * kNameMax;
    if (eeprom_.read(base) == 0) {
        const char fallback[] = {'L', 'o', 'o', 'p', ' ', static_cast<char>('1' + loop), '\0'};
        for (uint8_t i = 0; i < sizeof(fallback); ++i) out[i] = fallback[i];
        return;
    }

    uint8_t i = 0;
    for (; i < kNameMax; ++i) {
        const uint8_t c = eeprom_.read(base + i);
        if (c == 0) break;
        out[i] = static_cast<char>(c);
    }
    out[i] = '\0';
}

bool PresetStore::hasLoopLabel(uint8_t loop) const {
    return loop < kLabelCount && eeprom_.read(kLabelsAddr + loop * kNameMax) != 0;
}

bool PresetStore::setLoopLabel(uint8_t loop, const char* name) {
    char trimmed[kNameMax + 1];
    uint8_t length = 0;
    if (loop >= kLabelCount || !validateName(name, trimmed, length)) return false;

    const uint16_t base = kLabelsAddr + loop * kNameMax;
    for (uint8_t i = 0; i < kNameMax; ++i) {
        eeprom_.update(base + i, i < length ? static_cast<uint8_t>(trimmed[i]) : 0);
    }
    writeConfigCrc();
    return true;
}

namespace {
uint16_t presetBase(uint8_t slot) { return kPresetsAddr + slot * kPresetRecordSize; }
}  // namespace

bool PresetStore::presetUsed(uint8_t slot) const {
    return slot < kPresetCount && eeprom_.read(presetBase(slot)) != 0;
}

void PresetStore::presetName(uint8_t slot, char out[kNameMax + 1]) const {
    uint8_t i = 0;
    if (presetUsed(slot)) {
        for (; i < kNameMax; ++i) {
            const uint8_t c = eeprom_.read(presetBase(slot) + i);
            if (c == 0) break;
            out[i] = static_cast<char>(c);
        }
    }
    out[i] = '\0';
}

uint8_t PresetStore::presetMask(uint8_t slot) const {
    return presetUsed(slot) ? eeprom_.read(presetBase(slot) + kNameMax) : 0;
}

bool PresetStore::savePreset(uint8_t slot, const char* name, uint8_t mask) {
    char trimmed[kNameMax + 1];
    uint8_t length = 0;
    if (slot >= kPresetCount || !validateName(name, trimmed, length) || length == 0) return false;

    const uint16_t base = presetBase(slot);
    for (uint8_t i = 0; i < kNameMax; ++i) {
        eeprom_.update(base + i, i < length ? static_cast<uint8_t>(trimmed[i]) : 0);
    }
    eeprom_.update(base + kNameMax, mask);
    writeConfigCrc();
    return true;
}

bool PresetStore::renamePreset(uint8_t slot, const char* name) {
    if (!presetUsed(slot)) return false;
    return savePreset(slot, name, presetMask(slot));
}

bool PresetStore::setPresetMask(uint8_t slot, uint8_t mask) {
    if (!presetUsed(slot)) return false;
    eeprom_.update(presetBase(slot) + kNameMax, mask);
    writeConfigCrc();
    return true;
}

bool PresetStore::deletePreset(uint8_t slot) {
    if (!presetUsed(slot)) return false;
    for (uint8_t i = 0; i < kPresetRecordSize; ++i) eeprom_.update(presetBase(slot) + i, 0);
    writeConfigCrc();
    return true;
}

SavedState PresetStore::savedState() const {
    SavedState state{0, false, 0};
    ring_.load(state);
    return state;
}

void PresetStore::setState(const SavedState& state, uint32_t nowMs) {
    if (state == lastSaved_) {
        hasPending_ = false;
        return;
    }
    // Repeating the pending state must not restart the delay.
    if (!hasPending_ || state != pending_) changedAtMs_ = nowMs;
    pending_ = state;
    hasPending_ = true;
}

void PresetStore::tick(uint32_t nowMs) {
    if (hasPending_ && nowMs - changedAtMs_ >= kIdleSaveMs) flush();
}

void PresetStore::flush() {
    if (!hasPending_) return;
    ring_.append(pending_);
    lastSaved_ = pending_;
    hasPending_ = false;
}

#include "preset_management/state_ring.h"

namespace {
constexpr uint8_t kCheckSalt = 0xA5;
constexpr uint8_t kPresetModeBit = 0x80;
constexpr uint8_t kPresetIndexMask = 0x07;
constexpr uint8_t kUnusedBits = 0x78;

uint16_t slotAddr(uint8_t slot) { return kRingAddr + slot * kRingRecordSize; }

uint8_t checkByte(uint8_t seq, uint8_t mask, uint8_t modePreset) {
    return static_cast<uint8_t>(seq ^ mask ^ modePreset ^ kCheckSalt);
}
}  // namespace

bool StateRing::readRecord(uint8_t slot, uint8_t& seq, SavedState& state) const {
    const uint16_t base = slotAddr(slot);
    seq = eeprom_.read(base);
    const uint8_t mask = eeprom_.read(base + 1);
    const uint8_t modePreset = eeprom_.read(base + 2);
    if (eeprom_.read(base + 3) != checkByte(seq, mask, modePreset)) return false;
    if (modePreset & kUnusedBits) return false;

    state.loopMask = mask;
    state.presetMode = (modePreset & kPresetModeBit) != 0;
    state.activePreset = modePreset & kPresetIndexMask;
    return true;
}

void StateRing::scan() {
    hasRecord_ = false;
    for (uint8_t slot = 0; slot < kRingRecords; ++slot) {
        uint8_t seq = 0;
        SavedState state{};
        if (!readRecord(slot, seq, state)) continue;

        // A record is newer when its sequence number is 1 to 127 ahead (modulo 256).
        const uint8_t ahead = static_cast<uint8_t>(seq - newestSeq_);
        if (!hasRecord_ || (ahead >= 1 && ahead <= 127)) {
            hasRecord_ = true;
            newestSlot_ = slot;
            newestSeq_ = seq;
            newest_ = state;
        }
    }
}

bool StateRing::load(SavedState& out) const {
    if (!hasRecord_) return false;
    out = newest_;
    return true;
}

void StateRing::append(const SavedState& state) {
    const uint8_t slot = hasRecord_ ? static_cast<uint8_t>((newestSlot_ + 1) % kRingRecords) : 0;
    const uint8_t seq = hasRecord_ ? static_cast<uint8_t>(newestSeq_ + 1) : 0;
    const uint8_t modePreset = static_cast<uint8_t>((state.presetMode ? kPresetModeBit : 0) |
                                                    (state.activePreset & kPresetIndexMask));

    const uint16_t base = slotAddr(slot);
    eeprom_.update(base, seq);
    eeprom_.update(base + 1, state.loopMask);
    eeprom_.update(base + 2, modePreset);
    eeprom_.update(base + 3, checkByte(seq, state.loopMask, modePreset));

    hasRecord_ = true;
    newestSlot_ = slot;
    newestSeq_ = seq;
    newest_ = SavedState{state.loopMask, state.presetMode, static_cast<uint8_t>(state.activePreset & kPresetIndexMask)};
}

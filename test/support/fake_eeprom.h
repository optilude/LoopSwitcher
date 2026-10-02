#pragma once

#include <stdint.h>

#include <vector>

#include "preset_management/eeprom.h"

// A 256-byte EEPROM that starts erased (0xFF) and counts real writes per cell.
class FakeEeprom : public Eeprom {
public:
    static constexpr uint16_t kSize = 256;

    FakeEeprom() : cells_(kSize, 0xFF), writes_(kSize, 0) {}

    uint8_t read(uint16_t addr) const override {
        if (addr >= kSize) {
            ++outOfRangeAccesses;
            return 0xFF;
        }
        return cells_[addr];
    }

    void update(uint16_t addr, uint8_t value) override {
        if (addr >= kSize) {
            ++outOfRangeAccesses;
            return;
        }
        if (cells_[addr] == value) return;
        cells_[addr] = value;
        ++writes_[addr];
    }

    uint32_t writeCount(uint16_t addr) const { return writes_[addr]; }

    uint32_t totalWrites() const {
        uint32_t total = 0;
        for (uint32_t w : writes_) total += w;
        return total;
    }

    uint32_t maxWritesPerCell() const {
        uint32_t most = 0;
        for (uint32_t w : writes_) most = w > most ? w : most;
        return most;
    }

    void resetWriteCounts() { writes_.assign(kSize, 0); }

    mutable uint32_t outOfRangeAccesses = 0;

private:
    std::vector<uint8_t> cells_;
    std::vector<uint32_t> writes_;
};

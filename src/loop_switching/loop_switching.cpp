#include "loop_switching/loop_switching.h"

bool LoopSwitching::begin(uint8_t initialMask) {
    const uint8_t previous = state_;
    state_ = static_cast<uint8_t>(~initialMask);  // makes every loop differ
    if (apply(initialMask)) return true;
    state_ = previous;
    return false;
}

bool LoopSwitching::apply(uint8_t mask) {
    const RelayDrive drive = computeDrive(state_, mask);
    uint8_t pending = static_cast<uint8_t>(drive.setMask | drive.rstMask);
    bool firstGroup = true;

    while (pending != 0) {
        uint8_t group = 0;
        uint8_t count = 0;
        for (uint8_t loop = 0; loop < kLoopCount && count < kMaxSimultaneousRelays; ++loop) {
            const uint8_t bit = static_cast<uint8_t>(1u << loop);
            if (pending & bit) {
                group |= bit;
                ++count;
            }
        }

        if (!firstGroup) clock_.delayMs(kRelayGroupGapMs);
        firstGroup = false;

        const RelayDrive groupDrive{static_cast<uint8_t>(drive.setMask & group),
                                    static_cast<uint8_t>(drive.rstMask & group)};
        if (!port_.energize(groupDrive)) {
            port_.releaseAll();  // a partial write may have energized one port
            return false;
        }
        clock_.delayMs(kRelayPulseMs);
        if (!port_.releaseAll()) return false;

        state_ = static_cast<uint8_t>((state_ & ~group) | (mask & group));
        pending = static_cast<uint8_t>(pending & ~group);
    }
    return true;
}

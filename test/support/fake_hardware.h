#pragma once

#include <stdint.h>

#include <vector>

#include "loop_switching/clock.h"
#include "loop_switching/relay_port.h"

struct HwEvent {
    enum Type { Energize, Release, Delay } type;
    RelayDrive drive;
    uint32_t ms;
};

// Records every port and clock call in order so tests can assert on the sequence.
class FakeHardware : public RelayPort, public Clock {
public:
    std::vector<HwEvent> events;
    int energizeCalls = 0;
    int releaseCalls = 0;
    int failEnergizeOnCall = 0;  // 1-based; 0 = never fail
    int failReleaseOnCall = 0;

    bool energize(RelayDrive drive) override {
        events.push_back({HwEvent::Energize, drive, 0});
        ++energizeCalls;
        return energizeCalls != failEnergizeOnCall;
    }

    bool releaseAll() override {
        events.push_back({HwEvent::Release, {0, 0}, 0});
        ++releaseCalls;
        return releaseCalls != failReleaseOnCall;
    }

    void delayMs(uint32_t ms) override { events.push_back({HwEvent::Delay, {0, 0}, ms}); }
};

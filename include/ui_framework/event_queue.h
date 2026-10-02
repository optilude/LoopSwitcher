#pragma once

#include <stdint.h>

#include "ui_framework/event.h"

// Fixed-size FIFO; a full queue drops its oldest event. Not interrupt-safe.
class EventQueue {
public:
    static constexpr uint8_t kCapacity = 8;

    void push(Event event) {
        if (count_ == kCapacity) {
            head_ = static_cast<uint8_t>((head_ + 1) % kCapacity);
            --count_;
        }
        items_[(head_ + count_) % kCapacity] = event;
        ++count_;
    }

    Event pop() {
        if (count_ == 0) return Event::None;
        const Event event = items_[head_];
        head_ = static_cast<uint8_t>((head_ + 1) % kCapacity);
        --count_;
        return event;
    }

private:
    Event items_[kCapacity] = {};
    uint8_t head_ = 0;
    uint8_t count_ = 0;
};

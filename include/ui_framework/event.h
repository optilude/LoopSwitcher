#pragma once

#include <stdint.h>

enum class Event : uint8_t {
    None,
    Left,
    Right,
    Select,
    SelectLong,
    Back,
    BackLong,
    Mode,
    ModeLong,
};

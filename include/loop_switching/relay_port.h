#pragma once

#include <stdint.h>

#include "loop_switching/relay_map.h"

class RelayPort {
public:
    // Drives the coils in `drive` high; returns false on a bus error.
    virtual bool energize(RelayDrive drive) = 0;
    virtual bool releaseAll() = 0;
    virtual ~RelayPort() = default;
};

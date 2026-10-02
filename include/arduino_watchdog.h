#pragma once

#include <avr/io.h>

// The ATmega4809 watchdog runs from its own 1 kHz oscillator, so it still fires when the CPU is
// stuck waiting on a bus.
inline void watchdogStart() { _PROTECTED_WRITE(WDT.CTRLA, WDT_PERIOD_2KCLK_gc); }  // about 2 s
inline void watchdogPet() { __asm__ __volatile__("wdr"); }

// True if the last reset was the watchdog's; clears the flag.
inline bool watchdogCausedLastReset() {
    const bool caused = (RSTCTRL.RSTFR & RSTCTRL_WDRF_bm) != 0;
    RSTCTRL.RSTFR = 0xFF;
    return caused;
}

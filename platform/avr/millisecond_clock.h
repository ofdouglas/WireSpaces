/*
 * @file  millisecond_clock.h
 * @brief Timer0-backed millisecond clock for ATmega328P-class AVR targets.
 */

#pragma once

namespace wirespaces::platform::avr {

/**
 * @brief Start Timer0 compare-match interrupts at approximately 1 kHz.
 *
 * Call once near the beginning of main(), before interrupts are enabled.
 * Binds wirespaces::hal::MillisecondClock::now() to the Timer0 ISR.
 */
void millisecondClockInit() noexcept;

} // namespace wirespaces::platform::avr

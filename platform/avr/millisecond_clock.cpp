/*
 * @file  millisecond_clock.cpp
 * @brief Timer0 ISR and HAL binding for the AVR millisecond clock.
 */

#include <platform/avr/millisecond_clock.h>

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/atomic.h>

#include <cstdint>

#include <wirespaces/hal/clock.h>

namespace {

volatile std::uint32_t g_milliseconds{0U};

} // namespace

ISR(TIMER0_COMPA_vect) {
    ++g_milliseconds;
}

namespace wirespaces::platform::avr {

void millisecondClockInit() noexcept {
    TCCR0A = _BV(WGM01);
    TCCR0B = _BV(CS01) | _BV(CS00);
    OCR0A = 249U;
    TIMSK0 = _BV(OCIE0A);
}

} // namespace wirespaces::platform::avr

wirespaces::hal::MillisecondClock::TimePoint
wirespaces::hal::MillisecondClock::now() noexcept {
    std::uint32_t result{0U};
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        result = g_milliseconds;
    }
    return result;
}

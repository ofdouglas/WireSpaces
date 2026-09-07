/*
 * @file  builtin_led.h
 * @brief Software PWM for the Arduino Uno built-in LED on D13/PB5.
 */

#pragma once

#include <avr/io.h>
#include <util/atomic.h>

#include <cstdint>

namespace wirespaces::platform::avr {

/**
 * @brief Configure PB5 for software PWM driven by Timer 1 interrupts.
 *
 * Call once near the beginning of main(), before interrupts are enabled.
 */
inline void builtinLedInit() noexcept {
    DDRB |= _BV(DDB5);
    PORTB &= static_cast<std::uint8_t>(~_BV(PORTB5));
    TCCR1A = _BV(WGM10);
    TCCR1B = _BV(WGM12) | _BV(CS11) | _BV(CS10);
    OCR1A = 0U;
    TIMSK1 = 0U;
}

/**
 * @brief Apply 8-bit ratiometric brightness to the Uno LED on D13/PB5.
 *
 * Zero and 255 are driven statically to guarantee exact 0% and 100%
 * endpoints. PB5 is not a hardware PWM output, so Timer 1 compare and
 * overflow interrupts generate approximately 977 Hz PWM in software.
 *
 * @param[in] context Unused callback context.
 * @param[in] brightness Target brightness from 0 to 255.
 */
inline void setBuiltinLedBrightness(void* context,
                                    std::uint8_t brightness) noexcept {
    (void)context;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        TIMSK1 = 0U;
        if (brightness == 0U) {
            PORTB &= static_cast<std::uint8_t>(~_BV(PORTB5));
        } else if (brightness == 0xFFU) {
            PORTB |= _BV(PORTB5);
        } else {
            OCR1A = brightness;
            TCNT1 = 0U;
            PORTB |= _BV(PORTB5);
            TIFR1 = _BV(OCF1A) | _BV(TOV1);
            TIMSK1 = _BV(OCIE1A) | _BV(TOIE1);
        }
    }
}

} // namespace wirespaces::platform::avr

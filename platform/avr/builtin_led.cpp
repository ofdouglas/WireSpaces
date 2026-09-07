/*
 * @file  builtin_led.cpp
 * @brief Timer 1 ISRs for the Arduino Uno built-in LED software PWM.
 */

#include <platform/avr/builtin_led.h>

#include <avr/interrupt.h>
#include <avr/io.h>

ISR(TIMER1_COMPA_vect) {
    PORTB &= static_cast<std::uint8_t>(~_BV(PORTB5));
}

ISR(TIMER1_OVF_vect) {
    PORTB |= _BV(PORTB5);
}

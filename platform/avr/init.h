/**
 * @file init.h
 * @brief WireSpaces AVR platform initialization.
 */

#pragma once

#include <platform/avr/builtin_led.h>
#include <platform/avr/mcp2515.h>
#include <platform/avr/millisecond_clock.h>
#include <platform/avr/spi0.h>
#include <platform/avr/stack_monitor.h>
#include <platform/avr/uart0.h>

namespace wirespaces::platform::avr {

void initialize(std::uint32_t baud_rate) noexcept {
    uart0Init(baud_rate);
    spi0Init();
    millisecondClockInit();
    builtinLedInit();   
}

} // namespace wirespaces::platform::avr
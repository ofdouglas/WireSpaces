/*
 * @file  uart0.h
 * @brief Blocking USART0 driver for ATmega328P-class AVR targets.
 */

#pragma once

#include <avr/io.h>

#include <wirespaces/foundation/span.h>

#include <cstdint>

namespace wirespaces::platform::avr {

/**
 * @brief Configure USART0 for 8N1 at the requested baud rate.
 *
 * Uses double-speed mode with an F_CPU-derived divider.
 *
 * @param[in] baud_rate Target baud rate in bits per second.
 */
inline void uart0Init(std::uint32_t baud_rate) noexcept {
    const std::uint16_t baud_divider{static_cast<std::uint16_t>(
        (F_CPU / (8UL * baud_rate)) - 1UL)};
    UBRR0H = static_cast<std::uint8_t>(baud_divider >> 8U);
    UBRR0L = static_cast<std::uint8_t>(baud_divider);
    UCSR0A = _BV(U2X0);
    UCSR0B = _BV(RXEN0) | _BV(TXEN0);
    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
}

/**
 * @brief Block until USART0 can accept another byte, then transmit it.
 *
 * @param[in] byte Byte to send.
 */
inline void uart0WriteByte(std::uint8_t byte) noexcept {
    while ((UCSR0A & _BV(UDRE0)) == 0U) {
    }

    UDR0 = byte;
}

/**
 * @brief Send a span of Bytes, blocking.
 *
 * @param[in] data Span of bytes to send.
 */
inline void uart0WriteSpan(wirespaces::foundation::Span<const uint8_t> data) noexcept {
    for (const auto byte : data) {
        uart0WriteByte(byte);
    }
}

/**
 * @brief Write a hexadecimal nibble to the USART0.
 *
 * @param[in] nibble Nibble to send.
 */
inline void uart0WriteHexNibble(std::uint8_t nibble) noexcept {
    static constexpr char kHexDigits[]{'0', '1', '2', '3', '4', '5', '6', '7',
                                       '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    wirespaces::platform::avr::uart0WriteByte(
        static_cast<std::uint8_t>(kHexDigits[nibble & 0x0FU]));
}

/**
 * @brief Write a hexadecimal byte to the USART0.
 *
 * @param[in] value Byte to send.
 */
inline void uart0WriteHexByte(std::uint8_t value) noexcept {
    uart0WriteHexNibble(static_cast<std::uint8_t>(value >> 4U));
    uart0WriteHexNibble(value);
}

/**
 * @brief Return whether USART0 has a received byte waiting.
 */
inline bool uart0ByteAvailable() noexcept {
    return (UCSR0A & _BV(RXC0)) != 0U;
}

/**
 * @brief Read one byte from the USART0 receive data register.
 */
inline std::uint8_t uart0ReadByte() noexcept {
    return UDR0;
}

} // namespace wirespaces::platform::avr

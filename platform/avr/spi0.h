/*
 * @file  spi0.h
 * @brief Hardware SPI master driver for ATmega328P-class AVR targets.
 */

#pragma once

#include <avr/io.h>

#include <cstdint>

namespace wirespaces::platform::avr {

/**
 * @brief Configure the ATmega328P SPI master on the Uno ISP header pins.
 *
 * Pin mapping:
 * - MOSI: D11 / PB3
 * - MISO: D12 / PB4
 * - SCK:  D13 / PB5
 * - SS:   D10 / PB2 (chip-select GPIO, idle high)
 *
 * Uses SPI mode 0 at f_CPU/4. SS must remain an output when acting as master.
 *
 * @note D13 shares PB5 with the Uno built-in LED. Hardware SPI SCK takes
 *       precedence over software PWM on that pin.
 */
inline void spi0Init() noexcept {
    DDRB |= _BV(DDB2) | _BV(DDB3) | _BV(DDB5);
    DDRB &= static_cast<std::uint8_t>(~_BV(DDB4));
    PORTB |= _BV(PORTB2);

    SPCR = _BV(SPE) | _BV(MSTR);
    SPSR = 0U;
}

/**
 * @brief Drive the MCP2515 chip-select line (D10 / PB2) active.
 */
inline void spi0ChipSelectAssert() noexcept {
    PORTB &= static_cast<std::uint8_t>(~_BV(PORTB2));
}

/**
 * @brief Release the MCP2515 chip-select line (D10 / PB2).
 */
inline void spi0ChipSelectDeassert() noexcept {
    PORTB |= _BV(PORTB2);
}

/**
 * @brief Exchange one byte on the SPI bus.
 *
 * @param[in] out_byte Byte to transmit.
 * @return Byte captured from MISO during the transfer.
 */
inline std::uint8_t spi0Transfer(std::uint8_t out_byte) noexcept {
    SPDR = out_byte;
    while ((SPSR & _BV(SPIF)) == 0U) {
    }

    return SPDR;
}

} // namespace wirespaces::platform::avr

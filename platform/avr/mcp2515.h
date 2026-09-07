/*
 * @file  mcp2515.h
 * @brief SPI register access for the Microchip MCP2515 CAN controller.
 */

#pragma once

#include <platform/avr/spi0.h>

#include <cstdint>

namespace wirespaces::platform::avr {

/**
 * @brief Blocking SPI helpers for the MCP2515.
 *
 * The MCP2515 has no dedicated silicon ID register. After reset, CANSTAT
 * reports Configuration mode (0x80), which is the usual bring-up probe.
 */
struct Mcp2515 {
    static constexpr std::uint8_t kInstructionRead{0x03U};
    static constexpr std::uint8_t kInstructionWrite{0x02U};
    static constexpr std::uint8_t kInstructionReset{0xC0U};

    static constexpr std::uint8_t kRegisterCanstat{0x0EU};
    static constexpr std::uint8_t kCanstatConfigMode{0x80U};

    /**
     * @brief Issue the MCP2515 RESET instruction.
     */
    static void reset() noexcept;

    /**
     * @brief Read one MCP2515 register over SPI.
     *
     * @param[in] address Register address from the MCP2515 map.
     * @return Register value returned by the controller.
     */
    static std::uint8_t readRegister(std::uint8_t address) noexcept;

    /**
     * @brief Reset the controller and read CANSTAT for bring-up verification.
     *
     * @param[out] canstat CANSTAT value read after reset.
     * @return true when CANSTAT reports Configuration mode.
     */
    static bool probeCanstat(std::uint8_t& canstat) noexcept;
};

inline void Mcp2515::reset() noexcept {
    spi0ChipSelectAssert();
    static_cast<void>(spi0Transfer(kInstructionReset));
    spi0ChipSelectDeassert();
}

inline std::uint8_t Mcp2515::readRegister(std::uint8_t address) noexcept {
    spi0ChipSelectAssert();
    static_cast<void>(spi0Transfer(kInstructionRead));
    static_cast<void>(spi0Transfer(address));
    const std::uint8_t value{spi0Transfer(0x00U)};
    spi0ChipSelectDeassert();
    return value;
}

inline bool Mcp2515::probeCanstat(std::uint8_t& canstat) noexcept {
    reset();

    // Allow the MCP2515 oscillator and configuration state to settle.
    for (volatile std::uint16_t delay_count{0U}; delay_count < 8000U;
         ++delay_count) {
    }

    canstat = readRegister(kRegisterCanstat);
    return canstat == kCanstatConfigMode;
}

} // namespace wirespaces::platform::avr

/*
 * @file  mcp2515_can.h
 * @brief Minimal polling Classical CAN driver for an 8 MHz MCP2515.
 */

#pragma once

#include <platform/avr/mcp2515.h>
#include <platform/avr/spi0.h>

#include <cstdint>

namespace wirespaces::platform::avr {

/** A Classical CAN 2.0 standard data frame. */
struct Mcp2515CanFrame {
    std::uint16_t identifier{0U};
    std::uint8_t size{0U};
    std::uint8_t data[8]{};
};

/**
 * Allocation-free MCP2515 CAN11 data-frame driver.
 *
 * This initial driver polls the controller, accepts every bus frame, exposes
 * only standard 11-bit data frames, and uses transmit buffer 0. It deliberately
 * leaves interrupts, filters, extended identifiers, and remote frames for a
 * later increment.
 */
class Mcp2515Can final {
public:
    /** Configure an MCP2515 with an 8 MHz crystal for 500 kbit/s. */
    static bool initialize500Kbps8MHz() noexcept;

    /** Queue a standard data frame when transmit buffer 0 is available. */
    static bool transmit(const Mcp2515CanFrame& frame) noexcept;

    /** Poll the two receive buffers for one standard data frame. */
    static bool receive(Mcp2515CanFrame& frame) noexcept;

private:
    static constexpr std::uint8_t kInstructionWrite{0x02U};
    static constexpr std::uint8_t kInstructionBitModify{0x05U};
    static constexpr std::uint8_t kInstructionRequestToSendBuffer0{0x81U};

    static constexpr std::uint8_t kRegisterCanstat{0x0EU};
    static constexpr std::uint8_t kRegisterCanctrl{0x0FU};
    static constexpr std::uint8_t kRegisterCnf3{0x28U};
    static constexpr std::uint8_t kRegisterCnf2{0x29U};
    static constexpr std::uint8_t kRegisterCnf1{0x2AU};
    static constexpr std::uint8_t kRegisterCaninte{0x2BU};
    static constexpr std::uint8_t kRegisterCanintf{0x2CU};
    static constexpr std::uint8_t kRegisterTxb0ctrl{0x30U};
    static constexpr std::uint8_t kRegisterTxb0sidh{0x31U};
    static constexpr std::uint8_t kRegisterTxb0dlc{0x35U};
    static constexpr std::uint8_t kRegisterTxb0d0{0x36U};
    static constexpr std::uint8_t kRegisterRxb0ctrl{0x60U};
    static constexpr std::uint8_t kRegisterRxb0sidh{0x61U};
    static constexpr std::uint8_t kRegisterRxb0dlc{0x65U};
    static constexpr std::uint8_t kRegisterRxb0d0{0x66U};
    static constexpr std::uint8_t kRegisterRxb1ctrl{0x70U};
    static constexpr std::uint8_t kRegisterRxb1sidh{0x71U};
    static constexpr std::uint8_t kRegisterRxb1dlc{0x75U};
    static constexpr std::uint8_t kRegisterRxb1d0{0x76U};

    static constexpr std::uint8_t kModeMask{0xE0U};
    static constexpr std::uint8_t kReceiveBuffer0Flag{0x01U};
    static constexpr std::uint8_t kReceiveBuffer1Flag{0x02U};
    static constexpr std::uint8_t kTransmitRequest{0x08U};
    static constexpr std::uint8_t kExtendedIdentifier{0x08U};

    static void writeRegister(std::uint8_t address, std::uint8_t value) noexcept;
    static void bitModify(std::uint8_t address, std::uint8_t mask,
                          std::uint8_t value) noexcept;
};

inline void Mcp2515Can::writeRegister(std::uint8_t address,
                                      std::uint8_t value) noexcept {
    spi0ChipSelectAssert();
    static_cast<void>(spi0Transfer(kInstructionWrite));
    static_cast<void>(spi0Transfer(address));
    static_cast<void>(spi0Transfer(value));
    spi0ChipSelectDeassert();
}

inline void Mcp2515Can::bitModify(std::uint8_t address, std::uint8_t mask,
                                  std::uint8_t value) noexcept {
    spi0ChipSelectAssert();
    static_cast<void>(spi0Transfer(kInstructionBitModify));
    static_cast<void>(spi0Transfer(address));
    static_cast<void>(spi0Transfer(mask));
    static_cast<void>(spi0Transfer(value));
    spi0ChipSelectDeassert();
}

inline bool Mcp2515Can::initialize500Kbps8MHz() noexcept {
    std::uint8_t canstat{0U};
    if (!Mcp2515::probeCanstat(canstat)) {
        return false;
    }

    // Fosc=8 MHz, BRP=0: 8 time quanta per 2 us bit. The sample point is
    // 75%: Sync=1, PropSeg=1, PhaseSeg1=4, PhaseSeg2=2, SJW=1.
    writeRegister(kRegisterCnf1, 0x00U);
    writeRegister(kRegisterCnf2, 0x98U);
    writeRegister(kRegisterCnf3, 0x01U);

    // Receive any valid frame. Rollover lets RXB0 spill into RXB1 while the
    // polling application is servicing UART.
    writeRegister(kRegisterRxb0ctrl, 0x64U);
    writeRegister(kRegisterRxb1ctrl, 0x60U);
    writeRegister(kRegisterCaninte, 0x00U);
    writeRegister(kRegisterCanintf, 0x00U);

    bitModify(kRegisterCanctrl, kModeMask, 0x00U);
    for (std::uint16_t attempt{0U}; attempt < 1000U; ++attempt) {
        if ((Mcp2515::readRegister(kRegisterCanstat) & kModeMask) == 0U) {
            return true;
        }
    }
    return false;
}

inline bool Mcp2515Can::transmit(const Mcp2515CanFrame& frame) noexcept {
    if ((frame.identifier > 0x07FFU) || (frame.size > 8U) ||
        ((Mcp2515::readRegister(kRegisterTxb0ctrl) & kTransmitRequest) != 0U)) {
        return false;
    }

    writeRegister(kRegisterTxb0sidh,
                  static_cast<std::uint8_t>(frame.identifier >> 3U));
    writeRegister(static_cast<std::uint8_t>(kRegisterTxb0sidh + 1U),
                  static_cast<std::uint8_t>((frame.identifier & 0x07U) << 5U));
    writeRegister(static_cast<std::uint8_t>(kRegisterTxb0sidh + 2U), 0x00U);
    writeRegister(static_cast<std::uint8_t>(kRegisterTxb0sidh + 3U), 0x00U);
    writeRegister(kRegisterTxb0dlc, frame.size);
    for (std::uint8_t index{0U}; index < frame.size; ++index) {
        writeRegister(static_cast<std::uint8_t>(kRegisterTxb0d0 + index),
                      frame.data[index]);
    }

    spi0ChipSelectAssert();
    static_cast<void>(spi0Transfer(kInstructionRequestToSendBuffer0));
    spi0ChipSelectDeassert();
    return true;
}

inline bool Mcp2515Can::receive(Mcp2515CanFrame& frame) noexcept {
    const std::uint8_t flags{Mcp2515::readRegister(kRegisterCanintf)};
    std::uint8_t flag{0U};
    std::uint8_t identifier_register{0U};
    std::uint8_t dlc_register{0U};
    std::uint8_t data_register{0U};
    if ((flags & kReceiveBuffer0Flag) != 0U) {
        flag = kReceiveBuffer0Flag;
        identifier_register = kRegisterRxb0sidh;
        dlc_register = kRegisterRxb0dlc;
        data_register = kRegisterRxb0d0;
    } else if ((flags & kReceiveBuffer1Flag) != 0U) {
        flag = kReceiveBuffer1Flag;
        identifier_register = kRegisterRxb1sidh;
        dlc_register = kRegisterRxb1dlc;
        data_register = kRegisterRxb1d0;
    } else {
        return false;
    }

    const std::uint8_t sidh{Mcp2515::readRegister(identifier_register)};
    const std::uint8_t sidl{Mcp2515::readRegister(
        static_cast<std::uint8_t>(identifier_register + 1U))};
    if ((sidl & kExtendedIdentifier) != 0U) {
        bitModify(kRegisterCanintf, flag, 0x00U);
        return false;
    }

    frame.identifier = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(sidh) << 3U) | (sidl >> 5U));
    const std::uint8_t dlc{Mcp2515::readRegister(dlc_register)};
    if ((dlc & 0x40U) != 0U) {
        bitModify(kRegisterCanintf, flag, 0x00U);
        return false;
    }

    frame.size = static_cast<std::uint8_t>(dlc & 0x0FU);
    if (frame.size > 8U) {
        frame.size = 8U;
    }
    for (std::uint8_t index{0U}; index < frame.size; ++index) {
        frame.data[index] = Mcp2515::readRegister(
            static_cast<std::uint8_t>(data_register + index));
    }
    bitModify(kRegisterCanintf, flag, 0x00U);
    return true;
}

}  // namespace wirespaces::platform::avr

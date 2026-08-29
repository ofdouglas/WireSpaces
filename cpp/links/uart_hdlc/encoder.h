/*
 * @file  hdlc_encoder.hpp
 * @brief Freestanding C++ HDLC encoder for the AVR UART example.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include <crc/crc_algorithm.h>

namespace wirespaces::links::uart_hdlc {

/**
 * @brief Encode byte-stuffed HDLC frames without dynamic allocation.
 */
class HdlcEncoder {
public:
    /**
     * @brief Return the exact encoded size including CRC and frame flags.
     *
     * @param[in] payload Payload bytes to inspect.
     * @param[in] payload_size Number of payload bytes.
     * @return Encoded frame size, or zero for an invalid null payload.
     */
    static std::size_t encodedSize(
        const std::uint8_t* payload,
        std::size_t payload_size) noexcept;

    /**
     * @brief Encode one complete HDLC frame.
     *
     * @param[in] payload Payload bytes.
     * @param[in] payload_size Number of payload bytes.
     * @param[out] frame Destination storage.
     * @param[in] frame_capacity Destination capacity.
     * @return Number of encoded bytes, or zero if arguments/storage are invalid.
     */
    static std::size_t encode(
        const std::uint8_t* payload,
        std::size_t payload_size,
        std::uint8_t* frame,
        std::size_t frame_capacity) noexcept;

private:
    static std::size_t stuffedSize(std::uint8_t byte) noexcept;

    static constexpr std::uint8_t kFlag{0x7EU};
    static constexpr std::uint8_t kEscape{0x7DU};
    static constexpr std::uint8_t kEscapeXor{0x20U};
    static constexpr std::size_t kCrcSize{2U};
};

inline std::size_t HdlcEncoder::stuffedSize(std::uint8_t byte) noexcept {
    return ((byte == kFlag) || (byte == kEscape)) ? 2U : 1U;
}

inline std::size_t HdlcEncoder::encodedSize(
    const std::uint8_t* payload,
    std::size_t payload_size) noexcept {
    if ((payload == nullptr) && (payload_size != 0U)) {
        return 0U;
    }

    std::size_t result{2U};
    for (std::size_t index{0U}; index < payload_size; ++index) {
        result += stuffedSize(payload[index]);
    }
    const std::uint16_t crc{
        wirespaces::crc::algorithm::Crc16CcittFalse::compute(
            wirespaces::foundation::Span<const std::uint8_t>{payload, payload_size})};
    result += stuffedSize(static_cast<std::uint8_t>(crc));
    result += stuffedSize(static_cast<std::uint8_t>(crc >> 8U));
    return result;
}

inline std::size_t HdlcEncoder::encode(
    const std::uint8_t* payload,
    std::size_t payload_size,
    std::uint8_t* frame,
    std::size_t frame_capacity) noexcept {
    const std::size_t required_size{encodedSize(payload, payload_size)};
    if ((frame == nullptr) || (required_size == 0U) ||
        (frame_capacity < required_size)) {
        return 0U;
    }

    std::size_t output_index{0U};
    frame[output_index++] = kFlag;
    for (std::size_t input_index{0U}; input_index < payload_size; ++input_index) {
        const std::uint8_t byte{payload[input_index]};
        if ((byte == kFlag) || (byte == kEscape)) {
            frame[output_index++] = kEscape;
            frame[output_index++] = static_cast<std::uint8_t>(byte ^ kEscapeXor);
        } else {
            frame[output_index++] = byte;
        }
    }

    const std::uint16_t crc{
        wirespaces::crc::algorithm::Crc16CcittFalse::compute(
            wirespaces::foundation::Span<const std::uint8_t>{payload, payload_size})};
    const std::uint8_t trailer[kCrcSize]{
        static_cast<std::uint8_t>(crc),
        static_cast<std::uint8_t>(crc >> 8U),
    };
    for (std::size_t index{0U}; index < kCrcSize; ++index) {
        const std::uint8_t byte{trailer[index]};
        if ((byte == kFlag) || (byte == kEscape)) {
            frame[output_index++] = kEscape;
            frame[output_index++] =
                static_cast<std::uint8_t>(byte ^ kEscapeXor);
        } else {
            frame[output_index++] = byte;
        }
    }
    frame[output_index++] = kFlag;
    return output_index;
}

} // namespace wirespaces::links::uart_hdlc

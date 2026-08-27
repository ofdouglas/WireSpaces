#pragma once
/*
 * @file  hdlc_encoder.hpp
 * @brief Freestanding C++ HDLC encoder for the AVR UART example.
 */

#include <cstddef>
#include <cstdint>

namespace wirespaces::avr_example {

/**
 * @brief Encode byte-stuffed HDLC frames without dynamic allocation.
 */
class HdlcEncoder {
public:
    /**
     * @brief Return the exact encoded size including both frame flags.
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
    static constexpr std::uint8_t kFlag{0x7EU};
    static constexpr std::uint8_t kEscape{0x7DU};
    static constexpr std::uint8_t kEscapeXor{0x20U};
};

inline std::size_t HdlcEncoder::encodedSize(
    const std::uint8_t* payload,
    std::size_t payload_size) noexcept {
    if ((payload == nullptr) && (payload_size != 0U)) {
        return 0U;
    }

    std::size_t result{2U};
    for (std::size_t index{0U}; index < payload_size; ++index) {
        result += ((payload[index] == kFlag) || (payload[index] == kEscape))
                      ? 2U
                      : 1U;
    }
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
    frame[output_index++] = kFlag;
    return output_index;
}

} // namespace wirespaces::avr_example

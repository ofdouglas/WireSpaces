/*
 * @file  hdlc_decoder.hpp
 * @brief Freestanding C++ HDLC stream decoder for the AVR UART example.
 */

#pragma once

#include <wirespaces/crc/crc_algorithm.h>
#include <wirespaces/foundation/span.h>

#include <cstddef>
#include <cstdint>

namespace wirespaces::links::uart_hdlc {

/**
 * @brief Incrementally decode one bounded HDLC frame at a time.
 *
 * @tparam kFrameCapacity Maximum frame-body size excluding the CRC trailer.
 */
template <std::size_t kFrameCapacity>
class HdlcDecoder {
public:
    /**
     * @brief Consume one byte from the physical Link.
     *
     * @param[in] byte Next UART byte.
     * @return true when a complete non-empty frame with a valid CRC is available.
     */
    bool push(std::uint8_t byte) noexcept;

    /**
     * @brief Return the current completed frame body.
     */
    foundation::Span<const std::uint8_t> frame() const noexcept {
        return foundation::Span<const std::uint8_t>{frame_, frame_size_};
    }

    /**
     * @brief Release the completed frame and resume stream reception.
     */
    void consume() noexcept {
        frame_size_ = 0U;
        frame_ready_ = false;
    }

private:
    bool hasValidCrc() const noexcept;

    static constexpr std::uint8_t kFlag{0x7EU};
    static constexpr std::uint8_t kEscape{0x7DU};
    static constexpr std::uint8_t kEscapeXor{0x20U};
    static constexpr std::size_t kCrcSize{2U};

    std::uint8_t frame_[kFrameCapacity + kCrcSize]{};
    std::size_t frame_size_{0U};
    bool in_frame_{false};
    bool escaped_{false};
    bool frame_ready_{false};
};

template <std::size_t kFrameCapacity>
bool HdlcDecoder<kFrameCapacity>::hasValidCrc() const noexcept {
    if (frame_size_ <= kCrcSize) {
        return false;
    }

    const std::size_t payload_size{frame_size_ - kCrcSize};
    const std::uint16_t received_crc{static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(frame_[payload_size]) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(frame_[payload_size + 1U]) << 8U))};
    return received_crc == wirespaces::crc::algorithm::Crc16CcittFalse::compute(
                               foundation::Span<const std::uint8_t>{frame_, payload_size});
}

template <std::size_t kFrameCapacity>
bool HdlcDecoder<kFrameCapacity>::push(std::uint8_t byte) noexcept {
    if (frame_ready_) {
        return true;
    }

    if (byte == kFlag) {
        const bool completed{in_frame_ && !escaped_ && hasValidCrc()};
        in_frame_ = true;
        escaped_ = false;
        if (completed) {
            frame_size_ -= kCrcSize;
            frame_ready_ = true;
            return true;
        }
        frame_size_ = 0U;
        return false;
    }

    if (!in_frame_) {
        return false;
    }

    if (escaped_) {
        byte = static_cast<std::uint8_t>(byte ^ kEscapeXor);
        escaped_ = false;
    } else if (byte == kEscape) {
        escaped_ = true;
        return false;
    }

    if (frame_size_ >= sizeof(frame_)) {
        frame_size_ = 0U;
        in_frame_ = false;
        escaped_ = false;
        return false;
    }

    frame_[frame_size_++] = byte;
    return false;
}

}  // namespace wirespaces::links::uart_hdlc

#pragma once
/*
 * @file  hdlc_decoder.hpp
 * @brief Freestanding C++ HDLC stream decoder for the AVR UART example.
 */

#include <cstddef>
#include <cstdint>

namespace wirespaces::avr_example {

/**
 * @brief Incrementally decode one bounded HDLC frame at a time.
 *
 * @tparam kFrameCapacity Maximum unescaped frame-body size.
 */
template <std::size_t kFrameCapacity>
class HdlcDecoder {
public:
    /**
     * @brief Consume one byte from the physical Link.
     *
     * @param[in] byte Next UART byte.
     * @return true when a complete non-empty frame is available.
     */
    bool push(std::uint8_t byte) noexcept;

    /**
     * @brief Return the current completed frame body.
     */
    const std::uint8_t* frameData() const noexcept {
        return frame_;
    }

    /**
     * @brief Return the current completed frame-body length.
     */
    std::size_t frameSize() const noexcept {
        return frame_size_;
    }

    /**
     * @brief Release the completed frame and resume stream reception.
     */
    void consume() noexcept {
        frame_size_ = 0U;
        frame_ready_ = false;
    }

private:
    static constexpr std::uint8_t kFlag{0x7EU};
    static constexpr std::uint8_t kEscape{0x7DU};
    static constexpr std::uint8_t kEscapeXor{0x20U};

    std::uint8_t frame_[kFrameCapacity]{};
    std::size_t frame_size_{0U};
    bool in_frame_{false};
    bool escaped_{false};
    bool frame_ready_{false};
};

template <std::size_t kFrameCapacity>
bool HdlcDecoder<kFrameCapacity>::push(std::uint8_t byte) noexcept {
    if (frame_ready_) {
        return true;
    }

    if (byte == kFlag) {
        const bool completed{in_frame_ && (frame_size_ != 0U) && !escaped_};
        in_frame_ = true;
        escaped_ = false;
        if (completed) {
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

    if (frame_size_ >= kFrameCapacity) {
        frame_size_ = 0U;
        in_frame_ = false;
        escaped_ = false;
        return false;
    }

    frame_[frame_size_++] = byte;
    return false;
}

} // namespace wirespaces::avr_example

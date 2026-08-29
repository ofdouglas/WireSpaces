#pragma once
/*
 * @file  hdlc.h
 * @brief HDLC framing for the custom link-layer protocol.
 */

#include <cstdint>
#include <cstddef>
#include <array>
#include <cstring>
#include <algorithm>

#include "data_structures/ring_buffer.h"
#include "util/span.h"
#include "logging/logging.h"


// All WireSpaces HDLC frames end with this 2-byte trailer:
struct FrameTrailer {
    using CrcAlgorithm = crc::algorithm::Crc16CcittFalse;
    uint16_t crc16; // Little-endian on the wire. CRC covers the frame except this trailer.
  };


namespace wirespaces::hdlc {

    // Special byte values used in the HDLC framing protocol.
    struct Flag {
        static constexpr uint8_t kFlag     = 0x7E;
        static constexpr uint8_t kEscape   = 0x7D;
        static constexpr uint8_t kXorValue = 0x20;
    };

    size_t byteStuff(uint8_t byte, util::Span<uint8_t> out);
    bool encodeFrame(util::Span<const uint8_t> payload, util::Span<uint8_t> frame);
    bool decodePayload(util::Span<const uint8_t> input, util::Span<uint8_t> output);

    /*
     * @brief Interface for a byte-oriented HDLC receiver.
     *
     * @todo Define API contract fully.
     */
    class ReceiverInterface {
    public:
        virtual ~ReceiverInterface() = default;
        virtual void reset() = 0;
        virtual bool enqueue(uint8_t data) = 0;
        virtual bool enqueue(util::Span<const uint8_t> data) = 0;
        virtual bool process() = 0;
        virtual size_t receivePayload(util::Span<uint8_t> output) = 0;
    };

    /*
     * @brief Basic HDLC receiver.
     *
     * @todo Document this.
     */
    template <size_t PayloadBufferSize, size_t InputRingCapacity = PayloadBufferSize * 2U + 8U>
    class Receiver : public ReceiverInterface {
    private:
        enum class State : uint8_t {
            IDLE,
            FRAME_DELIMETER,
            DATA,
            ESCAPE
        };

        static const char* toString(State state) {
            switch (state) {
                case State::IDLE: return "IDLE";
                case State::FRAME_DELIMETER: return "FRAME_DELIMETER";
                case State::DATA: return "DATA";
                case State::ESCAPE: return "ESCAPE";
                default: return "UNKNOWN";
            }
        }

    public:
        Receiver() = default;

        void reset() override {
            state_ = State::IDLE;
            payload_index_ = 0U;
            input_buffer_.clear();
        }

        bool enqueue(uint8_t data) override {
            return input_buffer_.enqueue(data);
        }

        bool enqueue(util::Span<const uint8_t> data) override {
            for (size_t i = 0U; i < data.size(); i++) {
                if (!input_buffer_.enqueue(data[i])) {
                    return false;
                }
            }
            return true;
        }

        bool process(util::Span<const uint8_t> data) {
            if (!enqueue(data)) {
                return false;
            }
            return process();
        }

        bool process() override {
            while (!input_buffer_.isEmpty()) {
                uint8_t data{0U};
                if (!input_buffer_.dequeue(data)) {
                    return false;
                }
                switch (state_) {
                    case State::IDLE:
                        if (data == Flag::kFlag) {
                            payload_index_ = 0U;
                            state_ = State::FRAME_DELIMETER;
                        }
                        break;

                    case State::FRAME_DELIMETER:
                        if (data == Flag::kFlag) {
                            payload_index_ = 0U;
                        } else if (data == Flag::kEscape) {
                            state_ = State::ESCAPE;
                        } else if (payload_index_ >= PayloadBufferSize) {
                            logPayloadBufferOverflow();
                            return false;
                        } else {
                            payload_buffer_[payload_index_++] = data;
                            state_ = State::DATA;
                        }
                        break;

                    case State::ESCAPE:
                        if (payload_index_ >= PayloadBufferSize) {
                            logPayloadBufferOverflow();
                            return false;
                        }
                        payload_buffer_[payload_index_++] = static_cast<uint8_t>(data ^ Flag::kXorValue);
                        state_ = State::DATA;
                        break;

                    case State::DATA:
                        if (data == Flag::kFlag) {
                            state_ = State::FRAME_DELIMETER;
                            return true;
                        } else if (data == Flag::kEscape) {
                            state_ = State::ESCAPE;
                        } else if (payload_index_ >= PayloadBufferSize) {
                            logPayloadBufferOverflow();
                            return false;
                        } else {
                            payload_buffer_[payload_index_++] = data;
                        }
                        break;

                    default:
                        LOG_ERROR() << "Invalid state";
                        reset();
                        return false;
                }
            }
            return false;
        }

        size_t receivePayload(util::Span<uint8_t> output) override {
            if (state_ != State::FRAME_DELIMETER) {
                return 0U;
            }
            if ((payload_index_ == 0U) || (payload_index_ > output.size())) {
                return 0U;
            }
            std::memcpy(output.data(), payload_buffer_.data(), payload_index_);
            const size_t result = payload_index_;
            payload_index_ = 0U;
            state_ = State::IDLE;
            return result;
        }

        void logPayloadBufferOverflow() {
            const size_t dump_len = std::min(payload_index_, size_t{8U});
            util::Span<uint8_t> payload_first8{payload_buffer_.data(), dump_len};
            LOG_ERROR() << "Payload buffer overflow in state " << toString(state_) << ": " << payload_first8;
            reset();
        }

    private:
        RingBuffer<uint8_t, InputRingCapacity> input_buffer_;
        std::array<uint8_t, PayloadBufferSize> payload_buffer_;
        size_t payload_index_{0U};
        State state_{State::IDLE};
    };

} // namespace hdlc

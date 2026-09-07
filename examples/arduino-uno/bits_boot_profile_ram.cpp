/**
 * @file bits_boot_profile_ram.cpp
 * @brief Receiver-only Compact BITS RAM profile for an Arduino UNO hardware test.
 */

#include <platform/avr/uart0.h>
#include <platform/avr/millisecond_clock.h>
#include <wirespaces/hal/clock.h>
#include <avr/interrupt.h>
#include <wirespaces/crc/crc_algorithm.h>
#include <wirespaces/links/uart_hdlc/decoder.h>
#include <wirespaces/transports/bits/receiver_engine.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "wiring_constants.h"

#ifndef WS_BITS_BOOT_PROFILE_SIZE_ONLY
#define WS_BITS_BOOT_PROFILE_SIZE_ONLY 0
#endif

namespace bits = wirespaces::transport::bits;

namespace {

constexpr std::uint16_t kMaximumObjectSize{256U};
constexpr std::uint16_t kSegmentPayloadSize{24U};
constexpr std::uint16_t kMaximumBitsPayloadSize{bits::kSegmentHeaderSize + kSegmentPayloadSize};
constexpr std::uint16_t kMaximumUserDatagramSize{kMaximumBitsPayloadSize - bits::kUserDatagramHeaderSize};
constexpr std::size_t kMaximumCanonicalSize{sizeof(wirespaces::Header) + kMaximumBitsPayloadSize};

constexpr std::uint8_t kEchoRequest{0x01U};
constexpr std::uint8_t kEchoResponse{0x81U};
constexpr std::uint8_t kImageResult{0x82U};
constexpr std::uint8_t kImageOk{0U};
constexpr std::uint8_t kImageLengthMismatch{1U};
constexpr std::uint8_t kImageDataMismatch{2U};
constexpr std::uint16_t kNoBadOffset{0xFFFFU};

WS_PACKET_BUFFER_DEFINE(TransmitPacket, kMaximumBitsPayloadSize);

class UartPduSender final : public bits::ReceiverPduSender {
public:
    wirespaces::MutableByteSpan prepare(std::uint16_t payload_size) noexcept override {
        if (!packet_.initialize(payload_size, wiring_constants::kUploadConnection, wirespaces::ControlFields::bits())) {
            return wirespaces::MutableByteSpan{};
        }
        return packet_.payload();
    }

    bits::SendResult sendPrepared() noexcept override {
        wirespaces::ByteSpan tx_bytes{packet_.headerAndPayload()};
        auto crc{wirespaces::crc::algorithm::Crc16CcittFalse::compute(tx_bytes)};

        wirespaces::platform::avr::uart0WriteByte(kFlag);
        for (std::size_t index{0U}; index < tx_bytes.size(); ++index) {
            writeEscaped(tx_bytes[index]);
        }
        writeEscaped(static_cast<std::uint8_t>(crc));
        writeEscaped(static_cast<std::uint8_t>(crc >> 8U));
        wirespaces::platform::avr::uart0WriteByte(kFlag);
        return bits::SendResult::kSent;
    }

private:
    static void writeEscaped(std::uint8_t byte) noexcept {
        if (byte == kFlag || byte == kEscape) {
            wirespaces::platform::avr::uart0WriteByte(kEscape);
            byte = static_cast<std::uint8_t>(byte ^ kEscapeXor);
        }
        wirespaces::platform::avr::uart0WriteByte(byte);
    }

    static constexpr std::uint8_t kFlag{0x7EU};
    static constexpr std::uint8_t kEscape{0x7DU};
    static constexpr std::uint8_t kEscapeXor{0x20U};
    TransmitPacket packet_{};
};

class RamProfileCallbacks final : public bits::ReceiverCallbacks {
public:
    void attach(bits::BitsReceiverEngine& engine) noexcept {
        engine_ = &engine;
    }

    bits::TransferAdmission beginTransfer(
        const bits::TransferInfo& info) noexcept override {
        if (info.total_size > kMaximumObjectSize) return bits::TransferAdmission::kTooLarge;
        session_id_ = info.session_id;
        expected_size_ = static_cast<std::uint16_t>(info.total_size);
        received_size_ = 0U;
        return bits::TransferAdmission::kAccepted;
    }

    void onTransferFailed(bits::FailureReason) noexcept override {
        received_size_ = 0U;
        expected_size_ = 0U;
    }

    bool onSegment(std::uint32_t object_offset, wirespaces::ByteSpan payload) noexcept override {
        if (object_offset + payload.size() > kMaximumObjectSize) {
            return false;
        }
#if !WS_BITS_BOOT_PROFILE_SIZE_ONLY
        std::memcpy(object_ + object_offset, payload.data(), payload.size());
#endif
        const std::uint16_t end{static_cast<std::uint16_t>(object_offset + payload.size())};
        if (end > received_size_) {
            received_size_ = end;
        }
        return true;
    }

    void onDatagram(wirespaces::ByteSpan payload) noexcept override {
        if (engine_ == nullptr || payload.size() < 2U ||
            payload[0] != kEchoRequest) {
            return;
        }
#if WS_BITS_BOOT_PROFILE_SIZE_ONLY
        const std::uint8_t response[]{kEchoResponse, payload[1]};
        engine_->sendDatagram(wirespaces::ByteSpan{response});
#else
        if (payload.size() > kMaximumUserDatagramSize) {
            return;
        }
        std::uint8_t response[kMaximumUserDatagramSize]{};
        std::memcpy(response, payload.data(), payload.size());
        response[0] = kEchoResponse;
        engine_->sendDatagram(wirespaces::ByteSpan{response, payload.size()});
#endif
    }

    void onTransferComplete() noexcept override {
#if !WS_BITS_BOOT_PROFILE_SIZE_ONLY
        std::uint8_t status{kImageOk};
        std::uint16_t first_bad_offset{kNoBadOffset};
        if (received_size_ != expected_size_) {
            status = kImageLengthMismatch;
        } else {
            for (std::uint16_t index{0U}; index < received_size_; ++index) {
                if (object_[index] != static_cast<std::uint8_t>(index)) {
                    status = kImageDataMismatch;
                    first_bad_offset = index;
                    break;
                }
            }
        }

        const std::uint8_t result[]{
            kImageResult,
            session_id_,
            status,
            static_cast<std::uint8_t>(received_size_),
            static_cast<std::uint8_t>(received_size_ >> 8U),
            static_cast<std::uint8_t>(first_bad_offset),
            static_cast<std::uint8_t>(first_bad_offset >> 8U),
        };
        if (engine_ != nullptr) {
            engine_->sendDatagram(wirespaces::ByteSpan{result});
        }
#endif
    }

    void onTransferAborted(bits::AbortReason) noexcept override {
        received_size_ = 0U;
        expected_size_ = 0U;
    }

private:
    bits::BitsReceiverEngine* engine_{nullptr};
#if !WS_BITS_BOOT_PROFILE_SIZE_ONLY
    std::uint8_t object_[kMaximumObjectSize]{};
#endif
    std::uint16_t received_size_{0U};
    std::uint16_t expected_size_{0U};
    std::uint8_t session_id_{0U};
};

bool matchesConnection(const wirespaces::Header& header) noexcept {
    return header.transportType() == wirespaces::TransportType::kBits &&
           header.hasSupportedControl() &&
           header.wire == wiring_constants::kTestWire &&
           header.source == wiring_constants::kPcHost &&
           header.destination == wiring_constants::kArduinoHost &&
           header.endpoint == wiring_constants::kBitsUploadEndpoint;
}

void processFrame(wirespaces::ByteSpan frame, bits::BitsReceiverEngine& engine) noexcept {
    if (frame.size() < sizeof(wirespaces::Header)) {
        return;
    }

    wirespaces::Header header{};
    std::memcpy(&header, frame.data(), sizeof(header));
    if (!matchesConnection(header)) {
        return;
    }

    const wirespaces::ByteSpan message{
        frame.subspan(sizeof(wirespaces::Header))};
    if (message.empty()) {
        return;
    }

    engine.process(message, wirespaces::hal::MillisecondClock::now());
}

}  // namespace

int main() {
    wirespaces::platform::avr::uart0Init(wiring_constants::kUartBaudRate);

    wirespaces::platform::avr::millisecondClockInit();
    sei();

    UartPduSender sender{};
    RamProfileCallbacks callbacks{};
    bits::BitsReceiverEngine engine{bits::ReceiverEngineConfig{kSegmentPayloadSize, 1U, kMaximumObjectSize},callbacks, sender};
    callbacks.attach(engine);
    wirespaces::links::uart_hdlc::HdlcDecoder<kMaximumCanonicalSize> decoder{};

    for (;;) {
        engine.poll(wirespaces::hal::MillisecondClock::now());
        while (wirespaces::platform::avr::uart0ByteAvailable()) {
            const std::uint8_t byte{
                wirespaces::platform::avr::uart0ReadByte()};
            if (!decoder.push(byte)) {
                continue;
            }
            processFrame(decoder.frame(), engine);
            decoder.consume();
        }
    }
}

/**
 * @file bits_boot_profile_ram.cpp
 * @brief Receiver-only Compact BITS RAM profile for an Arduino UNO hardware test.
 */

#include <platform/avr/uart0.h>
#include <wirespaces/links/uart_hdlc/decoder.h>
#include <wirespaces/transports/bits/receiver_engine.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

#ifndef WS_BITS_BOOT_PROFILE_SIZE_ONLY
#define WS_BITS_BOOT_PROFILE_SIZE_ONLY 0
#endif

namespace {

constexpr std::uint32_t kBaudRate{115200UL};
constexpr wirespaces::WireNumber kTestWire{1U};
constexpr wirespaces::HostId kArduinoHost{1U};
constexpr wirespaces::HostId kPcHost{2U};
constexpr wirespaces::EndpointAddress kUploadEndpoint{
    wirespaces::EndpointAddress::from(wirespaces::Namespace::kUser0, 1U)};
constexpr std::uint16_t kMaximumObjectSize{256U};
constexpr std::uint16_t kSegmentPayloadSize{24U};
constexpr std::uint16_t kMaximumBitsPayloadSize{
    wirespaces::transport::bits::kSegmentHeaderSize + kSegmentPayloadSize};
constexpr std::uint16_t kMaximumUserDatagramSize{
    kMaximumBitsPayloadSize -
    wirespaces::transport::bits::kUserDatagramHeaderSize};
constexpr std::size_t kMaximumCanonicalSize{
    sizeof(wirespaces::Header) + kMaximumBitsPayloadSize};

constexpr std::uint8_t kEchoRequest{0x01U};
constexpr std::uint8_t kEchoResponse{0x81U};
constexpr std::uint8_t kImageResult{0x82U};
constexpr std::uint8_t kImageOk{0U};
constexpr std::uint8_t kImageLengthMismatch{1U};
constexpr std::uint8_t kImageDataMismatch{2U};
constexpr std::uint16_t kNoBadOffset{0xFFFFU};

WS_PACKET_BUFFER_DEFINE(TransmitPacket, kMaximumBitsPayloadSize);

class UartPduSender final
    : public wirespaces::transport::bits::ReceiverPduSender {
public:
    wirespaces::MutableByteSpan prepare(
        std::uint16_t payload_size) noexcept override {
        if (!packet_.initialize(
                payload_size,
                wirespaces::ControlFields{
                    wirespaces::QoS::kNormal, false,
                    wirespaces::TransportType::kBits})) {
            return wirespaces::MutableByteSpan{};
        }
        wirespaces::Header& header{packet_.header()};
        header.wire = kTestWire;
        header.source = kArduinoHost;
        header.destination = kPcHost;
        header.endpoint = kUploadEndpoint;
        return packet_.payload();
    }

    bool sendPrepared() noexcept override {
        const auto* canonical_bytes{
            reinterpret_cast<const std::uint8_t*>(&packet_.header())};
        const std::size_t canonical_size{
            sizeof(packet_.header()) + packet_.size()};
        std::uint16_t crc{0xFFFFU};

        wirespaces::platform::avr::uart0WriteByte(kFlag);
        for (std::size_t index{0U}; index < canonical_size; ++index) {
            const std::uint8_t byte{canonical_bytes[index]};
            crc = updateCrc(crc, byte);
            writeEscaped(byte);
        }
        writeEscaped(static_cast<std::uint8_t>(crc));
        writeEscaped(static_cast<std::uint8_t>(crc >> 8U));
        wirespaces::platform::avr::uart0WriteByte(kFlag);
        return true;
    }

private:
    static std::uint16_t updateCrc(std::uint16_t crc,
                                   std::uint8_t byte) noexcept {
        crc = static_cast<std::uint16_t>(
            crc ^ static_cast<std::uint16_t>(byte << 8U));
        for (std::uint8_t bit{0U}; bit < 8U; ++bit) {
            crc = (crc & 0x8000U) != 0U
                      ? static_cast<std::uint16_t>((crc << 1U) ^ 0x1021U)
                      : static_cast<std::uint16_t>(crc << 1U);
        }
        return crc;
    }

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

class RamProfileCallbacks final
    : public wirespaces::transport::bits::ReceiverCallbacks {
public:
    void attach(wirespaces::transport::bits::BitsReceiverEngine& engine) noexcept {
        engine_ = &engine;
    }

    void beginSession(std::uint8_t session_id,
                      std::uint32_t total_size) noexcept {
        session_id_ = session_id;
        expected_size_ = total_size <= kMaximumObjectSize
                             ? static_cast<std::uint16_t>(total_size)
                             : 0U;
        received_size_ = 0U;
    }

    bool onSegment(std::uint32_t object_offset,
                   wirespaces::ByteSpan payload) noexcept override {
        if (object_offset + payload.size() > kMaximumObjectSize) {
            return false;
        }
#if !WS_BITS_BOOT_PROFILE_SIZE_ONLY
        std::memcpy(object_ + object_offset, payload.data(), payload.size());
#endif
        const std::uint16_t end{
            static_cast<std::uint16_t>(object_offset + payload.size())};
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
        (void)engine_->sendDatagram(wirespaces::ByteSpan{response});
#else
        if (payload.size() > kMaximumUserDatagramSize) {
            return;
        }
        std::uint8_t response[kMaximumUserDatagramSize]{};
        std::memcpy(response, payload.data(), payload.size());
        response[0] = kEchoResponse;
        (void)engine_->sendDatagram(
            wirespaces::ByteSpan{response, payload.size()});
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
            (void)engine_->sendDatagram(wirespaces::ByteSpan{result});
        }
#endif
    }

    void onTransferAborted() noexcept override {
        received_size_ = 0U;
        expected_size_ = 0U;
    }

private:
    wirespaces::transport::bits::BitsReceiverEngine* engine_{nullptr};
#if !WS_BITS_BOOT_PROFILE_SIZE_ONLY
    std::uint8_t object_[kMaximumObjectSize]{};
#endif
    std::uint16_t received_size_{0U};
    std::uint16_t expected_size_{0U};
    std::uint8_t session_id_{0U};
};

bool matchesConnection(const wirespaces::Header& header) noexcept {
    return header.transportType() == wirespaces::TransportType::kBits &&
           !header.hasExtensions() && header.wire == kTestWire &&
           header.source == kPcHost && header.destination == kArduinoHost &&
           header.endpoint == kUploadEndpoint;
}

void processFrame(
    wirespaces::ByteSpan frame,
    wirespaces::transport::bits::BitsReceiverEngine& engine,
    RamProfileCallbacks& callbacks) noexcept {
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

    wirespaces::transport::bits::Control control{};
    if (engine.state() != wirespaces::transport::bits::TransferState::kActive &&
        wirespaces::transport::bits::decodeControl(message[0], control) &&
        control.type == wirespaces::transport::bits::MessageType::kSetup) {
        wirespaces::transport::bits::Setup setup{};
        if (wirespaces::transport::bits::detail::decodeSetupKnownType(message, setup) &&
            setupTotalSize(setup) <= kMaximumObjectSize) {
            callbacks.beginSession(setup.session_id, setupTotalSize(setup));
        }
    }

    (void)engine.process(message);
}

}  // namespace

int main() {
    wirespaces::platform::avr::uart0Init(kBaudRate);

    UartPduSender sender{};
    RamProfileCallbacks callbacks{};
    wirespaces::transport::bits::BitsReceiverEngine engine{
        wirespaces::transport::bits::ReceiverEngineConfig{
            kSegmentPayloadSize, 1U, kMaximumObjectSize},
        callbacks, sender};
    callbacks.attach(engine);
    wirespaces::links::uart_hdlc::HdlcDecoder<kMaximumCanonicalSize> decoder{};

    for (;;) {
        while (wirespaces::platform::avr::uart0ByteAvailable()) {
            const std::uint8_t byte{
                wirespaces::platform::avr::uart0ReadByte()};
            if (!decoder.push(byte)) {
                continue;
            }
            processFrame(decoder.frame(), engine, callbacks);
            decoder.consume();
        }
    }
}


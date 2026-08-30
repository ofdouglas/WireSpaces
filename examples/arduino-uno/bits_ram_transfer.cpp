/**
 * @file bits_ram_transfer.cpp
 * @brief RAM-backed Compact BITS loopback application for Arduino UNO hardware tests.
 */

#include <avr/interrupt.h>
#include <platform/avr/millisecond_clock.h>
#include <platform/avr/uart0.h>
#include <wirespaces/hal/clock.h>
#include <wirespaces/links/uart_hdlc/decoder.h>
#include <wirespaces/links/uart_hdlc/encoder.h>
#include <wirespaces/runtime/core.hpp>
#include <wirespaces/transports/bits/bits.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

constexpr std::uint32_t kBaudRate{115200UL};
constexpr wirespaces::WireNumber kTestWire{1U};
constexpr wirespaces::HostId kArduinoHost{1U};
constexpr wirespaces::HostId kPcHost{2U};
constexpr std::uint8_t kUartEgress{1U};
constexpr std::uint16_t kMaximumObjectSize{256U};
constexpr std::uint16_t kSegmentPayloadSize{24U};
constexpr std::uint16_t kMaximumBitsPayloadSize{
    wirespaces::transport::bits::kSegmentHeaderSize + kSegmentPayloadSize};
constexpr std::size_t kMaximumCanonicalSize{
    sizeof(wirespaces::Header) + kMaximumBitsPayloadSize};
constexpr std::size_t kHdlcCrcSize{2U};
constexpr std::size_t kMaximumFrameCapacity{
    (kMaximumCanonicalSize + kHdlcCrcSize) * 2U + 2U};
constexpr wirespaces::EndpointAddress kUploadEndpoint{
    wirespaces::EndpointAddress::from(wirespaces::Namespace::kUser0, 1U)};
constexpr wirespaces::EndpointAddress kEchoEndpoint{
    wirespaces::EndpointAddress::from(wirespaces::Namespace::kUser0, 2U)};

static_assert(kMaximumBitsPayloadSize >= wirespaces::transport::bits::kSetupSize,
              "BITS packet storage must hold SETUP");
static_assert(kMaximumBitsPayloadSize >= wirespaces::transport::bits::kAckSize,
              "BITS packet storage must hold ACK");

WS_PACKET_BUFFER_DEFINE(BitsPacket, kMaximumBitsPayloadSize);

wirespaces::links::uart_hdlc::HdlcDecoder<kMaximumCanonicalSize> g_hdlc_decoder{};
BitsPacket g_receiver_segment_packet_0{};
BitsPacket g_receiver_segment_packet_1{};
wirespaces::PacketBuffer* g_receiver_segment_slots[]{
    &g_receiver_segment_packet_0,
    &g_receiver_segment_packet_1,
};
BitsPacket g_receiver_datagram_packet{};
BitsPacket g_receiver_transmit_packet{};
BitsPacket g_transmitter_datagram_packet{};
BitsPacket g_transmitter_transmit_packet{};
std::uint8_t g_receive_object[kMaximumObjectSize]{};
std::uint8_t g_echo_object[kMaximumObjectSize]{};

/**
 * @brief Project canonical WireSpaces packets onto the UART HDLC Link.
 */
class UartForwarder final : public wirespaces::PacketForwarder {
public:
    void forward(const wirespaces::PacketBuffer& packet,
                 wirespaces::EgressSet egress_set) noexcept override {
        (void)egress_set;
        if (packet.size() > kMaximumBitsPayloadSize) {
            return;
        }

        const std::size_t canonical_size{sizeof(packet.header()) + packet.size()};
        const auto* canonical_bytes{
            reinterpret_cast<const std::uint8_t*>(&packet.header())};
        std::uint8_t frame_storage[kMaximumFrameCapacity]{};
        const std::size_t frame_size{
            wirespaces::links::uart_hdlc::HdlcEncoder::encode(
                wirespaces::foundation::Span<const std::uint8_t>{
                    canonical_bytes, canonical_size},
                frame_storage)};
        for (std::size_t index{0U}; index < frame_size; ++index) {
            wirespaces::platform::avr::uart0WriteByte(frame_storage[index]);
        }
    }
};

/**
 * @brief Store one complete incoming BITS object in fixed application RAM.
 */
class RamReceiverCallbacks final
    : public wirespaces::transport::bits::ReceiverCallbacks {
public:
    explicit RamReceiverCallbacks(
        wirespaces::MutableByteSpan storage) noexcept
        : storage_{storage} {}

    bool onSegment(std::uint32_t object_offset,
                   wirespaces::ByteSpan payload) noexcept override {
        if ((object_offset + payload.size()) > storage_.size()) {
            return false;
        }
        std::memcpy(storage_.data() + object_offset, payload.data(),
                    payload.size());
        const std::uint16_t segment_end{
            static_cast<std::uint16_t>(object_offset + payload.size())};
        if (segment_end > received_size_) {
            received_size_ = segment_end;
        }
        return true;
    }

    void onDatagram(wirespaces::ByteSpan payload) noexcept override {
        (void)payload;
    }

    void onTransferComplete() noexcept override {
        object_complete_ = true;
    }

    void onTransferAborted() noexcept override {
        received_size_ = 0U;
        object_complete_ = false;
    }

    [[nodiscard]] bool hasCompletedObject() const noexcept {
        return object_complete_;
    }

    [[nodiscard]] wirespaces::ByteSpan completedObject() const noexcept {
        return object_complete_
                   ? wirespaces::ByteSpan{storage_.data(), received_size_}
                   : wirespaces::ByteSpan{};
    }

    void releaseCompletedObject() noexcept {
        received_size_ = 0U;
        object_complete_ = false;
    }

private:
    wirespaces::MutableByteSpan storage_{};
    std::uint16_t received_size_{0U};
    bool object_complete_{false};
};

/**
 * @brief Record terminal transmitter events without adding a test-control protocol.
 */
class RamTransmitterCallbacks final
    : public wirespaces::transport::bits::TransmitterCallbacks {
public:
    void onDatagram(wirespaces::ByteSpan payload) noexcept override {
        (void)payload;
    }

    void onTransferComplete() noexcept override {}

    void onTransferRejected(
        wirespaces::transport::bits::RejectReason reason) noexcept override {
        (void)reason;
    }

    void onTransferAborted() noexcept override {}
};

/**
 * @brief Dispatch all complete, valid canonical packets currently available from UART.
 */
void processUartInput(const wirespaces::Dispatcher& dispatcher) noexcept {
    while (wirespaces::platform::avr::uart0ByteAvailable()) {
        const std::uint8_t byte{wirespaces::platform::avr::uart0ReadByte()};
        if (!g_hdlc_decoder.push(byte)) {
            continue;
        }

        const auto decoded_frame{g_hdlc_decoder.frame()};
        const std::size_t frame_size{decoded_frame.size()};
        if (frame_size >= sizeof(wirespaces::Header) &&
            frame_size <= kMaximumCanonicalSize) {
            BitsPacket packet{};
            const auto payload_size{
                static_cast<std::uint16_t>(frame_size - sizeof(wirespaces::Header))};
            if (packet.resize(payload_size)) {
                std::memcpy(&packet.header(), decoded_frame.data(),
                            sizeof(packet.header()));
                std::memcpy(packet.payload().data(),
                            decoded_frame.data() + sizeof(packet.header()),
                            packet.size());
                (void)dispatcher.dispatch(packet);
            }
        }
        g_hdlc_decoder.consume();
    }
}

[[nodiscard]] bool transmitterBusy(
    wirespaces::transport::bits::TransferState state) noexcept {
    return state == wirespaces::transport::bits::TransferState::kStarting ||
           state == wirespaces::transport::bits::TransferState::kActive;
}

}  // namespace

int main() {
    wirespaces::platform::avr::uart0Init(kBaudRate);
    wirespaces::platform::avr::millisecondClockInit();

    const wirespaces::RouteTableEntry route_entries[]{
        {kTestWire, kUartEgress},
    };
    UartForwarder uart_forwarder{};
    wirespaces::Router router{
        wirespaces::foundation::Span<const wirespaces::RouteTableEntry>{
            route_entries},
        uart_forwarder,
    };
    const wirespaces::HostInfo host_info{kArduinoHost, 1U, {kTestWire}};
    wirespaces::setLocalHostInfo(host_info);

    RamReceiverCallbacks receiver_callbacks{
        wirespaces::MutableByteSpan{g_receive_object}};
    RamTransmitterCallbacks transmitter_callbacks{};

    const wirespaces::transport::bits::ConnectionConfig receiver_connection{
        kTestWire, kArduinoHost, kPcHost, kUploadEndpoint};
    const wirespaces::transport::bits::ConnectionConfig transmitter_connection{
        kTestWire, kArduinoHost, kPcHost, kEchoEndpoint};
    const wirespaces::transport::bits::TimingConfig timing{
        100U,
        250U,
        5U,
    };
    wirespaces::transport::bits::BitsReceiver receiver{
        receiver_connection,
        router,
        receiver_callbacks,
        wirespaces::foundation::Span<wirespaces::PacketBuffer*>{
            g_receiver_segment_slots},
        g_receiver_datagram_packet,
        g_receiver_transmit_packet,
    };
    wirespaces::transport::bits::BitsTransmitter transmitter{
        transmitter_connection,
        timing,
        router,
        transmitter_callbacks,
        g_transmitter_datagram_packet,
        g_transmitter_transmit_packet,
    };
    const wirespaces::DispatchTableEntry dispatch_entries[]{
        {kArduinoHost, kUploadEndpoint, &receiver},
        {kArduinoHost, kEchoEndpoint, &transmitter},
    };
    const wirespaces::Dispatcher dispatcher{
        wirespaces::foundation::Span<const wirespaces::DispatchTableEntry>{
            dispatch_entries},
    };

    std::uint8_t echo_session_id{0x80U};
    sei();
    for (;;) {
        processUartInput(dispatcher);
        (void)receiver.process();

        if (receiver_callbacks.hasCompletedObject() &&
            !transmitterBusy(transmitter.state())) {
            const wirespaces::ByteSpan received_object{
                receiver_callbacks.completedObject()};
            std::memcpy(g_echo_object, received_object.data(),
                        received_object.size());
            const auto start_result{transmitter.startTransfer(
                wirespaces::ByteSpan{g_echo_object, received_object.size()},
                kSegmentPayloadSize,
                echo_session_id,
                static_cast<std::uint8_t>(
                    wirespaces::hal::MillisecondClock::now()))};
            if (start_result ==
                wirespaces::transport::bits::StartResult::kStarted) {
                receiver_callbacks.releaseCompletedObject();
                echo_session_id =
                    static_cast<std::uint8_t>(echo_session_id + 1U);
            }
        }

        (void)transmitter.process(
            wirespaces::hal::MillisecondClock::now());
    }
}

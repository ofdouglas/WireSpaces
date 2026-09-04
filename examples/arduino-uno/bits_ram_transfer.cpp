/**
 * @file bits_ram_transfer.cpp
 * @brief RAM-backed Compact BITS loopback application for Arduino UNO hardware tests.
 */

#include <avr/interrupt.h>
#include <platform/avr/millisecond_clock.h>
#include <platform/avr/uart0.h>
#include <wirespaces/hal/clock.h>
#include <wirespaces/runtime/core.hpp>
#include <wirespaces/transports/bits/bits.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "uart_hdlc_link.h"
#include "wiring_constants.h"

namespace {

constexpr std::uint16_t kMaximumObjectSize{256U};
constexpr std::uint16_t kSegmentPayloadSize{24U};
constexpr std::uint16_t kMaximumBitsPayloadSize{
    wirespaces::transport::bits::kSegmentHeaderSize + kSegmentPayloadSize};

static_assert(kMaximumBitsPayloadSize >= wirespaces::transport::bits::kSetupSize,
              "BITS packet storage must hold SETUP");
static_assert(kMaximumBitsPayloadSize >= wirespaces::transport::bits::kAckSize,
              "BITS packet storage must hold ACK");

WS_PACKET_BUFFER_DEFINE(BitsPacket, kMaximumBitsPayloadSize);

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

[[nodiscard]] bool transmitterBusy(
    wirespaces::transport::bits::TransferState state) noexcept {
    return state == wirespaces::transport::bits::TransferState::kStarting ||
           state == wirespaces::transport::bits::TransferState::kActive;
}

}  // namespace

int main() {
    wirespaces::platform::avr::uart0Init(wiring_constants::kUartBaudRate);
    wirespaces::platform::avr::millisecondClockInit();

    wirespaces::examples::arduino_uno::UartHdlcForwarder<
        kMaximumBitsPayloadSize>
        uart_forwarder{};
    wirespaces::examples::arduino_uno::UartHdlcReceiver<BitsPacket>
        uart_receiver{};
    wirespaces::Router router{
        wirespaces::foundation::Span<const wirespaces::RouteTableEntry>{
            &wiring_constants::kUartRoute, 1U},
        uart_forwarder,
    };
    wirespaces::setLocalHostInfo(wiring_constants::kArduinoHostInfo);

    RamReceiverCallbacks receiver_callbacks{
        wirespaces::MutableByteSpan{g_receive_object}};
    RamTransmitterCallbacks transmitter_callbacks{};

    const wirespaces::transport::bits::ConnectionConfig receiver_connection{
        wiring_constants::kTestWire, wiring_constants::kArduinoHost,
        wiring_constants::kPcHost, wiring_constants::kBitsUploadEndpoint};
    const wirespaces::transport::bits::ConnectionConfig transmitter_connection{
        wiring_constants::kTestWire, wiring_constants::kArduinoHost,
        wiring_constants::kPcHost, wiring_constants::kBitsEchoEndpoint};
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
        {wiring_constants::kArduinoHost, wiring_constants::kBitsUploadEndpoint,
         &receiver},
        {wiring_constants::kArduinoHost, wiring_constants::kBitsEchoEndpoint,
         &transmitter},
    };
    const wirespaces::Dispatcher dispatcher{
        wirespaces::foundation::Span<const wirespaces::DispatchTableEntry>{
            dispatch_entries},
    };

    std::uint8_t echo_session_id{0x80U};
    sei();
    for (;;) {
        uart_receiver.process(dispatcher);
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

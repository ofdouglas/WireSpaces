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

namespace bits = wirespaces::transport::bits;

constexpr std::uint16_t kMaximumObjectSize{256U};
constexpr std::uint16_t kSegmentPayloadSize{24U};
constexpr std::uint16_t kMaximumBitsPayloadSize{bits::kSegmentHeaderSize + kSegmentPayloadSize};

constexpr bits::ConnectionConfig kUploadConnection{
    wiring_constants::kTestWire, wiring_constants::kArduinoHost, wiring_constants::kPcHost,
    wiring_constants::kBitsUploadEndpoint};

constexpr bits::ConnectionConfig kEchoConnection{
    wiring_constants::kTestWire, wiring_constants::kArduinoHost, wiring_constants::kPcHost,
    wiring_constants::kBitsEchoEndpoint};

constexpr bits::TimingConfig kTransferTiming{100U, 250U, 5U};

static_assert(kMaximumBitsPayloadSize >= bits::kSetupSize, "BITS packet storage must hold SETUP");
static_assert(kMaximumBitsPayloadSize >= bits::kAckSize, "BITS packet storage must hold ACK");

WS_PACKET_BUFFER_DEFINE(BitsPacket, kMaximumBitsPayloadSize);

using UartForwarder = wirespaces::examples::arduino_uno::UartHdlcForwarder<kMaximumBitsPayloadSize>;
using UartReceiver = wirespaces::examples::arduino_uno::UartHdlcReceiver<BitsPacket>;
using PacketBuffer = wirespaces::PacketBuffer;
using PacketSlotSpan = wirespaces::foundation::Span<wirespaces::PacketBuffer*>;
using DispatchSpan = wirespaces::foundation::Span<const wirespaces::DispatchTableEntry>;

/**
 * @brief Store one complete incoming BITS object in fixed application RAM.
 */
class RamReceiverCallbacks final : public bits::ReceiverCallbacks {
public:
    explicit RamReceiverCallbacks(wirespaces::MutableByteSpan storage) noexcept : storage_{storage} {}

    bool onSegment(std::uint32_t object_offset, wirespaces::ByteSpan payload) noexcept override {
        if ((object_offset + payload.size()) > storage_.size()) {
            return false;
        }
        std::memcpy(storage_.data() + object_offset, payload.data(), payload.size());
        const auto segment_end{static_cast<std::uint16_t>(object_offset + payload.size())};
        if (segment_end > received_size_) {
            received_size_ = segment_end;
        }
        return true;
    }

    void onDatagram(wirespaces::ByteSpan payload) noexcept override { (void)payload; }

    void onTransferComplete() noexcept override { object_complete_ = true; }

    void onTransferAborted() noexcept override {
        received_size_ = 0U;
        object_complete_ = false;
    }

    [[nodiscard]] bool hasCompletedObject() const noexcept {
        return object_complete_;
    }

    [[nodiscard]] wirespaces::ByteSpan completedObject() const noexcept {
        return object_complete_ ? wirespaces::ByteSpan{storage_.data(), received_size_}
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
class RamTransmitterCallbacks final : public bits::TransmitterCallbacks {
public:
    void onDatagram(wirespaces::ByteSpan payload) noexcept override { (void)payload; }

    void onTransferComplete() noexcept override {}

    void onTransferRejected(bits::RejectReason reason) noexcept override {
        (void)reason;
    }

    void onTransferAborted() noexcept override {}
};

[[nodiscard]] bool transmitterBusy(bits::TransferState state) noexcept {
    return state == bits::TransferState::kStarting || state == bits::TransferState::kActive;
}

/**
 * @brief Own and poll the complete RAM-backed Compact BITS loopback image.
 *
 * All fixed-capacity packet and object storage is held by this application so
 * its RAM cost and lifetime are visible at the composition boundary.
 */
class BitsRamTransferApplication final {
public:
    BitsRamTransferApplication() noexcept = default;
    BitsRamTransferApplication(const BitsRamTransferApplication&) = delete;
    BitsRamTransferApplication(BitsRamTransferApplication&&) = delete;
    BitsRamTransferApplication& operator=(const BitsRamTransferApplication&) = delete;
    BitsRamTransferApplication& operator=(BitsRamTransferApplication&&) = delete;

    /** @brief Initialize target peripherals and process-wide host identity. */
    void initialize() noexcept;

    /** @brief Poll ingress and advance both Compact BITS roles once. */
    void runOnce() noexcept;

private:
    /** @brief Copy a completed receive object into stable storage and begin its echo transfer. */
    void startEcho(wirespaces::ByteSpan received_object) noexcept;

    BitsPacket segment_packets_[2U]{};
    PacketBuffer* segment_slots_[2U]{&segment_packets_[0], &segment_packets_[1]};
    BitsPacket receiver_datagram_packet_{};
    BitsPacket receiver_transmit_packet_{};
    BitsPacket transmitter_datagram_packet_{};
    BitsPacket transmitter_transmit_packet_{};
    std::uint8_t receive_object_[kMaximumObjectSize]{};
    std::uint8_t echo_storage_[kMaximumObjectSize]{};

    UartForwarder uart_forwarder_{};
    UartReceiver uart_receiver_{};

    demo_wiring::Forwarder egress_forwarder_{uart_forwarder_};
    wirespaces::Router router_{demo_wiring::routes(), egress_forwarder_};
    
    RamReceiverCallbacks receiver_callbacks_{wirespaces::MutableByteSpan{receive_object_}};
    RamTransmitterCallbacks transmitter_callbacks_{};

    bits::BitsReceiver receiver_{kUploadConnection, router_, receiver_callbacks_,
                                PacketSlotSpan{segment_slots_}, receiver_datagram_packet_,
                                receiver_transmit_packet_};
    bits::BitsTransmitter transmitter_{kEchoConnection, kTransferTiming, router_, transmitter_callbacks_,
                                      transmitter_datagram_packet_, transmitter_transmit_packet_};

    wirespaces::DispatchTableEntry dispatch_entries_[2U]{
        {wiring_constants::kBitsUploadEndpoint, &receiver_},
        {wiring_constants::kBitsEchoEndpoint, &transmitter_}};
    wirespaces::Dispatcher dispatcher_{DispatchSpan{dispatch_entries_}};
    std::uint8_t session_id_{0x80U};
};

// --- BitsRamTransferApplication implementations ---

void BitsRamTransferApplication::initialize() noexcept {
    wirespaces::platform::avr::uart0Init(wiring_constants::kUartBaudRate);
    wirespaces::platform::avr::millisecondClockInit();
    wirespaces::setLocalHostInfo(wiring_constants::kArduinoHostInfo);
}

void BitsRamTransferApplication::runOnce() noexcept {
    uart_receiver_.process(router_, dispatcher_, demo_wiring::kUartIngressIndex);
    (void)receiver_.process();

    if (receiver_callbacks_.hasCompletedObject() && !transmitterBusy(transmitter_.state())) {
        startEcho(receiver_callbacks_.completedObject());
    }

    (void)transmitter_.process(wirespaces::hal::MillisecondClock::now());
}

void BitsRamTransferApplication::startEcho(wirespaces::ByteSpan received_object) noexcept {
    std::memcpy(echo_storage_, received_object.data(), received_object.size());

    const wirespaces::ByteSpan echo{echo_storage_, received_object.size()};
    const auto sequence{static_cast<std::uint8_t>(wirespaces::hal::MillisecondClock::now())};
    const auto result{transmitter_.startTransfer(echo, kSegmentPayloadSize, session_id_, sequence)};
    if (result != bits::StartResult::kStarted) {
        return;
    }

    receiver_callbacks_.releaseCompletedObject();
    session_id_ = static_cast<std::uint8_t>(session_id_ + 1U);
}

}  // namespace

int main() {
    static BitsRamTransferApplication application{};
    application.initialize();
    sei();
    for (;;) {
        application.runOnce();
    }
}

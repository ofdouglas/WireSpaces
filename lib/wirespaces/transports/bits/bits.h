/**
 * @file bits.h
 * @brief Compact Binary Image Transport, Segmented (BITS) endpoints.
 */

#pragma once

#include <wirespaces/core/dispatch.h>
#include <wirespaces/core/router.h>
#include <wirespaces/foundation/span.h>
#include <wirespaces/transports/bits/codec.h>
#include <wirespaces/transports/bits/receiver_engine.h>

#include <cstdint>

namespace wirespaces::transport::bits {

/** @brief Immutable point-to-point identity of one BITS connection. */
using ConnectionConfig = ConnectionAddress;

/** @brief Caller-selected retransmission behavior, in monotonic milliseconds. */
struct TimingConfig {
    uint32_t retransmission_timeout_ms{100U};
    uint32_t probe_timeout_ms{250U};
    uint8_t max_retries{5U};
};

/** @brief Result of starting a transmitter-side transfer. */
enum class StartResult : uint8_t {
    kStarted = 0U,
    kBusy,
    kInvalidArgument,
    kPacketTooSmall,
};


/** @brief Service callbacks invoked by BitsTransmitter::process(). */
class TransmitterCallbacks {
public:
    /** @brief Deliver one connection-scoped unreliable sideband datagram. */
    virtual void onDatagram(ByteSpan payload) noexcept = 0;
    /** @brief Notify that the receiver has acknowledged the complete object. */
    virtual void onTransferComplete() noexcept = 0;
    /** @brief Notify that SETUP was rejected by the receiver. */
    virtual void onTransferRejected(RejectReason reason) noexcept = 0;
    /** @brief Notify that the active transfer was aborted locally or by its peer. */
    virtual void onTransferAborted() noexcept = 0;

protected:
    ~TransmitterCallbacks() = default;
};

/**
 * @brief Compact BITS receiver with caller-owned bounded ingress storage.
 *
 * Up to sixteen packet slots form the advertised receive window. receive() only validates,
 * classifies, and copies; protocol state and callbacks are advanced later by process().
 */
class BitsReceiver final : public EndpointReceiver, private ReceiverPduSender {
public:
    BitsReceiver(const ConnectionConfig& connection, Router& router, ReceiverCallbacks& callbacks,
                 foundation::Span<PacketBuffer*> segment_ingress,
                 PacketBuffer& datagram_ingress, PacketBuffer& transmit_packet) noexcept;

    ReceiveResult receive(const PacketBuffer& packet) noexcept override;
    ProcessResult process() noexcept;
    SendResult sendDatagram(ByteSpan payload) noexcept;
    SendResult abort() noexcept;
    TransferState state() const noexcept { return engine_.state(); }

private:
    [[nodiscard]] MutableByteSpan prepare(uint16_t payload_size) noexcept override;
    [[nodiscard]] bool sendPrepared() noexcept override;

    ConnectionConfig connection_{};
    Router& router_;
    foundation::Span<PacketBuffer*> segment_ingress_{};
    PacketBuffer& datagram_ingress_;
    PacketBuffer& transmit_packet_;
    uint16_t segment_occupied_mask_{0U};
    bool datagram_occupied_{false};
    BitsReceiverEngine engine_;
};

/**
 * @brief Compact BITS transmitter backed by stable caller-owned object bytes.
 *
 * The object passed to startTransfer() must remain valid until the transfer reaches a terminal
 * state. process() accepts a wrapping monotonic millisecond time and performs bounded work.
 */
class BitsTransmitter final : public EndpointReceiver {
public:
    BitsTransmitter(const ConnectionConfig& connection, const TimingConfig& timing, Router& router,
                    TransmitterCallbacks& callbacks, PacketBuffer& datagram_ingress,
                    PacketBuffer& transmit_packet) noexcept;

    ReceiveResult receive(const PacketBuffer& packet) noexcept override;
    ProcessResult process(uint32_t now_ms) noexcept;
    SendResult sendDatagram(ByteSpan payload) noexcept;
    SendResult abort() noexcept;
    [[nodiscard]] StartResult startTransfer(ByteSpan object, uint16_t segment_size,
                                            uint8_t session_id,
                                            uint8_t initial_sequence_number) noexcept;
    TransferState state() const noexcept { return state_; }

private:
    [[nodiscard]] bool handleDatagram() noexcept;
    [[nodiscard]] bool handleAck(ByteSpan payload) noexcept;
    [[nodiscard]] bool handleReject(ByteSpan payload) noexcept;
    [[nodiscard]] bool handleAbort(ByteSpan payload) noexcept;
    [[nodiscard]] bool sendSetup(uint32_t now_ms, bool retransmission) noexcept;
    [[nodiscard]] bool sendSegment(uint8_t window_offset, uint32_t now_ms,
                                   bool retransmission) noexcept;
    [[nodiscard]] bool sendProbe(uint32_t now_ms) noexcept;
    bool sendAbort() noexcept;
    [[nodiscard]] bool preparePacket(uint16_t payload_size, QoS qos) noexcept;
    [[nodiscard]] bool forwardPacket() noexcept;
    bool retryLimitReached(uint8_t retry_count) const noexcept;
    void shiftWindow(uint8_t count) noexcept;
    void resetTransferTracking() noexcept;
    void transitionToAborted() noexcept;

    ConnectionConfig connection_{};
    TimingConfig timing_{};
    Router& router_;
    TransmitterCallbacks& callbacks_;
    PacketBuffer& datagram_ingress_;
    PacketBuffer& transmit_packet_;
    bool datagram_occupied_{false};
    TransferState state_{TransferState::kIdle};
    ByteSpan object_{};
    Setup setup_{};
    uint32_t segment_count_{0U};
    uint32_t acknowledged_count_{0U};
    uint32_t granted_end_{0U};
    uint16_t sent_bitmap_{0U};
    uint16_t acknowledged_bitmap_{0U};
    uint32_t segment_last_send_ms_[kCompactWindowWidth]{};
    uint8_t segment_retry_count_[kCompactWindowWidth]{};
    uint32_t setup_last_send_ms_{0U};
    uint32_t probe_last_send_ms_{0U};
    uint8_t setup_retry_count_{0U};
    uint8_t probe_retry_count_{0U};
    bool setup_sent_{false};
    bool probe_timer_active_{false};
};

}  // namespace wirespaces::transport::bits

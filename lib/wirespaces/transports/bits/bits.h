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

/** @brief Non-owning receiver packet storage supplied by the application. */
struct ReceiverStorage {
    foundation::Span<PacketBuffer*> segment_ingress{};
    PacketBuffer& datagram_ingress;
    PacketBuffer& transmit_packet;
};

/** @brief Non-owning transmitter packet storage supplied by the application. */
struct TransmitterStorage {
    PacketBuffer& datagram_ingress;
    PacketBuffer& transmit_packet;
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
    BitsReceiver(ConnectionConfig connection, Router& router, ReceiverCallbacks& callbacks,
                 ReceiverStorage storage,
                 uint32_t inactivity_timeout_ms = kDefaultReceiverInactivityTimeoutMs) noexcept;

    ReceiveResult receive(const PacketBuffer& packet) noexcept override;
    /** @brief Poll ingress and expiry with wrapping monotonic milliseconds.
     * Call even without traffic. Expiry reports kError once and discards queued backlog.
     * Zero constructor timeout disables expiry only when the application owns recovery.
     * All receive/process/abort calls require serialized access to this receiver.
     */
    ProcessResult process(uint32_t now_ms) noexcept;
    SendResult sendDatagram(ByteSpan payload) noexcept;
    SendResult abort() noexcept;
    TransferState state() const noexcept { return engine_.state(); }

private:
    [[nodiscard]] MutableByteSpan prepare(uint16_t payload_size) noexcept override;
    SendResult sendPrepared() noexcept override;

    ConnectionConfig connection_{};
    Router& router_;
    ReceiverStorage storage_;
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
    BitsTransmitter(ConnectionConfig connection, TimingConfig timing, Router& router,
                    TransmitterCallbacks& callbacks, TransmitterStorage storage) noexcept;

    ReceiveResult receive(const PacketBuffer& packet) noexcept override;
    ProcessResult process(uint32_t now_ms) noexcept;
    SendResult sendDatagram(ByteSpan payload) noexcept;
    SendResult abort() noexcept;
    [[nodiscard]] StartResult startTransfer(ByteSpan object, uint16_t segment_size,
                                            uint8_t session_id,
                                            uint8_t initial_sequence_number) noexcept;
    TransferState state() const noexcept { return session_.state; }

private:
    /** @brief Mutable state belonging to the current or most recent object transfer. */
    struct TransferSession {
        TransferState state{TransferState::kIdle};
        ByteSpan object{};
        Setup setup{};
        uint32_t segment_count{0U};
        uint32_t acknowledged_count{0U};
        uint32_t granted_end{0U};
        uint16_t sent_bitmap{0U};
        uint16_t acknowledged_bitmap{0U};
        uint32_t segment_last_send_ms[kCompactWindowWidth]{};
        uint8_t segment_retry_count[kCompactWindowWidth]{};
        uint32_t setup_last_send_ms{0U};
        uint32_t probe_last_send_ms{0U};
        uint8_t setup_retry_count{0U};
        uint8_t probe_retry_count{0U};
        bool setup_sent{false};
        bool probe_timer_active{false};
    };

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
    TransmitterStorage storage_;
    bool datagram_occupied_{false};
    TransferSession session_{};
    SendResult last_send_{SendResult::kSent};
};

}  // namespace wirespaces::transport::bits

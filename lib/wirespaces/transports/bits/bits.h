/**
 * @file bits.h
 * @brief Compact Binary Image Transport, Segmented (BITS) endpoints.
 */

#pragma once

#include <wirespaces/core/dispatch.h>
#include <wirespaces/core/router.h>
#include <wirespaces/transports/bits/codec.h>

#include <cstdint>

namespace wirespaces::transport::bits {

/** @brief Immutable point-to-point identity of one BITS connection. */
struct ConnectionConfig {
    WireNumber wire{};
    HostId local_host{};
    HostId remote_host{};
    EndpointAddress endpoint{};
};

/** @brief Result of one bounded BITS processing step. */
enum class ProcessResult : uint8_t {
    kIdle = 0U,
    kProgress,
    kError,
};

/** @brief Observable state of a BITS segmented transfer. */
enum class TransferState : uint8_t {
    kIdle = 0U,
    kStarting,
    kActive,
    kCompleted,
    kError,
};

/** @brief Result of starting a transmitter-side transfer. */
enum class StartResult : uint8_t {
    kStarted = 0U,
    kBusy,
    kInvalidArgument,
    kPacketTooSmall,
};

/** @brief Result of an immediate sideband datagram send request. */
enum class SendResult : uint8_t {
    kSent = 0U,
    kTooLarge,
    kNoRoute,
};

/** @brief Service callbacks invoked by BitsReceiver::process(). */
class ReceiverCallbacks {
public:
    /**
     * @brief Store one segment at its absolute object offset.
     * @return True only when the sink accepted the bytes.
     */
    virtual bool onSegment(uint32_t object_offset, ByteSpan payload) noexcept = 0;
    /** @brief Deliver one connection-scoped unreliable sideband datagram. */
    virtual void onDatagram(ByteSpan payload) noexcept = 0;
    /** @brief Notify that every object segment has been accepted by the sink. */
    virtual void onTransferComplete() noexcept = 0;

protected:
    ~ReceiverCallbacks() = default;
};

/** @brief Service callbacks invoked by BitsTransmitter::process(). */
class TransmitterCallbacks {
public:
    /** @brief Deliver one connection-scoped unreliable sideband datagram. */
    virtual void onDatagram(ByteSpan payload) noexcept = 0;
    /** @brief Notify that the receiver has acknowledged the complete object. */
    virtual void onTransferComplete() noexcept = 0;

protected:
    ~TransmitterCallbacks() = default;
};

/**
 * @brief Compact BITS receiver with caller-owned depth-one ingress slots.
 *
 * receive() performs only connection validation, message classification, and a bounded copy.
 * Protocol processing and Service callbacks occur later from process().
 */
class BitsReceiver final : public EndpointReceiver {
public:
    BitsReceiver(const ConnectionConfig& connection, Router& router, ReceiverCallbacks& callbacks,
                 PacketBuffer& segment_ingress, PacketBuffer& datagram_ingress,
                 PacketBuffer& transmit_packet) noexcept;

    ReceiveResult receive(const PacketBuffer& packet) noexcept override;
    [[nodiscard]] ProcessResult process() noexcept;
    [[nodiscard]] SendResult sendDatagram(ByteSpan payload) noexcept;
    [[nodiscard]] TransferState state() const noexcept { return state_; }

private:
    [[nodiscard]] bool handleSegment() noexcept;
    [[nodiscard]] bool handleDatagram() noexcept;
    [[nodiscard]] bool handleSetup(ByteSpan payload) noexcept;
    [[nodiscard]] bool sendAck() noexcept;
    [[nodiscard]] bool preparePacket(uint16_t payload_size, QoS qos) noexcept;
    [[nodiscard]] bool forwardPacket() noexcept;

    ConnectionConfig connection_{};
    Router& router_;
    ReceiverCallbacks& callbacks_;
    PacketBuffer& segment_ingress_;
    PacketBuffer& datagram_ingress_;
    PacketBuffer& transmit_packet_;
    bool segment_occupied_{false};
    bool datagram_occupied_{false};
    TransferState state_{TransferState::kIdle};
    Setup setup_{};
    uint32_t segment_count_{0U};
    uint32_t received_segment_count_{0U};
};

/**
 * @brief Compact BITS transmitter backed by stable caller-owned object bytes.
 *
 * The object passed to startTransfer() must remain valid until the transfer reaches a terminal
 * state. One SETUP or SEGMENT is emitted per process() call.
 */
class BitsTransmitter final : public EndpointReceiver {
public:
    BitsTransmitter(const ConnectionConfig& connection, Router& router,
                    TransmitterCallbacks& callbacks, PacketBuffer& datagram_ingress,
                    PacketBuffer& transmit_packet) noexcept;

    ReceiveResult receive(const PacketBuffer& packet) noexcept override;
    [[nodiscard]] ProcessResult process() noexcept;
    [[nodiscard]] SendResult sendDatagram(ByteSpan payload) noexcept;
    [[nodiscard]] StartResult startTransfer(ByteSpan object, uint16_t segment_size,
                                            uint8_t session_id,
                                            uint8_t initial_sequence_number) noexcept;
    [[nodiscard]] TransferState state() const noexcept { return state_; }

private:
    [[nodiscard]] bool handleDatagram() noexcept;
    [[nodiscard]] bool handleAck(ByteSpan payload) noexcept;
    [[nodiscard]] bool sendSetup() noexcept;
    [[nodiscard]] bool sendNextSegment() noexcept;
    [[nodiscard]] bool preparePacket(uint16_t payload_size, QoS qos) noexcept;
    [[nodiscard]] bool forwardPacket() noexcept;

    ConnectionConfig connection_{};
    Router& router_;
    TransmitterCallbacks& callbacks_;
    PacketBuffer& datagram_ingress_;
    PacketBuffer& transmit_packet_;
    bool datagram_occupied_{false};
    TransferState state_{TransferState::kIdle};
    ByteSpan object_{};
    Setup setup_{};
    uint32_t segment_count_{0U};
    uint32_t next_segment_index_{0U};
    uint16_t outstanding_segment_index_{0U};
    uint8_t max_receive_sequence_{0U};
    bool setup_sent_{false};
    bool waiting_for_ack_{false};
};

}  // namespace wirespaces::transport::bits

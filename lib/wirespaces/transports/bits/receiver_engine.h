/**
 * @file receiver_engine.h
 * @brief Synchronous Compact BITS receiver state machine.
 */

#pragma once

#include <wirespaces/core/packet.h>
#include <wirespaces/transports/bits/codec.h>

#include <cstdint>

#ifndef WIRESPACES_BITS_RECEIVER_MAX_WINDOW_WIDTH
#define WIRESPACES_BITS_RECEIVER_MAX_WINDOW_WIDTH 16
#endif

namespace wirespaces::transport::bits {

static_assert(WIRESPACES_BITS_RECEIVER_MAX_WINDOW_WIDTH >= 1 &&
              WIRESPACES_BITS_RECEIVER_MAX_WINDOW_WIDTH <= kCompactWindowWidth,
              "BITS receiver build-time window limit must be 1..16");

/** @brief Result of one bounded BITS processing step. */
enum class ProcessResult : uint8_t {
    kIdle = 0U,
    kProgress,
    kError,
    kBlocked,
};

/** @brief Observable state of a BITS segmented transfer. */
enum class TransferState : uint8_t {
    kIdle = 0U,
    kStarting,
    kActive,
    kCompleted,
    kRejected,
    kAborted,
    kError,
};

/** @brief Result of an immediate BITS send request. */
enum class SendResult : uint8_t {
    kSent = 0U,
    kTooLarge,
    kNoRoute,
    kInvalidState,
    kFull,
    kRejected,
    kPartial,
};

struct TransferInfo {
    uint8_t session_id;
    uint32_t total_size;
    uint16_t segment_size;
};
enum class TransferAdmission : uint8_t { kAccepted, kBusy, kTooLarge, kRejected };
enum class FailureReason : uint8_t { kSinkRejected, kSendFailed };
enum class AbortReason : uint8_t { kLocal, kPeer };

/** @brief Service callbacks invoked synchronously by BitsReceiverEngine. */
class ReceiverCallbacks {
public:
    /** @brief Validate/reserve sink storage once per accepted transfer, after protocol validation.
     * Duplicate active Setup does not call this again. No reentrant engine calls here.
     */
    virtual TransferAdmission beginTransfer(const TransferInfo& info) noexcept = 0;
    /** @brief Exactly one failure notification for an active transfer that cannot continue. */
    virtual void onTransferFailed(FailureReason reason) noexcept = 0;
    /** @return True only when the sink accepted the segment bytes. */
    virtual bool onSegment(uint32_t object_offset, ByteSpan payload) noexcept = 0;
    /** @brief Deliver one connection-scoped unreliable sideband datagram. */
    virtual void onDatagram(ByteSpan payload) noexcept = 0;
    /** @brief Notify that every object segment has been accepted by the sink. */
    virtual void onTransferComplete() noexcept = 0;
    /** @brief Notify that the active transfer was aborted locally or by its peer. */
    virtual void onTransferAborted(AbortReason reason) noexcept = 0;

protected:
    ~ReceiverCallbacks() = default;
};

/**
 * @brief Supplies caller-owned payload storage and transmits one encoded BITS PDU.
 *
 * The engine has no dependency on Router, Dispatcher, queues, or a physical link.
 */
class ReceiverPduSender {
public:
    /** @return Exactly payload_size writable bytes, or an empty span when unavailable. */
    virtual MutableByteSpan prepare(uint16_t payload_size) noexcept = 0;
    /** @brief Transmit the PDU most recently prepared by prepare(). */
    virtual SendResult sendPrepared() noexcept = 0;

protected:
    ~ReceiverPduSender() = default;
};

/** @brief Static resource limits for one synchronous Compact BITS receiver. */
struct ReceiverEngineConfig {
    uint16_t maximum_segment_size{0U};
    uint8_t receive_window_width{WIRESPACES_BITS_RECEIVER_MAX_WINDOW_WIDTH};
    uint32_t maximum_object_size{UINT32_MAX};
};

/**
 * @brief Synchronous Compact BITS receiver independent of endpoint admission and queueing.
 *
 * process() consumes one already-admitted BITS PDU. Segment payloads and user datagrams are
 * valid only for the duration of that call. A width of one provides the constrained,
 * in-order boot profile; widths up to sixteen retain Compact BITS selective ACK behavior.
 */
class BitsReceiverEngine final {
public:
    BitsReceiverEngine(const ReceiverEngineConfig& config,
                       ReceiverCallbacks& callbacks,
                       ReceiverPduSender& sender) noexcept;

    ProcessResult process(ByteSpan message) noexcept;
    SendResult sendDatagram(ByteSpan payload) noexcept;
    SendResult abort() noexcept;
    TransferState state() const noexcept { return state_; }

private:
    [[nodiscard]] bool handleSegment(ByteSpan message) noexcept;
    [[nodiscard]] bool handleControl(ByteSpan message, MessageType type) noexcept;
    [[nodiscard]] bool handleSetup(ByteSpan payload) noexcept;
    [[nodiscard]] bool handleAbort(ByteSpan payload) noexcept;
    [[nodiscard]] bool sendAck() noexcept;
    [[nodiscard]] bool sendReject(uint8_t session_id, RejectReason reason) noexcept;
    bool sendAbort() noexcept;
    void updateGrant() noexcept;
    bool sendPrepared() noexcept;
    void fail(FailureReason reason) noexcept;

    ReceiverEngineConfig config_{};
    ReceiverCallbacks& callbacks_;
    ReceiverPduSender& sender_;
    TransferState state_{TransferState::kIdle};
    Setup setup_{};
    SendResult last_send_{SendResult::kSent};
    uint32_t segment_count_{0U};
    uint32_t contiguous_count_{0U};
#if WIRESPACES_BITS_RECEIVER_MAX_WINDOW_WIDTH == 1
    uint32_t next_object_offset_{0U};
#endif
    uint32_t granted_end_{0U};
#if WIRESPACES_BITS_RECEIVER_MAX_WINDOW_WIDTH > 1
    uint16_t receive_bitmap_{0U};
#endif
};

}  // namespace wirespaces::transport::bits

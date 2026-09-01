/**
 * @file receiver_engine.cpp
 * @brief Synchronous Compact BITS receiver state machine implementation.
 */

#include <wirespaces/transports/bits/receiver_engine.h>

namespace wirespaces::transport::bits {
namespace {

#if WIRESPACES_BITS_RECEIVER_MAX_WINDOW_WIDTH > 1
uint8_t sequenceFor(const Setup& setup, uint32_t segment_index) noexcept {
    return static_cast<uint8_t>(setup.initial_sequence_number +
                                static_cast<uint8_t>(segment_index));
}

uint8_t baseSequence(const Setup& setup, uint32_t contiguous_count) noexcept {
    return contiguous_count == 0U
               ? static_cast<uint8_t>(setup.initial_sequence_number - 1U)
               : sequenceFor(setup, contiguous_count - 1U);
}
#endif

bool setupMatches(const Setup& lhs, const Setup& rhs) noexcept {
    return lhs.session_id == rhs.session_id &&
           lhs.initial_sequence_number == rhs.initial_sequence_number &&
           lhs.final_segment_index == rhs.final_segment_index &&
           lhs.segment_size == rhs.segment_size &&
           lhs.final_segment_size == rhs.final_segment_size;
}

#if WIRESPACES_BITS_RECEIVER_MAX_WINDOW_WIDTH > 1
uint8_t popcount16(uint16_t value) noexcept {
    uint8_t count{0U};
    while (value != 0U) {
        value = static_cast<uint16_t>(value & static_cast<uint16_t>(value - 1U));
        ++count;
    }
    return count;
}

uint16_t lowBitMask(uint8_t count) noexcept {
    if (count >= kCompactWindowWidth) {
        return UINT16_MAX;
    }
    return count == 0U ? 0U : static_cast<uint16_t>((1UL << count) - 1UL);
}
#endif

}  // namespace

BitsReceiverEngine::BitsReceiverEngine(const ReceiverEngineConfig& config,
                                       ReceiverCallbacks& callbacks,
                                       ReceiverPduSender& sender) noexcept
    : config_{config}, callbacks_{callbacks}, sender_{sender} {
    if (config_.receive_window_width > WIRESPACES_BITS_RECEIVER_MAX_WINDOW_WIDTH) {
        config_.receive_window_width = WIRESPACES_BITS_RECEIVER_MAX_WINDOW_WIDTH;
    }
}

ProcessResult BitsReceiverEngine::process(ByteSpan message) noexcept {
    if (message.empty()) {
        state_ = TransferState::kError;
        return ProcessResult::kError;
    }

    Control control{};
    if (!decodeControl(message[0], control)) {
        state_ = TransferState::kError;
        return ProcessResult::kError;
    }

    const bool handled{control.type == MessageType::kSegment
                           ? handleSegment(message)
                           : handleControl(message, control.type)};
    if (!handled) {
        state_ = TransferState::kError;
        return ProcessResult::kError;
    }
    return ProcessResult::kProgress;
}

SendResult BitsReceiverEngine::sendDatagram(ByteSpan payload) noexcept {
    if (payload.size() > (UINT16_MAX - kUserDatagramHeaderSize)) {
        return SendResult::kTooLarge;
    }
    const uint16_t message_size{
        static_cast<uint16_t>(payload.size() + kUserDatagramHeaderSize)};
    const MutableByteSpan storage{sender_.prepare(message_size)};
    if (storage.size() != message_size || !encodeUserDatagram(payload, storage)) {
        return SendResult::kTooLarge;
    }
    return sender_.sendPrepared() ? SendResult::kSent : SendResult::kNoRoute;
}

SendResult BitsReceiverEngine::abort() noexcept {
    if (state_ != TransferState::kActive) {
        return SendResult::kInvalidState;
    }
    const bool sent{sendAbort()};
    state_ = TransferState::kAborted;
    callbacks_.onTransferAborted();
    return sent ? SendResult::kSent : SendResult::kNoRoute;
}

bool BitsReceiverEngine::handleSegment(ByteSpan message) noexcept {
    SegmentHeader header{};
    if (!detail::decodeSegmentHeaderKnownType(message, header)) {
        return false;
    }
    if ((state_ != TransferState::kActive && state_ != TransferState::kCompleted) ||
        header.session_id != setup_.session_id || header.segment_index >= segment_count_) {
        return true;
    }
    if (header.segment_index < contiguous_count_) {
        return sendAck();
    }
    if (state_ == TransferState::kCompleted || header.segment_index >= granted_end_) {
        return sendAck();
    }

#if WIRESPACES_BITS_RECEIVER_MAX_WINDOW_WIDTH == 1
    if (header.segment_index != contiguous_count_) {
        return sendAck();
    }
#else
    const uint32_t window_offset{header.segment_index - contiguous_count_};
    if (window_offset >= config_.receive_window_width) {
        return sendAck();
    }
    const uint16_t segment_bit{static_cast<uint16_t>(1U << window_offset)};
    if ((receive_bitmap_ & segment_bit) != 0U) {
        return sendAck();
    }
#endif

#if WIRESPACES_BITS_RECEIVER_MAX_WINDOW_WIDTH == 1
    const uint32_t object_offset{next_object_offset_};
#else
    const uint32_t object_offset{static_cast<uint32_t>(header.segment_index) *
                                 setup_.segment_size};
#endif
    const uint16_t expected_size{
        header.segment_index == setup_.final_segment_index
            ? setup_.final_segment_size
            : setup_.segment_size};
    const ByteSpan segment_payload{message.subspan(kSegmentHeaderSize)};
    if (segment_payload.size() != expected_size ||
        !callbacks_.onSegment(object_offset, segment_payload)) {
        return false;
    }

#if WIRESPACES_BITS_RECEIVER_MAX_WINDOW_WIDTH == 1
    ++contiguous_count_;
    next_object_offset_ += segment_payload.size();
#else
    receive_bitmap_ = static_cast<uint16_t>(receive_bitmap_ | segment_bit);
    while ((receive_bitmap_ & 1U) != 0U) {
        receive_bitmap_ = static_cast<uint16_t>(receive_bitmap_ >> 1U);
        ++contiguous_count_;
    }
#endif
    updateGrant();

    const bool completed{contiguous_count_ == segment_count_};
    if (completed) {
        state_ = TransferState::kCompleted;
    }
    if (!sendAck()) {
        return false;
    }
    if (completed) {
        callbacks_.onTransferComplete();
    }
    return true;
}

bool BitsReceiverEngine::handleControl(ByteSpan message, MessageType type) noexcept {
    switch (type) {
        case MessageType::kSetup:
            return handleSetup(message);
        case MessageType::kProbe: {
            Probe probe{};
            if (!detail::decodeProbeKnownType(message, probe)) {
                return false;
            }
            if ((state_ != TransferState::kActive &&
                 state_ != TransferState::kCompleted) ||
                probe.session_id != setup_.session_id) {
                return true;
            }
            return sendAck();
        }
        case MessageType::kUserDatagram: {
            ByteSpan payload{};
            if (!detail::decodeUserDatagramKnownType(message, payload)) {
                return false;
            }
            callbacks_.onDatagram(payload);
            return true;
        }
        case MessageType::kAbort:
            return handleAbort(message);
        case MessageType::kSegment:
        case MessageType::kAck:
        case MessageType::kReject:
            return false;
    }
    return false;
}

bool BitsReceiverEngine::handleSetup(ByteSpan payload) noexcept {
    if (payload.size() < 2U) {
        return false;
    }

    Setup requested{};
    if (!detail::decodeSetupKnownType(payload, requested)) {
        return sendReject(payload[1], RejectReason::kInvalidArgument);
    }
    if (requested.segment_size == 0U || requested.final_segment_size == 0U ||
        requested.final_segment_size > requested.segment_size) {
        return sendReject(requested.session_id, RejectReason::kInvalidArgument);
    }

    const uint32_t requested_total_size{setupTotalSize(requested)};
    if (requested_total_size > config_.maximum_object_size) {
        return sendReject(requested.session_id, RejectReason::kObjectTooLarge);
    }
    const uint32_t requested_segment_count{setupSegmentCount(requested)};
    if (config_.receive_window_width == 0U ||
        requested.segment_size > config_.maximum_segment_size) {
        return sendReject(requested.session_id,
                          RejectReason::kUnsupportedSegmentSize);
    }

    if (state_ == TransferState::kActive) {
        if (setupMatches(setup_, requested)) {
            return sendAck();
        }
        return sendReject(requested.session_id, RejectReason::kBusy);
    }

    setup_ = requested;
    segment_count_ = requested_segment_count;
    contiguous_count_ = 0U;
#if WIRESPACES_BITS_RECEIVER_MAX_WINDOW_WIDTH == 1
    next_object_offset_ = 0U;
#endif
    granted_end_ = 0U;
#if WIRESPACES_BITS_RECEIVER_MAX_WINDOW_WIDTH > 1
    receive_bitmap_ = 0U;
#endif
    state_ = TransferState::kActive;
    updateGrant();
    return sendAck();
}

bool BitsReceiverEngine::handleAbort(ByteSpan payload) noexcept {
    Abort abort_message{};
    if (!detail::decodeAbortKnownType(payload, abort_message)) {
        return false;
    }
    if (state_ != TransferState::kActive ||
        abort_message.session_id != setup_.session_id) {
        return true;
    }
    state_ = TransferState::kAborted;
    callbacks_.onTransferAborted();
    return true;
}

bool BitsReceiverEngine::sendAck() noexcept {
#if WIRESPACES_BITS_RECEIVER_MAX_WINDOW_WIDTH == 1
    const uint8_t window_base{static_cast<uint8_t>(
        setup_.initial_sequence_number +
        static_cast<uint8_t>(contiguous_count_) - 1U)};
    const uint8_t max_receive_sequence{
        contiguous_count_ < segment_count_
            ? static_cast<uint8_t>(window_base + 1U)
            : window_base};
#else
    const uint8_t window_base{baseSequence(setup_, contiguous_count_)};
    const uint8_t max_receive_sequence{
        granted_end_ > contiguous_count_
            ? sequenceFor(setup_, granted_end_ - 1U)
            : window_base};
#endif
    const MutableByteSpan storage{sender_.prepare(kAckSize)};
    if (storage.size() != kAckSize) {
        return false;
    }
#if WIRESPACES_BITS_RECEIVER_MAX_WINDOW_WIDTH == 1
    storage[0] = encodeControl(MessageType::kAck);
    storage[1] = setup_.session_id;
    storage[2] = 0U;
    storage[3] = 0U;
    storage[4] = max_receive_sequence;
    storage[5] = window_base;
#else
    const uint8_t window_span{
        static_cast<uint8_t>(granted_end_ - contiguous_count_)};
    const Ack ack{setup_.session_id,
                  static_cast<uint16_t>(receive_bitmap_ &
                                        lowBitMask(window_span)),
                  max_receive_sequence, window_base};
    if (!encodeAck(ack, storage)) {
        return false;
    }
#endif
    return sender_.sendPrepared();
}

bool BitsReceiverEngine::sendReject(uint8_t session_id,
                                    RejectReason reason) noexcept {
    const Reject reject{session_id, reason};
    const MutableByteSpan storage{sender_.prepare(kRejectSize)};
    return storage.size() == kRejectSize && encodeReject(reject, storage) &&
           sender_.sendPrepared();
}

bool BitsReceiverEngine::sendAbort() noexcept {
    const Abort abort_message{setup_.session_id};
    const MutableByteSpan storage{sender_.prepare(kAbortSize)};
    return storage.size() == kAbortSize && encodeAbort(abort_message, storage) &&
           sender_.sendPrepared();
}

void BitsReceiverEngine::updateGrant() noexcept {
#if WIRESPACES_BITS_RECEIVER_MAX_WINDOW_WIDTH == 1
    granted_end_ = contiguous_count_ < segment_count_
                       ? contiguous_count_ + 1U
                       : contiguous_count_;
#else
    uint32_t window_span{granted_end_ - contiguous_count_};
    uint8_t unreceived{static_cast<uint8_t>(
        window_span - popcount16(static_cast<uint16_t>(
                          receive_bitmap_ & lowBitMask(
                                                static_cast<uint8_t>(window_span)))))};
    while (granted_end_ < segment_count_ &&
           window_span < config_.receive_window_width &&
           unreceived < config_.receive_window_width) {
        ++granted_end_;
        ++window_span;
        ++unreceived;
    }
#endif
}

}  // namespace wirespaces::transport::bits

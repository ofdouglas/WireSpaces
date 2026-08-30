/**
 * @file bits.cpp
 * @brief Compact BITS receiver and transmitter implementation.
 */

#include <wirespaces/transports/bits/bits.h>

#include <cstring>

namespace wirespaces::transport::bits {
namespace {

bool matchesConnection(const PacketBuffer& packet, const ConnectionConfig& connection) noexcept {
    const Header& header{packet.header()};
    return header.transportType() == TransportType::kBits && header.wire == connection.wire &&
           header.source == connection.remote_host && header.destination == connection.local_host &&
           header.endpoint == connection.endpoint;
}

bool copyPacket(const PacketBuffer& source, PacketBuffer& destination) noexcept {
    if (!destination.resize(source.size())) {
        return false;
    }
    destination.header() = source.header();
    if (source.size() > 0U) {
        std::memcpy(destination.payload().data(), source.payload().data(), source.size());
    }
    return true;
}

uint32_t segmentCount(uint32_t total_size, uint16_t segment_size) noexcept {
    const uint32_t whole_segments{total_size / segment_size};
    return whole_segments + ((total_size % segment_size) == 0U ? 0U : 1U);
}

uint8_t sequenceFor(const Setup& setup, uint32_t segment_index) noexcept {
    return static_cast<uint8_t>(setup.initial_sequence_number +
                                static_cast<uint8_t>(segment_index));
}

uint8_t baseSequence(const Setup& setup, uint32_t contiguous_count) noexcept {
    return contiguous_count == 0U
               ? static_cast<uint8_t>(setup.initial_sequence_number - 1U)
               : sequenceFor(setup, contiguous_count - 1U);
}

bool setupMatches(const Setup& lhs, const Setup& rhs) noexcept {
    return lhs.session_id == rhs.session_id &&
           lhs.initial_sequence_number == rhs.initial_sequence_number &&
           lhs.segment_size == rhs.segment_size && lhs.total_size == rhs.total_size;
}

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

bool elapsed(uint32_t now_ms, uint32_t start_ms, uint32_t duration_ms) noexcept {
    return static_cast<uint32_t>(now_ms - start_ms) >= duration_ms;
}

}  // namespace

BitsReceiver::BitsReceiver(const ConnectionConfig& connection, Router& router,
                           ReceiverCallbacks& callbacks,
                           foundation::Span<PacketBuffer*> segment_ingress,
                           PacketBuffer& datagram_ingress,
                           PacketBuffer& transmit_packet) noexcept
    : connection_{connection},
      router_{router},
      callbacks_{callbacks},
      segment_ingress_{segment_ingress},
      datagram_ingress_{datagram_ingress},
      transmit_packet_{transmit_packet} {}

ReceiveResult BitsReceiver::receive(const PacketBuffer& packet) noexcept {
    if (!matchesConnection(packet, connection_) || packet.payload().empty()) {
        return ReceiveResult::kRejected;
    }

    Control control{};
    if (!decodeControl(packet.payload()[0], control)) {
        return ReceiveResult::kRejected;
    }

    if (control.type != MessageType::kSegment) {
        if (datagram_occupied_) {
            return ReceiveResult::kFull;
        }
        if (!copyPacket(packet, datagram_ingress_)) {
            return ReceiveResult::kRejected;
        }
        datagram_occupied_ = true;
        return ReceiveResult::kAccepted;
    }

    const size_t slot_count{segment_ingress_.size() < kCompactWindowWidth
                                ? segment_ingress_.size()
                                : kCompactWindowWidth};
    bool free_slot_seen{false};
    for (size_t index{0U}; index < slot_count; ++index) {
        const uint16_t bit{static_cast<uint16_t>(1U << index)};
        PacketBuffer* const slot{segment_ingress_[index]};
        if (slot == nullptr || (segment_occupied_mask_ & bit) != 0U) {
            continue;
        }
        free_slot_seen = true;
        if (copyPacket(packet, *slot)) {
            segment_occupied_mask_ = static_cast<uint16_t>(segment_occupied_mask_ | bit);
            return ReceiveResult::kAccepted;
        }
    }
    return free_slot_seen ? ReceiveResult::kRejected : ReceiveResult::kFull;
}

ProcessResult BitsReceiver::process() noexcept {
    bool progressed{false};
    bool failed{false};

    if (segment_occupied_mask_ != 0U) {
        for (size_t index{0U}; index < kCompactWindowWidth; ++index) {
            const uint16_t bit{static_cast<uint16_t>(1U << index)};
            if ((segment_occupied_mask_ & bit) == 0U) {
                continue;
            }
            segment_occupied_mask_ =
                static_cast<uint16_t>(segment_occupied_mask_ & static_cast<uint16_t>(~bit));
            progressed = true;
            failed = segment_ingress_[index] == nullptr ||
                     !handleSegment(*segment_ingress_[index]);
            break;
        }
    }
    if (datagram_occupied_) {
        progressed = true;
        failed = !handleDatagram() || failed;
        datagram_occupied_ = false;
    }

    if (failed) {
        state_ = TransferState::kError;
        return ProcessResult::kError;
    }
    return progressed ? ProcessResult::kProgress : ProcessResult::kIdle;
}

SendResult BitsReceiver::sendDatagram(ByteSpan payload) noexcept {
    if (transmit_packet_.capacity() < kUserDatagramHeaderSize ||
        payload.size() > static_cast<size_t>(transmit_packet_.capacity() -
                                             kUserDatagramHeaderSize)) {
        return SendResult::kTooLarge;
    }
    const uint16_t message_size{static_cast<uint16_t>(payload.size() +
                                                       kUserDatagramHeaderSize)};
    if (!preparePacket(message_size, QoS::kNormal) ||
        !encodeUserDatagram(payload, transmit_packet_.payload())) {
        return SendResult::kTooLarge;
    }
    return forwardPacket() ? SendResult::kSent : SendResult::kNoRoute;
}

SendResult BitsReceiver::abort() noexcept {
    if (state_ != TransferState::kActive) {
        return SendResult::kInvalidState;
    }
    const bool sent{sendAbort()};
    state_ = TransferState::kAborted;
    callbacks_.onTransferAborted();
    return sent ? SendResult::kSent : SendResult::kNoRoute;
}

bool BitsReceiver::handleSegment(PacketBuffer& packet) noexcept {
    const ByteSpan message{packet.payload().data(), packet.size()};
    SegmentHeader header{};
    if (!decodeSegmentHeader(message, header)) {
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

    const uint32_t window_offset{header.segment_index - contiguous_count_};
    if (window_offset >= kCompactWindowWidth) {
        return sendAck();
    }
    const uint16_t segment_bit{static_cast<uint16_t>(1U << window_offset)};
    if ((receive_bitmap_ & segment_bit) != 0U) {
        return sendAck();
    }

    const uint32_t object_offset{static_cast<uint32_t>(header.segment_index) *
                                 setup_.segment_size};
    const uint32_t remaining{setup_.total_size - object_offset};
    const uint16_t expected_size{static_cast<uint16_t>(
        remaining < setup_.segment_size ? remaining : setup_.segment_size)};
    const ByteSpan segment_payload{message.subspan(kSegmentHeaderSize)};
    if (segment_payload.size() != expected_size ||
        !callbacks_.onSegment(object_offset, segment_payload)) {
        return false;
    }

    receive_bitmap_ = static_cast<uint16_t>(receive_bitmap_ | segment_bit);
    while ((receive_bitmap_ & 1U) != 0U) {
        receive_bitmap_ = static_cast<uint16_t>(receive_bitmap_ >> 1U);
        ++contiguous_count_;
    }
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

bool BitsReceiver::handleDatagram() noexcept {
    const ByteSpan message{datagram_ingress_.payload().data(), datagram_ingress_.size()};
    if (message.empty()) {
        return false;
    }

    Control control{};
    if (!decodeControl(message[0], control)) {
        return false;
    }

    switch (control.type) {
        case MessageType::kSetup:
            return handleSetup(message);
        case MessageType::kProbe: {
            Probe probe{};
            if (!decodeProbe(message, probe)) {
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
            if (!decodeUserDatagram(message, payload)) {
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

bool BitsReceiver::handleSetup(ByteSpan payload) noexcept {
    if (payload.size() < 2U) {
        return false;
    }

    Setup requested{};
    if (!decodeSetup(payload, requested)) {
        return sendReject(payload[1], RejectReason::kInvalidArgument);
    }
    if (requested.segment_size == 0U || requested.total_size == 0U) {
        return sendReject(requested.session_id, RejectReason::kInvalidArgument);
    }

    const uint32_t requested_segment_count{segmentCount(requested.total_size,
                                                        requested.segment_size)};
    if (requested_segment_count == 0U ||
        requested_segment_count > kMaximumCompactSegmentCount) {
        return sendReject(requested.session_id, RejectReason::kObjectTooLarge);
    }

    uint8_t usable_slot_count{0U};
    const size_t slot_count{segment_ingress_.size() < kCompactWindowWidth
                                ? segment_ingress_.size()
                                : kCompactWindowWidth};
    for (size_t index{0U}; index < slot_count; ++index) {
        if (segment_ingress_[index] != nullptr &&
            segment_ingress_[index]->capacity() >=
                (static_cast<uint32_t>(requested.segment_size) + kSegmentHeaderSize)) {
            ++usable_slot_count;
        }
    }
    if (usable_slot_count == 0U) {
        return sendReject(requested.session_id, RejectReason::kUnsupportedSegmentSize);
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
    granted_end_ = 0U;
    receive_bitmap_ = 0U;
    state_ = TransferState::kActive;
    updateGrant();
    return sendAck();
}

bool BitsReceiver::handleAbort(ByteSpan payload) noexcept {
    Abort abort_message{};
    if (!decodeAbort(payload, abort_message)) {
        return false;
    }
    if (state_ != TransferState::kActive || abort_message.session_id != setup_.session_id) {
        return true;
    }
    state_ = TransferState::kAborted;
    callbacks_.onTransferAborted();
    return true;
}

bool BitsReceiver::sendAck() noexcept {
    const uint8_t window_base{baseSequence(setup_, contiguous_count_)};
    const uint8_t max_receive_sequence{
        granted_end_ > contiguous_count_ ? sequenceFor(setup_, granted_end_ - 1U) : window_base};
    const uint8_t window_span{static_cast<uint8_t>(granted_end_ - contiguous_count_)};
    const Ack ack{setup_.session_id,
                  static_cast<uint16_t>(receive_bitmap_ & lowBitMask(window_span)),
                  max_receive_sequence, window_base};
    if (!preparePacket(kAckSize, QoS::kNormal) ||
        !encodeAck(ack, transmit_packet_.payload())) {
        return false;
    }
    return forwardPacket();
}

bool BitsReceiver::sendReject(uint8_t session_id, RejectReason reason) noexcept {
    const Reject reject{session_id, reason};
    return preparePacket(kRejectSize, QoS::kNormal) &&
           encodeReject(reject, transmit_packet_.payload()) && forwardPacket();
}

bool BitsReceiver::sendAbort() noexcept {
    const Abort abort_message{setup_.session_id};
    return preparePacket(kAbortSize, QoS::kNormal) &&
           encodeAbort(abort_message, transmit_packet_.payload()) && forwardPacket();
}

bool BitsReceiver::preparePacket(uint16_t payload_size, QoS qos) noexcept {
    if (!transmit_packet_.initialize(payload_size,
                                     ControlFields{qos, false, TransportType::kBits})) {
        return false;
    }
    Header& header{transmit_packet_.header()};
    header.wire = connection_.wire;
    header.source = connection_.local_host;
    header.destination = connection_.remote_host;
    header.endpoint = connection_.endpoint;
    return true;
}

bool BitsReceiver::forwardPacket() noexcept {
    return router_.forward(transmit_packet_) == RouteResult::kForwarded;
}

void BitsReceiver::updateGrant() noexcept {
    uint8_t usable_slot_count{0U};
    const size_t slot_count{segment_ingress_.size() < kCompactWindowWidth
                                ? segment_ingress_.size()
                                : kCompactWindowWidth};
    for (size_t index{0U}; index < slot_count; ++index) {
        if (segment_ingress_[index] != nullptr &&
            segment_ingress_[index]->capacity() >=
                (static_cast<uint32_t>(setup_.segment_size) + kSegmentHeaderSize)) {
            ++usable_slot_count;
        }
    }

    uint32_t window_span{granted_end_ - contiguous_count_};
    uint8_t unreceived{static_cast<uint8_t>(
        window_span - popcount16(static_cast<uint16_t>(
                          receive_bitmap_ & lowBitMask(static_cast<uint8_t>(window_span)))))};
    while (granted_end_ < segment_count_ && window_span < kCompactWindowWidth &&
           unreceived < usable_slot_count) {
        ++granted_end_;
        ++window_span;
        ++unreceived;
    }
}

BitsTransmitter::BitsTransmitter(const ConnectionConfig& connection,
                                 const TimingConfig& timing, Router& router,
                                 TransmitterCallbacks& callbacks,
                                 PacketBuffer& datagram_ingress,
                                 PacketBuffer& transmit_packet) noexcept
    : connection_{connection},
      timing_{timing},
      router_{router},
      callbacks_{callbacks},
      datagram_ingress_{datagram_ingress},
      transmit_packet_{transmit_packet} {}

ReceiveResult BitsTransmitter::receive(const PacketBuffer& packet) noexcept {
    if (!matchesConnection(packet, connection_) || packet.payload().empty()) {
        return ReceiveResult::kRejected;
    }

    Control control{};
    if (!decodeControl(packet.payload()[0], control) ||
        (control.type != MessageType::kAck &&
         control.type != MessageType::kReject &&
         control.type != MessageType::kAbort &&
         control.type != MessageType::kUserDatagram)) {
        return ReceiveResult::kRejected;
    }
    if (datagram_occupied_) {
        return ReceiveResult::kFull;
    }
    if (!copyPacket(packet, datagram_ingress_)) {
        return ReceiveResult::kRejected;
    }
    datagram_occupied_ = true;
    return ReceiveResult::kAccepted;
}

ProcessResult BitsTransmitter::process(uint32_t now_ms) noexcept {
    bool progressed{false};
    if (datagram_occupied_) {
        progressed = true;
        const bool handled{handleDatagram()};
        datagram_occupied_ = false;
        if (!handled) {
            state_ = TransferState::kError;
            return ProcessResult::kError;
        }
    }

    if (state_ == TransferState::kStarting) {
        if (!setup_sent_) {
            if (!sendSetup(now_ms, false)) {
                state_ = TransferState::kError;
                return ProcessResult::kError;
            }
            return ProcessResult::kProgress;
        }
        if (elapsed(now_ms, setup_last_send_ms_, timing_.retransmission_timeout_ms)) {
            if (retryLimitReached(setup_retry_count_)) {
                (void)sendAbort();
                transitionToAborted();
                return ProcessResult::kError;
            }
            if (!sendSetup(now_ms, true)) {
                state_ = TransferState::kError;
                return ProcessResult::kError;
            }
            return ProcessResult::kProgress;
        }
        return progressed ? ProcessResult::kProgress : ProcessResult::kIdle;
    }

    if (state_ != TransferState::kActive) {
        return progressed ? ProcessResult::kProgress : ProcessResult::kIdle;
    }

    const uint8_t window_span{static_cast<uint8_t>(granted_end_ - acknowledged_count_)};
    const uint16_t window_mask{lowBitMask(window_span)};
    for (uint8_t offset{0U}; offset < window_span; ++offset) {
        const uint16_t bit{static_cast<uint16_t>(1U << offset)};
        if ((sent_bitmap_ & bit) == 0U) {
            if (!sendSegment(offset, now_ms, false)) {
                state_ = TransferState::kError;
                return ProcessResult::kError;
            }
            return ProcessResult::kProgress;
        }
    }

    const uint16_t outstanding{static_cast<uint16_t>(
        sent_bitmap_ & static_cast<uint16_t>(~acknowledged_bitmap_) & window_mask)};
    for (uint8_t offset{0U}; offset < window_span; ++offset) {
        const uint16_t bit{static_cast<uint16_t>(1U << offset)};
        if ((outstanding & bit) == 0U ||
            !elapsed(now_ms, segment_last_send_ms_[offset],
                     timing_.retransmission_timeout_ms)) {
            continue;
        }
        if (retryLimitReached(segment_retry_count_[offset])) {
            (void)sendAbort();
            transitionToAborted();
            return ProcessResult::kError;
        }
        if (!sendSegment(offset, now_ms, true)) {
            state_ = TransferState::kError;
            return ProcessResult::kError;
        }
        return ProcessResult::kProgress;
    }

    if (acknowledged_count_ < segment_count_ && outstanding == 0U) {
        if (!probe_timer_active_) {
            probe_timer_active_ = true;
            probe_last_send_ms_ = now_ms;
        } else if (elapsed(now_ms, probe_last_send_ms_, timing_.probe_timeout_ms)) {
            if (retryLimitReached(probe_retry_count_)) {
                (void)sendAbort();
                transitionToAborted();
                return ProcessResult::kError;
            }
            if (!sendProbe(now_ms)) {
                state_ = TransferState::kError;
                return ProcessResult::kError;
            }
            return ProcessResult::kProgress;
        }
    } else {
        probe_timer_active_ = false;
        probe_retry_count_ = 0U;
    }
    return progressed ? ProcessResult::kProgress : ProcessResult::kIdle;
}

SendResult BitsTransmitter::sendDatagram(ByteSpan payload) noexcept {
    if (transmit_packet_.capacity() < kUserDatagramHeaderSize ||
        payload.size() > static_cast<size_t>(transmit_packet_.capacity() -
                                             kUserDatagramHeaderSize)) {
        return SendResult::kTooLarge;
    }
    const uint16_t message_size{static_cast<uint16_t>(payload.size() +
                                                       kUserDatagramHeaderSize)};
    if (!preparePacket(message_size, QoS::kNormal) ||
        !encodeUserDatagram(payload, transmit_packet_.payload())) {
        return SendResult::kTooLarge;
    }
    return forwardPacket() ? SendResult::kSent : SendResult::kNoRoute;
}

SendResult BitsTransmitter::abort() noexcept {
    if (state_ != TransferState::kStarting && state_ != TransferState::kActive) {
        return SendResult::kInvalidState;
    }
    const bool sent{sendAbort()};
    transitionToAborted();
    return sent ? SendResult::kSent : SendResult::kNoRoute;
}

StartResult BitsTransmitter::startTransfer(ByteSpan object, uint16_t segment_size,
                                           uint8_t session_id,
                                           uint8_t initial_sequence_number) noexcept {
    if (state_ == TransferState::kStarting || state_ == TransferState::kActive) {
        return StartResult::kBusy;
    }
    if (object.empty() || segment_size == 0U || object.size() > UINT32_MAX) {
        return StartResult::kInvalidArgument;
    }
    const uint32_t object_size{static_cast<uint32_t>(object.size())};
    const uint32_t count{segmentCount(object_size, segment_size)};
    if (count == 0U || count > kMaximumCompactSegmentCount) {
        return StartResult::kInvalidArgument;
    }
    if (transmit_packet_.capacity() < kSetupSize ||
        transmit_packet_.capacity() <
            (static_cast<uint32_t>(segment_size) + kSegmentHeaderSize)) {
        return StartResult::kPacketTooSmall;
    }

    object_ = object;
    setup_ = Setup{session_id, initial_sequence_number, segment_size, object_size};
    segment_count_ = count;
    resetTransferTracking();
    state_ = TransferState::kStarting;
    return StartResult::kStarted;
}

bool BitsTransmitter::handleDatagram() noexcept {
    const ByteSpan message{datagram_ingress_.payload().data(), datagram_ingress_.size()};
    if (message.empty()) {
        return false;
    }

    Control control{};
    if (!decodeControl(message[0], control)) {
        return false;
    }
    switch (control.type) {
        case MessageType::kAck:
            return handleAck(message);
        case MessageType::kReject:
            return handleReject(message);
        case MessageType::kAbort:
            return handleAbort(message);
        case MessageType::kUserDatagram: {
            ByteSpan payload{};
            if (!decodeUserDatagram(message, payload)) {
                return false;
            }
            callbacks_.onDatagram(payload);
            return true;
        }
        case MessageType::kSetup:
        case MessageType::kSegment:
        case MessageType::kProbe:
            return false;
    }
    return false;
}

bool BitsTransmitter::handleAck(ByteSpan payload) noexcept {
    Ack ack{};
    if (!decodeAck(payload, ack)) {
        return false;
    }
    if ((state_ != TransferState::kStarting && state_ != TransferState::kActive) ||
        ack.session_id != setup_.session_id) {
        return true;
    }

    const uint8_t current_base{baseSequence(setup_, acknowledged_count_)};
    const uint8_t advance{static_cast<uint8_t>(ack.window_base - current_base)};
    const uint8_t grant_span{static_cast<uint8_t>(ack.max_receive_sequence -
                                                  ack.window_base)};
    if (advance > kCompactWindowWidth ||
        (acknowledged_count_ + advance) > segment_count_ ||
        grant_span > kCompactWindowWidth ||
        (acknowledged_count_ + advance + grant_span) > segment_count_ ||
        (ack.window_bitmap & static_cast<uint16_t>(~lowBitMask(grant_span))) != 0U) {
        return true;
    }

    shiftWindow(advance);
    acknowledged_count_ += advance;
    acknowledged_bitmap_ = static_cast<uint16_t>(
        acknowledged_bitmap_ |
        (ack.window_bitmap & lowBitMask(grant_span) & sent_bitmap_));
    const uint32_t advertised_end{acknowledged_count_ + grant_span};
    if (advertised_end > granted_end_) {
        granted_end_ = advertised_end;
    }
    setup_sent_ = true;
    probe_timer_active_ = false;
    probe_retry_count_ = 0U;

    if (acknowledged_count_ == segment_count_) {
        state_ = TransferState::kCompleted;
        callbacks_.onTransferComplete();
    } else {
        state_ = TransferState::kActive;
    }
    return true;
}

bool BitsTransmitter::handleReject(ByteSpan payload) noexcept {
    Reject reject{};
    if (!decodeReject(payload, reject)) {
        return false;
    }
    if ((state_ != TransferState::kStarting && state_ != TransferState::kActive) ||
        reject.session_id != setup_.session_id) {
        return true;
    }
    state_ = TransferState::kRejected;
    callbacks_.onTransferRejected(reject.reason);
    return true;
}

bool BitsTransmitter::handleAbort(ByteSpan payload) noexcept {
    Abort abort_message{};
    if (!decodeAbort(payload, abort_message)) {
        return false;
    }
    if ((state_ != TransferState::kStarting && state_ != TransferState::kActive) ||
        abort_message.session_id != setup_.session_id) {
        return true;
    }
    transitionToAborted();
    return true;
}

bool BitsTransmitter::sendSetup(uint32_t now_ms, bool retransmission) noexcept {
    if (!preparePacket(kSetupSize, QoS::kNormal) ||
        !encodeSetup(setup_, transmit_packet_.payload()) || !forwardPacket()) {
        return false;
    }
    setup_sent_ = true;
    setup_last_send_ms_ = now_ms;
    if (retransmission) {
        ++setup_retry_count_;
    }
    return true;
}

bool BitsTransmitter::sendSegment(uint8_t window_offset, uint32_t now_ms,
                                  bool retransmission) noexcept {
    const uint32_t segment_index{acknowledged_count_ + window_offset};
    if (segment_index >= segment_count_) {
        return false;
    }
    const uint32_t object_offset{segment_index * setup_.segment_size};
    const uint32_t remaining{setup_.total_size - object_offset};
    const uint16_t payload_size{static_cast<uint16_t>(
        remaining < setup_.segment_size ? remaining : setup_.segment_size)};
    const uint16_t message_size{static_cast<uint16_t>(kSegmentHeaderSize + payload_size)};
    if (!preparePacket(message_size, QoS::kBackground)) {
        return false;
    }

    const SegmentHeader header{setup_.session_id,
                               static_cast<uint16_t>(segment_index)};
    if (!encodeSegmentHeader(header, transmit_packet_.payload())) {
        return false;
    }
    std::memcpy(transmit_packet_.payload().data() + kSegmentHeaderSize,
                object_.data() + object_offset, payload_size);
    if (!forwardPacket()) {
        return false;
    }

    const uint16_t bit{static_cast<uint16_t>(1U << window_offset)};
    sent_bitmap_ = static_cast<uint16_t>(sent_bitmap_ | bit);
    segment_last_send_ms_[window_offset] = now_ms;
    if (retransmission) {
        ++segment_retry_count_[window_offset];
    } else {
        segment_retry_count_[window_offset] = 0U;
    }
    probe_timer_active_ = false;
    return true;
}

bool BitsTransmitter::sendProbe(uint32_t now_ms) noexcept {
    const Probe probe{setup_.session_id};
    if (!preparePacket(kProbeSize, QoS::kNormal) ||
        !encodeProbe(probe, transmit_packet_.payload()) || !forwardPacket()) {
        return false;
    }
    probe_last_send_ms_ = now_ms;
    ++probe_retry_count_;
    return true;
}

bool BitsTransmitter::sendAbort() noexcept {
    const Abort abort_message{setup_.session_id};
    return preparePacket(kAbortSize, QoS::kNormal) &&
           encodeAbort(abort_message, transmit_packet_.payload()) && forwardPacket();
}

bool BitsTransmitter::preparePacket(uint16_t payload_size, QoS qos) noexcept {
    if (!transmit_packet_.initialize(payload_size,
                                     ControlFields{qos, false, TransportType::kBits})) {
        return false;
    }
    Header& header{transmit_packet_.header()};
    header.wire = connection_.wire;
    header.source = connection_.local_host;
    header.destination = connection_.remote_host;
    header.endpoint = connection_.endpoint;
    return true;
}

bool BitsTransmitter::forwardPacket() noexcept {
    return router_.forward(transmit_packet_) == RouteResult::kForwarded;
}

bool BitsTransmitter::retryLimitReached(uint8_t retry_count) const noexcept {
    return retry_count >= timing_.max_retries;
}

void BitsTransmitter::shiftWindow(uint8_t count) noexcept {
    if (count == 0U) {
        return;
    }
    if (count >= kCompactWindowWidth) {
        sent_bitmap_ = 0U;
        acknowledged_bitmap_ = 0U;
        for (uint8_t index{0U}; index < kCompactWindowWidth; ++index) {
            segment_last_send_ms_[index] = 0U;
            segment_retry_count_[index] = 0U;
        }
        return;
    }

    sent_bitmap_ = static_cast<uint16_t>(sent_bitmap_ >> count);
    acknowledged_bitmap_ = static_cast<uint16_t>(acknowledged_bitmap_ >> count);
    for (uint8_t index{0U}; index < (kCompactWindowWidth - count); ++index) {
        segment_last_send_ms_[index] = segment_last_send_ms_[index + count];
        segment_retry_count_[index] = segment_retry_count_[index + count];
    }
    for (uint8_t index{static_cast<uint8_t>(kCompactWindowWidth - count)};
         index < kCompactWindowWidth; ++index) {
        segment_last_send_ms_[index] = 0U;
        segment_retry_count_[index] = 0U;
    }
}

void BitsTransmitter::resetTransferTracking() noexcept {
    acknowledged_count_ = 0U;
    granted_end_ = 0U;
    sent_bitmap_ = 0U;
    acknowledged_bitmap_ = 0U;
    setup_last_send_ms_ = 0U;
    probe_last_send_ms_ = 0U;
    setup_retry_count_ = 0U;
    probe_retry_count_ = 0U;
    setup_sent_ = false;
    probe_timer_active_ = false;
    for (uint8_t index{0U}; index < kCompactWindowWidth; ++index) {
        segment_last_send_ms_[index] = 0U;
        segment_retry_count_[index] = 0U;
    }
}

void BitsTransmitter::transitionToAborted() noexcept {
    if (state_ == TransferState::kAborted) {
        return;
    }
    state_ = TransferState::kAborted;
    callbacks_.onTransferAborted();
}

}  // namespace wirespaces::transport::bits

/**
 * @file bits.cpp
 * @brief Compact BITS receiver and transmitter implementation.
 */

#include <wirespaces/transports/bits/bits.h>

#include <cstring>

namespace wirespaces::transport::bits {
namespace {
SendResult sendResult(RouteResult result) noexcept {
    switch (result) {
        case RouteResult::kAccepted: return SendResult::kSent;
        case RouteResult::kPartial: return SendResult::kPartial;
        case RouteResult::kFull: return SendResult::kFull;
        case RouteResult::kTooLarge: return SendResult::kTooLarge;
        case RouteResult::kRejected: return SendResult::kRejected;
        default: return SendResult::kNoRoute;
    }
}

bool matchesConnection(const PacketBuffer& packet, const ConnectionConfig& connection) noexcept {
    const Header& header{packet.header()};
    return header.hasSupportedControl() && header.transportType() == TransportType::kBits &&
           header.wire == connection.wire &&
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

namespace {

ReceiverEngineConfig receiverEngineConfig(
    foundation::Span<PacketBuffer*> segment_ingress, uint32_t inactivity_timeout_ms) noexcept {
    const size_t slot_count{segment_ingress.size() < kCompactWindowWidth
                                ? segment_ingress.size()
                                : kCompactWindowWidth};
    uint8_t window_width{0U};
    uint16_t maximum_segment_size{UINT16_MAX};
    for (size_t index{0U}; index < slot_count; ++index) {
        const PacketBuffer* const slot{segment_ingress[index]};
        if (slot == nullptr) {
            continue;
        }
        ++window_width;
        const uint16_t slot_segment_size{
            slot->capacity() >= kSegmentHeaderSize
                ? static_cast<uint16_t>(slot->capacity() - kSegmentHeaderSize)
                : static_cast<uint16_t>(0U)};
        if (slot_segment_size < maximum_segment_size) {
            maximum_segment_size = slot_segment_size;
        }
    }
    if (window_width == 0U) {
        maximum_segment_size = 0U;
    }
    return ReceiverEngineConfig{maximum_segment_size, window_width, UINT32_MAX, inactivity_timeout_ms};
}

}  // namespace

BitsReceiver::BitsReceiver(ConnectionConfig connection, Router& router,
                           ReceiverCallbacks& callbacks, ReceiverStorage storage,
                           uint32_t inactivity_timeout_ms) noexcept
    : connection_{connection},
      router_{router},
      storage_{storage},
      engine_{receiverEngineConfig(storage.segment_ingress, inactivity_timeout_ms), callbacks, *this} {}

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
        if (!copyPacket(packet, storage_.datagram_ingress)) {
            return ReceiveResult::kRejected;
        }
        datagram_occupied_ = true;
        return ReceiveResult::kAccepted;
    }

    const size_t slot_count{storage_.segment_ingress.size() < kCompactWindowWidth
                                ? storage_.segment_ingress.size()
                                : kCompactWindowWidth};
    bool free_slot_seen{false};
    for (size_t index{0U}; index < slot_count; ++index) {
        const uint16_t bit{static_cast<uint16_t>(1U << index)};
        PacketBuffer* const slot{storage_.segment_ingress[index]};
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

ProcessResult BitsReceiver::process(uint32_t now_ms) noexcept {
    if (engine_.poll(now_ms) == ProcessResult::kError) {
        // Expired-session backlog must not keep a dead transfer alive or restart it.
        segment_occupied_mask_ = 0U;
        datagram_occupied_ = false;
        return ProcessResult::kError;
    }
    ProcessResult result{ProcessResult::kIdle};

    if (segment_occupied_mask_ != 0U) {
        for (size_t index{0U}; index < kCompactWindowWidth; ++index) {
            const uint16_t bit{static_cast<uint16_t>(1U << index)};
            if ((segment_occupied_mask_ & bit) == 0U) {
                continue;
            }
            segment_occupied_mask_ =
                static_cast<uint16_t>(segment_occupied_mask_ &
                                      static_cast<uint16_t>(~bit));
            const PacketBuffer* const packet{storage_.segment_ingress[index]};
            result = engine_.process(
                packet == nullptr ? ByteSpan{} : packet->payload(), now_ms);
            break;
        }
    }

    if (datagram_occupied_) {
        const ProcessResult datagram_result{
            engine_.process(ByteSpan{storage_.datagram_ingress.payload().data(),
                                     storage_.datagram_ingress.size()}, now_ms)};
        datagram_occupied_ = false;
        if (datagram_result == ProcessResult::kError ||
            result == ProcessResult::kError) {
            return ProcessResult::kError;
        }
        result = (result == ProcessResult::kBlocked || datagram_result == ProcessResult::kBlocked)
                     ? ProcessResult::kBlocked : ProcessResult::kProgress;
    }

    return result;
}

SendResult BitsReceiver::sendDatagram(ByteSpan payload) noexcept {
    return engine_.sendDatagram(payload);
}

SendResult BitsReceiver::abort() noexcept {
    return engine_.abort();
}

MutableByteSpan BitsReceiver::prepare(uint16_t payload_size) noexcept {
    if (!storage_.transmit_packet.initialize(payload_size, connection_, ControlFields::bits())) {
        return MutableByteSpan{};
    }
    return storage_.transmit_packet.payload();
}

SendResult BitsReceiver::sendPrepared() noexcept {
    return sendResult(router_.forward(storage_.transmit_packet));
}

BitsTransmitter::BitsTransmitter(ConnectionConfig connection, TimingConfig timing, Router& router,
                                 TransmitterCallbacks& callbacks,
                                 TransmitterStorage storage) noexcept
    : connection_{connection},
      timing_{timing},
      router_{router},
      callbacks_{callbacks},
      storage_{storage} {}

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
    if (!copyPacket(packet, storage_.datagram_ingress)) {
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
            session_.state = TransferState::kError;
            return ProcessResult::kError;
        }
    }

    if (session_.state == TransferState::kStarting) {
        if (!session_.setup_sent) {
            if (!sendSetup(now_ms, false)) {
                if (last_send_ == SendResult::kFull) return ProcessResult::kBlocked;
                session_.state = TransferState::kError;
                return ProcessResult::kError;
            }
            return ProcessResult::kProgress;
        }
        if (elapsed(now_ms, session_.setup_last_send_ms, timing_.retransmission_timeout_ms)) {
            if (retryLimitReached(session_.setup_retry_count)) {
                sendAbort();
                transitionToAborted();
                return ProcessResult::kError;
            }
            if (!sendSetup(now_ms, true)) {
                if (last_send_ == SendResult::kFull) return ProcessResult::kBlocked;
                session_.state = TransferState::kError;
                return ProcessResult::kError;
            }
            return ProcessResult::kProgress;
        }
        return progressed ? ProcessResult::kProgress : ProcessResult::kIdle;
    }

    if (session_.state != TransferState::kActive) {
        return progressed ? ProcessResult::kProgress : ProcessResult::kIdle;
    }

    const uint8_t window_span{
        static_cast<uint8_t>(session_.granted_end - session_.acknowledged_count)};
    const uint16_t window_mask{lowBitMask(window_span)};
    for (uint8_t offset{0U}; offset < window_span; ++offset) {
        const uint16_t bit{static_cast<uint16_t>(1U << offset)};
        if ((session_.sent_bitmap & bit) == 0U) {
            if (!sendSegment(offset, now_ms, false)) {
                if (last_send_ == SendResult::kFull) return ProcessResult::kBlocked;
                session_.state = TransferState::kError;
                return ProcessResult::kError;
            }
            return ProcessResult::kProgress;
        }
    }

    const uint16_t outstanding{static_cast<uint16_t>(
        session_.sent_bitmap & static_cast<uint16_t>(~session_.acknowledged_bitmap) & window_mask)};
    for (uint8_t offset{0U}; offset < window_span; ++offset) {
        const uint16_t bit{static_cast<uint16_t>(1U << offset)};
        if ((outstanding & bit) == 0U ||
            !elapsed(now_ms, session_.segment_last_send_ms[offset],
                     timing_.retransmission_timeout_ms)) {
            continue;
        }
        if (retryLimitReached(session_.segment_retry_count[offset])) {
            sendAbort();
            transitionToAborted();
            return ProcessResult::kError;
        }
        if (!sendSegment(offset, now_ms, true)) {
            if (last_send_ == SendResult::kFull) return ProcessResult::kBlocked;
            session_.state = TransferState::kError;
            return ProcessResult::kError;
        }
        return ProcessResult::kProgress;
    }

    if (session_.acknowledged_count < session_.segment_count && outstanding == 0U) {
        if (!session_.probe_timer_active) {
            session_.probe_timer_active = true;
            session_.probe_last_send_ms = now_ms;
        } else if (elapsed(now_ms, session_.probe_last_send_ms, timing_.probe_timeout_ms)) {
            if (retryLimitReached(session_.probe_retry_count)) {
                sendAbort();
                transitionToAborted();
                return ProcessResult::kError;
            }
            if (!sendProbe(now_ms)) {
                if (last_send_ == SendResult::kFull) return ProcessResult::kBlocked;
                session_.state = TransferState::kError;
                return ProcessResult::kError;
            }
            return ProcessResult::kProgress;
        }
    } else {
        session_.probe_timer_active = false;
        session_.probe_retry_count = 0U;
    }
    return progressed ? ProcessResult::kProgress : ProcessResult::kIdle;
}

SendResult BitsTransmitter::sendDatagram(ByteSpan payload) noexcept {
    if (storage_.transmit_packet.capacity() < kUserDatagramHeaderSize ||
        payload.size() > static_cast<size_t>(storage_.transmit_packet.capacity() -
                                             kUserDatagramHeaderSize)) {
        return SendResult::kTooLarge;
    }
    const uint16_t message_size{static_cast<uint16_t>(payload.size() +
                                                       kUserDatagramHeaderSize)};
    if (!preparePacket(message_size, QoS::kNormal) ||
        !encodeUserDatagram(payload, storage_.transmit_packet.payload())) {
        return SendResult::kTooLarge;
    }
    const bool sent{forwardPacket()};
    (void)sent;
    return last_send_;
}

SendResult BitsTransmitter::abort() noexcept {
    if (session_.state != TransferState::kStarting && session_.state != TransferState::kActive) {
        return SendResult::kInvalidState;
    }
    const bool sent{sendAbort()};
    transitionToAborted();
    (void)sent;
    return last_send_;
}

StartResult BitsTransmitter::startTransfer(ByteSpan object, uint16_t segment_size,
                                           uint8_t session_id,
                                           uint8_t initial_sequence_number) noexcept {
    if (session_.state == TransferState::kStarting || session_.state == TransferState::kActive) {
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
    if (storage_.transmit_packet.capacity() < kSetupSize ||
        storage_.transmit_packet.capacity() <
            (static_cast<uint32_t>(segment_size) + kSegmentHeaderSize)) {
        return StartResult::kPacketTooSmall;
    }

    session_.object = object;
    const uint16_t final_segment_index{static_cast<uint16_t>(count - 1U)};
    const uint16_t final_segment_size{static_cast<uint16_t>(
        object_size - static_cast<uint32_t>(final_segment_index) * segment_size)};
    session_.setup = Setup{session_id, initial_sequence_number, final_segment_index,
                           segment_size, final_segment_size};
    session_.segment_count = count;
    resetTransferTracking();
    session_.state = TransferState::kStarting;
    return StartResult::kStarted;
}

bool BitsTransmitter::handleDatagram() noexcept {
    const ByteSpan message{storage_.datagram_ingress.payload().data(),
                           storage_.datagram_ingress.size()};
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
            if (!detail::decodeUserDatagramKnownType(message, payload)) {
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
    if (!detail::decodeAckKnownType(payload, ack)) {
        return false;
    }
    if ((session_.state != TransferState::kStarting &&
         session_.state != TransferState::kActive) ||
        ack.session_id != session_.setup.session_id || !session_.setup_sent) {
        return true;
    }

    const uint8_t current_base{baseSequence(session_.setup, session_.acknowledged_count)};
    const uint8_t advance{static_cast<uint8_t>(ack.window_base - current_base)};
    const uint8_t grant_span{static_cast<uint8_t>(ack.max_receive_sequence -
                                                  ack.window_base)};
    if (advance > kCompactWindowWidth ||
        (session_.acknowledged_count + advance) > session_.segment_count ||
        grant_span > kCompactWindowWidth ||
        (session_.acknowledged_count + advance + grant_span) > session_.segment_count ||
        (ack.window_bitmap & static_cast<uint16_t>(~lowBitMask(grant_span))) != 0U) {
        return true;
    }

    // A cumulative ACK must cover only segments admitted by the local Link.
    // In particular, stale ACKs must not complete a newly reused session during SETUP.
    const uint16_t advanced_mask{lowBitMask(advance)};
    if ((session_.sent_bitmap & advanced_mask) != advanced_mask) {
        return true;
    }

    shiftWindow(advance);
    session_.acknowledged_count += advance;
    session_.acknowledged_bitmap = static_cast<uint16_t>(
        session_.acknowledged_bitmap |
        (ack.window_bitmap & lowBitMask(grant_span) & session_.sent_bitmap));
    const uint32_t advertised_end{session_.acknowledged_count + grant_span};
    if (advertised_end > session_.granted_end) {
        session_.granted_end = advertised_end;
    }
    session_.setup_sent = true;
    session_.probe_timer_active = false;
    session_.probe_retry_count = 0U;

    if (session_.acknowledged_count == session_.segment_count) {
        session_.state = TransferState::kCompleted;
        callbacks_.onTransferComplete();
    } else {
        session_.state = TransferState::kActive;
    }
    return true;
}

bool BitsTransmitter::handleReject(ByteSpan payload) noexcept {
    Reject reject{};
    if (!detail::decodeRejectKnownType(payload, reject)) {
        return false;
    }
    if ((session_.state != TransferState::kStarting &&
         session_.state != TransferState::kActive) ||
        reject.session_id != session_.setup.session_id) {
        return true;
    }
    session_.state = TransferState::kRejected;
    callbacks_.onTransferRejected(reject.reason);
    return true;
}

bool BitsTransmitter::handleAbort(ByteSpan payload) noexcept {
    Abort abort_message{};
    if (!detail::decodeAbortKnownType(payload, abort_message)) {
        return false;
    }
    if ((session_.state != TransferState::kStarting &&
         session_.state != TransferState::kActive) ||
        abort_message.session_id != session_.setup.session_id) {
        return true;
    }
    transitionToAborted();
    return true;
}

bool BitsTransmitter::sendSetup(uint32_t now_ms, bool retransmission) noexcept {
    if (!preparePacket(kSetupSize, QoS::kNormal) ||
        !encodeSetup(session_.setup, storage_.transmit_packet.payload()) || !forwardPacket()) {
        return false;
    }
    session_.setup_sent = true;
    session_.setup_last_send_ms = now_ms;
    if (retransmission) {
        ++session_.setup_retry_count;
    }
    return true;
}

bool BitsTransmitter::sendSegment(uint8_t window_offset, uint32_t now_ms,
                                  bool retransmission) noexcept {
    const uint32_t segment_index{session_.acknowledged_count + window_offset};
    if (segment_index >= session_.segment_count) {
        return false;
    }
    const uint32_t object_offset{segment_index * session_.setup.segment_size};
    const uint16_t payload_size{
        segment_index == session_.setup.final_segment_index
            ? session_.setup.final_segment_size
            : session_.setup.segment_size};
    const uint16_t message_size{static_cast<uint16_t>(kSegmentHeaderSize + payload_size)};
    if (!preparePacket(message_size, QoS::kBackground)) {
        return false;
    }

    const SegmentHeader header{session_.setup.session_id,
                               static_cast<uint16_t>(segment_index)};
    if (!encodeSegmentHeader(header, storage_.transmit_packet.payload())) {
        return false;
    }
    std::memcpy(storage_.transmit_packet.payload().data() + kSegmentHeaderSize,
                session_.object.data() + object_offset, payload_size);
    if (!forwardPacket()) {
        return false;
    }

    const uint16_t bit{static_cast<uint16_t>(1U << window_offset)};
    session_.sent_bitmap = static_cast<uint16_t>(session_.sent_bitmap | bit);
    session_.segment_last_send_ms[window_offset] = now_ms;
    if (retransmission) {
        ++session_.segment_retry_count[window_offset];
    } else {
        session_.segment_retry_count[window_offset] = 0U;
    }
    session_.probe_timer_active = false;
    return true;
}

bool BitsTransmitter::sendProbe(uint32_t now_ms) noexcept {
    const Probe probe{session_.setup.session_id};
    if (!preparePacket(kProbeSize, QoS::kNormal) ||
        !encodeProbe(probe, storage_.transmit_packet.payload()) || !forwardPacket()) {
        return false;
    }
    session_.probe_last_send_ms = now_ms;
    ++session_.probe_retry_count;
    return true;
}

bool BitsTransmitter::sendAbort() noexcept {
    const Abort abort_message{session_.setup.session_id};
    return preparePacket(kAbortSize, QoS::kNormal) &&
           encodeAbort(abort_message, storage_.transmit_packet.payload()) && forwardPacket();
}

bool BitsTransmitter::preparePacket(uint16_t payload_size, QoS qos) noexcept {
    last_send_ = SendResult::kTooLarge;
    return storage_.transmit_packet.initialize(payload_size, connection_, ControlFields::bits(qos));
}

bool BitsTransmitter::forwardPacket() noexcept {
    last_send_ = sendResult(router_.forward(storage_.transmit_packet));
    return last_send_ == SendResult::kSent;
}

bool BitsTransmitter::retryLimitReached(uint8_t retry_count) const noexcept {
    return retry_count >= timing_.max_retries;
}

void BitsTransmitter::shiftWindow(uint8_t count) noexcept {
    if (count == 0U) {
        return;
    }
    if (count >= kCompactWindowWidth) {
        session_.sent_bitmap = 0U;
        session_.acknowledged_bitmap = 0U;
        for (uint8_t index{0U}; index < kCompactWindowWidth; ++index) {
            session_.segment_last_send_ms[index] = 0U;
            session_.segment_retry_count[index] = 0U;
        }
        return;
    }

    session_.sent_bitmap = static_cast<uint16_t>(session_.sent_bitmap >> count);
    session_.acknowledged_bitmap = static_cast<uint16_t>(session_.acknowledged_bitmap >> count);
    for (uint8_t index{0U}; index < (kCompactWindowWidth - count); ++index) {
        session_.segment_last_send_ms[index] = session_.segment_last_send_ms[index + count];
        session_.segment_retry_count[index] = session_.segment_retry_count[index + count];
    }
    for (uint8_t index{static_cast<uint8_t>(kCompactWindowWidth - count)};
         index < kCompactWindowWidth; ++index) {
        session_.segment_last_send_ms[index] = 0U;
        session_.segment_retry_count[index] = 0U;
    }
}

void BitsTransmitter::resetTransferTracking() noexcept {
    session_.acknowledged_count = 0U;
    session_.granted_end = 0U;
    session_.sent_bitmap = 0U;
    session_.acknowledged_bitmap = 0U;
    session_.setup_last_send_ms = 0U;
    session_.probe_last_send_ms = 0U;
    session_.setup_retry_count = 0U;
    session_.probe_retry_count = 0U;
    session_.setup_sent = false;
    session_.probe_timer_active = false;
    for (uint8_t index{0U}; index < kCompactWindowWidth; ++index) {
        session_.segment_last_send_ms[index] = 0U;
        session_.segment_retry_count[index] = 0U;
    }
}

void BitsTransmitter::transitionToAborted() noexcept {
    if (session_.state == TransferState::kAborted) {
        return;
    }
    session_.state = TransferState::kAborted;
    callbacks_.onTransferAborted();
}

}  // namespace wirespaces::transport::bits

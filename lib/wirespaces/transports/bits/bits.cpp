/**
 * @file bits.cpp
 * @brief Compact BITS receiver and transmitter happy-path implementation.
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

bool setupMatches(const Setup& lhs, const Setup& rhs) noexcept {
    return lhs.session_id == rhs.session_id &&
           lhs.initial_sequence_number == rhs.initial_sequence_number &&
           lhs.segment_size == rhs.segment_size && lhs.total_size == rhs.total_size;
}

}  // namespace

BitsReceiver::BitsReceiver(const ConnectionConfig& connection, Router& router,
                           ReceiverCallbacks& callbacks, PacketBuffer& segment_ingress,
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

    PacketBuffer* slot{&datagram_ingress_};
    bool* occupied{&datagram_occupied_};
    if (control.type == MessageType::kSegment) {
        slot = &segment_ingress_;
        occupied = &segment_occupied_;
    }

    if (*occupied) {
        return ReceiveResult::kFull;
    }
    if (!copyPacket(packet, *slot)) {
        return ReceiveResult::kRejected;
    }
    *occupied = true;
    return ReceiveResult::kAccepted;
}

ProcessResult BitsReceiver::process() noexcept {
    bool progressed{false};
    bool failed{false};

    if (segment_occupied_) {
        progressed = true;
        failed = !handleSegment();
        segment_occupied_ = false;
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
        payload.size() > static_cast<size_t>(transmit_packet_.capacity() - kUserDatagramHeaderSize)) {
        return SendResult::kTooLarge;
    }
    const uint16_t message_size{static_cast<uint16_t>(payload.size() + kUserDatagramHeaderSize)};
    if (!preparePacket(message_size, QoS::kNormal) ||
        !encodeUserDatagram(payload, transmit_packet_.payload())) {
        return SendResult::kTooLarge;
    }
    return forwardPacket() ? SendResult::kSent : SendResult::kNoRoute;
}

bool BitsReceiver::handleSegment() noexcept {
    const ByteSpan message{segment_ingress_.payload().data(), segment_ingress_.size()};
    SegmentHeader header{};
    if (state_ != TransferState::kActive || !decodeSegmentHeader(message, header) ||
        header.session_id != setup_.session_id || header.segment_index >= segment_count_) {
        return false;
    }

    if (header.segment_index < received_segment_count_) {
        return sendAck();
    }
    if (header.segment_index != received_segment_count_) {
        return false;
    }

    const uint32_t object_offset{static_cast<uint32_t>(header.segment_index) * setup_.segment_size};
    const uint32_t remaining{setup_.total_size - object_offset};
    const uint16_t expected_size{static_cast<uint16_t>(
        remaining < setup_.segment_size ? remaining : setup_.segment_size)};
    const ByteSpan segment_payload{message.subspan(kSegmentHeaderSize)};
    if (segment_payload.size() != expected_size ||
        !callbacks_.onSegment(object_offset, segment_payload)) {
        return false;
    }

    ++received_segment_count_;
    const bool completed{received_segment_count_ == segment_count_};
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
            return decodeProbe(message, probe) && state_ != TransferState::kIdle &&
                   probe.session_id == setup_.session_id && sendAck();
        }
        case MessageType::kUserDatagram: {
            ByteSpan payload{};
            if (!decodeUserDatagram(message, payload)) {
                return false;
            }
            callbacks_.onDatagram(payload);
            return true;
        }
        case MessageType::kSegment:
        case MessageType::kAck:
        case MessageType::kReject:
            return false;
    }
    return false;
}

bool BitsReceiver::handleSetup(ByteSpan payload) noexcept {
    Setup requested{};
    if (!decodeSetup(payload, requested) || requested.segment_size == 0U ||
        requested.total_size == 0U) {
        return false;
    }

    const uint32_t requested_segment_count{segmentCount(requested.total_size,
                                                        requested.segment_size)};
    if (requested_segment_count == 0U ||
        requested_segment_count > kMaximumCompactSegmentCount ||
        (static_cast<uint32_t>(requested.segment_size) + kSegmentHeaderSize) >
            segment_ingress_.capacity()) {
        return false;
    }

    if (state_ == TransferState::kActive) {
        return setupMatches(setup_, requested) && sendAck();
    }

    setup_ = requested;
    segment_count_ = requested_segment_count;
    received_segment_count_ = 0U;
    state_ = TransferState::kActive;
    return sendAck();
}

bool BitsReceiver::sendAck() noexcept {
    const uint8_t window_base{received_segment_count_ == 0U
                                  ? static_cast<uint8_t>(setup_.initial_sequence_number - 1U)
                                  : sequenceFor(setup_, received_segment_count_ - 1U)};
    const uint8_t max_receive_sequence{received_segment_count_ < segment_count_
                                           ? sequenceFor(setup_, received_segment_count_)
                                           : window_base};
    const Ack ack{setup_.session_id, 0U, max_receive_sequence, window_base};
    if (!preparePacket(kAckSize, QoS::kNormal) ||
        !encodeAck(ack, transmit_packet_.payload())) {
        return false;
    }
    return forwardPacket();
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

BitsTransmitter::BitsTransmitter(const ConnectionConfig& connection, Router& router,
                                 TransmitterCallbacks& callbacks,
                                 PacketBuffer& datagram_ingress,
                                 PacketBuffer& transmit_packet) noexcept
    : connection_{connection},
      router_{router},
      callbacks_{callbacks},
      datagram_ingress_{datagram_ingress},
      transmit_packet_{transmit_packet} {}

ReceiveResult BitsTransmitter::receive(const PacketBuffer& packet) noexcept {
    if (!matchesConnection(packet, connection_) || packet.payload().empty()) {
        return ReceiveResult::kRejected;
    }

    Control control{};
    if (!decodeControl(packet.payload()[0], control) || control.type == MessageType::kSegment) {
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

ProcessResult BitsTransmitter::process() noexcept {
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

    bool sent{true};
    if (state_ == TransferState::kStarting && !setup_sent_) {
        sent = sendSetup();
        progressed = true;
    } else if (state_ == TransferState::kActive && !waiting_for_ack_ &&
               next_segment_index_ < segment_count_ &&
               sequenceFor(setup_, next_segment_index_) == max_receive_sequence_) {
        sent = sendNextSegment();
        progressed = true;
    }
    if (!sent) {
        state_ = TransferState::kError;
        return ProcessResult::kError;
    }
    return progressed ? ProcessResult::kProgress : ProcessResult::kIdle;
}

SendResult BitsTransmitter::sendDatagram(ByteSpan payload) noexcept {
    if (transmit_packet_.capacity() < kUserDatagramHeaderSize ||
        payload.size() > static_cast<size_t>(transmit_packet_.capacity() - kUserDatagramHeaderSize)) {
        return SendResult::kTooLarge;
    }
    const uint16_t message_size{static_cast<uint16_t>(payload.size() + kUserDatagramHeaderSize)};
    if (!preparePacket(message_size, QoS::kNormal) ||
        !encodeUserDatagram(payload, transmit_packet_.payload())) {
        return SendResult::kTooLarge;
    }
    return forwardPacket() ? SendResult::kSent : SendResult::kNoRoute;
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
    next_segment_index_ = 0U;
    outstanding_segment_index_ = 0U;
    max_receive_sequence_ = static_cast<uint8_t>(initial_sequence_number - 1U);
    setup_sent_ = false;
    waiting_for_ack_ = false;
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
    if (control.type == MessageType::kAck) {
        return handleAck(message);
    }
    if (control.type == MessageType::kUserDatagram) {
        ByteSpan payload{};
        if (!decodeUserDatagram(message, payload)) {
            return false;
        }
        callbacks_.onDatagram(payload);
        return true;
    }
    return false;
}

bool BitsTransmitter::handleAck(ByteSpan payload) noexcept {
    Ack ack{};
    if (!decodeAck(payload, ack) || ack.session_id != setup_.session_id) {
        return false;
    }
    max_receive_sequence_ = ack.max_receive_sequence;

    if (state_ == TransferState::kStarting) {
        const uint8_t empty_base{static_cast<uint8_t>(setup_.initial_sequence_number - 1U)};
        if (ack.window_base != empty_base ||
            ack.max_receive_sequence != setup_.initial_sequence_number) {
            return false;
        }
        state_ = TransferState::kActive;
        return true;
    }

    if (state_ != TransferState::kActive || !waiting_for_ack_) {
        return true;
    }
    const uint8_t expected_sequence{sequenceFor(setup_, outstanding_segment_index_)};
    if (ack.window_base != expected_sequence) {
        return true;
    }

    waiting_for_ack_ = false;
    next_segment_index_ = static_cast<uint32_t>(outstanding_segment_index_) + 1U;
    if (next_segment_index_ == segment_count_) {
        state_ = TransferState::kCompleted;
        callbacks_.onTransferComplete();
    }
    return true;
}

bool BitsTransmitter::sendSetup() noexcept {
    if (!preparePacket(kSetupSize, QoS::kNormal) ||
        !encodeSetup(setup_, transmit_packet_.payload()) || !forwardPacket()) {
        return false;
    }
    setup_sent_ = true;
    return true;
}

bool BitsTransmitter::sendNextSegment() noexcept {
    if (next_segment_index_ >= segment_count_) {
        return false;
    }
    const uint8_t sequence{sequenceFor(setup_, next_segment_index_)};
    if (sequence != max_receive_sequence_) {
        return true;
    }

    const uint32_t object_offset{next_segment_index_ * setup_.segment_size};
    const uint32_t remaining{setup_.total_size - object_offset};
    const uint16_t payload_size{static_cast<uint16_t>(
        remaining < setup_.segment_size ? remaining : setup_.segment_size)};
    const uint16_t message_size{static_cast<uint16_t>(kSegmentHeaderSize + payload_size)};
    if (!preparePacket(message_size, QoS::kBackground)) {
        return false;
    }

    const SegmentHeader header{setup_.session_id,
                               static_cast<uint16_t>(next_segment_index_)};
    if (!encodeSegmentHeader(header, transmit_packet_.payload())) {
        return false;
    }
    std::memcpy(transmit_packet_.payload().data() + kSegmentHeaderSize,
                object_.data() + object_offset, payload_size);
    if (!forwardPacket()) {
        return false;
    }

    outstanding_segment_index_ = static_cast<uint16_t>(next_segment_index_);
    waiting_for_ack_ = true;
    return true;
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

}  // namespace wirespaces::transport::bits

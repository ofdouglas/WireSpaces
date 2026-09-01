/**
 * @file codec.cpp
 * @brief Explicit little-endian Compact BITS wire codec implementation.
 */

#include <wirespaces/transports/bits/codec.h>

#include <cstring>

namespace wirespaces::transport::bits {
namespace {

constexpr uint8_t kProfileMask{0x80U};
constexpr uint8_t kProfileShift{7U};
constexpr uint8_t kVersionMask{0x70U};
constexpr uint8_t kVersionShift{4U};
constexpr uint8_t kTypeMask{0x0FU};

void writeU16(uint16_t value, uint8_t* output) noexcept {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    std::memcpy(output, &value, sizeof(value));
#else
    output[0] = static_cast<uint8_t>(value);
    output[1] = static_cast<uint8_t>(value >> 8U);
#endif
}

uint16_t readU16(const uint8_t* input) noexcept {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint16_t value{0U};
    std::memcpy(&value, input, sizeof(value));
    return value;
#else
    return static_cast<uint16_t>(static_cast<uint16_t>(input[0]) |
                                 (static_cast<uint16_t>(input[1]) << 8U));
#endif
}

bool hasType(ByteSpan input, MessageType expected) noexcept {
    Control control{};
    return !input.empty() && decodeControl(input[0], control) && control.type == expected;
}

}  // namespace

uint8_t encodeControl(MessageType type) noexcept {
    return static_cast<uint8_t>((kCompactProfile << kProfileShift) |
                                (kProtocolVersion << kVersionShift) |
                                static_cast<uint8_t>(type));
}

bool decodeControl(uint8_t encoded, Control& control) noexcept {
    const uint8_t profile{static_cast<uint8_t>((encoded & kProfileMask) >> kProfileShift)};
    const uint8_t version{static_cast<uint8_t>((encoded & kVersionMask) >> kVersionShift)};
    const uint8_t type_value{static_cast<uint8_t>(encoded & kTypeMask)};
    if (profile != kCompactProfile || version != kProtocolVersion ||
        type_value > static_cast<uint8_t>(MessageType::kAbort)) {
        return false;
    }

    control = Control{version, profile, static_cast<MessageType>(type_value)};
    return true;
}

bool encodeSetup(const Setup& setup, MutableByteSpan output) noexcept {
    if (output.size() < kSetupSize) {
        return false;
    }
    output[0] = encodeControl(MessageType::kSetup);
    output[1] = setup.session_id;
    output[2] = setup.initial_sequence_number;
    output[3] = 0U;
    writeU16(setup.final_segment_index, output.data() + 4U);
    writeU16(setup.segment_size, output.data() + 6U);
    writeU16(setup.final_segment_size, output.data() + 8U);
    return true;
}

bool detail::decodeSetupKnownType(ByteSpan input, Setup& setup) noexcept {
    if (input.size() != kSetupSize || input[3] != 0U) {
        return false;
    }
    setup = Setup{input[1], input[2], readU16(input.data() + 4U),
                  readU16(input.data() + 6U), readU16(input.data() + 8U)};
    return true;
}

bool decodeSetup(ByteSpan input, Setup& setup) noexcept {
    return hasType(input, MessageType::kSetup) &&
           detail::decodeSetupKnownType(input, setup);
}

bool encodeSegmentHeader(const SegmentHeader& header, MutableByteSpan output) noexcept {
    if (output.size() < kSegmentHeaderSize) {
        return false;
    }
    output[0] = encodeControl(MessageType::kSegment);
    output[1] = header.session_id;
    writeU16(header.segment_index, output.data() + 2U);
    return true;
}

bool detail::decodeSegmentHeaderKnownType(ByteSpan input,
                                          SegmentHeader& header) noexcept {
    if (input.size() < kSegmentHeaderSize) {
        return false;
    }
    header = SegmentHeader{input[1], readU16(input.data() + 2U)};
    return true;
}

bool decodeSegmentHeader(ByteSpan input, SegmentHeader& header) noexcept {
    return hasType(input, MessageType::kSegment) &&
           detail::decodeSegmentHeaderKnownType(input, header);
}

bool encodeAck(const Ack& ack, MutableByteSpan output) noexcept {
    if (output.size() < kAckSize) {
        return false;
    }
    output[0] = encodeControl(MessageType::kAck);
    output[1] = ack.session_id;
    writeU16(ack.window_bitmap, output.data() + 2U);
    output[4] = ack.max_receive_sequence;
    output[5] = ack.window_base;
    return true;
}

bool detail::decodeAckKnownType(ByteSpan input, Ack& ack) noexcept {
    if (input.size() != kAckSize) {
        return false;
    }
    ack = Ack{input[1], readU16(input.data() + 2U), input[4], input[5]};
    return true;
}

bool decodeAck(ByteSpan input, Ack& ack) noexcept {
    return hasType(input, MessageType::kAck) &&
           detail::decodeAckKnownType(input, ack);
}

bool encodeProbe(const Probe& probe, MutableByteSpan output) noexcept {
    if (output.size() < kProbeSize) {
        return false;
    }
    output[0] = encodeControl(MessageType::kProbe);
    output[1] = probe.session_id;
    return true;
}

bool detail::decodeProbeKnownType(ByteSpan input, Probe& probe) noexcept {
    if (input.size() != kProbeSize) {
        return false;
    }
    probe.session_id = input[1];
    return true;
}

bool decodeProbe(ByteSpan input, Probe& probe) noexcept {
    return hasType(input, MessageType::kProbe) &&
           detail::decodeProbeKnownType(input, probe);
}

bool encodeReject(const Reject& reject, MutableByteSpan output) noexcept {
    if (output.size() < kRejectSize) {
        return false;
    }
    output[0] = encodeControl(MessageType::kReject);
    output[1] = reject.session_id;
    output[2] = static_cast<uint8_t>(reject.reason);
    return true;
}

bool detail::decodeRejectKnownType(ByteSpan input, Reject& reject) noexcept {
    if (input.size() != kRejectSize ||
        input[2] > static_cast<uint8_t>(RejectReason::kInternalError)) {
        return false;
    }
    reject = Reject{input[1], static_cast<RejectReason>(input[2])};
    return true;
}

bool decodeReject(ByteSpan input, Reject& reject) noexcept {
    return hasType(input, MessageType::kReject) &&
           detail::decodeRejectKnownType(input, reject);
}

bool encodeAbort(const Abort& abort, MutableByteSpan output) noexcept {
    if (output.size() < kAbortSize) {
        return false;
    }
    output[0] = encodeControl(MessageType::kAbort);
    output[1] = abort.session_id;
    return true;
}

bool detail::decodeAbortKnownType(ByteSpan input, Abort& abort) noexcept {
    if (input.size() != kAbortSize) {
        return false;
    }
    abort.session_id = input[1];
    return true;
}

bool decodeAbort(ByteSpan input, Abort& abort) noexcept {
    return hasType(input, MessageType::kAbort) &&
           detail::decodeAbortKnownType(input, abort);
}

bool encodeUserDatagram(ByteSpan payload, MutableByteSpan output) noexcept {
    if (output.size() < (payload.size() + kUserDatagramHeaderSize)) {
        return false;
    }
    output[0] = encodeControl(MessageType::kUserDatagram);
    if (!payload.empty()) {
        std::memcpy(output.data() + kUserDatagramHeaderSize, payload.data(), payload.size());
    }
    return true;
}

bool detail::decodeUserDatagramKnownType(ByteSpan input,
                                         ByteSpan& payload) noexcept {
    if (input.empty()) {
        return false;
    }
    payload = input.subspan(kUserDatagramHeaderSize);
    return true;
}

bool decodeUserDatagram(ByteSpan input, ByteSpan& payload) noexcept {
    return hasType(input, MessageType::kUserDatagram) &&
           detail::decodeUserDatagramKnownType(input, payload);
}

}  // namespace wirespaces::transport::bits

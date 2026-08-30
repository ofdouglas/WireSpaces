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
    output[0] = static_cast<uint8_t>(value);
    output[1] = static_cast<uint8_t>(value >> 8U);
}

uint16_t readU16(const uint8_t* input) noexcept {
    return static_cast<uint16_t>(static_cast<uint16_t>(input[0]) |
                                 (static_cast<uint16_t>(input[1]) << 8U));
}

void writeU32(uint32_t value, uint8_t* output) noexcept {
    output[0] = static_cast<uint8_t>(value);
    output[1] = static_cast<uint8_t>(value >> 8U);
    output[2] = static_cast<uint8_t>(value >> 16U);
    output[3] = static_cast<uint8_t>(value >> 24U);
}

uint32_t readU32(const uint8_t* input) noexcept {
    return static_cast<uint32_t>(input[0]) |
           (static_cast<uint32_t>(input[1]) << 8U) |
           (static_cast<uint32_t>(input[2]) << 16U) |
           (static_cast<uint32_t>(input[3]) << 24U);
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
        type_value > static_cast<uint8_t>(MessageType::kUserDatagram)) {
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
    writeU16(setup.segment_size, output.data() + 3U);
    writeU32(setup.total_size, output.data() + 5U);
    return true;
}

bool decodeSetup(ByteSpan input, Setup& setup) noexcept {
    if (input.size() != kSetupSize || !hasType(input, MessageType::kSetup)) {
        return false;
    }
    setup = Setup{input[1], input[2], readU16(input.data() + 3U), readU32(input.data() + 5U)};
    return true;
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

bool decodeSegmentHeader(ByteSpan input, SegmentHeader& header) noexcept {
    if (input.size() < kSegmentHeaderSize || !hasType(input, MessageType::kSegment)) {
        return false;
    }
    header = SegmentHeader{input[1], readU16(input.data() + 2U)};
    return true;
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

bool decodeAck(ByteSpan input, Ack& ack) noexcept {
    if (input.size() != kAckSize || !hasType(input, MessageType::kAck)) {
        return false;
    }
    ack = Ack{input[1], readU16(input.data() + 2U), input[4], input[5]};
    return true;
}

bool encodeProbe(const Probe& probe, MutableByteSpan output) noexcept {
    if (output.size() < kProbeSize) {
        return false;
    }
    output[0] = encodeControl(MessageType::kProbe);
    output[1] = probe.session_id;
    return true;
}

bool decodeProbe(ByteSpan input, Probe& probe) noexcept {
    if (input.size() != kProbeSize || !hasType(input, MessageType::kProbe)) {
        return false;
    }
    probe.session_id = input[1];
    return true;
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

bool decodeUserDatagram(ByteSpan input, ByteSpan& payload) noexcept {
    if (input.empty() || !hasType(input, MessageType::kUserDatagram)) {
        return false;
    }
    payload = input.subspan(kUserDatagramHeaderSize);
    return true;
}

}  // namespace wirespaces::transport::bits

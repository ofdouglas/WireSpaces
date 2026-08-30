/**
 * @file protocol.h
 * @brief Compact BITS protocol value types and fixed wire sizes.
 */

#pragma once

#include <cstdint>

namespace wirespaces::transport::bits {

constexpr uint8_t kProtocolVersion{0U};
constexpr uint8_t kCompactProfile{0U};
constexpr uint8_t kReservedProfile{1U};

constexpr uint16_t kControlSize{1U};
constexpr uint16_t kSetupSize{9U};
constexpr uint16_t kSegmentHeaderSize{4U};
constexpr uint16_t kAckSize{6U};
constexpr uint16_t kProbeSize{2U};
constexpr uint16_t kRejectSize{3U};
constexpr uint16_t kUserDatagramHeaderSize{1U};
constexpr uint32_t kMaximumCompactSegmentCount{65536UL};

/** @brief BITS message type carried in the low nibble of the control byte. */
enum class MessageType : uint8_t {
    kSetup = 0U,
    kSegment = 1U,
    kAck = 2U,
    kProbe = 3U,
    kReject = 4U,
    kUserDatagram = 5U,
};

/** @brief Decoded fields from the fixed BITS control byte. */
struct Control {
    uint8_t version{kProtocolVersion};
    uint8_t profile{kCompactProfile};
    MessageType type{MessageType::kSetup};
};

/** @brief Compact SETUP message fields. */
struct Setup {
    uint8_t session_id{0U};
    uint8_t initial_sequence_number{0U};
    uint16_t segment_size{0U};
    uint32_t total_size{0U};
};

/** @brief Compact SEGMENT header fields. */
struct SegmentHeader {
    uint8_t session_id{0U};
    uint16_t segment_index{0U};
};

/** @brief Compact cumulative/selective ACK fields. */
struct Ack {
    uint8_t session_id{0U};
    uint16_t window_bitmap{0U};
    uint8_t max_receive_sequence{0U};
    uint8_t window_base{0U};
};

/** @brief Session-specific PROBE fields. */
struct Probe {
    uint8_t session_id{0U};
};

/** @brief SETUP rejection reason used by the initial Compact implementation. */
enum class RejectReason : uint8_t {
    kUnsupportedVersion = 0U,
    kUnsupportedProfile,
    kUnsupportedSegmentSize,
    kObjectTooLarge,
    kBusy,
    kInvalidArgument,
    kInternalError,
};

}  // namespace wirespaces::transport::bits

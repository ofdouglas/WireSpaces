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
constexpr uint8_t kCompactWindowWidth{16U};

constexpr uint16_t kControlSize{1U};
constexpr uint16_t kSetupSize{10U};
constexpr uint16_t kSegmentHeaderSize{4U};
constexpr uint16_t kAckSize{6U};
constexpr uint16_t kProbeSize{2U};
constexpr uint16_t kRejectSize{3U};
constexpr uint16_t kAbortSize{2U};
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
    kAbort = 6U,
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
    uint16_t final_segment_index{0U};
    uint16_t segment_size{0U};
    uint16_t final_segment_size{0U};
};

/** @brief Geometry derived from Compact SETUP fields. */
constexpr uint32_t setupSegmentCount(const Setup& setup) noexcept {
    return static_cast<uint32_t>(setup.final_segment_index) + 1U;
}

constexpr uint32_t setupTotalSize(const Setup& setup) noexcept {
    return static_cast<uint32_t>(setup.final_segment_index) * setup.segment_size +
           setup.final_segment_size;
}

/** @brief Compact SEGMENT header fields. */
struct SegmentHeader {
    uint8_t session_id{0U};
    uint16_t segment_index{0U};
};

/** @brief Compact cumulative/selective ACK fields. */
struct Ack {
    uint8_t session_id{0U};
    uint16_t window_bitmap{0U};  // TODO: move this for better alignment
    uint8_t max_receive_sequence{0U};
    uint8_t window_base{0U};
};

/** @brief Session-specific PROBE fields. */
struct Probe {
    uint8_t session_id{0U};
};

/** @brief SETUP rejection reason used by Compact BITS. */
enum class RejectReason : uint8_t {
    kUnsupportedVersion = 0U,
    kUnsupportedProfile,
    kUnsupportedSegmentSize,
    kObjectTooLarge,
    kBusy,
    kInvalidArgument,
    kInternalError,
};

/** @brief Session-specific SETUP rejection fields. */
struct Reject {
    uint8_t session_id{0U};
    RejectReason reason{RejectReason::kInvalidArgument};
};

/** @brief Session-specific transfer abort fields. */
struct Abort {
    uint8_t session_id{0U};
};

}  // namespace wirespaces::transport::bits

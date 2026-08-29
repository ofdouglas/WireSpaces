/*
 * @file reliable_segmented.h
 * @brief WireSpaces Reliable Segmented Transport
 */

#pragma once

#include <wirespaces/transports/common/transport_type.h>

#include <cstdint>

namespace wirespaces::transport::segmented {

// Control byte layout:
//  - 1 MSB  : Profile {Compact, Extended}
//  - 3 bits : Protocol Version
//  - 4 LSB  : Header Type

enum class HeaderType : uint8_t {
    Segment = 0,
    General = 1,
    Startup = 2,
    Ack = 3,
};

enum class Command : uint8_t {
    RejectStartup = 0,
    Abort = 1,
    Close = 2,
    Finish = 3,
    Probe = 4,
};

namespace compact {  // The compact profile uses 8-bit sequences numbers

// Precedes a segment payload
struct SegmentHeader {
    uint8_t control_byte;
    uint8_t session_id;

    uint8_t sequence_number;
};

// Sent with no payload, used for various commands / flags:
// - cmd = RejectStartup, arg = reason
// - cmd = abort / close connection
// - cmd = Finish / end connection normally
struct GeneralHeader {
    uint8_t control_byte;
    uint8_t session_id;

    uint8_t command;
    uint8_t argument;
};

// Sent in a message with no payload, to set up a transfer
struct StartupHeader {
    uint8_t control_byte;
    uint8_t session_id;

    uint16_t segment_size;
    uint32_t total_bytes;
    uint8_t initial_sequence_number;
};

// Sent with no payload, to ack a segment or transfer setup
struct AckHeader {
    uint8_t control_byte;
    uint8_t session_id;

    uint8_t max_recv_number;
    uint8_t window_base;
    uint16_t window_bitmap;
};

}  // namespace compact
}  // namespace wirespaces::transport::segmented
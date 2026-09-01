/**
 * @file codec.h
 * @brief Explicit little-endian Compact BITS wire codec.
 */

#pragma once

#include <wirespaces/core/packet.h>
#include <wirespaces/transports/bits/protocol.h>

namespace wirespaces::transport::bits {

/** @brief Encode a supported Compact BITS control byte. */
[[nodiscard]] uint8_t encodeControl(MessageType type) noexcept;

/**
 * @brief Decode and validate a BITS control byte.
 * @return True only for the supported protocol version, Compact profile, and known type.
 */
[[nodiscard]] bool decodeControl(uint8_t encoded, Control& control) noexcept;

/**
 * Decode fields after the caller has validated the control byte and dispatched
 * the expected message type. Length and message-specific fields remain checked.
 */
namespace detail {
[[nodiscard]] bool decodeSetupKnownType(ByteSpan input, Setup& setup) noexcept;
[[nodiscard]] bool decodeSegmentHeaderKnownType(ByteSpan input,
                                                SegmentHeader& header) noexcept;
[[nodiscard]] bool decodeAckKnownType(ByteSpan input, Ack& ack) noexcept;
[[nodiscard]] bool decodeProbeKnownType(ByteSpan input, Probe& probe) noexcept;
[[nodiscard]] bool decodeRejectKnownType(ByteSpan input, Reject& reject) noexcept;
[[nodiscard]] bool decodeAbortKnownType(ByteSpan input, Abort& abort) noexcept;
[[nodiscard]] bool decodeUserDatagramKnownType(ByteSpan input,
                                              ByteSpan& payload) noexcept;
}  // namespace detail

[[nodiscard]] bool encodeSetup(const Setup& setup, MutableByteSpan output) noexcept;
[[nodiscard]] bool decodeSetup(ByteSpan input, Setup& setup) noexcept;

[[nodiscard]] bool encodeSegmentHeader(const SegmentHeader& header,
                                       MutableByteSpan output) noexcept;
[[nodiscard]] bool decodeSegmentHeader(ByteSpan input, SegmentHeader& header) noexcept;

[[nodiscard]] bool encodeAck(const Ack& ack, MutableByteSpan output) noexcept;
[[nodiscard]] bool decodeAck(ByteSpan input, Ack& ack) noexcept;

[[nodiscard]] bool encodeProbe(const Probe& probe, MutableByteSpan output) noexcept;
[[nodiscard]] bool decodeProbe(ByteSpan input, Probe& probe) noexcept;

/** @brief Encode or decode a session-specific SETUP rejection. */
[[nodiscard]] bool encodeReject(const Reject& reject, MutableByteSpan output) noexcept;
[[nodiscard]] bool decodeReject(ByteSpan input, Reject& reject) noexcept;

/** @brief Encode or decode a session-specific transfer abort. */
[[nodiscard]] bool encodeAbort(const Abort& abort, MutableByteSpan output) noexcept;
[[nodiscard]] bool decodeAbort(ByteSpan input, Abort& abort) noexcept;

/** @brief Encode a connection-scoped unreliable sideband datagram. */
[[nodiscard]] bool encodeUserDatagram(ByteSpan payload, MutableByteSpan output) noexcept;

/** @brief Decode a USER_DATAGRAM and return its Service-defined payload. */
[[nodiscard]] bool decodeUserDatagram(ByteSpan input, ByteSpan& payload) noexcept;

}  // namespace wirespaces::transport::bits

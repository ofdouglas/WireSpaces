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

[[nodiscard]] bool encodeSetup(const Setup& setup, MutableByteSpan output) noexcept;
[[nodiscard]] bool decodeSetup(ByteSpan input, Setup& setup) noexcept;

[[nodiscard]] bool encodeSegmentHeader(const SegmentHeader& header,
                                       MutableByteSpan output) noexcept;
[[nodiscard]] bool decodeSegmentHeader(ByteSpan input, SegmentHeader& header) noexcept;

[[nodiscard]] bool encodeAck(const Ack& ack, MutableByteSpan output) noexcept;
[[nodiscard]] bool decodeAck(ByteSpan input, Ack& ack) noexcept;

[[nodiscard]] bool encodeProbe(const Probe& probe, MutableByteSpan output) noexcept;
[[nodiscard]] bool decodeProbe(ByteSpan input, Probe& probe) noexcept;

/** @brief Encode a connection-scoped unreliable sideband datagram. */
[[nodiscard]] bool encodeUserDatagram(ByteSpan payload, MutableByteSpan output) noexcept;

/** @brief Decode a USER_DATAGRAM and return its Service-defined payload. */
[[nodiscard]] bool decodeUserDatagram(ByteSpan input, ByteSpan& payload) noexcept;

}  // namespace wirespaces::transport::bits

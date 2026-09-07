"""Compact BITS message codec and sequence arithmetic; no device or I/O dependencies."""

from __future__ import annotations

from dataclasses import dataclass
import struct

BITS_TRANSPORT_TYPE = 1

MESSAGE_SETUP = 0
MESSAGE_SEGMENT = 1
MESSAGE_ACK = 2
MESSAGE_PROBE = 3
MESSAGE_REJECT = 4
MESSAGE_USER_DATAGRAM = 5
MESSAGE_ABORT = 6

REJECT_UNSUPPORTED_SEGMENT_SIZE = 2
REJECT_OBJECT_TOO_LARGE = 3
REJECT_BUSY = 4
REJECT_INVALID_ARGUMENT = 5

SETUP_SIZE = 10
SEGMENT_HEADER_SIZE = 4
ACK_SIZE = 6
PROBE_SIZE = 2
REJECT_SIZE = 3
ABORT_SIZE = 2
COMPACT_WINDOW_WIDTH = 16
MAXIMUM_COMPACT_SEGMENT_COUNT = 65536

REJECT_REASON_NAMES = (
    "unsupported version",
    "unsupported profile",
    "unsupported segment size",
    "object too large",
    "busy",
    "invalid argument",
    "internal error",
)


class BitsProtocolError(RuntimeError):
    """Report a terminal Compact BITS protocol or retry failure."""


@dataclass(frozen=True)
class Ack:
    """Decoded Compact BITS ACK fields."""

    session_id: int
    window_bitmap: int
    max_receive_sequence: int
    window_base: int


def _message_type(payload: bytes) -> int | None:
    """Return a Compact/version-zero message type, or None for unsupported control."""
    if not payload or payload[0] & 0xF0:
        return None
    message_type = payload[0] & 0x0F
    return message_type if message_type <= MESSAGE_ABORT else None


def _segment_count(total_size: int, segment_size: int) -> int:
    """Return the number of segments needed for a non-empty object."""
    return (total_size + segment_size - 1) // segment_size


def _sequence(initial_sequence: int, segment_index: int) -> int:
    """Map an absolute segment index into the Compact modulo-256 sequence space."""
    return (initial_sequence + segment_index) & 0xFF


def _base_sequence(initial_sequence: int, contiguous_count: int) -> int:
    """Return the cumulative ACK base before or after accepted segments."""
    return (
        (initial_sequence - 1) & 0xFF
        if contiguous_count == 0
        else _sequence(initial_sequence, contiguous_count - 1)
    )


def encode_setup(
    session_id: int,
    initial_sequence: int,
    segment_size: int,
    total_size: int,
) -> bytes:
    """Encode one Compact SETUP message."""
    count = _segment_count(total_size, segment_size)
    final_segment_index = count - 1
    final_segment_size = total_size - final_segment_index * segment_size
    return struct.pack(
        "<BBBBHHH",
        MESSAGE_SETUP,
        session_id,
        initial_sequence,
        0,
        final_segment_index,
        segment_size,
        final_segment_size,
    )


def encode_segment(session_id: int, segment_index: int, payload: bytes) -> bytes:
    """Encode one Compact SEGMENT message."""
    return struct.pack("<BBH", MESSAGE_SEGMENT, session_id, segment_index) + payload


def encode_ack(ack: Ack) -> bytes:
    """Encode one Compact ACK message."""
    return struct.pack(
        "<BBHBB",
        MESSAGE_ACK,
        ack.session_id,
        ack.window_bitmap,
        ack.max_receive_sequence,
        ack.window_base,
    )


def decode_ack(payload: bytes) -> Ack:
    """Decode and validate one Compact ACK message."""
    if len(payload) != ACK_SIZE or _message_type(payload) != MESSAGE_ACK:
        raise ValueError("invalid Compact BITS ACK")
    _, session_id, bitmap, maximum, base = struct.unpack("<BBHBB", payload)
    return Ack(session_id, bitmap, maximum, base)


def encode_probe(session_id: int) -> bytes:
    """Encode one session-specific Compact PROBE."""
    return bytes((MESSAGE_PROBE, session_id))


def encode_reject(session_id: int, reason: int) -> bytes:
    """Encode one session-specific Compact REJECT."""
    return bytes((MESSAGE_REJECT, session_id, reason))


def encode_abort(session_id: int) -> bytes:
    """Encode one session-specific Compact ABORT."""
    return bytes((MESSAGE_ABORT, session_id))

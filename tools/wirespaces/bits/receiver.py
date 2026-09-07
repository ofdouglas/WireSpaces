"""Transport-independent Compact BITS receiver state machine."""

from __future__ import annotations

import struct

from .codec import (
    MESSAGE_SETUP,
    MESSAGE_SEGMENT,
    MESSAGE_PROBE,
    MESSAGE_ABORT,
    REJECT_UNSUPPORTED_SEGMENT_SIZE,
    REJECT_OBJECT_TOO_LARGE,
    REJECT_BUSY,
    REJECT_INVALID_ARGUMENT,
    SETUP_SIZE,
    SEGMENT_HEADER_SIZE,
    PROBE_SIZE,
    ABORT_SIZE,
    COMPACT_WINDOW_WIDTH,
    MAXIMUM_COMPACT_SEGMENT_COUNT,
    Ack,
    _message_type,
    _sequence,
    _base_sequence,
    encode_ack,
    encode_reject,
)


class CompactBitsReceiver:
    """RAM-backed Compact BITS receiver for the UNO echo object."""

    def __init__(
        self,
        maximum_object_size: int,
        maximum_segment_size: int,
    ) -> None:
        self._maximum_object_size = maximum_object_size
        self._maximum_segment_size = maximum_segment_size
        self._state = "idle"
        self._session_id = 0
        self._initial_sequence = 0
        self._segment_size = 0
        self._segment_count = 0
        self._contiguous_count = 0
        self._granted_end = 0
        self._received_bitmap = 0
        self._data = bytearray()

    @property
    def complete(self) -> bool:
        """Return whether one complete object is available."""
        return self._state == "completed"

    @property
    def data(self) -> bytes:
        """Return the completed object, or an empty byte string while incomplete."""
        return bytes(self._data) if self.complete else b""

    def receive(self, payload: bytes) -> bytes | None:
        """Consume one BITS message and return its ACK/REJECT response, if any."""
        message_type = _message_type(payload)
        if message_type == MESSAGE_SETUP:
            return self._receive_setup(payload)
        if message_type == MESSAGE_SEGMENT:
            return self._receive_segment(payload)
        if message_type == MESSAGE_PROBE:
            if (
                len(payload) == PROBE_SIZE
                and self._state in ("active", "completed")
                and payload[1] == self._session_id
            ):
                return self._ack()
            return None
        if message_type == MESSAGE_ABORT:
            if (
                len(payload) == ABORT_SIZE
                and self._state == "active"
                and payload[1] == self._session_id
            ):
                self._state = "aborted"
            return None
        return None

    def _receive_setup(self, payload: bytes) -> bytes:
        """Validate SETUP, allocate bounded host RAM, and advertise the first window."""
        if len(payload) != SETUP_SIZE:
            session_id = payload[1] if len(payload) > 1 else 0
            return encode_reject(session_id, REJECT_INVALID_ARGUMENT)
        (
            _,
            session_id,
            initial,
            reserved,
            final_segment_index,
            segment_size,
            final_segment_size,
        ) = struct.unpack("<BBBBHHH", payload)
        if (
            reserved != 0
            or segment_size == 0
            or final_segment_size == 0
            or final_segment_size > segment_size
        ):
            return encode_reject(session_id, REJECT_INVALID_ARGUMENT)
        if segment_size > self._maximum_segment_size:
            return encode_reject(session_id, REJECT_UNSUPPORTED_SEGMENT_SIZE)
        count = final_segment_index + 1
        total_size = final_segment_index * segment_size + final_segment_size
        if (
            total_size > self._maximum_object_size
            or count > MAXIMUM_COMPACT_SEGMENT_COUNT
        ):
            return encode_reject(session_id, REJECT_OBJECT_TOO_LARGE)
        if self._state == "active":
            if (
                session_id == self._session_id
                and initial == self._initial_sequence
                and segment_size == self._segment_size
                and total_size == len(self._data)
            ):
                return self._ack()
            return encode_reject(session_id, REJECT_BUSY)

        self._state = "active"
        self._session_id = session_id
        self._initial_sequence = initial
        self._segment_size = segment_size
        self._segment_count = count
        self._contiguous_count = 0
        self._granted_end = min(count, COMPACT_WINDOW_WIDTH)
        self._received_bitmap = 0
        self._data = bytearray(total_size)
        return self._ack()

    def _receive_segment(self, payload: bytes) -> bytes | None:
        """Store one in-window segment and update cumulative/selective receive state."""
        if len(payload) < SEGMENT_HEADER_SIZE:
            return None
        _, session_id, segment_index = struct.unpack_from("<BBH", payload)
        if (
            self._state not in ("active", "completed")
            or session_id != self._session_id
            or segment_index >= self._segment_count
        ):
            return None
        if segment_index < self._contiguous_count:
            return self._ack()
        if self._state == "completed" or segment_index >= self._granted_end:
            return self._ack()

        window_offset = segment_index - self._contiguous_count
        segment_bit = 1 << window_offset
        if self._received_bitmap & segment_bit:
            return self._ack()

        object_offset = segment_index * self._segment_size
        expected_size = min(
            self._segment_size, len(self._data) - object_offset
        )
        segment = payload[SEGMENT_HEADER_SIZE:]
        if len(segment) != expected_size:
            return None
        self._data[object_offset : object_offset + expected_size] = segment
        self._received_bitmap |= segment_bit
        while self._received_bitmap & 1:
            self._received_bitmap >>= 1
            self._contiguous_count += 1
        self._granted_end = min(
            self._segment_count,
            self._contiguous_count + COMPACT_WINDOW_WIDTH,
        )
        if self._contiguous_count == self._segment_count:
            self._state = "completed"
        return self._ack()

    def _ack(self) -> bytes:
        """Encode current cumulative, selective, and receive-grant state."""
        base = _base_sequence(self._initial_sequence, self._contiguous_count)
        maximum = (
            _sequence(self._initial_sequence, self._granted_end - 1)
            if self._granted_end > self._contiguous_count
            else base
        )
        return encode_ack(
            Ack(
                self._session_id,
                self._received_bitmap,
                maximum,
                base,
            )
        )

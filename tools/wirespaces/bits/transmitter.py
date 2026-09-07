"""Transport-independent Compact BITS transmitter state machine."""

from __future__ import annotations

from .codec import (
    MESSAGE_ACK,
    MESSAGE_REJECT,
    MESSAGE_ABORT,
    REJECT_SIZE,
    ABORT_SIZE,
    COMPACT_WINDOW_WIDTH,
    MAXIMUM_COMPACT_SEGMENT_COUNT,
    REJECT_REASON_NAMES,
    BitsProtocolError,
    _message_type,
    _segment_count,
    _base_sequence,
    encode_setup,
    encode_segment,
    decode_ack,
    encode_probe,
    encode_abort,
)


class CompactBitsTransmitter:
    """Small stop-and-wait Compact BITS transmitter for PC hardware tests."""

    def __init__(
        self,
        object_bytes: bytes,
        segment_size: int,
        session_id: int,
        initial_sequence: int,
        retry_timeout: float = 0.25,
        maximum_retries: int = 10,
    ) -> None:
        if not object_bytes:
            raise ValueError("BITS object must not be empty")
        if not 1 <= segment_size <= 0xFFFF:
            raise ValueError("segment size must fit uint16 and be nonzero")
        count = _segment_count(len(object_bytes), segment_size)
        if count > MAXIMUM_COMPACT_SEGMENT_COUNT:
            raise ValueError("object needs too many Compact BITS segments")

        self._object = bytes(object_bytes)
        self._segment_size = segment_size
        self._segment_count = count
        self._session_id = session_id & 0xFF
        self._initial_sequence = initial_sequence & 0xFF
        self._retry_timeout = retry_timeout
        self._maximum_retries = maximum_retries
        self._state = "starting"
        self._acknowledged_count = 0
        self._granted_end = 0
        self._outstanding_segment: int | None = None
        self._last_payload: bytes | None = None
        self._last_send_time = 0.0
        self._retry_count = 0
        self._blocked_since: float | None = None

    @property
    def state(self) -> str:
        """Return the transmitter's observable test state."""
        return self._state

    @property
    def complete(self) -> bool:
        """Return whether every segment has been cumulatively acknowledged."""
        return self._state == "completed"

    def poll(self, now: float) -> bytes | None:
        """Return at most one newly generated or retransmitted protocol message."""
        if self._state in ("completed", "rejected", "aborted"):
            return None

        if self._state == "starting":
            if self._last_payload is None:
                return self._record_send(
                    encode_setup(
                        self._session_id,
                        self._initial_sequence,
                        self._segment_size,
                        len(self._object),
                    ),
                    now,
                    retransmission=False,
                )
            if now - self._last_send_time >= self._retry_timeout:
                return self._retry_or_abort(now)
            return None

        if self._outstanding_segment is not None:
            if now - self._last_send_time >= self._retry_timeout:
                return self._retry_or_abort(now)
            return None

        if self._acknowledged_count < self._granted_end:
            segment_index = self._acknowledged_count
            start = segment_index * self._segment_size
            segment = self._object[start : start + self._segment_size]
            self._outstanding_segment = segment_index
            self._blocked_since = None
            return self._record_send(
                encode_segment(self._session_id, segment_index, segment),
                now,
                retransmission=False,
            )

        if self._blocked_since is None:
            self._blocked_since = now
        elif now - self._blocked_since >= self._retry_timeout:
            if self._retry_count >= self._maximum_retries:
                self._state = "aborted"
                return encode_abort(self._session_id)
            self._blocked_since = now
            self._retry_count += 1
            return encode_probe(self._session_id)
        return None

    def receive(self, payload: bytes) -> None:
        """Consume ACK, REJECT, or ABORT traffic for this transmitter."""
        message_type = _message_type(payload)
        if message_type == MESSAGE_REJECT:
            if len(payload) != REJECT_SIZE or payload[1] != self._session_id:
                return
            reason = payload[2]
            reason_name = (
                REJECT_REASON_NAMES[reason]
                if reason < len(REJECT_REASON_NAMES)
                else f"unknown reason {reason}"
            )
            self._state = "rejected"
            raise BitsProtocolError(f"Arduino rejected BITS SETUP: {reason_name}")
        if message_type == MESSAGE_ABORT:
            if len(payload) == ABORT_SIZE and payload[1] == self._session_id:
                self._state = "aborted"
                raise BitsProtocolError("Arduino aborted the upload")
            return
        if message_type != MESSAGE_ACK:
            return

        ack = decode_ack(payload)
        if ack.session_id != self._session_id:
            return
        if self._state not in ("starting", "active"):
            return
        was_starting = self._state == "starting"
        if was_starting and self._last_payload is None:
            return  # SETUP has not been emitted yet.
        current_base = _base_sequence(
            self._initial_sequence, self._acknowledged_count
        )
        advance = (ack.window_base - current_base) & 0xFF
        grant_span = (ack.max_receive_sequence - ack.window_base) & 0xFF
        if (
            advance > 1
            or self._acknowledged_count + advance > self._segment_count
            or grant_span > COMPACT_WINDOW_WIDTH
            or self._acknowledged_count + advance + grant_span
            > self._segment_count
        ):
            return

        if advance and self._outstanding_segment != self._acknowledged_count:
            return  # A stale cumulative ACK cannot cover an unsent segment.

        self._acknowledged_count += advance
        self._granted_end = max(
            self._granted_end, self._acknowledged_count + grant_span
        )
        if advance == 1:
            self._outstanding_segment = None
        if was_starting or advance == 1 or self._outstanding_segment is None:
            self._last_payload = None
            self._retry_count = 0
            self._blocked_since = None

        if self._acknowledged_count == self._segment_count:
            self._state = "completed"
        else:
            self._state = "active"

    def _record_send(
        self, payload: bytes, now: float, retransmission: bool
    ) -> bytes:
        """Record timeout state for a SETUP or SEGMENT transmission."""
        self._last_payload = payload
        self._last_send_time = now
        self._retry_count = self._retry_count + 1 if retransmission else 0
        return payload

    def _retry_or_abort(self, now: float) -> bytes:
        """Regenerate the outstanding message or emit ABORT at the retry limit."""
        if self._retry_count >= self._maximum_retries:
            self._state = "aborted"
            return encode_abort(self._session_id)
        if self._last_payload is None:
            raise BitsProtocolError("missing retransmission state")
        return self._record_send(
            self._last_payload,
            now,
            retransmission=True,
        )

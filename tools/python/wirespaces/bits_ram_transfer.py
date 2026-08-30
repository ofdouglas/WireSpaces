#!/usr/bin/env python3
"""Exercise Compact BITS by round-tripping arbitrary RAM data through an Arduino UNO."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
from pathlib import Path
import random
import struct
import sys
import time

import serial

from .receiver import HdlcStreamDecoder, WireSpacesPacket, encode_hdlc_frame


BITS_TRANSPORT_TYPE = 1
NORMAL_BITS_CONTROL = 0x81
BACKGROUND_BITS_CONTROL = 0xC1
TEST_WIRE = 1
ARDUINO_HOST = 1
PC_HOST = 2
UPLOAD_ENDPOINT = 0x0001
ECHO_ENDPOINT = 0x0002

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

SETUP_SIZE = 9
SEGMENT_HEADER_SIZE = 4
ACK_SIZE = 6
PROBE_SIZE = 2
REJECT_SIZE = 3
ABORT_SIZE = 2
COMPACT_WINDOW_WIDTH = 16
MAXIMUM_COMPACT_SEGMENT_COUNT = 65536
UNO_MAXIMUM_OBJECT_SIZE = 256
UNO_SEGMENT_SIZE = 24

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
    return struct.pack(
        "<BBBHI",
        MESSAGE_SETUP,
        session_id,
        initial_sequence,
        segment_size,
        total_size,
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
        was_starting = self._state == "starting"
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
        _, session_id, initial, segment_size, total_size = struct.unpack(
            "<BBBHI", payload
        )
        if segment_size == 0 or total_size == 0:
            return encode_reject(session_id, REJECT_INVALID_ARGUMENT)
        if segment_size > self._maximum_segment_size:
            return encode_reject(session_id, REJECT_UNSUPPORTED_SEGMENT_SIZE)
        count = _segment_count(total_size, segment_size)
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


def _send_bits_message(
    uart: serial.Serial,
    endpoint: int,
    payload: bytes,
) -> None:
    """Frame and send one PC-to-UNO Compact BITS protocol message."""
    message_type = _message_type(payload)
    control = (
        BACKGROUND_BITS_CONTROL
        if message_type == MESSAGE_SEGMENT
        else NORMAL_BITS_CONTROL
    )
    packet = WireSpacesPacket(
        control=control,
        wire_number=TEST_WIRE,
        source_participant=PC_HOST,
        destination_participant=ARDUINO_HOST,
        endpoint=endpoint,
        payload=payload,
    )
    uart.write(encode_hdlc_frame(packet.encode()))
    uart.flush()


def round_trip(
    uart: serial.Serial,
    object_bytes: bytes,
    segment_size: int,
    timeout_seconds: float,
) -> bytes:
    """Upload arbitrary bytes to UNO RAM and receive its BITS echo transfer."""
    transmitter = CompactBitsTransmitter(
        object_bytes,
        segment_size,
        session_id=0x51,
        initial_sequence=0xFA,
    )
    receiver = CompactBitsReceiver(
        maximum_object_size=UNO_MAXIMUM_OBJECT_SIZE,
        maximum_segment_size=UNO_SEGMENT_SIZE,
    )
    decoder = HdlcStreamDecoder()
    deadline = time.monotonic() + timeout_seconds

    while time.monotonic() < deadline:
        now = time.monotonic()
        outbound = transmitter.poll(now)
        if outbound is not None:
            _send_bits_message(uart, UPLOAD_ENDPOINT, outbound)
            if transmitter.state == "aborted":
                raise BitsProtocolError("upload retry limit exhausted")

        for frame in decoder.feed(uart.read(64)):
            try:
                packet = WireSpacesPacket.decode(frame)
            except ValueError:
                continue
            if (
                packet.transport_type != BITS_TRANSPORT_TYPE
                or packet.wire_number != TEST_WIRE
                or packet.source_participant != ARDUINO_HOST
                or packet.destination_participant != PC_HOST
            ):
                continue

            if packet.endpoint == UPLOAD_ENDPOINT:
                transmitter.receive(packet.payload)
            elif packet.endpoint == ECHO_ENDPOINT:
                response = receiver.receive(packet.payload)
                if response is not None:
                    _send_bits_message(uart, ECHO_ENDPOINT, response)

        if transmitter.complete and receiver.complete:
            return receiver.data

    raise BitsProtocolError(
        f"BITS loopback timed out: upload={transmitter.state} "
        f"echo={'completed' if receiver.complete else 'incomplete'}"
    )


def parse_arguments() -> argparse.Namespace:
    """Parse RAM object generation and serial connection options."""
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group()
    source.add_argument("--file", type=Path, help="upload the exact bytes from this file")
    source.add_argument(
        "--hex",
        dest="hex_bytes",
        help="upload hexadecimal bytes; spaces and ':' separators are accepted",
    )
    parser.add_argument(
        "--size",
        type=int,
        default=128,
        help="generated object size when --file/--hex is omitted (default: 128)",
    )
    parser.add_argument(
        "--seed",
        type=lambda value: int(value, 0),
        default=0xB175,
        help="generated-object PRNG seed (default: 0xB175)",
    )
    parser.add_argument(
        "--segment-size",
        type=int,
        default=UNO_SEGMENT_SIZE,
        help=f"PC upload segment size, 1..{UNO_SEGMENT_SIZE} (default: {UNO_SEGMENT_SIZE})",
    )
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=10.0)
    parser.add_argument(
        "--reset",
        action="store_true",
        help="pulse DTR and wait for the UNO bootloader before testing",
    )
    return parser.parse_args()


def _load_object(arguments: argparse.Namespace) -> bytes:
    """Load literal input or generate deterministic arbitrary test bytes."""
    if arguments.file is not None:
        object_bytes = arguments.file.read_bytes()
    elif arguments.hex_bytes is not None:
        normalized = arguments.hex_bytes.replace(" ", "").replace(":", "")
        object_bytes = bytes.fromhex(normalized)
    else:
        if not 1 <= arguments.size <= UNO_MAXIMUM_OBJECT_SIZE:
            raise ValueError(
                f"size must be between 1 and {UNO_MAXIMUM_OBJECT_SIZE}"
            )
        generator = random.Random(arguments.seed)
        object_bytes = bytes(
            generator.getrandbits(8) for _ in range(arguments.size)
        )
    if not 1 <= len(object_bytes) <= UNO_MAXIMUM_OBJECT_SIZE:
        raise ValueError(
            f"object must contain 1..{UNO_MAXIMUM_OBJECT_SIZE} bytes"
        )
    return object_bytes


def main() -> int:
    """Run one full PC-to-UNO-to-PC RAM-backed BITS transfer."""
    arguments = parse_arguments()
    try:
        object_bytes = _load_object(arguments)
        if not 1 <= arguments.segment_size <= UNO_SEGMENT_SIZE:
            raise ValueError(
                f"segment size must be between 1 and {UNO_SEGMENT_SIZE}"
            )
    except (OSError, ValueError) as error:
        print(error, file=sys.stderr)
        return 2

    try:
        with serial.Serial(arguments.port, arguments.baud, timeout=0.01) as uart:
            if arguments.reset:
                uart.dtr = False
                time.sleep(0.05)
                uart.dtr = True
                time.sleep(2.0)
                uart.reset_input_buffer()
            echoed = round_trip(
                uart,
                object_bytes,
                arguments.segment_size,
                arguments.timeout,
            )
    except (serial.SerialException, BitsProtocolError) as error:
        print(error, file=sys.stderr)
        return 1

    if echoed != object_bytes:
        print(
            f"BITS RAM mismatch: sent {len(object_bytes)} bytes, "
            f"received {len(echoed)} bytes",
            file=sys.stderr,
        )
        return 1

    digest = hashlib.sha256(echoed).hexdigest()
    print(
        f"BITS RAM round trip passed: {len(echoed)} bytes "
        f"segment_size={arguments.segment_size} sha256={digest}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

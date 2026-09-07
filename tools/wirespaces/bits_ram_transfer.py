#!/usr/bin/env python3
"""Exercise Compact BITS by round-tripping arbitrary RAM data through an Arduino UNO."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import random
import sys
import time

import serial

# Preserve imports used by existing scripts while keeping implementation in its owning module.
from .bits import (
    BITS_TRANSPORT_TYPE,
    MESSAGE_SETUP,
    MESSAGE_SEGMENT,
    MESSAGE_ACK,
    MESSAGE_PROBE,
    MESSAGE_REJECT,
    MESSAGE_USER_DATAGRAM,
    MESSAGE_ABORT,
    REJECT_UNSUPPORTED_SEGMENT_SIZE,
    REJECT_OBJECT_TOO_LARGE,
    REJECT_BUSY,
    REJECT_INVALID_ARGUMENT,
    SETUP_SIZE,
    SEGMENT_HEADER_SIZE,
    ACK_SIZE,
    PROBE_SIZE,
    REJECT_SIZE,
    ABORT_SIZE,
    COMPACT_WINDOW_WIDTH,
    MAXIMUM_COMPACT_SEGMENT_COUNT,
    REJECT_REASON_NAMES,
    BitsProtocolError,
    Ack,
    encode_setup,
    encode_segment,
    encode_ack,
    decode_ack,
    encode_probe,
    encode_reject,
    encode_abort,
    CompactBitsReceiver,
    CompactBitsTransmitter,
)
from .bits.codec import (
    _message_type,
    _segment_count,
    _sequence,
    _base_sequence,
)
from .bench.uno import (
    NORMAL_BITS_CONTROL,
    BACKGROUND_BITS_CONTROL,
    TEST_WIRE,
    ARDUINO_HOST,
    PC_HOST,
    UPLOAD_ENDPOINT,
    ECHO_ENDPOINT,
    UNO_MAXIMUM_OBJECT_SIZE,
    UNO_SEGMENT_SIZE,
)
from .bench.bits_ram_transfer import _send_bits_message, round_trip


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
    parser.add_argument("--port", default="/dev/arduino-uno")
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

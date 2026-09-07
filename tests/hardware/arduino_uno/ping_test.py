#!/usr/bin/env python3
"""Send a WireSpaces ping request and require its matching response."""

from __future__ import annotations

import argparse
import struct
import sys
import time

import serial

from wirespaces.hdlc import HdlcStreamDecoder, encode_hdlc_frame
from wirespaces.packet import WireSpacesPacket


PING_MAGIC = 0xAB
PING_REQUEST = 0x01
PING_RESPONSE = 0x02
PING_ENDPOINT = 0xFFFD
NORMAL_SIMPLE_CONTROL = 0x80


def parse_arguments() -> argparse.Namespace:
    """Parse hardware-test settings."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/arduino-uno")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=5.0)
    parser.add_argument("--sequence", type=lambda value: int(value, 0), default=0x7E7D)
    parser.add_argument("--reset", action="store_true")
    return parser.parse_args()


def main() -> int:
    """Run one request/response exchange against the attached Arduino."""
    arguments = parse_arguments()
    if not 0 <= arguments.sequence <= 0xFFFF:
        print("sequence must fit uint16", file=sys.stderr)
        return 2

    request = WireSpacesPacket(
        control=NORMAL_SIMPLE_CONTROL,
        wire_number=1,
        source_host=2,
        destination_host=1,
        endpoint=PING_ENDPOINT,
        payload=struct.pack("<BBH", PING_MAGIC, PING_REQUEST, arguments.sequence),
    )
    encoded_request = encode_hdlc_frame(request.encode())
    decoder = HdlcStreamDecoder()

    try:
        with serial.Serial(arguments.port, arguments.baud, timeout=0.1) as uart:
            if arguments.reset:
                uart.dtr = False
                time.sleep(0.05)
                uart.dtr = True
                time.sleep(1.5)
                uart.reset_input_buffer()

            deadline = time.monotonic() + arguments.timeout
            next_send = 0.0
            while time.monotonic() < deadline:
                now = time.monotonic()
                if now >= next_send:
                    uart.write(encoded_request)
                    uart.flush()
                    next_send = now + 0.5

                for frame in decoder.feed(uart.read(64)):
                    try:
                        packet = WireSpacesPacket.decode(frame)
                    except ValueError:
                        continue
                    if len(packet.payload) != 4:
                        continue

                    magic, message_type, sequence = struct.unpack(
                        "<BBH", packet.payload
                    )
                    if (
                        packet.wire_number == request.wire_number
                        and packet.source_host == request.destination_host
                        and packet.destination_host == request.source_host
                        and packet.endpoint == request.endpoint
                        and magic == PING_MAGIC
                        and message_type == PING_RESPONSE
                        and sequence == arguments.sequence
                    ):
                        print(
                            f"ping response wire={packet.wire_number} "
                            f"src={packet.source_host} "
                            f"dst={packet.destination_host} "
                            f"endpoint=0x{packet.endpoint:04X} "
                            f"sequence=0x{sequence:04X}"
                        )
                        return 0
    except serial.SerialException as error:
        print(f"serial error: {error}", file=sys.stderr)
        return 2

    print(
        f"no matching ping response for sequence 0x{arguments.sequence:04X}",
        file=sys.stderr,
    )
    return 1


if __name__ == "__main__":
    raise SystemExit(main())

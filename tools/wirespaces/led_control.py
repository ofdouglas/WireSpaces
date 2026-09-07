#!/usr/bin/env python3
"""Set Arduino UNO built-in LED brightness through WireSpaces."""

from __future__ import annotations

import argparse
import struct
import sys
import time

import serial

from .hdlc import HdlcStreamDecoder, encode_hdlc_frame
from .packet import WireSpacesPacket


LED_MAGIC = 0x4C
LED_REQUEST = 0x01
LED_RESPONSE = 0x02
LED_ENDPOINT = 0xFFFB
STACK_REPORT_ENDPOINT = 0xFFFC
NORMAL_SIMPLE_CONTROL = 0x80


def parse_arguments() -> argparse.Namespace:
    """Parse LED command and serial settings."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "brightness",
        help="brightness as 0..255, 0%..100%, or 'on'/'off'",
    )
    parser.add_argument("--port", default="/dev/arduino-uno")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=5.0)
    parser.add_argument(
        "--sequence",
        type=lambda value: int(value, 0),
        default=0x7E,
    )
    parser.add_argument(
        "--wait-stack-report",
        action="store_true",
        help="also print the first stack report received after acknowledgement",
    )
    return parser.parse_args()


def parse_brightness(value: str) -> int:
    """Parse an 8-bit value, percentage, or on/off alias."""
    aliases = {"off": 0, "on": 255}
    try:
        if value.lower() in aliases:
            brightness = aliases[value.lower()]
        elif value.endswith("%"):
            percentage = float(value[:-1])
            if not 0.0 <= percentage <= 100.0:
                raise ValueError
            brightness = int((percentage * 255.0 / 100.0) + 0.5)
        else:
            brightness = int(value, 0)
    except ValueError as error:
        raise ValueError(
            "brightness must be 0..255, 0%..100%, 'on', or 'off'"
        ) from error
    if not 0 <= brightness <= 255:
        raise ValueError("brightness must be between 0 and 255")
    return brightness


def main() -> int:
    """Send one LED brightness command and require its acknowledgement."""
    arguments = parse_arguments()
    if not 0 <= arguments.sequence <= 0xFF:
        print("sequence must fit uint8", file=sys.stderr)
        return 2

    try:
        requested_brightness = parse_brightness(arguments.brightness)
    except ValueError as error:
        print(error, file=sys.stderr)
        return 2

    request = WireSpacesPacket(
        control=NORMAL_SIMPLE_CONTROL,
        wire_number=1,
        source_host=2,
        destination_host=1,
        endpoint=LED_ENDPOINT,
        payload=struct.pack(
            "<BBBB",
            LED_MAGIC,
            LED_REQUEST,
            requested_brightness,
            arguments.sequence,
        ),
    )
    encoded_request = encode_hdlc_frame(request.encode())
    decoder = HdlcStreamDecoder()
    acknowledged = False

    try:
        with serial.Serial(arguments.port, arguments.baud, timeout=0.1) as uart:
            deadline = time.monotonic() + arguments.timeout
            next_send = 0.0
            while time.monotonic() < deadline:
                now = time.monotonic()
                if not acknowledged and now >= next_send:
                    uart.write(encoded_request)
                    uart.flush()
                    next_send = now + 0.5

                for frame in decoder.feed(uart.read(64)):
                    try:
                        packet = WireSpacesPacket.decode(frame)
                    except ValueError:
                        continue

                    if packet.endpoint == LED_ENDPOINT and len(packet.payload) == 4:
                        magic, message_type, brightness, sequence = struct.unpack(
                            "<BBBB", packet.payload
                        )
                        if (
                            packet.source_host
                            == request.destination_host
                            and packet.destination_host
                            == request.source_host
                            and magic == LED_MAGIC
                            and message_type == LED_RESPONSE
                            and brightness == requested_brightness
                            and sequence == arguments.sequence
                        ):
                            acknowledged = True
                            percentage = 100.0 * brightness / 255.0
                            print(
                                f"LED brightness {brightness}/255 "
                                f"({percentage:.1f}%) acknowledged "
                                f"by Participant {packet.source_host} "
                                f"sequence=0x{sequence:02X}"
                            )
                            if not arguments.wait_stack_report:
                                return 0

                    if (
                        acknowledged
                        and packet.endpoint == STACK_REPORT_ENDPOINT
                        and len(packet.payload) == 4
                    ):
                        peak_used, capacity = struct.unpack(
                            "<HH", packet.payload
                        )
                        utilization = (100.0 * peak_used) / capacity
                        print(
                            f"stack peak={peak_used} B capacity={capacity} B "
                            f"utilization={utilization:.1f}%"
                        )
                        return 0
    except serial.SerialException as error:
        print(f"serial error: {error}", file=sys.stderr)
        return 2

    if acknowledged:
        print("LED command succeeded, but no stack report arrived", file=sys.stderr)
    else:
        print("no matching LED acknowledgement received", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())

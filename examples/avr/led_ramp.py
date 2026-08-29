#!/usr/bin/env python3
"""Ramp UNO LED brightness from 0% to 100% over one second."""

from __future__ import annotations

import argparse
import struct
import sys
import time

import serial

from led_control import (
    LED_ENDPOINT,
    LED_MAGIC,
    LED_REQUEST,
    LED_RESPONSE,
    NORMAL_SIMPLE_CONTROL,
)
from receiver import HdlcStreamDecoder, WireSpacesPacket, encode_hdlc_frame


def parse_arguments() -> argparse.Namespace:
    """Parse serial settings."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    return parser.parse_args()


def set_brightness(
    uart: serial.Serial,
    decoder: HdlcStreamDecoder,
    brightness: int,
    sequence: int,
    timeout: float,
) -> bool:
    """Send one brightness command and await its matching response."""
    request = WireSpacesPacket(
        control=NORMAL_SIMPLE_CONTROL,
        wire_number=1,
        source_participant=2,
        destination_participant=1,
        endpoint=LED_ENDPOINT,
        payload=struct.pack(
            "<BBBB",
            LED_MAGIC,
            LED_REQUEST,
            brightness,
            sequence,
        ),
    )
    encoded = encode_hdlc_frame(request.encode())
    deadline = time.monotonic() + timeout
    next_send = 0.0

    while time.monotonic() < deadline:
        now = time.monotonic()
        if now >= next_send:
            uart.write(encoded)
            uart.flush()
            next_send = now + 0.03

        for frame in decoder.feed(uart.read(32)):
            try:
                packet = WireSpacesPacket.decode(frame)
            except ValueError:
                continue
            if packet.endpoint != LED_ENDPOINT or len(packet.payload) != 4:
                continue

            magic, message_type, applied, response_sequence = struct.unpack(
                "<BBBB", packet.payload
            )
            if (
                packet.source_participant == 1
                and packet.destination_participant == 2
                and magic == LED_MAGIC
                and message_type == LED_RESPONSE
                and applied == brightness
                and response_sequence == sequence
            ):
                return True
    return False


def main() -> int:
    """Execute eleven acknowledged steps at 100 ms intervals."""
    arguments = parse_arguments()
    decoder = HdlcStreamDecoder()

    try:
        with serial.Serial(arguments.port, arguments.baud, timeout=0.005) as uart:
            # Opening an UNO serial port may reset it into the bootloader.
            time.sleep(1.5)
            uart.reset_input_buffer()

            ramp_start = time.monotonic()
            for step in range(11):
                target_time = ramp_start + (step * 0.1)
                time.sleep(max(0.0, target_time - time.monotonic()))

                brightness = (step * 255 + 5) // 10
                if not set_brightness(
                    uart,
                    decoder,
                    brightness,
                    sequence=step,
                    timeout=0.09,
                ):
                    print(
                        f"no acknowledgement at {step * 10}% "
                        f"({brightness}/255)",
                        file=sys.stderr,
                    )
                    return 1
                print(f"{step * 10:3d}% -> {brightness:3d}/255")

            elapsed = time.monotonic() - ramp_start
            print(f"ramp complete in {elapsed:.3f} s")
            return 0
    except serial.SerialException as error:
        print(f"serial error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())

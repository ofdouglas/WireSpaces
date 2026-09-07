#!/usr/bin/env python3
"""Require one valid peak stack-utilization report from the Arduino."""

from __future__ import annotations

import argparse
import struct
import sys
import time

import serial

from wirespaces.hdlc import HdlcStreamDecoder
from wirespaces.packet import WireSpacesPacket


STACK_REPORT_ENDPOINT = 0xFFFC
ATMEGA328P_SRAM_BYTES = 2048


def parse_arguments() -> argparse.Namespace:
    """Parse hardware-test settings."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/arduino-uno")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=5.0)
    parser.add_argument("--reset", action="store_true")
    return parser.parse_args()


def main() -> int:
    """Receive and validate one stack report."""
    arguments = parse_arguments()
    decoder = HdlcStreamDecoder()

    try:
        with serial.Serial(arguments.port, arguments.baud, timeout=0.1) as uart:
            if arguments.reset:
                uart.dtr = False
                time.sleep(0.05)
                uart.dtr = True

            deadline = time.monotonic() + arguments.timeout
            while time.monotonic() < deadline:
                for frame in decoder.feed(uart.read(64)):
                    try:
                        packet = WireSpacesPacket.decode(frame)
                    except ValueError:
                        continue
                    if (
                        packet.endpoint != STACK_REPORT_ENDPOINT
                        or len(packet.payload) != 4
                    ):
                        continue

                    peak_used, capacity = struct.unpack("<HH", packet.payload)
                    if (
                        capacity == 0
                        or capacity > ATMEGA328P_SRAM_BYTES
                        or peak_used == 0
                        or peak_used > capacity
                    ):
                        print(
                            f"invalid stack report: peak={peak_used} "
                            f"capacity={capacity}",
                            file=sys.stderr,
                        )
                        return 1

                    percentage = (100.0 * peak_used) / capacity
                    print(
                        f"stack report peak={peak_used} B "
                        f"capacity={capacity} B utilization={percentage:.1f}%"
                    )
                    return 0
    except serial.SerialException as error:
        print(f"serial error: {error}", file=sys.stderr)
        return 2

    print("no stack report received", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())

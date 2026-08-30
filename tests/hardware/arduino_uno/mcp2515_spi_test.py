#!/usr/bin/env python3
"""Verify basic SPI communication between an Arduino Uno and MCP2515."""

from __future__ import annotations

import argparse
import re
import sys
import time

import serial


STATUS_PATTERN = re.compile(rb"([CE]):([0-9A-F]{2})\r\n")


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=5.0)
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()

    try:
        with serial.Serial(arguments.port, arguments.baud, timeout=0.1) as uart:
            uart.dtr = False
            time.sleep(0.05)
            uart.dtr = True

            deadline = time.monotonic() + arguments.timeout
            received = bytearray()
            while time.monotonic() < deadline:
                received.extend(uart.read(64))
                match = STATUS_PATTERN.search(received)
                if match is None:
                    continue

                status = match.group(1).decode("ascii")
                canstat = int(match.group(2), 16)
                if status == "C" and canstat == 0x80:
                    print("MCP2515 SPI communication passed: CANSTAT=0x80")
                    return 0

                print(
                    f"MCP2515 SPI communication failed: {status}:{canstat:02X}",
                    file=sys.stderr,
                )
                return 1
    except serial.SerialException as error:
        print(f"serial error: {error}", file=sys.stderr)
        return 2

    print("timed out waiting for MCP2515 status", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())

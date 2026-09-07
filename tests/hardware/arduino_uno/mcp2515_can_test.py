#!/usr/bin/env python3
"""Exchange raw Classical CAN frames between an Uno MCP2515 and CANtact."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import sys
import time

import can
import serial


@dataclass(frozen=True)
class CanFrame:
    identifier: int
    data: bytes


CANTACT_TO_ARDUINO = (
    CanFrame(0x123, b"\x00"),
    CanFrame(0x321, b"\x7e\x7d\x00\xff"),
    CanFrame(0x7AA, bytes(range(8))),
)

ARDUINO_TO_CANTACT = (
    CanFrame(0x045, b""),
    CanFrame(0x456, b"\xde\xad\xbe\xef"),
    CanFrame(0x6F0, b"\x00\x11\x22\x33\x44\x55\x66\x77"),
)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arduino-port", required=True)
    parser.add_argument("--cantact-interface", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=5.0)
    return parser.parse_args()


def encode_arduino_transmit(frame: CanFrame) -> bytes:
    return f"T:{frame.identifier:03X}:{frame.data.hex().upper()}\r".encode("ascii")


def parse_arduino_receive(line: bytes) -> CanFrame | None:
    parts = line.split(b":", 2)
    if len(parts) != 3 or parts[0] != b"R":
        return None
    try:
        identifier = int(parts[1], 16)
        data = bytes.fromhex(parts[2].decode("ascii"))
    except (UnicodeDecodeError, ValueError):
        return None
    if identifier > 0x7FF or len(data) > 8:
        return None
    return CanFrame(identifier, data)


def read_until(uart: serial.Serial, deadline: float, parser, expected):
    observed: list[bytes] = []
    while time.monotonic() < deadline:
        line = uart.read_until(b"\r")
        if not line:
            continue
        line = line.strip(b"\r\n")
        observed.append(line)
        parsed = parser(line)
        if parsed == expected:
            return
    raise RuntimeError(f"timed out waiting for {expected}; observed={observed!r}")


def wait_for_ready(arduino: serial.Serial, deadline: float) -> None:
    observed: list[bytes] = []
    while time.monotonic() < deadline:
        line = arduino.read_until(b"\n").strip()
        if not line:
            continue
        observed.append(line)
        if line == b"READY:500K":
            return
        if line.startswith(b"ERROR:"):
            raise RuntimeError(line.decode("ascii", "replace"))
    raise RuntimeError(f"timed out waiting for Arduino readiness; observed={observed!r}")


def wait_for_transmit_accept(arduino: serial.Serial, deadline: float) -> None:
    observed: list[bytes] = []
    while time.monotonic() < deadline:
        line = arduino.read_until(b"\n").strip()
        if not line:
            continue
        observed.append(line)
        if line == b"T:OK":
            return
        if line.startswith(b"T:"):
            raise RuntimeError(f"Arduino rejected CAN transmit: {line!r}")
    raise RuntimeError(f"timed out waiting for transmit acceptance; observed={observed!r}")


def read_until_can(bus: can.Bus, deadline: float, expected: CanFrame) -> None:
    observed: list[can.Message] = []
    while time.monotonic() < deadline:
        message = bus.recv(timeout=0.1)
        if message is None:
            continue
        observed.append(message)
        if message.arbitration_id == expected.identifier and message.data == expected.data:
            return
    raise RuntimeError(f"timed out waiting for {expected}; observed={observed!r}")


def run_test(arguments: argparse.Namespace) -> None:
    with (
        # SocketCAN timing is configured by the OS/slcand, not this constructor.
        can.interface.Bus(interface="socketcan", channel=arguments.cantact_interface) as cantact_bus,
        serial.Serial(arguments.arduino_port, arguments.baud, timeout=0.1) as arduino
    ):
        arduino.dtr = False
        time.sleep(0.05)
        # After flashing, a previous boot's READY may still be queued by USB.
        # Drain it before triggering the new boot.
        arduino.reset_input_buffer()
        arduino.dtr = True
        wait_for_ready(arduino, time.monotonic() + arguments.timeout)

        for frame in CANTACT_TO_ARDUINO:
            cantact_bus.send(can.Message(arbitration_id=frame.identifier, data=frame.data, is_extended_id=False))
            read_until(
                arduino,
                time.monotonic() + arguments.timeout,
                parse_arduino_receive,
                frame,
            )

        for frame in ARDUINO_TO_CANTACT:
            arduino.write(encode_arduino_transmit(frame))
            arduino.flush()
            wait_for_transmit_accept(
                arduino, time.monotonic() + arguments.timeout
            )
            read_until_can(
                cantact_bus,
                time.monotonic() + arguments.timeout,
                frame,
            )


def main() -> int:
    arguments = parse_arguments()
    try:
        run_test(arguments)
    except (RuntimeError, serial.SerialException, can.CanError) as error:
        print(f"MCP2515 CAN test failed: {error}", file=sys.stderr)
        return 1

    print(
        "MCP2515 CAN test passed: "
        f"{len(CANTACT_TO_ARDUINO)} CANtact->Arduino and "
        f"{len(ARDUINO_TO_CANTACT)} Arduino->CANtact frames at 500 kbit/s"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

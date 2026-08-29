#!/usr/bin/env python3
"""Minimal diagnostic decoder for the AVR heartbeat HDLC frame."""

import struct
import sys
import time

import serial


FLAG = 0x7E
ESCAPE = 0x7D
ESCAPE_XOR = 0x20
EXPECTED_UPTIME = 0xC0DEBABE


def decode_frame(encoded: bytes) -> bytes:
    """Remove HDLC byte stuffing from one frame body."""
    decoded = bytearray()
    escaped = False
    for byte in encoded:
        if escaped:
            decoded.append(byte ^ ESCAPE_XOR)
            escaped = False
        elif byte == ESCAPE:
            escaped = True
        else:
            decoded.append(byte)
    if escaped:
        raise ValueError("truncated HDLC escape")
    return bytes(decoded)


def main() -> int:
    port = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"
    baud = int(sys.argv[2]) if len(sys.argv) > 2 else 115200

    with serial.Serial(port, baud, timeout=0.1) as uart:
        uart.dtr = False
        time.sleep(0.05)
        uart.dtr = True

        deadline = time.monotonic() + 5.0
        stream = bytearray()
        while time.monotonic() < deadline:
            stream.extend(uart.read(64))

            while FLAG in stream:
                start = stream.index(FLAG)
                del stream[: start + 1]
                if FLAG not in stream:
                    break

                frame_end = stream.index(FLAG)
                encoded = bytes(stream[:frame_end])
                del stream[: frame_end + 1]
                if not encoded:
                    continue

                frame = decode_frame(encoded)
                if len(frame) != 10:
                    continue

                control, wire, source, destination, endpoint, uptime = struct.unpack(
                    "<BBBBHI", frame
                )
                print(
                    f"frame={frame.hex(' ')} wire={wire} src={source} "
                    f"dst={destination} endpoint=0x{endpoint:04X} "
                    f"heartbeat=0x{uptime:08X}"
                )
                if uptime == EXPECTED_UPTIME:
                    return 0

    print(f"did not receive heartbeat 0x{EXPECTED_UPTIME:08X}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())

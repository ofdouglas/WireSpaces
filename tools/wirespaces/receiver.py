#!/usr/bin/env python3
"""Receive HDLC-framed WireSpaces packets from a serial Link."""

from __future__ import annotations

import argparse
import logging
import sys
import time

import serial

# Keep the original protocol imports available to existing callers.
from .packet import (
    HEADER_FORMAT,
    HEADER_SIZE,
    ENDPOINT_ID_MASK,
    ENDPOINT_NAMESPACE_SHIFT,
    QOS_NAMES,
    NAMESPACE_NAMES,
    WireSpacesPacket,
    format_packet,
)
from .hdlc import (
    HDLC_FLAG,
    HDLC_ESCAPE,
    HDLC_ESCAPE_XOR,
    HDLC_CRC_SIZE,
    crc16_ccitt_false,
    encode_hdlc_frame,
    HdlcStreamDecoder,
)

LOGGER = logging.getLogger("wirespaces.receiver")


def receive_packets(
    uart: serial.Serial,
    packet_limit: int | None,
    timeout_seconds: float | None,
) -> int:
    """Read, decode, and log packets until a configured stopping condition."""
    decoder = HdlcStreamDecoder()
    received = 0
    deadline = (
        None if timeout_seconds is None else time.monotonic() + timeout_seconds
    )

    while packet_limit is None or received < packet_limit:
        if deadline is not None and time.monotonic() >= deadline:
            break

        chunk = uart.read(64)
        for frame in decoder.feed(chunk):
            try:
                packet = WireSpacesPacket.decode(frame)
            except ValueError as error:
                LOGGER.warning("dropping invalid WireSpaces frame: %s", error)
                continue

            LOGGER.info("%s", format_packet(packet))
            received += 1
            if packet_limit is not None and received >= packet_limit:
                break

    return received


def parse_arguments() -> argparse.Namespace:
    """Parse command-line receiver settings."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--port",
        default="/dev/arduino-uno",
        help="serial device (default: /dev/arduino-uno)",
    )
    parser.add_argument(
        "--baud",
        default=115200,
        type=int,
        help="serial baud rate (default: 115200)",
    )
    parser.add_argument(
        "--count",
        type=int,
        help="exit after receiving this many packets",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        help="exit after this many seconds",
    )
    parser.add_argument(
        "--reset",
        action="store_true",
        help="pulse DTR after opening the port to reset an Arduino UNO",
    )
    parser.add_argument(
        "--log-level",
        choices=("DEBUG", "INFO", "WARNING", "ERROR"),
        default="INFO",
        help="logging level (default: INFO)",
    )
    return parser.parse_args()


def main() -> int:
    """Open the serial Link and log received WireSpaces packets."""
    arguments = parse_arguments()
    logging.basicConfig(
        level=getattr(logging, arguments.log_level),
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
    )

    try:
        with serial.Serial(
            arguments.port,
            arguments.baud,
            timeout=0.1,
        ) as uart:
            if arguments.reset:
                uart.dtr = False
                time.sleep(0.05)
                uart.dtr = True

            received = receive_packets(
                uart,
                packet_limit=arguments.count,
                timeout_seconds=arguments.timeout,
            )
    except serial.SerialException as error:
        LOGGER.error("serial error: %s", error)
        return 2
    except KeyboardInterrupt:
        return 0

    if arguments.count is not None and received < arguments.count:
        LOGGER.error(
            "received %d of %d requested packets",
            received,
            arguments.count,
        )
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Receive HDLC-framed WireSpaces packets from a serial Link."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import logging
import struct
import sys
import time
from collections.abc import Iterator

import serial


LOGGER = logging.getLogger("wirespaces.receiver")

HDLC_FLAG = 0x7E
HDLC_ESCAPE = 0x7D
HDLC_ESCAPE_XOR = 0x20
HEADER_FORMAT = "<BBBBH"
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)
ENDPOINT_ID_MASK = 0x3FFF
ENDPOINT_NAMESPACE_SHIFT = 14
HDLC_CRC_SIZE = 2

QOS_NAMES = ("CRITICAL", "HIGH", "NORMAL", "BACKGROUND")
NAMESPACE_NAMES = ("USER0", "USER1", "USER2", "COMMON")


@dataclass(frozen=True)
class WireSpacesPacket:
    """Decoded canonical WireSpaces header and payload."""

    control: int
    wire_number: int
    source_participant: int
    destination_participant: int
    endpoint: int
    payload: bytes

    @property
    def qos(self) -> int:
        """Return the two-bit canonical QoS value."""
        return (self.control >> 6) & 0x03

    @property
    def has_extensions(self) -> bool:
        """Return whether canonical header extensions are present."""
        return (self.control & 0x08) != 0

    @property
    def transport_type(self) -> int:
        """Return the three-bit TransportType value."""
        return self.control & 0x07

    @property
    def namespace(self) -> int:
        """Return the Endpoint namespace value."""
        return (self.endpoint >> ENDPOINT_NAMESPACE_SHIFT) & 0x03

    @property
    def endpoint_id(self) -> int:
        """Return the namespace-local Endpoint ID."""
        return self.endpoint & ENDPOINT_ID_MASK

    @classmethod
    def decode(cls, frame: bytes) -> WireSpacesPacket:
        """Decode one unescaped canonical frame."""
        if len(frame) < HEADER_SIZE:
            raise ValueError(
                f"frame has {len(frame)} bytes; canonical header needs {HEADER_SIZE}"
            )

        control, wire, source, destination, endpoint = struct.unpack_from(
            HEADER_FORMAT, frame
        )
        return cls(
            control=control,
            wire_number=wire,
            source_participant=source,
            destination_participant=destination,
            endpoint=endpoint,
            payload=frame[HEADER_SIZE:],
        )

    def encode(self) -> bytes:
        """Encode the canonical header and payload before Link framing."""
        return struct.pack(
            HEADER_FORMAT,
            self.control,
            self.wire_number,
            self.source_participant,
            self.destination_participant,
            self.endpoint,
        ) + self.payload


def crc16_ccitt_false(data: bytes) -> int:
    """Calculate non-reflected CRC-16/CCITT-FALSE."""
    result = 0xFFFF
    for byte in data:
        result ^= byte << 8
        for _ in range(8):
            result = (
                ((result << 1) ^ 0x1021)
                if result & 0x8000
                else result << 1
            ) & 0xFFFF
    return result


def encode_hdlc_frame(frame: bytes) -> bytes:
    """Append little-endian CRC-16 and byte-stuff one flagged HDLC frame."""
    crc = crc16_ccitt_false(frame)
    protected_frame = frame + struct.pack("<H", crc)
    encoded = bytearray((HDLC_FLAG,))
    for byte in protected_frame:
        if byte in (HDLC_FLAG, HDLC_ESCAPE):
            encoded.append(HDLC_ESCAPE)
            encoded.append(byte ^ HDLC_ESCAPE_XOR)
        else:
            encoded.append(byte)
    encoded.append(HDLC_FLAG)
    return bytes(encoded)


class HdlcStreamDecoder:
    """Decode byte-stuffed HDLC frames and discard CRC mismatches."""

    def __init__(self, maximum_frame_size: int = 1024) -> None:
        self._maximum_frame_size = maximum_frame_size
        self._frame = bytearray()
        self._in_frame = False
        self._escaped = False

    def feed(self, data: bytes) -> Iterator[bytes]:
        """Yield CRC-validated frame bodies without their CRC trailers."""
        for byte in data:
            if byte == HDLC_FLAG:
                if self._in_frame and self._frame and not self._escaped:
                    if len(self._frame) > HDLC_CRC_SIZE:
                        frame = bytes(self._frame[:-HDLC_CRC_SIZE])
                        received_crc = int.from_bytes(
                            self._frame[-HDLC_CRC_SIZE:],
                            byteorder="little",
                        )
                        expected_crc = crc16_ccitt_false(frame)
                        if received_crc == expected_crc:
                            yield frame
                        else:
                            LOGGER.warning(
                                "dropping HDLC frame with CRC mismatch: "
                                "received=0x%04X expected=0x%04X",
                                received_crc,
                                expected_crc,
                            )
                self._frame.clear()
                self._in_frame = True
                self._escaped = False
                continue

            if not self._in_frame:
                continue

            if self._escaped:
                self._frame.append(byte ^ HDLC_ESCAPE_XOR)
                self._escaped = False
            elif byte == HDLC_ESCAPE:
                self._escaped = True
            else:
                self._frame.append(byte)

            if len(self._frame) > self._maximum_frame_size + HDLC_CRC_SIZE:
                LOGGER.warning(
                    "dropping HDLC frame larger than %d bytes",
                    self._maximum_frame_size,
                )
                self._frame.clear()
                self._in_frame = False
                self._escaped = False


def format_packet(packet: WireSpacesPacket) -> str:
    """Format a packet as a stable, human-readable log record."""
    qos = QOS_NAMES[packet.qos]
    namespace = NAMESPACE_NAMES[packet.namespace]
    destination = (
        "broadcast"
        if packet.destination_participant == 0xFF
        else str(packet.destination_participant)
    )
    payload = packet.payload.hex(" ") if packet.payload else "-"
    return (
        f"WS packet wire={packet.wire_number} "
        f"src={packet.source_participant} dst={destination} "
        f"qos={qos} transport={packet.transport_type} "
        f"extensions={str(packet.has_extensions).lower()} "
        f"namespace={namespace} endpoint={packet.endpoint_id} "
        f"raw_endpoint=0x{packet.endpoint:04X} "
        f"payload[{len(packet.payload)}]={payload}"
    )


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

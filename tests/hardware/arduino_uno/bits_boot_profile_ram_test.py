#!/usr/bin/env python3
"""Test the receiver-only Compact BITS RAM profile on an Arduino UNO."""

from __future__ import annotations

import argparse
import struct
import sys
import time

import serial

from wirespaces.bits import (
    BITS_TRANSPORT_TYPE,
    CompactBitsTransmitter,
    MESSAGE_ABORT,
    MESSAGE_ACK,
    MESSAGE_REJECT,
    MESSAGE_USER_DATAGRAM,
    BitsProtocolError,
)
from wirespaces.bench.uno import (
    ARDUINO_HOST,
    NORMAL_BITS_CONTROL,
    PC_HOST,
    TEST_WIRE,
    UNO_SEGMENT_SIZE,
    UPLOAD_ENDPOINT,
)
from wirespaces.hdlc import HdlcStreamDecoder, encode_hdlc_frame
from wirespaces.packet import WireSpacesPacket


ECHO_REQUEST = 0x01
ECHO_RESPONSE = 0x81
IMAGE_RESULT = 0x82
IMAGE_OK = 0
NO_BAD_OFFSET = 0xFFFF


def _send_bits_message(uart: serial.Serial, payload: bytes) -> None:
    """Send one Compact BITS PDU to the constrained UNO endpoint."""
    packet = WireSpacesPacket(
        control=NORMAL_BITS_CONTROL,
        wire_number=TEST_WIRE,
        source_host=PC_HOST,
        destination_host=ARDUINO_HOST,
        endpoint=UPLOAD_ENDPOINT,
        payload=payload,
    )
    uart.write(encode_hdlc_frame(packet.encode()))
    uart.flush()


def _is_profile_packet(packet: WireSpacesPacket) -> bool:
    """Return whether a packet belongs to the receiver-only test connection."""
    return (
        packet.transport_type == BITS_TRANSPORT_TYPE
        and packet.wire_number == TEST_WIRE
        and packet.source_host == ARDUINO_HOST
        and packet.destination_host == PC_HOST
        and packet.endpoint == UPLOAD_ENDPOINT
    )


def _read_packets(
    uart: serial.Serial, decoder: HdlcStreamDecoder
) -> list[WireSpacesPacket]:
    """Read one bounded UART chunk and return valid profile packets."""
    packets: list[WireSpacesPacket] = []
    for frame in decoder.feed(uart.read(64)):
        try:
            packet = WireSpacesPacket.decode(frame)
        except ValueError:
            continue
        if _is_profile_packet(packet):
            packets.append(packet)
    return packets


def verify_datagram_echo(
    uart: serial.Serial,
    decoder: HdlcStreamDecoder,
    timeout_seconds: float,
) -> None:
    """Verify unreliable user datagrams in both directions, with retries."""
    request = bytes((ECHO_REQUEST, 0xA5, 0x00, 0x7E, 0x7D, 0xFF))
    expected = bytes((ECHO_RESPONSE,)) + request[1:]
    message = bytes((MESSAGE_USER_DATAGRAM,)) + request
    deadline = time.monotonic() + timeout_seconds
    next_send = 0.0

    while time.monotonic() < deadline:
        now = time.monotonic()
        if now >= next_send:
            _send_bits_message(uart, message)
            next_send = now + 0.25
        for packet in _read_packets(uart, decoder):
            if (
                packet.payload[:1] == bytes((MESSAGE_USER_DATAGRAM,))
                and packet.payload[1:] == expected
            ):
                return
    raise BitsProtocolError("BITS user-datagram echo timed out")


def transfer_pattern(
    uart: serial.Serial,
    decoder: HdlcStreamDecoder,
    object_size: int,
    session_id: int,
    timeout_seconds: float,
) -> None:
    """Transfer data[i] == uint8(i) and verify the UNO completion report."""
    object_bytes = bytes(index & 0xFF for index in range(object_size))
    transmitter = CompactBitsTransmitter(
        object_bytes,
        UNO_SEGMENT_SIZE,
        session_id=session_id,
        initial_sequence=0xFA,
    )
    result: tuple[int, int, int] | None = None
    deadline = time.monotonic() + timeout_seconds

    while time.monotonic() < deadline:
        outbound = transmitter.poll(time.monotonic())
        if outbound is not None:
            _send_bits_message(uart, outbound)
            if transmitter.state == "aborted":
                raise BitsProtocolError("upload retry limit exhausted")

        for packet in _read_packets(uart, decoder):
            payload = packet.payload
            if not payload:
                continue
            message_type = payload[0] & 0x0F
            if message_type in (MESSAGE_ACK, MESSAGE_REJECT, MESSAGE_ABORT):
                transmitter.receive(payload)
                continue
            if message_type != MESSAGE_USER_DATAGRAM or len(payload) != 8:
                continue
            service = payload[1:]
            opcode, reported_session, status, length, first_bad = struct.unpack(
                "<BBBHH", service
            )
            if opcode == IMAGE_RESULT and reported_session == session_id:
                result = status, length, first_bad

        if transmitter.complete and result is not None:
            status, length, first_bad = result
            if status != IMAGE_OK or length != object_size or first_bad != NO_BAD_OFFSET:
                raise BitsProtocolError(
                    "Arduino image check failed: "
                    f"status={status} length={length} first_bad=0x{first_bad:04x}"
                )
            return

    raise BitsProtocolError(
        f"BITS RAM profile timed out: upload={transmitter.state} result={result}"
    )


def parse_arguments() -> argparse.Namespace:
    """Parse serial hardware-test options."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=10.0)
    parser.add_argument(
        "--reset",
        action="store_true",
        help="pulse DTR and wait for the existing UNO bootloader",
    )
    return parser.parse_args()


def main() -> int:
    """Run datagram, full-image, and partial-final-segment checks."""
    arguments = parse_arguments()
    try:
        with serial.Serial(arguments.port, arguments.baud, timeout=0.01) as uart:
            if arguments.reset:
                uart.dtr = False
                time.sleep(0.05)
                uart.dtr = True
                time.sleep(2.0)
                uart.reset_input_buffer()

            decoder = HdlcStreamDecoder()
            verify_datagram_echo(uart, decoder, arguments.timeout)
            transfer_pattern(uart, decoder, 256, 0x51, arguments.timeout)
            transfer_pattern(uart, decoder, 251, 0x52, arguments.timeout)
    except (serial.SerialException, BitsProtocolError) as error:
        print(error, file=sys.stderr)
        return 1

    print(
        "BITS boot-profile RAM test passed: datagrams, 256-byte image, "
        "251-byte partial-final image"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

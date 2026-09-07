"""Serial adapter for the Arduino UNO RAM echo bench firmware."""

from __future__ import annotations

import time

import serial

from ..bits import (
    BITS_TRANSPORT_TYPE,
    MESSAGE_SEGMENT,
    BitsProtocolError,
    CompactBitsReceiver,
    CompactBitsTransmitter,
)
from ..bits.codec import _message_type
from ..hdlc import HdlcStreamDecoder, encode_hdlc_frame
from ..packet import WireSpacesPacket
from .uno import (
    NORMAL_BITS_CONTROL,
    BACKGROUND_BITS_CONTROL,
    TEST_WIRE,
    ARDUINO_HOST,
    PC_HOST,
    UPLOAD_ENDPOINT,
    ECHO_ENDPOINT,
    UNO_MAXIMUM_OBJECT_SIZE,
    UNO_SEGMENT_SIZE,
)


def _send_bits_message(
    uart: serial.Serial,
    endpoint: int,
    payload: bytes,
) -> None:
    """Frame and send one PC-to-UNO Compact BITS protocol message."""
    message_type = _message_type(payload)
    control = (
        BACKGROUND_BITS_CONTROL
        if message_type == MESSAGE_SEGMENT
        else NORMAL_BITS_CONTROL
    )
    packet = WireSpacesPacket(
        control=control,
        wire_number=TEST_WIRE,
        source_host=PC_HOST,
        destination_host=ARDUINO_HOST,
        endpoint=endpoint,
        payload=payload,
    )
    uart.write(encode_hdlc_frame(packet.encode()))
    uart.flush()


def round_trip(
    uart: serial.Serial,
    object_bytes: bytes,
    segment_size: int,
    timeout_seconds: float,
) -> bytes:
    """Upload arbitrary bytes to UNO RAM and receive its BITS echo transfer."""
    transmitter = CompactBitsTransmitter(
        object_bytes,
        segment_size,
        session_id=0x51,
        initial_sequence=0xFA,
    )
    receiver = CompactBitsReceiver(
        maximum_object_size=UNO_MAXIMUM_OBJECT_SIZE,
        maximum_segment_size=UNO_SEGMENT_SIZE,
    )
    decoder = HdlcStreamDecoder()
    deadline = time.monotonic() + timeout_seconds

    while time.monotonic() < deadline:
        now = time.monotonic()
        outbound = transmitter.poll(now)
        if outbound is not None:
            _send_bits_message(uart, UPLOAD_ENDPOINT, outbound)
            if transmitter.state == "aborted":
                raise BitsProtocolError("upload retry limit exhausted")

        for frame in decoder.feed(uart.read(64)):
            try:
                packet = WireSpacesPacket.decode(frame)
            except ValueError:
                continue
            if (
                packet.transport_type != BITS_TRANSPORT_TYPE
                or packet.wire_number != TEST_WIRE
                or packet.source_host != ARDUINO_HOST
                or packet.destination_host != PC_HOST
            ):
                continue

            if packet.endpoint == UPLOAD_ENDPOINT:
                transmitter.receive(packet.payload)
            elif packet.endpoint == ECHO_ENDPOINT:
                response = receiver.receive(packet.payload)
                if response is not None:
                    _send_bits_message(uart, ECHO_ENDPOINT, response)

        if transmitter.complete and receiver.complete:
            return receiver.data

    raise BitsProtocolError(
        f"BITS loopback timed out: upload={transmitter.state} "
        f"echo={'completed' if receiver.complete else 'incomplete'}"
    )

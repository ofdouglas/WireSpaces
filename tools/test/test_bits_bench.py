"""Exercise the UNO serial adapter with a framed RAM echo peer, without hardware."""

import time
import unittest

from wirespaces.bench.bits_ram_transfer import round_trip
from wirespaces.bench.uno import (
    ARDUINO_HOST, PC_HOST, TEST_WIRE, UPLOAD_ENDPOINT, ECHO_ENDPOINT,
    NORMAL_BITS_CONTROL, BACKGROUND_BITS_CONTROL, UNO_SEGMENT_SIZE,
    UNO_MAXIMUM_OBJECT_SIZE,
)
from wirespaces.bits import CompactBitsReceiver, CompactBitsTransmitter, MESSAGE_SEGMENT
from wirespaces.hdlc import HdlcStreamDecoder, encode_hdlc_frame
from wirespaces.packet import WireSpacesPacket


class RamEchoSerial:
    """Simulate the UNO's upload endpoint and independent echo transfer over HDLC."""

    def __init__(self):
        self.decoder = HdlcStreamDecoder()
        self.upload = CompactBitsReceiver(UNO_MAXIMUM_OBJECT_SIZE, UNO_SEGMENT_SIZE)
        self.echo = None
        self.pending = bytearray()
        self.sent = []

    def _queue(self, endpoint, payload):
        packet = WireSpacesPacket(
            NORMAL_BITS_CONTROL, TEST_WIRE, ARDUINO_HOST, PC_HOST, endpoint, payload,
        )
        self.pending.extend(encode_hdlc_frame(packet.encode()))

    def write(self, data):
        for frame in self.decoder.feed(data):
            packet = WireSpacesPacket.decode(frame)
            self.sent.append(packet)
            if packet.endpoint == UPLOAD_ENDPOINT:
                reply = self.upload.receive(packet.payload)
                if reply is not None:
                    self._queue(UPLOAD_ENDPOINT, reply)
                if self.upload.complete and self.echo is None:
                    self.echo = CompactBitsTransmitter(self.upload.data, 17, 0x62, 0xFA)
            elif packet.endpoint == ECHO_ENDPOINT and self.echo is not None:
                self.echo.receive(packet.payload)
        return len(data)

    def flush(self):
        pass

    def read(self, size):
        if self.echo is not None:
            payload = self.echo.poll(time.monotonic())
            if payload is not None:
                self._queue(ECHO_ENDPOINT, payload)
        # Deliberately fragment frames across reads, including escaped bytes and CRC.
        count = min(size, 7)
        data = bytes(self.pending[:count])
        del self.pending[:count]
        return data


class BitsBenchTest(unittest.TestCase):
    def test_round_trip_preserves_data_and_routes_control_and_segments(self):
        uart = RamEchoSerial()
        data = bytes(range(256))
        self.assertEqual(round_trip(uart, data, UNO_SEGMENT_SIZE, 2.0), data)
        self.assertTrue(uart.echo.complete)
        self.assertEqual({packet.endpoint for packet in uart.sent}, {UPLOAD_ENDPOINT, ECHO_ENDPOINT})
        for packet in uart.sent:
            self.assertEqual((packet.wire_number, packet.source_host, packet.destination_host),
                             (TEST_WIRE, PC_HOST, ARDUINO_HOST))
            expected = BACKGROUND_BITS_CONTROL if packet.payload[0] == MESSAGE_SEGMENT else NORMAL_BITS_CONTROL
            self.assertEqual(packet.control, expected)

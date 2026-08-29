"""Unit tests for the serial WireSpaces packet receiver."""

import struct
import unittest

from led_control import parse_brightness
from receiver import (
    HdlcStreamDecoder,
    WireSpacesPacket,
    encode_hdlc_frame,
    format_packet,
)


class LedBrightnessParserTest(unittest.TestCase):
    """Verify 8-bit brightness parsing and aliases."""

    def test_accepts_endpoints_and_aliases(self) -> None:
        self.assertEqual(parse_brightness("off"), 0)
        self.assertEqual(parse_brightness("0"), 0)
        self.assertEqual(parse_brightness("10%"), 26)
        self.assertEqual(parse_brightness("50%"), 128)
        self.assertEqual(parse_brightness("128"), 128)
        self.assertEqual(parse_brightness("0xff"), 255)
        self.assertEqual(parse_brightness("100%"), 255)
        self.assertEqual(parse_brightness("on"), 255)

    def test_rejects_out_of_range_brightness(self) -> None:
        with self.assertRaises(ValueError):
            parse_brightness("256")
        with self.assertRaises(ValueError):
            parse_brightness("101%")


class HdlcStreamDecoderTest(unittest.TestCase):
    """Verify incremental framing and byte unescaping."""

    def test_decodes_frame_split_across_reads(self) -> None:
        decoder = HdlcStreamDecoder()

        self.assertEqual(list(decoder.feed(b"\x7e\x01")), [])
        self.assertEqual(list(decoder.feed(b"\x02\x7e")), [b"\x01\x02"])

    def test_unescapes_reserved_bytes(self) -> None:
        decoder = HdlcStreamDecoder()

        frames = list(decoder.feed(b"\x7e\x7d\x5e\x7d\x5d\x7e"))

        self.assertEqual(frames, [b"\x7e\x7d"])

    def test_encoder_round_trips_reserved_bytes(self) -> None:
        decoder = HdlcStreamDecoder()
        frame = b"\x01\x7e\x7d\x02"

        decoded = list(decoder.feed(encode_hdlc_frame(frame)))

        self.assertEqual(decoded, [frame])


class WireSpacesPacketTest(unittest.TestCase):
    """Verify canonical header decoding and default formatting."""

    def test_decodes_heartbeat_packet(self) -> None:
        frame = struct.pack("<BBBBHI", 0x80, 1, 1, 0xFF, 0xFFFE, 0xC0DEBABE)

        packet = WireSpacesPacket.decode(frame)

        self.assertEqual(packet.qos, 2)
        self.assertFalse(packet.has_extensions)
        self.assertEqual(packet.transport_type, 0)
        self.assertEqual(packet.wire_number, 1)
        self.assertEqual(packet.source_participant, 1)
        self.assertEqual(packet.destination_participant, 0xFF)
        self.assertEqual(packet.namespace, 3)
        self.assertEqual(packet.endpoint_id, 0x3FFE)
        self.assertEqual(packet.payload, b"\xBE\xBA\xDE\xC0")
        self.assertEqual(packet.encode(), frame)

    def test_formats_packet_for_default_logger(self) -> None:
        frame = struct.pack("<BBBBHI", 0x80, 1, 1, 0xFF, 0xFFFE, 0xC0DEBABE)

        text = format_packet(WireSpacesPacket.decode(frame))

        self.assertIn("wire=1", text)
        self.assertIn("dst=broadcast", text)
        self.assertIn("qos=NORMAL", text)
        self.assertIn("namespace=COMMON", text)
        self.assertIn("raw_endpoint=0xFFFE", text)
        self.assertIn("payload[4]=be ba de c0", text)


if __name__ == "__main__":
    unittest.main()

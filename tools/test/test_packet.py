"""Canonical packet encoding and display formatting."""

import unittest

import struct
from wirespaces.packet import WireSpacesPacket, format_packet


class WireSpacesPacketTest(unittest.TestCase):
    """Verify canonical header decoding and default formatting."""

    def test_decodes_heartbeat_packet(self) -> None:
        frame = struct.pack("<BBBBHI", 0x80, 1, 1, 0xFF, 0xFFFE, 0xC0DEBABE)

        packet = WireSpacesPacket.decode(frame)

        self.assertEqual(packet.qos, 2)
        self.assertFalse(packet.has_extensions)
        self.assertEqual(packet.transport_type, 0)
        self.assertEqual(packet.wire_number, 1)
        self.assertEqual(packet.source_host, 1)
        self.assertEqual(packet.destination_host, 0xFF)
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

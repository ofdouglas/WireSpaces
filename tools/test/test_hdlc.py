"""HDLC streaming, escaping, and CRC validation without serial I/O."""

import unittest

from wirespaces.hdlc import HdlcStreamDecoder, crc16_ccitt_false, encode_hdlc_frame


class HdlcStreamDecoderTest(unittest.TestCase):
    """Verify framing, byte unescaping, and CRC acceptance/rejection."""

    def test_decodes_frame_split_across_reads(self) -> None:
        decoder = HdlcStreamDecoder()
        encoded = encode_hdlc_frame(b"\x01\x02")
        split = len(encoded) // 2

        self.assertEqual(list(decoder.feed(encoded[:split])), [])
        self.assertEqual(list(decoder.feed(encoded[split:])), [b"\x01\x02"])

    def test_unescapes_reserved_bytes(self) -> None:
        decoder = HdlcStreamDecoder()

        frames = list(decoder.feed(encode_hdlc_frame(b"\x7e\x7d")))

        self.assertEqual(frames, [b"\x7e\x7d"])

    def test_ccitt_false_known_vector_and_little_endian_trailer(self) -> None:
        self.assertEqual(crc16_ccitt_false(b"123456789"), 0x29B1)
        self.assertEqual(encode_hdlc_frame(b"123456789")[-3:], b"\xb1\x29\x7e")

    def test_encoder_round_trips_reserved_bytes(self) -> None:
        decoder = HdlcStreamDecoder()
        frame = b"\x01\x7e\x7d\x02"

        decoded = list(decoder.feed(encode_hdlc_frame(frame)))

        self.assertEqual(decoded, [frame])

    def test_drops_frame_with_crc_mismatch(self) -> None:
        decoder = HdlcStreamDecoder()
        encoded = bytearray(encode_hdlc_frame(b"\x01\x02"))
        encoded[1] ^= 0x01

        with self.assertLogs("wirespaces.receiver", level="WARNING") as logs:
            decoded = list(decoder.feed(encoded))

        self.assertEqual(decoded, [])
        self.assertIn("CRC mismatch", logs.output[0])


if __name__ == "__main__":
    unittest.main()

"""Streaming HDLC framing and CRC, independent of the underlying byte transport."""

from __future__ import annotations

from collections.abc import Iterator
import logging
import struct

# Preserve the receiver CLI logging category for existing log consumers.
LOGGER = logging.getLogger("wirespaces.receiver")

HDLC_FLAG = 0x7E
HDLC_ESCAPE = 0x7D
HDLC_ESCAPE_XOR = 0x20
HDLC_CRC_SIZE = 2


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

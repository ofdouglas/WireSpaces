"""Tests for the PC-side Compact BITS RAM transfer implementation.

Modules covered: CompactBitsTransmitter, CompactBitsReceiver, Compact wire
messages, retransmission, sequence wrap, and selective ACK state.
Equivalence classes: complete objects, dropped segments, out-of-order segments,
partial final segments, and rejected oversized objects.
"""

import unittest

from wirespaces.bits_ram_transfer import (
    CompactBitsReceiver,
    CompactBitsTransmitter,
    MESSAGE_ABORT,
    MESSAGE_REJECT,
    MESSAGE_SEGMENT,
    REJECT_OBJECT_TOO_LARGE,
    decode_ack,
    encode_segment,
    encode_setup,
)


class CompactBitsRamTransferTest(unittest.TestCase):
    """Exercise both PC protocol roles without a serial device."""

    # A stop-and-wait sender and 16-position RAM receiver exchange a wrapped,
    # multi-segment object including a partial final segment.
    def test_transmitter_and_receiver_complete_object(self) -> None:
        source = bytes((index * 7 + 3) & 0xFF for index in range(53))
        transmitter = CompactBitsTransmitter(
            source,
            segment_size=12,
            session_id=0x31,
            initial_sequence=0xFE,
        )
        receiver = CompactBitsReceiver(
            maximum_object_size=128,
            maximum_segment_size=24,
        )

        now = 0.0
        for _ in range(32):
            message = transmitter.poll(now)
            if message is not None:
                response = receiver.receive(message)
                if response is not None:
                    transmitter.receive(response)
            if transmitter.complete and receiver.complete:
                break
            now += 0.01

        self.assertTrue(transmitter.complete)
        self.assertTrue(receiver.complete)
        self.assertEqual(receiver.data, source)

    # A lost first SEGMENT remains outstanding and is regenerated exactly at
    # the configured retry deadline.
    def test_transmitter_retries_lost_segment(self) -> None:
        transmitter = CompactBitsTransmitter(
            b"abcdefgh",
            segment_size=4,
            session_id=0x32,
            initial_sequence=0x20,
            retry_timeout=0.1,
        )
        receiver = CompactBitsReceiver(32, 8)

        setup = transmitter.poll(0.0)
        self.assertIsNotNone(setup)
        transmitter.receive(receiver.receive(setup))
        segment = transmitter.poll(0.01)
        self.assertEqual(segment[0], MESSAGE_SEGMENT)

        self.assertIsNone(transmitter.poll(0.109))
        self.assertEqual(transmitter.poll(0.11), segment)

    # A delayed duplicate SETUP ACK updates no cumulative state and must not
    # discard the retransmission image for the currently outstanding segment.
    def test_transmitter_retains_segment_across_duplicate_ack(self) -> None:
        transmitter = CompactBitsTransmitter(
            b"abcdefgh",
            segment_size=4,
            session_id=0x36,
            initial_sequence=0x20,
            retry_timeout=0.1,
        )
        receiver = CompactBitsReceiver(32, 8)

        setup = transmitter.poll(0.0)
        setup_ack = receiver.receive(setup)
        transmitter.receive(setup_ack)
        segment = transmitter.poll(0.01)
        transmitter.receive(setup_ack)

        self.assertEqual(transmitter.poll(0.11), segment)

    # Selective ACK bit one records segment 1 before segment 0; receiving the
    # hole then advances the cumulative base across both stored segments.
    def test_receiver_accepts_out_of_order_segments(self) -> None:
        receiver = CompactBitsReceiver(32, 8)
        setup_ack = receiver.receive(encode_setup(0x33, 0xFE, 4, 10))
        self.assertIsNotNone(setup_ack)

        selective_ack = decode_ack(
            receiver.receive(encode_segment(0x33, 1, b"efgh"))
        )
        self.assertEqual(selective_ack.window_bitmap, 0b10)
        self.assertEqual(selective_ack.window_base, 0xFD)

        cumulative_ack = decode_ack(
            receiver.receive(encode_segment(0x33, 0, b"abcd"))
        )
        self.assertEqual(cumulative_ack.window_bitmap, 0)
        self.assertEqual(cumulative_ack.window_base, 0xFF)

        receiver.receive(encode_segment(0x33, 2, b"ij"))
        self.assertTrue(receiver.complete)
        self.assertEqual(receiver.data, b"abcdefghij")

    # SETUP exceeding caller-provided RAM capacity receives an explicit,
    # session-specific ObjectTooLarge REJECT.
    def test_receiver_rejects_object_larger_than_ram(self) -> None:
        receiver = CompactBitsReceiver(16, 8)

        reject = receiver.receive(encode_setup(0x34, 0x10, 8, 17))

        self.assertEqual(reject[0], MESSAGE_REJECT)
        self.assertEqual(reject[1], 0x34)
        self.assertEqual(reject[2], REJECT_OBJECT_TOO_LARGE)

    # Exhausting retries emits a bounded session-specific ABORT instead of
    # retrying forever.
    def test_transmitter_aborts_after_retry_limit(self) -> None:
        transmitter = CompactBitsTransmitter(
            b"abcd",
            segment_size=4,
            session_id=0x35,
            initial_sequence=0x10,
            retry_timeout=0.1,
            maximum_retries=1,
        )

        transmitter.poll(0.0)
        transmitter.poll(0.1)
        abort = transmitter.poll(0.2)

        self.assertEqual(abort, bytes((MESSAGE_ABORT, 0x35)))
        self.assertEqual(transmitter.state, "aborted")


if __name__ == "__main__":
    unittest.main()

#!/usr/bin/env python3
"""Check the demo's UART route/destination rejection, ingress reflection suppression and recovery."""

import argparse
from dataclasses import replace
import struct
import time

import serial

from wirespaces.hdlc import HdlcStreamDecoder, encode_hdlc_frame
from wirespaces.packet import WireSpacesPacket


def exchange(uart, request, timeout, expect_reply):
    decoder = HdlcStreamDecoder()
    deadline = time.monotonic() + timeout
    next_send = 0.0
    found = False
    while time.monotonic() < deadline:
        if time.monotonic() >= next_send and not found:
            uart.write(encode_hdlc_frame(request.encode()))
            uart.flush()
            # Like ping_test, tolerate boot/USB settling and the polling UART's
            # best-effort transport. Repeated invalid requests must still be rejected.
            next_send = time.monotonic() + .25
        for frame in decoder.feed(uart.read(64)):
            packet = WireSpacesPacket.decode(frame)
            if packet.endpoint != request.endpoint:
                continue  # Periodic heartbeat is independent background traffic.
            if packet == request:
                raise RuntimeError("Received packet was reflected onto its ingress UART")
            expected = replace(request, source_host=1, destination_host=2,
                               payload=bytes((0xAB, 2)) + request.payload[2:])
            if packet != expected or not expect_reply:
                raise RuntimeError(f"Unexpected ping packet after {request}: {packet}")
            found = True
    if expect_reply and not found:
        raise RuntimeError("Valid ping was not delivered after ingress rejection probes")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/arduino-uno")
    args = parser.parse_args()
    request = WireSpacesPacket(0x80, 1, 2, 1, 0xFFFD, struct.pack("<BBH", 0xAB, 1, 0x3456))
    with serial.Serial(args.port, 115200, timeout=.05) as uart:
        uart.dtr = False
        time.sleep(.05)
        uart.dtr = True
        time.sleep(1.5)
        uart.reset_input_buffer()
        exchange(uart, request, 3, True)
        exchange(uart, replace(request, wire_number=2), .5, False)
        exchange(uart, replace(request, destination_host=3), .5, False)
        exchange(uart, replace(request, destination_host=255), .5, False)  # Ping intentionally declines broadcast.
        exchange(uart, replace(request, payload=struct.pack("<BBH", 0xAB, 1, 0x4567)), 3, True)
    print("UART ingress test passed: no reflection, unknown Wire/destination rejection, valid ping recovery")


if __name__ == "__main__":
    main()

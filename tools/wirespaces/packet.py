"""Canonical WireSpaces packet encoding, decoding, and display formatting."""

from __future__ import annotations

from dataclasses import dataclass
import struct

HEADER_FORMAT = "<BBBBH"
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)
ENDPOINT_ID_MASK = 0x3FFF
ENDPOINT_NAMESPACE_SHIFT = 14
QOS_NAMES = ("CRITICAL", "HIGH", "NORMAL", "BACKGROUND")
NAMESPACE_NAMES = ("USER0", "USER1", "USER2", "COMMON")

@dataclass(frozen=True)
class WireSpacesPacket:
    """Decoded canonical WireSpaces header and payload."""

    control: int
    wire_number: int
    source_host: int
    destination_host: int
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
            source_host=source,
            destination_host=destination,
            endpoint=endpoint,
            payload=frame[HEADER_SIZE:],
        )

    def encode(self) -> bytes:
        """Encode the canonical header and payload before Link framing."""
        return struct.pack(
            HEADER_FORMAT,
            self.control,
            self.wire_number,
            self.source_host,
            self.destination_host,
            self.endpoint,
        ) + self.payload

def format_packet(packet: WireSpacesPacket) -> str:
    """Format a packet as a stable, human-readable log record."""
    qos = QOS_NAMES[packet.qos]
    namespace = NAMESPACE_NAMES[packet.namespace]
    destination = (
        "broadcast"
        if packet.destination_host == 0xFF
        else str(packet.destination_host)
    )
    payload = packet.payload.hex(" ") if packet.payload else "-"
    return (
        f"WS packet wire={packet.wire_number} "
        f"src={packet.source_host} dst={destination} "
        f"qos={qos} transport={packet.transport_type} "
        f"extensions={str(packet.has_extensions).lower()} "
        f"namespace={namespace} endpoint={packet.endpoint_id} "
        f"raw_endpoint=0x{packet.endpoint:04X} "
        f"payload[{len(packet.payload)}]={payload}"
    )

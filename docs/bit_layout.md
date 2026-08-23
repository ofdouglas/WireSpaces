# WireSpaces — Bit and Byte Layout Conventions

**Status:** Preferred provisional canonical encoding; exact allocation widths are not frozen
**Purpose:** Define the candidate canonical descriptor serialization and the rules independent implementations must follow
**Authority:** Canonical byte and bit packing. Field semantics belong to `CORE`; Link-profile encodings belong to `LINK`

Cross-references use the document code plus a section number, for example `BITS §2`. A bare `§x` always means the current document.

---

# 1. Conventions

**Byte order is little-endian** wherever a representation serializes a literal multi-byte numeric value, unless a profile explicitly specifies otherwise (`PDU-4`).

**Bit order within a byte or word is MSB to LSB.** The first field in a layout listing occupies the most significant bits, and the last occupies the least significant.

**Prefer byte-oriented fields and natural byte alignment.** A constrained Link profile may elide or compress canonical fields, but that profile representation does not redefine the canonical descriptor.

**Priority occupies the most significant available bits.** QoS is the count of strictly higher-priority classes, so Critical is 0 (`CORE §14`). A numerically lower canonical QoS therefore has higher priority.

## 1.1 Language bitfields and native object layout are not encodings

> **A C or C++ bitfield declaration is not a specification of any layout in this document.**

Bit-field allocation order within a storage unit is implementation-defined, as are straddling, padding, and the signedness of a plain `int` field. Structure padding and host byte order are also not serialization rules.

For example, this does not portably encode §2:

```cpp
struct Control {          // not a portable encoding
    uint8_t qos : 2;
    uint8_t reserved : 2;
    uint8_t has_extensions : 1;
    uint8_t transport_type : 3;
};
```

Implementations shall serialize and parse with explicit bytes, shifts, and masks. They shall not transmit or persist a native structure image. Conformance vectors shall exercise both little-endian and big-endian implementation models (`CONFORM §2`).

## 1.2 Reserved fields

A transmitter shall write every reserved bit as zero. A receiver shall reject a descriptor whose reserved bits are nonzero before interpreting extensions, Transport metadata, or payload. Reserved values are not an extension-discovery mechanism.

---

# 2. Preferred provisional canonical descriptor

The preferred ordinary canonical descriptor is **48 bits / 6 bytes**:

```text
byte 0      Control
byte 1      WireNumber
byte 2      SrcParticipantId
byte 3      DestParticipantId
bytes 4..5  Endpoint, little-endian
```

The candidate allocation is:

```text
Control: 8 bits
    QoS                   2   // bits 7..6
    Reserved              2   // bits 5..4, transmit zero; reject nonzero
    HasExtensions         1   // bit  3
    TransportType         3   // bits 2..0

WireNumber                8   // byte 1
SrcParticipantId          8   // byte 2
DestParticipantId         8   // byte 3

Endpoint: 16 bits, serialized little-endian in bytes 4..5
    Namespace             2   // bits 15..14
    Id                   14   // bits 13..0
```

Equivalently:

```text
control =
    ((qos & 0x03) << 6) |
    ((has_extensions & 0x01) << 3) |
    (transport_type & 0x07)

endpoint =
    ((namespace & 0x03) << 14) |
    (endpoint_id & 0x3FFF)

byte[4] = endpoint & 0xFF
byte[5] = (endpoint >> 8) & 0xFF
```

On decode:

```text
qos            = (byte[0] >> 6) & 0x03
reserved       = (byte[0] >> 4) & 0x03
has_extensions = (byte[0] >> 3) & 0x01
transport_type = byte[0] & 0x07

endpoint              = byte[4] | (byte[5] << 8)
namespace             = (endpoint >> 14) & 0x03
endpoint_id           = endpoint & 0x3FFF
```

The reserved-field check from §1.2 applies to `reserved` before further descriptor interpretation.

This byte-oriented form replaces the former `RoutingWord`. Canonical routing is represented explicitly by `WireNumber`, `SrcParticipantId`, and `DestParticipantId`; there is no canonical `NodeId`, `Direction`, `WireAlias`, or `RoutingWord`.

## 2.1 Provisional width status

The six-byte shape is the preferred implementation direction, not a frozen interoperability allocation. In particular, the 8-bit `WireNumber`, 8-bit participant identifiers, and resulting 14-bit Endpoint Id require validation against a representative topology corpus before freeze.

That corpus must include multicore Endpoint Domains, redundant controllers, gateways, multiple constrained buses, overlapping Wires, device-private and debug/platform Wires, local/sentinel reservations, and plausible product growth. The review must record peak consumption, reservation cost, and remaining headroom. Poor headroom reopens the allocation widths; it does not silently introduce aliases or truncation.

---

# 3. Relationship to CAN `PduControl`

The relationship between canonical `Control` and a Classical CAN `PduControl` is **open and provisional**.

The previous mask-and-OR correspondence is no longer valid: canonical bits 5..4 are now reserved, while `Namespace` is part of the 16-bit Endpoint. CAN11 also reconstructs canonical QoS, Wire, and participant identity from the selected Link Binding and CAN identifier rather than necessarily carrying the canonical descriptor verbatim.

`LINK §2` retains reusable PDUA framing and capacity work, but the following require redesign and revalidation together:

- the exact `PduControl` fields and bit positions;
- representation of the 16-bit `Endpoint`;
- optimized N=1 eligibility and byte layout;
- General N=1 metadata size and capacity;
- aggregate CRC placement, protected range, and exact algorithms.

Until that work is complete, no implementation shall infer a CAN `PduControl` layout by copying canonical `Control`, and no document shall claim a byte-exact conversion between them. A finalized CAN profile must provide explicit encode/decode rules and golden vectors.

---

# 4. What Is Still Open

- Exact canonical allocation widths, pending the topology-corpus validation in §2.1.
- Header-extension length encoding and internal field placement (`REG §6.1`).
- CAN `PduControl`, Endpoint packing, optimized and General N=1 details (`LINK §2`).
- CAN aggregate CRC algorithms, protected range, placement, and byte order (`LINK §2`).
- Final CAN29 canonical-field representation.

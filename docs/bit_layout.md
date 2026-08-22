# WireSpaces — Bit and Byte Layout Conventions

**Status:** Settled for the canonical descriptor; profile identifier layouts remain open  
**Purpose:** Fix how fields are ordered within bytes and words, so independent implementations agree  
**Authority:** Layout conventions and canonical descriptor packing. Field widths and meaning belong to `CORE §2`; profile encodings belong to `LINK`

Cross-references use the document code plus a section number, for example `BITS §2`. A bare `§x` always means the current document.

---

# 1. Conventions

**Byte order is little-endian** wherever a representation serializes a literal multi-byte numeric value, unless a profile explicitly specifies otherwise (`PDU-4`).

**Bit order within a byte or word is MSB to LSB.** The first field in a layout listing occupies the most significant bits, the last occupies the least significant.

**Prefer aligning larger bitfields to natural byte boundaries**, and prefer grouping related fields. Where these two preferences conflict, alignment wins — see the `RoutingWord` case in §3.

**Priority occupies the most significant available bits.** QoS is the count of strictly-higher-priority classes, so Critical is 0 (`CORE §14`); placing it at the top means a **numerically lower value always wins**, in the canonical descriptor and in a profile identifier alike. Anyone who inverts it produces a visibly wrong value rather than a subtly inverted priority order.

## 1.1 Language bitfields must not define these layouts

> **A C or C++ bitfield declaration is not a specification of any layout in this document.** Bit-field allocation order within a storage unit is implementation-defined, as is straddling, padding, and the signedness of a plain `int` field.

So this:

```cpp
struct Control {          // does NOT portably mean §2
    uint8_t qos : 2;
    uint8_t namespace_ : 2;
    uint8_t has_header_extensions : 1;
    uint8_t transport_type : 3;
};
```

is not an encoding of the `Control` byte, however much it resembles one. Encode and decode with explicit shifts and masks, and test against golden vectors on both a little-endian and a big-endian implementation model (`CONFORM §2`). This is the concrete form of `PDU-4` and `SVC-7`: native object layout is never a wire representation.

---

# 2. `Control` byte

```text
Control: 8 bits
    QoS                   2   // bits 7..6, MSB of QoS is MSB of Control
    Namespace             2   // bits 5..4
    HasHeaderExtensions   1   // bit  3
    TransportType         3   // bits 2..0, LSB is LSB of Control
```

`TransportType` sits at the bottom because it is checked on every ingress for support (`CORE §19.1`) and `control & 0x07` is the cheapest possible extraction. `HasHeaderExtensions` is a single-bit test at `control & 0x08`, which a parser needs before it can locate the payload.

**`Control` is fully allocated.** Two plus two plus one plus three leaves no reserved bits and no growth room, so `PDU-5`'s reserved-field rule has nothing to enforce here, and any future global flag must go in a header extension (`PDU-6`) rather than into spare space that does not exist.

---

# 3. `RoutingWord`

```text
RoutingWord: 16 bits
    NodeId                5   // bits 15..11
    Direction             1   // bit  10
    WireNumber           10   // bits  9..0
```

This is the case where the alignment preference and the "largest field first" instinct pull in opposite directions, and alignment should win. With `WireNumber` in the low ten bits, little-endian serialization makes the first byte **exactly `WireNumber[7:0]`**, and extraction is a single mask:

```text
wire      = word & 0x03FF
direction = (word >> 10) & 0x01
node_id   = (word >> 11) & 0x1F
```

Placing `WireNumber` at the top instead would split it 2/8 across the byte boundary. That costs nothing computationally, but it leaves no field occupying a whole byte and makes the serialized form harder to read in a trace.

Note that a profile identifier is a separate question with different pressures. The Classical CAN identifier packs `WireAlias` rather than `WireNumber` and is driven by arbitration rather than extraction cost; its layout is still open (`REG §6.8`).

---

# 4. Relationship to CAN `PduControl`

The canonical `Control` byte is **never transmitted verbatim on Classical CAN.** QoS travels in the CAN identifier, and the General PDUA START frame carries a `PduControl` byte for the rest (`LINK §2.5`). Optimized N=1 carries no control byte at all.

The two bytes are deliberately aligned on the six bits they share, differing only in the top two:

```text
Control                       PduControl
    QoS                  2        EndpointId[9:8]      2   bits 7..6
    Namespace            2        Namespace            2   bits 5..4
    HasHeaderExtensions  1        HasHeaderExtensions  1   bit  3
    TransportType        3        TransportType        3   bits 2..0
```

Conversion is therefore a mask and an OR in each direction:

```text
control    = (qos    << 6) | (pdu_control & 0x3F)
pdu_control = (eid_hi << 6) | (control     & 0x3F)
```

The alternative — letting each byte order its fields independently — costs three shifts per PDU in each direction and, more importantly, is easy to get subtly wrong in one direction only. Because the two bytes cannot both appear in one frame, they can never disagree, so no cross-validation rule is needed.

---

# 5. What Is Still Open

- Physical placement and significance order of `Direction`, `WireAlias`, and `NodeId` within the Classical CAN identifier, and which Direction value means `OriginToNode` (`REG §6.8`, `LINK §2.2`).
- Commissioning control space, which must be reserved before that identifier layout freezes (`LINK §2.13`).
- Header extension length encoding and internal field placement (`REG §6.1`).
- Byte order and placement of the CAN aggregate CRC (`REG §6.8`).

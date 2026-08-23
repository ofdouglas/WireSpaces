# WireSpaces — Link Profiles

**Status:** Draft; CAN11 addressing direction selected, PDUA control/CRC details still provisional
**Scope:** How canonical WS PDUs are carried on specific Physical Links
**Authority:** This document owns per-carrier encodings and profile-local reconstruction metadata. Canonical Wire, participant, Endpoint, Transport, and routing semantics belong to `CORE`. A conflict means the profile here is unfinished, not that `CORE` is wrong

---

# 1. Scope and Status

A **Link profile** describes how one Physical Link carries canonical WS PDUs (`CORE §2`), encoding, compressing, or omitting fields per `CORE §2.2`. No profile here is frozen or an interoperability contract (`CONFORM §6`).

Current profile maturity:

| Profile | Status |
|---|---|
| Classical CAN, 11-bit | Three static addressing profiles selected; PDUA details require revalidation |
| Byte-stream / UART | Direction chosen; framing and CRC unresolved |
| CAN FD, CAN XL | In scope; approach undecided |
| USB | Start with CDC/serial; native bulk and FTDI FIFO attractive |
| Ethernet | Two approaches identified; neither developed |
| I2C, SPI | Architecturally placed (`CORE §1.7`); no transaction format |
| Shared memory, FPGA FIFO | Simplest case; minimal profile needed |

---

# 2. Classical CAN — 11-bit Profiles

Classical CAN11 is a constrained compatibility floor, not the definition or ceiling of WireSpaces. It does not carry the preferred 48-bit canonical descriptor literally. The CAN LLL reconstructs a complete canonical descriptor from one statically selected profile, its Link Binding, the complete 11-bit identifier, and the PDU data.

There are three selected CAN11 profiles:

```text
Guest VCN
Native VCN
Native Participant-Compressed
```

They expose two addressing models: VCN names a configured participant relation; Participant-Compressed names participant codes directly. The former committed/`WireAlias`/`NodeId` model is retired.

## 2.1 Link Binding and canonical reconstruction

> **Each CAN11 Link Binding carries exactly one WS Wire.**

This is a property of the Link Binding, not a claim that an entire physical CAN bus equals one Wire. In particular, a Guest binding owns only its allocated identifier block on a legacy-governed bus; frames outside that block may belong to other protocols or bindings.

`WireNumber` is never represented in a baseline CAN11 frame. It is always reconstructed from the binding:

```text
ingress:
    classify frame to exactly one active CAN11 Link Binding
    canonical.WireNumber = binding.WireNumber
    canonical.SrcParticipantId,
    canonical.DestParticipantId = profile identifier reconstruction
    canonical.QoS = identifier field or binding-fixed QoS
    canonical.Endpoint and remaining control = PDUA reconstruction

egress:
    require canonical.WireNumber == binding.WireNumber
    resolve canonical source, destination, and QoS to exactly one CAN ID
    encode Endpoint and remaining control through the binding's PDUA codec
```

Ingress classification must be unique. A frame matching no Guest allocation is non-WS for that binding, not a malformed WS frame. Overlapping active Guest allocations or any other configuration that makes classification ambiguous are invalid.

The addressing profile and its exact version are selected statically in generated configuration. They are never negotiated, inferred from traffic, or auto-detected at runtime. Payload byte 0 may distinguish optimized from General PDUA within that selected profile; it does not select the addressing profile.

## 2.2 Common identifier and arbitration rules

Where a profile carries QoS, canonical QoS occupies the most arbitration-significant identifier bits and is packed unchanged:

| QoS | Class | Arbitration among WS identifiers |
|---:|---|---|
| `0` | Critical | highest |
| `1` | High | next |
| `2` | Normal | next |
| `3` | Background / bulk | lowest |

CAN arbitration is lower-identifier-wins, matching canonical QoS ordering. Fields below QoS also affect arbitration and therefore require visible, deterministic allocation rules.

`Direction` is Link-local reconstruction metadata. It is not part of the canonical descriptor.

## 2.3 VCN semantics

A `VirtualCircuitNumber` is a Link-local configured name for an **unordered participant relation**:

```text
VCN -> {ParticipantA, ParticipantB}
```

For an ordinary pair:

```text
Direction = AToB -> SrcParticipantId = A, DestParticipantId = B
Direction = BToA -> SrcParticipantId = B, DestParticipantId = A
```

The Direction bit uses `0` for `AToB` and `1` for `BToA`. Swapping the stored A/B order without also updating the Direction interpretation is therefore a configuration change.

A VCN names participants, not an Endpoint. Multiple Endpoints and Transports between the same participant pair reuse the same VCN. The Endpoint is reconstructed from PDUA data.

VCN configuration shall satisfy all of the following:

- self-pairs `{A,A}` are prohibited;
- an unordered ordinary pair may appear under at most one ordinary VCN;
- a broadcast relation is stored as `{A,kBroadcast}` and permits only `A -> kBroadcast`; the reverse Direction is invalid for ordinary traffic;
- a participant may have at most one broadcast-source VCN;
- reserved Link-control VCNs are not ordinary mappings;
- egress lookup for `(source, destination)` within a binding is unique.

An egress lookup that finds zero or more than one legal VCN is rejected before any frame is emitted. These rules make canonical reconstruction and reverse egress resolution one-to-one without making VCNs Service-specific.

## 2.4 Guest VCN

Guest VCN is the CAN11 coexistence profile. The physical bus owner shall allocate WireSpaces one **aligned contiguous block of 16 standard identifiers**:

```text
CAN ID bits 10..4   fixed GuestBase prefix
CAN ID bits  3..1   VirtualCircuitNumber[2:0]
CAN ID bit       0  Direction
```

Equivalently:

```text
CAN ID = GuestBase | (VCN << 1) | Direction
GuestBase & 0x00F = 0
allocated range = [GuestBase, GuestBase + 15]
```

The high bits are fixed by the allocation; the low four bits are owned by this profile. The bus owner determines where the block sits relative to legacy arbitration priorities.

Guest VCN carries no per-frame QoS. The binding supplies one fixed canonical QoS. Egress of a canonical PDU with any different QoS shall be rejected before a frame is emitted; QoS is not silently rewritten.

`VCN = 7` (the all-ones VCN) is reserved for future Link control. This leaves ordinary VCN values `0..6`: **7 ordinary configured relations and 14 ordinary directional CAN identifiers**. Both Direction values under the reserved VCN remain non-ordinary.

A frame outside the allocated 16-ID block is non-WS for this binding and shall not enter WS parsing or error handling. A frame inside the block with reserved or semantically invalid VCN/Direction is classified as WS and rejected according to the profile's malformed/reserved-value rules.

Larger Guest blocks may be defined by future, separately named profile versions. They are not runtime options of this 16-ID profile.

## 2.5 Native VCN

Native VCN uses the full relevant CAN11 identifier space:

```text
bits 10..9   QoS                     2
bits  8..1   VirtualCircuitNumber    8
bit       0  Direction               1
--------------------------------------
             total                  11
```

`VCN = 255` (the all-ones VCN) is reserved for future Link control, leaving ordinary VCN values `0..254`: **255 ordinary configured relations**. The reservation applies at every QoS; reserved combinations do not reconstruct ordinary canonical PDUs.

The same VCN relation is used across QoS values. The frame's QoS bits reconstruct canonical QoS, while VCN and Direction reconstruct source and destination according to §2.3. Arbitrary canonical ParticipantIds are representable through the VCN map.

Within a QoS class, VCN numeric allocation affects CAN arbitration. Deployment tooling shall expose that ordering. The exact VCN allocation policy remains open.

## 2.6 Native Participant-Compressed

Native Participant-Compressed uses:

```text
bits 10..9   QoS                     2
bits  8..6   CompactCode             3
bits  5..1   GeneralCode             5
bit       0  Direction               1
--------------------------------------
             total                  11
```

Direction uses:

```text
0 = CompactToGeneral
1 = GeneralToCompact
```

The default is direct canonical ParticipantId mapping:

```text
CompactCode 0..7   -> canonical ParticipantId 0..7
GeneralCode 0..30  -> canonical ParticipantId 0..30
```

A Link Binding may instead enable a Link-local projection from code `0..30` to arbitrary canonical ParticipantIds. Compact codes use the same mapping entries `0..7`. Projection changes only Link representation; it does not create another canonical identity namespace. The active projection shall assign each represented canonical participant one unique code so ingress reconstruction and egress selection remain unambiguous.

Every ordinary unicast pair must have at least one participant represented by a compact code:

- if exactly one endpoint is compact, place it in `CompactCode`, place the other in `GeneralCode`, and set Direction from the canonical source;
- if both endpoints are compact, encode `CompactCode = min(src_code,dest_code)` and `GeneralCode = max(src_code,dest_code)`;
- in the both-compact case use `CompactToGeneral` when `src_code <= dest_code`, otherwise use `GeneralToCompact`;
- a pair in which neither endpoint has a compact code is unrepresentable and is rejected before transmission.

The min/max rule gives a deterministic encoding when both participants fit the compact set. Code ordering is the Link-local code ordering, including when projection is enabled.

`GeneralCode = 31` is reserved:

```text
GeneralCode = 31, Direction = CompactToGeneral
    ordinary broadcast
    SrcParticipantId  = mapping[CompactCode]
    DestParticipantId = kBroadcast

GeneralCode = 31, Direction = GeneralToCompact
    reserved for future CAN11 Link control
    not an ordinary canonical PDU
```

Every ordinary broadcast source must therefore have a compact code. There is no Guest Participant-Compressed profile.

## 2.7 Endpoint representation and PDUA status

All three addressing profiles reuse a common CAN11 PDU adaptation (PDUA) where possible. The identifier reconstructs Wire, participants, and QoS; PDUA reconstructs Endpoint, `HasExtensions`, `TransportType`, and the PDU bytes.

The canonical Endpoint is now a 16-bit `Namespace[2] + Id[14]` value (`BITS §2`). Consequently, the former 10-bit General Endpoint packing and former `PduControl` mask relationship are not valid as committed layouts. Exact `PduControl`, Endpoint packing, optimized N=1, General N=1, and aggregate CRC details are provisional and require joint redesign, capacity recalculation, and golden-vector revalidation.

An exact profile version must state its directly representable Endpoint range. An unrepresentable Endpoint fails placement or TX before any frame is emitted; it is never truncated, implicitly aliased, or substituted.

## 2.8 Optimized N=1 candidate

The retained optimization direction is a one-frame form for small ordinary datagrams:

```text
candidate byte 0
+---+-------------------------+
| 0 | Endpoint Id [6:0]       |
+---+-------------------------+

candidate bytes 1..7
+-----------------------------------------------+
| Service payload, 0..7 bytes                   |
+-----------------------------------------------+
```

Candidate eligibility remains:

```text
Namespace               0
TransportType           UnreliableDatagram
HasExtensions           0
Endpoint Id             1..127
Service payload         0..7 bytes
canonical addressing    uniquely representable by selected CAN11 binding
```

If retained, DLC gives payload length as `DLC - 1`, and native CAN frame integrity is sufficient without an aggregate PDUA CRC. A small N=1-only implementation remains a desired capability.

These byte positions and eligibility details are **not frozen**. They must be revalidated against the redesigned General discriminator and canonical Endpoint/Control model before interoperability use.

The provisional Namespace-0 compact allocation remains:

```text
Id 0          invalid/reserved
Id 1..31      scarce Core/Common optimized allocations
Id 32..127    user/deployment optimized allocations
Id 128..16383 normal Namespace-0 space, not represented by this candidate
```

The `1..31` region is a ceiling to allocate cautiously, not a quota.

## 2.9 General PDUA skeleton

The reusable General PDUA direction is:

```text
START frame:
    byte 0      FrameControl
    following bytes carry provisional PduControl/Endpoint metadata
    remaining bytes begin the PDU stream

Continuation frame:
    byte 0      FrameControl
    bytes 1..7  next 0..7 PDU bytes
```

The preferred provisional `FrameControl` remains:

```text
bit 7       General-PDUA discriminator
bit 6       START
bits 5:3    MessageGeneration[2:0]
bits 2:0    FramesRemaining[2:0]
-----------------------------------
            8 bits
```

`FramesRemaining` values `0..7` represent `N = 1..8` and decrement by one per constituent frame; the final frame carries zero. `MessageGeneration` wraps modulo 8.

The preferred bounds remain:

```text
Maximum PDUA aggregation depth: N = 8 CAN frames
Normal small-node target:       N <= 4
```

Anything routinely requiring more than eight Classical CAN frames should use Transport/Service segmentation or a richer Link profile. Fragmentation multiplies loss (`CONFORM §3`). Critical-QoS PDUA depth should normally remain around `N <= 4`; larger values require explicit bus-load and latency analysis.

The exact bytes following `FrameControl`, including `PduControl`, full Endpoint representation, length derivation, and General N=1 capacity, remain open.

## 2.10 Complete-PDU serialization and reassembly

VCN and Participant-Compressed identifiers do not contain Endpoint. Multiple Endpoints with the same participant relation and QoS therefore share one complete CAN identifier.

> **For each complete CAN identifier, a transmitter shall serialize whole PDUs and shall not interleave constituent frames from different PDUs.**

All constituent frames of one General PDU carry the same complete identifier, are emitted in order, and complete or abort before another PDU using that identifier starts. Frames using different identifiers may interleave through normal CAN arbitration.

The reassembly key is:

```text
(ingress Link Binding/interface, complete 11-bit CAN identifier)
```

There is **at most one active reassembly context per key**, drawn from a fixed global pool. A context holds bounded accumulated bytes, active `MessageGeneration`, expected `FramesRemaining`, exact accumulated length, integrity state, and timeout deadline. Contexts never assemble across identifiers or ingress interfaces. A START with no free context is rejected and counted; there is no dynamic allocation or eviction (`CORE §15.7`).

Reassembly safety relies on:

- explicit START creating or resetting the key's context;
- exact `FramesRemaining` progression;
- matching `MessageGeneration`;
- complete-PDU noninterleaving for the identifier;
- a bounded context lifetime;
- the finalized aggregate PDU integrity check for multi-frame reconstruction.

A START arriving mid-sequence for an active key is a fault and resets or rejects according to the finalized error rule. No partial PDU becomes visible above the LLL.

For every active ordinary identifier mapping, exactly one physical transmitter shall be the reconstructed canonical source. This prevents two controllers from attempting different frame data under the same CAN identifier.

## 2.11 Provisional CRC policy and conditional capacity

The retained candidate aggregate CRC policy is:

```text
N = 1       no aggregate PDUA CRC
N = 2..4    CRC-8
N = 5..8    CRC-16
```

Its purpose is protection of multi-frame composition/reassembly in addition to native CAN frame integrity. The policy, algorithms, parameters, protected range, placement, byte order, DLC/padding interaction, and golden vectors are all provisional and require revalidation after `PduControl` and Endpoint packing are selected.

SAE J1850 remains a CRC-8 candidate. No CRC-16 choice is selected.

The prior General capacity facts are retained only as a **conditional calculation**. If the redesigned START frame still leaves five PDU-stream bytes after `FrameControl` and metadata, then:

```text
B(N) = 5 + 7 * (N - 1) = 7N - 2
```

and, if the candidate CRC schedule above is also retained:

| `N` | Gross `B(N)` | Candidate CRC | Conditional net PDU bytes |
|---:|---:|---|---:|
| 1 | 5 | none | **5** |
| 2 | 12 | CRC-8 | **11** |
| 3 | 19 | CRC-8 | **18** |
| 4 | 26 | CRC-8 | **25** |
| 5 | 33 | CRC-16 | **31** |
| 6 | 40 | CRC-16 | **38** |
| 7 | 47 | CRC-16 | **45** |
| 8 | 54 | CRC-16 | **52** |

These numbers are not promises of the redesigned profile. If START metadata consumes a different number of bytes or the CRC schedule changes, the formula and table shall be replaced. Net PDU capacity must cover header extensions (`H`) and Transport overhead (`T`) before Service payload:

```text
maximum Service bytes = NetPdu(N) - H - T
```

The optimized candidate's 0..7 Service bytes and General N=1's conditional 5 PDU bytes are therefore useful design targets, not frozen facts.

## 2.12 Delivery and flow-control boundary

Classical CAN ACK and controller retransmission are below this adapter and do not constitute WS PDU delivery:

- CAN ACK means at least one controller received the frame, not that an Endpoint received the PDU;
- controller retransmission recovers frame-level errors, not a dropped PDU or exhausted reassembly pool;
- transmitting `N-1` frames then aborting may ACK every transmitted frame while delivering no PDU.

The CAN11 adapter adds no PDU acknowledgment, retry, or duplicate suppression; those belong to a selected Transport (`CORE §20`, `CORE §21.1`).

Generic CAN participants are not expected to implement WS Link-credit flow control. A gateway may still translate CAN queue pressure into reduced upstream credit on richer Links (`CORE §15.3`). Reliable bulk transfer over CAN should use Transport-level receiver control.

## 2.13 Transmit procedure

All rejectable conditions shall be checked before the first frame is emitted:

1. select the statically configured egress Link Binding and require an exact canonical `WireNumber` match;
2. validate canonical reserved bits, participant identities, Endpoint, extensions, `TransportType`, and QoS;
3. reconstruct the selected profile's egress mapping and require exactly one legal complete CAN identifier;
4. for Guest VCN, require canonical QoS to equal the binding's fixed QoS;
5. reject every unrepresentable value with no truncation, aliasing, remapping, or substitution;
6. select optimized or General PDUA according to the finalized mandatory encoding rule;
7. select the smallest legal `N` using finalized net capacity and CRC rules;
8. reserve bounded queue, controller, and buffer capacity for the complete PDU;
9. serialize the complete PDU against other traffic using the same CAN identifier and hold a stable byte/metadata view through completion or abort.

Once START has been emitted, later failure is an aborted partial transmission, not a pre-transmit rejection. It is counted, and receiver state is left to the finalized reset/timeout rule. Local transmit completion is not delivery.

## 2.14 Active-map consistency and update TODO

All participants interpreting the same active CAN11 binding shall use consistent VCN relations, participant projection, reserved values, fixed Guest QoS, Guest range, and exact PDUA profile version. Mixed active maps can reconstruct the same identifier as different canonical sources or destinations and are invalid.

The following mechanism remains an explicit TODO and is not designed here:

- exact configuration fingerprint contents and comparison;
- map/version distribution;
- atomic activation or cutover of a replacement map;
- handling of in-flight TX at activation;
- exact reassembly-context flush timing and signaling.

The eventual activation mechanism must prevent mixed-map operation and must ensure that reassembly begun under one map cannot complete under another. Until that mechanism is specified, consistency is a deployment/configuration invariant, not an on-bus negotiation protocol.

## 2.15 Reserved Link-control space

Space is reserved for future CAN11 Link control:

```text
Guest VCN:                   VCN = 7
Native VCN:                  VCN = 255
Native Participant-Compressed:
    GeneralCode = 31, Direction = GeneralToCompact
```

This reservation keeps future Link control, including possible commissioning use, structurally possible. It does **not** define commissioning payloads, opcodes, identity rules, ownership procedure, retries, persistence, or a state machine. Ordinary PDU decoders shall not interpret reserved Link-control identifiers as canonical Service traffic.

## 2.16 Profile selection and CAN29 escalation

Select one profile statically per CAN11 Link Binding:

```text
legacy-governed bus with an explicit allocated 16-ID block
    -> Guest VCN

WS-native identifier space, arbitrary participant relations,
and a per-relation map is acceptable
    -> Native VCN

WS-native identifier space and every ordinary relation has
at least one participant in the compact code set
    -> Native Participant-Compressed
```

Use CAN29 or another richer Link profile when several WS Wires must share one physical CAN interface without separate suitable bindings, when CAN11 map/configuration limits are awkward, when participant-compressed topology constraints do not fit, or when richer routing identity and payload efficiency justify it. Do not accumulate additional runtime-detected CAN11 modes to avoid that escalation.

## 2.17 Open items (CAN)

- Final `PduControl`, full Endpoint packing, optimized N=1, General N=1, and mandatory encoding-selection rules.
- General START metadata size and resulting capacity table.
- CRC schedule, algorithms and complete parameters, protected range, placement, byte order, DLC/short-frame/padding rules, and golden vectors.
- Controller filter rules for multiple explicit Link Bindings and Guest allocations.
- Native VCN allocation policy and tooling presentation of within-QoS arbitration.
- Exact configuration fingerprint, atomic map activation, and reassembly-flush mechanism (§2.14).
- Link-control and commissioning payload/state design; only identifier space is reserved here.
- Final CAN29 representation and migration guidance.

---

# 3. Byte-Stream / UART

The UART/byte-stream profile is not frozen. Current direction:

```text
WS LLL frame
    -> CRC
    -> byte-stream framing
    -> UART / RS-485 / USB-VCP / similar stream
```

This profile is also the natural starting point for USB CDC (§5) and, with a turnaround convention, for half-duplex RS-485 — which makes it master-initiated in the sense of `CORE §1.7`.

## 3.1 COBS vs HDLC-style escaping

COBS is currently an attractive candidate because it provides tightly bounded framing expansion, which simplifies maximum encoded-frame sizing, fixed buffer allocation, deterministic MCU resource analysis, and RTL implementation.

HDLC-style escaping remains viable and mature, but its worst-case byte-stuffing expansion is less attractive for bounded-resource design.

**This is not yet an agreed/frozen profile choice.** An implementation prototype should compare COBS complexity, resynchronization behavior, DMA friendliness, and encoded-buffer requirements before standardization.

## 3.2 UART CRC

The previous CRC-16/CCITT-FALSE direction is not automatically final merely because it is common. A modern CRC-16 polynomial with strong guaranteed distance over the chosen maximum UART protected-frame length should be considered.

The formal profile must freeze:

- polynomial;
- initial value;
- reflection;
- final XOR;
- protected byte range;
- CRC byte order;
- golden test vectors.

Unlike constrained Classical CAN, one fixed CRC-16 for all UART LLL frame sizes is currently preferred for simplicity.

---

# 4. CAN FD and CAN XL

**CAN FD** can often carry a complete small WS PDU in one frame and greatly reduces the need for PDUA fragmentation. The same canonical PDU and Service definitions apply.

**CAN XL** is especially attractive for large datagrams and high-throughput embedded gateways. Larger frames make destination-owned/zero-copy schemes more attractive (`FUTURE §2`), but routing and Wire semantics are unchanged.

Both may justify richer Link-level flow control than Classical CAN due to larger and faster transfers, though it remains optional (`CORE §15.4`).

**Open:** whether CAN FD/XL share the same PDUA concepts or receive simpler native-PDU profiles.

---

# 5. USB

USB can start with CDC/serial framing, reusing the byte-stream profile in §3.

A future native USB bulk profile, or an FTDI synchronous FIFO profile, is especially attractive for FPGA/host development because it provides a fast PC pipe without requiring a CPU or full network stack in the device (`DEPLOY §3`).

---

# 6. Ethernet

Ethernet has two broad approaches:

```text
WS directly over Ethernet     dedicated Layer-2 embedded network
WS over UDP/IP                existing routed infrastructure
```

WS should not fight IP where IP provides valuable reachability, VPNs, routed networks, Wi-Fi, security infrastructure, or general interoperability. At the same time, a dedicated FPGA/measurement device that only needs to talk to one or two known PCs may benefit from a much simpler WS-over-Ethernet hardware path (`CORE §24`).

Ethernet is the primary case for **aggregation** rather than fragmentation: an Ethernet LLL may pack several small WS PDUs into one transfer (`CORE §12.5`). It is also the strongest candidate for hop-by-hop credit flow control, where the 8-byte QoS-Full credit extension is negligible overhead (`CORE §15.5`).

---

# 7. I2C and SPI

I2C and SPI are architecturally in scope as ordinary Physical Links, and are the main reason `CORE §1.7` exists. Both are **master-initiated**: a non-master Participant's traffic can appear only when the master's LLL polls or otherwise provides transfer cadence, and that polling or autonomous scheduling does not make the master the canonical source or semantic producer.

A profile for either must specify:

- how a PDU is delimited inside a transaction;
- how an idle Node reports "nothing to send", which is a normal outcome and not an error;
- the polling cadence, and whether it is a Link-profile or Wiring property;
- for SPI, framing and a CRC, since SPI has neither native addressing nor native integrity;
- for I2C, how device addressing relates to Link Interface identity.

These are attractive for board-local companion devices and sensor subsystems that would otherwise need a dedicated bus. Note `CORE §4.5`: reaching a chip over SPI does not make it local, and such a peripheral is normally a separate Endpoint Domain on an ordinary network-visible Wire.

**Open:** everything above. Only the architectural placement is settled.

---

# 8. Shared Memory and FPGA FIFO

An inter-core shared-memory queue or an on-chip FIFO is simply another Physical Link with Link Interfaces on its ends (`CORE §13.1`). This is the simplest profile class:

- framing may be trivial, since the carrier already delimits transfers;
- integrity checking is usually unnecessary, since the medium is not lossy;
- fragmentation is usually unnecessary, since the carrier can hold a whole PDU;
- flow control is a strong candidate, because the producer and consumer are tightly coupled.

The concurrency shape matters more than the encoding. If the primitive is SPSC, the choices are one queue per producer, a local serializing task, or an MPSC queue where justified (`CORE §13.3`).

---

# 9. What a Profile Must Specify

A checklist, so a new profile does not silently omit something another profile had to answer:

```text
profile identity/version    static selection unit; incompatible-version behavior
binding and classification  carrier scope owned by one binding; unique ingress match
canonical reconstruction    every transmitted, elided, compressed, or fixed field
Wire reconstruction         direct value or the exact Link-Binding rule
participant representation  ranges, maps, Direction semantics, and uniqueness
Endpoint representation     directly representable range; behavior above it
byte and bit order          field significance and packing, exactly
arbitration/priority        relationship between native ordering and canonical QoS
framing                     delimiting, escaping, padding, and resynchronization
length interpretation       what a length covers, and every minimum and maximum
reserved values             what a transmitter writes and a receiver does
integrity                   CRC or native; polynomial, parameters, protected range,
                            field order, serialized form, residue convention,
                            validation order relative to parsing
fragmentation               whether needed, complete-PDU serialization, and the
                            reassembly key/safety model
aggregation                 whether several PDUs may share one transfer
maximum PDU                 the MTU reported in Link capabilities
QoS profile                 Minimal or Full, fixed/dynamic reconstruction, rejection
flow control                supported or not; extension encoding if so
initiation                  who may transmit when; polling cadence if applicable
idle behavior               how "nothing to send" is expressed
error reporting             which native errors map to which WS categories
transmit ownership          who may drive each native identifier/configured state
coexistence                 whether non-WS traffic can share the medium, and how
map consistency/update      consistency invariant; version/activation/flush behavior
Link-control reservation    reserved carrier space and exclusion from ordinary PDUs
resource bounds             context/queue/buffer limits and exhaustion behavior
golden vectors              encode/decode and malformed/configuration-boundary cases
```

Six rules apply to every profile:

- An unrepresentable canonical value **fails before a frame is emitted**, never by truncation or silent remapping.
- **No partial PDU** may ever become visible above the LLL.
- **The profile and exact version are selected statically per Link Binding** in generated configuration; nothing auto-detects or negotiates framing at runtime.
- **Malformed traffic is counted and dropped, with no response emitted** (`CORE §18.1`). A Link profile does not introduce a protocol-level error reply.
- **Validation precedes parsing.** Framing, length, and integrity are checked before any untrusted PDU field is interpreted.
- **A stabilized profile is immutable.** Incompatible changes take a new name or version.

An LLL implementing a profile also stays inside two boundaries: it **never reinterprets application payload bytes**, and it **never terminates end-to-end Transport state** (`CORE §20`). It may replace hop-local framing and Link-scoped representation freely; that is its job.

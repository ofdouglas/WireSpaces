# WireSpaces — Link Profiles

**Status:** Private first draft; Classical CAN is the most developed profile and is still not byte-exact  
**Scope:** How canonical WS PDUs are carried on specific Physical Links  
**Authority:** This document owns per-carrier encodings only. Wire, Endpoint, Direction, NodeId, Transport, and routing semantics belong to `CORE`. A conflict means the profile here is unfinished, not that `CORE` is wrong

---

# 1. Scope and Status

A **Link profile** describes how one Physical Link carries canonical WS PDUs (`CORE §2`), encoding, compressing, or omitting fields per `CORE §2.2`. No profile here is frozen or an interoperability contract (`CONFORM §6`).

Current profile maturity:

| Profile | Status |
|---|---|
| Classical CAN, 11-bit | Most developed; layouts selected, not byte-exact |
| Byte-stream / UART | Direction chosen; framing and CRC unresolved |
| CAN FD, CAN XL | In scope; approach undecided |
| USB | Start with CDC/serial; native bulk and FTDI FIFO attractive |
| Ethernet | Two approaches identified; neither developed |
| I2C, SPI | Architecturally placed (`CORE §1.7`); no transaction format |
| Shared memory, FPGA FIFO | Simplest case; minimal profile needed |

---

# 2. Classical CAN — 11-bit Profile

Classical CAN remains an important constrained compatibility profile and stress test, but it does not define WireSpaces as a whole.

The goal is not to force every Service to fit one CAN frame. The goal is to make small WS messages genuinely useful on standard 11-bit CAN and to provide a lightweight bounded adaptation when they do not.

The 11-bit identifier is a deliberately constrained **compatibility floor**, not the ceiling of Classical CAN support; the planned escape hatch is `FUTURE §11`.

## 2.1 Committed and Guest profile families

The layout in §2.2 spends all 11 identifier bits on WireSpaces semantics. That is the right choice for a bus WS owns, and it has an unavoidable consequence: **there is no identifier left over for anyone else.** A bus cannot be half-committed. Two families therefore exist, and a Link is statically one or the other.

**Committed CAN** is the developed case and the subject of the rest of §2. Every one of the 11 identifier bits carries WS meaning, and any frame on the bus is a WS frame. There is no per-frame protocol discriminator, and — importantly — **no identifier bit selects the payload encoding**; that is decided by data byte 0 (§2.4). A committed bus carries no legacy CAN traffic.

**Guest CAN** shares a legacy-governed bus; WS occupies an allocated identifier range disjoint from all legacy owners. Routing, framing, Transports, QoS, and capacity are undesigned (`FUTURE §11.2`). Guest needs its own encoding — not a subset reinterpretation of §2.2. Profile family is static per Link (§9 rule 3).

## 2.2 11-bit CAN identifier direction

```text
QoS        2      most arbitration-significant
Direction  1
WireAlias  3
NodeId     5
----------------
           11
```

This gives 4 QoS values, 2 directions, 8 WireAlias codes, and 31 Nodes plus broadcast semantics.

The important semantics:

```text
WireAlias 0      kLocalBus
WireAlias 1..7   configured named-Wire aliases

NodeId 0         broadcast in OriginToNode; invalid in NodeToOrigin
NodeId 1..31     individual Node
```

### QoS occupies the top bits, unchanged

CAN arbitration is lower-identifier-wins, and canonical QoS already counts upward from the highest priority (`CORE §14`). The two agree, so the profile places the QoS value in the **most arbitration-significant** bits and packs it **as-is**:

| QoS | Class | Arbitration |
|---:|---|---|
| `0` | Critical | highest |
| `1` | High | next |
| `2` | Normal | next |
| `3` | Background / bulk | lowest |

Canonical QoS comparison and CAN arbitration are the same comparison — no inversion or lookup table. WireAlias and NodeId carry no arbitration meaning below QoS.

## 2.3 CAN Endpoint representation

Canonical EndpointId remains 16 bits per Namespace.

General PDUA directly represents:

```text
Namespace     0..3
EndpointId    1..1023
```

The optimized tiny single-frame form supports:

```text
Namespace 0
EndpointId 1..127
small UnreliableDatagram payload
```

EndpointIds above 1023 remain valid canonically; the constrained 11-bit Classical CAN profile simply cannot directly represent them without another profile/aliasing mechanism. There is no Endpoint truncation, implicit alias, or fallback: an unrepresentable value must fail placement or TX before any frame is emitted.

## 2.4 Optimized N=1 encoding

For the smallest ordinary WS datagrams:

```text
byte 0
+---+-------------------------+
| 0 | EndpointId [6:0]        |
+---+-------------------------+
  ^ optimized discriminator

bytes 1..7
+-----------------------------------------------+
| Service payload, 0..7 bytes                   |
+-----------------------------------------------+
```

Eligibility:

```text
Namespace               0
TransportType           UnreliableDatagram
HasHeaderExtensions     0
EndpointId              1..127
Service payload         0..7 bytes
routing                 representable by CAN ID
```

CAN DLC gives the actual payload length; the Service payload length is `DLC - 1`. There is no aggregate CRC in this form — native CAN frame integrity is sufficient.

An N=1-only node (optimized form, NS0 EIDs 32–127) is a valid WS device without General PDUA.

### Namespace 0 compact EID allocation

The optimized Classical-CAN N=1 form can directly represent `Namespace 0, EndpointId 1..127`. The current preferred split of that scarce space is:

```text
EID 0         invalid/reserved

EID 1..31     Core/Common FOSS services
              scarce optimized allocations

EID 32..127   user/deployment services
              96 optimized N=1 IDs

EID 128..65535
              normal Namespace-0 user space
              not representable by optimized N=1 CAN
```

The `1..31` Common region should be allocated **slowly and cautiously**. It is a reserved ceiling, not a quota to fill. If the ecosystem eventually needs fewer optimized Common IDs and users need more compact IDs, the boundary may move downward while unallocated IDs remain available.

Broader ecosystem allocation hierarchy for Namespace 3 and compact Common Services is in `FUTURE §8.1`.

## 2.5 General PDUA frame layout

START frame:

```text
byte 0      FrameControl
byte 1      PduControl
byte 2      EndpointId[7:0]
bytes 3..7  first 0..5 PDU bytes
```

Continuation frame:

```text
byte 0      FrameControl
bytes 1..7  next 0..7 PDU bytes
```

`PduControl`:

```text
EndpointId[9:8]         2   // bits 7..6
Namespace               2   // bits 5..4
HasHeaderExtensions     1   // bit  3
TransportType           3   // bits 2..0
--------------------------
                        8
```

`EndpointId[9:8]` and byte 2 form one direct 10-bit value.

Field order matches canonical `Control` except QoS (in CAN ID, §2.2); see `BITS §4` for packing.

Encoding selection on TX is mandatory rather than free:

```text
if optimized eligibility is satisfied:
    use optimized N=1
else if General PDUA can represent the complete canonical PDU within bounds:
    use General PDUA, including N=1 when it fits
else:
    reject placement or local TX before emitting a frame
```

## 2.6 PDUA maximum and normal range

```text
Maximum PDUA aggregation depth: N = 8 CAN frames
Normal design target:           N <= 4
```

Services for broad small-node compatibility should usually fit within `N <= 4` (CRC-8 only, §2.9). `N = 5..8` exists for selective cases such as bootloader/bulk-ish messages. Anything routinely needing more than eight Classical CAN frames should be segmented at the Transport/Service layer (`CORE §20`). Fragmentation multiplies loss (`CONFORM §3`).

Because QoS occupies the top identifier bits (§2.2), each frame of a Critical PDU wins arbitration against essentially everything else on the bus. **Deployment tooling should default Critical-QoS PDUA depth to about `N = 4`** — a Wiring policy, not a protocol maximum (the encoding permits `N = 8` at any QoS). Larger values need explicit justification against bus load, lower-class latency, and reassembly cost; local scheduling cannot fix this (`CORE §14.2`).

## 2.7 FrameControl byte

Reducing MaxN from 16 to 8 frees a third generation bit.

```text
bit 7       General-PDUA discriminator
bit 6       START
bits 5:3    MessageGeneration[2:0]
bits 2:0    FramesRemaining[2:0]
-----------------------------------
            8 bits
```

`FramesRemaining` supports 0..7, corresponding to N = 1..8, and decrements by exactly one per frame so the final frame carries 0. `MessageGeneration` wraps modulo 8.

## 2.8 Reassembly safety model

MessageGeneration is not a globally unique PDU identifier. After wrap, a continuation from a later PDU can theoretically alias an old incomplete reassembly if enough intervening traffic is completely lost. The design relies on several independent checks:

- explicit START resets/creates reassembly state;
- FramesRemaining must match the expected sequence;
- continuation generation must match the active generation;
- constituent frames for one PDU are transmitted in order and non-interleaved for that CAN ID (START mid-sequence is a fault);
- reassembly state has a bounded timeout/lifetime;
- an aggregate PDU CRC validates the completed reconstruction.

This is sufficient for the intended small CAN PDUA without spending more header bits on a larger sequence number.

**Reassembly key:** `(ingress Link, complete CAN identifier)` — **at most one active context per key**, drawn from a **fixed global pool**. A context holds bounded state: accumulated bytes (MaxN capacity), active MessageGeneration, expected FramesRemaining, exact accumulated length, integrity state, and timeout deadline. Different identifiers interleave freely; contexts must never assemble across keys or ingress Links. A START with no free context is **rejected and counted** — no allocation, no eviction (`CORE §15.7`).

Every constituent frame of one PDU carries the same complete CAN arbitration ID, and exactly one physical transmitter owns each CAN ID in every configured state, including commissioning (§2.13).

### Delivery boundary

Classical CAN ACK and controller retransmission are below this adapter and do not constitute WS delivery:

- CAN ACK means at least one controller received the *frame*, not that any Endpoint received the *PDU*;
- controller retransmission recovers frame-level errors, not a dropped PDU, full queue, or rejected authority check;
- transmitting `N-1` frames then aborting ACKs every frame sent and delivers nothing.

No adapter-level PDU acknowledgment, retry, or duplicate suppression — those belong to a selected Transport (`CORE §20`, `CORE §21.1`).

## 2.9 Aggregate CRC policy

```text
N = 1       no aggregate PDUA CRC
N = 2..4    CRC-8
N = 5..8    CRC-16
```

The CRC is primarily additional protection for **multi-frame composition/reassembly**; each constituent CAN frame already has CAN's native frame-level integrity.

A useful consequence:

```text
Small CAN implementation:
    MaxN = 4
    needs only one standardized CRC-8 implementation

Larger CAN implementation:
    MaxN = 8
    adds CRC-16
```

This avoids forcing tiny nodes to implement two aggregate CRC algorithms merely because the full profile supports longer PDUs.

**CRC-8 algorithm:** SAE J1850 is currently implemented and is a viable candidate, but this document does not declare its polynomial/parameters formally frozen.  
**CRC-16 algorithm:** not yet frozen; the profile must choose and specify the full parameter set and golden vectors before any interoperability claim.

## 2.10 General PDUA capacity

Gross bytes available to the PDU stream, before any aggregate CRC, are:

```text
B(N) = 5 + 7 * (N - 1) = 7N - 2
```

which follows from 8 CAN data bytes per frame, minus one FrameControl byte per frame, minus the START frame's PduControl and EndpointId bytes.

Applying the §2.9 CRC policy gives the **net** PDU capacity:

| `N` | CAN frames | Gross `B(N)` | Aggregate CRC | Net PDU bytes |
|---:|---:|---:|---|---:|
| 1 | 1 | 5 | none | **5** |
| 2 | 2 | 12 | CRC-8 (1 B) | **11** |
| 3 | 3 | 19 | CRC-8 (1 B) | **18** |
| 4 | 4 | 26 | CRC-8 (1 B) | **25** |
| 5 | 5 | 33 | CRC-16 (2 B) | **31** |
| 6 | 6 | 40 | CRC-16 (2 B) | **38** |
| 7 | 7 | 47 | CRC-16 (2 B) | **45** |
| 8 | 8 | 54 | CRC-16 (2 B) | **52** |

Net PDU bytes must still cover canonical header extensions (`H`) and the selected transport's overhead (`T`) before Service payload:

```text
maximum Service bytes = NetPdu(N) - H - T
```

For comparison, optimized N=1 carries 0..7 Service bytes with no in-payload WS header — EndpointId shares byte 0 and routing fields live in the CAN ID — buying 2 payload bytes over General N=1.

> **Note:** earlier overviews used gross `B(N)` as usable capacity; use the Net column for N ≥ 2.

## 2.11 CAN and Link-level flow control

Generic CAN Nodes are not expected to participate in WS Link credit flow control.

A gateway can still use CAN queue pressure to reduce upstream Ethernet/UART/other-Link credit, so slow CAN egress can cause backpressure without changing the CAN wire protocol (`CORE §15.3`).

Reliable file/image transfer over CAN should use Transport-level receiver control rather than generic CAN-node LLL credits.

## 2.12 Transmit procedure

The ordering of transmit-side checks is itself a requirement, because several of them are only meaningful *before* the first frame reaches the bus. Once a START has been emitted, a rejection is no longer a rejection — it is a partial PDU somebody has to clean up.

Before emitting any frame, transmit:

1. resolves the Wire and confirms the caller's authority to produce on it;
2. validates the canonical descriptor — Wire representation, Direction, NodeId, Namespace, EndpointId, extensions, TransportType, QoS;
3. applies the encoding selection of §2.5, which is mandatory rather than advisory;
4. **rejects anything unrepresentable, with no aliasing, truncation, or substitution** (`CORE §2.2`);
5. confirms this interface owns the complete CAN identifier in the currently active state (§2.8);
6. selects the **smallest legal `N`**, counting the aggregate CRC the resulting `N` requires;
7. reserves the bounded queue, controller, and buffer capacity the whole PDU will need;
8. holds a stable view of the bytes and metadata until the attempt completes or aborts.

Steps 6–7: choose smallest legal `N` using **net** capacity (§2.10), not gross `B(N)` — e.g. 26 bytes needs `N = 5`, not `N = 4`. Reserve whole-PDU capacity before START; mid-PDU failure aborts, counts, and leaves reassembly to timeout (`CORE §18.1`). Local "complete" ≠ delivery (`CORE §21.1`).

## 2.13 Commissioning control space

`DEPLOY §1.2` describes commissioning generically. Committed CAN has a specific, currently unresolved problem with it, recorded here so the profile is not frozen in a way that makes commissioning impossible.

The difficulty is structural. §2.2 allocates all 11 identifier bits to WS routing semantics, and §2.4 spends both data-byte-0 encodings on ordinary Endpoint traffic. **An unconfigured node has no NodeId, so it cannot form a valid identifier** — yet it has to exchange something to acquire one.

Two rules bound whatever mechanism eventually resolves this:

- **Transmit ownership holds in every phase.** The one-transmitter-per-identifier invariant is a physical arbitration property, and a commissioning exchange that lets two unconfigured nodes answer under one identifier can corrupt frames rather than merely confuse software. This must hold while nodes are being discovered, which is precisely when the configuration that would guarantee it does not exist yet.
- **An unconfigured node emits no ordinary Service traffic.** Before commitment it has no Endpoint authority, so it participates only in commissioning control. It does not publish telemetry, announce Endpoints, or answer ordinary Wire traffic.

The profile must therefore **reserve enough identifier or control space to keep a commissioning exchange feasible** before its layout is frozen. What that reservation costs — one WireAlias code, a reserved NodeId, a distinguished QoS/Direction combination, or something outside the committed allocation — is open, and it is a real cost against an already exhausted field.

Scan algorithms, identity, persistence, and factory reset are out of scope (`FUTURE §11`, `REG §6`).

## 2.14 Open items (CAN)

- Physical placement and significance order of Direction, WireAlias, and NodeId; Direction bit value for `OriginToNode`; controller filter rules (§2.11).
- CRC placement (trailing bytes of final frame assumed above), DLC/short-frame/padding rules, length from DLC vs explicit field.
- CRC-8 polynomial/parameters (SAE J1850 is a candidate, not frozen); CRC-16 algorithm and golden vectors.
- Commissioning identifier/control reservation before §2.2/§2.4 layout is frozen (§2.13).

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

I2C and SPI are architecturally in scope as ordinary Physical Links, and are the main reason `CORE §1.7` exists. Both are **master-initiated**: the master's LLL drives the cadence at which a Node's `NodeToOrigin` traffic can appear, and the LLL may poll or schedule autonomously without becoming the semantic producer.

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
canonical field mapping     which fields go in native metadata vs payload
Wire representation         alias scheme, or direct WireNumber, and its width
Endpoint representation     directly representable range; behavior above it
byte and bit order          field significance and packing, exactly
framing                     delimiting, escaping, padding, and resynchronization
length interpretation       what a length covers, and every minimum and maximum
reserved values             what a transmitter writes and a receiver does
integrity                   CRC or native; polynomial, parameters, protected range,
                            field order, serialized form, residue convention,
                            validation order relative to parsing
fragmentation               whether needed, and the reassembly safety model
aggregation                 whether several PDUs may share one transfer
maximum PDU                 the MTU reported in Link capabilities
QoS profile                 Minimal or Full, and how unsupported QoS behaves
flow control                supported or not; extension encoding if so
initiation                  who may transmit when; polling cadence if applicable
idle behavior               how "nothing to send" is expressed
error reporting             which native errors map to which WS categories
transmit ownership          who may drive the medium, in every configured state
coexistence                 whether non-WS traffic can share the medium, and how
commissioning space         what an unconfigured node may send, and where it fits
resource bounds             context/queue/buffer limits and exhaustion behavior
golden vectors              encode/decode cases for cross-implementation checks
```

Six rules apply to every profile:

- An unrepresentable canonical value **fails before a frame is emitted**, never by truncation or silent remapping.
- **No partial PDU** may ever become visible above the LLL.
- **The profile is selected statically per Link** in generated configuration; nothing auto-detects or negotiates framing at runtime.
- **Malformed or unauthorized traffic is counted and dropped, with no response emitted** (`CORE §18.1`). A Link profile does not introduce a protocol-level error reply.
- **Validation precedes parsing.** Framing, length, and integrity are checked before any untrusted PDU field is interpreted.
- **A stabilized profile is immutable.** Incompatible changes take a new name or version.

An LLL implementing a profile also stays inside two boundaries: it **never reinterprets application payload bytes**, and it **never terminates end-to-end Transport state** (`CORE §20`). It may replace hop-local framing and Link-scoped representation freely; that is its job.

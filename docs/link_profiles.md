# WireSpaces — Link Profiles

**Status:** Private first draft; Classical CAN is the most developed profile and is still not byte-exact  
**Scope:** How canonical WS PDUs are carried on specific Physical Links  
**Authority:** This document owns per-carrier encodings only. Wire, Endpoint, Direction, NodeId, Transport, and routing semantics belong to `CORE`. A conflict means the profile here is unfinished, not that `CORE` is wrong

---

# 1. Scope and Status

A **Link profile** describes how one Physical Link carries canonical WS PDUs (`CORE §2`). A profile may encode canonical fields in native metadata, compress them with aliases, or omit values the profile implies, provided it reconstructs the canonical descriptor before handing a PDU upward (`CORE §2.2`).

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

No profile here is frozen, and none should be treated as an interoperability contract (`CONFORM §4`).

---

# 2. Classical CAN — 11-bit Profile

Classical CAN remains an important constrained compatibility profile and stress test, but it does not define WireSpaces as a whole.

The goal is not to force every Service to fit one CAN frame. The goal is to make small WS messages genuinely useful on standard 11-bit CAN and to provide a lightweight bounded adaptation when they do not.

The 11-bit identifier is a deliberately constrained **compatibility floor**, not the ceiling of Classical CAN support; the planned escape hatch is `FUTURE §11`.

## 2.1 Committed and Guest profile families

The layout in §2.2 spends all 11 identifier bits on WireSpaces semantics. That is the right choice for a bus WS owns, and it has an unavoidable consequence: **there is no identifier left over for anyone else.** A bus cannot be half-committed. Two families therefore exist, and a Link is statically one or the other.

**Committed CAN** is the developed case and the subject of the rest of §2. Every one of the 11 identifier bits carries WS meaning, and any frame on the bus is a WS frame. There is no per-frame protocol discriminator, and — importantly — **no identifier bit selects the payload encoding**; that is decided by data byte 0 (§2.4). A committed bus carries no legacy CAN traffic.

**Guest CAN** is the coexistence case, where WS traffic shares a bus governed primarily by existing raw or legacy CAN. Here WS cannot own the identifier field, so it occupies an **explicitly allocated identifier range that is unambiguous and provably disjoint from every legacy owner on that bus**. The allocation may be contiguous or generated; that, along with Guest CAN's routing budget, payload framing, supported Transports, QoS behavior, and capacity, is undesigned. `FUTURE §11` records it.

Two rules apply now, before any Guest design exists:

- **No committed-CAN field layout is implicitly a Guest-CAN layout.** Guest CAN has fewer bits to work with by construction, so it needs its own encoding rather than a reinterpretation of §2.2.
- **The profile is selected statically per Link**, appears in generated configuration, and participates in compatibility checking. Traffic never auto-detects or negotiates which family it is on. A receiver that had to guess whether an identifier was a WS identifier or a legacy one would have no safe answer, and that guess is exactly what static selection removes.

This is a general Link-profile rule rather than a CAN quirk: no WS Link discovers its own framing at runtime (§9).

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

There is no inversion, no lookup table, and nothing for an LLL to get backwards. A canonical priority comparison and a CAN arbitration outcome are the same comparison.

Placing QoS at the top means a Critical PDU outranks *every* Normal PDU on the bus regardless of Wire or Node, which is the behavior a priority class should have. The cost is that WireAlias and NodeId sit below QoS and therefore carry no arbitration meaning of their own — a busy high-priority Wire cannot be de-prioritized by identifier assignment, only by choosing a lower QoS.

**Still open:** the physical placement and significance order of Direction, WireAlias, and NodeId within the remaining 8 bits, and which numeric value of the Direction bit means `OriginToNode`. Those choices affect arbitration between equal-QoS traffic and controller filter design, and should be fixed together with filtering rules (§2.11).

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

An **N=1-only node is a valid and useful WireSpaces device**. It can still support selected Common Services, up to 96 compact user EIDs in the current NS0 allocation (§2.4), commands/status, identity, health, small telemetry, and simple device control. It does not need General PDUA to be considered a real WS node.

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

The field order is not arbitrary. `PduControl` carries the same three fields as the canonical `Control` byte plus `EndpointId[9:8]` where `Control` carries QoS — QoS itself travels in the CAN identifier (§2.2) and is never duplicated here. Placing each byte's distinct 2-bit field at bits 7..6 makes the remaining **six bits identical in both**, so conversion is one mask and one OR in each direction rather than three shifts (`BITS §4`). Because the two bytes never appear together in one frame, they cannot disagree and no cross-check is required.

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

Services intended for broad small-node compatibility should usually fit within N <= 4. N = 5..8 exists for cases such as bootloader/bulk-ish messages that materially benefit from a larger atomic PDU.

Anything routinely needing more than eight Classical CAN frames should be segmented at the Transport/Service layer rather than making PDUA itself larger (`CORE §20`).

### A separate, stricter limit for Critical QoS

`N <= 4` above is a compatibility target: it keeps a Service usable on small nodes and holds CRC-8 sufficient (§2.9). A different limit exists for a different reason, and the two should not be conflated.

**Deployment tooling should cap PDUA depth for Critical-QoS traffic more aggressively than for other classes, with a default around `N = 4`.** The reason is arbitration rather than memory: because QoS occupies the top identifier bits (§2.2), each frame of a Critical PDU wins arbitration against essentially everything else on the bus. An 8-frame Critical PDU is therefore eight consecutive high-priority arbitration wins, and a burst of them starves Normal and Background traffic for a duration that grows with `N` and does not appear anywhere in the sender's own timing.

This is a Wiring and tooling policy, not a protocol maximum — the encoding permits `N = 8` at any QoS. Configuring a larger value requires explicit justification against bus load, worst-case latency for lower classes, starvation, and reassembly-context cost. The interaction with local scheduling is `CORE §14.2`: a local scheduler cannot fix this, because the frames it is emitting are the ones causing it.

A second reason to keep `N` small on CAN is that fragmentation multiplies loss — see `CONFORM §3`.

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

MessageGeneration is not a globally unique PDU identifier. After wrap, a continuation from a later PDU can theoretically alias an old incomplete reassembly if enough intervening traffic is completely lost.

The design intentionally relies on several independent checks:

- explicit START resets/creates reassembly state;
- FramesRemaining must match the expected sequence;
- continuation generation must match the active generation;
- constituent frames for one PDU are transmitted in order and non-interleaved for that CAN ID;
- reassembly state has a bounded timeout/lifetime;
- an aggregate PDU CRC validates the completed reconstruction.

This is sufficient for the intended small CAN PDUA without spending more header bits on a larger sequence number.

Delivery is all-or-nothing: no Endpoint, Service, queue, or application-visible state may observe a partial PDU. Every constituent frame of one PDU carries the same complete CAN arbitration ID, and exactly one physical transmitter owns each CAN ID in every configured state — including every commissioning state, not only the committed one (§2.13).

### Reassembly context model

The reassembly key is the pair `(ingress Link, complete CAN identifier)`, and the initial profile holds **at most one active context per key**. This is what makes the checks above sufficient: with one context per identifier there is no ambiguity about which partial PDU a continuation belongs to, so no PDU-instance field is needed beyond the generation counter.

A context holds only bounded state:

```text
accumulated bytes (bounded by MaxN capacity)
active MessageGeneration
expected FramesRemaining
exact accumulated length
integrity state
timeout deadline
```

Interleaving follows directly. **Two PDUs never interleave under one CAN identifier** — the sender emits a PDU's frames consecutively for that identifier, and a START arriving mid-sequence is a fault, not a second PDU. Different identifiers interleave freely, since local scheduling and bus arbitration will reorder them; contexts on distinct identifiers are independent and must never assemble across keys or across ingress Links.

Contexts may be drawn from a **fixed global pool** rather than pre-allocated per identifier, which is what makes a many-Wire gateway affordable. The exhaustion rule is then explicit: a START with no free context is **rejected and counted**. It does not allocate, and it does not evict another context — silently evicting an in-progress reassembly would convert local pressure into a phantom loss on an unrelated Wire, which is exactly the kind of invisible failure `CORE §15.7` exists to prevent.

### CAN's own acknowledgment is not delivery

Classical CAN acknowledges frames at the bit level and retransmits on error, and both remain entirely below this adapter. Neither constitutes WS delivery:

- an ACK means at least one controller on the bus received the *frame*, not that any Endpoint received the *PDU*, and not that the intended recipient was even present;
- controller retransmission recovers frame-level errors, not a dropped PDU, a full queue, or a rejected authority check;
- a successful transmission of `N-1` frames followed by a local abort acknowledges every frame sent and delivers nothing.

There is no adapter-level PDU acknowledgment, retry, or duplicate suppression. Those belong to a selected Transport (`CORE §20`), and a Service that needs them must ask for them rather than inferring them from CAN's reputation for reliability.

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

For comparison, optimized N=1 carries 0..7 Service bytes with no in-payload WS header at all, because the EndpointId shares byte 0 and the routing fields live in the CAN ID. Optimized N=1 therefore buys 2 payload bytes over General N=1.

The intended usage pattern is unchanged:

```text
N=1        tiny optimized or general messages
N=2..4     mainstream MCU Service library target
N=5..8     selective use, especially bulk-ish messages
```

> **Note:** an earlier overview published 5 / 12 / 19 / 26 as the per-N budget. Those are the *gross* `B(N)` values and overstate usable capacity for N >= 2 under the current CRC policy. Use the Net column.

**Open:** CRC placement (trailing bytes of the final frame is the assumption above), exact DLC/short-frame/padding rules, and whether length is derived from DLC or carried explicitly.

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

Two consequences of steps 6 and 7 are worth spelling out.

Choosing the smallest legal `N` must account for the CRC width that choice implies, which is not monotone at the boundary: a 26-byte PDU fits `N = 4` gross but needs one CRC byte, so it does not fit, and `N = 5` applies CRC-16 and yields 31 net (§2.10). Compute against the net column, not the gross formula.

Reserving capacity for the whole PDU up front matters because a PDUA transmission has no partial success. If the attempt fails after the START frame — a queue rejection, a Link fault, a controller error — the **whole attempt aborts, its bounded state is released, and a mid-PDU abort is counted** (`CORE §18.1`). The adapter does not retry the PDU and does not emit the remaining frames; the receiver's reassembly context is left to time out. Discovering at frame 5 that there is no room for frame 6 is therefore a design error, not a runtime condition to handle gracefully.

Finally, "complete" on the transmit side means only the implementation's documented local completion point — buffer released, or controller reports the last frame sent. It never means the PDU arrived (`CORE §21.1`).

## 2.13 Commissioning control space

`DEPLOY §1.2` describes commissioning generically. Committed CAN has a specific, currently unresolved problem with it, recorded here so the profile is not frozen in a way that makes commissioning impossible.

The difficulty is structural. §2.2 allocates all 11 identifier bits to WS routing semantics, and §2.4 spends both data-byte-0 encodings on ordinary Endpoint traffic. **An unconfigured node has no NodeId, so it cannot form a valid identifier** — yet it has to exchange something to acquire one.

Two rules bound whatever mechanism eventually resolves this:

- **Transmit ownership holds in every phase.** The one-transmitter-per-identifier invariant is a physical arbitration property, and a commissioning exchange that lets two unconfigured nodes answer under one identifier can corrupt frames rather than merely confuse software. This must hold while nodes are being discovered, which is precisely when the configuration that would guarantee it does not exist yet.
- **An unconfigured node emits no ordinary Service traffic.** Before commitment it has no Endpoint authority, so it participates only in commissioning control. It does not publish telemetry, announce Endpoints, or answer ordinary Wire traffic.

The profile must therefore **reserve enough identifier or control space to keep a commissioning exchange feasible** before its layout is frozen. What that reservation costs — one WireAlias code, a reserved NodeId, a distinguished QoS/Direction combination, or something outside the committed allocation — is open, and it is a real cost against an already exhausted field.

Scan algorithms, commissioning identity, control encodings, persistence, power-loss behavior, factory reset, and physical slot selection are all out of scope here. `FUTURE §11` and `REG §6` carry the open items.

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

A future native USB bulk profile, or an FTDI synchronous FIFO profile, is especially attractive for FPGA/host development because it provides a fast PC pipe without requiring a CPU or full network stack in the device (`DEPLOY §3.2`).

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
integrity                   CRC or native; parameters and protected range
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

Where a profile carries a trailing check value, "integrity" is more than naming an algorithm. It needs the polynomial and parameters, the **protected range**, the **field order**, the **serialized form** of the check value, the **residue convention** if verification is by residue, and the **validation order** relative to parsing. Two implementations agreeing on "CRC-16" and disagreeing on any one of those do not interoperate, and the failure looks like random corruption.

Six rules apply to every profile:

- An unrepresentable canonical value **fails before a frame is emitted**, never by truncation or silent remapping.
- **No partial PDU** may ever become visible above the LLL.
- **The profile is selected statically per Link** and is part of generated configuration and compatibility checking. Nothing auto-detects or negotiates framing at runtime (§2.1).
- **Malformed or unauthorized traffic is counted and dropped, with no response emitted** (`CORE §18.1`). A Link profile does not introduce a protocol-level error reply.
- **Validation precedes parsing.** Framing, length, and integrity are checked before any untrusted PDU field is interpreted — otherwise a length field from a corrupted frame is used to decide how much to read.
- **A stabilized profile is immutable.** Any incompatible change takes a new name or version rather than editing the existing one. Reusing a name for changed behavior removes the only mechanism by which two devices can tell they disagree.

An LLL implementing a profile also stays inside two boundaries: it **never reinterprets application payload bytes**, and it **never terminates end-to-end Transport state** (`CORE §20`). It may replace hop-local framing and Link-scoped representation freely; that is its job.

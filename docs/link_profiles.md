# WireSpaces — Link Profiles

**Status:** Draft; unified CAN11 VCN direction incorporated; exact profile, PDUA, control, CRC, and transition details remain provisional
**Scope:** How canonical WS PDUs are carried on specific Physical Links
**Authority:** This document owns per-carrier encodings and profile-local reconstruction metadata. Canonical Wire, host, Endpoint, Transport, and routing semantics belong to `CORE`. A conflict means the profile here is unfinished, not that `CORE` is wrong

---

# 1. Scope and Status

A **Link profile** describes how one Physical Link carries canonical WS PDUs (`CORE §2`), encoding, compressing, or omitting fields per `CORE §2.2`. No profile here is frozen or an interoperability contract (`CONFORM §6`).

Current profile maturity:

| Profile | Status |
|---|---|
| Classical CAN, 11-bit | Unified VCN model with Guest/Native encodings; packing and transition details provisional |
| Byte-stream / UART | Direction chosen; framing and CRC unresolved |
| CAN FD, CAN XL | In scope; approach undecided |
| USB | Start with CDC/serial; native bulk and FTDI FIFO attractive |
| Ethernet | Two approaches identified; neither developed |
| I2C, SPI | Architecturally placed (`CORE §1.7`); no transaction format |
| Shared memory, FPGA FIFO | Simplest case; minimal profile needed |

---

# 2. Classical CAN — Unified 11-bit VCN Profiles

CAN11 uses one addressing concept: **Virtual Circuit Number (VCN) + Direction**. Guest and Native are statically selected carrier encodings of that concept. This replaces the former Native VCN8 and separate Compact/General Host-compressed baseline. The latter remains an experimental alternative only (`REG §6.8`). Canonical Host identity and Wire propagation are unchanged.

This chapter incorporates the unified VCN proposal as the current design direction. The identifier budgets and default mapping below are preferred provisional profile definitions; no byte-exact interoperability is claimed. PDUA packing, exact control allocation, fingerprints, and migration completion rules still require evidence and freeze.

## 2.1 Link Binding and canonical reconstruction

A Guest Link Binding selects one Wire, one aligned allocated CAN-ID block, one fixed QoS, and a deployment-wide Guest VCN relationship map.

A Native Link Binding contains up to eight active alias bindings:

```text
WireAlias -> {canonical WireNumber, VCN map, profile version/parameters}
```

Each alias selects exactly one Wire and one map. Several aliases may select different Wires or the same Wire with different representation maps. VCN interpretation is scoped by the alias binding, not by canonical `(WireNumber, VCN)`.

Ingress classification is unique: first select the carrier profile/binding, then the Native alias if present, then resolve VCN and Direction. Before Router/dispatch, reconstruct canonical Wire, source Host, destination Host, and QoS. A Guest frame outside its allocated block is non-WS for that binding and never enters its parser or error handling. Overlapping Guest blocks or other ambiguous classifications are invalid. Native operation requires control of the relevant identifier space; it is not inferred from a Guest-looking payload.

A binding's exact profile/version is selected statically. PDUA's optimized/General discriminator never selects an addressing profile.

## 2.2 Common identifier and arbitration rules

Where transmitted, QoS occupies the most arbitration-significant identifier bits unchanged: Critical `0`, High `1`, Normal `2`, Background `3`. Lower CAN identifier wins. Alias and VCN allocation also affect arbitration within a QoS class and must be visible in tooling.

`Direction = 0` means `AToB`; `Direction = 1` means `BToA` for the map's ordered A/B representation. This is a provisional encoding convention, never canonical request/reply or authority semantics. Final identifier positions and control priority must be frozen together with vectors; reserved control is not automatically lowest-priority merely because it is reserved.

## 2.3 VCN semantics

A VCN identifies an unordered Host relationship, stored with a defined A/B order to interpret Direction:

```text
VCN -> {HostA, HostB}
0 -> SrcHostId=A, DestHostId=B
1 -> SrcHostId=B, DestHostId=A
```

A VCN is not an Endpoint. Multiple Endpoints and Transports between the same Hosts reuse it. Ordinary self-pairs are invalid. Within one map, an unordered pair has at most one VCN and a Host has at most one broadcast-source VCN. A broadcast entry is stored `{HostA, kBroadcast}` and permits only `AToB`; the reverse direction never becomes ordinary traffic.

Guest relationship meanings are global within one deployment identity universe: the same Guest VCN resolves to the same ordered Host pair wherever used. Its Wire still comes from the local Guest binding. Different native aliases may have different maps, including aliases on different Links. All devices interpreting one active alias on one bus agree on its full binding.

Egress resolves the canonical tuple to one explicitly configured TX representation. Multiple receive aliases for one Wire do not authorize arbitrary TX choice: configuration selects exactly one active TX alias for each admitted canonical tuple on that interface, then exactly one legal VCN/Direction. An absent, ambiguous, unsupported, or unauthorized selection rejects before emission. No first-match, lowest-alias, observed-traffic selection, or automatic retry under another alias is permitted. QoS and Endpoint/Transport representability remain part of validation. Changing the selected TX alias is an explicit migration operation, not mutation of either alias's meaning (§2.14).

## 2.4 Guest VCN

The bus owner allocates an aligned contiguous block; low bits carry VCN and Direction and high bits are the fixed Guest prefix. Guest-4 is the initial implementation profile:

```text
CAN ID bits 10..4    fixed prefix
CAN ID bits  3..1    VCN[2:0]
CAN ID bit       0  Direction

CAN ID = GuestBase | (VCN << 1) | Direction
GuestBase & 0x00F = 0
allocated range = [GuestBase, GuestBase + 15]
```

Guest-5 and Guest-6 are growth directions using VCN4/5 plus Direction, in aligned blocks of 32/64 IDs. They are separate selected profiles, not runtime width negotiation. Widening exposes more default VCNs without changing the meaning of existing VCN values. Moving or expanding the actual CAN-ID block still requires explicit bus-owner allocation and coordinated cutover.

| Profile | CAN IDs | VCN codes | Default map capacity |
|---|---:|---:|---|
| Guest-4 | 16 | 0..7 | two Main positions and two Node positions |
| Guest-5 (growth) | 32 | 0..15 | two Main positions and six Node positions |
| Guest-6 (growth) | 64 | 0..31 | two Main positions and fourteen Node positions |

Guest has no per-frame QoS. The binding supplies one fixed canonical value; TX with a different QoS rejects, never rewrites. Physical arbitration placement comes from the allocated block. The default map reserves VCN 3; **the all-ones VCN is no longer reserved** and may name an ordinary relationship (§2.6). Exact profile-wide control allocation remains open (§2.15).

## 2.5 Native VCN

Preferred provisional identifier:

```text
bits 10..9  QoS         2
bits  8..6  WireAlias   3
bits  5..1  VCN         5
bit       0 Direction   1
------------------------
                       11
```

There are eight alias values and 32 VCN codes per alias, subject to control reservations. Each alias selects `{WireNumber, VcnMap, profile parameters}`. Multiple aliases may bind the same Wire during migration, but they remain distinct representation contexts. The native interface can carry overlapping Wires without putting a literal WireNumber in each frame.

Initial low-configuration use may define alias 0 as `{kLocalBus, DefaultMap}` and leave other aliases inactive. This does not permanently reserve alias 0 for LocalBus. Rebinding it later still obeys immutability, retirement, and stale-frame exclusion (§2.14); observing traffic cannot promote LocalBus. At most one local Link Interface may carry `kLocalBus` for a Router/Endpoint Domain (`CORE §5.1`). Multiple aliases on that same interface do not create multiple LocalBus Links.

## 2.6 Default mapping and custom mappings

The preferred five-bit default map is:

| VCN | Ordered A/B entry | Ordinary direction |
|---:|---|---|
| 0 | `{MainA, kBroadcast}` | AToB only |
| 1 | `{MainB, kBroadcast}` | AToB only |
| 2 | `{MainA, MainB}` | both |
| 3 | reserved / Link control | none |
| `4 + 2*n` | `{Node[n], MainA}` | both |
| `5 + 2*n` | `{Node[n], MainB}` | both |

For Native and Guest-6, `n = 0..13`; Guest-4 exposes `n = 0..1`, and Guest-5 `n = 0..5`. The A/B order above is explicit so independent implementations agree on Direction. It remains part of the provisional profile encoding.

`MainA`, `MainB`, and `Node[n]` are **profile positions**, not WS Host classes, Origin/Node roles, leadership, or authority. Deployment binds positions to ordinary canonical HostIds; a role map must not assign two positions to the same Host. Unbound positions are inactive and traffic requiring them rejects. A low-ID default Host assignment is not yet selected. This formula handles the common two-central-Host graph without an arbitrary edge table.

A Native alias may instead use a bounded explicit VCN map for arbitrary modest Host relationships, with uniqueness and broadcast rules from §2.3. Custom maps come from the same authoritative deployment source as default-role bindings. Partial inheritance from the default map and a control reservation common to every custom map remain open. Until a profile defines them, an implementation must label its complete custom-map/control policy provisional and must not assume that VCN 3 has a map-independent wire protocol.

Guest maps likewise have one authoritative deployment-wide definition. A custom Guest meaning cannot be authored independently per bus. Whether custom Guest maps are part of the first interoperable Guest profile is open; Guest-4 prototyping starts with the default map.

## 2.7 Endpoint representation and PDUA status

Guest and Native VCN reuse a common CAN11 PDU adaptation (PDUA) where possible. The identifier reconstructs Wire, hosts, and QoS; PDUA reconstructs Endpoint, `HasExtensions`, `TransportType`, and the PDU bytes.

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

VCN identifiers do not contain Endpoint. Multiple Endpoints with the same host relation and QoS therefore share one complete CAN identifier.

> **For each complete CAN identifier, a transmitter shall serialize whole PDUs and shall not interleave constituent frames from different PDUs.**

All constituent frames of one General PDU carry the same complete identifier, are emitted in order, and complete or abort before another PDU using that identifier starts. Frames using different identifiers may interleave through normal CAN arbitration.

The reassembly key is:

```text
(ingress Link Binding/interface, complete 11-bit CAN identifier including alias where present)
```

There is **at most one active reassembly context per key**, drawn from a fixed global pool. A context holds bounded accumulated bytes, active `MessageGeneration`, expected `FramesRemaining`, exact accumulated length, integrity state, and timeout deadline. Contexts never assemble across identifiers, aliases, or ingress interfaces. A context retains the immutable binding that interpreted its START; retirement cannot reinterpret its buffered bytes under a replacement map. A START with no free context is rejected and counted; there is no dynamic allocation or eviction (`CORE §15.7`).

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

Generic CAN hosts are not expected to implement WS Link-credit flow control. A gateway may still translate CAN queue pressure into reduced upstream credit on richer Links (`CORE §15.3`). Reliable bulk transfer over CAN should use Transport-level receiver control.

## 2.13 Transmit procedure

All rejectable conditions are checked before the first frame:

1. Select the statically configured carrier profile and validate canonical scope and fields.
2. For Guest, require the binding's Wire and fixed QoS, then resolve the deployment Guest VCN map.
3. For Native, resolve exactly one configured TX alias for the admitted canonical tuple; require that alias to be active and bound to the PDU's Wire.
4. Resolve exactly one VCN/Direction, respecting Host, broadcast, control, Endpoint, Transport, and extension constraints. No truncation, silent substitution, or fallback alias.
5. Choose optimized or General PDUA and the smallest legal N under the finalized packing and integrity rules.
6. Reserve bounded capacity for the complete PDU; pin its alias/map and byte view through completion or abort.
7. Serialize whole PDUs sharing a complete CAN identifier. A TX-selector update affects new submissions only; already accepted work retains its original representation or is explicitly cancelled.

After START, failure is an aborted partial transmission, counted separately from pre-TX rejection. Local completion is not Endpoint delivery.

## 2.14 Immutable bindings and migration

An active Native alias's Wire, VCN map, profile version, and representation parameters never change in place. One authoritative Wiring source generates every device's slice. Compatibility checking must detect inconsistent active bindings; a mismatch fails closed. Exact fingerprint coverage and distribution remain open.

The update primitive is a spare alias:

1. Prepare the new binding under an unused alias, often for the same canonical Wire.
2. Install and validate it on all affected receivers before enabling any sender to use it.
3. Explicitly change each sender's TX selection. Both receive bindings may coexist; send one representation per PDU on that physical interface, not duplicate copies under both aliases.
4. Stop new submissions under the old alias and complete or cancel its accepted TX work.
5. Retire the old alias only after its RX/reassembly work and queued carrier units can no longer be interpreted as current traffic. Release or discard uncertain state with counters.

Different aliases isolate reassembly, but do not by themselves prove distributed readiness or protect against reuse after retirement. Reusing an alias for a different meaning requires a proven stale-frame exclusion boundary covering controllers, queues, retransmission, and reassembly on affected devices. Until a profile defines that boundary, live reuse is unsupported; use an explicit coordinated stop/drain or discard/reconfigure procedure with traffic disabled and demonstrated exclusion. Do not assume a local reset or an arbitrary wait proves that all peers are clear.

No spare alias means no live spare-alias migration. Retain the active binding, reject the update, or use a coordinated offline reconfiguration. Alias 0 follows the same rules, including transition from its default LocalBus binding.

Guest frames have no alias selector. Their globally coordinated VCN meanings cannot be changed independently on one bus. Guest-map changes, block relocation/expansion, and affected in-flight state require a separately specified coordinated cutover; live map mutation is not defined.

Still open: fingerprint fields, readiness/commit protocol, exact retirement/reuse evidence, drain/flush timing, partial migration recovery, and Guest cutover. These are profile-freeze requirements, not permission to reinterpret active traffic.

## 2.15 Reserved Link-control space

The default VCN map reserves code 3 and has invalid reverse directions on its broadcast entries. These are candidate control encodings, not defined commissioning messages. **All-ones VCNs are ordinary usable values in the new default map.** The former VCN7/255 reservations and Compact/General code31 convention are superseded.

Ordinary decoding never turns a reserved entry or broadcast source into a canonical PDU. Whether VCN3 is permanently reserved across all custom maps, and which invalid directions are assigned to control, must be settled before interoperability freeze. A diagnostic/commissioning path should remain interpretable independently of arbitrary ordinary maps; no such wire protocol is specified here yet.

The anonymous identical-response/prefix-search note is design input, not an accepted profile. Its old NodeId/WireAlias layout and 64-bit manufacturing-identity choice are not imported. Stable device identity, exact complete response bitstream, control priority, opcodes, retries, and commissioning state transitions remain open (`DEPLOY §1.2`, `REG §6.8`).

## 2.16 Profile selection and CAN29 escalation

```text
legacy bus with an allocated identifier block
    -> Guest-4 VCN initially; Guest-5/6 as explicit growth profiles

WS-native identifier space
    -> Native VCN; use the default map when it fits
    -> explicit maps on selected aliases when needed
```

Native CAN11 supports several Wires, bounded by eight aliases and 32 VCN codes per alias before reservations. Reserve alias headroom when live migration is required. Consider CAN29 when relationship count, alias count, spare-alias needs, filters, configuration size, or payload efficiency makes compression unsuitable. Ordinary CAN29 should represent canonical Wire/source/destination directly; its exact layout is not selected here.

Compact/General addressing remains an experimental comparison, not another baseline mode. Reconsider only if measured realistic CAN11 topologies show a material advantage after comparing default/custom VCN cost and CAN29 (`IMPL §7`).

## 2.17 Open items (CAN)

- Final identifier bits, Direction convention, profile names/versions, default-map vectors, and Guest-5/6 standardization.
- Map-independent control allocation, custom-map inheritance, and first-profile custom Guest support.
- Default MainA/MainB/NodeN-to-Host assignment and arbitration-aware custom VCN allocation.
- Final `PduControl`, full Endpoint packing, optimized/General N=1, mandatory encoding selection, and resulting capacity.
- CRC schedule, algorithms/parameters, coverage, placement, byte order, DLC/padding, and golden vectors.
- Controller filters and classification for the selected profile and allocations.
- Fingerprint scope, readiness protocol, TX-selector publication, retirement/reuse, offline transition, and Guest cutover details (§2.14).
- Commissioning identity, anonymous-response safety, payloads, retries, persistence, and state machine.
- Final CAN29 representation and measured crossover; any evidence sufficient to reopen Compact/General.

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

I2C and SPI are architecturally in scope as ordinary Physical Links, and are the main reason `CORE §1.7` exists. Both are **master-initiated**: a non-master Host's traffic can appear only when the master's LLL polls or otherwise provides transfer cadence, and that polling or autonomous scheduling does not make the master the canonical source or semantic producer.

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
host representation  ranges, maps, Direction semantics, and uniqueness
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

# Classical CAN PDU Adapter

**Status:** Experimental / provisional; incompatible change remains possible<br>
**Scope:** Bounded WireSpaces PDU adaptation to 11-bit Classical CAN<br>
**Interoperability:** Not claimed; no immutable CAN wire profile is frozen

This document distinguishes:

- **accepted adapter contract** — architecture invariants every experiment must preserve;
- **current profile direction** — the selected Guest-CAN and committed-CAN design, still subject to byte-exact finalization;
- **open profile details** — behavior that implementations must not fill in silently and call interoperable.

Normative words apply to the accepted adapter contract. The selected layouts are the controlling prototype direction, not a stable wire standard or a C/C++ ABI.

## 1. Relationship to the Consolidated Architecture

Read this document with:

- [Architecture overview](architecture_overview.md);
- [Endpoint Domains, Wires, Routes, and Access](endpoint_domains_wires_routes_and_access.md);
- [Core protocol and routing](core_protocol_and_routing.md);
- [Logical links and transports](logical_links_and_transports.md);
- [Link Engine runtime and status](link_engine_runtime_and_status.md);
- [Prototype and validation](prototype_and_validation.md);
- the [document map and authority guide](README.md).

Those documents control canonical `Control`, `RoutingWord`, `WireBand`, `RoutingCode`, Endpoint, Wire, Route, authority, transport, ownership, and runtime semantics. This adapter owns only its experimental Classical-CAN representation. A conflict leaves this adapter unfinished; it does not override the owning specification.

## 2. Accepted Adapter Contract

### 2.1 Service and bounds

The adapter is a Classical-CAN Logical Link Layer operated by a Link Engine. It carries one complete bounded upper-layer PDU in one or more CAN data frames.

It shall provide:

1. **Unreliable datagram acceptance.** Acceptance never promises arrival.
2. **All-or-nothing receive visibility.** Exactly one complete validated PDU is delivered upward, or nothing is delivered.
3. **No adapter ACK or retry.** PDU acknowledgment, retransmission, duplicate suppression, and end-to-end reliability belong to a selected higher transport.
4. **Static bounds.** PDU length, frame count, active contexts, queues, timers, storage, and diagnostic work are bounded.
5. **Allocation-free feasibility.** Fixed buffers, pools, queues, and context tables are sufficient.

Native CAN acknowledgment and controller retransmission remain below the adapter. They are not PDU-level delivery acknowledgment or retry.

### 2.2 Framing, ownership, and atomic delivery

- Every constituent frame of one PDU shall carry the same complete CAN arbitration ID.
- On each physical CAN Link, exactly one physical transmitter/interface shall own each CAN ID in every configured runtime or commissioning state in which that ID can be emitted. Two devices shall never be authorized to transmit different data under one ID.
- CAN-ID ownership is a physical arbitration-safety invariant. It does not grant Wire source, sink, reply, or forwarding authority.
- No Endpoint, Wire sink, Tap, Relay, Replicator, transport, queue, callback, or application-visible storage may observe or act on a partial PDU.
- A local failure after the first frame stops that attempt, releases bounded state, and records a bounded local abort. The adapter does not automatically retry the PDU.
- A malformed, incomplete, ambiguous, unauthorized, or integrity-failing receive attempt discards its complete affected reassembly context and delivers nothing.

The semantic receive boundary is:

```text
constituent frames
    -> bounded private collection
    -> exact framing/length/integrity validation
    -> complete canonical reconstruction
    -> Wiring/Wire/authority validation
    -> one atomic dispatch, Relay handoff, or Tap copy
```

Framing fields may be inspected to collect bytes, but they shall not cause Endpoint dispatch, Relay forwarding, transport action, or application-visible state mutation before the complete PDU passes the sequence above.

### 2.3 Canonical reconstruction and authority

Every accepted receive PDU shall reconstruct exact canonical values before authority checks or dispatch. Values that the selected profile cannot represent shall fail generated/static placement validation or local TX before any frame is emitted. No value may be truncated, guessed, implicitly aliased, or silently remapped.

Wiring resolves and validates the configured Wire, Route segment, Endpoint binding, ingress, Direction, source, sinks, and permitted transport/QoS/length set. Wiring does not invent canonical packet fields omitted without an explicit profile-defined constant.

Every Wire has exactly one authoritative source Endpoint instance. A valid CAN ID, physical receipt, filter match, routing context, or Endpoint value grants no authority by itself.

### 2.4 Broadcast, Tap, Relay, and Replicator

- Band-0 `PeerId::Broadcast` selects the statically configured sinks of a one-source Wire. It does not mean every electrically visible node and grants no reverse-Wire, reply, or transmit authority.
- A Tap is an explicitly configured passive receive-only copy. It is not a Wire sink or transport peer and gains no ACK, retry, forwarding, flow-control, source, or reply authority.
- A Relay consumes only a complete validated PDU and re-encodes it for the next Link while preserving the resolved Wire, canonical fields, Direction, Endpoint identity, source lineage, transport state, and payload as applicable.
- A Replicator consumes a complete PDU and originates semantically new traffic on a separately authorized Wire. It starts new source lineage and requires explicit policy, bounds, provenance, diagnostics, and cycle prevention.
- Neither a Relay nor a Replicator treats constituent CAN frames as semantic PDUs.

### 2.5 QoS, flow control, and diagnostics

The local scheduler shall preserve canonical QoS ordering and account for CAN's lower-identifier-wins arbitration. It shall avoid preventable local priority inversion when controller capabilities permit priority-aware selection, multiple pending objects, or safe cancellation/replacement. An in-progress CAN frame is non-preemptible, and local scheduling does not guarantee bus-wide fairness or latency.

QoS is not flow control. This baseline has no generic adapter-level receiver-credit mechanism. Static bounds protect receiver resources; a higher transport may define separate receiver-controlled flow control. Local TX acceptance/backpressure and congestion reporting are also separate contracts.

Implementations shall expose bounded local counters or equivalent bounded observations for accepted, rejected, completed, aborted, delivered, and discarded work with stable reason categories. Malformed or unauthorized traffic is counted and dropped without an adapter error response. Counter widths, saturation, reset lifetime, publication, aggregation, and rate limiting remain profile/runtime details.

## 3. Static Classical-CAN Profile Families

The CAN profile is selected statically per Link and included in generated configuration and compatibility checking. Traffic does not auto-detect or negotiate its profile.

### 3.1 Guest CAN

Guest CAN is the high-level profile family for explicitly allocated WireSpaces coexistence on a bus primarily governed by legacy/raw CAN. Its configured CAN-ID allocation must be unambiguous and disjoint from legacy ownership. The allocation may be contiguous or generated according to a later named Guest profile.

Guest CAN is intentionally not frozen here. Its logical routing budget, payload framing, supported transports, QoS behavior, allocation mapping, fingerprints, and conformance variants require separate profile work. No committed-CAN field layout is implicitly a Guest-CAN layout.

### 3.2 Committed WireSpaces CAN

On a committed 11-bit Link, all 11 CAN-ID bits have WireSpaces semantics:

```text
QoS                  2
Direction            1
PathTag[3:0]         4
PeerId[3:0]          4
----------------------
Total                11
```

There is no per-frame raw/protocol discriminator and no CAN-ID bit that selects the payload encoding. Legacy/raw coexistence uses Guest CAN or another separately named statically allocated profile.

The two QoS bits occupy the most arbitration-significant positions. The established canonical-to-CAN mapping is:

| Canonical QoS | CAN arbitration code | Native order |
|---|---:|---|
| `Critical Priority` (`QoS 3`) | `00` | highest |
| `High Priority` (`QoS 2`) | `01` | next |
| `Normal` (`QoS 1`) | `10` | next |
| `Background / Bulk` (`QoS 0`) | `11` | lowest |

This one-to-one meaning and priority order are retained. The exact physical placement/significance of Direction, `PathTag[3:0]`, and `PeerId[3:0]`, and the numeric Direction association, remain candidate details.

Every constituent General-PDUA frame repeats the same complete ID. Static tooling shall prove unique ownership for the complete ID across all configured Wires, QoS values, Directions, profile states, and Link Engines that could emit it.

## 4. Committed Canonical Reconstruction

The committed profile reconstructs Band-0 canonical routing directly:

```text
Control.QoS                 <- committed CAN ID

RoutingWord.WireBand        <- 0
RoutingWord.Reserved        <- 0
RoutingCode.Direction       <- committed CAN ID
RoutingCode.PathTag[7:4]    <- 0
RoutingCode.PathTag[3:0]    <- committed CAN ID
RoutingCode.PeerId[3:0]     <- committed CAN ID
```

Thus committed CAN directly represents Band 0, `PathTag` values `0..15`, and the complete four-bit `PeerId` field. `PeerId 0` is Broadcast and `PeerId 1..15` are individual peers. Canonical `PathTag` validity still applies, including the separate restrictions on `PathTag::Local`.

The remaining canonical fields come from one of the two payload encodings in Section 5. The profile definition supplies the constants above. Wiring then resolves and validates the claimed routing context and Wire; it does not choose a different `WireBand`, fill high `PathTag` bits, substitute a peer, or infer canonical Control fields.

## 5. Exactly Two Normal Committed Data Encodings

Committed CAN has exactly two normal Endpoint/Wire data encodings:

1. optimized N=1;
2. General PDUA with `N >= 1`.

The discriminator is CAN data byte 0. Link commissioning control is below normal Endpoint/Wire traffic and is not a third normal data encoding.

### 5.1 Optimized N=1

```text
CAN byte 0
    bit 7       0
    bits 6:0    EndpointId[6:0]

CAN bytes 1..7
    0..7 Service payload bytes
```

This form reconstructs:

```text
Namespace                 0
TransportType             UnreliableDatagram
QoS                       from committed CAN ID
HasHeaderExtensions       0
EndpointId                1..127
RoutingWord               as defined in Section 4
```

`EndpointId 0` is invalid. DLC carries the actual frame length; at least byte 0 is present, and the Service payload length is `DLC - 1`. There is no CAN-specific Service sideband and no additional PDU FCS in this one-frame candidate.

### 5.2 General PDUA

The START frame is:

```text
byte 0      FrameControl
    bit 7       1
    bit 6       START = 1
    bit 5       MessageGeneration
    bit 4       Reserved
    bits 3:0    FramesRemaining = N - 1

byte 1      PduControl
    Namespace                 2 bits
    TransportType             3 bits
    HasHeaderExtensions       1 bit
    EndpointId[9:8]           2 bits

byte 2      EndpointId[7:0]
bytes 3..7  first 0..5 PDU bytes
```

Each continuation is:

```text
byte 0      FrameControl
    bit 7       1
    bit 6       START = 0
    bit 5       same MessageGeneration
    bit 4       Reserved
    bits 3:0    FramesRemaining

bytes 1..7  next 0..7 PDU bytes
```

The exact internal bit ordering of `PduControl` remains open. `EndpointId[9:8]` and byte 2 form one direct 10-bit value. `EndpointId 0` is invalid; `1..1023` are directly representable.

General PDUA represents all bit patterns of Namespace, `TransportType`, and `HasHeaderExtensions`. Representability does not mean every transport code is specified, implemented, or permitted by a Wire. Unspecified, unsupported, or disallowed transport values fail validation.

The PDU byte stream begins with the canonical header-extension block when indicated, followed by the selected transport PDU. Exact extension semantics remain owned by the core specification.

### 5.3 Mandatory three-way selection

TX selection is:

```text
if optimized eligibility is satisfied:
    use optimized N=1
else if General PDUA can represent the complete canonical PDU
        within the configured bounds:
    use General PDUA, including N=1 when it fits
else:
    reject Wiring placement or local TX before emitting a frame
```

Optimized eligibility requires all of:

- committed Band-0 routing is directly representable;
- `Namespace == 0`;
- `TransportType == UnreliableDatagram`;
- `HasHeaderExtensions == 0`;
- `EndpointId` is `1..127`;
- Service payload length is `0..7`;
- the configured Wire, authority, QoS, bounds, and Link state permit TX.

The optimized form is used if and only if these conditions hold. General N=1 is valid for non-optimized PDUs with up to five General-PDUA bytes. There is no Endpoint alias, truncation, or fallback representation in committed CAN.

## 6. General PDUA Framing and Capacity Direction

For an `N`-frame General PDU:

```text
START:
    START = 1
    FramesRemaining = N - 1

each continuation:
    START = 0
    FramesRemaining decrements by exactly one

final frame:
    FramesRemaining = 0
```

The four-bit countdown gives a candidate maximum of `N = 16`. The initial profile does not interleave two PDUs under one complete CAN ID. Different CAN IDs may interleave through local scheduling and bus arbitration.

The bytes available to the General PDU stream before a trailing PDU FCS are:

```text
B(N) = 5 + 7 * (N - 1)
     = 7N - 2
```

| `N` | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `B(N)` | 5 | 12 | 19 | 26 | 33 | 40 | 47 | 54 |

| `N` | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `B(N)` | 61 | 68 | 75 | 82 | 89 | 96 | 103 | 110 |

For candidate FCS width `F(N)`, canonical extension bytes `H`, and selected transport overhead `T`:

```text
maximum Service bytes = B(N) - F(N) - H - T
```

Native CAN frame integrity is sufficient for optimized N=1 and is the current candidate for General N=1, so `F(1) = 0`. For General `N > 1`, a trailing PDU FCS is required by the selected direction, but its width, algorithm, parameters, coverage, byte order, residue, and serialization remain open. Therefore the tables are gross pre-FCS capacities, not final multi-frame Service maxima.

The approximate default `MaxPduFrames = 4` for Critical PDUA is a Wiring/tooling policy intended to limit repeated high-priority arbitration opportunities. It is not a protocol maximum. Larger configured values require explicit deployment justification and bus-load, latency, starvation, and reassembly-resource analysis.

Exact DLC rules, short-frame rules, padding, length derivation, generation transitions, timeout, malformed continuation handling, reset behavior, and resynchronization remain open. Candidate implementations shall label their choices and shall not claim interoperability.

## 7. Endpoint Allocation and Representability

Canonical `EndpointId` remains 16 bits. The official Namespace-0 allocation is:

```text
EndpointId 0            invalid
EndpointId 1..32        stable public/Common, optimized-capable
EndpointId 33..991      deployment-owned
EndpointId 992..1023    stable public/Common, General-only
EndpointId 1024..65535  canonical, not directly representable by committed 11-bit CAN
```

Namespaces 1 through 3 and their allocation policies remain canonical concerns outside this profile; General PDUA can carry their 10-bit Endpoint values when otherwise supported.

Values above 1023 remain valid canonical identities but fail committed-CAN placement or TX. The profile provides no endpoint alias, truncation, implicit mapping, or fallback. Stable public allocation does not imply that a Service or transport is specified, implemented, or supported on this Link.

## 8. Prototype-Safe Processing Direction

### 8.1 TX

Before emitting any frame, TX:

1. resolves an authorized source Wire and Route segment;
2. validates canonical Control, `RoutingWord`, Endpoint identity, extensions, transport, QoS, Direction, and source authority;
3. applies the mandatory encoding selection in Section 5.3;
4. rejects unrepresentable or unsupported fields without aliasing;
5. proves complete CAN-ID ownership in the active state;
6. selects the smallest legal `N`, including any required trailing FCS;
7. reserves bounded queue, controller, and storage capacity;
8. holds a stable byte/metadata view until completion or local abort.

After START, a local failure aborts the whole attempt and releases bounded state. Completion means only the implementation's documented local completion point, never remote delivery.

### 8.2 RX

The initial implementation keeps at most one active General-PDUA context per ingress Link and complete CAN ID. A context holds bounded storage, generation, expected countdown, exact accumulated length, integrity state, and timeout state.

```text
IDLE + valid START
    -> initialize bounded private context
    -> if FramesRemaining == 0, validate complete N=1 candidate
    -> otherwise COLLECTING

COLLECTING + exact expected continuation
    -> append within bound
    -> decrement expected FramesRemaining

complete candidate
    -> validate exact length and FCS when required
    -> reconstruct every canonical field
    -> validate Wiring, Wire, ingress, Direction, Endpoint, and authority
    -> atomically deliver once, or discard everything
```

Missing START, unexpected START, wrong ID, generation/countdown mismatch, duplicate/skipped/out-of-order frame, timeout, illegal DLC, overflow, malformed metadata, reserved-bit violation, FCS failure, unsupported transport, unrepresentable canonical value, or authority failure produces no partial delivery. Exact replacement-START, reason precedence, timeout, generation, and lifecycle-reset rules remain open.

A fixed global pool may bound contexts across CAN IDs. Pool exhaustion rejects new work with bounded accounting; it does not allocate dynamically or silently evict state.

## 9. Link Commissioning Placement

Pre-addressing bootstrap is Link commissioning control below Endpoint/Wire semantics. Before commitment, an unconfigured node has no ordinary Endpoint authority and emits no ordinary Service traffic.

Only these architectural phases are retained:

```text
Unconfigured
    Link commissioning only

Selected
    exactly one physical instance selected

Staged
    routing, peer, Endpoint, and profile configuration prepared

Committed
    normal Endpoint/Wire semantics enabled
```

The Link profile must reserve enough control space and ownership-safe behavior to keep later commissioning feasible. Any future mechanism shall preserve the one-physical-transmitter-owner-per-CAN-ID invariant in every phase. Scan algorithms, commissioning identity, control encodings, persistence, power-loss behavior, security, factory reset, and physical slot rules are intentionally outside this specification.

## 10. Diagnostics, Security, and Non-Goals

Prototype diagnostics should distinguish, within fixed storage and rate bounds:

- unsupported frame/profile or CAN-ID ownership conflict;
- illegal DLC, length, frame count, metadata, or reserved bit;
- continuation without context, unexpected START, countdown/generation/order fault, or timeout;
- context, queue, buffer, controller, or configured-MTU exhaustion;
- trailing-FCS failure;
- invalid or unsupported canonical fields, transport, extensions, Endpoint, or routing context;
- Wire, ingress, Direction, source/sink, Broadcast, Tap, Relay, or Replicator authority failure;
- local scheduling/acceptance failure and mid-sequence TX abort;
- Link Engine stop, reset, restart, or uncertain-state discard.

Native CAN CRC and the prospective PDU FCS detect accidental corruption only. They provide no authentication, authorization, confidentiality, freshness, replay protection, or proof of source identity. Static CAN-ID ownership is an integration invariant, not a cryptographic control.

This adapter does not provide dynamic discovery, dynamic routing, runtime profile negotiation, adapter-level reliability, generic receiver credit, end-to-end safety/security, arbitrary streams, fragment-level Relay semantics, or remote error responses.

## 11. Validation and Prototype Evidence

The prototype and generated Wiring checks shall cover at least:

| Area | Required evidence |
|---|---|
| Encoding selection | Every optimized eligibility boundary; mandatory optimized use when eligible; General N=1 fallback only when optimized is ineligible; rejection when neither encoding represents the PDU |
| Endpoint boundaries | `0, 1, 32, 33, 127, 128, 991, 992, 1023, 1024, 65535`; exact reconstruction or fail-closed rejection |
| Committed routing | Both Directions; every QoS; `PathTag 0, 1, 15, 16`; `PeerId 0, 1, 15`; exact Band-0 reconstruction with zero high PathTag nibble |
| QoS | Exact `00/01/10/11` Critical-to-Background mapping; local contention and FIFO head-of-line scenarios; documented controller limitations |
| Capacity | Optimized lengths `0..7`; General boundaries at `N=1,2,4,16`; pre-FCS formula; selected experimental FCS overhead; oversize rejection before TX |
| Reassembly faults | Missing, duplicate, swapped, skipped, delayed, replayed, malformed, short, overlong, unexpected-START, reset, and timeout cases; never partial delivery |
| Contexts | Interleaved distinct IDs, attempted same-ID interleaving, full context pool, and no cross-Link/context assembly |
| Ownership | Duplicate CAN-ID owners in each runtime and commissioning state rejected by tooling |
| Canonical validation | Unsupported transport/code point, malformed extension, invalid Endpoint, invalid Local use, wrong Direction, wrong ingress, and unresolved Wire all rejected before dispatch |
| Semantic boundaries | Broadcast recipients, passive Tap, complete-PDU Relay, re-originating Replicator, and physical-only listener |
| Lifecycle/resources | Stop/restart, abort, pool/queue/controller exhaustion, fuzzing, bounded CPU/RAM/time, and bounded diagnostics |
| Independence | Golden and negative vectors consumed by at least two independent implementations before any interoperability claim |

System validation also measures worst-case RAM, execution time, queue occupancy, latency, bus load, scheduler response, loss amplification with `N`, and counter behavior under fault storms.

## 12. Open Profile Issues

The following remain explicitly unresolved:

1. Immutable names/versions and compatibility fingerprints for Guest and committed profiles.
2. Guest-CAN ID allocation, routing, framing, transport, QoS, and conformance definitions.
3. Physical placement/significance of committed Direction, `PathTag`, and `PeerId`, plus numeric Direction orientation.
4. Internal `PduControl` bit ordering and reserved/unsupported code-point handling.
5. Final `FrameControl` reserved-bit rule, candidate maximum `N`, and whether no same-ID interleaving becomes permanent.
6. `MessageGeneration` advancement, wrap, restart, stale-frame, replacement-START, malformed-input, and resynchronization rules.
7. Exact legal DLCs, length derivation, padding, short continuations, timeout behavior, and failure-reason precedence.
8. Required N>1 trailing-FCS width, algorithm, parameters, coverage, serialized order, residue, placement, and vectors.
9. Exact header-extension serialization and preserve/reject behavior under this capacity model.
10. Filtering, scheduler fallback, admission, and timing rules for representative CAN controllers.
11. Diagnostic names, widths, saturation, reset lifetime, publication, aggregation, and rate limits.
12. Commissioning control-space reservation and ownership-safe feasibility, without defining a bootstrap protocol here.
13. Whether separate 29-bit Classical CAN, CAN FD, or richer CAN profiles are standardized.

No implementation may fill these gaps silently and call the result interoperable.

## 13. Stabilization Exit Criteria

This adapter may advance beyond Experimental/provisional only when:

1. a named immutable profile fixes every arbitration-ID, byte, bit, DLC, length, padding, generation, timeout, reset, malformed-input, and state-transition rule;
2. the N>1 integrity contract is byte-exact and justified;
3. generated tooling proves representability, unique CAN-ID ownership in every state, Wire/authority correctness, bounds, and profile compatibility;
4. positive, boundary, and negative vectors cover both encodings, all canonical reconstruction, capacities, faults, and lifecycle behavior;
5. bounded RAM, CPU, queues, timers, and diagnostics are demonstrated on constrained targets;
6. QoS mapping/scheduling and deployment timing analysis are validated;
7. Broadcast, Tap, Relay, Replicator, source-lineage, and commissioning boundaries match the semantic authorities;
8. two independent implementations produce byte-identical output and matching accept/reject results.

Until then, this document makes no interoperability claim.

## 14. Source Lineage

This revision integrates the five archived post-PDF WireSpaces inputs in chronological order:

1. [`wirespaces_post_terminology_architecture_design_update.md`](../drafts/network/wirespaces_post_terminology_architecture_design_update.md) — Wire Space scope, `WireBand`, trailing-integrity direction, and bounded ownership;
2. [`wirespaces_agent_feedback_followup_design_update(1).md`](../drafts/network/wirespaces_agent_feedback_followup_design_update%281%29.md) — set-valued Wiring validation, Guest CAN, static profile selection, and exact-FCS requirements;
3. [`wirespaces_composition_hardware_domain_control_design_update.md`](../drafts/network/wirespaces_composition_hardware_domain_control_design_update.md) — routing-context correction, complete-PDU composition boundaries, and hardware/prototype direction;
4. [`wirespaces_can_bootstrap_local_links_design_update.md`](../drafts/network/wirespaces_can_bootstrap_local_links_design_update.md) — primitive nodes, generated local Wiring projections, profile-family split, and commissioning feasibility;
5. [`wirespaces_committed_can_pdua_change_statement.md`](../drafts/network/wirespaces_committed_can_pdua_change_statement.md) — latest controlling delta for committed 11-bit CAN, the two payload encodings, General START metadata, 10-bit Endpoint representation, allocation fences, FCS direction, and commissioning placement.

The fifth input supersedes broader earlier committed-CAN details wherever they differ. Earlier updates contribute only unsuperseded architecture and rationale. Older CAN/PDUA drafts remain historical provenance through the [document map](README.md); they do not restore discarded layouts, aliases, capacities, or profile variants.

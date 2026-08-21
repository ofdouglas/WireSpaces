# WireSpaces — Architecture Register

**Status:** Authoritative for *status*. The other documents are authoritative for content  
**Purpose:** The control surface — what is settled, what is provisional, what is rejected, what is open  
**Rule:** A change to any WireSpaces document is not finished until it is reflected here

---

# 1. How This Register Is Maintained

Four sections act as the index for the whole document set:

```text
§2   confidence levels
§4   core architectural invariants
§5   explicitly superseded / do-not-reintroduce
§6   open questions
```

If a change to any document does not show up in those four sections, the change is not finished.

Invariants have short stable IDs (`SPLICE-2`, `PDU-1`) rather than positions in a list. Cite the ID, not a number, so that inserting an invariant never invalidates a reference. IDs are never reused: a retired invariant moves to §5 and its ID is retired with it.

`FUTURE` holds undesigned material as prose. This register holds its *status*. Where an item appears in both, the entry here is current and the `FUTURE` section only expands it.

---

# 2. Confidence Levels

## 2.1 Strong / current direction

- Wire = one Origin + zero or more Nodes.
- Canonical 40-bit base descriptor.
- Physical Link / Link Interface / LLL / Router / Endpoint Domain separation.
- Complete canonical PDU as the generic forwarding unit.
- Link-scoped WireAlias and `kLocalBus` semantics.
- Device-private Wire range and the `kLocalDomain` reserved value.
- Wire Splicing as the single sanctioned way a device-private Wire reaches an external Link, applied before egress.
- Shared read-mostly Router state with caller-context routing.
- Copy-based buffer ownership as the first-class baseline; zero-copy deferred.
- Injected Service TX bindings; `Inline` and `Serialized` delivery policies.
- Optional QoS implementation profiles: Minimal and Full.
- Bounded queue policies and congestion as a normal send failure.
- Link telemetry, Link capabilities, and static capacity checking.
- Classical CAN PDUA maximum of 8 frames, with N <= 4 as the normal target.
- Classical CAN aggregate CRC policy: none / CRC-8 / CRC-16 by frame count.
- Namespace 0 compact CAN region and Namespace 3 as the FOSS ecosystem space.
- Physical Wires and Virtual Wires as named cases of one abstraction.
- `kLocalBus` as the compression code for a Link's own physical Wire number.
- Usage maturity levels 0-5, with Levels 0-1 protected from advanced-feature complexity.
- Structural validity always enforced; contract-level policy always optional.
- Promiscuous/bring-up observation as a tooling and gateway capability that never creates Wiring.
- Ephemeral auto-Wiring must announce itself and is never silently authoritative.
- A future 29-bit Classical CAN profile as the planned escape hatch from 11-bit limits.
- Port (local typed interface) versus Endpoint (network-visible termination) as distinct named concepts.
- Transport chosen by Service semantics; reliability is not assumed safer than loss with freshness detection.
- Link independence bounded by declared size, timing, and transport compatibility.
- Master-initiated/polled Links; initiating a transfer never confers producer authority.
- Electrical attachment to a medium confers no membership, delivery, forwarding, or transmit authority.
- Physical proximity does not imply `kLocalDomain` or a device-private Wire.
- Higher-level communication composition must flatten into ordinary Endpoints, Wires, and bindings.
- Privileged capabilities require a separate build; runtime configuration alone cannot enable them.
- One WireSpace is one identity universe; joining two requires an explicit translating gateway.
- QoS is the count of strictly-higher-priority classes: Critical 0 .. Background 3, so lower wins and CAN packs it unchanged.
- One externally visible producer per `(Endpoint Domain, Namespace, EndpointId)`, with declared Endpoint concurrency preserved.
- Endpoint identity provides naming only; Wires, bindings, and typed handles provide authority.
- An Endpoint Domain is a logical dispatch/authority boundary, not a physical node and not a security boundary.
- Direction is structural: it names the producing end, not the interaction pattern.
- Source lineage is preserved by every forwarding mechanism; re-origination starts new lineage and needs its own authority.
- Every delivery path has bounded storage and a chosen exhaustion behavior.
- Malformed or unauthorized traffic is counted and dropped with no response emitted.
- Link profiles are selected statically; nothing auto-detects or negotiates framing.
- Committed versus Guest CAN as distinct profile families, with Guest undesigned.
- Transmit ownership on a shared medium holds in every phase, including commissioning.
- Four-phase commissioning: Unconfigured, Selected, Staged, Committed.
- Little-endian serialization for literal multi-byte numeric values; bit packing still profile-owned.
- Reserved fields are rejected on receive, not ignored.
- A header extension block is self-describing in length, so a parser can skip what it does not know.
- Exactly one binding mode per Endpoint registration; reply authority is never inferred from Direction.
- A Service contract is a schema over bytes; generated language types are views.
- Protocol version, compatibility fingerprint, and build identity are three separate identities.
- Storage classes (snapshot, value queue, ownership queue, event queue) are distinct and never substitute.
- One serialized mutable execution context per LLL instance.
- Hop integrity and end-to-end integrity are different claims; a gateway breaks the hop chain.
- Diagnostic containment: an error report can never trigger another.

## 2.2 Provisional implementation direction

- Exact internal WireNumber range allocation.
- Whether a splice is one bidirectional mapping or two directional route entries.
- Seqlock vs immutable-pointer-swap Router table update mechanics.
- Exact queue-full defaults for each QoS class.
- Exact hop-by-hop credit accounting algorithm.
- Exact Link Telemetry Service schema.
- UART framing and CRC algorithms.
- Registry process details for Namespace 3.
- Scope and build-time removal rules for promiscuous/bring-up mode.
- How a Service declares freshness requirements, and where staleness is detected.
- Whether Port is a generated API concept or only an architectural term.
- I2C and SPI Link profiles, including who drives polling cadence.
- Physical bit placement of Direction, WireAlias, and NodeId within the CAN identifier.
- The Critical-QoS PDUA depth policy and its default value.
- Reassembly context pooling strategy and its bound on a multi-Link gateway.
- The configuration compatibility-check mechanism (coverage, computation, and mismatch behavior).
- Where commissioning control space lives in a fully allocated committed CAN identifier.
- The header-extension length encoding (a 2-bit code giving 1..3 bytes is the working direction).
- Whether an unrecognized extension is preserved on forwarding and rejected on dispatch.
- Schema language and code generation for Service contracts.
- Whether `WeightedFair` accounting is by frames, bytes, or a profile-defined unit.
- The credit quantum and lifeline reservation for each flow-controlled profile.

## 2.3 Deferred

All of these live in `FUTURE`, which is where their current thinking is recorded.

| Deferred item | Expanded in |
|---|---|
| Flow and WireContract semantics | `FUTURE §5` |
| Full Wiring Manifest schema and static analysis models | `FUTURE §4` |
| General security architecture | — (`CORE §22` is the position) |
| Automatic Origin failover/election | — (`CORE §20.1` is the boundary) |
| Cyclic/redundant forwarding profiles | `FUTURE §6` |
| General adaptive congestion-control protocol | — (`CORE §15.6` is the boundary) |
| Zero-copy ownership APIs | `FUTURE §2` |
| Sequenced / end-to-end-protected datagram | `FUTURE §3.1` |
| Reliable and bulk Transports | `FUTURE §3.2` |
| A reusable communication-component catalog | `FUTURE §13` |
| Domain Control as a group-management layer | `FUTURE §14` |
| Cross-WireSpace identity translation | `FUTURE §12` |
| Remote maintenance | `FUTURE §9` |
| 29-bit Classical CAN profile | `FUTURE §11` |
| Namespace 3 registry process | `FUTURE §7` |
| Broader ecosystem Service catalog | `FUTURE §8` |

---

# 3. Capacity / Fact Sheet

| Characteristic | Current direction |
|---|---|
| Canonical message type | bounded datagram |
| Canonical base descriptor | 40 bits / 5 bytes |
| Canonical header incl. ordinary extensions | ~8 bytes target: 5 base + up to 3 extension |
| Literal multi-byte serialization | little-endian; bit packing is profile-owned |
| Header extension length | self-describing; 2-bit code for 1..3 bytes (provisional) |
| Reserved fields | zero on TX, dropped and counted on RX |
| Namespace | 2 bits / 4 spaces |
| EndpointId | 16 bits per Namespace; 0 invalid |
| FOSS ecosystem Namespace | Namespace 3 |
| TransportType | 3 bits / up to 8 values |
| QoS | 2 bits / 4 classes: Critical 0, High 1, Normal 2, Background 3 |
| QoS profiles | QoS-Minimal (Normal only), QoS-Full (all four) |
| WireNumber | 10 bits |
| Device-private Wires | 896..1022 provisional (127 values) |
| `kLocalDomain` | 1023 provisional; never enters a Link Interface |
| Direction | OriginToNode / NodeToOrigin |
| NodeId | 5 bits; 0 = broadcast (OtN) / invalid (NtO) |
| Nodes per Wire | up to 31 Nodes + one Origin |
| Conventional NodeIds | 30 diagnostic/test, 31 development |
| CAN profile families | Committed (all 11 bits WS) and Guest (allocated range, undesigned) |
| Classical CAN identifier | QoS 2 + Direction 1 + WireAlias 3 + NodeId 5 |
| CAN QoS packing | as-is, in the most arbitration-significant bits: `00` = Critical |
| CAN WireAlias | 0 = `kLocalBus`, 1..7 named aliases per Link |
| CAN optimized N=1 | NS0, EID 1..127, 0..7 payload bytes, no aggregate CRC |
| CAN General PDUA | Namespace 0..3, EID 1..1023 |
| CAN PDUA max frames | 8 (N <= 4 normal target; stricter for Critical QoS) |
| CAN aggregate CRC | none / CRC-8 / CRC-16 by N (`LINK §2.9`) |
| CAN net PDU bytes | 5 / 11 / 18 / 25 / 31 / 38 / 45 / 52 for N = 1..8 |
| CAN reassembly key | `(ingress Link, complete CAN identifier)`, one active context each |
| Generic gateway unit | complete canonical WS PDU |
| Gateway baseline | flood-and-filter within a Wire's realization |
| Wire representation change | only via explicit splice, applied before egress |
| Egress representation | Link Interface bitmask |
| Runtime routing model | caller-context routing over read-mostly tables |
| Buffer ownership | copy-based first-class; zero-copy deferred |
| Delivery policies | `Inline`, `Serialized`; a callback ABI is never mandatory |
| Endpoint binding modes | Static, Learned-from-ingress, Request-scoped, Receive-only, Transmit-only |
| Scheduling disciplines | `StrictPriority`, `WeightedFair` |
| Storage classes | snapshot, value queue, ownership-transfer queue, event queue |
| Static configuration | recommended medium-term; not required |
| Runtime reconfiguration | permitted |
| Commissioning phases | Unconfigured, Selected, Staged, Committed |
| Producer ownership | one visible producer per (Domain, Namespace, EndpointId) |
| Rejection behavior | counted and dropped; no error response emitted |
| Flow control | optional, per Link profile; 2 B or 8 B credit extension |
| Future CAN growth path | 29-bit Classical CAN profile planned; layout unspecified |
| Usage maturity model | Levels 0-5 (`INTRO §6`); Levels 0-1 must stay easy |
| Promiscuous/bring-up mode | host tooling and gateways only; never creates Wiring |
| Identity scope | one WireSpace; joining two needs a translating gateway |
| Local interface concept | Port (`CORE §1.6`); never appears in a PDU |
| Baseline Transport | Unreliable Datagram + receiver freshness handling |
| Next Transport candidate | Sequenced / end-to-end-protected datagram (`FUTURE §3.1`) |
| Service contract | canonical schema over bytes; generated types are views |
| Service message prefix | `u8 protocol_version`, `u8 message_type` (recommended default) |
| Master-initiated Links | I2C, SPI in scope; LLL polls without becoming producer |
| Privileged capabilities | separate build required; config cannot enable them |
| Interoperability | not claimed |
| RTL support | future first-class target |

---

# 4. Core Architectural Invariants

Particularly important when generating code or designs from these documents. Cite by ID.

## 4.1 Wire model — `WIRE`

| ID | Invariant |
|---|---|
| `WIRE-1` | **A Wire has exactly one Origin and zero or more Nodes.** |
| `WIRE-2` | **Origin is a per-Wire role, not a device class.** |
| `WIRE-3` | **NodeId 0 is not a Node.** It represents broadcast in `OriginToNode` and is invalid in `NodeToOrigin`. |
| `WIRE-4` | **Broadcast reaches the configured Nodes of one Wire and nothing else.** It implies no reverse Wire, grants no recipient transmit authority, and never discovers its membership. |
| `WIRE-5` | **Direction is structural.** It names which end produced the PDU, never the interaction pattern — not request/response, client/server, or command/status. |

## 4.2 Scope and identity — `SCOPE`

| ID | Invariant |
|---|---|
| `SCOPE-1` | **Device-private Wires never leave the device as device-private WireNumbers.** The only way their traffic reaches an external Link is an explicitly configured splice to a network-visible Wire. |
| `SCOPE-2` | **`kLocalDomain` never enters a Link Interface and is never spliced.** |
| `SCOPE-3` | **Long FPGA physical paths do not imply more Wires.** |
| `SCOPE-4` | **Physical proximity does not imply `kLocalDomain` or a device-private Wire.** |
| `SCOPE-5` | **Canonical identity is scoped to one WireSpace.** Connecting two WireSpaces requires explicit identity translation, not plain forwarding. |
| `SCOPE-6` | **An Endpoint Domain is a logical dispatch and authority boundary.** It is not a synonym for a device, core, or process; it is not a Node; and it is not a security boundary without a real protection mechanism. |

## 4.3 Aliases — `ALIAS`

| ID | Invariant |
|---|---|
| `ALIAS-1` | **WireAlias is Link-scoped.** It is never a globally canonical Wire identity. |
| `ALIAS-2` | **`kLocalBus` is alias 0.** If mapped, canonicalize immediately; if unmapped, RX remains locally usable but cannot be generically forwarded. |
| `ALIAS-3` | **Anonymous LocalBus TX is allowed only when one eligible Link makes it unambiguous.** |
| `ALIAS-4` | **`kLocalBus` is the compression code for the Link's own physical Wire**, not an arbitrary reserved number, and not a claim that the bus is Wire 0. |

## 4.4 Splicing — `SPLICE`

| ID | Invariant |
|---|---|
| `SPLICE-1` | **A splice applies before egress and after ingress canonicalization.** The scope check runs on the post-splice representation, and only the Wire representation changes. |
| `SPLICE-2` | **At most one splice per local routing step.** No chained or recursive splices. |
| `SPLICE-3` | **An anonymous (unmapped) LocalBus is not spliceable.** |
| `SPLICE-4` | **A spliced Wire must still have exactly one Origin and unique NodeIds across all its segments.** |

## 4.5 PDU integrity — `PDU`

| ID | Invariant |
|---|---|
| `PDU-1` | **The complete canonical PDU is the generic Router/gateway forwarding unit.** |
| `PDU-2` | **No Endpoint truncation, implicit aliasing, or silent remapping.** An unrepresentable canonical value fails placement or TX before a frame is emitted. |
| `PDU-3` | **No partial PDU is ever visible above the LLL.** |
| `PDU-4` | **Literal multi-byte numeric values serialize little-endian.** Bit packing within a byte remains profile-owned, and native object layout is never a wire representation. |
| `PDU-5` | **Reserved fields are rejected, not ignored.** Zero on transmit; a nonzero reserved field is dropped and counted rather than masked away. |
| `PDU-6` | **A header extension block states its own total size.** A parser can locate the payload without understanding the extension's contents. |

## 4.6 Routing and forwarding — `ROUTE`

| ID | Invariant |
|---|---|
| `ROUTE-1` | **No generalized RouterPort abstraction is currently required.** |
| `ROUTE-2` | **The Router should not require a central routing task.** Caller-context concurrent routing against read-mostly state is preferred. |
| `ROUTE-3` | **Failure on one egress does not cancel other successful egresses.** |
| `ROUTE-4` | **Base multihop Wire realization is acyclic**, including through splices. |
| `ROUTE-5` | **Electrical visibility grants nothing.** Attachment to a medium confers no membership, delivery, forwarding, or transmit authority; only configuration does. |
| `ROUTE-6` | **Every forwarding mechanism preserves source lineage.** Re-originating traffic is not forwarding: it makes the re-originator the authoritative producer, and requires its own Endpoint identity and configured authority. |

## 4.7 Dispatch and Ports — `DISP`

| ID | Invariant |
|---|---|
| `DISP-1` | **Autonomous Service TX uses an injected binding, not a hard-coded Wire.** |
| `DISP-2` | **Declared delivery policy is preserved across placement changes.** A `Serialized` Service stays serialized when its peer moves on-chip. |
| `DISP-3` | **A Port is local; an Endpoint is network-visible.** No canonical field carries Port identity, and `TxBinding` is an output Port. |
| `DISP-4` | **Exactly one externally visible producer per `(Endpoint Domain, Namespace, EndpointId)`.** Several synchronized local writers may sit behind it; they do not become separately addressable sources. |
| `DISP-5` | **Endpoint identity provides naming, not authority.** Wires, bindings, and typed local access provide authority; a Link or Wire reaching a device exposes no more of its Endpoint namespace than it is configured to carry. |
| `DISP-6` | **Declared Endpoint concurrency contracts are preserved.** Dispatch and generated APIs neither add nor remove an Endpoint's ownership, synchronization, queueing, or reentrancy constraints. |
| `DISP-7` | **Each Endpoint registration declares exactly one binding mode**, and **reply authority is never inferred by reversing Direction.** An Endpoint needing both request-scoped replies and autonomous transmission uses separate registrations. |
| `DISP-8` | **Learned-from-ingress binding is disabled on unauthenticated multi-access Links**, and where enabled is bounded, auditable, and unable to create a Wire, broaden authority, reinterpret an alias, or authorize another Transport. |
| `DISP-9` | **A callback ABI is never the only way to receive.** The requirement is a bounded receive-delivery port; no profile may force arbitrary application code into the LLL's execution context. |

## 4.8 Buffer ownership — `OWN`

| ID | Invariant |
|---|---|
| `OWN-1` | **Copy-based buffer ownership is the first-class baseline.** A successful `send()` means the caller may reuse its buffer; it does not mean delivery. |
| `OWN-2` | **Every storage element on a delivery path is bounded, with a chosen exhaustion behavior.** This includes reassembly contexts, retry windows, and observation buffers. Observation never degrades delivery. |
| `OWN-3` | **Ownership transfer is explicit and traceable.** A view is not ownership; a rejected submission leaves ownership with the caller; an accepted owning submission transfers it exactly once; storage is not mutated after publication. |
| `OWN-4` | **Every accepted PDU reaches exactly one terminal local outcome:** transmission complete, cancelled by stop or restart, or a terminal local fault. |
| `OWN-5` | **Storage classes never substitute for one another.** A snapshot replacement is not a queue delivery, a value copy is not ownership transfer, and queue exhaustion never becomes latest-value replacement. |

## 4.9 QoS, queues, and congestion — `QOS`

| ID | Invariant |
|---|---|
| `QOS-1` | **QoS-Minimal and QoS-Full are the two standard implementation profiles currently favored.** |
| `QOS-2` | **Queue-full policy is configurable per Link instance.** |
| `QOS-3` | **Congestion is a normal send outcome, not automatically a Link fault.** |
| `QOS-4` | **Link-level flow control is optional and profile-specific.** |
| `QOS-5` | **Queue utilization/overuse and congestion drops must be observable.** |
| `QOS-6` | **QoS is not flow control.** QoS orders what has already been accepted; it never governs admission, receiver capacity, or exhaustion behavior. |
| `QOS-7` | **`StrictPriority` and `WeightedFair` are the two named scheduling disciplines.** Strict priority may starve lower classes by design; weighted fair may not. Neither provides a deadline or latency bound. |
| `QOS-8` | **Receiver credit measures storage in a fixed quantum, never a count of variable-size PDUs**, and any credit scheme reserves a lifeline path for credit restoration and Link control that ordinary traffic can never consume. |
| `QOS-9` | **A rejected send never advances protocol state.** Local rejection is distinct from a transport timeout and says nothing about remote receiver capacity. |

## 4.10 Links and profiles — `LINK`

| ID | Invariant |
|---|---|
| `LINK-1` | **The LLL owns Link mechanics, not generic Wire routing policy.** |
| `LINK-2` | **Inter-core communication channels are ordinary Links/Link Interfaces.** |
| `LINK-3` | **Initiating a transfer does not confer producer authority.** An LLL may poll or autonomously schedule; the source Endpoint remains the producer. |
| `LINK-4` | **Classical CAN Nodes are not required to implement generic LLL credit flow control.** |
| `LINK-5` | **Classical CAN PDUA MaxN is 8; N <= 4 is the normal target.** |
| `LINK-6` | **Classical CAN aggregate CRC policy is N=1 none, N=2..4 CRC-8, N=5..8 CRC-16**, and usable capacity is the net column of `LINK §2.10`. |
| `LINK-7` | **Classical CAN FrameControl uses 3 MessageGeneration bits and 3 FramesRemaining bits.** |
| `LINK-8` | **A Link profile is selected statically per Link.** Traffic never auto-detects or negotiates framing, encoding, or profile family at runtime. |
| `LINK-9` | **Local scheduling is not a bus-wide guarantee.** An in-progress transmission unit is non-preemptible and controller capability bounds achievable ordering; latency bounds come from deployment analysis, not from queues. |
| `LINK-10` | **Where a medium requires transmit ownership, it holds in every configured state**, including every commissioning phase. |
| `LINK-11` | **One serialized mutable execution context per LLL instance** owns its parser, reassembly, transmit scheduling, timers, and pools. Others reach it only through bounded ports, queues, or snapshots; a seqlock requires a serialized writer. |
| `LINK-12` | **Hop integrity and end-to-end integrity are different claims.** A gateway that reassembles and re-encodes computes a fresh check value, so per-hop verification does not cover the path. Neither is authentication. |
| `LINK-13` | **Validation precedes parsing**, and a stabilized profile is immutable — an incompatible change takes a new name or version. |

## 4.11 Configuration and authority — `CFG`

| ID | Invariant |
|---|---|
| `CFG-1` | **Static traffic/capacity checking is preferred over adding a complex adaptive congestion protocol to the core.** |
| `CFG-2` | **Do not require static Manifests, Flows, or WireContracts for basic communication.** |
| `CFG-3` | **Structural validity is always enforced; contract-level policy is always optional.** The two lists are in `CORE §19.1`. |
| `CFG-4` | **Promiscuous observation never creates persistent Wiring**, and is a host-tooling/gateway capability rather than ordinary Node behavior. |
| `CFG-5` | **Ephemeral auto-Wiring must announce itself** and must never be silently promoted to authoritative configuration. |
| `CFG-6` | **Commissionability is independent of routing capability.** A LocalBusOnly or N=1-only node may still accept NodeId/Wire assignment. |
| `CFG-7` | **A gateway reports local facts; the Organizer decides.** Gateways do not infer or repair routes. |
| `CFG-8` | **Privileged capabilities must be absent from normal builds.** Runtime configuration alone must never enable one. |
| `CFG-9` | **No wire interoperability is claimed.** Independent implementations are not presumed compatible without shared definitions or common vectors. |
| `CFG-10` | **Generated artifacts carry a compatibility check** sufficient to detect mismatched routing, Endpoint, and alias configuration between participants. A mismatch is reported, never repaired locally. |
| `CFG-11` | **An unconfigured node emits no ordinary Service traffic.** Before commitment it holds no Endpoint authority and participates only in Link commissioning control. |
| `CFG-12` | **Acceptance is set-valued and the constraints are coupled.** A tuple may be rejected even when every field is individually admitted; per-field validation is insufficient at both configuration and runtime. |
| `CFG-13` | **One authoritative Wiring source generates every projection.** Software, RTL, static, and host artifacts come from one source, and projections that disagree fail closed. |

## 4.12 Services, Transport, and composition — `SVC`

| ID | Invariant |
|---|---|
| `SVC-1` | **Origin failover/election is not a base Wire feature.** |
| `SVC-2` | **Link independence is bounded by declared size, timing, and transport compatibility.** An unplaceable Service fails configuration or TX; it is never silently degraded. |
| `SVC-3` | **Transport is selected by Service semantics, and reliability is not assumed safer than loss with freshness detection.** |
| `SVC-4` | **A receiver of periodic state must define its behavior when that state is absent or stale.** Duplicate suppression belongs to the Service or composition, never to the Router. |
| `SVC-5` | **Higher-level composition must flatten into ordinary Endpoints, Wires, bindings, and bounded local state.** The Router and LLL never learn a composition pattern. |
| `SVC-6` | **Redundancy composed above Wires owns correlation, duplicate suppression, stale rejection, failover, per-Wire health, and per-sink coverage.** Observers provide no coverage. |
| `SVC-7` | **A Service protocol is a canonical schema over bytes, never a C/C++ ABI.** Generated structs and classes are views; native padding, enum width, alignment, and host endianness define nothing. |
| `SVC-8` | **Protocol version, compatibility fingerprint, and build identity are three separate identities.** A build hash is not a schema compatibility check. |

## 4.13 Error and rejection semantics — `ERR`

| ID | Invariant |
|---|---|
| `ERR-1` | **Malformed, unrepresentable, or unauthorized traffic is counted and dropped, with no response emitted.** Errors are reported upward and locally, never backward and automatically. |
| `ERR-2` | **Native Link acknowledgment and controller retransmission are not WS delivery.** They operate below the LLL and say nothing about whether any Endpoint received a PDU. |
| `ERR-3` | **Diagnostic reporting is contained.** An error report can never generate another error report, local counters remain authoritative, and a malformed or babbling peer cannot force unbounded diagnostic work. |

---

# 5. Explicitly Superseded / Do-Not-Reintroduce Without Review

Older documents contain these concepts. They should not be assumed current:

- Wire as a directed one-source/one-or-more-sink object rather than a bus.
- `Main` / `Peer` / `PeerId` terminology.
- `PathTag` as the canonical Wire routing identity.
- old `{WireBand, RoutingCode}` architecture.
- old 11-bit CAN `PathTag`/`PeerId` allocations, including 4-bit PathTag and 4-bit PeerId.
- a globally scarce 3-bit Wire number on CAN.
- a mandatory `Link Engine` object that performs generic routing.
- a generalized RouterPort abstraction for Endpoint Domains/inter-core channels.
- requirement that static Wiring/Manifest exist before a network is useful.
- treating Flow/WireContract as implicit or mandatory base-network concepts.
- generic frame-by-frame gateway forwarding as the canonical heterogeneous gateway model.
- 16-frame Classical CAN PDUA maximum, and the 4-bit `FramesRemaining` countdown that enabled it.
- 1-bit or 2-bit MessageGeneration in the current N <= 8 PDUA layout.
- the `FrameControl` reserved bit at bit 4 (now absorbed by MessageGeneration).
- the gross per-N capacity table (5/12/19/26...) presented as usable Service capacity.
- the older adapter draft's Namespace-0 allocation (`1..32` public, `33..991` deployment, `992..1023` public General-only). The current working plan is `CORE §8.3`.
- `Tap` / `Relay` / `Replicator` as base architectural roles. Local taps and gateway forwarding are ordinary routing outcomes (`CORE §12.3`); re-originating traffic is an application-level service.
- **Ephemeral route repair / learned forwarding.** An earlier change statement proposed that a gateway with no entry for a Wire, observing that Wire's traffic on two eligible interfaces, could infer continuity and install a temporary learned splice, aged out or discarded on reboot. This is rejected: it puts topology inference in the data plane, conflicts with the acyclic-realization and no-duplicate-suppression rules (`CORE §12.4`), and makes forwarding depend on observed traffic rather than installed configuration. A gateway reports what it observed; the Organizer installs routes (`CFG-7`).
- Every physical bus having canonical WireNumber 0 because it uses `kLocalBus`. Alias 0 compresses the Link's *own* number, whatever that number is (`ALIAS-4`).
- `Route` as an object distinct from the Wire it realizes, with its own terminals and Direction. Forwarding is a local table (`CORE §11`), and Direction is a per-PDU field on a Wire (`CORE §3.1`); there is no separate cross-domain path object.
- `ParticipantId` as a third identity space alongside Endpoint and Node identity. It existed to name members inside a communication component; that catalog is deferred (`FUTURE §13`), and if such components arrive they flatten into ordinary Endpoints and Wires (`SVC-5`).
- **Reachability requiring a Manifest.** An earlier generation held that reachability and authority exist only where a Manifest configures them. Deliberately relaxed; see `DEPLOY §2`. Structural validity is still always enforced (`CFG-3`).
- A **Wire Space as a security or memory-protection boundary.** Recovered only as an identity scope (`CORE §4.6`). It is not claimed to enforce anything, and `CORE §22` remains the security position.
- **`Node` meaning an Endpoint Domain.** An earlier generation avoided the word "Node" precisely because it implies physical topology, and used "Endpoint Domain" for a dispatch scope. WireSpaces reuses `Node` for the *Wire participant role* that carries a NodeId. Both concepts survive; only the earlier word choice is superseded. See `SCOPE-6`.
- **Bidirectional interaction requiring two Wires.** In the earlier directed-Wire model, a request and its response were separate Wires. A WireSpaces Wire carries both Directions, so a request/response pair is two exchanges on one Wire (`CORE §3.1`).
- **`RouteId`, and any deployment-wide unique identifier for a Wire-realizing path.** Already implied by retiring `Route` as an object, but stated separately because the old validation rules enforced its uniqueness. The identity that matters is the WireNumber; table indices are local artifacts (`DEPLOY §2.3`).
- **A separate runtime producer token** (`ProducerKey` and its predecessors). The producing identity is resolved from Wire, Direction, NodeId, Namespace, EndpointId, and ingress context; no extra field is carried (`CORE §12.7`).
- **`RoutingWord` / Band-0 canonical reconstruction on CAN**, along with the 4-bit `PathTag` and 4-bit `PeerId` fields it reconstructed. The current mapping is direct to Wire, Direction, WireAlias, and NodeId (`LINK §2.2`).
- **A trailing "PDU FCS" whose width and algorithm were open.** Superseded by a settled aggregate CRC policy, which is further along than the older draft: none at N=1, CRC-8 at N=2..4, CRC-16 at N=5..8 (`LINK-6`).
- **Gross per-N capacity tables extending to N=16.** Both the depth and the gross figures are superseded (`LINK-5`, `LINK §2.10`).
- **A committed-CAN field layout reused as a Guest-CAN layout.** Guest CAN has fewer identifier bits by construction and needs its own encoding; reinterpreting committed field positions in a subset of the identifier space invites silent aliasing (`FUTURE §11.2`).
- **The ascending QoS numbering** (`QoS 0` = Background through `QoS 3` = Critical), and the CAN inversion step it required. Reversed deliberately: QoS is now the count of strictly-higher-priority classes, so Critical is 0, and CAN packs the value unchanged (`CORE §14`, `LINK §2.2`). Any older table, constant, or arbitration-mapping helper using the ascending order is wrong, and the error is silent — it produces a system that runs with its priorities exactly inverted.
- **`WireBand` as a per-deployment reinterpretation of the routing field**, with Band 0 standardized and Bands 1-3 user-defined including opaque or generated routing. Retired along with `RoutingWord`, and not worth recovering: there are no spare bits for it, and one canonical WireNumber interpretation is the point. The *requirements* it imposed on any extension point are worth keeping, though — a named immutable definition, deterministic validation, static compatibility checking, unambiguous canonical reconstruction, and fail-closed rejection of anything unsupported.
- **`PathTag::Local` and its open Direction/PeerId reconstruction rules.** Local delivery is `kLocalDomain` (`CORE §4`), which is a reserved WireNumber rather than a routing-field value with its own validation puzzle.
- **A "reliable transport" defined by field sketches.** An earlier generation carried draft reliable-transport header layouts that were explicitly not a contract. They are not recovered, and the current position is that reliability is designed from concrete Service requirements or not at all (`FUTURE §3.2`). The intermediate transport in `FUTURE §3.1` is the one worth designing first.
- **Optional automatic remote infrastructure-error reports.** An earlier generation permitted these under heavy constraints. Superseded by `ERR-1`: nothing is emitted automatically in response to bad traffic. Deliberate remote diagnostic reporting remains available as an ordinary configured Service, subject to `ERR-3`.

If an implementation task appears to require one of these, first verify that the current architecture genuinely cannot solve the problem without it.

---

# 6. Open Questions

## 6.1 Canonical / allocation

- Final exact WireNumber allocation fences.
- Whether canonical header extensions need additional standardized common fields before freeze.
- The extension length encoding itself. A 2-bit code giving 1..3 bytes is the working direction (`CORE §2.3`); what is settled is only that the block must be self-describing (`PDU-6`).
- Whether an unrecognized but well-formed extension is preserved on forwarding and rejected on dispatch, decided per extension or globally. This must be answered before any extension is defined.
- Whether the first extension defined is the sequence/end-to-end check value from `FUTURE §3.1`, which would make it the mechanism's first real test.
- Exact TransportType registry.
- Namespace 3 registry process (allocation authority, experimental reclamation, tombstones).
- Physical bit positions and field significance within `Control`. Byte order for literal multi-byte values is settled (`PDU-4`); packing inside a byte is not.

## 6.2 Splicing

- Whether a splice is one bidirectional mapping or two directional route entries.
- Whether multiple devices may splice onto one shared external Wire by default, and how NodeId coordination is validated if so.
- Whether `InternalDebugWire` receives a standard reserved device-private WireNumber.
- Splice representation in generated/static configuration.

## 6.3 Router / concurrency

- Seqlock vs immutable-table swap as the preferred reference implementation.
- Exact representation of local Endpoint Domain delivery in routing tables.
- Sparse vs dense tables on constrained targets.
- Exact behavior during live route-table reconfiguration.
- Endpoint lifetime/safe-publication mechanism if runtime Service replacement is introduced.

## 6.4 QoS / queues

- Whether unsupported QoS collapses to Normal or is rejected on QoS-Minimal Links.
- Exact default queue-full policies.
- Whether latest-value replacement is keyed by Service/Endpoint stream, and how that key is defined. That only replaceable *state* traffic may be coalesced is settled (`OWN-5`); how an Endpoint declares itself replaceable is not.
- Whether `WeightedFair` accounting is by frames, bytes, or a profile-declared unit, and what weight range is expected.
- Whether any standard shedding/admission hooks are needed beyond implementation-specific policy.

## 6.5 Flow control

- Exact credit accounting (cumulative grant, watermark, or another scheme).
- Update cadence and standalone credit-frame format.
- The credit quantum and accounting rule per profile. That credit measures a *fixed storage quantum* rather than a PDU count is settled (`QOS-8`); the size is not.
- The lifeline reservation per profile: how much capacity, how it is kept unreachable by ordinary traffic, and how it is verified.
- How much downstream pressure a gateway should reflect into upstream credits.

## 6.6 Ownership

- Zero-copy destination-owned allocation API, if and when it is justified.
- Whether source-owned immutable buffers with multi-driver completion tracking are ever worth the complexity.
- The bounded-shared-lease contract, if fan-out by copy is ever insufficient: maximum consumer count, release tracking, reclamation owner, and restart behavior.
- Whether the storage allocator's lifetime is separated from restartable Link state explicitly, or by convention (`OWN-3`).
- Platform contracts for cross-core and DMA-backed paths, per `CORE §13.3`. These are per-adapter answers rather than one global decision, but an unanswered list is the blocker on treating any such path as stable.

## 6.7 Telemetry

- Exact Domain Local Link Telemetry Service schema.
- Exact minimum counters required by conformance classes.
- Exact per-Wire top-talker/drop-report format.

## 6.8 Classical CAN

- Final CRC-8 algorithm/parameters; SAE J1850 is the current implemented candidate.
- Final CRC-16 algorithm/parameters.
- Aggregate CRC placement, coverage, and byte order.
- Final byte-exact General PDUA and optimized-N1 encoding.
- Physical placement and significance order of Direction, WireAlias, and NodeId in the remaining 8 identifier bits, and which Direction value means `OriginToNode`. The QoS position and inversion are settled (`LINK §2.2`).
- Where commissioning control space lives, given that all 11 identifier bits and both byte-0 encodings are allocated (`LINK §2.13`). This must be reserved before the layout is frozen.
- The default Critical-QoS PDUA depth cap, and whether it is tooling policy or a profile constraint (`LINK §2.6`).
- Reassembly context pool sizing, and whether the one-context-per-identifier limit survives contact with a many-Wire gateway.
- Guest CAN in its entirety: identifier allocation, routing budget, framing, Transports, QoS behavior, and how disjointness from legacy owners is verified (`FUTURE §11.2`).
- Controller filter configuration and scheduler fallback rules for representative CAN peripherals.
- Whether the aggregate CRC and PDUA generation counter are sufficient to detect the duplicate case named in `CORE §18.1`, or whether that is purely a Service-level concern.
- Exact DLC rules, length derivation, padding, and short-continuation handling.
- Reassembly timeout rules and rate assumptions.
- Whether CAN FD/XL share the same PDUA concepts or receive simpler native-PDU profiles.
- Whether the planned 29-bit Classical CAN profile is standardized, and its identifier layout.

## 6.9 UART / byte streams

- COBS vs HDLC-style framing.
- CRC-16 polynomial/parameters.
- Maximum frame/PDU size.
- Flow-control extension encoding and interaction with framing.

## 6.10 Discovery / configuration

- Exact pre-addressing commissioning algorithm per Link type.
- NodeId conflict/rejoin behavior.
- Alias/config generation and update rules.
- Persistence/lease semantics for ephemeral host configuration.
- Exact discovery Service schema.
- Exact gateway-management Service schema, including the "make interface N carry Wire W" operation.
- Policy for cyclic and non-tree topologies during auto-Wiring.
- Scope and build-time removal rules for promiscuous/bring-up mode.
- What the `Staged` phase persists, and how an abandoned commissioning attempt is rolled back.
- The configuration compatibility-check mechanism: what it covers, how it is computed, whether it is per-Link or per-deployment, and whether a mismatch blocks traffic or only raises a diagnostic (`DEPLOY §2.4`).

## 6.11 Freshness and transport semantics

- How a Service declares freshness: a locally checked interval, or a canonical timestamp/sequence in a header extension.
- Whether any standard "state is stale" notification exists, or whether each Service defines its own degraded behavior.
- Whether ordering guarantees are declared separately from delivery guarantees.
- How polled-Link cadence is expressed in Link capabilities so freshness bounds can be checked statically.

## 6.12 Ports and composition

- Whether Port becomes a generated API concept or stays an architectural term.
- Whether input Ports need a naming/binding mechanism symmetric with `TxBinding`.
- The first composition pattern to attempt under `SVC-5`, and whether it genuinely flattens.

## 6.13 Master-initiated Links

- I2C and SPI transaction formats, PDU delimiting, and the idle/"nothing to send" response.
- Who owns polling cadence configuration, and whether it is a Link-profile or Wiring property.
- Whether SPI needs its own framing and CRC choices or can reuse the byte-stream profile (`LINK §3`).

## 6.14 Identity scope

- Whether a WireSpace needs an explicit identifier, or remains an implicit deployment-wide scope.
- What a cross-WireSpace translating gateway looks like, and whether it is worth specifying before a real case exists.
- Whether any detection is possible when two WireSpaces are accidentally bridged by a plain forwarding gateway.

## 6.15 Services and schemas

- The schema language and code generator for Service contracts. `SVC-7` fixes that the schema is the contract; it names no format.
- What the compatibility fingerprint covers, and whether it is generated from the schema alone or from schema plus profile and configuration choices (`DEPLOY §2.4`).
- Whether the recommended `protocol_version` / `message_type` prefix becomes a requirement or stays a default.
- The exact API representation of a reply context: storage, lifetime, correlation, invalidation, and the retained-handle form needed for deferred responses (`CORE §10.5`).
- Whether binding mode is expressed in generated code, in the Manifest, or both.

## 6.16 Higher layers

- Reliable Transport(s), and whether the sequenced/end-to-end-protected datagram is designed first (`FUTURE §3.1`).
- Multi-Origin command-source policy if useful.
- Formal Manifest, Flow, WireContract, and static analysis models.
- Security/authentication profiles.
- Redundant/cyclic Wire realization if ever justified.
- Whether a Domain Control layer is ever needed for group management of Services.
- Whether a communication-component catalog is worth establishing once `SVC-5` has been exercised.

---

# 7. Pending Source Material

The following older documents have **not** yet been mined. Their model is the superseded generation, but they may hold recoverable detail in the same way `WS_old/network/architecture_overview.md` did.

| Source | Approx. lines | Expected destination |
|---|---:|---|
| `WS_old/network/link_engine_runtime_and_status.md` | 640 | `CORE §18`, `LINK` |
| `WS_old/network/hdlc_logical_link_profile.md` | 560 | `LINK §3` |
| `WS_old/network/prototype_and_validation.md` | 1,460 | `CORE §27`, possibly a new document |
| `WS_old/network/rationale_use_cases_and_risks.md` | 510 | `INTRO`, `FUTURE` |

The copy of `can_pdu_adapter_spec.md` that sat in this directory was byte-identical to the `WS_old` one, so mining the latter covered both. It has moved to `archive/`.

`link_engine_runtime_and_status.md` is now the most valuable remaining source, because `CORE §13.3` and `OWN-4` opened questions about restart and lifecycle that it was written to answer.

The mining method that has worked so far: read for concepts that were *dropped* rather than *superseded*, recover the ones that still hold, record the rejections in §5 so they cannot leak back, and note deliberate divergences rather than silently overriding them.

---

# 8. Revision History

## 8.1 Revision 0.7 — QoS renumbering, and the last three large legacy specifications

Two changes: the canonical QoS numbering was reversed by decision, and the three largest remaining `WS_old/network` documents were mined, which completes the bulk of the legacy material.

**QoS renumbering.** Canonical QoS is now Critical 0 through Background 3, inverted from revision 0.6. The reason for the reversal is that it makes the numbering *derivable* rather than conventional: **QoS is the count of classes with strictly higher priority than yours**, so Critical has none above it and is 0. Lower-wins then follows from the definition, and the CAN inversion step introduced one revision earlier disappears entirely — the value packs into the arbitration-significant bits unchanged (`CORE §14`, `LINK §2.2`).

This is worth flagging as the highest-risk change in the document set so far. It inverts a numeric constant that appears in header packing, comparisons, queue indexing, and arbitration mapping, and getting it wrong is silent: the system runs, with its priorities exactly reversed. Anything written against revision 0.6 or earlier needs checking. The old ordering is recorded in §5.

**From `core_protocol_and_routing.md`.** Its `RoutingWord` / `WireBand` model was already retired, but it held four concrete decisions:

- **Little-endian serialization for literal multi-byte numeric values** (`CORE §2`, `PDU-4`). Cheap, settled, and previously absent — a real divergence risk across this project's C++, Python, and RTL implementations. The document's careful boundary is kept: byte order for literal values is decided, bit packing inside a byte is not, and native object layout is never a wire representation.
- **Self-describing header extension length** (`CORE §2.3`, `PDU-6`). The property that matters is not the encoding but the consequence: a parser can locate the payload without understanding the extension, so a new extension is not a flag-day change across a deployment. It also surfaced a genuine open question about whether an unrecognized extension is preserved on forwarding or rejected on dispatch.
- **Reserved fields are rejected, not ignored** (`PDU-5`). The choice that keeps future field assignment possible. Ignoring reserved bits today forecloses using them tomorrow.
- **Set-valued acceptance with coupled constraints** (`CORE §19.1`, `CFG-12`). The subtle one. A validator that checks each field against its own permitted set passes almost everything and fails exactly on the combinations that matter — a Critical PDU at a length only permitted at lower QoS, for instance.

Its **five binding modes** were recovered and adapted (`CORE §10.5`, `DISP-7`, `DISP-8`). `CORE §10` already described three ways a Service gets transmit context; naming the full set turned that into something checkable, and carried in two rules with real teeth: reply authority is never inferred by reversing Direction, and learned-from-ingress binding is disabled on unauthenticated multi-access Links. WireSpaces diverges here in one respect worth noting — because one Wire carries both Directions, replying is structurally easy in a way it was not in the old directed-Wire model, which makes it *more* important to say that structural ease is not authority.

**From `logical_links_and_transports.md`.** The richest source so far for implementation-level contracts:

- **Ownership discipline** (`CORE §16.1`, `OWN-3`): a view is not ownership, a rejected submission leaves ownership with the caller, an accepted owning submission transfers exactly once. Plus two lifetime consequences that are easy to get wrong — receive storage retained by application code must outlive a Link restart, and reclamation goes through the allocator's owner.
- **Storage classes as distinct contracts** (`CORE §16.4`, `OWN-5`). Snapshot, value queue, ownership-transfer queue, and event queue look alike in code and differ in what they promise. This also partly answers the standing question about latest-value replacement: only *state* may be coalesced, and queue exhaustion must never quietly become replacement.
- **Terminal outcome for every accepted PDU** (`OWN-4`). Without it, buffer reclamation has no defined point and no counter can be balanced.
- **Two named scheduling disciplines** (`CORE §14.2`, `QOS-7`), with strict priority's starvation stated as a property rather than a defect, and weighted fair's accounting unit as a required declaration.
- **The credit lifeline** (`CORE §15.5`, `QOS-8`). The standout recovery of this pass. Credit flow control has a natural deadlock — if the credit update is itself subject to credit, a stalled link stays stalled with both ends behaving correctly — and a permanently reserved lifeline is the structural fix. Also settles that credit measures a fixed storage quantum rather than a count of variable-size PDUs, narrowing an open question in §6.5.
- **One serialized mutable context per LLL instance** (`CORE §13.3`, `LINK-11`), with the platform-contract checklist that cross-core and DMA paths must answer rather than assume, and the observation that a seqlock requires a serialized writer.
- **Hop versus end-to-end integrity** (`CORE §20`, `LINK-12`). A gateway validates, reassembles, re-encodes, and computes a *fresh* check value, so every hop is verified and the path is not. Invisible in a single-Link deployment and real the moment a gateway exists.
- **A callback ABI must never be mandatory** (`DISP-9`), which matters for RTL and polled implementations.

Its **sequenced / end-to-end-protected datagram** was recovered as the named next Transport (`FUTURE §3.1`) and promoted ahead of reliability, because `SVC-3` already argues that most traffic wants freshness detection rather than retransmission.

**From `application_protocols_and_services.md`.** Mostly Service-level, and two items are structural:

- **A Service contract is a schema over bytes** (`CORE §21.3`, `SVC-7`). Generated structs are views; padding, enum width, alignment, and host endianness define nothing. Given that this project expects C++, Python, and RTL implementations of the same protocol, casting a buffer to a struct is the single most likely source of silent divergence.
- **Three separate identities** (`SVC-8`): protocol version, compatibility fingerprint, and build identity. A build hash is both too sensitive and not sensitive enough to serve as a compatibility check.

Also recovered: the recommended two-byte message prefix; the **Service archetype** table with QoS defaults and the note that RPC implies no reliable Transport (`CORE §21.4`); **diagnostic containment**, whose first rule is that an error report can never generate another (`ERR-3`); the DRIP-shaped summary-plus-detail-on-request pattern; catalog promotion criteria including "at least one real deployment" (`FUTURE §8.1`); and, for a future Domain Control, serialized acceptance with supersession plus the rule that gating must not gate its own control path (`FUTURE §14`) — the same structural error as a credit scheme without a lifeline.

**Divergences and rejections** recorded in §5: the ascending QoS order and its CAN inversion; `WireBand` as a per-deployment routing reinterpretation, with its requirements-for-any-extension-point preserved; `PathTag::Local`; reliable transport defined by field sketches; and optional automatic remote error reports, which `ERR-1` supersedes while `ERR-3` keeps the useful constraint.

Twenty invariants were added across eight families; no existing ID changed. `CORE §21` gained two subsections, shifting its last two subsections down by two, and inbound references were updated. Open questions gained a Services and schemas group in §6.15.

## 8.2 Revision 0.6 — recovered from the endpoint-domain and CAN-adapter specifications

Two `WS_old/network` documents were mined: `endpoint_domains_wires_routes_and_access.md` and `can_pdu_adapter_spec.md`. Both belong to the superseded generation, and both held material with no current equivalent.

**From the endpoint-domain specification.** Its core model is the one already retired in §5, but three of its ideas were load-bearing and absent here:

- **Producer ownership and Endpoint concurrency** (`CORE §9.6`, `DISP-4`, `DISP-6`). The clearest gap found in any mining pass. `WIRE-1` constrains a Wire to one Origin, but nothing constrained an *Endpoint identity* to one producer, so two Services in one Domain could both publish as EndpointId 42 with no way for a receiver to distinguish them. The companion rule — that each Endpoint declares its concurrency model and that dispatch preserves rather than "fixes" it — is the producing-side counterpart to `DISP-2`.
- **Endpoint naming is not authority** (`CORE §9.7`, `DISP-5`), including the typed-handle discipline and an honest statement of its limits. `ROUTE-5` already said electrical visibility grants nothing; this says *knowing a name* grants nothing either, which is the software-side half of the same idea.
- **Source lineage** (`CORE §12.7`, `ROUTE-6`). Lineage preservation existed in fragments across forwarding, splicing, fanout, and observation. Consolidating it made the boundary visible: every mechanism preserves the producer, and re-origination is the one operation that does not — so it needs its own authority rather than hiding inside a route.

Also recovered: the Endpoint Domain as an explicitly *logical* boundary that is not a device and not a security boundary (`SCOPE-6`); Direction as structural (`WIRE-5`); the negative list for broadcast, including the prohibition on sharing one acknowledgement state across recipients (`WIRE-4`); universal bounded storage (`CORE §15.7`, `OWN-2`); the explicit "what a Wire does not guarantee" list (`CORE §3.6`); the tooling rejection checklist (`DEPLOY §2.3`); generated-artifact compatibility checking (`DEPLOY §2.4`, `CFG-10`); and the rule that collapsing layers on a tiny target must preserve authority distinctions (`CORE §26.1`).

**From the CAN adapter specification.** Its identifier layout, frame depth, and Endpoint allocation are superseded, but it resolved an open question and supplied several implementation-level rules:

- **QoS numbering and its CAN placement** (`CORE §14`, `LINK §2.2`). `LINK §2.1` previously carried this as an unresolved note about needing an inversion. Resolved instead by defining QoS as the **count of strictly-higher-priority classes** — Critical 0 through Background 3 — which makes "lower number wins" a consequence of the definition rather than a convention. CAN then packs the value unchanged into the most arbitration-significant bits, with no inversion step for an LLL to get backwards. Only the placement of the remaining identifier fields is still open.
- **Committed versus Guest CAN profile families** (`LINK §2.1`, `FUTURE §11.2`). The current layout spends all 11 identifier bits, which means a WS bus cannot carry legacy CAN traffic — a constraint that was implicit and unstated. Naming the two families makes the limitation explicit and gives coexistence somewhere to live.
- **Commissioning control space** (`LINK §2.13`). Follows directly: an unconfigured node has no NodeId and therefore cannot form a valid identifier, yet must transmit to acquire one. The profile has to reserve space for this before freezing, which is a real cost against an exhausted field.
- **The reassembly context model** (`LINK §2.8`): keyed by `(ingress Link, CAN identifier)`, one active context per key, drawn from a fixed pool, where exhaustion rejects rather than evicts. Evicting an in-progress reassembly would convert local pressure into phantom loss on an unrelated Wire.
- **The ordered transmit procedure** (`LINK §2.12`), whose ordering is itself the requirement, plus the rules that transmit selects the smallest legal `N` against net capacity and that a failure after START aborts the whole PDU without retrying.

Also recovered: a stricter PDUA depth policy for Critical QoS, justified by arbitration monopolization rather than memory (`LINK §2.6`); local scheduling realism — non-preemptible frames, controller mailbox limits, and local ordering as no bus-wide guarantee (`CORE §14.2`, `LINK-9`); QoS is not flow control (`QOS-6`); native CAN acknowledgment is not WS delivery (`ERR-2`); static profile selection with no negotiation (`LINK-8`); transmit ownership holding in every phase (`LINK-10`); rejection being silent on the wire (`CORE §18.1`, `ERR-1`); the four-phase commissioning model (`DEPLOY §1.2`, `CFG-11`); boundary-value conformance vectors and loss amplification with `N` (`CORE §27.1`, `CORE §27.2`); and exit criteria for provisional status (`CORE §27.3`).

One structural note: `ERR` is a new invariant family, added because rejection semantics fit none of the twelve established in revision 0.5. `LINK §2` subsections shifted by one to accommodate the new §2.1, and inbound references were updated. No existing invariant ID changed.

## 8.3 Revision 0.5 — document set split

The single working document `wirespaces_architecture_preliminary.md` (3,891 lines) was split into the current six documents plus this register. Content was preserved; the changes were structural.

Material moved out of the main line into `FUTURE` under one criterion: **not yet designed, and its absence cannot cause a wrong implementation decision today.** Both clauses were required, which is why several forward-looking items stayed:

- Boundary statements stayed in `CORE`: Origin failover is not a Wire feature, composition must flatten, redundancy owns six responsibilities, a constrained LLL must not grow into a transport, two WireSpaces do not merge by being connected.
- Optional-but-current features stayed with their subsystem: credit flow control, promiscuous mode, per-Wire top-talker telemetry, QoS-Full. With one implementation, "optional" and "future" are the same thing, but an implementer of the mandatory part needs the optional part adjacent.
- Where a topic was both constraint and ambition, the constraint was compressed into `CORE` and the discussion moved. This applies to static capacity analysis, redundancy direction, bulk transfer, and cross-WireSpace identity.

Also in this revision:

- Invariants were regrouped by topic and given stable IDs (§4), replacing a flat list of 55 positional entries. The old numbers are not preserved; the mapping was one-to-one and no invariant was dropped or added.
- Cross-references became document-coded (`CORE §6.2`). A bare `§x` always means the current document.
- The interoperability disclaimer was reduced from a subsection to a line, and conformance vectors were re-justified: the reason is divergence between the project's own C++, Python, and RTL implementations, not external implementers, of whom there are none.
- Per-document status headers replaced the single front matter.

## 8.4 Revision 0.4 — recovered from `WS_old/network/architecture_overview.md`

That document describes an earlier generation whose core model — the directed one-source/one-or-more-sink Wire, `{WireBand, RoutingCode}`, `RoutingAlias`, `PathTag`, Route as an object distinct from Wire, `ParticipantId`, and Tap/Relay/Replicator as base roles — is listed in §5. What survived:

- **Port versus Endpoint** (`CORE §1.6`): a Port is the local typed directional interface; an Endpoint is the network-visible termination. `TxBinding` is an output Port, so the receive side got the same treatment.
- **Transport is a Service-semantics choice, and reliability is not assumed safer than loss with freshness detection** (`CORE §20.2`). The sharpest idea in that document, with no prior equivalent here. It pairs directly with latest-value queue policy.
- **Freshness and staleness as declared Service properties** (`CORE §21.6`), with duplicate and stale data added to the error categories.
- **Link independence is conditional** (`CORE §13.4`): link-independent only inside declared size, timing, and transport compatibility. Revision 0.3 made the claim unqualified even though Link capabilities already had the machinery to check it.
- **Master-initiated and polled Links** (`CORE §1.7`, `LINK §7`), including I2C and SPI as in-scope Physical Links, and `LINK-3`.
- **Higher-level composition must flatten** (`CORE §19.2`), taken as a constraint without the component catalog.
- **What a redundancy composition owns** (`CORE §23.2`), plus the distinction between path redundancy and voting across independent producers.
- **Electrical visibility is not membership** (`CORE §12.6`). Needed precisely because flood-and-filter was chosen.
- **Proximity does not imply locality** (`CORE §4.5`), the inverse of the FPGA rule.
- **WireSpace as an identity universe** (`CORE §4.6`), recovered as scoping only.
- **Privileged capabilities require a separate build** (`CORE §22.1`), generalizing the promiscuous-mode rule into a stated policy with a capability list.
- **No interoperability is claimed.**

Recorded as a deliberate divergence rather than a recovery: that document's invariant 13 held that reachability and authority exist **only** where the Wiring Manifest configures them. WireSpaces relaxed this so that anonymous `kLocalBus` and Level 0 use work with no Manifest at all. This is the principal philosophical difference between the two generations, and it is a trade, not an oversight.

Noted but not recovered: Domain Control as a named layer, and the reusable Communication Component catalog. Both are in `FUTURE`.

## 8.5 Revision 0.3 — recovered from `wirespaces_simplified_wire_and_autowiring_design_change.md`

That document's terminology and CAN numbering are superseded, but several ideas had no equivalent in revisions 0.1 or 0.2:

- **Usage maturity levels 0-5** (`INTRO §6`), with the rule that advanced features must not make Levels 0-1 harder.
- **Physical and Virtual Wires** as named cases (`CORE §3.5`).
- **Wire identity continuity** (`CORE §4.1`): a Wire's UUID/name keep logs interpretable when its short WireNumber is reassigned.
- **`kLocalBus` re-derived** (`CORE §5`): alias 0 is the compression code for *this Link's own physical Wire number*, which explains why it is privileged and yields a concrete TX rule. The unmapped case became the exception rather than the base case.
- **Commissionable is independent of routable** (`CORE §5.6`).
- **Gateway discovery reporting** (`DEPLOY §1.10`), including per-interface Wire membership — the missing mechanism behind the loop rule.
- **Globally stable NodeIds as a convenience** (`DEPLOY §1.8`).
- **Ephemeral configuration must announce itself** (`DEPLOY §1.9`).
- **Structural validity versus contract** (`CORE §19.1`).
- **Promiscuous / bring-up operation** (`DEPLOY §3.3`).
- **A future 29-bit Classical CAN profile** (`FUTURE §11`).

Rejected rather than recovered: ephemeral route repair / learned forwarding (recorded in §5), the 3-bit canonical WireNumber on CAN, and `Main`/`Peer`/`PeerId`, `Link Engine`, and WireContract as specified there.

## 8.6 Revision 0.2 — merged the overview into the snapshot

Merged from `wirespaces_high_level_architecture_design_overview.md`, having been confirmed as still current: Wire Splicing including the `spliceWire` route field and the before-egress ordering rule; the Default Internal Debug Wire; Service TX bindings and `Inline`/`Serialized` delivery; copy-based buffer ownership; Namespace and EndpointId allocation; why a bus rather than pairwise edges; small-device Router notes; Ethernet/CAN adaptation asymmetry; implementation scaling profiles; and the worked examples.

Reconciled where reinstating those collided with snapshot text: `SCOPE-1` carries the splice carve-out; the field-preservation invariant names splicing explicitly; the route entry carries an optional `spliceWire`; the Organizer may install splice mappings; anonymous `kLocalBus` is explicitly not spliceable.

Corrected: the Classical CAN payload budget under the CRC-8/CRC-16 policy. The overview's table (N=1→5, N=2→12, N=3→19, N=4→26) was a pre-integrity gross budget and overstated usable bytes for N >= 2.

Carried forward from the snapshot and still superseding the overview: PDUA MaxN 8 (was 16) and the 3+3 FrameControl layout (was 1+4); COBS favored over HDLC-style escaping; UART CRC-16 choice reopened; QoS-Minimal/QoS-Full profiles; congestion as a normal send outcome; and all terminology supersessions.

## 8.7 Superseded source documents

| Document | Superseded by |
|---|---|
| `wirespaces_high_level_architecture_design_overview.md` | Revision 0.2 |
| `wirespaces_architecture_snapshot_2026-08-20.md` | Revision 0.2 |
| `wirespaces_simplified_wire_and_autowiring_design_change.md` | Revision 0.3 |
| `WS_old/network/architecture_overview.md` | Revision 0.4 |
| `wirespaces_architecture_preliminary.md` | Revision 0.5 (this document set) |
| `intro/wirespaces_high_level_design_overview.md` | `introduction.md` |
| `WS_old/network/endpoint_domains_wires_routes_and_access.md` | Revision 0.6 |
| `can_pdu_adapter_spec.md` (and its `WS_old` twin) | Revision 0.6 |
| `WS_old/network/core_protocol_and_routing.md` | Revision 0.7 |
| `WS_old/network/logical_links_and_transports.md` | Revision 0.7 |
| `WS_old/network/application_protocols_and_services.md` | Revision 0.7 |

`WS_old` is left intact as its own historical tree; `archive/` holds copies of the documents mined from it, so this document set carries its own provenance without editing the old one.

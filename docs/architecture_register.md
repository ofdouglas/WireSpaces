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
- Injected transmit Endpoints rather than hard-coded Wires.
- Endpoint delivery crosses a bounded storage boundary and never runs Service code synchronously.
- Queue and Snapshot are the two Endpoint storage semantics; one Endpoint owns exactly one storage element.
- Storage semantics and writer concurrency are immutable properties of a Service definition, not deployment choices.
- Arrival time is captured at acceptance, because consumer latency now sits inside the delivery path.
- A Snapshot Endpoint always carries a generation counter; Snapshot transmit Endpoints echo sampled and sent generations.
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
- WireSpaces defines no Service-to-application interface; its reach ends at the Endpoint storage boundary.
- The Endpoint API is a portability contract, so WireSpaces-facing Service code is portable across comparable stacks.
- An Endpoint holds declared metadata plus payload, copied at acceptance; reading source metadata authorizes no transmission.
- One Service writes a transmit Endpoint; one reads a Queue Endpoint; any number read a Snapshot Endpoint.
- An Endpoint Domain provides implementation-defined serialization for concurrent writers.
- Every transmit Endpoint has exactly one consuming Wire binding; fan-out is splicing or gateway work.
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
- Little-endian serialization for literal multi-byte numeric values; MSB-first field order within a byte.
- Canonical `Control` and `RoutingWord` bit placement fixed, with CAN `PduControl` aligned on their shared six bits.
- Priority occupies the most significant available bits, so a numerically lower QoS always wins.
- Reserved fields are rejected on receive, not ignored.
- A header extension block is self-describing in length, so a parser can skip what it does not know.
- Exactly one binding mode per Endpoint registration; reply authority is never inferred from Direction.
- A Service contract is a schema over bytes; generated language types are views.
- Protocol version, compatibility fingerprint, and build identity are three separate identities.
- Storage semantics, ownership representation, and writer concurrency are independent axes and never substitute.
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
- Whether a small default Queue capacity is offered, or every Queue Endpoint must declare its depth.
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
| Literal multi-byte serialization | little-endian; MSB-first fields within a byte (`BITS §1`) |
| `Control` packing | QoS 7..6, Namespace 5..4, HasHeaderExtensions 3, TransportType 2..0 |
| `RoutingWord` packing | NodeId 15..11, Direction 10, WireNumber 9..0 |
| CAN `PduControl` | `EndpointId[9:8]` 7..6, then identical to `Control` bits 5..0 |
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
| Endpoint delivery | bounded storage boundary; no synchronous Service execution |
| Endpoint storage semantics | Queue (history-preserving) or Snapshot (latest-value); one per Endpoint |
| Endpoint writer concurrency | single writer, or serialized concurrent writers |
| Queue capacity | declared per Endpoint; default-capacity policy open |
| Snapshot metadata | generation counter and arrival time always carried |
| Delivered metadata | declared per Endpoint: payload only, with source, or full |
| Metadata storage cost | ~3 B packed descriptor + ~4 B arrival; copied, never viewed |
| Snapshot TX feedback | `last_sampled_generation`, `last_sent_generation`; 16-bit wrapping |
| Transmit Endpoint fan-out | exactly one consuming Wire binding |
| Endpoint binding modes | Static, Learned-from-ingress, Request-scoped, Receive-only, Transmit-only |
| Scheduling disciplines | `StrictPriority`, `WeightedFair` |
| Storage axes | semantics, ownership representation, writer concurrency (independent) |
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
| Service-to-application interface | undefined by WireSpaces; never appears in a PDU |
| Endpoint API | intended portability contract for Service-facing code |
| Service portability | WireSpaces-facing code portable within a declared resource/timing envelope |
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
| `PDU-4` | **Literal multi-byte numeric values serialize little-endian, and fields within a byte are ordered MSB-first** (`BITS §1`). Native object layout — including C/C++ bitfield allocation order — is never a wire representation. |
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

## 4.7 Dispatch and Endpoints — `DISP`

| ID | Invariant |
|---|---|
| `DISP-1` | **Autonomous Service TX uses an injected transmit Endpoint, not a hard-coded Wire.** |
| `DISP-2` | **Endpoint delivery never synchronously executes Service application code.** Caller-context routing and dispatch are permitted; caller-context execution of destination Service code is not. Acceptance performs only bounded framework work and captures arrival time. |
| `DISP-3` | **WireSpaces defines no Service-to-application interface**, and none is visible on the wire. Its reach ends at acceptance into Endpoint storage on receive, and begins at acceptance into a transmit Endpoint; a Service may not inject work back across that boundary. |
| `DISP-4` | **Exactly one externally visible producer per `(Endpoint Domain, Namespace, EndpointId)`.** Several serialized writer contexts within one Service may sit behind it; they do not become separately addressable sources. |
| `DISP-5` | **Endpoint identity provides naming, not authority.** Wires, bindings, and typed local access provide authority; a Link or Wire reaching a device exposes no more of its Endpoint namespace than it is configured to carry. |
| `DISP-6` | **An Endpoint owns exactly one storage element, and its semantics and writer concurrency are immutable properties of the Service definition.** Deployment, dispatch, and generated code neither add nor remove them, so they need no preservation mechanism. |
| `DISP-7` | **Each Endpoint registration declares exactly one binding mode**, and **reply authority is never inferred by reversing Direction.** An Endpoint needing both request-scoped replies and autonomous transmission uses separate registrations. |
| `DISP-8` | **Learned-from-ingress binding is disabled on unauthenticated multi-access Links**, and where enabled is bounded, auditable, and unable to create a Wire, broaden authority, reinterpret an alias, or authorize another Transport. |
| `DISP-9` | **No profile may force arbitrary application code into a Link driver's or LLL's execution context**, and no callback ABI is ever mandatory. Strengthened by `DISP-2`, which removes synchronous Service execution from delivery entirely; any notification mechanism is layered above storage and optional. |
| `DISP-10` | **One Service writes a transmit Endpoint; one Service reads a Queue Endpoint; any number of Services read a Snapshot Endpoint.** Framework producers are not Services, so several Link drivers may write one receive Endpoint. Multi-reader Queues do not exist. |
| `DISP-11` | **Every transmit Endpoint has exactly one consuming Wire binding.** Publishing identical state onto several Wires is splicing or gateway forwarding, never Endpoint fan-out. |
| `DISP-12` | **A Snapshot Endpoint always carries a generation counter**, and a reader's "have I seen this" watermark lives in the reader, never in the Endpoint. |
| `DISP-13` | **An Endpoint Domain provides whatever mechanism serializes concurrent writers to its Endpoints.** The property is required; the primitive is implementation-defined, and the serialized region is bounded, allocation-free, and free of application code. |
| `DISP-14` | **An Endpoint holds declared metadata plus payload, copied at acceptance.** Source, class, extension, and arrival information is available where the Endpoint declares it; a view into ingress storage is never valid, because the consumer reads after that storage is reclaimed. |
| `DISP-15` | **Reading source metadata confers no transmit authority.** A reply is authorized by the receiving Endpoint's registration, never by the arrival of a message whose Wire and NodeId the Service can read. Source fields may select among peers a registration already permits; they may never widen that set. |

## 4.8 Buffer ownership — `OWN`

| ID | Invariant |
|---|---|
| `OWN-1` | **Copy-based buffer ownership is the first-class baseline.** A successful `send()` means the caller may reuse its buffer; it does not mean delivery. |
| `OWN-2` | **Every storage element on a delivery path is bounded, with a chosen exhaustion behavior.** This includes reassembly contexts, retry windows, and observation buffers. Observation never degrades delivery. |
| `OWN-3` | **Ownership transfer is explicit and traceable.** A view is not ownership; a rejected submission leaves ownership with the caller; an accepted owning submission transfers it exactly once; storage is not mutated after publication. |
| `OWN-4` | **Every accepted PDU reaches exactly one terminal local outcome:** transmission complete, cancelled by stop or restart, or a terminal local fault. |
| `OWN-5` | **Storage semantics, ownership representation, and writer concurrency are independent axes, and no position on one substitutes for another.** A Snapshot replacement is not a Queue delivery, a value copy is not ownership transfer, and Queue exhaustion never becomes latest-value replacement. Copyless delivery changes only the ownership axis. |
| `OWN-6` | **A Snapshot transmit publication is not a submission.** Ownership and terminal-outcome rules govern the PDUs an LLL generates from samples, not the publications a Service overwrites. |

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
| `LINK-11` | **One serialized mutable execution context per LLL instance** owns its parser, reassembly, transmit scheduling, timers, and pools. Others reach it only through bounded queues, snapshots, or explicitly synchronized control requests; a seqlock requires a serialized writer. |
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
| `SVC-9` | **The Endpoint API is a portability contract, not an implementation detail.** A Service's WireSpaces-facing code should compile and behave identically across implementations on comparable stacks, bounded only by its declared resource and timing envelope and by its non-WireSpaces dependencies. The stack beneath the Endpoint API and the interface above the Service are both free to differ. |

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
- the older adapter draft's Namespace-0 allocation (`1..32` public, `33..991` deployment, `992..1023` public General-only). The current working plan is `LINK §2.4`.
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
- **`Inline` delivery, and the `Inline`/`Serialized` delivery-policy pair.** Endpoint delivery is now always a bounded storage operation (`DISP-2`). This one will be proposed again, because a direct call is visibly cheaper than a queue and the argument for it is always "we measured it and it is faster." The reason it is refused is not performance: **`Inline` makes a Link's worst-case execution time depend on every Service that might be delivered to,** so the Link cannot be analyzed in isolation and its worst case changes when a deployment adds a Service its author never saw. It also makes message topology into call topology, allows a Service to reenter itself by transmitting during a receive, and has no meaning at all in RTL. The latency it saved was typical-case latency bought with an unbounded worst case.
- **Delivery policy as a deployment-preserved property.** `DISP-2` used to promise that a declared policy survived a placement change, which implied there was something to preserve. Storage semantics are now fixed by the Service definition (`DISP-6`), so nothing is configurable and nothing needs preserving. Any tooling check written to verify policy preservation is checking a property that can no longer vary.
- **The four-storage-class taxonomy** (snapshot, value queue, ownership-transfer queue, event queue). It conflated storage semantics with ownership representation, and "event queue" was a semantic duplicate of "value queue." Refactored into three independent axes (`OWN-5`), which is what makes the copyless path a change on one axis rather than a new set of classes.
- **`Port` as an architectural term.** It named the local typed interface at a Service boundary as distinct from the network-visible Endpoint. That distinction was real only while delivery meant invoking a handler; bounded storage makes the object the Service holds and the object the network addresses the same one (`CORE §1.6`). The term survived as a classification that constrained nothing. What it carried is preserved by `DISP-3`: a Service's local interface never appears in a PDU, and restructuring it is not a protocol change. `TxBinding` became a transmit Endpoint.
- **Endpoint-level transmit fan-out** ("one Endpoint implementation may source several Wires"). Superseded by `DISP-11`. It would have forced per-reader sampled/sent state into every Snapshot transmit Endpoint, and the capability already exists one layer down as splicing or gateway forwarding, both of which preserve source lineage.
- **Multi-reader Queue Endpoints** ("several local consumers may read one Endpoint where its contract permits", as applied to queues). Superseded by `DISP-10`. Draining is destructive, so two consumers silently split the stream — it passes every test with one consumer and loses messages in production. It is a worker-pool construct with no RTL meaning, and it will be asked for again as a "load-balanced handler pool."

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
- Header extension internal field placement, once any extension is defined. `Control` and `RoutingWord` bit positions are now settled (`BITS §2`–`§3`); the CAN identifier is not (`§6.8`).

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

## 6.12 Endpoint API and composition

- Final type and API names. `QueueEndpoint<T, N>` and `SnapshotEndpoint<T>` are working names only, and they are misleading in one respect worth fixing before they stick: **a slot is declared metadata plus payload** (`DISP-14`), so `T` names the payload representation rather than the slot. `T` is also the **decoded local representation**, never the wire contract — the schema is (`SVC-7`), and naming that lets `T` look like the wire type is how a messaging layer accidentally becomes ABI-dependent. These names are higher-stakes than usual because the Endpoint API is a portability contract (`SVC-9`), so they are the surface Services get written against rather than an implementation choice.
- How delivered metadata is declared and accessed: how the three levels are named and selected, whether recognized extensions are exposed decoded or as bytes, and what an accessor returns when a Service reads a field its Endpoint did not declare — a compile error is preferable to a zero.
- How much of the Endpoint API `SVC-9` actually fixes, and how it is verified. Candidates: a documented signature set, a header a conforming implementation must satisfy, or a portability test suite that compiles the same Service against two implementations. Nothing is decided, and without one of them the contract is an intention rather than a check.
- Whether a small documented default Queue capacity is offered, or every Queue Endpoint declares its depth. A default in general builds with profiles able to require explicit capacity is the likely compromise; prototype experience should decide.
- Whether a narrowly scoped synchronous hook is ever permitted for instrumentation, tracing, or platform scheduling notifications. Framework arrival-timestamping at acceptance is settled (`DISP-2`); **application-supplied hooks are not in prospect**, since an unrestricted user callback recreates what `Inline` was withdrawn for. If one is ever considered it must answer: who may supply it, before or after acceptance, may it inspect payload, may it transmit, may it block, what bound applies, and whether a dedicated observation Endpoint consumed asynchronously would do instead.
- Whether transmit direction keeps its own vocabulary. `TxBinding` became a transmit Endpoint, but it still carries authority that receive storage does not, which may justify a distinct name.
- Exact Snapshot memory ordering: atomics, barriers, seqlock retry rules, and the interaction between the value generation and the sampled/sent echo.
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
- The exact API representation of a reply context: storage, lifetime, correlation, invalidation, and the retained-handle form needed for deferred responses (`CORE §10.6`).
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
| `WS_old/network/prototype_and_validation.md` | 1,460 | `CONFORM`, possibly `code/sim_rfp.md` |
| `WS_old/network/rationale_use_cases_and_risks.md` | 510 | `INTRO`, `FUTURE` |

The copy of `can_pdu_adapter_spec.md` that sat in this directory was byte-identical to the `WS_old` one, so mining the latter covered both. It has moved to `archive/`.

`link_engine_runtime_and_status.md` is now the most valuable remaining source, because `CORE §13.3` and `OWN-4` opened questions about restart and lifecycle that it was written to answer.

The mining method that has worked so far: read for concepts that were *dropped* rather than *superseded*, recover the ones that still hold, record the rejections in §5 so they cannot leak back, and note deliberate divergences rather than silently overriding them.

---

# 8. Revision History

Current revision: **0.9** (bounded Endpoint delivery, Snapshot semantics, the retirement of `Port`, and canonical descriptor packing — see `HIST §1`).

Full revision narrative and superseded-source provenance live in [history.md](history.md). `REG` keeps only status; `HIST` is archival and is not part of the control surface.

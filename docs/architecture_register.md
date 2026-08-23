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

The following areas are settled at the architectural level. Numeric limits, field widths, and profile specifics are in §3; normative rules with stable IDs are in §4; detail is in `CORE`.

**Model and identity**

- One deployment-scoped `ParticipantId` per Endpoint Domain, used unchanged on every Wire.
- A Wire is a loop-free Logical Bus propagation domain. Wires may overlap on Physical Links; ordinary forwarding follows configured Wire topology, while destination controls acceptance.
- Canonical source and destination Participant identity; no canonical Origin, Node, NodeId, or Direction. Endpoint identity is naming — authority comes from Wires and bindings.
- Wire scopes include ordinary named Wires, device-private Wires, canonical local-only `kLocalBus`, `kLocalDomain`, and the deployment identity universe.

**Delivery and Endpoints**

- Bounded storage delivery; no synchronous Service execution; Queue vs Snapshot; copy-based ownership baseline.
- Declared metadata copied at acceptance; one producer per external Endpoint identity; one Wire binding per transmit Endpoint.
- No Service-to-application interface; Endpoint API as portability contract for Service-facing code.

**QoS, congestion, and bounds**

- QoS Minimal/Full; congestion as a normal send outcome; every delivery path bounded with chosen exhaustion behavior.

**Links and profiles**

- Exactly one statically selected profile per Link Binding; traffic never auto-detects or negotiates it; master-initiated/polled Links remain supported.
- Three CAN11 profiles: Guest VCN, Native VCN, and Native Participant-Compressed. Every CAN11 Link Binding carries exactly one Wire and reconstructs canonical identity before Router/dispatch.
- Four-phase commissioning direction and transmit ownership in all phases.

**Forwarding and gatewaying**

- Caller-context routing over read-mostly tables; complete PDU as forwarding unit; flood-and-filter baseline.
- Plain forwarding preserves canonical identity and does not merge independent Participant identity universes.
- A splice is the explicit Wire-scope projection: it changes Wire scope while preserving source, destination, Endpoint, applicable control/extensions, and payload.

**Lifecycle, telemetry, and discipline**

- Separate Link and restart-unit lifecycles; runtime generation; bounded quiesce; escalation; declared isolation.
- Live vs latched telemetry; five distinguishable value states; diagnostic containment; one telemetry Service per Domain.
- Prototype generates evidence, does not close open questions (`CONFORM §1.1`); capability claims require tests.

**Security and tooling posture**

- Privileged capabilities require separate build; promiscuous observation never creates Wiring; ephemeral auto-Wiring must announce itself.
- Malformed/unauthorized traffic counted and dropped with no wire response.

**Serialization and contracts**

- Preferred, still-provisional 48-bit / 6-byte canonical descriptor: `Control`, `Wire`, `SrcPID`, `DestPID`, and `Endpoint`; little-endian multi-byte values; self-describing header extensions; reserved fields rejected.
- Service contract is schema over bytes; protocol version, fingerprint, and build identity are separate.
- Structural validity always enforced; contract-level policy optional; composition must flatten.

**Maturity**

- Usage levels 0–5; Levels 0–1 protected from advanced-feature complexity; 29-bit CAN as planned escape hatch.

## 2.2 Provisional implementation direction

- The 8-bit `WireNumber` and `ParticipantId` budgets, including exact Wire allocation fences, pending a representative topology/headroom corpus.
- Exact numeric values reserved for `kLocalBus`, `kLocalDomain`, and device-private Wire ranges.
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
- The Critical-QoS PDUA depth policy and its default value.
- Reassembly context pooling strategy and its bound on a multi-Link gateway.
- The configuration compatibility-check mechanism's computation and generic scope. VCN/projection fingerprint coverage, atomic activation, and reassembly flush are explicit TODOs, not settled requirements.
- CAN11 commissioning payload and state-machine details.
- Exact CAN11 `PduControl`, optimized-N1, aggregate-CRC, and CAN29 packing.
- Guest fixed-QoS configuration details beyond rejecting TX whose canonical QoS does not match the binding.
- Native VCN allocation policy and VCN table storage representation.
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
| Automatic producer failover/election | — (not a base Wire feature) |
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
| Canonical base descriptor | preferred provisional 48 bits / 6 bytes; widths remain subject to topology-corpus validation |
| Canonical header incl. ordinary extensions | ~9 bytes target: 6 base + up to 3 extension |
| Literal multi-byte serialization | little-endian; MSB-first fields within a byte (`BITS §1`) |
| `Control` packing | QoS 7..6, Reserved 5..4, HasHeaderExtensions 3, TransportType 2..0 |
| Base descriptor order | `Control[8]`, `Wire[8]`, `SrcPID[8]`, `DestPID[8]`, `Endpoint[16]` |
| Header extension length | self-describing; 2-bit code for 1..3 bytes (provisional) |
| Reserved fields | zero on TX, dropped and counted on RX |
| Endpoint | 16 bits: Namespace 15..14 + Id 13..0 |
| Namespace | 2 bits / 4 spaces |
| Endpoint Id | 14 bits per Namespace; 0 invalid |
| FOSS ecosystem Namespace | Namespace 3 |
| TransportType | 3 bits / up to 8 values |
| QoS | 2 bits / 4 classes: Critical 0, High 1, Normal 2, Background 3 |
| QoS profiles | QoS-Minimal (Normal only), QoS-Full (all four) |
| ParticipantId | 8 bits; `0x00..0xFE` ordinary |
| Broadcast ParticipantId | `0xFF`, destination only and invalid as source |
| Participant identity rule | exactly one per Endpoint Domain in a deployment identity universe; same PID on every Wire |
| WireNumber | preferred provisional 8 bits; allocation fences not frozen |
| Device-private Wires | range provisional; never escape unchanged |
| `kLocalDomain` | canonical local scope; exact value provisional; never enters a Link Interface |
| `kLocalBus` | canonical local-only Wire; exact value provisional; at most one local Link binding; never forwarded or spliced as itself |
| Canonical Direction/Origin/Node/NodeId | none |
| Wire semantics | loop-free Logical Bus propagation domain; Wires may overlap |
| Forwarding selection | configured Wire topology, not destination next-hop routing |
| Destination semantics | Participant acceptance address; `local PID` or broadcast accepts |
| CAN11 profile selection | exactly one static profile per Link Binding; never auto-detected or negotiated |
| CAN11 Wire mapping | all WS traffic under one Link Binding maps to exactly one Wire; Wire is elided and reconstructed from the binding |
| CAN11 Guest VCN | aligned 16-ID block; VCN 3 + Direction 1; fixed QoS; all-ones VCN reserved, leaving 7 ordinary VCNs |
| CAN11 Native VCN | QoS 2 + VCN 8 + Direction 1; all-ones VCN reserved, leaving 255 ordinary VCNs |
| CAN11 VCN semantics | Link-local `VCN -> {ParticipantA, ParticipantB}`; self-pairs, duplicate unordered participant-pair maps, and duplicate broadcast-source maps prohibited |
| CAN11 Guest QoS | reconstructed from Link Binding; TX rejects a canonical QoS mismatch |
| CAN11 Native Participant-Compressed | QoS 2 + Compact 3 + General 5 + Direction 1 |
| CAN11 Participant compression | direct mapping by default; optional Link-local projection to canonical PID |
| CAN11 General code 31 | broadcast in the valid compact-source direction; opposite direction reserved for Link control/commissioning |
| CAN canonicalization boundary | Wire elision/projection reconstructs canonical Wire, source, and destination before Router/dispatch |
| CAN `PduControl` / optimized N1 | exact packing provisional |
| CAN aggregate CRC | exact algorithm, coverage, placement, and byte order provisional |
| CAN PDUA max frames | 8 (N <= 4 normal target; stricter for Critical QoS) |
| CAN reassembly key | `(ingress Link, complete CAN identifier)`, one active context each |
| Generic gateway unit | complete canonical WS PDU |
| Gateway baseline | propagation over configured loop-free Wire topology |
| Plain-forwarding identity scope | one coordinated Participant identity universe; independent universes are not merged |
| Wire representation change | explicit splice only; preserves src/dest/Endpoint/control/extensions/payload |
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
| Producer ownership | one visible producer per `(ParticipantId, Endpoint)` |
| Rejection behavior | counted and dropped; no error response emitted |
| Flow control | optional, per Link profile; 2 B or 8 B credit extension |
| Future CAN growth path | 29-bit Classical CAN profile planned; layout unspecified |
| Usage maturity model | Levels 0-5 (`INTRO §6`); Levels 0-1 must stay easy |
| Promiscuous/bring-up mode | host tooling and gateways only; never creates Wiring |
| Identity scope | one deployment Participant identity universe; joining independent universes needs coordinated assignment or explicit translation/composition |
| Service-to-application interface | undefined by WireSpaces; never appears in a PDU |
| Endpoint API | intended portability contract for Service-facing code |
| Service portability | WireSpaces-facing code portable within a declared resource/timing envelope |
| Baseline Transport | Unreliable Datagram + receiver freshness handling |
| Next Transport candidate | Sequenced / end-to-end-protected datagram (`FUTURE §3.1`) |
| Service contract | canonical schema over bytes; generated types are views |
| Service message prefix | `u8 protocol_version`, `u8 message_type` (recommended default) |
| Master-initiated Links | I2C, SPI in scope; LLL polls without becoming producer |
| Privileged capabilities | separate build required; config cannot enable them |
| Default restart unit | the LLL instance; grouping permitted and declared |
| Runtime generation | changes on every restart; nothing compared across a change |
| Recovery escalation | driver → Link → restart unit → subsystem → device |
| Restart isolation | assumed independent unless a dependency is declared |
| Durability boundaries | two: restart-unit reconstruction, and power loss |
| Telemetry lifetimes | live (current generation) and latched (survives restart) |
| Telemetry value states | valid, stale, unavailable, not applicable, omitted |
| Telemetry Services | one per Endpoint Domain; local and network faces, one model |
| Telemetry schema classes | Compact / Standard / Extended, fixed schema per version |
| Capability claims | required / optional-enabled / optional-disabled / unsupported |
| Interoperability | not claimed |
| RTL support | future first-class target |

---

# 4. Core Architectural Invariants

Particularly important when generating code or designs from these documents. Cite by ID.

## 4.1 Wire model — `WIRE`

| ID | Invariant |
|---|---|
| `WIRE-6` | **A Wire is a loop-free Logical Bus: a configured logical propagation domain realized by one or more Links.** It is not synonymous with a cable, bus, LAN, process, or device. |
| `WIRE-7` | **A PDU injected onto a Wire is logically propagated across that Wire's configured Link topology.** `DestParticipantId` controls acceptance rather than ordinary next-hop forwarding. |
| `WIRE-8` | **Multiple Participants may independently source traffic on one Wire.** No permanent controller, Origin, or Node role is implied. |
| `WIRE-9` | **Several Wires may overlap on the same Physical Links.** Each remains an ordinary Wire with its own identity and configured membership. |
| `WIRE-10` | **Prefer the smallest Wire that usefully represents the required communication or broadcast scope.** A broad Wire is legal, but its traffic may consume capacity on every segment in that Wire. |

## 4.2 Scope and identity — `SCOPE`

| ID | Invariant |
|---|---|
| `SCOPE-1` | **Device-private Wires never leave the device as device-private WireNumbers.** The only way their traffic reaches an external Link is an explicitly configured splice to a network-visible Wire. |
| `SCOPE-2` | **`kLocalDomain` never enters a Link Interface and is never spliced.** |
| `SCOPE-3` | **Long FPGA physical paths do not imply more Wires.** |
| `SCOPE-4` | **Physical proximity does not imply `kLocalDomain` or a device-private Wire.** |
| `SCOPE-5` | **Plain forwarding never merges independently assigned Participant identity universes.** A Wire spanning several Links assumes one coordinated canonical ParticipantId universe; connecting independent universes requires coordinated assignment or explicit translation/composition. |
| `SCOPE-6` | **An Endpoint Domain is a logical dispatch and authority boundary.** It is not a synonym for a device, core, or process, and it is not a security boundary without a real protection mechanism. |

## 4.3 Participant and local-only identity — `SCOPE`

| ID | Invariant |
|---|---|
| `SCOPE-7` | **Every Endpoint Domain has exactly one ParticipantId in a deployment identity universe and uses that same identity on every Wire.** Distinct Endpoint Domains have distinct PIDs; `0x00..0xFE` are ordinary, and `0xFF` is broadcast destination only and invalid as source. |
| `SCOPE-8` | **Participant identity is separate from physical-device identity.** A device may contain several Endpoint Domains, and replacement hardware may assume a deployment Participant assignment. |
| `SCOPE-9` | **`kLocalBus` is a canonical local-only WireNumber bound to at most one local Link Interface in a Router/Endpoint Domain.** |
| `SCOPE-10` | **A PDU on `kLocalBus` is fully canonical and may be dispatched locally, but it is never transparently forwarded or spliced as `kLocalBus`.** |

## 4.4 Splicing — `SPLICE`

| ID | Invariant |
|---|---|
| `SPLICE-1` | **A splice applies before egress and after ingress canonicalization.** It is the explicit configured Wire-scope projection: only Wire scope changes; canonical source, destination, Endpoint, applicable control metadata/extensions, and payload are preserved. |
| `SPLICE-2` | **At most one splice per local routing step.** No chained or recursive splices. |
| `SPLICE-5` | **A splice does not merge independent Participant identity universes or silently resolve ParticipantId collisions.** |

## 4.5 PDU integrity — `PDU`

| ID | Invariant |
|---|---|
| `PDU-1` | **The complete canonical PDU is the generic Router/gateway forwarding unit.** |
| `PDU-2` | **No Endpoint truncation, implicit aliasing, or silent remapping.** An unrepresentable canonical value fails placement or TX before a frame is emitted. |
| `PDU-3` | **No partial PDU is ever visible above the LLL.** |
| `PDU-4` | **Literal multi-byte numeric values serialize little-endian, and fields within a byte are ordered MSB-first** (`BITS §1`). Native object layout — including C/C++ bitfield allocation order — is never a wire representation. |
| `PDU-5` | **Reserved fields are rejected, not ignored.** Zero on transmit; a nonzero reserved field is dropped and counted rather than masked away. |
| `PDU-6` | **A header extension block states its own total size.** A parser can locate the payload without understanding the extension's contents. |
| `PDU-7` | **The canonical base descriptor carries Control, Wire, SrcParticipantId, DestParticipantId, and Endpoint.** Its preferred provisional serialization is 48 bits / 6 bytes; field-width freeze awaits topology-corpus evidence. |
| `PDU-8` | **Origin, Node, NodeId, and Direction are not canonical PDU semantics.** A constrained Link may carry a local Direction bit only to reconstruct canonical source and destination. |

## 4.6 Routing and forwarding — `ROUTE`

| ID | Invariant |
|---|---|
| `ROUTE-1` | **No generalized RouterPort abstraction is currently required.** |
| `ROUTE-2` | **The Router should not require a central routing task.** Caller-context concurrent routing against read-mostly state is preferred. |
| `ROUTE-3` | **Failure on one egress does not cancel other successful egresses.** |
| `ROUTE-4` | **Ordinary transparent Wire realization is loop-free**, including through splices. Cyclic/redundant profiles require an explicit additional mechanism. |
| `ROUTE-5` | **Electrical visibility grants nothing.** Attachment to a medium confers no membership, delivery, forwarding, or transmit authority; only configuration does. |
| `ROUTE-6` | **Every forwarding mechanism preserves source lineage.** Re-originating traffic is not forwarding: it makes the re-originator the authoritative producer, and requires its own Endpoint identity and configured authority. |
| `ROUTE-7` | **Ordinary forwarding is based on canonical Wire identity, ingress context, and configured Link topology.** Destination-based early filtering is permitted only when semantically equivalent; it does not redefine Wire propagation. |

## 4.7 Dispatch and Endpoints — `DISP`

| ID | Invariant |
|---|---|
| `DISP-1` | **Autonomous Service TX uses an injected transmit Endpoint, not a hard-coded Wire.** |
| `DISP-2` | **Endpoint delivery never synchronously executes Service application code.** Caller-context routing and dispatch are permitted; caller-context execution of destination Service code is not. Acceptance performs only bounded framework work and captures arrival time. |
| `DISP-3` | **WireSpaces defines no Service-to-application interface**, and none is visible on the wire. Its reach ends at acceptance into Endpoint storage on receive, and begins at acceptance into a transmit Endpoint; a Service may not inject work back across that boundary. |
| `DISP-4` | **Exactly one externally visible producer per `(Endpoint Domain, Namespace, EndpointId)`.** Several serialized writer contexts within one Service may sit behind it; they do not become separately addressable sources. |
| `DISP-5` | **Endpoint identity provides naming, not authority.** Wires, bindings, and typed local access provide authority; a Link or Wire reaching a device exposes no more of its Endpoint namespace than it is configured to carry. |
| `DISP-6` | **An Endpoint owns exactly one storage element, and its semantics and writer concurrency are immutable properties of the Service definition.** Deployment, dispatch, and generated code neither add nor remove them, so they need no preservation mechanism. |
| `DISP-7` | **Each Endpoint registration declares exactly one binding mode**, and **reply authority is never inferred merely by swapping canonical source and destination.** An Endpoint needing both request-scoped replies and autonomous transmission uses separate registrations. |
| `DISP-8` | **Learned-from-ingress binding is disabled on unauthenticated multi-access Links**, and where enabled is bounded, auditable, and unable to create a Wire, broaden authority, reinterpret a Link-local representation, or authorize another Transport. |
| `DISP-9` | **No profile may force arbitrary application code into a Link driver's or LLL's execution context**, and no callback ABI is ever mandatory. Strengthened by `DISP-2`, which removes synchronous Service execution from delivery entirely; any notification mechanism is layered above storage and optional. |
| `DISP-10` | **One Service writes a transmit Endpoint; one Service reads a Queue Endpoint; any number of Services read a Snapshot Endpoint.** Framework producers are not Services, so several Link drivers may write one receive Endpoint. Multi-reader Queues do not exist. |
| `DISP-11` | **Every transmit Endpoint has exactly one consuming Wire binding.** Publishing identical state onto several Wires is splicing or gateway forwarding, never Endpoint fan-out. |
| `DISP-12` | **A Snapshot Endpoint always carries a generation counter**, and a reader's "have I seen this" watermark lives in the reader, never in the Endpoint. |
| `DISP-13` | **An Endpoint Domain provides whatever mechanism serializes concurrent writers to its Endpoints.** The property is required; the primitive is implementation-defined, and the serialized region is bounded, allocation-free, and free of application code. |
| `DISP-14` | **An Endpoint holds declared metadata plus payload, copied at acceptance.** Source, class, extension, and arrival information is available where the Endpoint declares it; a view into ingress storage is never valid, because the consumer reads after that storage is reclaimed. |
| `DISP-15` | **Reading source metadata confers no transmit authority.** A reply is authorized by the receiving Endpoint's registration, never by the arrival of a message whose Wire and SrcParticipantId the Service can read. Source fields may select among peers a registration already permits; they may never widen that set. |

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
| `LINK-4` | **Classical CAN Participants are not required to implement generic LLL credit flow control.** |
| `LINK-5` | **Classical CAN PDUA MaxN is 8; N <= 4 is the normal target.** |
| `LINK-8` | **Exactly one Link profile is selected statically per Link Binding.** Traffic never auto-detects or negotiates framing, encoding, or profile family at runtime. |
| `LINK-9` | **Local scheduling is not a bus-wide guarantee.** An in-progress transmission unit is non-preemptible and controller capability bounds achievable ordering; latency bounds come from deployment analysis, not from queues. |
| `LINK-10` | **Where a medium requires transmit ownership, it holds in every configured state**, including every commissioning phase. |
| `LINK-11` | **One serialized mutable execution context per LLL instance** owns its parser, reassembly, transmit scheduling, timers, and pools. Others reach it only through bounded queues, snapshots, or explicitly synchronized control requests; a seqlock requires a serialized writer. |
| `LINK-12` | **Hop integrity and end-to-end integrity are different claims.** A gateway that reassembles and re-encodes computes a fresh check value, so per-hop verification does not cover the path. Neither is authentication. |
| `LINK-13` | **Validation precedes parsing**, and a stabilized profile is immutable — an incompatible change takes a new name or version. |
| `LINK-14` | **Every CAN11 Link Binding carries exactly one canonical Wire, and all WS CAN11 traffic under that binding maps to it.** WireNumber is elided on the carrier and reconstructed from the binding. |
| `LINK-15` | **A CAN11 VCN is a Link-local configured participant relationship.** Direction selects its canonical source and destination; self-pairs, duplicate unordered participant-pair mappings, and duplicate broadcast-source mappings are invalid. |
| `LINK-16` | **Guest VCN uses an aligned 16-ID allocation with VCN[3] + Direction[1], fixed binding QoS, and the all-ones VCN reserved.** Seven ordinary VCNs remain; TX rejects a canonical QoS mismatch. |
| `LINK-17` | **Native VCN uses QoS[2] + VCN[8] + Direction[1], with the all-ones VCN reserved.** 255 ordinary VCNs remain. |
| `LINK-18` | **Native Participant-Compressed uses QoS[2] + Compact[3] + General[5] + Direction[1].** Direct canonical-PID mapping is the default; optional projection is Link-local representation only, and every ordinary addressed PDU has at least one compact participant. |
| `LINK-19` | **Participant-Compressed General code 31 is reserved for broadcast/control.** Compact-to-General carries ordinary broadcast from a compact source; the opposite direction is Link-control/commissioning space. |
| `LINK-20` | **Every ingress Link representation reconstructs unambiguous canonical Wire, source, and destination before Router/dispatch.** Egress may elide or project only fields uniquely recoverable from the Link Binding and frame context. |

## 4.11 Configuration and authority — `CFG`

| ID | Invariant |
|---|---|
| `CFG-1` | **Static traffic/capacity checking is preferred over adding a complex adaptive congestion protocol to the core.** |
| `CFG-2` | **Do not require static Manifests, Flows, or WireContracts for basic communication.** |
| `CFG-3` | **Structural validity is always enforced; contract-level policy is always optional.** The two lists are in `CORE §19.1`. |
| `CFG-4` | **Promiscuous observation never creates persistent Wiring**, and is a host-tooling/gateway capability rather than ordinary Participant behavior. |
| `CFG-5` | **Ephemeral auto-Wiring must announce itself** and must never be silently promoted to authoritative configuration. |
| `CFG-6` | **Commissionability is independent of routing capability.** A LocalBus-only or N=1-only Participant may still accept ParticipantId, Wire, and profile configuration. |
| `CFG-7` | **A gateway reports local facts; the Organizer decides.** Gateways do not infer or repair routes. |
| `CFG-8` | **Privileged capabilities must be absent from normal builds.** Runtime configuration alone must never enable one. |
| `CFG-9` | **No wire interoperability is claimed.** Independent implementations are not presumed compatible without shared definitions or common vectors. |
| `CFG-10` | **Generated artifacts carry a configuration compatibility check, and incompatible projections fail closed.** A mismatch is reported and never repaired or guessed locally; exact fingerprint coverage and activation mechanics remain open. |
| `CFG-11` | **An unconfigured Participant emits no ordinary Service traffic.** Before commitment it holds no Endpoint authority and participates only in Link commissioning control. |
| `CFG-12` | **Acceptance is set-valued and the constraints are coupled.** A tuple may be rejected even when every field is individually admitted; per-field validation is insufficient at both configuration and runtime. |
| `CFG-13` | **One authoritative Wiring source generates every projection.** Software, RTL, static, and host artifacts come from one source, and projections that disagree fail closed. |

## 4.12 Services, Transport, and composition — `SVC`

| ID | Invariant |
|---|---|
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
| `ERR-4` | **Live and latched status have different lifetimes, and a static live snapshot is not evidence of health.** Latched fault and restart records remain readable from outside a failed runtime. |
| `ERR-5` | **Unavailable, stale, not-applicable, and schema-omitted values are distinguishable from an ordinary zero or healthy value.** A telemetry field is never fabricated to fill a schema slot. |

## 4.14 Runtime lifecycle and restart — `RUN`

| ID | Invariant |
|---|---|
| `RUN-1` | **A Link's lifecycle is separate from that of the restart unit owning its state.** Both support explicit start, stop, reset, enable, and disable; lifecycle control serves power management, maintenance, and commissioning, not only faults. |
| `RUN-2` | **Every mutable state item has exactly one owning restart unit.** The LLL instance is the default, following its single serialized mutable context (`LINK-11`); grouping several Links into one unit is permitted and declared. |
| `RUN-3` | **A restart unit owns local Link state and local forwarding availability, never the Wires realized through it.** Restarting it does not redefine a Wire, change canonical Participant identity, or transfer producer authority. |
| `RUN-4` | **A runtime generation changes on every restart, and no observation may be compared or combined across a change in it.** Its width and wrap behavior are declared; it is distinct from an Endpoint snapshot generation. |
| `RUN-5` | **Stopping is bounded.** A quiesce deadline and forced-cancellation behavior are declared, and every accepted transmit interrupted by stop or restart reaches a defined terminal outcome. |
| `RUN-6` | **Transient state whose validity cannot be established is discarded, not reconstructed from partial evidence.** The resulting loss is counted; continuity across a restart is an end-to-end concern. |
| `RUN-7` | **Recovery escalates from the smallest affected scope upward**, under declared attempt limits and deadlines, and never becomes a busy loop or a diagnostic storm. |
| `RUN-8` | **Restart isolation is a declared claim.** Sibling Links continue through another's restart unless a shared dependency is declared in configuration. |
| `RUN-9` | **"Persistent" means surviving a restart unit's reconstruction, not surviving power loss.** Every counter and record declares which boundary applies, and storage backing application-owned buffers outlives the Link that filled it. |
| `RUN-10` | **A supervisor sits outside what it supervises and depends on less infrastructure than it watches.** A heartbeat written and read by the same context is not supervision, and absence of traffic alone is not a fault. |

---

# 5. Explicitly Superseded / Do-Not-Reintroduce Without Review

Older documents contain these concepts. They should not be assumed current:

- **Retired invariant IDs `WIRE-1` through `WIRE-5`.** The one-Origin/many-Nodes model, NodeId broadcast rules, and canonical Direction are superseded by `WIRE-6`..`WIRE-10`, `SCOPE-7`, and `PDU-8`; these IDs are never reused.
- **Retired invariant IDs `ALIAS-1` through `ALIAS-4`.** WireAlias and anonymous alias-0 LocalBus semantics are superseded by canonical local-only `kLocalBus` (`SCOPE-9`, `SCOPE-10`); these IDs are never reused.
- **Retired invariant IDs `SPLICE-3` and `SPLICE-4`.** Anonymous LocalBus no longer exists, and splice validity is no longer expressed in Origin/NodeId terms; these IDs are never reused.
- **Retired invariant IDs `LINK-6` and `LINK-7`.** Their frozen aggregate-CRC and exact FrameControl claims predate the accepted CAN11 profile architecture; exact CRC and PDUA packing remain provisional. These IDs are never reused.
- **Retired invariant ID `SVC-1`.** Origin failover/election is not meaningful in the canonical model because Origin is no longer a Wire role; this ID is never reused.
- Wire as a directed one-source/one-or-more-sink object rather than a Logical Bus propagation domain.
- `Main` / `Peer` / `PeerId` terminology.
- `PathTag` as the canonical Wire routing identity.
- old `{WireBand, RoutingCode}` architecture.
- old 11-bit CAN `PathTag`/`PeerId` allocations, including 4-bit PathTag and 4-bit PeerId.
- any CAN11-carried Wire number or Wire alias. One CAN11 Link Binding carries one Wire, reconstructed from the binding.
- a mandatory `Link Engine` object that performs generic routing. Only the routing role is superseded; the execution and fault-containment framing is recovered as the restart unit (`CORE §23.2`).
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
- Every physical bus having canonical WireNumber 0 because it uses `kLocalBus`. `kLocalBus` is now a reserved canonical local-only WireNumber, not an alias, and a named Wire may use any allocated WireNumber.
- `Route` as an object distinct from the Wire it realizes, with its own terminals and Direction. Forwarding is configured Wire topology; Direction is not canonical PDU semantics, and there is no separate cross-domain path object.
- `ParticipantId` as a third identity space alongside canonical Node identity. ParticipantId now *is* the deployment-scoped canonical identity of an Endpoint Domain; formal Node/NodeId identity is retired.
- **Reachability requiring a Manifest.** An earlier generation held that reachability and authority exist only where a Manifest configures them. Deliberately relaxed; see `DEPLOY §2`. Structural validity is still always enforced (`CFG-3`).
- A **Wire Space as a security or memory-protection boundary.** Recovered only as an identity scope (`CORE §4.6`). It is not claimed to enforce anything, and `CORE §22` remains the security position.
- **`Node` as a canonical role or as a synonym for Endpoint Domain.** Endpoint Domain remains the dispatch/authority boundary; Participant is its deployment identity. Origin, Node, and NodeId are not canonical architecture.
- **Bidirectional interaction requiring two Wires, or canonical Direction to distinguish its legs.** Canonical source and destination directly identify each PDU; request/reply semantics remain with the Service or Transport.
- **`RouteId`, and any deployment-wide unique identifier for a Wire-realizing path.** Already implied by retiring `Route` as an object, but stated separately because the old validation rules enforced its uniqueness. The identity that matters is the WireNumber; table indices are local artifacts (`DEPLOY §2.3`).
- **A separate runtime producer token** (`ProducerKey` and its predecessors). The producing identity is canonical `SrcParticipantId` plus Endpoint/Wire context; no extra field is carried.
- **`RoutingWord` / Band-0 canonical reconstruction on CAN**, along with Direction, WireAlias, and NodeId canonical fields. CAN11 now reconstructs canonical Wire/source/destination through Link Binding, VCN, or Participant projection.
- **A frozen Classical-CAN aggregate CRC policy.** The old none/CRC-8/CRC-16-by-N rule (`LINK-6`) is no longer settled; exact CRC algorithm, coverage, placement, byte order, and interaction with PDUA remain provisional.
- **Gross or net per-N capacity tables treated as architectural fact.** PDUA, N1, and CRC packing must be rederived for the accepted six-byte descriptor and are provisional.
- **Committed/shared CAN profile families and discriminator-based generic legacy coexistence.** They are superseded by Guest VCN, Native VCN, and Native Participant-Compressed. Guest coexistence requires an explicit aligned CAN-ID allocation from the bus owner.
- **The ascending QoS numbering** (`QoS 0` = Background through `QoS 3` = Critical), and the CAN inversion step it required. Reversed deliberately: QoS is now the count of strictly-higher-priority classes, so Critical is 0, and CAN packs the value unchanged (`CORE §14`, `LINK §2.2`). Any older table, constant, or arbitration-mapping helper using the ascending order is wrong, and the error is silent — it produces a system that runs with its priorities exactly inverted.
- **`WireBand` as a per-deployment reinterpretation of the routing field**, with Band 0 standardized and Bands 1-3 user-defined including opaque or generated routing. Retired along with `RoutingWord`, and not worth recovering: there are no spare bits for it, and one canonical WireNumber interpretation is the point. The *requirements* it imposed on any extension point are worth keeping, though — a named immutable definition, deterministic validation, static compatibility checking, unambiguous canonical reconstruction, and fail-closed rejection of anything unsupported.
- **`PathTag::Local` and its Direction/PeerId reconstruction rules.** Local scopes are canonical reserved WireNumbers (`kLocalDomain` and local-only `kLocalBus`), not routing-field values.
- **A "reliable transport" defined by field sketches.** An earlier generation carried draft reliable-transport header layouts that were explicitly not a contract. They are not recovered, and the current position is that reliability is designed from concrete Service requirements or not at all (`FUTURE §3.2`). The intermediate transport in `FUTURE §3.1` is the one worth designing first.
- **Optional automatic remote infrastructure-error reports.** An earlier generation permitted these under heavy constraints. Superseded by `ERR-1`: nothing is emitted automatically in response to bad traffic. Deliberate remote diagnostic reporting remains available as an ordinary configured Service, subject to `ERR-3`.
- **`Inline` delivery, and the `Inline`/`Serialized` delivery-policy pair.** Endpoint delivery is now always a bounded storage operation (`DISP-2`). This one will be proposed again, because a direct call is visibly cheaper than a queue and the argument for it is always "we measured it and it is faster." The reason it is refused is not performance: **`Inline` makes a Link's worst-case execution time depend on every Service that might be delivered to,** so the Link cannot be analyzed in isolation and its worst case changes when a deployment adds a Service its author never saw. It also makes message topology into call topology, allows a Service to reenter itself by transmitting during a receive, and has no meaning at all in RTL. The latency it saved was typical-case latency bought with an unbounded worst case.
- **Delivery policy as a deployment-preserved property.** `DISP-2` used to promise that a declared policy survived a placement change, which implied there was something to preserve. Storage semantics are now fixed by the Service definition (`DISP-6`), so nothing is configurable and nothing needs preserving. Any tooling check written to verify policy preservation is checking a property that can no longer vary.
- **The four-storage-class taxonomy** (snapshot, value queue, ownership-transfer queue, event queue). It conflated storage semantics with ownership representation, and "event queue" was a semantic duplicate of "value queue." Refactored into three independent axes (`OWN-5`), which is what makes the copyless path a change on one axis rather than a new set of classes.
- **`Port` as an architectural term.** It named the local typed interface at a Service boundary as distinct from the network-visible Endpoint. That distinction was real only while delivery meant invoking a handler; bounded storage makes the object the Service holds and the object the network addresses the same one (`CORE §1.6`). The term survived as a classification that constrained nothing. What it carried is preserved by `DISP-3`: a Service's local interface never appears in a PDU, and restructuring it is not a protocol change. `TxBinding` became a transmit Endpoint.
- **Endpoint-level transmit fan-out** ("one Endpoint implementation may source several Wires"). Superseded by `DISP-11`. It would have forced per-reader sampled/sent state into every Snapshot transmit Endpoint, and the capability already exists one layer down as splicing or gateway forwarding, both of which preserve source lineage.
- **Multi-reader Queue Endpoints** ("several local consumers may read one Endpoint where its contract permits", as applied to queues). Superseded by `DISP-10`. Draining is destructive, so two consumers silently split the stream — it passes every test with one consumer and loses messages in production. It is a worker-pool construct with no RTL meaning, and it will be asked for again as a "load-balanced handler pool."
- **The older `Control` field order** (`Namespace, TransportType, QoS, HasHeaderExtensions`, in that allocation order). Superseded by `BITS §2`, which places QoS in the most significant bits so that a numeric comparison of whole `Control` bytes orders by priority and CAN can carry the field unchanged. The old order is not merely a different arrangement — it makes both of those free properties impossible.
- **A canonical `RoutingWord`, reserved or otherwise.** The base descriptor directly carries 8-bit Wire, source PID, and destination PID fields. Reserved-field rejection (`PDU-5`) applies to `Control` and defined extension layouts.
- **One status Service per Link, or separate local-introspection and network-telemetry Services.** Superseded by one Service per Endpoint Domain presenting two faces over a single semantic model (`DEPLOY §3.3`). Two Services over the same underlying state is two chances to disagree about what "degraded" means.
- **Self-describing telemetry** — a runtime tag/type/value encoding for status. Rejected in favor of a fixed schema per version selected by identifier. A self-describing format is an unbounded parser on the receive path of the one Service most likely to be reachable during a fault.

If an implementation task appears to require one of these, first verify that the current architecture genuinely cannot solve the problem without it.

---

# 6. Open Questions

## 6.1 Canonical / allocation

- Validate the preferred 8-bit WireNumber and ParticipantId budgets against a representative topology corpus covering multicore/internal Domains, redundant controllers, gateways, several CAN buses, overlapping broad/narrow Wires, device-private and debug Wires, local reservations, and product growth. Record peak use, reservation cost, and remaining headroom before freezing widths.
- Final exact WireNumber allocation fences and numeric assignments for device-private Wires, `kLocalBus`, and `kLocalDomain`.
- Whether canonical header extensions need additional standardized common fields before freeze.
- The extension length encoding itself. A 2-bit code giving 1..3 bytes is the working direction (`CORE §2.3`); what is settled is only that the block must be self-describing (`PDU-6`).
- Whether an unrecognized but well-formed extension is preserved on forwarding and rejected on dispatch, decided per extension or globally. This must be answered before any extension is defined.
- Whether the first extension defined is the sequence/end-to-end check value from `FUTURE §3.1`, which would make it the mechanism's first real test.
- Exact TransportType registry.
- Namespace 3 registry process (allocation authority, experimental reclamation, tombstones).
- Header extension internal field placement, once any extension is defined. The preferred base `Control` allocation is recorded in §3; CAN profile details still open are listed in §6.8.

## 6.2 Splicing

- Whether a splice is one bidirectional mapping or two directional route entries.
- Whether multiple devices may splice onto one shared external Wire by default, and how coordinated ParticipantId-universe assumptions and collision checks are validated if so.
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

- Exact Domain Local Link Telemetry Service schema, and its Endpoint allocation.
- Exact minimum counters required by conformance classes.
- Exact per-Wire top-talker/drop-report format.
- How the five value states of `ERR-5` are encoded: per-section validity, reserved codes, an explicit validity map, or a mix. That they are distinguishable is settled; the mechanism is not, and it is the choice that determines whether Compact stays small.
- Field widths and quantization for each of Compact, Standard, and Extended, and whether schema evolution preserves any byte-prefix compatibility (the working assumption is that it does not need to).
- When rejection-reason **numeric** values are frozen. Semantic names are stable from the start; numbers are not, until a reason is serialized into a telemetry schema or crosses a wire. `LIB §4.5` withdrew an earlier "stable numbering, append only" promise as premature, on the grounds that the registry will be reorganized during prototyping and an early freeze buys nothing. The freeze should be an explicit act taken with this schema, not something inherited from a comment.
- Whether the counter registry and the rejection-reason registry (`CONFORM §4`) are one enumeration or two. They serve different consumers but must not drift, and merging them is the cheap way to guarantee that. `LIB §4.5` proposes a third option — one wide registry plus a narrow API result type with a documented mapping — which keeps a call-site `switch` exhaustive without allowing two tables.
- Detail pagination and cursor behavior for Extended, including what happens to a cursor across a restart.
- Resolution, epoch, and wrap-comparison rules for the arrival timestamp stored in an Endpoint slot. `LIB §4.3` fixes only that the stored form is narrower than the platform clock's 64-bit nanosecond time point and that the reduction happens once on the ingress path; the width and what a wrap means for a staleness test are open.

## 6.8 Classical CAN

- Freeze the exact arbitration-significant bit order and Direction values within the accepted Guest VCN, Native VCN, and Native Participant-Compressed field budgets.
- Guest VCN details beyond the settled aligned 16-ID, VCN[3] + Direction[1], fixed-QoS profile: allocation-record format, exact fixed-QoS configuration rule, filter setup, and proof that the bus owner reserved the range. Five- or six-guest-bit growth profiles are not frozen.
- Native VCN allocation policy, arbitration visibility, and VCN table storage representation.
- Exact byte-level `PduControl`, optimized-N1, General PDUA, Endpoint, DLC, length, padding, and continuation encoding for the six-byte canonical descriptor.
- Exact aggregate CRC algorithms, parameters, placement, coverage, byte order, and incremental-vs-whole-buffer computation. Existing CRC candidates are evidence, not a frozen CAN11 policy.
- Commissioning/control payload, opcodes, identity, selection/session state, commit/persistence state machine, retries, and failure recovery remain open. The three reserved control encodings are settled: all-ones VCN in Guest VCN, all-ones VCN in Native VCN, and Participant-Compressed General code 31 in the General-to-Compact direction.
- The default Critical-QoS PDUA depth cap, and whether it is tooling policy or a profile constraint (`LINK §2.9`).
- Reassembly context pool sizing, and whether the one-context-per-identifier limit survives contact with a many-Wire gateway.
- TODO: define the exact compatibility-fingerprint coverage for VCN tables and Participant projection maps; this is not yet an invariant or frozen profile rule.
- TODO: define atomic activation of CAN11 profile, VCN, projection, fixed-QoS, and Wire-binding configuration; this is not yet an invariant or frozen profile rule.
- TODO: define when configuration activation flushes reassembly contexts and other representation-dependent transient state; this is not yet an invariant or frozen profile rule.
- Controller filter configuration and scheduler fallback rules for representative CAN peripherals.
- Reassembly timeout rules and rate assumptions.
- Whether CAN FD/XL share the same PDUA concepts or receive simpler native-PDU profiles.
- Whether the planned 29-bit Classical CAN profile is standardized, and its exact canonical/PDUA packing and identifier layout.

## 6.9 UART / byte streams

- COBS vs HDLC-style framing.
- CRC-16 polynomial/parameters.
- Maximum frame/PDU size.
- Flow-control extension encoding and interaction with framing.

## 6.10 Discovery / configuration

- Exact pre-addressing commissioning algorithm per Link type.
- ParticipantId assignment conflict, replacement, and rejoin behavior.
- VCN-table, Participant-projection, fixed-QoS, profile-selection, and Wire-binding generation/update rules.
- Persistence/lease semantics for ephemeral host configuration.
- Exact discovery Service schema.
- Exact gateway-management Service schema, including the "make interface N carry Wire W" operation.
- Policy for cyclic and non-tree topologies during auto-Wiring.
- Scope and build-time removal rules for promiscuous/bring-up mode.
- What the `Staged` phase persists, and how an abandoned commissioning attempt is rolled back.
- The configuration compatibility-check algorithm and whether its scope is per-Link, per-Wire, or per-deployment. Fail-closed mismatch handling is settled (`CFG-10`); exact VCN/projection fingerprint coverage, atomic activation, and reassembly flush remain TODOs listed in §6.8.
- Whether `CORE §17`'s capability set is one descriptor or two. `LIB §8.3` splits it: properties that survive a change of controller (max PDU size, fragmentation, QoS mapping) describe the Logical Link and are what configuration validates against, while queue depth, DMA use, and ISR involvement describe the driver and are diagnostic only. The dividing line is proposed, not settled, and it matters because mixing them makes configuration validation depend on a peripheral.
- Whether a Link's transfer-unit kind remains a described capability. It is still useful as a *description*, but `LIB §8.1` removed it as a runtime discriminator: hardware driver contracts are typed per carrier shape there, because a WireSpaces CAN identifier carries descriptor content (`LINK §2`) that a byte-span signature cannot express. Carrier-neutrality is asserted at the Logical Link, not at the driver.

## 6.11 Freshness and transport semantics

- How a Service declares freshness: a locally checked interval, or a canonical timestamp/sequence in a header extension.
- Whether any standard "state is stale" notification exists, or whether each Service defines its own degraded behavior.
- Whether ordering guarantees are declared separately from delivery guarantees.
- How polled-Link cadence is expressed in Link capabilities so freshness bounds can be checked statically.

## 6.12 Endpoint API and composition

- Whether Endpoints store a decoded representation or raw payload bytes. `LIB §5.1` argues for bytes until a generated codec exists, on the grounds that hand-written decode called from acceptance is unbounded-by-construction work in a Link's context. That defers the naming question below rather than settling it.
- Final type and API names. `QueueEndpoint<T, N>` and `SnapshotEndpoint<T>` are working names only, and they are misleading in one respect worth fixing before they stick: **a slot is declared metadata plus payload** (`DISP-14`), so `T` names the payload representation rather than the slot. `T` is also the **decoded local representation**, never the wire contract — the schema is (`SVC-7`), and naming that lets `T` look like the wire type is how a messaging layer accidentally becomes ABI-dependent. These names are higher-stakes than usual because the Endpoint API is a portability contract (`SVC-9`), so they are the surface Services get written against rather than an implementation choice.
- How delivered metadata is declared and accessed: how the three levels are named and selected, whether recognized extensions are exposed decoded or as bytes, and what an accessor returns when a Service reads a field its Endpoint did not declare — a compile error is preferable to a zero. `LIB §5.2` sketches a provisional shape that delivers the compile error; it is a candidate, not a decision.
- How much of the Endpoint API `SVC-9` actually fixes, and how it is verified. Candidates: a documented signature set, a header a conforming implementation must satisfy, or a portability test suite that compiles the same Service against two implementations. Nothing is decided, and without one of them the contract is an intention rather than a check.
- Whether a small documented default Queue capacity is offered, or every Queue Endpoint declares its depth. A default in general builds with profiles able to require explicit capacity is the likely compromise; prototype experience should decide.
- Whether a narrowly scoped synchronous hook is ever permitted for instrumentation, tracing, or platform scheduling notifications. Framework arrival-timestamping at acceptance is settled (`DISP-2`); **application-supplied hooks are not in prospect**, since an unrestricted user callback recreates what `Inline` was withdrawn for. If one is ever considered it must answer: who may supply it, before or after acceptance, may it inspect payload, may it transmit, may it block, what bound applies, and whether a dedicated observation Endpoint consumed asynchronously would do instead.
- Whether transmit direction keeps its own vocabulary. `TxBinding` became a transmit Endpoint, but it still carries authority that receive storage does not, which may justify a distinct name.
- Exact Snapshot memory ordering. The **mechanism is now decided: a seqlock** (`LIB §9.2`), so a concurrent read is safe by construction and this is no longer a question about whether multi-reader Snapshots work. What remains open is the detail — which atomics or barriers, whether a retry bound is declared, and the interaction between the value generation and the sampled/sent echo. `LIB §9.2` also fixes two consequences worth carrying: writers still need exclusion from each other, and the seqlock sequence is kept distinct from the `DISP-12` generation so that an implementation detail is not exported as a Service-facing contract.
- Whether the seqlock's priority constraint should be a stated invariant rather than a note. A reader spinning at higher priority than a preempted mid-write writer livelocks on a single core; the natural arrangement (framework RX writer at or above the reading Service) is safe, but nothing in the type system enforces it.
- Concurrency beyond Snapshot reads: several writers into one Endpoint, and concurrent access to one Logical Link, are undesigned. `LIB §9.1` states a narrow phase-1 contract that requires the application to serialize them.
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

- The schema language and code generator for Service contracts. `SVC-7` fixes that the schema is the contract; it names no format. A candidate field-encoding vocabulary is recorded in `FUTURE §16`; the per-field definition checklist there is the part worth adopting regardless of syntax.
- What the compatibility fingerprint covers, and whether it is generated from the schema alone or from schema plus profile and configuration choices (`DEPLOY §2.4`).
- Whether the recommended `protocol_version` / `message_type` prefix becomes a requirement or stays a default.
- The exact API representation of a reply context: storage, lifetime, correlation, invalidation, and the retained-handle form needed for deferred responses (`CORE §10.6`).
- Whether binding mode is expressed in generated code, in the Manifest, or both.

## 6.16 Higher layers

- Reliable Transport(s), and whether the sequenced/end-to-end-protected datagram is designed first (`FUTURE §3.1`).
- Multi-Participant command-source policy if useful.
- Formal Manifest, Flow, WireContract, and static analysis models.
- Security/authentication profiles.
- Redundant/cyclic Wire realization if ever justified.
- Whether a Domain Control layer is ever needed for group management of Services.

## 6.17 Lifecycle, restart, and supervision

`CORE §23` settles the observable shape; these are the parts still open.

- The lifecycle state enumeration and transition API, and the vocabulary for a cancelled or faulted transmit. Both are local naming, but they appear in every Link driver, so getting them wrong is a wide edit later.
- Runtime generation width and wrap comparison rules (`RUN-4`). Narrow enough to be cheap on a constrained node, wide enough that a comparison window is unambiguous — the same trade already made for the Snapshot generation, and probably deserving the same answer.
- Where persistent counters and latched fault records actually live on each target class, and how a field declares which of the two boundaries in `RUN-9` applies to it.
- How a restart-unit dependency group (`RUN-8`) is expressed in configuration, and whether tooling can check a claimed isolation against the platform's real sharing.
- The heartbeat and supervisor interface, including what a supervisor is on a single-core bare-metal target where there may be nothing more privileged to run it.
- Whether a peer's restart is observable to a Service holding learned bindings or reply contexts, and if so whether that requires a protocol field (`FUTURE §6`). This is the one item here that could become a wire change.
- Whether stale application-owned handles must be detectable after a restart that reuses compact slot indices, or whether a generation-tagged handle is required (`RUN-9`, `REG §6.6`).
- Whether a communication-component catalog is worth establishing once `SVC-5` has been exercised.

---

# 7. Pending Source Material

The following older documents have **not** yet been mined. Their model is the superseded generation, but they may hold recoverable detail in the same way `WS_old/network/architecture_overview.md` did.

| Source | Approx. lines | Expected destination |
|---|---:|---|
| `WS_old/network/hdlc_logical_link_profile.md` | 410 | `LINK §3` |
| `WS_old/network/rationale_use_cases_and_risks.md` | 510 | `INTRO`, `FUTURE` |

The copy of `can_pdu_adapter_spec.md` that sat in this directory was byte-identical to the `WS_old` one, so mining the latter covered both. It has moved to `archive/`.

Two sources were mined in revision 0.10. `link_engine_runtime_and_status.md` answered the restart and lifecycle questions that `CORE §13.3` and `OWN-4` had opened, and supplied the telemetry lifetime and validity distinctions (`CORE §23`, `CORE §18.4`, `CORE §18.5`, `RUN-1`..`RUN-10`, `ERR-4`, `ERR-5`). `prototype_and_validation.md` supplied validation discipline rather than architecture: the evidence-generator stance, capability claims, the reason registry, frozen budgets, and staged freezes (`CONFORM §1.1`, `CONFORM §3.2`, `CONFORM §4`, `CONFORM §5`). Its own protocol content was almost entirely the superseded generation.

Of the two remaining, `hdlc_logical_link_profile.md` is the more useful, because `LINK §3` is a sketch and the credit and lifeline mechanics in `CORE §15.4` were derived from it secondhand. `rationale_use_cases_and_risks.md` is non-normative by its own description and is the lowest priority of anything on this list.

The mining method that has worked so far: read for concepts that were *dropped* rather than *superseded*, recover the ones that still hold, record the rejections in §5 so they cannot leak back, and note deliberate divergences rather than silently overriding them.

---

# 8. Revision History

Current revision: **0.14** (accepted Revision 6 architecture recorded: deployment-scoped Participants, Logical Bus Wires, canonical source/destination, explicit splice and identity-universe boundaries, canonical local-only `kLocalBus`, and the three static CAN11 profiles; descriptor widths and listed CAN packing/configuration details remain provisional).

Full revision narrative and superseded-source provenance live in [history.md](history.md). `REG` keeps only status; `HIST` is archival and is not part of the control surface.

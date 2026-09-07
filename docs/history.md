# WireSpaces — Revision History

**Status:** Provenance and change log only — not a control surface  
**Purpose:** Record how the document set evolved  
**Rule:** Current status lives in `architecture_register.md` (`REG`). This file is archival.

Cross-references use document codes. Section numbers in older entries refer to locations at the time of the revision; where content moved later, see revisions 0.8 and 0.9.

---

# Revision 0.16 — Dedicated BITS Transport document

`bits_transport.md` (`BITS-TRANSPORT`) now owns the main-set prototype design for Binary Image Transport, Segmented. `BITS` remains the existing code for `bit_layout.md`. The new document consolidates the updated proposal's fixed 1:1 connection and single-session model, two bounded ingress workstreams, deferred processing, candidate Compact SETUP/SEGMENT layouts, cumulative/selective ACKs, receiver grants, retries/PROBE, unreliable sideband, source/sink responsibilities and conditional CAN11 capacity.

Integration makes the safety boundaries explicit: ingress retention is not sink acceptance; only committed sink acceptance is ACKable; duplicate suppression is scoped to retained session state. A valid authorized SETUP may receive a bounded protocol refusal, while infrastructure/parser rejection remains silent. Flash work stays outside receive hooks. Hop validation does not provide end-to-end integrity, and a small modular window or random session ID does not exclude stale traffic.

The protocol is still provisional. Exact control/ACK encoding and TransportType allocation, delayed-ACK ambiguity, session reuse/tombstones, completion/abort and sink-result APIs remain open. Detailed library/implementation and full conformance integration remain follow-on work. Navigation, CORE boundaries, the architecture register and proposal disposition now point to the main document; the source proposal and earlier revision narratives are unchanged.

---

# Revision 0.15 — Host terminology, Transport Entity boundaries, and unified CAN11 VCN

The main set now uses Host, HostId, SrcHostId, and DestHostId for the existing deployment-scoped Endpoint Domain identity. This does not change the provisional six-byte canonical descriptor. Older revision narratives and proposal filenames retain their original terminology.

An Endpoint exposes one Transport Entity boundary with one or more declared bounded ingress elements. Simple datagram Endpoints retain one Queue or Snapshot. Receive acceptance may classify and retain data; protocol state transitions and Service callbacks run later. Copies remain the baseline, while explicit ownership or immutable leases may preserve retained references. This adopts the boundary needed by the BITS proposal without standardizing its detailed protocol, API, or TransportType allocation.

CAN11 now uses unified VCN + Direction with Guest and Native encodings. The preferred Native budget is QoS2/WireAlias3/VCN5/Direction1. Each alias selects one Wire and immutable map; several aliases may represent overlapping Wires or coexist during migration. TX selection is explicit and unique. Spare-alias migration requires receiver readiness, pinned accepted work, and stale-frame exclusion before retirement/reuse. The default two-Main map reserves VCN3 for control; Guest meanings are deployment-wide and widen without renumbering existing default entries. Native VCN8 and Compact/General are removed from the baseline; Compact/General remains experimental.

`proposal_disposition.md` records the disposition of all eight current proposals. Exact CAN profile details and migration management, commissioning identity, and detailed BITS integration remain open in `REG`. Implementation guidance and baseline conformance expectations follow the revised CAN11 direction; this revision does not claim code implementation.

---

# Revision 0.14 — Participant identity, Logical Buses, and revised CAN11 profiles

Revision 6 and its accepted CAN11 refinement replaced the canonical Origin/Node architecture. Every independently routed Endpoint Domain now has one deployment-scoped `ParticipantId`, used on every Wire it joins. Canonical `Origin`, `Node`, `NodeId`, and `Direction` are retired; the ordinary PDU instead carries canonical source and destination Participant identity, and multiple Participants may independently source traffic on one Wire. `Direction` survives only where a constrained Link profile uses it to reconstruct source and destination and has no canonical request/reply or authority meaning.

A Wire is now explicitly a **Logical Bus**: a configured loop-free propagation domain realized by one or more Links. Destination Participant identity controls acceptance rather than ordinary next-hop routing, and several broad or narrow Wires may overlap on the same Physical Links. Plain forwarding preserves canonical Wire, source, destination, Endpoint, control metadata/extensions, and payload while changing only Link-local representation. Consuming one interaction and authoring another is composition, so the composing Participant becomes the new canonical source.

The preferred ordinary descriptor became a provisional 48-bit / 6-byte form: one Control byte, 8-bit `WireNumber`, 8-bit `SrcParticipantId`, 8-bit `DestParticipantId`, and a 16-bit Endpoint. The 8-bit Wire and Participant allocations remain subject to representative topology/headroom validation before freeze.

`kLocalBus` is now a reserved canonical local-only Wire number rather than an anonymous alias. It may be bound to at most one local Link Interface in a Router/Endpoint Domain, produces fully canonical ingress, and is locally dispatchable but not transparently forwardable or spliceable as itself. Transparent forwarding does not merge independently assigned Participant identity universes. A splice is the explicit configured Wire-scope projection boundary: it preserves canonical Participant identity and lineage while deliberately changing Wire scope, and it does not resolve collisions between independent identity universes.

Link representation was generalized into two independent mechanisms. **Elision** omits a canonical field uniquely implied by the Link Binding or carrier context; **projection** maps a bounded Link-local participant code to canonical `ParticipantId` without changing identity. Every ingress representation reconstructs canonical Wire/source/destination before generic routing or dispatch.

CAN11 now has three profiles over two addressing models:

- **Guest VCN:** an aligned 16-identifier block allocated by the legacy-bus owner, 3-bit VCN plus Link-local Direction, and one fixed QoS reconstructed from the Link Binding;
- **Native VCN:** 2-bit QoS, 8-bit VCN, and Link-local Direction;
- **Native Participant-Compressed:** 2-bit QoS, 3-bit compact participant code, 5-bit general participant code, and Link-local Direction, with direct mapping by default and optional projection.

Every CAN11 Link-profile binding carries exactly one WS Wire and elides its Wire number. The all-ones VCN values are reserved for Link control rather than ordinary configured circuits. Guest fixed-QoS behavior is accepted, while the exact fixed-QoS policy, VCN allocation policy, map-fingerprint coverage, atomic map activation and interaction with in-progress reassembly, and commissioning protocol remain deferred. CAN29 remains the richer escalation path; its final identifier and byte layout are not frozen.

---

# 1. Revision 0.13 — `LIB` corrected after review

`LIB` was reviewed and several of its choices were wrong. This entry records the corrections, because three of them were errors rather than preferences and the reasoning is worth keeping.

**A hidden dependency cycle.** `PduSink` and `IngressSink` were defined in `forwarding`, but `LogicalLink` implements `PduSink` and a transmit Endpoint holds one, so `link` and `endpoint` both depended on `forwarding` while `forwarding` depended on them. The module diagram asserted independence that the types denied. A fourth wrong-way edge was worse because it was invisible: the LLL was described as sampling bound `SnapshotTransmitEndpoint`s, which would have made `link` depend on `endpoint`. The fix puts the one-method seam interfaces in the dependency root and adds a `PduSource` for the pull direction, so the LLL samples through a neutral interface. `core` now depends on nothing at all — the previous `core -> platform` edge was unnecessary, since no part of the descriptor codec needs a clock or a lock, and removing it means the codec and the conformance-vector tools build with no port present.

This also forced an honest statement of what `core` is. It holds canonical protocol types *and* dependency-root runtime types such as `ReceivedPdu` and `LinkIndex`. The earlier rule — a Link-scoped concept in `core` means a type is in the wrong module — was violated by the document that stated it. The rule now applies per header rather than per target.

**One byte-span driver seam for every carrier.** This was the most expensive error, because it would have been discovered only once a CAN driver existed. `sendUnit(Span<const uint8_t>)` cannot express a CAN frame, and in WireSpaces that is not generic awkwardness: `LINK §2` packs QoS, WireAlias, NodeId, and Direction into the 11-bit identifier, so the identifier carries descriptor content that the peer's LLL must recover. Forcing it through a byte span means inventing a private serialization format to talk to ourselves. The `TransferUnitKind` capability field was the tell — a seam needing a runtime enumeration to describe the true shape of its own arguments has the wrong signature. Hardware driver contracts are now typed per carrier shape (`CanDriver`, `ByteStreamDriver`, a datagram driver when one is needed), and carrier-neutrality is asserted at the Logical Link, which is what every layer above actually sees. The capability descriptor split with it, along the line of whether a property survives a change of controller.

**Snapshot coherence, now decided.** The review caught that a writer-side lock policy does nothing to make `read()` coherent against a concurrent `tryAccept()`: with a single writer, a no-op policy, and a reader in another task, a reader can observe metadata from one message beside payload from the next. The previous draft's claim that an RTOS could run every Link and Service in its own task "with the same objects and no library change" had no basis, and `REG §6.12` had listed Snapshot memory ordering as open throughout.

The resolution is a decision rather than a deferral: **a Snapshot Endpoint uses a seqlock**, so concurrent reads are safe by construction and coherence is not a configurable policy — every alternative setting would be one where an ordinary read tears. Three consequences are recorded in `LIB §9.2`: a read may retry and so is not wait-free, which makes the writer-priority relationship a real constraint; writers still need exclusion from each other, since a seqlock coordinates one writer with many readers; and the seqlock sequence is kept distinct from the `DISP-12` generation so an implementation detail is not exported as a Service-facing contract. Concurrency beyond Snapshot reads — several writers into one Endpoint, and concurrent access to one Logical Link — stays undesigned, with a narrow phase-1 contract requiring the application to serialize it.

**Two reversals of positions taken one revision earlier.** Revision 0.12 argued that resolving `util::Uint24_s`'s TODO into a general bounded-integer type was the highest-leverage change available, on the grounds that it would make reject-don't-mask "a property of the type rather than a rule the codec must remember." That argument does not hold. A type carrying an integer and an `isValid()` does not prevent invalid instances; the codec still has to call the check at every field, so nothing is centralized. Delivering the claimed property needs controlled construction — a private constructor with a `tryMake` factory — which is a substantially larger exercise. The sequencing was also backwards, and contradicted the `INTRO §9` rule this project applies elsewhere: one descriptor codec is the first demand, not the second. `LIB §4.1` now uses plain range-checked wrappers with an explicit stopping condition.

Revision 0.12 also recommended putting `Design/Firmware` on the core library's include path. That was wrong for a reason confirmed by inspection: `WireSpaces` and `Firmware` are in **separate repositories**, so an include path makes a clean checkout unbuildable except when an unrelated repository happens to sit at the right relative filesystem location, at an unrecorded revision. The earlier objection to copying — silent divergence — is answered by recording the upstream commit, which makes divergence a visible, reviewable event. A vendored extraction with recorded provenance is now the recommendation, and the observation that a core vendoring six foreign headers is not standalone in any meaningful sense strengthens the open question of whether the library belongs in the other repository entirely.

**Smaller corrections.** `decode()` returned `bool`, which made `CONFORM §4`'s requirement to assert *why* something was rejected impossible to satisfy; per-operation result enumerations replace it. `AcceptResult::kReplaced` was removed: replacement is simply what a Snapshot does, so every healthy publication after the first would have reported a non-accepted result, and any counter aggregating those would read a working system as a failing one — consumers learn what they missed from the generation counter instead, and overwriting a never-read value became a counter rather than a return value. The promise that `Reason` had stable append-only numbering was withdrawn as premature; names are stable, numbers freeze when they become externally visible. `RouteEntry` gained an explicit splice flag, because `CORE` reserves a high WireNumber range and one top value but **not** zero, so the `splice_to == 0 means none` sentinel would have silently disabled a legitimate splice. And the empty `DeliveredMetadata` specialization was described as costing nothing, which is untrue for a data member in C++17.

The first prototype increment now includes a transmit Endpoint feeding a recording sink, so both faces of the Endpoint model are exercised. The second is deliberately adversarial: a fake Classical CAN driver and the simplest CAN LLL path, before any byte-stream work, because CAN is what tests whether the redesigned seams are genuinely carrier-neutral rather than only claimed to be.

---

# 2. Revision 0.12 — `LIB` built on the existing utility layer

`Design/Firmware` was surveyed for components the core library would otherwise reinvent, and `LIB` was reworked to depend on them. The finding was that most of the utility layer already exists and is better tested than anything written fresh would be: `util::Span` replaced the `Span` the previous revision had invented, `RingBuffer` became the Queue Endpoint storage, and `hal::PlatformClock` became the time source.

Two findings mattered more than the convenience.

The first narrowed `REG §6.8`. `Firmware/crc` already implements and vector-tests candidate CRC-8 and CRC-16 algorithms, so the choice `LINK-6` needs is now between implemented options rather than table entries. It also surfaced a hard constraint that was not previously visible: the existing bitwise implementation asserts against reflection, so any reflected polynomial requires implementing reflection first. `REG §6.8` records both, and gained an open item about whether incremental CRC is needed for fragmented PDUs.

The second was that `util/integer.h` contains, in `Uint24_s`, exactly the pattern the identity types want — a narrow value in a wider container whose serialization *fails* rather than truncating — and a TODO asking for the general form. `BITS §2` gives a WireNumber 10 bits and a NodeId 5, so out-of-range values are reachable and `PDU-2` requires rejecting them. Resolving that TODO into a general bounded-integer type would make reject-don't-mask a property of the type rather than a rule the codec must remember at each field boundary. `LIB §4.1` is written assuming it, with the plain-integer fallback stated.

Three smaller things were recorded because they cost nothing to know now and are annoying to discover later. `memory_pool.h` does not compile as written, `RingBuffer` sits at global scope against the project's own namespace rule, and `Can::CanFrame` is not a usable base for the CAN profile because it carries bootloader-specific CRC and HDLC framing. None blocks phase 1.

`LIB` also gained a build-integration section. The two trees use different build systems, but everything reused is header-only except `crc.cpp`, so an include path suffices; a vendored copy was considered and rejected as silently divergent. One consequence worth noting: a core that includes six `Firmware` headers is not meaningfully standalone, which weakens the case for keeping it separate from `Firmware` at all — that remains open in `LIB §15`.

`Timestamp` was also separated from the platform clock, since a 64-bit nanosecond time point costs eight bytes in every stored slot against `CORE §9.3`'s budget of roughly four. Its resolution and wrap behavior are now open in `REG §6.7`.

---

# 3. Revision 0.11 — `LIB` added

`library_architecture.md` (`LIB`) was added: module structure, seam mechanisms, and public API shape for the prototype core library. It changes no architecture and adds no invariants — `CORE` remains the source of behavior — but it is where the first implementation's structure gets argued about before code exists.

Three of its choices are worth noting here because they are provisional answers to items `REG §6` had left open, and are recorded as candidates rather than decisions (`CONFORM §1.1`):

- **Endpoints store bytes, not a decoded type**, until a generated codec exists. Hand-written decode called from acceptance would be unbounded-by-construction work in a Link's context, which is what `DISP-2` exists to prevent, so the byte form is what keeps acceptance trivially bounded. It also defers the `T` naming question in `REG §6.12` instead of answering it by accident.
- **Declared metadata as a template parameter**, with the accessor type differing per level so that reading an undeclared field is a compile error. `REG §6.12` stated that preference; this is the cheapest mechanism that delivers it, and a runtime "unavailable" flag cannot.
- **One wide reason registry plus a narrow API result type**, with a documented mapping between them. This is a third option for the question in `REG §6.7`, and it keeps a call-site `switch` exhaustive while still guaranteeing telemetry has one table.

Two structural commitments are the part most worth protecting as the library grows. The dependency direction — `core` depending only on `platform`, with `link` and `forwarding` connected through two one-method interfaces — is what keeps each module testable alone. And the split between the framework-facing and Service-facing faces of an Endpoint is structural rather than documented: acceptance is a private virtual override reachable only through the dispatch interface, while the read side is a concrete templated API, so Service code cannot inject into its own Endpoint and the portability contract carries no indirection.

The library also creates no tasks and owns no loop, which turns `IMPL §3`'s execution shape from an observation into a constraint on the code.

---

# 4. Revision 0.10 — lifecycle and restart, telemetry lifetimes, validation discipline

Two `WS_old` sources were mined: `link_engine_runtime_and_status.md` and `prototype_and_validation.md`. They divided cleanly. The first was almost entirely recoverable, because it addressed a subject the current documents had explicitly deferred. The second was almost entirely superseded on protocol content — old `Control` order, `WireBand`, `PathTag`, `PeerId`, ascending QoS, PDUA to `N=16` — but its *validation discipline* was the best material in either document and survived intact.

## 3.1 Lifecycle and restart

`CORE §23` had been a stub: four sentences naming likely recoverable units and deferring everything else to `FUTURE §6`. It is now the lifecycle chapter, and `OWN-4`'s open question about terminal outcomes during restart is answered. Ten invariants were added as a new family (`RUN-1`..`RUN-10`).

The load-bearing recoveries, each of which was absent rather than superseded:

- **A Link and its restart unit have separate lifecycles** (`RUN-1`). Conflating them is what produces a system whose only recovery is a reboot. The corollary is easy to miss and worth as much: lifecycle control is not only a fault path — power management, maintenance, and commissioning use the same operations, so an implementation that can only bring a Link down by faulting it reports every planned shutdown as an incident.
- **The restart unit** (`RUN-2`, `CORE §23.2`). The old term for this was *Link Engine*, superseded for being a routing catch-all — but its execution and fault-containment framing was the useful part and had been discarded with the name. The LLL instance is the natural default because `LINK-11` already gives it one serialized mutable context: the property that makes state safe to mutate is the same one that makes it coherent to discard.
- **Runtime generation** (`RUN-4`). A restart is a discontinuity no consumer can see unless it is reported, and all three resulting bugs are quiet: counters read as traffic anomalies, a previous life's high-water mark attributed to this one, and a pre-restart handle honored because its slot index became valid again — ABA arriving through a path that looks like recovery.
- **Bounded quiesce** (`RUN-5`), because otherwise the failure mode of the recovery mechanism is a hang, which is worse than the fault. And **discarding uncertain transient state rather than reconstructing it** (`RUN-6`) — guessing a half-parsed frame back into shape produces confident wrong data, where dropping it is a counted loss the system already handles.
- **Escalation from the smallest scope** (`RUN-7`), **declared restart isolation** (`RUN-8`), and **two durability boundaries** (`RUN-9`). The last resolves a real ambiguity: "persistent" had been used for both surviving a restart and surviving power loss, which is how a diagnostic counter ends up either uselessly volatile or wearing out flash.
- **A supervisor must sit outside what it supervises** (`RUN-10`), with the sharpest line in either source document: a heartbeat written by the failed context and read only by that same context is not supervision.
- **An out-of-band debug path** (`CORE §23.9`). This is not a duplicate of the Internal Debug Wire, which is in-band by design and therefore shares the fate of the stack carrying it. Both are wanted, for different failures.

## 3.2 Telemetry has two lifetimes, and unavailable is not zero

Two additions to `CORE §18` (`ERR-4`, `ERR-5`).

**Live versus latched** (`CORE §18.4`). Live status describes the current runtime and stops being observable when that runtime fails; latched status is evidence stored where the failure cannot reach it. The reason for the split is one bad inference: a live snapshot that has stopped changing is at least as likely to mean the producer died as it is to mean the system is quiet, and a reader holding only live status cannot tell.

**The five value states** (`CORE §18.5`): valid, stale with age, unavailable, not applicable, omitted by schema — none of which may be encoded as an ordinary zero. The concrete case is what makes it worth an invariant. A Link with no credit-based flow control has no credit balance; reporting *zero credit* says it is stalled and out of credit, which is a different and alarming condition. Per-QoS depth on a Link with one shared hardware queue is the same error, and fabricating a split is worse than reporting the split is unobservable.

Age got the same treatment: elapsed monotonic time to the point the report was assembled, not wall-clock and no evidence of clock sync, with a saturated maximum meaning "this old or older."

## 3.3 One telemetry Service per Domain, with schema classes

`DEPLOY §3.3` gained structure it had been missing. One Service per Endpoint Domain — not one per Link, and not a separate local-introspection Service beside a network-reporting one, since two Services over the same state is two chances to disagree about what "degraded" means. Two access faces over a single semantic model, so local telemetry stays useful during bring-up when no telemetry Wire is reachable.

The Compact/Standard/Extended split by target scale was recovered as the plausible schema shape, along with the rule that a field absent from a smaller schema is unavailable rather than zero. Self-describing tag/type/value telemetry was rejected outright and recorded in `REG §5`: an unbounded parser on the receive path of the one Service most likely to be reachable during a fault is the wrong trade at any size.

One recovery was a pleasant confirmation rather than an addition. The old requirement that each destination hold **at most one pending summary**, updated in place by a newer sample rather than queued behind it, is exactly a Snapshot transmit Endpoint (`CORE §10.4`) — designed in revision 0.9 for unrelated reasons. An older generation independently needing the same primitive is reasonable evidence it is the right one.

## 3.4 Validation discipline

`CONFORM` grew four sections from `prototype_and_validation.md`, none of them about encoding.

**The evidence-generator stance** (`CONFORM §1.1`) is the most useful line in either source: an implementation choice does not close a specification gap and does not become a decision by being shipped. The failure it prevents is specific and familiar — someone picks a reassembly timeout because the code needs a number, and six months later it is in three implementations and a test suite while the actual question has been answered by nobody. Given that prototyping starts next, this arrives at the right time.

**Capability claims** (`CONFORM §4`): required, optional-enabled, optional-disabled, unsupported — and a capability may be claimed only if its tests ran. This solves a problem a fixed pass/fail suite cannot, given targets from 8-bit nodes to Linux gateways, and it names the silently skipped success: a suite reporting green because the flow-control tests found no flow control and returned early. The **fixed rejection-reason registry** in the same section is the same idea applied to negatives, since an oversize PDU rejected as an unknown Wire passes an outcome-only test.

**Budgets are inputs, not outputs** (`CONFORM §3.2`). A budget written after the measurement is a description and will accommodate whatever was measured. Also recovered: the load profiles nobody runs — startup, degraded, diagnostic burst, error storm — and the assertion that diagnostics cannot starve control, which is easy to build wrong because diagnostic traffic scales with how badly things are going.

**Staged freezes** (`CONFORM §5`) replaced the old Gate 0-8 model with six stages ordered by where rework concentrates. CAN is last, not because it is hardest but because it is the only one whose mistakes are visible to other devices.

`CONFORM §2.1` collects the restart cases, which are nearly all unreachable from a vector file and all reachable from a field failure.

## 3.5 Also recovered, and rejected

Recovered in smaller form: **receive-side offload** guidance was folded into existing rules rather than added, since `CORE §1.7` and `DISP-9` already cover an LLL doing framing, integrity, decode, and timestamp capture without becoming the producer; **static schedule entries** turned out to be transmit Endpoints plus an LLL cadence, already present in `CORE §10.3`; and a **candidate field-encoding vocabulary** went to `FUTURE §16`, where `SaturatingUInt<N>` (maximum means "at least this") and the rule that unassigned enum codes are reserved rather than mapped to a nearest known state are the parts carrying real weight.

Newly recorded in `REG §5`: the older `Control` field order, which made both of `BITS §2`'s free properties impossible; the `RoutingWord` reserved bit, since the current layout is fully allocated; one status Service per Link; and self-describing telemetry.

Deliberate divergences left standing rather than imported: the old prohibition on bidirectional Wires (already recorded), `Route` and its segments as objects, `ParticipantId`, and PDUA depth to `N=16`.

---

# 5. Revision 0.9 — bounded Endpoint delivery, `Port` retired, descriptor packing fixed

## 4.1 Bit and byte layout

`bit_layout.md` (`BITS`) was added, fixing conventions that had been deferred as "profile-owned" and were blocking the first roadmap step: descriptor helpers cannot be written without bit positions.

Settled: MSB-first field order within a byte; `Control` as QoS 7..6, Namespace 5..4, `HasHeaderExtensions` 3, TransportType 2..0; and `RoutingWord` as NodeId 15..11, Direction 10, WireNumber 9..0.

Two of those deserve their reasons recorded. **Priority occupies the most significant available bits**, which combined with QoS being the count of higher-priority classes means a numerically lower value always wins, in the descriptor and in a profile identifier alike — an inversion then produces a visibly wrong value rather than a silently reversed priority order. And **`WireNumber` went in the low ten bits rather than leading the word**, because little-endian serialization then makes the first byte exactly `WireNumber[7:0]` and extraction a single mask; leading with it would split the field 2/8 across the byte boundary and leave no field occupying a whole byte. That is the case where the alignment preference and the largest-field-first instinct conflict, and it is why `BITS §1` says alignment wins.

A related change fell out of writing it. `LINK §2.5`'s `PduControl` byte was defined as an unordered field *list*, so the new MSB-first convention would have retroactively placed its three shared fields at different positions from `Control` — costing three shifts per PDU in each direction for no reason but the order the two lists happened to be written in. `PduControl` now leads with `EndpointId[9:8]`, which puts each byte's distinct 2-bit field at bits 7..6 and makes the remaining **six bits identical in both**. Conversion is a mask and an OR each way. QoS is not duplicated: it travels in the CAN identifier, `Control` is never transmitted verbatim on CAN, and the two bytes cannot appear in one frame, so no cross-validation rule is needed.

`Control` is now noted as fully allocated, with no reserved bits and no growth room, so any future global flag must go in a header extension.

## 4.2 Bounded Endpoint delivery, and the retirement of `Port`

Adopted from `docs/archive/decision-bounded-endpoint-delivery.md` (formerly `docs/proposal-bounded-endpoint-delivery-and-snapshot-semantics.md`) with modifications. This is the largest semantic change since the QoS renumbering, and unlike that one it removes a capability rather than renumbering a field.

**Delivery.** `Inline` and `Serialized` delivery policies are withdrawn and replaced by a single model: Endpoint delivery crosses a bounded storage boundary and never synchronously executes Service code (`DISP-2`, `CORE §9.4`). Routing stays synchronous in the caller's context; only destination Service execution is deferred. The decisive argument is not tidiness but analyzability — `Inline` made a Link's worst-case execution time depend on every Service that might be delivered to, so no Link could be analyzed in isolation and its worst case changed whenever a deployment added a Service its author never saw. The reasoning is recorded in `REG §5` because "just call it directly, it is faster" will be proposed again.

**Storage.** Every Endpoint owns exactly one storage element, Queue (history-preserving) or Snapshot (latest-value), with capacity declared per Endpoint (`CORE §9.5`). A Service needing several message types either multiplexes internally or declares several Endpoints; the bootloader case — one deep-1 segment Endpoint beside a deeper command Endpoint — is why depth is never global. Snapshot generation counters became required rather than recommended, since a Snapshot without one cannot distinguish "no new value" from "producer died."

**Concurrency and multiplicity.** Storage semantics and writer concurrency are immutable properties of a Service definition rather than deployment choices (`DISP-6`), which is what made the old delivery-policy-preservation promise unnecessary rather than merely reworded. One Service writes a transmit Endpoint, one reads a Queue Endpoint, any number read a Snapshot Endpoint (`DISP-10`). Framework producers are not Services, so several Link drivers may still write one receive Endpoint — the AMP arrangement depends on it. Endpoint Domains became explicit concurrency scopes required to provide serialization, stated as a property rather than a primitive so that RTL arbiters and SMP locks both satisfy it (`DISP-13`, `CORE §1.5`).

**Transmit.** `TxBinding` became a transmit Endpoint, unifying both directions under the same two axes (`CORE §10`). The proposal's blanket transmit-side symmetry was *not* adopted: the receive rule is justified by arbitrary application code with no work bound, and a Link driver is bounded framework code, so requiring a queue hop on every send would have cost a copy and a context switch to prevent a problem that does not exist — and would have contradicted the small-MCU profile's direct `driver.send()` path. Instead, Snapshot transmit Endpoints give periodic publishers a genuinely queue-free path where the LLL samples on its own cadence.

Snapshot transmit Endpoints also gained a 16-bit wrapping sampled/sent generation echo (`CORE §10.4`). The motivation is a silent failure mode rather than staleness reporting: a Snapshot transmit Endpoint that was never bound to a Wire behaves identically to a working one, where an unbound Queue Endpoint would fill and reject.

**Terminology.** `Port` is retired (`REG §5`). It distinguished the local typed interface from the network-visible Endpoint, which was a real distinction only while delivery meant invoking a handler. WireSpaces now defines no Service-to-application interface at all (`INTRO §4`). The boundary is scoped deliberately: WireSpaces owns the receive path to acceptance and the transmit path from acceptance, and a Service may not inject work back across it — without that clause, a Service-defined user callback fired inside acceptance would reintroduce `Inline` through the side door.

**Delivered metadata, added during review.** The first draft of the storage sections described a slot as holding a payload type, which contradicted `§9.3` and `ROUTE-6` — both of which already require source identity to reach the consumer. An Endpoint now explicitly holds declared metadata plus payload: source, QoS and TransportType, extension access, and arrival time (`DISP-14`). Two findings came out of writing it. Metadata must be **copied at acceptance rather than viewed**, because withdrawing `Inline` also withdrew the only conditions under which a view into ingress storage was valid — the consumer now reads after that buffer is reclaimed. And metadata is **declared** rather than unconditional, because roughly 8 bytes per slot is negligible on a bootloader segment and close to a doubling on a queue of 8-byte commands, which is the profile where storage was already the binding constraint. Arrival time is unconditional on Snapshots alone, since latest-value semantics with no time basis cannot support freshness at all.

`DISP-15` was added alongside it: reading source metadata confers no transmit authority. Without that, a Service could read a Wire and NodeId and construct a transmit from them, which is exactly the reverse-the-Direction hole `DISP-7` exists to close.

**Portability, corrected during review.** The silence above the Service was briefly and wrongly extended below it, as a non-goal on Service source portability. That is backwards: the Endpoint API is precisely the surface that must be portable, and `SVC-9` now states it as a contract. A low-end and a high-end 32-bit MCU should run identical Service source over entirely different stacks, which is a precondition for any Service ecosystem — schema-over-bytes agreement alone would only guarantee that two incompatible implementations exchanged the same bytes. Portability is bounded the same way link independence is, by the Service's declared resource and timing envelope and by its non-WireSpaces dependencies. The correction also raises the stakes on the Endpoint API's names and shape, which is now recorded in `REG §6.12` along with the question of how the contract gets verified at all.

**Also changed.** The four-storage-class taxonomy was refactored into three independent axes, which is what makes copyless delivery a change on the ownership axis alone (`OWN-5`, `FUTURE §2.1`). Arrival time is now captured at acceptance, because consumer latency sits inside the delivery path and freshness handling would otherwise be unable to distinguish late production from late consumption. Endpoint-level transmit fan-out and multi-reader Queues were both superseded.

Five items were added to `REG §6.12` rather than settled: final type names, the Queue default-capacity policy, whether any synchronous instrumentation hook is ever permitted, whether transmit keeps a distinct vocabulary, and exact Snapshot memory ordering.

---

# 6. Revision 0.8 — document-set trim and split

Three structural changes, following the rule that `CORE` carries buildable runtime behavior and catalogs, host Service schemas, and test vectors live elsewhere:

1. **`conformance.md` (`CONFORM`)** — extracted from former `CORE §27` (vectors, boundary tests, exit criteria).
2. **`implementation.md` (`IMPL`)** — language choice, scaling-profile table, and execution shape from former `CORE §25`–`§26.2`. Authority-preserving small-MCU rules remain in `CORE §25`.
3. **`history.md` (`HIST`)** — this file; former `REG §8` revision history.

Additional moves:

- `CORE §7` compressed to the splice invariant; host debug path and default bindings → `DEPLOY §3.2`.
- Link Telemetry Service sketch → `DEPLOY §3.3`.
- Namespace 0 compact EID allocation → `LINK §2.4`.
- Service archetypes and Level-0 Service list → `FUTURE §8`.
- `INTRO §8.2`–`§8.3` shortened to pointers.

`README.md` gained an explicit scope rule for `CORE` versus catalog/test documents.

---

# 7. Revision 0.7 — QoS renumbering, and the last three large legacy specifications

Two changes: the canonical QoS numbering was reversed by decision, and the three largest remaining `WS_old/network` documents were mined, which completes the bulk of the legacy material.

**QoS renumbering.** Canonical QoS is now Critical 0 through Background 3, inverted from revision 0.6. The reason for the reversal is that it makes the numbering *derivable* rather than conventional: **QoS is the count of classes with strictly higher priority than yours**, so Critical has none above it and is 0. Lower-wins then follows from the definition, and the CAN inversion step introduced one revision earlier disappears entirely — the value packs into the arbitration-significant bits unchanged (`CORE §14`, `LINK §2.2`).

This is worth flagging as the highest-risk change in the document set so far. It inverts a numeric constant that appears in header packing, comparisons, queue indexing, and arbitration mapping, and getting it wrong is silent: the system runs, with its priorities exactly reversed. Anything written against revision 0.6 or earlier needs checking. The old ordering is recorded in `REG §5`.

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
- **The credit lifeline** (`CORE §15.5`, `QOS-8`). The standout recovery of this pass. Credit flow control has a natural deadlock — if the credit update is itself subject to credit, a stalled link stays stalled with both ends behaving correctly — and a permanently reserved lifeline is the structural fix. Also settles that credit measures a fixed storage quantum rather than a count of variable-size PDUs, narrowing an open question in `REG §6.5`.
- **One serialized mutable context per LLL instance** (`CORE §13.3`, `LINK-11`), with the platform-contract checklist that cross-core and DMA paths must answer rather than assume, and the observation that a seqlock requires a serialized writer.
- **Hop versus end-to-end integrity** (`CORE §20`, `LINK-12`). A gateway validates, reassembles, re-encodes, and computes a *fresh* check value, so every hop is verified and the path is not. Invisible in a single-Link deployment and real the moment a gateway exists.
- **A callback ABI must never be mandatory** (`DISP-9`), which matters for RTL and polled implementations.

Its **sequenced / end-to-end-protected datagram** was recovered as the named next Transport (`FUTURE §3.1`) and promoted ahead of reliability, because `SVC-3` already argues that most traffic wants freshness detection rather than retransmission.

**From `application_protocols_and_services.md`.** Mostly Service-level, and two items are structural:

- **A Service contract is a schema over bytes** (`CORE §21.3`, `SVC-7`). Generated structs are views; padding, enum width, alignment, and host endianness define nothing. Given that this project expects C++, Python, and RTL implementations of the same protocol, casting a buffer to a struct is the single most likely source of silent divergence.
- **Three separate identities** (`SVC-8`): protocol version, compatibility fingerprint, and build identity. A build hash is both too sensitive and not sensitive enough to serve as a compatibility check.

Also recovered: the recommended two-byte message prefix; the **Service archetype** table with QoS defaults and the note that RPC implies no reliable Transport (now `FUTURE §8.2`); **diagnostic containment**, whose first rule is that an error report can never generate another (`ERR-3`); the DRIP-shaped summary-plus-detail-on-request pattern; catalog promotion criteria including "at least one real deployment" (`FUTURE §8.3`); and, for a future Domain Control, serialized acceptance with supersession plus the rule that gating must not gate its own control path (`FUTURE §14`) — the same structural error as a credit scheme without a lifeline.

**Divergences and rejections** recorded in `REG §5`: the ascending QoS order and its CAN inversion; `WireBand` as a per-deployment routing reinterpretation, with its requirements-for-any-extension-point preserved; `PathTag::Local`; reliable transport defined by field sketches; and optional automatic remote error reports, which `ERR-1` supersedes while `ERR-3` keeps the useful constraint.

Twenty invariants were added across eight families; no existing ID changed. `CORE §21` gained two subsections, shifting its last two subsections down by two, and inbound references were updated. Open questions gained a Services and schemas group in `REG §6.15`.

---

# 8. Revision 0.6 — recovered from the endpoint-domain and CAN-adapter specifications

Two `WS_old/network` documents were mined: `endpoint_domains_wires_routes_and_access.md` and `can_pdu_adapter_spec.md`. Both belong to the superseded generation, and both held material with no current equivalent.

**From the endpoint-domain specification.** Its core model is the one already retired in `REG §5`, but three of its ideas were load-bearing and absent here:

- **Producer ownership and Endpoint concurrency** (`CORE §9.6`, `DISP-4`, `DISP-6`). The clearest gap found in any mining pass. `WIRE-1` constrains a Wire to one Origin, but nothing constrained an *Endpoint identity* to one producer, so two Services in one Domain could both publish as EndpointId 42 with no way for a receiver to distinguish them. The companion rule — that each Endpoint declares its concurrency model and that dispatch preserves rather than "fixes" it — is the producing-side counterpart to `DISP-2`.
- **Endpoint naming is not authority** (`CORE §9.7`, `DISP-5`), including the typed-handle discipline and an honest statement of its limits. `ROUTE-5` already said electrical visibility grants nothing; this says *knowing a name* grants nothing either, which is the software-side half of the same idea.
- **Source lineage** (`CORE §12.7`, `ROUTE-6`). Lineage preservation existed in fragments across forwarding, splicing, fanout, and observation. Consolidating it made the boundary visible: every mechanism preserves the producer, and re-origination is the one operation that does not — so it needs its own authority rather than hiding inside a route.

Also recovered: the Endpoint Domain as an explicitly *logical* boundary that is not a device and not a security boundary (`SCOPE-6`); Direction as structural (`WIRE-5`); the negative list for broadcast, including the prohibition on sharing one acknowledgement state across recipients (`WIRE-4`); universal bounded storage (`CORE §15.7`, `OWN-2`); the explicit "what a Wire does not guarantee" list (`CORE §3.6`); the tooling rejection checklist (`DEPLOY §2.3`); generated-artifact compatibility checking (`DEPLOY §2.4`, `CFG-10`); and the rule that collapsing layers on a tiny target must preserve authority distinctions (`CORE §25`).

**From the CAN adapter specification.** Its identifier layout, frame depth, and Endpoint allocation are superseded, but it resolved an open question and supplied several implementation-level rules:

- **QoS numbering and its CAN placement** (`CORE §14`, `LINK §2.2`). `LINK §2.1` previously carried this as an unresolved note about needing an inversion. Resolved instead by defining QoS as the **count of strictly-higher-priority classes** — Critical 0 through Background 3 — which makes "lower number wins" a consequence of the definition rather than a convention. CAN then packs the value unchanged into the most arbitration-significant bits, with no inversion step for an LLL to get backwards. Only the placement of the remaining identifier fields is still open.
- **Committed versus Guest CAN profile families** (`LINK §2.1`, `FUTURE §11.2`). The current layout spends all 11 identifier bits, which means a WS bus cannot carry legacy CAN traffic — a constraint that was implicit and unstated. Naming the two families makes the limitation explicit and gives coexistence somewhere to live.
- **Commissioning control space** (`LINK §2.13`). Follows directly: an unconfigured node has no NodeId and therefore cannot form a valid identifier, yet must transmit to acquire one. The profile has to reserve space for this before freezing, which is a real cost against an exhausted field.
- **The reassembly context model** (`LINK §2.8`): keyed by `(ingress Link, CAN identifier)`, one active context per key, drawn from a fixed pool, where exhaustion rejects rather than evicts. Evicting an in-progress reassembly would convert local pressure into phantom loss on an unrelated Wire.
- **The ordered transmit procedure** (`LINK §2.12`), whose ordering is itself the requirement, plus the rules that transmit selects the smallest legal `N` against net capacity and that a failure after START aborts the whole PDU without retrying.

Also recovered: a stricter PDUA depth policy for Critical QoS, justified by arbitration monopolization rather than memory (`LINK §2.6`); local scheduling realism — non-preemptible frames, controller mailbox limits, and local ordering as no bus-wide guarantee (`CORE §14.2`, `LINK-9`); QoS is not flow control (`QOS-6`); native CAN acknowledgment is not WS delivery (`ERR-2`); static profile selection with no negotiation (`LINK-8`); transmit ownership holding in every phase (`LINK-10`); rejection being silent on the wire (`CORE §18.1`, `ERR-1`); the four-phase commissioning model (`DEPLOY §1.2`, `CFG-11`); boundary-value conformance vectors and loss amplification with `N` (`CONFORM §2`, `CONFORM §3`); and exit criteria for provisional status (`CONFORM §6`).

One structural note: `ERR` is a new invariant family, added because rejection semantics fit none of the twelve established in revision 0.5. `LINK §2` subsections shifted by one to accommodate the new §2.1, and inbound references were updated. No existing invariant ID changed.

---

# 9. Revision 0.5 — document set split

The single working document `wirespaces_architecture_preliminary.md` (3,891 lines) was split into the current six documents plus this register. Content was preserved; the changes were structural.

Material moved out of the main line into `FUTURE` under one criterion: **not yet designed, and its absence cannot cause a wrong implementation decision today.** Both clauses were required, which is why several forward-looking items stayed:

- Boundary statements stayed in `CORE`: Origin failover is not a Wire feature, composition must flatten, redundancy owns six responsibilities, a constrained LLL must not grow into a transport, two WireSpaces do not merge by being connected.
- Optional-but-current features stayed with their subsystem: credit flow control, promiscuous mode, per-Wire top-talker telemetry, QoS-Full. With one implementation, "optional" and "future" are the same thing, but an implementer of the mandatory part needs the optional part adjacent.
- Where a topic was both constraint and ambition, the constraint was compressed into `CORE` and the discussion moved. This applies to static capacity analysis, redundancy direction, bulk transfer, and cross-WireSpace identity.

Also in this revision:

- Invariants were regrouped by topic and given stable IDs (`REG §4`), replacing a flat list of 55 positional entries. The old numbers are not preserved; the mapping was one-to-one and no invariant was dropped or added.
- Cross-references became document-coded (`CORE §6.2`). A bare `§x` always means the current document.
- The interoperability disclaimer was reduced from a subsection to a line, and conformance vectors were re-justified: the reason is divergence between the project's own C++, Python, and RTL implementations, not external implementers, of whom there are none.
- Per-document status headers replaced the single front matter.

---

# 10. Revision 0.4 — recovered from `WS_old/network/architecture_overview.md`

That document describes an earlier generation whose core model — the directed one-source/one-or-more-sink Wire, `{WireBand, RoutingCode}`, `RoutingAlias`, `PathTag`, Route as an object distinct from Wire, `ParticipantId`, and Tap/Relay/Replicator as base roles — is listed in `REG §5`. What survived:

- **Port versus Endpoint** (`CORE §1.6`): a Port is the local typed directional interface; an Endpoint is the network-visible termination. `TxBinding` is an output Port, so the receive side got the same treatment.
- **Transport is a Service-semantics choice, and reliability is not assumed safer than loss with freshness detection** (`CORE §20.2`). The sharpest idea in that document, with no prior equivalent here. It pairs directly with latest-value queue policy.
- **Freshness and staleness as declared Service properties** (`CORE §21.4`), with duplicate and stale data added to the error categories.
- **Link independence is conditional** (`CORE §13.4`): link-independent only inside declared size, timing, and transport compatibility. Revision 0.3 made the claim unqualified even though Link capabilities already had the machinery to check it.
- **Master-initiated and polled Links** (`CORE §1.7`, `LINK §7`), including I2C and SPI as in-scope Physical Links, and `LINK-3`.
- **Higher-level composition must flatten** (`CORE §19.2`), taken as a constraint without the component catalog.
- **What a redundancy composition owns** (`CORE §23.11`), plus the distinction between path redundancy and voting across independent producers.
- **Electrical visibility is not membership** (`CORE §12.6`). Needed precisely because flood-and-filter was chosen.
- **Proximity does not imply locality** (`CORE §4.5`), the inverse of the FPGA rule.
- **WireSpace as an identity universe** (`CORE §4.6`), recovered as scoping only.
- **Privileged capabilities require a separate build** (`CORE §22.1`), generalizing the promiscuous-mode rule into a stated policy with a capability list.
- **No interoperability is claimed.**

Recorded as a deliberate divergence rather than a recovery: that document's invariant 13 held that reachability and authority exist **only** where the Wiring Manifest configures them. WireSpaces relaxed this so that anonymous `kLocalBus` and Level 0 use work with no Manifest at all. This is the principal philosophical difference between the two generations, and it is a trade, not an oversight.

Noted but not recovered: Domain Control as a named layer, and the reusable Communication Component catalog. Both are in `FUTURE`.

---

# 11. Revision 0.3 — recovered from `wirespaces_simplified_wire_and_autowiring_design_change.md`

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
- **Promiscuous / bring-up operation** (`DEPLOY §3.1`).
- **A future 29-bit Classical CAN profile** (`FUTURE §11`).

Rejected rather than recovered: ephemeral route repair / learned forwarding (recorded in `REG §5`), the 3-bit canonical WireNumber on CAN, and `Main`/`Peer`/`PeerId`, `Link Engine`, and WireContract as specified there.

---

# 12. Revision 0.2 — merged the overview into the snapshot

Merged from `wirespaces_high_level_architecture_design_overview.md`, having been confirmed as still current: Wire Splicing including the `spliceWire` route field and the before-egress ordering rule; the Default Internal Debug Wire; Service TX bindings and `Inline`/`Serialized` delivery; copy-based buffer ownership; Namespace and EndpointId allocation; why a bus rather than pairwise edges; small-device Router notes; Ethernet/CAN adaptation asymmetry; implementation scaling profiles; and the worked examples.

Reconciled where reinstating those collided with snapshot text: `SCOPE-1` carries the splice carve-out; the field-preservation invariant names splicing explicitly; the route entry carries an optional `spliceWire`; the Organizer may install splice mappings; anonymous `kLocalBus` is explicitly not spliceable.

Corrected: the Classical CAN payload budget under the CRC-8/CRC-16 policy. The overview's table (N=1→5, N=2→12, N=3→19, N=4→26) was a pre-integrity gross budget and overstated usable bytes for N >= 2.

Carried forward from the snapshot and still superseding the overview: PDUA MaxN 8 (was 16) and the 3+3 FrameControl layout (was 1+4); COBS favored over HDLC-style escaping; UART CRC-16 choice reopened; QoS-Minimal/QoS-Full profiles; congestion as a normal send outcome; and all terminology supersessions.

---

# 13. Superseded source documents

| Document | Superseded by |
|---|---|
| `docs/archive/wirespaces_high_level_design_overview.md` | Revision 0.2 |
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
| `docs/archive/decision-bounded-endpoint-delivery.md` | Revision 0.9 (`CORE §9`, `REG` DISP-*) |

`WS_old` is left intact as its own historical tree; `docs/archive/` holds copies of the documents mined from it, so this document set carries its own provenance without editing the old one.

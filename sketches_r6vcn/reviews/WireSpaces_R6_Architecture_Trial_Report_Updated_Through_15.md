# WireSpaces R6 Architecture Trial Report
## Updated synthesis through Archetype 15

**Status:** Working architecture synthesis  
**Scope:** Reviewed R6 architecture trials from Archetype 01 through Archetype 15, including the later adversarial trials beyond 11C  
**Date:** 2026-08-23

---

## 1. Executive summary

The trial campaign has now exercised WireSpaces R6 across a wide range of embedded-system shapes: small CAN machines, heterogeneous gateways, symmetric peer systems, multicore automotive topologies, redundant flight-control architectures, large field-serviced battery systems, hard-real-time motion networks, high-rate streams, and opportunistic mobile wireless meshes.

The strongest conclusion is deliberately narrower than “R6 works everywhere”:

> **Within this centrally engineered embedded-system corpus, no reviewed archetype has produced a counterexample to deployment-scoped Participant identity, reusable Services, or ordinary tree-shaped Logical Buses.**

The trials *have* found clear boundaries. Those boundaries are useful because they are concentrated in places where the underlying communication semantics genuinely differ from ordinary datagram-oriented embedded networking:

- independently commissioned identity universes and field-service lifecycle;
- redundant physical paths;
- microsecond/sub-millisecond cyclic process images;
- hardware phase synchronization and group-atomic application;
- high-rate streams and large-object transfer without a defined segmenting Transport;
- opportunistic geographic routing and dynamic mesh convergence;
- shared-fabric bandwidth guarantees that must be supplied below the Wire model.

The central R6 model has therefore become **more credible**, not less. The pressure is not distributed randomly throughout the architecture. It clusters at explicit boundaries between ordinary embedded Service communication and specialized networking or deployment mechanisms.

The current stable center is:

- **Participant** — canonical deployment-scoped source / acceptance identity;
- **Wire** — static logical propagation scope;
- **Service / Endpoint** — reusable application capability and interaction surface;
- **forwarding** — same canonical PDU, same author, different carrier;
- **composition** — consume one interaction and author another;
- **Link projection** — constrained or specialized representation of canonical semantics.

The principal follow-up areas are now narrower:

1. define the intended contract for redundant/bonded Links versus separately visible redundant paths;
2. clarify the operational role and configuration cost of participant-location pruning;
3. review splice design-center before adding selector machinery;
4. finish a reusable sequenced/segmenting Transport for streams and large objects;
5. formalize the distinction between deployment ParticipantId and immutable hardware/vendor identity;
6. make committed CAN11 WireAlias scope explicitly local to a physical classifier/interface;
7. synchronize Guest CAN trial material with the current Guest design;
8. document where specialized native data planes sit below or beside WS without ceasing to expose WS Services.

---

## 2. Scope and methodology

This report synthesizes the reviewed trial corpus:

- **01** — Small Single-Controller CAN Machine
- **02** — Raspberry Pi Robot + ECUs + Dev PC
- **05** — Heterogeneous Gateway
- **06** — Overlapping Logical Scope / Bandwidth Isolation
- **07** — Symmetric Peer CAN, original and denser revised form
- **08** — Flat Ethernet Peer Mesh
- **09** — Distributed Chassis CAN Cell
- **10** — Automotive Zone + Central Compute Slice, independent mappings
- **11** — Redundant eVTOL Flight Control, three independent mappings
- **12** — Modular BESS, field-serviced and multi-vendor, adversarial mappings
- **13** — Hard-Real-Time Cyclic Motion + High-Rate Streaming, two adversarial mappings
- **14** — Mobile Warehouse Fleet on Opportunistic Multi-Hop Mesh
- **15** — Sub-Millisecond Redundant Motion-Control Cell

There were no reviewed 03 or 04 submissions in this sequence.

The trials are architecture thought experiments, not implementation benchmarks. They test whether the model remains natural and bounded under increasingly difficult topologies and deployment constraints. They do **not** validate code size, CPU cost, exact failure timing, API quality, or final Link-profile encodings.

Where mapper conclusions and reviewer-normalized conclusions differ, this report distinguishes:

- **independent convergence** — multiple agents reached the same conclusion before review;
- **mapper friction** — how awkward the mapping felt to the mapper;
- **normalized burden** — structural/configuration cost after correcting counting or modeling mistakes;
- **reviewer-adjusted conclusion** — a technical conclusion corrected after mapping review;
- **open disagreement** — plausible alternatives remain.

That separation matters because review corrections can otherwise make post-review consensus look like independent evidence.

---

## 3. Threats to validity

The trial campaign has meaningful limitations.

### 3.1 Domain bias

The archetypes are mostly centrally engineered machines: automotive, industrial, robotics, flight control, energy storage, and embedded gateways. They are not arbitrary Internet-scale or ad-hoc networks.

### 3.2 Model-aware mappers

Mappers were given WireSpaces concepts and invariants. They were not blind evaluators comparing unrelated networking architectures.

### 3.3 Negative controls are uneven

Some adversarial trials attack constrained-Link projections, timing, or deployment ownership more than the canonical Participant/Wire model itself.

### 3.4 Review normalization can hide friction

Several mappers over-counted forwarding state or misused forwarding/splice/composition. Correcting those errors improves the architecture result, but the original confusion is itself evidence about documentation and tooling.

### 3.5 No executable validation yet

The trials do not prove:

- runtime cost on tiny MCUs;
- queue/ownership/concurrency ergonomics;
- actual CAN, Ethernet, SHM, or radio profile performance;
- failure recovery timing;
- generated-configuration usability;
- stream Transport behavior;
- HIL/SIL workflow quality;
- field commissioning UX.

### 3.6 What a serious R6 failure would have looked like

A strong counterexample would have required one or more of:

- artificial Participant roles that exist only to satisfy routing;
- Participant identities that must change under ordinary faults or controller takeover;
- Wire count growing combinatorially relative to real physical/application structure;
- dynamic routing/election machinery required for ordinary target systems;
- per-Link reinvention of application Service identity;
- topology state that cannot be made reasonably bounded or auditable;
- canonical semantics that cannot be reconstructed without changing the meaning of the interaction.

The later adversarial trials finally produced real failures in some *subdomains* — cyclic process images and mobile mesh topology — but did not require reopening the stable Participant/Service center.

---

# 4. Core finding: Participant identity remains the strongest R6 result

Across the entire corpus, Participant identity has been strikingly stable.

The simple case is unsurprising: one controller and several leaves on one CAN bus map directly.

The more important evidence comes from systems with:

- symmetric peer controllers;
- multicore SoCs;
- separate application and network/platform domains;
- diagnostic/service authorities;
- redundant active/shadow controllers;
- remote tools and ground stations;
- hundreds of field-service devices;
- devices participating in both WS Services and specialized native data planes.

No reviewed trial required a canonical “master,” “Origin,” “Node,” active-primary routing role, or ParticipantId reassignment during ordinary failure.

A consistent rule emerged:

> **ParticipantId identifies an independently meaningful canonical source/acceptance domain, not a physical device role, transport owner, authority rank, or current redundancy state.**

Examples:

- a CAN-controller-owning gateway core does not become the canonical source of an application command merely because it owns the transceiver;
- Controller A and B retain distinct identities across takeover;
- Ground Station authority remains application state rather than address semantics;
- a servo drive remains one WS Participant even when its cyclic control data plane is native and non-WS;
- physical hardware UUID/serial remains separate from deployment role identity.

This is now one of the highest-confidence conclusions in the trial campaign.

---

# 5. Logical Bus Wires: strong for stable propagation scopes, deliberately not universal

## 5.1 Where Wires work naturally

Wires perform best when they represent stable communication, locality, and failure scopes.

Strong examples include:

- one plant CAN bus;
- separate robot-control and developer-access scopes;
- heterogeneous SHM→CAN or SHM→Ethernet→CAN propagation trees;
- automotive fieldbus scopes;
- stable service scopes over routed infrastructure;
- service/maintenance scope around a specialized deterministic motion network.

A Wire may span heterogeneous Links without changing canonical source identity.

This became one of the most distinctive positive results of the trials.

## 5.2 Wires are not cables

A physical cable does not imply a distinct Wire, and several Wires may legitimately share one physical Link.

Likewise, a physical topology may contain cycles while a particular Wire realization remains loop-free because not every electrically visible interface forwards or binds that Wire.

Archetype 13 demonstrated this cleanly: SHM and Ethernet formed a physical loop through two controller domains, but only one domain forwarded between them, so the relevant Wire realization remained a tree.

## 5.3 Wires are not traffic classes

Archetype 06 established that overlapping Wires do not create bandwidth isolation. A PDU on W1 does not become a W2 PDU merely because the same Participants also share W2.

Archetypes 13 and 15 reinforce the same point on switched Ethernet:

> **Wire isolation is propagation isolation, not physical resource reservation.**

Two Wires on the same Ethernet switch still contend for the same queues and ports unless the underlay supplies shaping/scheduling.

## 5.4 Wires are not dynamic geographic groups

Archetype 14 is the clearest negative control.

The natural audience is “robots near me now” or “AMRs affected by this zone now.” Those sets change continuously with motion. Exposing that as Wire membership produces dozens of semantic membership changes per second and hundreds of route changes per second.

The correct boundary is:

> **WS may run over a dynamic routed network, but WS is not itself a MANET/geographic-routing protocol.**

Stable Service scopes over an opaque routed underlay are reasonable. Dynamic proximity groups and mesh convergence remain underlay/application concerns.

---

# 6. Intra-device and inter-device continuity are strongly validated

Several trials independently use shared memory as an ordinary Link.

The same Service-facing model can span:

- same-process communication;
- shared memory between domains;
- Ethernet between computers;
- CAN/CAN-FD to field ECUs;
- scheduled mailbox channels;
- routed wireless underlays.

The important distinction is not “IPC versus network.” It is whether the communication preserves the same canonical interaction.

This supports one of the project's central product claims:

> **A Service boundary can move across deployment boundaries without redesigning the Service identity model, even though latency, failure, and bandwidth remain physical properties of the selected Links.**

This appears particularly valuable for:

- multicore SoCs;
- simulator/host deployments;
- gateways;
- field-service tools;
- native deterministic devices whose control data plane remains specialized but whose Service plane is unified.

---

# 7. Forwarding, observation, composition, and splice

The trials repeatedly showed that these concepts are useful, but also repeatedly confused them.

## 7.1 Forwarding

Forwarding preserves:

- canonical Wire;
- canonical source;
- canonical destination;
- Endpoint;
- payload meaning.

A gateway core that owns a CAN controller does not become the canonical source merely because it emitted the physical frame.

## 7.2 Observation

Observation gives an additional consumer access to the same canonical PDU.

It does not rewrite destination identity and does not create a second PDU or VCN relationship.

This was a useful efficiency result in Archetype 06.

## 7.3 Composition

Composition emerged as a **positive architectural success**, not merely a correction mechanism.

It is the right boundary when a component:

- summarizes;
- transforms;
- interprets;
- applies local authority;
- aggregates;
- crosses a capacity boundary;
- authors a new decision.

Examples include:

- ZoneControl consuming central time/state and re-authoring a local fieldbus publication;
- condition-monitoring analyzers turning raw vibration into diagnosis/trend;
- vision controllers turning images into registration corrections;
- BESS gateways exposing container-level plant facades;
- platform domains producing summarized health over scarce links.

A strong general rule emerged:

> **Composition is how WS crosses semantic, authority, or capacity boundaries without lying about source identity.**

## 7.4 Splice

Splice should remain conservative.

The later eVTOL mappings pushed splice toward selective service routing between already-populated scopes. Before adding destination or Endpoint selectors, the more important question is whether such cases should be composition instead.

The current follow-up is therefore:

> **Review the intended splice design-center before making it more programmable.**

Selector-heavy splice risks turning a simple scope-projection primitive into routing/policy machinery.

---

# 8. CAN11: clear design center and graceful boundary

## 8.1 Default two-Main map is well justified

The strongest repeated positive pattern is:

- one operational/control authority;
- one diagnostic/service authority;
- many leaves.

Examples include automotive body networks and the BESS rack design.

This is much more compelling than treating MainA/MainB as a legacy master/slave concept.

## 8.2 Peer graphs require explicit maps but remain manageable

The original four-peer trial required an explicit relation map.

The denser eight-domain version still required only a modest number of actual relations because the system was not a fabricated full mesh.

The key lesson is:

> **Count real Participant relationships, not all theoretically possible pairs.**

## 8.3 Real CAN11 pressure appears in the high twenties

Archetype 09 reached approximately 29 usable ordinary VCN relationships in production.

With VCN 3 reserved, the correct denominator is:

- 32 total values;
- **31 usable ordinary relationships**.

At this point CAN11 remains valid, but CAN29 begins to win on headroom and simplicity.

## 8.4 Multiple aliases may legitimately map the same Wire

The dense CAN11 trials showed that multiple WireAliases can be useful for relation-capacity partitioning as well as migration.

## 8.5 Committed WireAlias scope should be Link-local

Archetype 12 independently exposed an ambiguity: twenty isolated RackCAN buses should be able to reuse the same alias value.

The natural rule is:

> **Committed WireAlias values are scoped to the physical CAN interface/classifier that interprets them.**

Otherwise a small Link-local compression field accidentally becomes a deployment-global resource limit.

Guest remains a different case because its no-alias projection deliberately has stronger global assumptions.

## 8.6 Guest trial material needs synchronization

The #10 mappers followed the experiment overlay they were given. The issue is not mapper error; the overlay is behind the newer Guest design direction.

Before more Guest conclusions are accumulated, the experiment material should be synchronized with the current profile naming/control-VCN rules.

---

# 9. CAN29 and rich Links confirm that VCN complexity is Link-local

Rich Links repeatedly show that canonical peer density does not imply pair-table complexity.

Flat Ethernet peers and CAN-FD/29-bit motion/actuator networks can carry direct or nearly direct canonical identity without CAN11-style relation compression.

This strongly supports the architectural layering:

- canonical model: Participants + Wires + Endpoints;
- CAN11: constrained projection;
- richer Links: direct representation.

VCN complexity is therefore best understood as a **CAN11 representation cost**, not an inherent WS communication cost.

---

# 10. Configuration burden: distinguish authored entropy from generated state

Several agents initially reported large object counts.

Two recurring corrections matter.

## 10.1 Forwarding was often over-counted

The preferred ordinary form is:

```text
Wire -> LinkBitmask
egress = mask & ~IngressLink
```

A three-branch tree is one membership/forwarding object, not three independent route decisions.

Ingress-sensitive forwarding remains available when a topology genuinely needs asymmetry.

## 10.2 Generated records are not independent human decisions

Archetype 12 is a good example.

A twenty-rack BESS may serialize hundreds of deployment records, but most are template instances:

- role allocation;
- Wire number;
- default VCN positions;
- alias binding;
- membership;
- forwarding.

Human-authored information may be much smaller:

- rack slot;
- hardware UUID/serial;
- installed revision;
- accepted/replaced asset state.

A useful distinction is:

- **generated deployment size**;
- **human-authored topology burden**;
- **asset/lifecycle data burden**;
- **mapper-perceived friction**.

The first and third may be significant while the second remains mild or moderate.

---

# 11. Field service and multi-vendor identity: Archetype 12

Archetype 12 is the first adversarial trial to attack organizational identity ownership rather than topology.

It found a real boundary, but the result must be interpreted carefully.

## 11.1 Factory identity is not automatically ParticipantId

WireSpaces already benefits from separating:

- immutable hardware/vendor identity;
- canonical deployment role identity.

If a vendor ships an immutable serial number, UUID, Link-local CAN identity, or factory-local slot ID, the Organizer can still assign a deployment ParticipantId.

The hard case is specifically:

> a device treats a short factory-assigned value as its immutable canonical ParticipantId, multiple vendors collide, and none permit reassignment.

No architecture with unique short canonical addresses can transparently merge such universes without:

- renumbering;
- translation;
- namespace isolation;
- globally unique identity.

This is therefore a deployability constraint that should be explicit:

> **A generally deployable WS-native Participant must accept deployment identity assignment, support a Link-local projection to one, or explicitly remain behind an identity/composition boundary.**

## 11.2 Replacement fits role identity cleanly

A replacement module can retain the same functional ParticipantId while hardware UUID/serial changes.

That is desirable.

The missing piece is not routing semantics; it is lifecycle tooling:

- replacement event;
- hardware history;
- retirement;
- tombstone/reuse policy;
- version/capability validation;
- rollback/source of truth.

This belongs primarily in Organizer/deployment tooling and Identity/asset metadata.

## 11.3 Extended addressing has a natural real-world use

A single twenty-rack container remains inside the ordinary 8-bit PID range.

A three-container plant reaches more than 500 listed Participants and therefore naturally motivates the proposed wider canonical ParticipantId.

This is strong evidence for the extended-address escape hatch.

Importantly, a CAN11 VCN map can reconstruct a wider canonical PID without consuming additional CAN identifier bits; the binding table simply stores the wider canonical value.

That suggests a concrete profile requirement:

> **CAN11 VCN bindings should be able to reconstruct extended canonical ParticipantIds when the local implementation declares that capability.**

## 11.4 Plant federation is a product-boundary choice

Two valid architectures exist:

**One plant WireSpace**
- direct module-level standard Services;
- requires coordinated/extended identity or translation.

**One WireSpace per container**
- plant sees container-level facade;
- composition/proxy at the boundary;
- less transparent module access;
- often more natural operationally.

WireSpaces should not promise transparent federation of independently commissioned identity universes “for free.”

---

# 12. Physical redundancy: refined conclusion after Archetypes 11 and 15

The eVTOL trial initially concentrated much of the open pressure around redundant physical paths.

Later trials sharpened the picture.

## 12.1 RS-485 triangle

All three eVTOL mappers independently converged on one pair Wire per point-to-point leg.

For that specific cross-channel topology, this is arguably a **complete current-R6 solution**, not evidence of a missing protocol primitive.

It gives:

- all three physical legs active;
- no forwarding dependency between flight computers;
- exact failure visibility;
- complete symmetry;
- trivial loop freedom.

The broader question of a semantic one-scope physical ring with alternate reachability remains separate.

## 12.2 Dual A/B actuator buses

Here the ambiguity is more genuine.

If the application semantics treat A and B as independently meaningful safety channels, they should remain separately visible.

If instead A and B are merely redundant realizations of one Link service, hiding redundancy below WS may be better.

## 12.3 Archetype 15 supplies a strong model

The deterministic redundant ring presents:

- two physical directions;
- duplicate paths;
- ring break recovery;
- duplicate suppression;
- clock continuity;

as **one coherent Link Interface** to WS.

The native Link implementation owns:

- path selection;
- deduplication;
- recovery;
- cycle identity;
- physical clocking;
- babbler containment.

WS sees one Link and receives Link-status telemetry describing the physical state.

This suggests a useful criterion:

> **If redundancy is a property of the Link service, bond it below WS. If path diversity is part of application semantics, expose the paths separately.**

This may eliminate the need for a general “Redundant Wire” concept.

The remaining follow-up is therefore better framed as:

> **Do we need a WS-level redundancy mechanism at all, or only a clean contract for redundant/bonded Link profiles?**

---

# 13. Participant-location pruning: useful and architectural, but not free

Pruning is already permitted by the architecture.

The open question is not whether it exists, but its expected deployment role.

Key questions:

- Is pruning a normal supported profile or an exceptional optimization?
- What static location state must be generated and audited?
- How are multi-homed Participants represented?
- What happens when reachability changes?
- When is pruning cheaper than splitting Wires?

The later trials show two things simultaneously:

1. flood-and-filter can force extra Wire decomposition or irrelevant constrained-Link traffic;
2. carefully chosen Wire scopes and fixed Link admission budgets can solve many cases without making pruning universally mandatory.

Therefore:

> **Pruning trades Wire proliferation for location/configuration state. It should be treated as an explicit design choice, not as free optimization.**

---

# 14. Hard-real-time cyclic motion: a clear non-goal for ordinary WS PDUs

Archetypes 13 and 15 independently converge on the same conclusion.

## 14.1 Per-device cyclic PDUs are the wrong abstraction

At high cycle rates, tiny independently framed PDUs fail for several reasons:

- Ethernet minimum-frame overhead;
- canonical-header overhead relative to tiny payloads;
- extremely high PDU rates;
- Router/Endpoint processing cost;
- no common apply instant;
- no cycle completeness;
- no distributed coherent snapshot.

Archetype 13 showed A48 reaching roughly 192,000 cyclic PDU-equivalents/s on a 100-Mbit link.

Archetype 15 showed Config C requiring more than 2 million minimum-framed cyclic PDUs/s and exceeding 1 Gbit/s.

These are clean negative results.

## 14.2 Native process images are the correct mechanism

The natural representation is:

- generated static offsets;
- one cyclic command image;
- one feedback image;
- cycle number;
- distributed clock;
- validity/working counters;
- timed group application;
- native schedule;
- native redundancy.

Ordinary canonical PDU semantics should not be forced into that path.

## 14.3 But the device remains a WS Participant

This is the crucial positive result.

A servo drive may simultaneously have:

**Native cyclic path**
- setpoint;
- feedback;
- phase;
- safety;
- process-image validity.

**WS Service path**
- Identity;
- Version;
- Health;
- fault detail;
- parameters;
- tuning;
- logs;
- firmware;
- Link status.

The boundary is therefore interaction-specific, not device-specific.

A general design principle follows:

> **A Participant may expose ordinary WS Services while some specialized high-rate or tightly synchronized interactions use a native data plane below WS.**

This substantially broadens the useful integration envelope without pretending WS is a universal real-time fieldbus.

---

# 15. Specialized cyclic projection: total elision is not the main issue

Archetype 13 raised the question of a scheduled passing-frame profile that statically implies every canonical field.

Zero transmitted canonical metadata is not automatically invalid if the binding unambiguously reconstructs the canonical interaction.

However, the more important issue is semantic:

- the useful object is the whole process image;
- slot ownership, phase, completeness, validity, and apply time are first-class;
- treating every slot as an independently dispatched PDU adds machinery without adding meaning.

Therefore:

> **A totally projected static cyclic data plane may share WS Participant/Endpoint identity and generated configuration, but it is not an ordinary canonical-PDU transport.**

A future specialized integration may share the authoritative Wiring/identity source without pretending the cyclic plane is just another asynchronous Wire.

---

# 16. Streams and large objects: carriage works, Transport is the gap

Archetype 13 produced a valuable distinction.

For sensible block sizes, canonical header overhead is tiny:

- vibration blocks: well under 1%;
- image chunks: well under 1%.

The Endpoint storage model also behaves sensibly:

- Snapshot is wrong when every block matters;
- Queue is the correct bounded history-preserving primitive;
- overflow is visible and countable.

The missing functionality is higher-level Transport behavior:

- sequence;
- receiver-side gap detection;
- duplicate handling;
- segmentation;
- object integrity;
- retry;
- resume;
- end-to-end flow control/backpressure.

This is a much narrower conclusion than “WS needs a separate streaming architecture.”

The strongest synthesis is:

> **WS PDU carriage and Queue storage are suitable for bounded stream blocks; the missing component is a reusable sequenced/segmenting Transport for continuous streams and large objects.**

This is now one of the clearest implementation priorities after the first prototype.

---

# 17. Time semantics: separate three concepts

The motion trials exposed an important vocabulary distinction.

## 17.1 Wall-clock / event-correlation time

Portable Service-level concept.

Useful for:

- logs;
- event correlation;
- archive metadata;
- trend records.

## 17.2 Link phase reference

Hardware/Link-profile property.

Useful for:

- sub-microsecond synchronized servo application;
- distributed cyclic schedule;
- native process-image timing.

Not generally transitive across gateways.

## 17.3 Local arrival timestamp

Router/Endpoint-local monotonic acceptance metadata.

Useful for:

- freshness;
- local timeout logic;
- diagnostics.

No cross-Participant clock meaning is implied.

The standard Service story remains coherent as long as these are not conflated.

---

# 18. Group atomicity and coordinated operational state

Ordinary broadcast does not imply:

- every recipient received the same cycle;
- every recipient held the same value;
- every recipient applied on the same physical tick.

That remains a native deterministic-Link/process-image property in the tested motion systems.

However, several trials independently rediscovered a higher-level application pattern:

- command target mode/state;
- report actual state;
- report blocking reason;
- abort/fault;
- advance through phases.

Examples include:

- coordinated motion phase-in;
- flight mode;
- commissioning state.

This looks more like an **ecosystem Service opportunity** than a protocol primitive.

A generic operational-state / coordinated-mode Service may be worth exploring later.

---

# 19. Dynamic routed wireless: explicit boundary from Archetype 14

Archetype 14 is a successful negative control.

## 19.1 Stable identity survives mobility

AMRs, pallets, docks, schedulers, and coordinators retain stable Participant identity while:

- routes change;
- APs roam;
- peer paths appear/disappear;
- partitions form and merge.

This is a positive R6 identity result.

## 19.2 WS should not model route topology

Attempting to expose mesh hops/zones as Wires produces:

- hundreds of transient Wire-like scopes;
- dozens of semantic membership changes per second;
- hundreds of next-hop changes per second;
- stale reviewed configuration immediately;
- undefined distributed merge behavior.

That is clearly outside the intended static/read-mostly model.

## 19.3 Stable Service scopes over an opaque routed Link are reasonable

A dynamic routed underlay can provide:

- unicast routing;
- roaming;
- duplicate suppression;
- sleeping-node delivery;
- local partition routing.

WS can sit above it with stable directed Services.

## 19.4 Proximity interaction may still need a native data plane

High-rate “near me now” pose exchange can be expensive even as repeated directed WS PDUs.

This suggests that high-rate local collision/pose exchange may itself belong to a native proximity-aware wireless primitive, while WS carries:

- task state;
- diagnostics;
- asset handoff state;
- Health;
- update;
- lower-rate coordination.

Again, the useful boundary is interaction-specific rather than Participant-specific.

---

# 20. Switched fabrics and QoS

The later trials sharpened an important non-goal.

Canonical QoS is a cross-Link **classification hint**, not a proof of deadline or bandwidth reservation.

On shared switched Ethernet:

- different Wires share the same physical fabric;
- switch queues are not WS Participants;
- DSCP/802.1p-like markings only matter if the underlay honors them;
- average bandwidth does not prove latency.

Therefore:

> **QoS is not a replacement for schedule, shaping, admission, or switch configuration.**

This is not a defect in the Wire model, but should be stated clearly in the deployment documentation.

---

# 21. Link capability metadata: keep it useful, not universal

The motion trials reasonably ask for enough Link metadata to prevent obviously invalid placements.

A minimal direction may be useful:

- asynchronous;
- master-polled;
- scheduled-cyclic;
- bounded mailbox capacity;
- supported Transport types;
- representable PDU sizes.

But the trial results do **not** justify turning CORE into a universal schedulability language.

Detailed engineering facts such as:

- exact 500 µs cycle;
- 1 µs phase;
- per-node traversal;
- switch residence bounds;
- group atomicity;
- safety reaction proof;

belong largely in Link-profile-specific engineering and system timing analysis.

The useful principle is:

> **Expose enough capability to reject category errors; do not require WS configuration to encode the entire machine timing proof.**

---

# 22. Standard Services remain the likely product-level payoff

Across the corpus, Services reuse existing Wires and Participant relationships rather than generating new topology objects.

Repeatedly useful Service classes include:

- Identity / Version;
- Health;
- fault/diagnostic state;
- update;
- Link status;
- time;
- logs/events;
- telemetry;
- RPC/query;
- operational state/mode;
- configuration/inventory.

The later adversarial trials strengthen this result because even systems whose **primary data plane is outside WS** still benefit from a common Service plane.

This is especially important for:

- deterministic motion devices;
- field-serviced BESS modules;
- AMRs on routed wireless;
- multicore automotive nodes;
- flight-control systems with specialized physical redundancy.

The network layer remains the foundation; the Service ecosystem remains the product-level reason to adopt.

---

# 23. Trial-by-trial disposition

| Trial | Main finding | Disposition |
|---|---|---|
| **01 Simple CAN Machine** | One controller + leaves maps trivially to one Wire and default CAN11. | Strong baseline pass |
| **02 Pi Robot** | Stable plant and dev scopes are natural; routed/developer access needs explicit composition/policy. | Strong pass |
| **05 Heterogeneous Gateway** | SHM + fieldbus Wires work; canonical source is application identity, not transceiver ownership. | Strong pass with important semantic corrections |
| **06 Motion/Safety/Telemetry** | Two-Main CAN11 pattern works; observation is cheaper than duplicate publication; Wires are not traffic classes. | Strong pass |
| **07 Peer CAN** | Explicit VCN maps handle symmetric peers; real relation count matters more than theoretical mesh size. | R6 strong; CAN11 explicit mapping mild–moderate |
| **08 Flat Ethernet Mesh** | R6 fits naturally but networking differentiation is small; Service ecosystem must justify adoption. | Natural fit, weak networking differentiation |
| **09 Distributed Chassis CAN Cell** | High-20s relations produce real CAN11 pressure; CAN29 becomes attractive for headroom. | Excellent CAN11 boundary test |
| **10 Automotive Zone** | Large heterogeneous/multicore system converges on scope-shaped Wires, two-Main body CAN, and reusable Services. | Very strong system-scale pass |
| **11 eVTOL** | Identity remains clean; ring and A/B redundancy expose physical-path questions. | R6 strong; redundancy boundary needs refinement |
| **12 Field-Serviced BESS** | Rack communication is easy; multi-vendor identity ownership, lifecycle, and federation are the real pressure. | Strong communication model; deployment/federation boundary exposed |
| **13 Cyclic Motion + Streams** | Ordinary WS PDUs fail for microsecond cyclic control; Service plane still fits; stream carriage fits but Transport is missing. | Strong boundary result |
| **14 Mobile Opportunistic Mesh** | WS should ride above dynamic routing, not model geographic neighbor/route state. | Strong negative control |
| **15 Redundant Sub-ms Motion** | Independently confirms cyclic boundary; opaque native redundant Link is natural. | Strong boundary result; useful redundancy clarification |

---

# 24. Assessment against the main WireSpaces goals

## Logical Bus propagation

**Strong within stable tree-shaped scopes.**

Main boundary:
- dynamic routed/geographic topology is not a Wire problem;
- redundant physical realization should either remain explicit or be hidden inside a coherent Link abstraction.

## Heterogeneous Link portability

**Very strong evidence.**

The same Service model spans:
- SHM;
- CAN/CAN-FD;
- Ethernet;
- serial;
- routed wireless;
- specialized scheduled mailbox channels.

## Tiny-target friendliness

**Still unvalidated by trials.**

Architecturally compatible, but real code/RAM/CPU measurements are still needed.

## Constrained-Link projections

**Strong evidence.**

CAN11 has a clear design center and graceful boundary.

Static slot/mailbox systems may also project canonical identity, but cyclic process-image semantics are not ordinary PDU transport.

## Integrated Service ecosystem

**Strong architectural evidence.**

Later adversarial trials strengthen rather than weaken this: WS remains useful even where the primary native data plane remains specialized.

## Static/bounded forwarding

**Strong on ordinary engineered topologies.**

Clear non-goal:
- no opportunistic route convergence;
- no geographic multicast maintenance;
- no universal dynamic mesh.

## Intra-device + inter-device continuity

**Very strong evidence.**

One of the most consistently successful ideas in the campaign.

---

# 25. What the trials still do not validate

The following remain prototype or later engineering work:

- runtime and code size on tiny MCUs;
- queue ownership and concurrency model;
- actual zero-copy/buffer-pool ergonomics;
- concrete CAN-FD/29-bit profile;
- concrete Ethernet profile;
- Guest CAN final encoding;
- commissioning workflow;
- extended PID implementation;
- bonded/redundant Link API;
- segmenting/stream Transport;
- recovery timing;
- configuration generator UX;
- Standard Service API quality;
- HIL/SIL/debugging workflow;
- conformance tooling;
- security/authentication profile;
- capability negotiation/version skew.

---

# 26. Updated follow-up priorities

## 26.1 Implement the stable center

The trial campaign has reached diminishing returns for Participant + Wire + Service architecture.

The next prototype should validate:

- canonical PDU representation;
- queue-based Endpoint delivery;
- one or two ordinary Links;
- forwarding;
- composition;
- Identity/Version;
- Health/Link Status;
- one request/response or publication pattern.

## 26.2 Redundancy: test the Link-layer hypothesis

Build a small executable experiment comparing:

1. two separate Wires;
2. a bonded/redundant Link implementation presenting one Link Interface;
3. a restricted WS-aware redundant-Link profile.

Measure:

- duplicate handling;
- failure transition;
- path diagnostics;
- ordering/freshness;
- configuration size;
- Endpoint-visible semantics.

Current evidence increasingly favors **redundancy below WS when the Link already owns a coherent redundant-delivery contract**.

## 26.3 Define pruning as a deployment choice

Document:

- expected use cases;
- authored location state;
- multi-homing;
- failures;
- tooling/audit implications.

Do not treat it as free.

## 26.4 Review splice design-center

Before adding selectors, decide whether populated-scope→populated-scope service access should be splice or composition.

## 26.5 Implement a sequenced/segmenting Transport

The stream trials give a concrete need:

- sequence;
- gap detection;
- segmentation;
- integrity;
- retry/resume;
- bounded flow control.

## 26.6 Clarify deployment identity contract

Make explicit that:

- hardware UUID/vendor identity and ParticipantId are distinct;
- WS-native third-party devices must accept deployment identity, support projection, or live behind a boundary;
- replacement/lifecycle history belongs in Organizer/tooling and Identity metadata.

## 26.7 Make committed WireAlias scope normative

State that committed alias values are local to the physical CAN classifier/interface.

## 26.8 Synchronize Guest CAN experiment material

Bring trial overlays in line with the current Guest profile direction before using further Guest results as normative evidence.

## 26.9 Document specialized-data-plane boundaries

Explicitly state that WS may coexist with:

- cyclic process images;
- hardware phase synchronization;
- black-channel safety;
- dynamic mesh routing;
- proximity broadcast;
- native redundant-ring mechanisms.

A Participant can remain first-class in WS without every interaction being a WS PDU.

---

# 27. Bottom-line architecture judgment

The later adversarial trials improve the case for R6 because they found sensible limits.

The architecture does **not** appear to be universal, and should not try to become universal.

It covers a large middle ground:

- ordinary embedded command/status;
- diagnostics and update;
- multicore IPC;
- fieldbus communication;
- gateways;
- heterogeneous networks;
- host/device continuity;
- routed network overlays;
- moderate-rate control and telemetry;
- standardized Services;
- bounded streams once a real Transport exists.

It should deliberately stop short of pretending to own:

- microsecond/sub-millisecond cyclic process images;
- distributed hardware phase;
- group-atomic real-time apply;
- dynamic mesh route convergence;
- geographic multicast;
- certified safety transport semantics;
- arbitrary independently commissioned namespace federation without translation.

The strongest updated synthesis is:

> **WireSpaces is a common embedded Service and communication fabric for the large middle ground between local function calls and highly specialized real-time or dynamic networking systems.**

Within that range, the trial campaign repeatedly shows that **system complexity grows faster than WireSpaces conceptual complexity**.

Outside that range, the best results come from preserving a clean boundary: keep specialized native mechanisms specialized, while reusing WS identity, Services, forwarding/composition semantics, and tooling around them.

That is a healthy architecture boundary rather than a missing feature.

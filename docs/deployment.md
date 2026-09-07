# WireSpaces — Deployment, Discovery, and Tooling

**Status:** Private first draft; the Organizer and Manifest are directions, not designs
**Scope:** How a WireSpaces system is discovered, commissioned, configured, and observed from a host
**Relationship to `CORE`:** Everything here configures the runtime described in `CORE`. None of it is required for basic communication

Host tooling is a first-class part of WireSpaces, not demo scaffolding. But the ordering matters: a device with one Link works with no tooling and no configuration at all (`CORE §5.3`), and everything in this document is a capability rather than an admission requirement (`INTRO §6`).

---

# 1. Discovery, Commissioning, and the Organizer

WireSpaces should support both explicitly configured deployment and very low-configuration lab use.

## 1.1 Organizer

A host-side **Organizer** is a temporary/explicit configuration authority. It is not a permanent host role and does not become part of canonical Wire forwarding.

An Organizer may:

- discover devices;
- read stable identity;
- assign one deployment-scoped HostId to each Endpoint Domain;
- enumerate gateway Link Interfaces;
- inspect Link capabilities;
- assign WireNumbers and define each Wire's member Links;
- define Link Bindings, including constrained-Link elision and projection;
- define overlapping Wires where their distinct propagation scopes are useful;
- install gateway forwarding tables;
- install Wire splice mappings;
- inspect active configuration;
- export discovered Wiring as a candidate static configuration.

The Organizer runs the graph/configuration algorithm. Embedded Hosts execute the resulting bounded local state.

Official project tools should initially be simple and conservative: explicit validation, bounded topology assumptions, obvious errors over clever inference, ephemeral development configuration, and export to static configuration. They should not attempt to become a general distributed routing protocol.

All of the operations above are privileged (`CORE §22`).

## 1.2 Pre-addressing bootstrap

Shared media such as CAN cannot enumerate unconfigured Hosts by normal application traffic. Commissioning uses four phases (`REG §2.1`):

```text
Unconfigured   Link commissioning control only; no ordinary HostId source
Selected       exactly one physical instance addressable for commissioning
Staged         configuration prepared, not yet active
Committed      ordinary Endpoint and Wire semantics enabled
```

Collision-avoidance encoding is Link-profile-specific and not frozen. The anonymous identical-response/prefix-search proposal is a candidate; its historical identifier layout and 64-bit DeviceId choice are not adopted. Native alias/VCN control space and Guest control behavior must be specified independently of ordinary custom maps. Three rules are not negotiable:

- **Below Wire semantics.** Commissioning uses reserved Link-control space (`LINK §2.15`), not application traffic. `Unconfigured` Hosts emit **no ordinary Service traffic** (`CFG-11`).
- **`Selected` before `Staged`.** Selection resolves arbitration (one addressable instance); staging validates the full set before activation.
- **Transmit ownership in every phase.** Ordinary identifiers have one physical transmitter (`LINK-10`). Any future shared anonymous response must be an explicitly defined identical-bitstream exception, including frame type, identifier, DLC/data, timing, and controller behavior. Same identifier alone is insufficient; differing responses can corrupt traffic. The exception remains unvalidated and cannot be assumed by a production profile.

## 1.3 Recursive discovery through gateways

A host may discover a gateway on an upstream Wire, ask that gateway to enumerate/manage a downstream Link, then repeat. This enables centralized configuration of a physical hierarchy without requiring every Host to run distributed routing algorithms.

What each gateway must report for this to work is §1.10.

## 1.4 Dynamic construction of static forwarding

Auto-Wiring should produce ordinary static/read-mostly runtime forwarding tables:

```text
discover topology
    -> assign HostIds and name Wires
    -> define Wire member Links and Link-local representations
    -> validate capabilities/loops/scopes/splices
    -> generate local propagation tables
    -> install tables
    -> normal fixed data plane
```

This is **dynamic configuration of a static data plane**, not continuously converging routing.

The distinction is load-bearing. A gateway never infers a route from observed traffic, and never installs one to repair a gap it noticed; it reports local facts and the Organizer decides (§1.10).

Each configured Wire is a loop-free Logical Bus. Generated propagation state follows the Wire's member-Link topology; canonical destination controls Host acceptance rather than ordinary next-hop selection. Several Wires may overlap on the same Links, while remaining distinct propagation scopes with independent masks or ingress/egress matrices.

## 1.5 Static configuration is optional

A small/simple system may redo discovery/configuration every boot indefinitely. Exporting to a static Manifest is useful for repeatability and serious deployment, but is not a prerequisite to using WireSpaces successfully.

## 1.6 HostId allocation

Every independently routed/dispatchable Endpoint Domain has exactly one HostId within a deployment identity universe. Distinct Endpoint Domains have distinct Host IDs, and one Endpoint Domain uses the same HostId on every Wire it joins.

HostId is a deployment role identity, not a physical-device identity. Replacement hardware may therefore take over the same assigned role, while a device containing several Endpoint Domains receives one HostId for each domain. The current preferred provisional 8-bit allocation, pending the representative topology/headroom corpus, assigns `0x00..0xFE` to ordinary Hosts and `0xFF` to the canonical broadcast destination; `0xFF` remains invalid as a source.

Wiring and generated bindings express canonical `SrcHostId` and `DestHostId`; they do not assign permanent controller/leaf routing roles. A constrained profile's Direction field is Link-local reconstruction state only.

## 1.7 Stable device identity

A 128-bit UUID is the current tooling identity direction. The provisioning proposal instead uses a permanent 64-bit DeviceId; that conflict remains open (`REG §6.10`) and neither value is a HostId. Do not silently narrow a UUID or claim the provisioning choice is settled. A stable device identity provides continuity for tooling/history. It is separate from:

```text
WireNumber
HostId
Endpoint
current topology
```

Tooling can therefore remember physical continuity across Host-role replacement or reassignment. Wire UUIDs and names exist for the same reason (`CORE §4.1`).

## 1.8 Identity universes and splices

Plain forwarding never merges independently assigned HostId universes. A Wire spanning several Links assumes one coordinated canonical identity universe; interconnecting separately engineered systems requires coordinated HostId assignment, explicit identity translation, or a composition/application gateway boundary.

A splice is an explicit configured Wire-scope projection. It preserves canonical source, destination, Endpoint, applicable control metadata/extensions, and payload while deliberately changing Wire scope. It does not silently resolve HostId collisions between identity universes.

## 1.9 Ephemeral configuration must announce itself

Auto-generated lab Wiring should normally be treated as **ephemeral**, and the fact that it is ephemeral must be visible rather than inferred.

Recommended behavior:

- clearly report that automatic/ephemeral Wiring is active;
- keep discovered and installed state inspectable from host tooling;
- never silently treat an ephemeral lab map as an authoritative production configuration;
- allow the host tool to save/export the discovered topology;
- permit review and editing before generating a static deployment.

The intended workflow:

```text
plug devices in
    -> discover
    -> auto-wire
    -> experiment
    -> save/export discovered Wiring
    -> review/edit
    -> generate static deployment
```

This makes strong static configuration a natural maturation path — Level 1 to Level 2 in `INTRO §6` — rather than a prerequisite for first use.

## 1.10 Gateway discovery reporting

A gateway is a Host with two or more WS-capable Link Interfaces and the ability to propagate Wire traffic between them. Its discovery response should carry enough for host tooling to continue mapping outward without guessing.

At minimum:

```text
stable device identity
number of WS-capable Link Interfaces
per-interface current Wire membership / configuration
```

Usefully also:

```text
per-interface Link/profile type and Link state
per-interface Link capabilities (CORE §17)
existing forwarding and splice configuration
observed Hosts on each interface
```

Reporting **current Wire membership per interface** is what makes the loop-free Logical-Bus rule enforceable: the host can see that an interface already carries Wire 42 and therefore refuse a configuration that propagates Wire 42 back into itself. Without it, a host performing recursive discovery (§1.3) cannot distinguish an unconfigured branch from one it has already wired.

Gateways still do not run a distributed routing algorithm. They report local facts; the Organizer decides.

---

# 2. Wiring and Manifests

A future Wiring/Manifest format can capture deploy-time facts such as:

- Physical Links and Link Interfaces;
- Link profiles and capabilities;
- Endpoint Domains and their deployment-scoped HostIds;
- WireNumbers and Wire member Links;
- loop-free propagation topology, including overlapping Wires;
- canonical source/destination Endpoint bindings;
- Link-local elision or host-projection maps;
- Wire splices;
- gateway Wire-to-Link masks or ingress/egress matrices;
- Service placement and TX bindings;
- traffic bounds.

The Manifest is **not required to make the networking foundation work**. For serious deployments, generated/static Wiring is nevertheless the preferred model because it keeps runtime behavior boring and inspectable.

This is a deliberate divergence from an earlier generation of this architecture, which held that reachability and authority exist *only* where a Manifest configures them. That absolutism is incompatible with canonical local-only `kLocalBus` (`CORE §5.3`) and with Level 0 use (`INTRO §6`), both of which exist so that one device and one cable work with no Manifest at all. The trade is accepted knowingly: structural validity is still always enforced (`CORE §19.1`), so an unconfigured system is limited rather than unchecked.

The exact schema is unresolved, and the analysis models that would consume it are `FUTURE §4` and `FUTURE §5`.

## 2.1 What the Manifest is for

Three uses, in order of near-term value:

1. **Repeatability.** The same topology comes up the same way every boot, without rediscovery.
2. **Validation before installation.** Loops, identity collisions, inconsistent scopes, capability/profile mismatches, and unrepresentable placements are caught by tooling rather than at runtime.
3. **Capacity checking.** See §2.2.

## 2.2 Static capacity checking

Static prevention is the preferred mechanism for serious deployments, rather than relying on runtime backpressure as a steady-state scheduler (`CORE §15.2`).

The intended shape is that Services declare conservative traffic claims (`CORE §21.2`), tooling projects them across configured Wires and Links, and obvious saturation is caught before installation. Polling cadence on master-initiated Links must be included, because it bounds a Host's effective TX rate (`CORE §1.7`).

The analysis method itself is undesigned; early value comes from catching impossible deployments, not from proving timing theorems. See `FUTURE §4`.

## 2.3 What tooling must reject

Tooling extends `CORE §19.1`: it sees the whole deployment and should reject configurations that fail at runtime as silence or intermittent misbehavior. Checklist (not a formal schema):

**Wires and Endpoints**

```text
a Wire with no member Link or Host
an Endpoint referenced for a Host that does not host it
two producer implementations claiming one (HostId, Endpoint)
incompatible Endpoint types, schemas, operations, or concurrency models
                                             across the ends of one Wire
a Service granted a transmit binding it has no authority to use
```

**Identity and scope**

```text
a duplicate HostId anywhere in one deployment identity universe
one Endpoint Domain assigned different HostIds on different Wires
a transparently forwarded Wire joining inconsistent HostId universes
a device-private WireNumber configured to leave its device without a splice
a splice with a kLocalBus end
a splice treated as resolving an identity-universe collision
more than one local Link Binding using kLocalBus in one Router/Endpoint Domain
```

**Routing and Links**

```text
a forwarding loop in the configured topology
Wire member Links or ingress/egress propagation state inconsistent
    with the declared Wire scope
ambiguous resolution: one ingress plus one canonical Wire
                      resolving to two different propagation actions
a Link representation that cannot encode the configured Wire,
    source, destination, Endpoint, QoS, or required Link-local Direction
a Service placement exceeding the Link's MTU or PDUA depth
a QoS class the Link profile does not implement, with no stated fallback
a cross-domain path modeled as local, or a local path modeled as remote
```

**Resources**

```text
queue, reassembly-context, retry, or buffer bounds absent or
    insufficient for the declared traffic
aggregate declared load exceeding a Link's usable capacity
a polled Link whose cadence cannot satisfy a declared freshness bound
```

**Observation and composition**

```text
an observation or capture path configured to receive traffic
    not covered by its configuration
an observation path counted as delivery coverage or as a redundancy member
redundant member Wires exposing different producer Endpoint identities
    without an explicit replicated-source component
a composition that has not flattened to Endpoints, Wires, and bindings
```

**Bindings and combinations**

```text
an Endpoint registration declaring more than one binding mode,
    or none                                       (CORE 10.5)
a learned-from-ingress binding on an unauthenticated multi-access Link
a learned binding without finite capacity, lifetime, eviction,
    and defined restart behavior
a tuple rejected by coupled constraints even though every field is
    individually admitted                         (CORE 19.1)
an extension policy that a Link's size budget cannot accommodate
```

The coupled-constraint check is the one a generator is most likely to omit — per-field validation passes on almost everything and only fails on bad combinations.

Tooling must **not** reject Level 0 systems with no Manifest (`INTRO §6`). `kLocalBus` is nevertheless a canonical reserved WireNumber: it is locally dispatchable, bound to at most one Link Interface in a Router/Endpoint Domain, and is neither transparently forwarded nor spliced as itself.

## 2.4 CAN11 VCN configuration

CAN11 uses the unified VCN model in `LINK §2`. One authoritative deployment source defines Hosts, Wires, Guest relationships, Native alias bindings, and TX selection. Device tables are generated slices, never independently authored map namespaces.

**Guest** configuration contains one canonical Wire, one bus-owner-reserved aligned CAN-ID block, fixed QoS, an exact profile version, and the applicable slice of the deployment-wide Guest VCN definition. Guest-4 is the initial 16-ID form; Guest-5/6 are growth directions. A Guest VCN has the same Host relationship across the deployment even where Link Bindings supply different Wires. Widening exposes additional default entries without reinterpreting existing ones; actual block changes still require coordinated cutover.

**Native** configuration contains up to eight alias bindings:

```text
WireAlias -> {WireNumber, DefaultMap or ExplicitMap, profile parameters}
```

Each alias has one Wire and one immutable map. Several aliases may represent separate Wires or the same Wire during migration. Default maps bind MainA, MainB, and NodeN positions to ordinary HostIds; these are profile positions without authority. Unbound positions remain inactive. Explicit maps name arbitrary modest Host pairs, subject to profile control reservations and uniqueness rules.

A separate deterministic TX selection chooses exactly one active alias for each admitted canonical tuple on the interface. The Router does not select aliases. Two RX aliases representing the same tuple are permitted; two competing TX selections are not. No first-match selection, traffic learning, automatic fallback, or double transmission under old and new aliases is allowed.

Tooling rejects, in addition to §2.3:

```text
missing, inconsistent, or runtime-inferred profile/version
ambiguous ingress classification or overlapping Guest allocations
unaligned, out-of-range, noncontiguous, or unreserved Guest block
Guest TX QoS differing from the binding's fixed QoS
conflicting deployment-wide Guest VCN meanings
an alias selecting more than one Wire or VCN map
more than eight active Native aliases or out-of-range VCNs
self-pairs, duplicate unordered pairs, or duplicate broadcast-source
    entries within one map
invalid/unbound default positions or two positions assigned one Host
ordinary use of reserved control or broadcast-source encodings
unrepresentable Host/Endpoint/Transport/extension combinations
missing or ambiguous TX selection for an admitted canonical tuple
in-place mutation of an active alias binding
live migration without spare-alias capacity or receiver readiness
retirement/reuse without a proven stale-frame exclusion boundary
```

Within a QoS class, alias and VCN values influence CAN arbitration. Tooling displays that ordering and any control-priority limitations. It also reports alias capacity used by migration, not just by steady-state Wires.

### Native alias migration

Prepare a new binding under a spare alias; install and validate it at all affected receivers; then explicitly move sender TX selections. Keep the old binding available until accepted TX and RX/reassembly work is completed or cancelled. A PDU already accepted for transmission retains its selected representation. Retire and reuse an alias only under `LINK §2.14`'s stale-frame exclusion rules.

With no spare alias, live migration is unavailable. Keep the old configuration or use coordinated offline reconfiguration with traffic disabled and queues/reassembly safely drained or discarded. A local reset alone does not establish peer readiness or clear remote traffic.

Alias 0 may initially select `kLocalBus` and the default map, but it is not permanently reserved. Changing that binding follows the same rules; it never transparently promotes or splices LocalBus traffic. Guest has no alias migration mechanism and needs a separately defined coordinated cutover.

This establishes the update boundary. Fingerprint encoding, readiness/commit messages, exact drain/flush timing, reuse limits, and partial-migration recovery remain open.

## 2.5 Configuration compatibility checking

Generated configuration is distributed across devices, and the pieces have to agree. A device flashed from another generation may otherwise reconstruct a valid-looking Wire, host pair, profile, or projection and confidently apply the wrong semantics. Every field can be individually valid while the combination is incompatible.

**Generated artifacts must therefore make incompatible routing, Endpoint, Link-profile, and representation configuration detectable. A detected incompatibility fails closed or enters an explicitly configured degraded mode.**

The compatibility indication may be a version, digest, fingerprint, or another bounded generated check. A mismatch is reported, not repaired: the device says what it has, the host decides. A Host that adapts to a peer's configuration has started doing distributed routing (§1.4).

The exact mechanism, scope, and computation remain open. It never authorizes guessed semantics or a silent fallback.

### One source generates every projection

The same configuration is emitted in very different forms: C++ tables, RTL parameters, a host tool's view of the deployment, generated Python constants. A single deployment may contain all four (`IMPL §2`, `CORE §25`).

> **One authoritative Wiring source generates every participating projection**, and validation rejects projections that disagree about a Wire, a Host or VCN map, an Endpoint binding, a profile, a bound, or a capability.

Hand-maintaining a second copy of anything in that list is the most reliable way to produce a deployment that is internally inconsistent in a way no single device can detect. The RTL Host and the firmware Host each behave exactly as configured; they were simply configured differently. Generating both from one source makes the disagreement a build error instead.

Two artifacts generated from *different revisions* of the same source are the common case in practice, so compatibility checking must apply to generated projections rather than only device identity.

### Remaining CAN11 configuration-transition work

Native active bindings are immutable and replacements use spare aliases (§2.4). Before profile freeze, specify exact fingerprint coverage, receiver-readiness proof, TX-selector publication, retirement/reuse and stale-frame exclusion, and recovery after interrupted migration. Guest map/block cutover remains separate because Guest has no alias selector.

Compatibility mismatch is reported locally and fails closed. A future map-independent Link-control protocol may expose it remotely; ordinary malformed traffic still causes no automatic infrastructure-error reply.

---

# 3. Host Tooling and the Maintenance Port

One host tool should understand dev boards over USB/UART, multicore MCUs, CAN buses, gateways, FPGA RTL Endpoints, and remote links — same Service model, different front-door Link.

```text
Engineer PC
    |
USB / UART / Ethernet / FTDI FIFO   (maintenance Link)
    |
Main SoC / FPGA gateway
    +-- CAN_A, CAN_B, RS-485 -> Hosts
    +-- internal FPGA/CPU domains
    +-- RTL Endpoints
```

FTDI FIFO is an attractive FPGA path: fast PC pipe without a CPU or network stack in the device (`LINK §5`, `CORE §24`).

Illustrative CLI shape:

```text
ws devices | ws info | ws logs | ws events | ws monitor
ws update | ws links | ws services | ws capture
```

Exact command names are illustrative. Other useful functions: Service discovery, Wire topology, packet capture, propagation/Link-Binding inspection, configuration generation.

## 3.1 Promiscuous and bring-up operation

Auto-Wiring (§1) deliberately *creates* configuration. A separate and complementary capability is deliberately *ignoring* it.

In promiscuous/bring-up mode, an implementation may:

- observe PDUs regardless of whether local configuration recognizes them;
- log unknown WireNumbers, HostIds, Endpoints, and Services;
- interrogate observed Hosts;
- perform explicitly privileged exploratory transmissions.

Two boundaries are firm:

> **Promiscuous observation never creates persistent Wiring.** It only reports. Auto-Wiring is the separate mechanism that deliberately installs temporary configuration.

> **Promiscuous mode is a host-tooling and gateway capability.** Ordinary embedded Hosts remain strictly configured: the Dispatcher still rejects and counts unknown Endpoints (`CORE §9.1`), and a Host does not consume directed traffic unless its canonical HostId is the destination.

The capability must be absent from normal builds — compiled out for constrained targets and production — rather than merely disabled, per `CORE §22.1`.

This pairs naturally with the counters and drop journals in `CORE §18`: a gateway that can already count what it rejects is most of the way to reporting what it observed. An observer also provides no redundancy coverage (`CORE §23.11`).

**Open:** how far promiscuous capability may extend on a gateway that is simultaneously carrying production traffic, and the exact build-time removal rules.

## 3.2 Internal Debug Wire and default bindings

Before static system allocation, multiple devices cannot safely assume that the same *external* WireNumber refers to their own private debug traffic. Every device may instead use the same **device-private debug Wire identity** without collision, because that identity never leaves the device unspliced (`CORE §7`). A single eligible native Link may separately use canonical local-only `kLocalBus`; `kLocalBus` is not the splice endpoint for this debug pattern.

```text
Device

 Service A ----\
 Service B -----+---- InternalDebugWire ---- splice ---- host-facing Wire
 Service C ----/                               |
                                               v
                                               PC
```

Once commissioned, a typical host path is:

```text
InternalDebugWire (device-private allocation TBD)
    <splice>
Wire 101
    |
USB / Ethernet / CAN
    |
PC
```

A reusable Service can have a default injected `debug_tx` binding (`CORE §10`) without knowing which physical Link reaches the developer, which host transport is in use, which external WireNumber was assigned, or whether the device is still using local-only `kLocalBus` bring-up. Selected Services can later be moved to different application or diagnostic Wires.

This gives WS a useful zero-configuration bring-up path while avoiding accidental external WireNumber collision.

## 3.3 Link telemetry and observability Services

A proposed default-enabled **Domain Local Link Telemetry Service** collects periodic snapshots from all Link Interfaces in one Endpoint Domain.

```text
Endpoint Domain
    +-- CAN_A
    +-- CAN_B
    +-- shared-memory Link
    +-- Ethernet
    |
    `-- Link Telemetry Service
            |
            | ~1 Hz snapshots
            v
       InternalDebugWire
            |
            | splice (§3.2)
            v
       host-facing Wire -> PC
```

The telemetry Service can remain small on embedded targets while host/Linux implementations expose much richer detail. The exact schema is open (`REG §6.7`).

Larger gateways may additionally report Wires using the most bandwidth, Wires producing the most congestion drops, and per-QoS pressure. This is useful for human diagnosis even if no automatic congestion manager is ever implemented.

A future optional Link Manager Service could consume this telemetry and change admission/shedding policy, but that is **not base protocol behavior** (`FUTURE §15`).

### One Service per Endpoint Domain, two faces, one model

Three tempting structures are worth ruling out explicitly, because each one splits the truth.

> **Each Endpoint Domain exposes exactly one telemetry Service, covering every Link Interface in that domain.** Not one Service per Link, not a separate local-introspection Service alongside a network-reporting one, and not an aggregate spanning domains.

A device hosting several Endpoint Domains therefore exposes one such Service per domain, and none of them reports another domain's Links — which follows from the domain being the dispatch and concurrency scope in the first place (`CORE §1.5`).

The Service has two access faces over **one** semantic model:

```text
local face      a coherent snapshot readable by authorized code in the domain
network face    an optional projection carried on configured telemetry Wires
```

The network face is an encoding or a bounded subset of the local snapshot. It may carry less precision or omit fields, but it must not redefine what a field means or maintain a second, disagreeing account of the same thing. The practical payoff is that local telemetry stays useful when no telemetry Wire is configured or reachable, which is the situation during bring-up and after a bad configuration push.

### Compact, Standard, and Extended

The plausible schema split is by target scale rather than by feature:

| Class | Target | Character |
|---|---|---|
| Compact | Classical CAN, constrained Hosts | masks, coarse states, saturating counts, aggressive quantization; a single CAN frame is a design target, not a contract |
| Standard | typical MCU with UART/HDLC, CAN FD, shared memory | per-Link and per-QoS detail at ordinary operational precision |
| Extended | gateways, hosts, multi-Link devices | richer bounded fault and recovery history, timing detail, larger counters |

Standard and Extended are semantic expansions of the same model. They need not be byte-prefix extensions of Compact, and trying to make them so tends to distort the compact encoding for no benefit.

Two rules keep the classes honest. A host may normalize all three into one internal model, but **a field absent from a smaller schema is unavailable, not zero, false, healthy, or unchanged** (`CORE §18.5`). And telemetry must remain a fixed schema per version rather than becoming a runtime tag/type/value language, because a self-describing telemetry format is precisely the kind of unbounded parser a constrained Host cannot afford and an attacker enjoys.

Field widths, quantization, endpoint allocation, and pagination are all open (`REG §6.7`).

### Publishing over a healthy Link

A faulted Link cannot report its own fault, so telemetry publication is configured to not depend on the path being diagnosed (`CORE §23.10`). Four constraints follow, and the last two exist because diagnostics are the classic source of self-sustaining traffic:

- publication is driven by a configured cadence or an explicit request, and by nothing else;
- **each destination has at most one pending summary.** A newer sample updates the pending one rather than queueing behind it — which is exactly a Snapshot transmit Endpoint (`CORE §10.4`), and a good argument that the primitive is the right shape;
- a transmission outcome updates local counters and never triggers another publication;
- a malformed request, a publish failure, or a diagnostic fault is recorded locally and produces no reply (`ERR-3`).

Local counters remain the authoritative record; whatever leaves on a Wire is a bounded projection of them.

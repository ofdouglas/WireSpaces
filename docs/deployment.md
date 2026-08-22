# WireSpaces — Deployment, Discovery, and Tooling

**Status:** Private first draft; the Organizer and Manifest are directions, not designs  
**Scope:** How a WireSpaces system is discovered, commissioned, configured, and observed from a host  
**Relationship to `CORE`:** Everything here configures the runtime described in `CORE`. None of it is required for basic communication

Host tooling is a first-class part of WireSpaces, not demo scaffolding. But the ordering matters: a device with one Link works with no tooling and no configuration at all (`CORE §5.3`), and everything in this document is a capability rather than an admission requirement (`INTRO §6`).

---

# 1. Discovery, Commissioning, and the Organizer

WireSpaces should support both explicitly configured deployment and very low-configuration lab use.

## 1.1 Organizer

A host-side **Organizer** is a temporary/explicit configuration authority. It is **not the same thing as a Wire Origin**.

An Organizer may:

- discover devices;
- read stable identity;
- assign NodeIds;
- enumerate gateway Link Interfaces;
- inspect Link capabilities;
- name Anonymous Wires by assigning WireNumbers;
- allocate WireAliases;
- install gateway forwarding tables;
- install Wire splice mappings;
- inspect active configuration;
- export discovered Wiring as a candidate static configuration.

The Organizer runs the graph/configuration algorithm. Embedded nodes execute the resulting bounded local state.

Official project tools should initially be simple and conservative: explicit validation, bounded topology assumptions, obvious errors over clever inference, ephemeral development configuration, and export to static configuration. They should not attempt to become a general distributed routing protocol.

All of the operations above are privileged (`CORE §22`).

## 1.2 Pre-addressing bootstrap

Shared media such as CAN cannot enumerate unconfigured nodes by normal application traffic. Commissioning uses four phases (`REG §2.1`):

```text
Unconfigured   Link commissioning control only; no NodeId, no Endpoint authority
Selected       exactly one physical instance addressable for commissioning
Staged         configuration prepared, not yet active
Committed        ordinary Endpoint and Wire semantics enabled
```

Collision-avoidance encoding is Link-profile-specific and not frozen. Three rules are not negotiable:

- **Below Wire semantics.** Commissioning is Link control, not application traffic. `Unconfigured` nodes emit **no ordinary Service traffic** (`LINK §2.13`).
- **`Selected` before `Staged`.** Selection resolves arbitration (one addressable instance); staging validates the full set before activation.
- **Transmit ownership in every phase.** One owner per identifier/time slot always (`LINK §2.8`, `LINK-10`). On CAN, two simultaneous unconfigured replies can corrupt unrelated traffic.

## 1.3 Recursive discovery through gateways

A host may discover a gateway on an upstream Wire, ask that gateway to enumerate/manage a downstream Link, then repeat. This enables centralized configuration of a physical hierarchy without requiring every node to run distributed routing algorithms.

What each gateway must report for this to work is §1.10.

## 1.4 Dynamic construction of static forwarding

Auto-Wiring should produce ordinary static/read-mostly runtime forwarding tables:

```text
discover topology
    -> assign/name Wires
    -> allocate Link aliases
    -> validate capabilities/loops/splices
    -> generate local gateway tables
    -> install tables
    -> normal fixed data plane
```

This is **dynamic configuration of a static data plane**, not continuously converging routing.

The distinction is load-bearing. A gateway never infers a route from observed traffic, and never installs one to repair a gap it noticed; it reports local facts and the Organizer decides (§1.10).

## 1.5 Static configuration is optional

A small/simple system may redo discovery/configuration every boot indefinitely. Exporting to a static Manifest is useful for repeatability and serious deployment, but is not a prerequisite to using WireSpaces successfully.

## 1.6 Preferred conventional NodeIds

Two high NodeIds are useful as conventions:

```text
30  DiagnosticAndTestEquipment (preferred)
31  DevelopmentEquipment       (preferred)
```

They are preferences, not immutable identities. Commissioning may assign another free NodeId if the preferred value is occupied.

## 1.7 Stable device identity

A 128-bit UUID is a useful device identity for tooling/history. It is separate from:

```text
WireNumber
NodeId
EndpointId
current topology
```

Tooling can therefore remember that `Wire 107 / Node 2` and later `Wire 18 / Node 9` were the same physical device after a topology change. Wire UUIDs and names exist for the same reason (`CORE §4.1`).

## 1.8 Globally stable NodeIds as a convenience

NodeIds are only required to be unique **within** a Wire. Small systems may nevertheless choose to keep them stable **across** Wires, so that:

```text
Wire X / Node 7
Wire Y / Node 7
```

refer to the same physical device. This is purely an allocation convenience. It costs nothing, makes logs and captures much easier for humans to read, and is not a requirement of the model.

It has one concrete architectural benefit. `CORE §6.6` notes that splicing several device-private segments onto one shared external Wire requires coordinating NodeIds across the participating devices. A deployment that already keeps NodeIds globally stable satisfies that constraint by construction rather than by validation.

Static NodeId configuration remains available and is preferable wherever stable assignments matter.

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

A gateway is a node with two or more WS-capable Link Interfaces and the ability to forward Wire traffic between them. Its discovery response should carry enough for host tooling to continue mapping outward without guessing.

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
observed participants on each interface
```

Reporting **current Wire membership per interface** is what makes the loop rule in `CORE §12.4` enforceable: the host can see that an interface already carries Wire 42 and therefore refuse to bridge or splice Wire 42 back into itself. Without it, a host performing recursive discovery (§1.3) cannot distinguish an unconfigured branch from one it has already wired.

Gateways still do not run a distributed routing algorithm. They report local facts; the Organizer decides.

---

# 2. Wiring and Manifests

A future Wiring/Manifest format can capture deploy-time facts such as:

- Physical Links and Link Interfaces;
- Link profiles and capabilities;
- Wire membership;
- Origins and Nodes;
- NodeIds;
- WireNumbers and aliases;
- Wire splices;
- gateway forwarding tables;
- Service placement and TX bindings;
- traffic bounds.

The Manifest is **not required to make the networking foundation work**. For serious deployments, generated/static Wiring is nevertheless the preferred model because it keeps runtime behavior boring and inspectable.

This is a deliberate divergence from an earlier generation of this architecture, which held that reachability and authority exist *only* where a Manifest configures them. That absolutism is incompatible with anonymous `kLocalBus` (`CORE §5.3`) and with Level 0 use (`INTRO §6`), both of which exist so that one device and one cable work with no configuration artifact at all. The trade is accepted knowingly: structural validity is still always enforced (`CORE §19.1`), so an unconfigured system is limited rather than unchecked.

The exact schema is unresolved, and the analysis models that would consume it are `FUTURE §4` and `FUTURE §5`.

## 2.1 What the Manifest is for

Three uses, in order of near-term value:

1. **Repeatability.** The same topology comes up the same way every boot, without rediscovery.
2. **Validation before installation.** Loops, capability mismatches, NodeId collisions, and unrepresentable placements are caught by tooling rather than at runtime (`CORE §6.6`).
3. **Capacity checking.** See §2.2.

## 2.2 Static capacity checking

Static prevention is the preferred mechanism for serious deployments, rather than relying on runtime backpressure as a steady-state scheduler (`CORE §15.2`).

The intended shape is that Services declare conservative traffic claims (`CORE §21.2`), tooling projects them across configured Wires and Links, and obvious saturation is caught before installation. Polling cadence on master-initiated Links must be included, because it bounds a Node's effective TX rate (`CORE §1.7`).

The analysis method itself is undesigned; early value comes from catching impossible deployments, not from proving timing theorems. See `FUTURE §4`.

## 2.3 What tooling must reject

Tooling extends `CORE §19.1`: it sees the whole deployment and should reject configurations that fail at runtime as silence or intermittent misbehavior. Checklist (not a formal schema):

**Wires and Endpoints**

```text
a Wire with no Origin, or with more than one
a Wire with no participants other than its Origin
an Endpoint referenced in a Domain that does not host it
two producer implementations claiming one (Domain, Namespace, EndpointId)
incompatible Endpoint types, schemas, operations, or concurrency models
                                             across the ends of one Wire
a Service granted a transmit binding it has no authority to use
```

**Identity and scope**

```text
NodeId collision on a Wire
a device-private WireNumber configured to leave its device without a splice
a splice with an anonymous or unconfigured kLocalBus end
WireAlias collision within the scope where the alias must resolve uniquely
an alias mapping that disagrees between the two ends of a Link
```

**Routing and Links**

```text
a forwarding loop in the configured topology
ambiguous resolution: one ingress plus one canonical identity
                      resolving to two different actions
a Link representation that cannot encode the configured Wire,
    Endpoint, QoS, or Direction
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

Tooling must **not** require deployment-wide unique path identifiers (WireNumber is what matters) or reject Level 0 systems with no Manifest (`INTRO §6`).

## 2.4 Configuration compatibility checking

Generated configuration is distributed across devices, and the pieces have to agree. Aliases are the sharpest case: `WireAlias 3` means whatever the two ends of a Link were told it means, so a device flashed from an older generation will forward traffic confidently onto the wrong Wire. Nothing in the PDU detects this, because every field is individually valid.

**Generated artifacts should therefore carry a compatibility check — a version, digest, or fingerprint — sufficient to detect mismatched routing, Endpoint, and alias configuration between participants.**

The properties that matter are modest. It must cover the parts that must agree, so that a change to an alias mapping or an Endpoint allocation changes the value while an unrelated change does not. It must be cheap enough for a small node to hold and report, which means a fingerprint rather than the configuration itself. And a mismatch must be **reported, not repaired**: the device says what it has, the host decides. A node that adapts to a peer's configuration has started doing distributed routing (§1.4).

The exact mechanism — what is covered, how it is computed, whether it is per-Link or per-deployment, and whether a mismatch blocks traffic or only raises a diagnostic — is open. What is not open is the failure behavior: a mismatch **fails closed or enters an explicitly configured degraded mode.** It never authorizes guessed semantics or a silent fallback.

### One source generates every projection

The same configuration is emitted in very different forms: C++ tables, RTL parameters, a host tool's view of the deployment, generated Python constants. A single deployment may contain all four (`IMPL §2`, `CORE §25`).

> **One authoritative Wiring source generates every participating projection**, and validation rejects projections that disagree about a Wire, an alias, an Endpoint binding, a profile, a bound, or a capability.

Hand-maintaining a second copy of anything in that list is the most reliable way to produce a deployment that is internally inconsistent in a way no single device can detect. The RTL node and the firmware node each behave exactly as configured; they were simply configured differently. Generating both from one source makes the disagreement a build error instead.

This is also why the fingerprint should cover projections rather than only devices: two artifacts generated from *different revisions* of the same source are the common case in practice, and the fingerprint is what catches it.

---

# 3. Host Tooling and the Maintenance Port

One host tool should understand dev boards over USB/UART, multicore MCUs, CAN buses, gateways, FPGA RTL Endpoints, and remote links — same Service model, different front-door Link.

```text
Engineer PC
    |
USB / UART / Ethernet / FTDI FIFO   (maintenance Link)
    |
Main SoC / FPGA gateway
    +-- CAN_A, CAN_B, RS-485 -> Nodes
    +-- internal FPGA/CPU domains
    +-- RTL Endpoints
```

FTDI FIFO is an attractive FPGA path: fast PC pipe without a CPU or network stack in the device (`LINK §5`, `CORE §24`).

Illustrative CLI shape:

```text
ws devices | ws info | ws logs | ws events | ws monitor
ws update | ws links | ws services | ws capture
```

Exact command names are illustrative. Other useful functions: Service discovery, Wire topology, packet capture, routing/alias inspection, configuration generation.

## 3.1 Promiscuous and bring-up operation

Auto-Wiring (§1) deliberately *creates* configuration. A separate and complementary capability is deliberately *ignoring* it.

In promiscuous/bring-up mode, an implementation may:

- observe PDUs regardless of whether local configuration recognizes them;
- log unknown WireNumbers, WireAliases, NodeIds, Endpoints, and Services;
- interrogate observed nodes;
- perform explicitly privileged exploratory transmissions.

Two boundaries are firm:

> **Promiscuous observation never creates persistent Wiring.** It only reports. Auto-Wiring is the separate mechanism that deliberately installs temporary configuration.

> **Promiscuous mode is a host-tooling and gateway capability.** Ordinary embedded Nodes remain strictly configured: the Dispatcher still rejects and counts unknown EIDs (`CORE §9.1`), and a Node does not consume traffic it was not wired to receive (`CORE §12.6`).

The capability must be absent from normal builds — compiled out for constrained targets and production — rather than merely disabled, per `CORE §22.1`.

This pairs naturally with the counters and drop journals in `CORE §18`: a gateway that can already count what it rejects is most of the way to reporting what it observed. An observer also provides no redundancy coverage (`CORE §23.11`).

**Open:** how far promiscuous capability may extend on a gateway that is simultaneously carrying production traffic, and the exact build-time removal rules.

## 3.2 Internal Debug Wire and default bindings

Before static system allocation, multiple devices cannot safely assume that the same *external* WireNumber refers to their own private debug traffic. Every device may instead use the same **device-private debug Wire identity** without collision, because that identity never leaves the device unspliced (`CORE §7`).

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
InternalDebugWire (e.g. 1020)
    <splice>
Wire 101
    |
USB / Ethernet / CAN
    |
PC
```

A reusable Service can have a default injected `debug_tx` binding (`CORE §10`) without knowing which physical Link reaches the developer, which host transport is in use, which external WireNumber was assigned, or whether the device is still in anonymous bring-up mode. Selected Services can later be moved to different application or diagnostic Wires.

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
| Compact | Classical CAN, constrained nodes | masks, coarse states, saturating counts, aggressive quantization; a single CAN frame is a design target, not a contract |
| Standard | typical MCU with UART/HDLC, CAN FD, shared memory | per-Link and per-QoS detail at ordinary operational precision |
| Extended | gateways, hosts, multi-Link devices | richer bounded fault and recovery history, timing detail, larger counters |

Standard and Extended are semantic expansions of the same model. They need not be byte-prefix extensions of Compact, and trying to make them so tends to distort the compact encoding for no benefit.

Two rules keep the classes honest. A host may normalize all three into one internal model, but **a field absent from a smaller schema is unavailable, not zero, false, healthy, or unchanged** (`CORE §18.5`). And telemetry must remain a fixed schema per version rather than becoming a runtime tag/type/value language, because a self-describing telemetry format is precisely the kind of unbounded parser a constrained node cannot afford and an attacker enjoys.

Field widths, quantization, endpoint allocation, and pagination are all open (`REG §6.7`).

### Publishing over a healthy Link

A faulted Link cannot report its own fault, so telemetry publication is configured to not depend on the path being diagnosed (`CORE §23.10`). Four constraints follow, and the last two exist because diagnostics are the classic source of self-sustaining traffic:

- publication is driven by a configured cadence or an explicit request, and by nothing else;
- **each destination has at most one pending summary.** A newer sample updates the pending one rather than queueing behind it — which is exactly a Snapshot transmit Endpoint (`CORE §10.4`), and a good argument that the primitive is the right shape;
- a transmission outcome updates local counters and never triggers another publication;
- a malformed request, a publish failure, or a diagnostic fault is recorded locally and produces no reply (`ERR-3`).

Local counters remain the authoritative record; whatever leaves on a Wire is a bounded projection of them.

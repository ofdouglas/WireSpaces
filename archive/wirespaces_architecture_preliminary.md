# WireSpaces — Working Architecture

**Revision:** 0.4  
**Date:** 2026-08-20  
**Status:** Working architecture; intentionally provisional; not polished  
**Audience:** WireSpaces implementers, design reviewers, coding/design agents, embedded software engineers, FPGA/RTL engineers, and host-tool developers  
**Interoperability:** Not claimed; see §32.

**Supersedes:**

- `wirespaces_high_level_architecture_design_overview.md` (the long overview);
- `wirespaces_architecture_snapshot_2026-08-20.md` (revision 0.1 snapshot);
- `wirespaces_simplified_wire_and_autowiring_design_change.md` (the simplified-Wire / auto-Wiring change statement);
- `WS_old/network/architecture_overview.md` (the consolidated-draft generation).

Revision 0.2 merged the first two; where they conflicted, the snapshot won unless noted in §0.3. Revision 0.3 recovers material from the third; see §0.4. Revision 0.4 recovers material from the fourth; see §0.5.

The last of these delegates to a set of sibling documents under `WS_old/network/` (endpoint domains/Wires/Routes/access, core protocol and routing, application protocols and services, logical links and transports, Link Engine runtime and status, HDLC profile, prototype and validation, rationale/use cases/risks). Those have **not** yet been mined. Their model is the superseded generation, but they may hold recoverable detail in the same way this one did.

---

## 0. How to Use This Document

This document captures the **current working architecture** of WireSpaces (WS). It is intended as input to implementation and design work while the architecture is still changing.

It is **not a normative protocol specification**. Byte-exact Link profiles, CRC parameters, API signatures, timing requirements, registry allocations, and interoperability requirements belong in narrower protocol/profile specifications. Where this document gives a concrete field width or behavior, it means "current agreed direction" unless the text says it is frozen.

The most important design rule for ongoing work is:

> **Do not add a new protocol abstraction until a concrete implementation or use case demonstrates that the current model cannot solve the problem cleanly.**

The networking foundation should remain understandable without learning future Flow, WireContract, Manifest, assurance, or redundancy-analysis machinery.

### 0.1 How this document is maintained

More material will be folded in over time. Four sections act as the index and should be updated whenever body text changes:

```text
§0.2   confidence levels
§36    core architectural invariants
§37    explicitly superseded / do-not-reintroduce
§38    open questions
```

If a change to the body does not show up in those four sections, the change is not finished.

### 0.2 Current confidence levels

**Strong / current direction**

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

**Provisional implementation direction**

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

**Deferred**

- Flow and WireContract semantics.
- Full Wiring Manifest schema.
- General security architecture.
- Automatic Origin failover/election.
- Cyclic/redundant forwarding profiles.
- General adaptive congestion-control protocol.
- Zero-copy ownership APIs.
- A reusable communication-component catalog above Endpoints/Wires.
- Domain Control: a layer applying management intent to statically defined *groups* of Services, distinct from both product policy and application data transport. Recovered as an idea only; the current architecture has per-device lifecycle Services (§26.4) and no group-management layer, and none should be added until a real deployment needs it.
- Cross-WireSpace identity translation beyond the scoping rule in §5.6.

### 0.3 Changes in revision 0.2

Merged from the long overview, having been confirmed as still current:

- **Wire Splicing** (§7), including the `spliceWire` route field, with the ordering rule that a splice applies **before egress**.
- **Default Internal Debug Wire** (§8), whose host path is the splice.
- **Service TX bindings** (§10) and **`Inline` / `Serialized` delivery policies** (§9.4).
- **Copy-based buffer ownership** and the deferred zero-copy path (§16).
- **Namespace and EndpointId allocation** (§21), including the NS0 compact CAN region and the NS3 FOSS registry.
- Why a bus rather than pairwise edges (§4.4), small-device Router notes (§11.6), Ethernet/CAN adaptation asymmetry (§12.5), implementation scaling profiles (§33), and the worked examples (§35).

Reconciled where reinstating the above collided with snapshot text:

- §5.2 and invariant #9 now carry the splice carve-out. A device-private Wire still never appears on an external Link, because the splice rewrites it to a network-visible Wire before the egress LLL sees it.
- §4.3's field-preservation invariant now names splicing explicitly, so it is not confused with an application-level transformation service.
- §11.3's route entry carries an optional `spliceWire`, alongside the egress bitmask and local-delivery indication.
- §19.1's Organizer may install splice mappings.
- Anonymous (unmapped) `kLocalBus` is explicitly **not** spliceable (§6.8).

Corrected or added:

- §22.9 gives the **corrected Classical CAN payload budget** under the CRC-8/CRC-16 policy. The overview's table (N=1→5, N=2→12, N=3→19, N=4→26) was a pre-integrity gross budget and overstated usable bytes for N >= 2.
- §21 keeps the overview's Namespace allocation as the working plan; only the NS3 registry *process* remains open, not the allocation itself.

Carried forward from the snapshot unchanged, and still superseding the overview:

- Classical CAN PDUA MaxN 8 (was 16) and the 3+3 FrameControl layout (was 1+4).
- COBS favored over HDLC-style escaping for byte streams; UART CRC-16 choice reopened.
- QoS-Minimal / QoS-Full profiles; congestion as a normal send outcome.
- All terminology supersessions (`Main`/`Peer`/`PeerId`, `PathTag`, `RoutingProfile`, `RoutingCode`, `Link Engine`, `RouterPort`).

### 0.4 Changes in revision 0.3

Recovered from `wirespaces_simplified_wire_and_autowiring_design_change.md`. That document's terminology and CAN numbering are superseded, but several of its ideas had no equivalent in revision 0.1 or 0.2:

- **Usage maturity levels 0-5** (§1.5), with the rule that advanced features must not make Levels 0-1 harder.
- **Physical and Virtual Wires** as named cases (§4.5): every physical bus has a Wire identity, and a Virtual Wire is one whose broadcast domain is not exactly one complete physical bus.
- **Wire identity continuity** (§5.1): a Wire's UUID/name keep logs and captures interpretable when its short WireNumber is reassigned, for the same reason device UUIDs do (§19.7).
- **`kLocalBus` re-derived** (§6, §6.2): alias 0 is the compression code for *this Link's own physical Wire number*, which explains why it is privileged and yields a concrete TX rule. The unmapped/anonymous case becomes the exception rather than the base case.
- **Commissionable is independent of routable** (§6.6): a LocalBusOnly or N=1-only node may still accept NodeId and Wire assignment.
- **Gateway discovery reporting** (§17.1), including per-interface current Wire membership so a host does not bridge a Wire back into itself — the missing mechanism behind §12.4 and §7.6.
- **Globally stable NodeIds as a convenience** (§19.8), which also makes multi-device splicing collision-free by construction.
- **Ephemeral configuration must announce itself** (§19.9).
- **Structural validity versus contract** (§20.4): which checks the runtime always enforces, versus optional policy.
- **Promiscuous / bring-up operation** (§27.4), scoped to host tooling and gateways.
- **A future 29-bit Classical CAN profile** (§22.11), named as the planned escape hatch with no layout specified.

Rejected from that document rather than recovered:

- Ephemeral **route repair / learned forwarding**, where a gateway infers a missing route from observed traffic and installs a temporary learned splice. Recorded in §37 so it cannot leak into the data plane.
- The 3-bit **canonical** WireNumber on CAN, already superseded by Link-scoped aliases.
- `Main` / `Peer` / `PeerId`, `Link Engine`, and WireContract as specified there.

### 0.5 Changes in revision 0.4

Recovered from `WS_old/network/architecture_overview.md`. That document describes an earlier generation whose core model — the directed one-source/one-or-more-sink Wire, `{WireBand, RoutingCode}`, `RoutingAlias`, `PathTag`, Route as an object distinct from Wire, `ParticipantId`, and Tap/Relay/Replicator as base roles — is already listed in §37. What survives is the material below.

Structural and semantic recoveries:

- **Port versus Endpoint** (§2.6): a Port is the local typed directional interface; an Endpoint is the network-visible termination. `TxBinding` (§10) is an output Port, so the receive side now gets the same treatment.
- **Transport is a Service-semantics choice, and reliability is not assumed safer than loss with freshness detection** (§25.3). This is the sharpest idea in that document and had no equivalent here. It pairs directly with the latest-value queue policy (§14.3).
- **Freshness and staleness as declared Service properties** (§26.5), with duplicate and stale data added to the error categories (§18.1).
- **Link independence is conditional** (§13.4): services are link-independent only inside declared size, timing, and transport compatibility. Revision 0.3 made the independence claim unqualified even though §17 already had the machinery to check it.
- **Master-initiated and polled Links** (§2.8, §23), including I2C and SPI as in-scope Physical Links, and the rule that an LLL that autonomously schedules transmission or initiates a transaction does not thereby become the semantic producer.
- **Higher-level composition must flatten** (§20.5): request/response, redundancy, feedback, and freshness patterns compose statically and reduce to ordinary Endpoints, Wires, and bindings, so the Router and LLL never learn them. Taken as a constraint without the component catalog, which stays deferred (§0.2).
- **What a redundancy composition owns** (§30.2): message-instance correlation, duplicate suppression, stale-copy rejection, failover policy, per-Wire health, per-sink coverage; plus the distinction between path redundancy and voting across independent producers.
- **Electrical visibility is not membership** (§12.6). Needed here precisely because §12.1 chose flood-and-filter: flooding a branch does not make its Nodes participants.
- **Proximity does not imply locality** (§5.5), the inverse of the FPGA rule in §5.4. A peripheral on the same PCB reached over SPI is a separate Endpoint Domain using ordinary routing.
- **WireSpace as an identity universe** (§5.6), recovered as scoping only. The project is named the plural of that document's "Wire Space" concept, and nothing here previously said what the spaces are or what happens when two independently engineered systems meet.
- **Privileged capabilities require a separate build** (§29.1), generalizing the promiscuous-mode rule (§27.4) into a stated policy with a concrete capability list.
- **No interoperability is claimed** (front matter, §32), and independently developed implementations must not be presumed compatible until profiles and vectors exist.

Recorded as a deliberate divergence rather than a recovery:

- That document's invariant 13 held that reachability and authority exist **only** where the Wiring Manifest configures them. WireSpaces has deliberately relaxed this: anonymous `kLocalBus` (§6.4) and Level 0 use (§1.5) exist so that a device works with no Manifest at all (§20, invariant 37). This is the principal philosophical difference between the two generations, and it is a trade, not an oversight.

Noted but not recovered:

- **Domain Control** as a named layer, and the reusable **Communication Component** catalog, are deferred (§0.2). The flattening rule from the latter is kept; the catalog is not.
- That document's finer-grained per-area maturity table duplicates the purpose of §0.2 and is not reproduced.

---

# 1. Architectural Intent

WireSpaces is a lightweight embedded communication fabric intended to span:

- one PC-connected device;
- small MCU networks;
- Classical CAN / CAN FD / CAN XL systems;
- UART and RS-485 links;
- USB or FTDI-style host links;
- Ethernet-connected embedded machines;
- multicore MCUs and SoCs;
- FPGA softcores and pure RTL Endpoints;
- shared-memory and FIFO communication;
- gateways between unlike physical media.

The central idea is deliberately **bus-oriented**, not socket-oriented and not arbitrary-graph-routing-oriented:

> A **Wire** is a logical bus with exactly one **Origin** and zero or more **Nodes**.

WireSpaces should be useful in the smallest possible configuration:

```text
PC ---- one Physical Link ---- Device
```

and remain conceptually similar as the system grows:

```text
PC
 |
USB / Ethernet / FTDI FIFO
 |
Main SoC / FPGA
 |       |        |
CAN     RS-485   shared memory
 |       |        |
ECUs    Nodes    CPU domains / RTL
```

The same Endpoint and Service model should survive these changes in realization.

## 1.1 Why this exists

Embedded systems typically use several unrelated communication mechanisms — direct calls and RTOS queues inside one MCU, shared memory between cores, CAN or RS-485 between MCUs, Ethernet or USB to a host, custom RTL interfaces inside FPGAs — while the *application-level* traffic is remarkably similar across all of them: small commands, status and telemetry, logs and events, request/response, firmware update, and bounded datagrams with a small number of known sizes.

Two further observations shape the design:

1. Traditional stacks often insert a central execution layer between producers and consumers even when the topology is statically known.
2. Embedded systems are usually **centrally engineered**. Their communication relationships are not arbitrary Internet routes discovered at runtime; they are largely fixed relationships between known components.

WireSpaces therefore treats the system as a set of **logical Wires** whose placement onto execution contexts and Physical Links is configurable. The same Service interaction can be realized as a direct callback, an inter-core shared-memory Link, a CAN bus, or an FPGA datapath without redesigning the Service-facing message model.

## 1.2 Design biases

WireSpaces intentionally favors:

- **simple runtime execution** over sophisticated distributed control planes;
- **static/read-mostly tables** over dynamic distributed routing;
- **host-side configuration and validation** over large embedded management stacks;
- **bounded storage** and explicit resource exhaustion;
- **clear local invariants** over implicit "smart" behavior;
- **software/RTL symmetry** where it is natural;
- **zero-configuration bring-up** until topology becomes genuinely ambiguous;
- **implementation-driven standardization** rather than speculative completeness.

A useful summary is:

> **Complexity belongs in constructing configuration and tables, not in executing the data plane.**

## 1.3 Non-goals of the base architecture

The base architecture is not trying to provide:

- Internet-scale routing;
- arbitrary peer-addressed graph networking as the primitive;
- mandatory dynamic routing convergence;
- mandatory distributed discovery on every target;
- a general end-to-end congestion-control algorithm;
- a universal security layer;
- automatic Origin election;
- mandatory full static system modeling before first communication;
- a general byte-stream abstraction;
- a requirement to use dynamic memory, an RTOS, or lock-free algorithms;
- a requirement that tiny targets implement every feature.

## 1.4 Characteristic messages

WireSpaces is **datagram-oriented**. Most Services define a small set of bounded message types; many are fixed-size or tightly bounded rather than arbitrary streams.

Illustrative sizes, not requirements:

```text
Heartbeat             4 B
TemperatureStatus     8 B
MotorCommand         12 B
LinkStatus           16 B
FaultEvent            8-24 B
BootloaderCommand     8-16 B
FirmwareSegment      tens to hundreds of bytes
```

The design pressure is:

> Common embedded Services should be easy to carry over constrained Links without forcing the entire ecosystem to adopt the smallest possible payload.

Classical CAN is an important compatibility floor. A useful tiny device may support only one CAN frame per WS PDU. Mainstream MCU Service libraries should generally remain comfortable with roughly 2-4 CAN frames per PDU for common operations, with larger aggregation available where appropriate.

## 1.5 Usage maturity levels

WireSpaces should be useful long before a user is ready to model, constrain, and statically analyze a whole system. The intended progression is:

```text
Level 0
    Connect devices. Send messages.
    Minimal or no authored topology.

Level 1
    Discover devices. Assign identities and Wires.
    Use host auto-Wiring.

Level 2
    Save and deploy a repeatable static Wiring configuration.

Level 3
    Define richer Services, generated bindings, documentation,
    and whole-system topology.

Level 4
    Add optional WireContracts and advanced static analysis.

Level 5
    Use restricted profiles and stronger assurance processes
    where required.
```

> **Advanced features must not make Level 0 or Level 1 unnecessarily difficult.**

This is a ladder of user commitment, and it is orthogonal to the implementation scaling profiles in §33, which are a ladder of target hardware. A tiny bare-metal MCU can legitimately participate in a Level 3 system, and a Linux gateway can legitimately sit at Level 0 on a bench.

Three consequences are worth stating directly:

- static configuration is a **capability, not an admission requirement**;
- dynamic host-side configuration can still produce a fixed data plane (§19.4);
- advanced analysis is **additive** — WireContracts, authority restrictions, schedulability and redundancy analysis build on the Wire model rather than defining the minimum viable user experience.

---

# 2. Layering and Terminology

The preferred mental model is:

```text
Physical Link
    CAN / UART / RS-485 / USB / Ethernet / shared memory / FPGA FIFO / ...
        |
        v
Link Interface
    one participant's local attachment to that Physical Link
        |
        v
Logical Link Layer (LLL)
    framing / validation / PDU fragmentation and reassembly
    Link-local representations such as WireAlias
        |
        v
complete canonical WireSpaces PDU
        |
        v
Router
    decides local delivery and/or egress Link Interfaces
       / \
      /   \
     v     v
Endpoint   egress LLL / Link Interface
Domain
Dispatcher
   |
Endpoint / Service
```

The responsibilities are intentionally separate:

```text
Link driver / LLL:
    "How does this Physical Link move a WS PDU?"

Router:
    "Where should this complete PDU go?"

Endpoint Domain Dispatcher:
    "Which local Endpoint receives this PDU?"
```

## 2.1 Physical Link

A **Physical Link** is the actual communication medium, bus, connection, or channel: Classical CAN, CAN FD/XL, UART, RS-485, USB, Ethernet, I2C, SPI, a shared-memory queue, an FPGA FIFO or streaming interconnect, a custom serial PHY, or a radio. A Physical Link may be point-to-point or bus-like.

It may also be **master-initiated**, meaning data moves only when one side initiates a transaction. I2C and SPI are the clearest cases, and some RS-485 turnaround schemes behave the same way. See §2.8.

## 2.2 Link Interface

A **Link Interface** is one participant's local attachment to a Physical Link.

A gateway with four external buses and one inter-core shared-memory channel therefore has five Link Interfaces. Link Interfaces may be indexed locally beginning at zero; a small integer such as `uint8_t` is expected to be sufficient for normal systems.

No globally persistent Link Interface identifier is required by the base architecture.

Inter-core communication does not need a separate routing abstraction. A shared-memory or FIFO channel is simply another Physical Link with Link Interfaces on its ends.

## 2.3 Logical Link Layer (LLL)

The **LLL** adapts a Physical Link to canonical WireSpaces PDUs.

Typical responsibilities:

- framing and delimiting;
- integrity validation where the Link profile defines it;
- fragmentation and reassembly when the carrier is smaller than a PDU;
- aggregation when the carrier is much larger than a PDU;
- Link-local control metadata;
- translating `WireAlias <-> WireNumber` where aliases are used;
- optional Link-level flow control;
- bounded scheduling or polling where the carrier is master-initiated (§2.8);
- native Link error/status accounting.

The LLL **does not generally own Wire routing policy**. It may need to understand a Link-local Wire representation such as `WireAlias`, but the generic decision about which other Links or Domains carry a Wire belongs to the Router.

The generic routing boundary is the **complete PDU**, not an arbitrary carrier fragment. This is what permits heterogeneous gatewaying without every pair of Links requiring a custom bridge protocol.

## 2.4 Router

The **Router** owns local Wire forwarding decisions.

It should be thought of primarily as:

- a read-mostly table/database;
- a routing function;
- local forwarding policy;

not necessarily as a thread or task.

The preferred software direction is for Link RX paths and Endpoint Domains to invoke routing in **their own execution context** against shared read-mostly tables. This avoids introducing a single central routing task as an artificial throughput bottleneck.

## 2.5 Endpoint Domain

An **Endpoint Domain** is a local Endpoint ownership and dispatch boundary.

Examples:

- one MCU application domain;
- one CPU core's set of Endpoints;
- one isolated partition;
- one host process/domain;
- one RTL subsystem.

Every Endpoint Domain has a **Dispatcher** that resolves received PDU identity to the appropriate local Endpoint and records/reports invalid delivery attempts.

## 2.6 Port, Endpoint, and Service

An **Endpoint** is a network/message-visible termination identified by `Namespace + EndpointId`. It may belong to an MCU Service, a host application, a hardware/RTL block, a diagnostic utility, or a gateway-local observer.

Endpoint identity identifies the protocol or recipient semantics. It does **not** by itself determine the Wire on which an autonomous transmission should occur (see §10).

A **Port** is the *local* typed, directional interface at a Service boundary — the thing the Service implementation actually holds and calls. It is software- or RTL-facing and has no network representation of its own.

```text
Port        local typed interface; not visible on any Link
Endpoint    Namespace + EndpointId; visible in every PDU
```

The distinction matters because the two are not one-to-one. A Port may bind directly to a single Endpoint, or a Service may implement one Port using several Endpoints. `TxBinding` (§10.3) is exactly an output Port: the Service says "send on my telemetry output" and the deployment decides which Wire and Endpoint that becomes. The receive side is the same idea in the other direction — a bound handler or RTL sink is an input Port, and the Dispatcher (§2.5) is what connects an arriving Endpoint identity to it.

Nothing in the canonical PDU carries Port identity. A Port is a local binding artifact only, and renaming or restructuring Ports is not a protocol change.

A **Service** is reusable functionality exposed through one or more Endpoints, with Ports as its local interface. A Service may run inline in bare metal, in its own RTOS task, with several Services in one task, on another core, on another ECU, in a softcore, or in pure RTL. WireSpaces should not require a specific task model.

Whether Ports become a generated API concept, or remain purely an architectural term describing what `TxBinding` and handler registration already do, is open (§0.2).

## 2.7 Terms intentionally no longer central

Older documents used `PathTag`, `RoutingProfile`, `RoutingCode`, `PeerId`, `WireBand`, and `Main`. These are superseded by the simpler current Wire model.

Current preferred terminology:

```text
Main       -> Origin
Peer       -> Node
PeerId     -> NodeId
MainToPeer -> OriginToNode
PeerToMain -> NodeToOrigin
```

The old term **Link Engine** should not be used as a catch-all for routing. Link-specific mechanics belong to the Link driver/LLL; generic Wire routing belongs to the Router. Where an older document says Link Engine, it usually means what is now the Link driver plus LLL (§2.3), and only the routing connotation is wrong.

A generalized **Router Port** abstraction is not currently needed. Inter-core and internal communication channels are ordinary Link Interfaces.

Three further terms from the earliest generation have no current equivalent:

```text
Route            no separate path object; forwarding is a local table (§11)
ParticipantId    no third identity space beyond Endpoint and Node identity
Wire Space       survives only as an identity scope, renamed WireSpace (§5.6)
```

The full list of superseded concepts, with reasons, is §37.

## 2.8 Master-initiated and polled Links

Not every carrier lets both sides transmit whenever they choose. On I2C and SPI, and on some half-duplex turnaround schemes, data moves only when one side initiates a transaction. A Node on such a Link cannot push a `NodeToOrigin` publication at the moment it is produced; the traffic appears only when the master polls for it.

This is a Link mechanic, and it belongs entirely to the LLL and driver. The LLL may therefore:

- schedule transmission of Service-owned state on its own timetable;
- initiate a bus transaction purely to collect whatever a Node has queued;
- poll at a cadence unrelated to when any Service called `send()`.

None of that makes the LLL a producer:

> **Initiating a transfer is not authoring a message.** Producer authority stays with the source Endpoint that supplied the payload, regardless of which side of the Link caused the bytes to move.

The practical consequences are worth stating, because they are easy to get wrong:

- A polled Node's publication is stale by up to one polling interval. That latency is a Link capability fact (§17), not a Service behavior, and it is exactly the kind of thing freshness handling (§26.5) exists to expose.
- The master's polling cadence bounds the Node's effective TX rate, so static capacity checking (§20.1) has to account for it.
- A poll that returns nothing is not an error. An empty response is the normal case on a mostly idle Link and must not be counted as a Link fault (§18.1).
- QoS on a polled Link is limited by cadence, not arbitration. A Critical publication cannot beat the next poll, so such Links are usually QoS-Minimal (§14.1).

Exact I2C and SPI transaction formats are future Link-profile work; only their architectural placement is settled here.

---

# 3. Canonical PDU Descriptor

The current canonical base descriptor is **40 bits / 5 bytes**.

```text
Control: 8 bits
    Namespace             2
    TransportType         3
    QoS                   2
    HasHeaderExtensions   1

RoutingWord: 16 bits
    WireNumber           10
    Direction             1
    NodeId                5

EndpointId: 16 bits
--------------------------------
Base descriptor:         40 bits
```

Exact physical bit ordering is a wire-format/profile concern. The architectural significance is the field width and meaning.

## 3.1 Canonical address-space snapshot

| Concept | Current canonical capacity |
|---|---:|
| Namespaces | 4 |
| EndpointId width | 16 bits per Namespace |
| Valid EndpointIds | 1..65535 |
| Usable Namespace/EID combinations | 262,140 |
| WireNumber width | 10 bits |
| NodeId width | 5 bits |
| Nodes per Wire | 31 Nodes + 1 Origin |
| QoS field | 2 bits / 4 canonical values |
| TransportType | 3 bits / up to 8 values |

`EndpointId == 0` is invalid in every Namespace, which is why the usable combination count is `4 * 65,535` rather than `4 * 65,536`.

The exact allocation of the 10-bit WireNumber space is still provisional; the current direction reserves a high range for device-private communication and one top value for `kLocalDomain` (§5).

## 3.2 Carrier encodings need not be literal

A Physical Link is **not required to transmit this exact five-byte sequence literally**. A Link profile may:

- encode some fields in native metadata;
- use aliases;
- omit values implied by the profile;
- use a compact representation;
- reconstruct the canonical descriptor before handing a PDU to the Router/Endpoint layer.

Classical CAN is the clearest example: QoS, Direction, WireAlias, and NodeId live in the CAN arbitration ID rather than consuming CAN data bytes.

## 3.3 Header extensions

The base descriptor includes `HasHeaderExtensions`. The current compact-header target is that the **canonical WS header remains at most approximately 8 bytes including ordinary WS header extensions**.

This target is useful for logging and bounded-buffer implementations. It is not yet a frozen byte-exact extension format.

**Link-local LLL extensions are separate.** For example, an 8-byte QoS-Full flow-control credit extension on an Ethernet LLL frame is Link metadata, not part of the canonical WS PDU header budget.

---

# 4. Wire Model

A **Wire** is a logical bus containing:

- exactly one Origin;
- zero or more Nodes;
- a canonical WireNumber when named;
- zero or more Physical Links that realize it;
- local Endpoint participation as configured.

A Wire is a communication-domain abstraction, not a list of pairwise Endpoint connections. A Wire may coincide with one complete Physical Link, select only a subset of participants on a Physical Link, or span several Physical Links through gateways. Multiple Wires may share the same Physical Link and may overlap in device membership.

```text
Wire MotorBus

Origin:
    MainSoC

Nodes:
    MotorA
    MotorB
    MotorC
```

## 4.1 Direction semantics

The Wire supports two structural directions:

```text
OriginToNode
NodeToOrigin
```

### OriginToNode

```text
NodeId == 0      broadcast to all Nodes
NodeId 1..31     selected destination Node
source           Origin
```

### NodeToOrigin

```text
NodeId 1..31     transmitting/source Node
NodeId == 0      invalid
required sink    Origin
```

On a broadcast Physical Link, other Nodes may physically observe and optionally consume `NodeToOrigin` publications where filtering/configuration permits it. That does not make arbitrary Node-to-Node unicast a base Wire primitive.

If two Nodes require substantial direct addressed interaction, another Wire can be created with one of them as Origin.

## 4.2 Origin is a per-Wire role

`Origin` is not a permanent device class.

```text
MainComputer
    |
  Wire A
    |
   ECU1       Node on Wire A
    |
  Wire B
   /   \
 ECU2 ECU3    ECU1 is Origin on Wire B
```

A device may be Origin on several Wires, Node on several Wires, or Origin on one and Node on another. There is no universal system-wide "master" implied by the model.

## 4.3 Wire structural invariants

Current strong invariants:

- exactly one semantic Origin per Wire;
- NodeIds are unique across the entire Wire, including across spliced segments;
- a participant is not both Origin and Node on the same Wire;
- `NodeId == 0` is not an individual Node;
- gateway forwarding preserves Direction, NodeId, Namespace, EndpointId, TransportType, QoS, and PDU payload. The **only** field a forwarding step may change is the Wire representation, and only through an explicitly configured Wire Splice (§7). Anything that alters other fields or re-originates traffic is a higher-level transformation service, not forwarding;
- physical realization does not change Wire identity.

## 4.4 Why a bus rather than pairwise edges

An earlier family of designs tended toward one logical relationship per producer/consumer pair. The bus-oriented Wire is simpler and scales better for common embedded topologies:

```text
Origin
  |
  +-- Node 1
  +-- Node 2
  +-- Node 3
  ...
  +-- Node 31
```

is one Wire, not 31 independent network objects. This is particularly natural for CAN, RS-485 multidrop, shared-memory broadcast domains, host-to-device control groups, and FPGA on-chip interconnects.

It also means a large FPGA does not need one Wire per geometric path across the fabric (§5.4, §28.3).

## 4.5 Physical Wires and Virtual Wires

The simplest Wire is just a physical bus.

> A physical bus is itself a Wire. Every physical bus therefore has a Wire identity, whether or not a canonical WireNumber has been assigned to it yet.

A **Virtual Wire** is a Wire whose logical broadcast domain is not exactly one complete physical bus. A Virtual Wire may be formed by:

- taking the union of several physical buses;
- selecting a subset of participants on one physical bus;
- selecting subsets from several physical buses and joining them through gateways or splices.

Therefore:

- every physical bus has a physical Wire identity;
- some logical Wires span several physical Link segments;
- some physical participants may be excluded from a Virtual Wire even though they share one of its constituent physical buses.

These are two named cases of one abstraction, not two mechanisms. Routing, Direction, NodeId, and Endpoint semantics are identical in both. The distinction matters for explaining `kLocalBus` (§6), for tooling that must show a user what a Wire physically covers, and as a reminder that the common case needs no composition at all.

---

# 5. Wire Scope and WireNumber Allocation

WireSpaces needs several scopes of communication without inventing separate message formats.

The current scope ladder is:

```text
kLocalDomain
    one Endpoint Domain only

Device-private Wire
    one device / SoC, possibly crossing cores/domains

Named network Wire
    routable across devices and Physical Links

kLocalBus
    Link-local representation of the native Wire;
    anonymous until mapped to a canonical WireNumber
```

## 5.1 Named network Wires

A named Wire has a canonical `WireNumber`. Generic routing and cross-Link forwarding use that canonical identity.

Named Wire metadata may eventually include a stable UUID, a human-readable name, and configuration/version identity. The exact metadata schema is not part of the base PDU.

The UUID and name exist for the same reason device UUIDs do (§19.7): they provide **continuity in tooling and logs even when the short WireNumber changes**. A WireNumber is a compact routing identity and may legitimately be reassigned between deployments, during commissioning, or when a topology is reorganized. Historical captures, drop journals (§18.3), and exported Wiring should remain interpretable across such a change, which requires an identity that is not the 10-bit number.

## 5.2 Device-private Wires

A high WireNumber range is reserved for **device-private Wires**.

Current proposed split:

```text
0..895       network-visible WireNumber space
896..1022    device-private Wires (127 values)
1023         kLocalDomain
```

The exact numeric fences remain provisional, but the **range-based model and roughly 127 internal Wires are considered ample** for normal CPU-to-CPU / domain-to-domain communication.

A device-private Wire is a real Wire. It has one Origin, one or more Nodes, Direction semantics, Endpoint semantics, and ordinary routing. It:

- can cross internal Physical Links such as shared memory;
- can span CPU cores or Endpoint Domains;
- **must not be emitted onto an external Link Interface as a device-private WireNumber**;
- must not be accepted as externally addressable network state.

The single sanctioned exception is a **Wire Splice** (§7): a device-private Wire may be spliced to a network-visible Wire, and the splice rewrites the Wire representation *before* egress. What leaves the device is therefore always a network-visible WireNumber, never a device-private one. The scope check is evaluated on the post-splice Wire.

Absent such a splice, an external LLL/Router boundary treats a device-private Wire on external ingress or egress as invalid configuration/traffic.

## 5.3 `kLocalDomain`

`kLocalDomain` is the top reserved Wire value and means:

> Deliver within the originating Endpoint Domain; do not enter a Link Interface.

This is useful when several Endpoints in one Domain communicate using ordinary Endpoint/PDU APIs but the communication should never leave the Domain. With static tables this can approach the cost of a table lookup plus a function call.

The same `kLocalDomain` value is used independently in every Endpoint Domain because its scope is inherently local. Unlike a device-private Wire, `kLocalDomain` is **never** spliceable and never reaches a Link Interface.

`kLocalDomain` is distinct from `kLocalBus`:

```text
kLocalDomain:
    no Physical Link involved

kLocalBus:
    the native Wire represented on one Physical Link
```

## 5.4 FPGA interpretation

Device-private Wire count should **not** scale with physical distance or register stages inside an FPGA.

These do not create new Wires by themselves:

- long on-chip routes;
- additional pipeline stages;
- register slices;
- clock-domain crossings;
- crossing to the other side of the die;
- crossbar or NoC traversal.

A long hardware path may simply be the physical realization of one logical Wire. Internal Wires exist because of **logical communication/domain structure**.

A multicore MCU might use only a handful:

```text
ControlInternalWire
SensorInternalWire
HealthInternalWire
DebugInternalWire
```

An FPGA with hundreds of blocks may likewise use a small number of bus-like Wires. If an implementation someday needs hundreds of independent internal Wires, an extended profile can be considered rather than spending scarce canonical bits today.

## 5.5 Proximity does not imply locality

§5.4 says that physical distance does not create Wires. The inverse also holds, and is easier to get wrong:

> **Physical closeness does not make something local.**

A peripheral on the same PCB, reached over SPI or I2C, is normally a **separate Endpoint Domain** on an ordinary Physical Link, using an ordinary network-visible Wire. "Board-local", "on the same die", or "same connector" implies none of the following:

```text
kLocalDomain
a device-private WireNumber
Inline delivery
skipping Endpoint dispatch
skipping structural validity checks
```

`kLocalDomain` (§5.3) means *within this Endpoint Domain's dispatch scope*, which is an authority and dispatch property, not a distance. Two cores 3 mm apart that own separate Endpoint sets are two Endpoint Domains communicating over a Link; one core and an RTL block sharing a dispatch scope may be one Endpoint Domain even if the routing between them crosses the die.

The device-private range (§5.2) is likewise scoped by *device*, not by proximity. A companion chip on the same board is not inside the device boundary just because it is inside the enclosure, and traffic reaching it needs an ordinary Wire or an explicit splice (§7).

## 5.6 One WireSpace is one identity universe

Every canonical identifier in this architecture — WireNumber, NodeId, and `Namespace + EndpointId` — is unique within **one statically configured WireSpace**, and means nothing outside it. This is what the plural in the project's name refers to.

```text
within one WireSpace:   WireNumber 42 is one specific Wire
across two WireSpaces:  two unrelated Wires may both be 42
```

Scoping already exists at two inner levels; this is the outermost one:

```text
Link-scoped        WireAlias                         (§6)
Domain-scoped      kLocalDomain                      (§5.3)
Device-scoped      device-private WireNumbers        (§5.2)
WireSpace-scoped   WireNumber, NodeId, Namespace/EID (here)
```

The operational rule is the same shape as splicing:

> **Two independently engineered WireSpaces do not become one by being connected.** Joining them requires a gateway that explicitly translates identity, in the same way a device-private Wire requires an explicit splice to become externally visible.

A plain forwarding gateway (§12) is **not** such a translator. It preserves canonical identity by design (§4.3), so wiring two WireSpaces together with one produces silent identity collisions: two different Wire 42s merge, NodeIds duplicate, and the uniqueness invariants (§36) are violated with nothing detecting it.

This is deliberately a scoping rule and nothing more. A WireSpace is **not** claimed here as a security or memory-protection boundary; it is the scope within which identity is meaningful and uniqueness is checkable. Whether it should additionally carry policy or trust semantics, and what a translating gateway looks like, are open (§38).

Most systems are one WireSpace and can ignore this section entirely. It exists so that the first project to connect two independently developed WS systems recognizes the problem before debugging it.

---

# 6. `kLocalBus`, Anonymous Wires, and WireAlias

`kLocalBus` exists to make the first-use experience extremely easy while still allowing a system to grow into explicit multi-Link routing.

Because every physical bus is itself a Wire (§4.5), a Link always has a *native* Wire — the one its own physical bus realizes. The model follows from that:

> `kLocalBus` is **WireAlias 0**, the Link-relative compression code for **this Link's own physical Wire number**.

Alias 0 is therefore not an arbitrary reserved value. It is the one Wire identity a Link never has to be told, because it is the Link itself. Two consequences shape the rest of this section:

- when the Link's own Wire has been assigned a canonical WireNumber, alias 0 is a pure compression of it and canonicalizes losslessly (§6.2);
- when it has not, alias 0 still names something real — this bus — but that identity is meaningful only on this Link, which is exactly what limits forwarding and splicing (§6.3, §6.8).

`kLocalBus == 0` does **not** mean that every physical bus has canonical WireNumber zero.

## 6.1 WireAlias

A `WireAlias` is a compact, Link-scoped representation of a canonical Wire.

For the current constrained 11-bit Classical CAN direction:

```text
WireAlias 0      kLocalBus
WireAlias 1..7   configured aliases for named Wires
```

Aliases are independent on each Physical Link:

```text
Canonical Wire 107

CAN_A:
    alias 2 -> Wire 107

CAN_B:
    alias 6 -> Wire 107
```

Generic routing never treats alias 2 and alias 6 as different Wires. Each ingress LLL canonicalizes its alias before handing the PDU to the Router.

The seven named aliases are a **per-CAN-Link** limit, not a system-wide Wire limit.

## 6.2 Configured `kLocalBus`

Once this Link's own physical Wire has been assigned a canonical WireNumber:

```text
CAN_A physical Wire = 42
    -> alias 0 on CAN_A means Wire 42
```

the LLL translates in both directions:

```text
Receive:
    alias 0
        -> restore this Link's own canonical WireNumber (42)

    alias 1..7
        -> configured alias map -> canonical WireNumber

Transmit:
    canonical Wire == this Link's own physical Wire
        -> encode alias 0

    canonical Wire has a configured alias on this Link
        -> encode that alias

    otherwise
        -> not representable on this Link; reject before emitting a frame
```

Example:

```text
CAN_A physical Wire = 107

RX alias 0    -> canonical Wire 107
RX alias 5    -> whatever CAN_A's alias 5 maps to
TX Wire 107   -> alias 0
```

Because alias 0 restores an *arbitrary* canonical number, a physical bus may hold any WireNumber while still using the single-code compact representation on the wire. The scarce values 1..7 are spent only on Wires whose identity must stay explicit while crossing this particular segment.

After canonicalization:

- routing uses Wire 42;
- logs use Wire 42 / its human-readable name;
- local Endpoint delivery uses canonical identity;
- cross-Link forwarding is permitted according to the route table;
- splicing is permitted, because the Wire now has canonical identity.

No generic subsystem should continue carrying the label `kLocalBus` once canonicalization succeeds.

## 6.3 Anonymous LocalBus fallback on RX

If this Link's own physical Wire has **not** been assigned a canonical WireNumber, received `kLocalBus` traffic is still valid. It represents an **Anonymous Local-Bus Wire** — a real bus that has no canonical identity yet:

```text
RX kLocalBus
    + this Link's own Wire is unnamed
        -> local Endpoint Domain delivery allowed
        -> generic cross-Link forwarding not allowed
        -> splicing not allowed
```

This enables a one-bus prototype to work without assigning any WireNumber.

For diagnostics before naming, logs should disambiguate the physical context:

```text
CAN_A::LocalBus
UART_0::LocalBus
```

rather than writing only `LocalBus`.

## 6.4 Anonymous LocalBus TX

Zero-configuration TX is allowed only when unambiguous.

```text
0 eligible Link Interfaces:
    fail

1 eligible Link Interface:
    anonymous LocalBus TX may automatically use that Link

2+ eligible Link Interfaces:
    ambiguous -> fail until Wiring assigns/selects a Wire
```

The protocol should not guess based on "first Link," recent RX traffic, or hidden heuristics.

This produces a useful configuration gradient:

```text
one Link      -> almost zero Wire configuration
several Links -> explicit destination binding becomes necessary
gateway/multihop -> canonical WireNumber is required
```

Configuration is introduced when the topology actually creates ambiguity.

## 6.5 Anonymous LocalBus forwarding invariant

An anonymous LocalBus must **never** be forwarded as `kLocalBus` onto another Link.

Bad:

```text
CAN_A kLocalBus -> gateway -> CAN_B kLocalBus
```

because `kLocalBus` on CAN_B means CAN_B's own native Wire, which may be unrelated.

Correct:

```text
CAN_A kLocalBus
    -> alias map -> Wire 42
    -> Router
    -> CAN_B Wire 42 -> alias 3
```

If CAN_A's LocalBus has no canonical mapping, cross-Link forwarding fails by construction.

## 6.6 LocalBusOnly devices

A small **LocalBusOnly** device may permanently use only alias 0 and never know its canonical WireNumber.

Such a device may know only alias 0, never participate in generic forwarding, implement only optimized N=1 CAN, use a small dispatch table or switch, and hold no canonical Wire database. This is a first-class small-device profile, not an error state.

It can still participate in a larger named/multihop Wire because a gateway or richer participant maps:

```text
alias0 on this CAN bus <-> canonical Wire 42
```

This is an important scalability property for tiny MCUs and RTL nodes.

**Being commissionable is independent of being routable.** A LocalBusOnly or N=1-only node may still support having its NodeId assigned, and its Wire identity assigned or changed, without implementing any forwarding, alias table, or gateway behavior. Management capability and routing capability are separate axes, and the small-device profile should not be read as "unmanageable."

## 6.7 Alias-map invariants

For one Link Interface:

- every active non-local alias maps to exactly one named Wire;
- a named Wire normally has at most one active alias on that Link;
- alias 0 always means the native/local Wire representation;
- alias reassignment must not reinterpret fragments belonging to an already-started reassembly;
- the simplest safe update is to flush/expire affected LLL reassembly state before activating the new alias mapping.

## 6.8 Anonymous LocalBus is not spliceable

A splice joins two segments of **one logical Wire** and therefore requires canonical identity on both sides.

An anonymous (unmapped) LocalBus has no canonical identity, no agreed Origin, and no coordinated NodeId space beyond its own Physical Link. Splicing it is not permitted.

```text
anonymous CAN_A::LocalBus  -> splice -> Wire 42     rejected
CAN_A alias0 -> Wire 42    -> splice -> Wire 903    permitted
```

The remedy for wanting the first case is to name the Wire: map alias 0 to a canonical WireNumber, at which point ordinary splicing rules apply. Tooling should reject splice configuration referencing an unmapped alias 0 rather than silently inventing an identity.

---

# 7. Wire Splicing

## 7.1 Semantic definition

A **Wire Splice** joins two Wire segments so that they become **one logical Wire**.

The resulting logical Wire must have:

- exactly one Origin across the combined topology;
- unique NodeIds across all Nodes on both segments;
- preserved Direction semantics;
- preserved Endpoint/Service semantics.

```text
Segment A                         Segment B

Origin
  |
Node 1
  |                                 Node 3
Node 2                              Node 4

          <------ splice ------>

Result:

One logical Wire
    Origin
    Nodes 1,2,3,4
```

The Origin may be on either segment.

> A splice is **not** an application-level translator. The two segments are treated as one logical Wire, and only the local Wire *representation* changes.

## 7.2 A splice applies before egress

This is the ordering rule that makes device-private Wires usable off-device.

On the transmit/forward path:

```text
PDU on device-private Wire X
    |
Router lookup for X
    |
route entry carries spliceWire = Y   (Y is network-visible)
    |
Wire representation becomes Y        <-- splice applied here
    |
device-private scope check runs on Y  -> passes
    |
egress LLL maps Y to its local alias
    |
transmit
```

So internally the traffic is Wire X; what leaves the device is Wire Y. The device-private scope rule in §5.2 is not weakened, because it is evaluated after the splice, on the representation actually emitted.

On the receive path the mapping is applied in the mirror position, after alias canonicalization and before routing:

```text
external frames
    |
ingress LLL
    |
WireAlias -> canonical Wire Y
    |
splice applied: Y -> X               <-- before Router
    |
Router / local delivery on X
```

Rules:

- A splice is applied **once** per local routing step. Chained or recursive splices, where the output of one splice is re-resolved through another, are not permitted in the base model. The simplest implementations should not re-enter routing after applying a splice.
- Alias canonicalization always happens before splicing on ingress, and always after splicing on egress. The LLL deals in aliases; the splice deals in canonical WireNumbers.
- The scope validity check (device-private vs network-visible) is evaluated on the representation that crosses the boundary, not the one the producer used.

**Open:** whether a splice is best expressed as one bidirectional mapping in the Wire/route entry, or as two directional route entries. A single bidirectional mapping is easier to validate and harder to configure asymmetrically by accident; two entries are more uniform with the rest of the route table. This should be settled by the first gateway implementation.

## 7.3 `spliceWire` implementation concept

A routing-table entry may carry an optional field conceptually named:

```text
spliceWire
```

This is the implementation mechanism for changing the local Wire representation as a PDU crosses the splice. See §11.3 for its position in the route entry.

## 7.4 What a splice preserves

A splice preserves:

```text
Direction
NodeId
Namespace
EndpointId
TransportType
QoS
payload
```

Only the Wire representation changes, as required by the local topology. This is the carve-out named in §4.3.

## 7.5 Intended initial use

The common and easiest case is:

```text
populated Wire
    <splice>
empty/new Wire segment
```

which avoids address and role conflicts.

Splicing two already-populated segments is not fundamentally impossible, but management policy remains provisional. Conservative tooling may initially reject it unless it can validate:

- exactly one Origin across both segments;
- no NodeId collision;
- compatible direction/address expectations;
- no forwarding loops.

## 7.6 Validation responsibilities

Host tooling, not the runtime data plane, should establish before installing a splice:

- both sides have canonical identity (§6.8);
- exactly one Origin across the combined Wire;
- NodeIds are unique across the combined Wire;
- the combined realization remains acyclic (§12.4);
- both Links can represent the resulting PDUs (§17);
- the external side is a network-visible WireNumber, so nothing device-private is emitted.

The NodeId uniqueness requirement has a consequence worth stating explicitly: if several devices each splice their own device-private Wire onto **one shared** external Wire, their internal NodeId assignments must be coordinated, because they are now Nodes on the same logical Wire. Giving each device its own external Wire avoids the coordination entirely and is the recommended default during bring-up.

**Open:** whether tooling should reject same-external-Wire splices from multiple devices by default, or attempt NodeId coordination.

---

# 8. Default Internal Debug Wire

One particularly useful application of device-private Wires and splicing is a **default debug/maintenance path**.

## 8.1 Motivation

Before static system allocation, multiple devices cannot safely assume that the same *external* WireNumber refers to their own private debug traffic. Instead, every device may use the same **device-private debug Wire identity** without collision, because that identity never leaves the device unspliced.

```text
Device

 Service A ----\
 Service B -----+---- InternalDebugWire ---- splice ---- host-facing Wire
 Service C ----/                               |
                                               v
                                               PC
```

The Services are Nodes on the internal debug Wire. The effective debug Origin — normally the host — is reachable on the external segment through the splice.

`InternalDebugWire` is a conventional device-private Wire used for internal diagnostics. It is not a new protocol layer.

## 8.2 Host path

Once the device is commissioned:

```text
InternalDebugWire (e.g. 1020)
    <splice>
Wire 101
    |
USB / Ethernet / CAN
    |
PC
```

This is also how the Domain Local Link Telemetry Service (§18.4) reaches a host. Telemetry is published to `InternalDebugWire`; the splice is what makes it externally visible. Without a splice, telemetry published to a device-private Wire stays on the device by construction.

## 8.3 Benefits

A reusable Service can have a default injected `debug_tx` binding (§10) without knowing:

- which physical Link reaches the developer;
- whether the host is USB, UART, CAN, Ethernet, or radio;
- which external WireNumber has been assigned;
- whether the device is still in anonymous bring-up mode.

All Services become visible once the splice exists, without each Service being individually reconfigured. Selected Services can later be moved to different application/diagnostic Wires if desired.

This gives WS a useful zero-configuration bring-up path while avoiding accidental external WireNumber collision.

## 8.4 Security boundary

The splice is also where private traffic becomes externally visible, which makes the trust boundary explicit and inspectable. A remote maintenance link should not blindly splice every internal Wire (§29).

**Open:** whether `InternalDebugWire` receives a standard reserved device-private WireNumber, or remains a per-deployment convention.

---

# 9. Endpoint Domains and Dispatch

An Endpoint Domain owns a set of local Endpoints and provides their dispatch boundary.

```text
canonical PDU
    |
Endpoint Domain Dispatcher
    |
(Namespace, EndpointId)
    |
Endpoint
```

## 9.1 Dispatcher responsibilities

The Dispatcher should:

- resolve `(Namespace, EndpointId)` to a registered Endpoint;
- deliver the complete PDU/message to that Endpoint;
- reject or count unknown EIDs;
- reject malformed/unsupported local delivery;
- provide diagnostics/status for delivery errors;
- support `kLocalDomain` fast/local delivery.

A richer implementation may also validate expected TransportType, allowed message sizes, direction, and local Service state.

The Dispatcher does **not** decide cross-Link routing.

## 9.2 Endpoint registration

The exact API remains implementation-specific. Expected implementations include static generated tables, fixed arrays, compile-time registration, and host dictionaries/maps on larger systems. The embedded baseline should avoid dynamic allocation requirements.

## 9.3 Endpoint source identity

A received message should have enough context to identify its source unambiguously within the Wire model.

For a named Wire, logical source identity includes at least:

```text
WireNumber
Direction / source participant
Namespace
EndpointId
```

For an anonymous LocalBus, source identity is scoped by the ingress Link Interface until the Wire is named.

## 9.4 Delivery policies: `Inline` and `Serialized`

Delivery to a local Endpoint has two high-level policies:

```text
Inline
    invoke the target immediately in the producer's context

Serialized
    queue/copy for later execution in the Service's own context
```

`Inline` supports the high-performance receive path:

```text
Link driver task
    |
LLL completes PDU
    |
Router lookup
    |
Dispatcher lookup
    |
Endpoint callback
```

No network-stack task is required.

`Serialized` is the normal RTOS shape. The Endpoint callback should usually do little more than copy/enqueue into the Service's own inbox and return:

```text
CAN task -> Dispatcher -> Service inbox -> Service task
```

A Service may have several Endpoints but one serialized inbox.

This distinction matters for **location independence** (§13.4). A Service configured for serialized delivery must remain serialized when its peer moves onto the same MCU, rather than silently becoming an arbitrary cross-thread direct call. WireSpaces cannot eliminate concurrency bugs, but it can preserve explicit delivery semantics across placement changes.

The exact API is not frozen.

## 9.5 Dispatch table synchronization and Endpoint lifetime

The Dispatcher can use the same read-mostly design philosophy as the Router (§11.2). A capable multicore system may use seqlocks or snapshots; a small static MCU may use a small `const` array with linear search and no lock.

A subtle lifetime issue exists if a runtime update can remove or destroy an Endpoint while another thread has just read its callback pointer. Early implementations should prefer stable Endpoint registrations, or adopt a safe publication/lifetime mechanism before introducing runtime Service replacement.

---

# 10. Service Transmit Bindings

Receiving naturally tells a Service where a packet came from. Autonomous transmission does not. WireSpaces therefore needs explicit **Service TX bindings**, preferably injected into the Service.

## 10.1 Reply-only Service

A Service that only replies can use the receive context:

```text
request:
    Wire 12
    Node 7

reply:
    same Wire
    reverse Direction
    matching Node semantics
```

No separate static output Wire is required.

## 10.2 Remembered peer/session

A Service may explicitly retain a recent reply/destination context:

```text
client contacts Service
    |
Service stores destination context
    |
later publishes progress/status to that peer
```

This is Service policy, not an implicit WS global behavior.

## 10.3 Autonomous transmission

A Service that transmits on its own initiative needs a configured destination Wire. Rather than hard-code a WireNumber into reusable Service logic, the preferred architecture is an injected handle:

```cpp
class TemperatureService {
public:
    explicit TemperatureService(TxBinding telemetry_tx);

    void tick();

private:
    TxBinding telemetry_tx_;
};
```

Conceptually the Service says:

```text
send on my telemetry output
```

not:

```text
send on CAN1 / Wire 42
```

The deployment binds that output. A Service may have several outputs, each wired differently:

```text
status_tx
fault_tx
debug_tx
```

Each of these is an output **Port** (§2.6): a local typed interface whose network meaning is supplied by the deployment rather than by the Service. The Service's source code names the Port; the Wiring names the Wire and Endpoint.

## 10.4 Defaults and safety

Useful defaults reduce configuration during bring-up:

- single-Link anonymous LocalBus (§6.4);
- the device-private Internal Debug Wire (§8), which is the natural default for `debug_tx`.

Safety- or control-critical application outputs should generally require explicit bindings rather than silently falling back to a development/debug route.

---

# 11. Router Architecture

The Router is intentionally a **simple local forwarding mechanism**, not a distributed routing protocol. The preferred software architecture is a shared, read-mostly table accessed concurrently from multiple execution contexts.

## 11.1 Caller-context routing

Instead of:

```text
all RX/TX -> central Router task -> destinations
```

prefer:

```text
CAN RX context --------\
Endpoint Domain context +--> shared routing table/function
Ethernet RX context ---/
```

Each producer performs the route lookup itself and then enqueues/delivers to the selected destinations.

Benefits:

- no unnecessary central serialization;
- natural multicore scaling;
- deterministic static read path;
- the same conceptual model works in host software and RTL.

For a Service-generated message, the injected TX binding and Wire identify the routing context without requiring an incoming Link.

## 11.2 Read-mostly routing tables

Normal deployed routing configuration is expected to be static for long periods, so the hot path should behave like read-only state. Runtime reconfiguration is still allowed. Candidate synchronization strategies:

### Seqlock-style tables

Readers:

```text
read generation
read route entry
re-read generation
retry if changed
```

Advantages: lightweight readers, no reader mutex, easy to split into independent subtables.

Tradeoff: readers may retry while writers are active, so heavy runtime configuration degrades routing WCET. This is acceptable because frequent route-table writes are not the normal data-plane operating mode.

### Immutable-table pointer swap

A writer builds a replacement table out-of-band, validates it, then atomically replaces the active pointer.

Advantages: readers never retry; very clean static-reader behavior.

Tradeoff: temporary duplicate storage and reclamation/lifetime management.

The architecture does not require one mechanism yet.

## 11.3 Routing key and egress representation

For a received named-Wire PDU, the simple gateway lookup is:

```text
(WireNumber, ingress_link_interface_index)
    ->
{
    egress_link_interface_bitmask
    local Endpoint Domain delivery
    optional spliceWire
}
```

The API does not need a generalized `RouterPort` type. Link ingress is identified by Link Interface; local Endpoint-originated routing can use local call context/domain information directly.

`spliceWire`, when present, rewrites the Wire representation as described in §7.2 — after the ingress alias has been canonicalized, or before the egress LLL encodes its own alias.

## 11.4 Egress bitmask

A bitmask is the preferred baseline representation for Link egress sets:

```text
0 = CAN_A
1 = CAN_B
2 = CAN_LOGGER
3 = ETH_1

MotorWire:
    ingress 0 -> 0b1100
    ingress 1 -> 0b1100
    ingress 3 -> 0b0011
```

Mask width follows configured interface count:

```text
<= 8   -> uint8_t
<= 16  -> uint16_t
<= 32  -> uint32_t
<= 64  -> uint64_t
```

This makes multicast little more expensive than unicast in the routing table.

## 11.5 Lookup implementation

WireNumber-to-entry lookup may be implemented as appropriate for target scale:

- short linear static array;
- sorted array / binary search;
- generated switch;
- direct-index table;
- hash/map on hosts.

This is an implementation choice, not a protocol feature.

## 11.6 Small-device implementation

A tiny target may simply use:

```text
for each route entry:
    if wire matches:
        send
```

or a generated `switch (wire) { ... }`.

If the table never changes, no lock is needed, no seqlock is needed, and no central Router object may be needed at all. The architecture should permit generated constants and direct calls to compile away most of the apparent abstraction.

---

# 12. Gateway Forwarding

The generic gateway forwarding unit is a **complete canonical PDU**.

```text
Physical-Link-native frames
    |
ingress LLL
    |
complete canonical PDU
    |
Router
    |
egress LLL(s)
    |
Physical-Link-native frames
```

Example heterogeneous bridge:

```text
3 Classical CAN frames
    -> CAN PDUA reassembly
    -> 1 canonical WS PDU
    -> Router
    -> UART/COBS or Ethernet Link representation
```

A same-profile cut-through/frame-forwarding optimization may exist later only if it is behaviorally equivalent. It is not the generic architecture.

There does not need to be a dedicated gateway broker task. Each Link's RX path can reconstruct a PDU, query the Router, and hand the PDU to the egress path in its own context:

```text
CAN_A LLL ----\
CAN_B LLL -----\
CAN_C LLL ------+--> routing table --> Ethernet TX input
CAN_D LLL -----/
```

A pure gateway can therefore be nearly: Link drivers, LLLs, a routing table, and TX synchronization/queues.

## 12.1 Flood-and-filter baseline

The base gateway behavior is intentionally close to **flood-and-filter within the configured realization of a Wire**.

If Wire 42 spans several branches, the gateway may forward an `OriginToNode` unicast PDU to every branch that can contain Wire 42 and allow non-target Nodes/Endpoint Domains to ignore it.

A more detailed branch map can later prune based on NodeId if bandwidth pressure justifies the complexity.

## 12.2 Independent egress results

Forwarding to multiple destinations is not transactional.

```text
PDU -> {CAN_A, CAN_B, Ethernet}

CAN_A accepted
CAN_B congested / rejected
Ethernet accepted
```

CAN_A and Ethernet still transmit. Failure on one egress must not cancel or roll back successful egresses.

## 12.3 Local delivery is independent

A gateway may host Endpoints on the same Wire it forwards. One PDU may therefore be:

- delivered locally;
- forwarded to zero or more Link Interfaces;
- both;
- neither (drop/error).

No fake `LOCAL` Link Interface is needed merely to represent Endpoint Domain delivery.

A Wire may also have a purely observational local tap — a logger or monitor — or no local semantic consumer at all.

## 12.4 Loop model

The base forwarding realization for a Wire should be acyclic.

A simple bridge between A and B is not itself problematic; the problem is a physical/global forwarding graph that permits a forwarded copy to return through another path. Splices are part of that graph and must be included when validating it.

Host tooling should validate Wire realization loops before configuration is installed.

The base protocol does not need hop counts, spanning tree, duplicate suppression, or general loop recovery. Those belong to a future explicit redundant/cyclic profile if a real use case requires them.

## 12.5 Carrier adaptation is asymmetric

Fragmentation and aggregation are inverse shapes of the same LLL responsibility:

```text
CAN:
    one WS PDU -> several tiny CAN frames

Ethernet:
    several small WS PDUs -> one larger Ethernet transfer
```

```text
Ethernet payload

+---------+
| WS PDU  |
+---------+
| WS PDU  |
+---------+
| WS PDU  |
+---------+
```

An Ethernet LLL may aggregate until the MTU is nearly full, a maximum latency expires, high-QoS traffic requires transmission, the queue becomes idle, or another profile-specific condition occurs.

The Router sees complete PDUs in both cases.

## 12.6 Electrical visibility is not membership

Flood-and-filter (§12.1) means a gateway may put a PDU onto every branch that could contain the Wire, and a shared medium means every attached device sees every frame on it. Neither fact confers anything:

> **Being able to see traffic grants no membership, no delivery right, no forwarding authority, and no transmit authority.**

Concretely, electrical attachment to a Physical Link does not by itself make a device:

```text
a Node on a Wire carried by that Link
a legitimate recipient of PDUs addressed elsewhere
entitled to forward what it observed
entitled to transmit on that Wire
part of a broadcast recipient set
```

Membership comes from configuration — an assigned NodeId on a named Wire, a mapped alias, an installed route — never from wiring or from what happened to arrive. The two ideas separate cleanly by level:

```text
realization level   a PDU may reach devices that are not its destination
semantic level      only the addressed participant is a recipient
```

This is what makes §12.1 acceptable. Flooding is a bandwidth trade, not a widening of authority, and pruning by NodeId later changes efficiency without changing who was ever a participant.

Two consequences are worth naming. First, a device that consumes traffic it merely observed is misbehaving, even though nothing on the bus can stop it; `NodeToOrigin` publications observed by other Nodes (§4.1) are usable only where configuration explicitly permits it. Second, observation without configuration is exactly what promiscuous mode (§27.4) is for, and that mode is deliberately restricted to host tooling and gateways and never creates Wiring.

The same rule applies to broadcast. `NodeId 0` addresses the configured Nodes of one Wire, not every device electrically present on the medium carrying it.

---

# 13. Multicore and Intra-Device Communication

The multicore architecture reuses the same Link model rather than inventing a separate IPC stack.

## 13.1 Inter-core communication as Links

A shared-memory channel is simply another Physical Link:

```text
CPU0 Link Interface
      |
shared-memory queue / FIFO
      |
CPU1 Link Interface
```

Its LLL may be very small because framing/integrity requirements differ from UART/CAN, but it still transports canonical WS PDUs. Device-private Wires provide the logical scope:

```text
Core 0 Domain
    |
device-private Wire 900
    |
shared-memory Link Interface
===============================
shared-memory Link Interface
    |
device-private Wire 900
    |
Core 2 Domain
```

The Router does not need special "core routing" concepts.

## 13.2 Multiple Endpoint Domains

A multicore device can look like:

```text
CPU0 Endpoint Domain --\
                       shared internal Wires / Links
CPU1 Endpoint Domain ---+--> external Links as configured
                        |
CPU2 Endpoint Domain --/
```

Each Domain has its own Dispatcher. Routing tables may be shared across cores and read concurrently.

## 13.3 Inter-core queue serialization

The Router lookup itself can run concurrently, but an inter-core queue may have simpler concurrency constraints.

If the shared-memory primitive is SPSC, several approaches are valid:

- one SPSC queue per producer/domain;
- a local inter-core output task that serializes multiple local producers into one SPSC queue;
- an MPSC queue on platforms where its complexity is justified.

The protocol should not require one implementation. For high-integrity MCU code, one producer task per inter-core link can be attractive because it preserves a very simple shared-memory boundary even if it serializes that specific internal path.

## 13.4 Location independence

A Service relationship can move from:

```text
ECU A -> CAN -> ECU B
```

to:

```text
Core 0 -> shared memory -> Core 1
```

to:

```text
Service A -> local callback -> Service B
```

without changing the application-level message schema or Endpoint identity. The deployment/Wiring changes; the Service protocol need not.

Delivery policy must be preserved across such a move (§9.4).

### Independence is bounded, not absolute

The claim above holds only inside a compatibility envelope, and stating it without the qualifier invites a real failure:

> **A Service is link-independent only within its declared size, timing, and transport compatibility.**

Three things can make an otherwise identical Service relationship unplaceable on a given Link:

```text
size        the PDU does not fit the Link's usable capacity
timing      the Link cannot meet the rate, latency, or cadence the Service needs
transport   the Link or profile cannot provide the required Transport or QoS
```

A Service exchanging 40-byte PDUs cannot move onto a Classical CAN Link limited to N <= 4 (§22.9), no matter how unchanged its schema is. A control loop that works over shared memory may be unplaceable on a polled SPI Link whose cadence sets the floor on latency (§2.8). A Service that needs Critical QoS is not portable to a QoS-Minimal Link without an explicit decision about what happens to its priority.

The machinery to detect all of this already exists — Link capabilities (§17) plus static capacity checking (§20.1) — and the required behavior is unambiguous: an unrepresentable placement **fails at configuration or before TX**, never by truncation or silent degradation (invariant 33).

So the accurate framing is that WireSpaces removes *placement* from the Service's source code, not that every Service runs over every Link. The Service stops naming its Link; it does not stop having requirements.

## 13.5 No special many-core assumptions

The current 127-device-private-Wire direction is expected to be ample for normal multicore MCUs and FPGAs. A hypothetical very-large-many-core processor can justify an extended profile later rather than consuming canonical header bits now.

---

# 14. QoS Model

The canonical descriptor carries a 2-bit QoS value, providing four canonical classes:

```text
Critical
High
Normal
Background
```

Not every implementation must provide four separate schedulers/queues.

## 14.1 Standard implementation profiles

| Profile | Implemented scheduling classes |
|---|---|
| **QoS-Minimal** | Normal only |
| **QoS-Full** | Critical, High, Normal, Background |

Intermediate profiles such as `Normal+Background` are intentionally not standardized unless implementation experience shows a real need.

A Minimal implementation may collapse canonical QoS values to Normal internally, or may reject unsupported QoS according to the eventual Link profile. Exact interoperability behavior remains to be frozen; a Service asking for behavior a Link cannot provide should not be silently misrepresented.

## 14.2 Per-Link queues

A Link Interface normally owns one TX queue per implemented scheduling class.

```text
Critical FIFO ----\
High FIFO ---------+--> configurable scheduler --> Link TX
Normal FIFO -------+
Background FIFO --/
```

Within one queue, FIFO order is the natural baseline.

Scheduling between queues is configurable per Link implementation/instance. Strict priority is valid even though it can starve lower classes; fairness is not a mandatory WS property.

## 14.3 Queue policy is configurable per Link instance

Different Links may need different queue-full behavior. Candidate policies:

- reject new PDU;
- drop oldest queued PDU;
- latest-value coalescing/replacement;
- service/stream-aware shedding;
- custom policy for a special Link instance.

A current default direction:

- **Critical / High:** favor latest-value behavior where the traffic semantics support it;
- **Normal / Background:** reject the new PDU when full, which is friendlier to reliable bulk/file transfer than silently deleting old queued segments.

A richer implementation that knows which Services/streams are producing traffic on a Link may also shed Normal/Background packets in a deterministic round-robin or otherwise fair pattern across producers. This is implementation policy, not a protocol requirement.

**Open detail:** blind replacement of the literal last queue entry can delete an unrelated Service's PDU. A robust latest-value implementation likely needs a stream/logical key such as `(Wire, Direction/Node, Namespace, EndpointId)` or explicit Service support. Resolve this in implementation rather than standardizing it accidentally.

## 14.4 Congestion is a normal send outcome

A full queue is not necessarily a driver failure. Services and Transports must handle send rejection as an expected communication outcome.

API semantics should make the distinction obvious:

```text
kSuccess
kCongestedLink        (E_FULL)
kLinkUnavailable
kUnsupported
kUnknownWire
kUnknownEndpoint
...
```

`kCongestedLink` means approximately:

> The stack and Link are functioning, but this PDU was not admitted because bounded TX resources are currently unavailable.

It must not automatically trigger Link reset or fault recovery intended for broken hardware/drivers.

---

# 15. Congestion, Backpressure, and Flow Control

WireSpaces should keep congestion handling simple unless implementation proves that more is needed.

```text
1. Static traffic engineering
2. Bounded local queues with explicit drop/admission policy
3. Optional hop-by-hop Link backpressure
4. Transport-level windows/credits for reliable bulk transfer
5. Runtime telemetry and diagnostics
```

A generalized TCP-like end-to-end congestion-control protocol is **not currently required**.

## 15.1 Congestion occurs at a Link resource

A Wire itself is not inherently "congested." Multiple Wires may converge on one slow Link:

```text
Wire A --\
Wire B ---\
Wire C ----+--> CAN_A TX queues --> bus capacity
Wire D ---/
```

The constrained resource is the egress Link Interface and its queues/serialization bandwidth. Link drivers/interfaces are therefore the authoritative source for queue occupancy, free TX storage, utilization, drop/reject counts, and link state.

## 15.2 Static prevention is the preferred serious-deployment mechanism

For controlled embedded systems, Services can eventually declare bounds such as:

```text
max frequency
max encoded PDU size
QoS
burst assumptions
```

Wiring/Manifest tooling can project those claims across configured Wires and Links and estimate worst-case Link utilization. The objective is not perfect network calculus in the first version; even conservative summation catches obvious impossible deployments.

> **Engineer normal traffic below capacity; use runtime backpressure and drops for bursts/faults, not as the primary steady-state scheduler.**

## 15.3 Optional hop-by-hop Link backpressure

Some Links benefit strongly from credit-based flow control.

```text
PC / Origin
    |
Ethernet
    |
Gateway
    |
Classical CAN
```

If the gateway's CAN TX resources fill because the PC is injecting faster than CAN can drain, the Ethernet-facing LLL can advertise less receive credit to the PC:

```text
CAN TX queue pressure increases
    -> gateway has less admissible upstream storage
    -> Ethernet credit decreases
    -> PC stops/reduces injection
```

If pressure propagates through several gateways, each upstream Link can reduce its own advertised credit. This creates natural hop-by-hop backpressure without a separate per-Wire congestion protocol.

## 15.4 Flow control is optional and per Link profile

**Strong candidates:** UART, RS-485, Ethernet WS links, USB-like streams, FTDI/FIFO links, shared-memory/FIFO links.

**Classical CAN:**

- ordinary Nodes should not be required to participate in generic Link flow control;
- normal traffic is expected to be mostly static/bounded periodic traffic;
- reliable image/file transfer should use Transport receiver windows/credits;
- a CAN gateway's **TX queue pressure can still influence credit on an upstream Ethernet/UART/etc. Link**.

**CAN FD / CAN XL:** may justify richer Link-level flow control in some profiles due to larger/faster transfers; still optional.

A Link capability advertises whether hop-by-hop flow control is supported (§17).

## 15.5 Credit extension sizes

For Links that use credit flow control, a simple LLL header extension is currently favored:

```text
QoS-Minimal:
    NormalCredit        uint16_t
    total               2 bytes

QoS-Full:
    CriticalCredit      uint16_t
    HighCredit          uint16_t
    NormalCredit        uint16_t
    BackgroundCredit    uint16_t
    total               8 bytes
```

On Ethernet, 8 bytes is negligible. On constrained UART, Minimal's 2-byte form is attractive.

The extension need not appear on every user frame. A profile may piggyback it when credit changes materially or a maximum update interval expires, and send a standalone credit update when no reverse user traffic exists.

**Open detail:** the exact credit-accounting algorithm is not frozen. A cumulative-grant/watermark scheme is attractive because duplicate or lost credit updates do not accidentally grant the same storage twice, but this should be validated in a concrete UART/Ethernet implementation before standardization.

## 15.6 Coarse backpressure is acceptable initially

A shared upstream Link may carry traffic destined for both a congested and an uncongested downstream Link. One coarse credit pool can therefore throttle unrelated traffic. This head-of-line/backpressure coupling is acknowledged.

Do not add per-Wire congestion signaling until a concrete system demonstrates that the coarse model is inadequate. More granular queues/credit domains can be an implementation extension later.

---

# 16. Buffer Ownership and the Copy-Based Baseline

Copy-based operation is the preferred initial implementation strategy, and the architecture should make copying efficient rather than treating it as an inferior temporary mode.

This is especially appropriate for Classical CAN, CAN FD, small MCU datagrams, Service inboxes, early multicore implementations, and most control/status traffic.

## 16.1 Link send contract

A Link TX path should conceptually expose something like:

```cpp
SendResult send(PacketView packet);
```

For a copy-based profile, a successful return means the caller may immediately reuse or destroy the source buffer.

The underlying implementation may copy directly into hardware/controller memory, into a bounded queue, or into driver-owned storage.

This is the same call whose result taxonomy is described in §14.4: success means "admitted and your buffer is free," not "delivered."

## 16.2 Composition for synchronization

A driver itself does not have to be thread-safe. The user can compose a WS TX adapter/multiplexer around it:

```text
Service A ----\
Service B -----+--> TxInputMux --> Link driver send()
CAN RX -------/
```

Possible implementations:

```text
DirectTx
    direct driver call

MutexSerializedTx
    short mutex/critical section around send

QueuedTx
    bounded MPSC queue drained by a Link TX task
```

The names are provisional. The important abstraction is that the Router targets a packet sink/send interface, not a hard-coded queue implementation. This composes with the inter-core serialization choices in §13.3 and the per-class queues in §14.2.

## 16.3 Fanout by copy

For fanout, v1 should simply copy:

```text
one source PDU
    |
    +--> copy for Link A
    +--> copy for Link B
    `--> copy for local tap
```

This is consistent with independent egress results (§12.2): each copy succeeds or fails on its own.

## 16.4 Future zero-copy path

Zero-copy is intentionally **not** an initial focus. A likely later architecture is destination-owned allocation:

```text
Service
    |
createPacketFor(destination)
    |
destination Link pool allocates block
    |
Service fills header + payload
    |
submit
    |
Link TX queue -> driver
    |
buffer returned to destination pool
```

This becomes attractive for Ethernet-sized messages, CAN XL, high-throughput FPGA/softcore gateways, and large bulk transfers. A future advanced feature may permit source-owned immutable buffers with completion tracking across multiple drivers.

That complexity should not be imposed on MCU users unless measurements show it is valuable.

## 16.5 V1 synchronization philosophy

Initial implementations should prefer boring, inspectable concurrency:

```text
mutexes
critical sections
bounded rings
copies
```

Lock-free structures may be added later where profiling justifies them. The expected progression matters more than implementing every optimization in v1.

---

# 17. Link Capabilities

Host configuration and runtime diagnostics need a compact way to understand what each Link Interface can support.

Useful capability fields:

```text
Physical Link / profile type
maximum PDU / MTU
QoS profile (Minimal / Full)
hop-by-hop flow-control support
fragmentation/reassembly support
header-extension support
WireAlias capacity / addressing limits
supported TransportTypes
nominal/usable link rate
Link-specific limits such as CAN MaxN
```

The purpose is practical:

- prevent routing an unrepresentable PDU onto a Link;
- validate auto-Wiring and splices;
- validate static configuration;
- support host inspection;
- make heterogeneous gateways predictable.

This should remain a **capability descriptor**, not a large negotiation protocol.

## 17.1 Gateway discovery reporting

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
per-interface Link capabilities (above)
existing forwarding and splice configuration
observed participants on each interface
```

Reporting **current Wire membership per interface** is what makes the loop rule in §12.4 enforceable: the host can see that an interface already carries Wire 42 and therefore refuse to bridge or splice Wire 42 back into itself. Without it, a host performing recursive discovery (§19.3) cannot distinguish an unconfigured branch from one it has already wired.

Gateways still do not run a distributed routing algorithm. They report local facts; the Organizer decides (§19.1).

---

# 18. Error Semantics and Link Telemetry

WireSpaces should make errors and resource pressure highly observable while allowing tiny implementations to expose only small counters.

## 18.1 Representative error/status categories

```text
TX congestion / queue full
Link unavailable/down
Unsupported PDU/profile/capability
Unknown or unmapped Wire/WireAlias
Invalid internal/external Wire scope
Unknown EndpointId
Invalid NodeId/direction combination
PDUA/reassembly timeout
aggregate integrity failure
Link-native framing / CRC / bus error
queue overuse / high-water condition
duplicate delivery detected
stale/expired data rejected
```

The exact enumeration can be refined, but **congestion must be distinguishable from actual Link malfunction**. WS should not disguise local rejection as a generic transport timeout.

The last two categories exist because this architecture deliberately permits loss, latest-value replacement (§14.3), and non-transactional multi-egress (§12.2). A system that allows those outcomes must be able to *report* the receive-side consequences rather than leaving them invisible; see §26.5.

## 18.2 Queue utilization and overuse are first-class telemetry

A Link that has dropped zero PDUs but repeatedly reaches nearly full queues is already signaling a capacity problem.

Every useful Link implementation should expose at least some of:

```text
queue capacity
current queue occupancy
high-water mark
number of times over a configured pressure threshold
congestion rejects/drops
replacement/coalescing count
```

"Queue overuse" must be reportable, not inferred only after packet loss begins.

## 18.3 Observability ladder

### Tiny embedded target

```text
TX accepted PDUs
TX congestion-dropped/rejected PDUs
RX PDUs
link-native errors
queue high-water / overuse indication
```

No per-Wire table is required.

### Medium embedded target

Add per-Wire accounting:

```text
Wire 12:
    TX bytes/PDUs
    congestion drops

Wire 19:
    TX bytes/PDUs
    congestion drops
```

A periodic telemetry Service can report counts/deltas at approximately 1 Hz.

### Linux / PC / large gateway

A capable gateway may record every congestion-dropped PDU in a circular file/journal. The canonical WS header is intentionally compact enough that a useful record can be roughly:

```text
uint64 timestamp
<= 8-byte WS header/descriptor
-----------------------------
~16 bytes per drop record
```

A file can wrap and overwrite old data indefinitely. If one journal is maintained per Link Interface, Link identity can live in file metadata instead of every record; a combined journal can add local Link index and reason code as needed.

This enables postmortem queries such as:

```text
Which Wires caused CAN_A saturation?
Which EIDs were being dropped?
Which QoS class dominated the burst?
When did the congestion begin and end?
```

## 18.4 Domain-local Link Telemetry Service

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
            | splice (§8.2)
            v
       host-facing Wire -> PC
```

The telemetry Service can remain small on embedded targets while host/Linux implementations expose much richer detail.

## 18.5 Optional per-Wire "top talker" reporting

Larger gateways may report Wires using the most bandwidth, Wires producing the most congestion drops, and per-QoS pressure. This is useful for human diagnosis even if no automatic congestion manager is ever implemented.

A future optional Link Manager Service could consume this telemetry and change admission/shedding policy, but that is **not base protocol behavior**.

---

# 19. Discovery, Commissioning, and Organizer

WireSpaces should support both explicitly configured deployment and very low-configuration lab use.

## 19.1 Organizer

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

## 19.2 Pre-addressing bootstrap

Shared media such as CAN have a chicken-and-egg problem before unique NodeIds exist. Several unaddressed Nodes cannot simply answer the same normal CAN request simultaneously with different payloads; CAN arbitration will not enumerate them and may produce errors.

Therefore:

```text
Phase 1
    Link-specific commissioning/bootstrap
    discover/select an unaddressed participant

Phase 2
    assign a collision-free temporary/permanent NodeId

Phase 3
    ordinary WS management Services
```

The exact collision-avoidance method remains Link-profile-specific and is not frozen.

## 19.3 Recursive discovery through gateways

A host may discover a gateway on an upstream Wire, ask that gateway to enumerate/manage a downstream Link, then repeat. This enables centralized configuration of a physical hierarchy without requiring every node to run distributed routing algorithms.

## 19.4 Dynamic construction of static forwarding

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

## 19.5 Static configuration is optional

A small/simple system may redo discovery/configuration every boot indefinitely. Exporting to a static Manifest is useful for repeatability and serious deployment, but is not a prerequisite to using WireSpaces successfully.

## 19.6 Preferred conventional NodeIds

Two high NodeIds are useful as conventions:

```text
30  DiagnosticAndTestEquipment (preferred)
31  DevelopmentEquipment       (preferred)
```

They are preferences, not immutable identities. Commissioning may assign another free NodeId if the preferred value is occupied.

## 19.7 Stable device identity

A 128-bit UUID is a useful device identity for tooling/history. It is separate from:

```text
WireNumber
NodeId
EndpointId
current topology
```

Tooling can therefore remember that `Wire 107 / Node 2` and later `Wire 18 / Node 9` were the same physical device after a topology change.

## 19.8 Globally stable NodeIds as a convenience

NodeIds are only required to be unique **within** a Wire. Small systems may nevertheless choose to keep them stable **across** Wires, so that:

```text
Wire X / Node 7
Wire Y / Node 7
```

refer to the same physical device. This is purely an allocation convenience. It costs nothing, makes logs and captures much easier for humans to read, and is not a requirement of the model.

It has one concrete architectural benefit. §7.6 notes that splicing several device-private segments onto one shared external Wire requires coordinating NodeIds across the participating devices. A deployment that already keeps NodeIds globally stable satisfies that constraint by construction rather than by validation.

Static NodeId configuration remains available and is preferable wherever stable assignments matter.

## 19.9 Ephemeral configuration must announce itself

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

This makes strong static configuration a natural maturation path — Level 1 to Level 2 in §1.5 — rather than a prerequisite for first use.

---

# 20. Wiring, Manifests, and Static Capacity Analysis

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

This is a deliberate divergence from an earlier generation of this architecture, which held that reachability and authority exist *only* where a Manifest configures them. That absolutism is incompatible with anonymous `kLocalBus` (§6.4) and with Level 0 use (§1.5), both of which exist so that one device and one cable work with no configuration artifact at all. The trade is accepted knowingly: structural validity is still always enforced (§20.4), so an unconfigured system is limited rather than unchecked.

## 20.1 Traffic claims

A Service or generated schema may eventually declare conservative traffic properties:

```text
maximum messages/sec
maximum encoded PDU bytes
QoS
burst behavior
```

Tooling can trace each stream across Wires/Links and accumulate estimated worst-case load per Physical Link:

```text
CAN_A
    MotorCommand       80 kbit/s
    MotorStatus       140 kbit/s
    Diagnostics        25 kbit/s
    other bounded     110 kbit/s
                     -----------
    estimated worst   355 kbit/s
```

The exact analysis method can grow later. Early value comes from catching obvious saturation, not proving every timing theorem.

## 20.2 Longer-term static visibility

The same information may eventually support system-wide communication visibility, configuration validation, bandwidth analysis, loop detection, compatibility checks, generated topology diagrams, and more advanced static analysis. These are intentionally not prerequisites for the initial protocol.

## 20.3 Relationship to future Flow/WireContract work

Flow declarations, WireContracts, authority analysis, schedulability analysis, and redundancy analysis remain plausible higher-level features. They should be built **after** the basic network, Link profiles, routing, discovery, and tooling are proven useful.

There is no implicit WireContract in the base architecture.

## 20.4 Structural validity versus contract

A useful line separates checks the implementation must always perform from policy that a future WireContract might add.

**Structural validity** — required for the representation itself to function, and therefore always enforced:

```text
exactly one Origin role per Wire
valid NodeId values for the Direction
no duplicate NodeId where uniqueness is required
valid Wire representation for the selected Link profile
representable Endpoint identity for the selected Link profile
valid forwarding and splice configuration
```

These are structural protocol/configuration constraints. They are not a contract, they are not optional, and they are the same set enumerated as invariants in §36.

**Contract-level policy** — optional, additive, and never implied:

```text
which Services/Endpoints may transmit
which participants may consume or invoke them
rate limits
message-size limits
QoS expectations
version compatibility
resource bounds
redundancy requirements
runtime enforcement rules
```

A beginner must be able to put devices on a Wire, exchange Service messages, discover devices, and build a small system without declaring anything in the second list. Conversely, no implementation may skip the first list on the grounds that no contract was supplied.

## 20.5 Higher-level composition must flatten

Patterns above the bare Wire will eventually be wanted — request/response correlation, redundancy across member Wires, closed-loop feedback pairs, freshness monitoring, reliable bulk transfer. The governing rule for all of them is:

> **A composition pattern must reduce, at generation or configuration time, to ordinary Endpoints, Wires, bindings, and bounded local state.** The Router and the LLL must never need to understand the pattern.

So a request/response pair flattens into two directed exchanges on existing Wires plus correlation state held by the requesting Service. A redundancy group flattens into several ordinary member Wires plus the coordination state described in §30.2. In both cases the data plane sees nothing new: the same canonical PDUs, the same forwarding table, the same dispatch.

What this buys is specific. It keeps the forwarding path from accumulating cases, so a gateway written today still forwards correctly for patterns invented later, and it keeps a tiny Node from paying for abstractions it never uses. It is the mechanism behind invariant 4 — the complete PDU stays the generic forwarding unit — extended upward instead of downward.

Two boundaries follow:

- If a proposed pattern *cannot* flatten, and genuinely requires the Router or LLL to learn a new concept, that is the signal to re-read §0's design rule and look for a formulation that composes instead. It is not automatically forbidden, but it is a much larger change than it looks.
- Flattening is a property of configuration, not of runtime. Nothing here implies dynamic composition, pattern discovery, or a runtime component graph.

A catalog of such components — and the naming for them — is explicitly **not** established here (§0.2). Earlier drafts sketched names like `Harness`, `Bus`, `Channel`, `RequestResponse`, and `FreshnessMonitor`; those were illustrative of possible behavior and were never a frozen set of classes or APIs. Only the flattening constraint is adopted now, precisely because it is the part that protects the current architecture from the catalog.

---

# 21. Endpoint Namespaces and Address Allocation

## 21.1 Canonical space

```text
Namespace       2 bits
EndpointId     16 bits
```

gives four independent 16-bit Endpoint spaces and, with `EndpointId == 0` reserved in each, 262,140 usable Namespace/EID combinations. This is intentionally much larger than the directly representable Classical-CAN subset.

## 21.2 Namespace allocation

Current working plan:

```text
Namespace 0
    default/user Namespace
    low compact region partly reserved for Core/Common services

Namespace 1
    user-defined

Namespace 2
    user-defined

Namespace 3
    public WireSpaces / FOSS ecosystem
```

Namespaces 0-2 provide deployments with large independent user spaces and room for staged upgrades, migrations, or coexistence between generations. Namespace 3 treats the open-source ecosystem as a first-class audience rather than an afterthought.

## 21.3 Namespace 0 compact CAN region

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

## 21.4 Namespace 3 ecosystem registry

Namespace 3 is intended for reusable, interoperable FOSS Services.

On rich Links, `NS3 EID 1..65535` is available. On the current General Classical-CAN PDUA profile, `NS3 EID 1..1023` is directly representable.

A plausible registry policy:

- mature/stable public Services allocate upward from low IDs;
- experimental/beta allocations may come from a high region;
- stable published IDs are not silently reused for unrelated semantics;
- retired stable IDs may remain tombstoned;
- not every reusable library needs an immediately permanent public ID.

The large canonical namespace is intentional. Identifier scarcity should exist only where the carrier is genuinely constrained, not because an early registry chose unnecessarily tiny ranges.

**Open:** the registry *process* — who allocates, how experimental ranges are reclaimed, and how tombstones are published. The allocation model above is the working plan.

## 21.5 Service allocation hierarchy

```text
NS0 EID 1..31
    exceptionally valuable compact Common Services

NS3 EID 1..1023
    broad FOSS ecosystem Services that must work on General Classical CAN

NS3 EID 1024..65535
    richer-link ecosystem Services
```

Not every Service needs to be in the compact Namespace-0 Common region. This avoids wasting the scarce N=1 encoding while leaving the future community ample permanent address space.

---

# 22. Classical CAN Profile — Current Snapshot

Classical CAN remains an important constrained compatibility profile and stress test, but it does not define WireSpaces as a whole.

The goal is not to force every Service to fit one CAN frame. The goal is to make small WS messages genuinely useful on standard 11-bit CAN and to provide a lightweight bounded adaptation when they do not.

## 22.1 11-bit CAN identifier direction

```text
QoS        2
Direction  1
WireAlias  3
NodeId     5
----------------
           11
```

This gives 4 QoS values, 2 directions, 8 WireAlias codes, and 31 Nodes plus broadcast semantics.

Exact physical bit ordering is a profile-specification detail. The important semantics:

```text
WireAlias 0      kLocalBus
WireAlias 1..7   configured named-Wire aliases

NodeId 0         broadcast in OriginToNode; invalid in NodeToOrigin
NodeId 1..31     individual Node
```

**Open, carried in from the older adapter draft for review:** because CAN arbitration is lower-ID-wins, the canonical-QoS-to-arbitration mapping must be inverted, i.e. `Critical` encodes to the numerically lowest code and `Background` to the highest, and the QoS bits should occupy the most arbitration-significant positions. That mapping and the placement of Direction/WireAlias/NodeId need to be restated for the current 3-bit-alias/5-bit-NodeId layout before freeze.

## 22.2 CAN Endpoint representation

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

## 22.3 Optimized N=1 encoding

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

An **N=1-only node is a valid and useful WireSpaces device**. It can still support selected Common Services, up to 96 compact user EIDs in the current NS0 allocation, commands/status, identity, health, small telemetry, and simple device control. It does not need General PDUA to be considered a real WS node.

## 22.4 General PDUA frame layout

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
Namespace               2
TransportType           3
HasHeaderExtensions     1
EndpointId[9:8]         2
--------------------------
                        8
```

`EndpointId[9:8]` and byte 2 form one direct 10-bit value.

Encoding selection on TX is mandatory rather than free:

```text
if optimized eligibility is satisfied:
    use optimized N=1
else if General PDUA can represent the complete canonical PDU within bounds:
    use General PDUA, including N=1 when it fits
else:
    reject placement or local TX before emitting a frame
```

## 22.5 PDUA maximum and normal range

```text
Maximum PDUA aggregation depth: N = 8 CAN frames
Normal design target:           N <= 4
```

Services intended for broad small-node compatibility should usually fit within N <= 4. N = 5..8 exists for cases such as bootloader/bulk-ish messages that materially benefit from a larger atomic PDU.

Anything routinely needing more than eight Classical CAN frames should be segmented at the Transport/Service layer rather than making PDUA itself larger.

## 22.6 FrameControl byte

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

## 22.7 Reassembly safety model

MessageGeneration is not a globally unique PDU identifier. After wrap, a continuation from a later PDU can theoretically alias an old incomplete reassembly if enough intervening traffic is completely lost.

The design intentionally relies on several independent checks:

- explicit START resets/creates reassembly state;
- FramesRemaining must match the expected sequence;
- continuation generation must match the active generation;
- constituent frames for one PDU are transmitted in order and non-interleaved for that CAN ID;
- reassembly state has a bounded timeout/lifetime;
- an aggregate PDU CRC validates the completed reconstruction.

This is sufficient for the intended small CAN PDUA without spending more header bits on a larger sequence number.

Delivery is all-or-nothing: no Endpoint, Service, queue, or application-visible state may observe a partial PDU. Every constituent frame of one PDU carries the same complete CAN arbitration ID, and exactly one physical transmitter owns each CAN ID in every configured state.

## 22.8 Aggregate CRC policy

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
**CRC-16 algorithm:** not yet frozen; the profile must choose and specify the full parameter set and golden vectors before interoperability freeze.

## 22.9 Corrected General PDUA capacity

Gross bytes available to the PDU stream, before any aggregate CRC, are:

```text
B(N) = 5 + 7 * (N - 1) = 7N - 2
```

which follows from 8 CAN data bytes per frame, minus one FrameControl byte per frame, minus the START frame's PduControl and EndpointId bytes.

Applying the §22.8 CRC policy gives the **net** PDU capacity:

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

> **Note:** the earlier overview published 5 / 12 / 19 / 26 as the per-N budget. Those are the *gross* `B(N)` values and overstate usable capacity for N >= 2 under the current CRC policy. Use the Net column.

**Open:** CRC placement (trailing bytes of the final frame is the assumption above), exact DLC/short-frame/padding rules, and whether length is derived from DLC or carried explicitly.

## 22.10 CAN and Link-level flow control

Generic CAN Nodes are not expected to participate in WS Link credit flow control.

A gateway can still use CAN queue pressure to reduce upstream Ethernet/UART/other-Link credit, so slow CAN egress can cause backpressure without changing the CAN wire protocol.

Reliable file/image transfer over CAN should use Transport-level receiver control rather than generic CAN-node LLL credits.

## 22.11 Future 29-bit Classical CAN profile

The 11-bit identifier is a deliberately constrained **compatibility floor**, not the ceiling of Classical CAN support.

A future 29-bit Classical CAN profile is the planned escape hatch. Eighteen additional identifier bits are ample to carry a full 10-bit `WireNumber` directly, and possibly more Endpoint bits, which would retire:

- the seven-named-alias-per-Link limit (§6.1);
- the need to spend scarce aliases on transit Wires;
- some of the pressure behind the compact NS0 EID region (§21.3).

Wire, Direction, NodeId, Endpoint, and Transport semantics would be unchanged; only the projection into the identifier differs. The 11-bit profile remains valuable for coexistence with legacy 11-bit traffic and for the smallest nodes.

**No 29-bit layout is specified here**, and none should be inferred from the 11-bit field order.

---

# 23. CAN FD, CAN XL, USB, Ethernet, I2C, and SPI

WireSpaces does not treat Classical CAN as the permanent center of the architecture. It is a useful lower bound.

**CAN FD** can often carry a complete small WS PDU in one frame and greatly reduces the need for PDUA fragmentation. The same canonical PDU and Service definitions apply.

**CAN XL** is especially attractive for large datagrams and high-throughput embedded gateways. Larger frames make destination-owned/zero-copy schemes more attractive, but Routing and Wire semantics are unchanged.

**USB** can start with CDC/serial framing. A future native USB bulk or FTDI synchronous FIFO profile is especially attractive for FPGA/host development (§27.2).

**Ethernet** has two broad approaches:

```text
WS directly over Ethernet     dedicated Layer-2 embedded network
WS over UDP/IP                existing routed infrastructure
```

WS should not fight IP where IP provides valuable reachability, VPNs, routed networks, Wi-Fi, security infrastructure, or general interoperability. At the same time, a dedicated FPGA/measurement device that only needs to talk to one or two known PCs may benefit from a much simpler WS-over-Ethernet hardware path.

**I2C** and **SPI** are architecturally in scope as ordinary Physical Links, and are the main reason §2.8 exists. Both are master-initiated, so the master's LLL drives the cadence at which a Node's `NodeToOrigin` traffic can appear, and both need a Link profile that says how a PDU is delimited inside a transaction and how an idle Node reports "nothing to send". SPI additionally has no native addressing or integrity, so a profile must supply framing and a CRC. These are attractive for board-local companion devices and sensor subsystems that would otherwise need a dedicated bus — but note §5.5: reaching a chip over SPI does not make it local.

**Open:** whether CAN FD/XL share the same PDUA concepts or receive simpler native-PDU profiles; exact I2C and SPI transaction formats and polling-cadence configuration.

---

# 24. Byte-Stream / UART Link Direction

The UART/byte-stream profile is not frozen. Current direction:

```text
WS LLL frame
    -> CRC
    -> byte-stream framing
    -> UART / RS-485 / USB-VCP / similar stream
```

## 24.1 COBS vs HDLC-style escaping

COBS is currently an attractive candidate because it provides tightly bounded framing expansion, which simplifies maximum encoded-frame sizing, fixed buffer allocation, deterministic MCU resource analysis, and RTL implementation.

HDLC-style escaping remains viable and mature, but its worst-case byte-stuffing expansion is less attractive for bounded-resource design.

**This is not yet an agreed/frozen profile choice.** An implementation prototype should compare COBS complexity, resynchronization behavior, DMA friendliness, and encoded-buffer requirements before standardization.

## 24.2 UART CRC

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

# 25. Transport Layer Responsibilities

The canonical descriptor reserves a 3-bit `TransportType`, but the exact registry is not frozen.

The baseline Transport is an **Unreliable Datagram** style:

- bounded PDU;
- no mandatory retransmission;
- delivery failure is possible;
- application/Service chooses semantics.

Other Transports may provide reliable segmented transfer, receiver windows/credits, request/response retry behavior, sequence/E2E integrity, or specialized command-source selection.

## 25.1 Bulk transfer

Firmware images, files, logs, and similar data should generally use a Transport that can segment across multiple PDUs, retry missing data, bound receiver storage, and resume where useful.

This is preferable to growing a constrained LLL such as Classical CAN PDUA into a large transport protocol.

## 25.2 Origin failover is not a base Wire feature

If the Origin of a Wire goes down, that Wire becomes only marginally useful for many command/control functions. Node publications may still exist, but Origin-driven behavior is absent.

Base WS does not define automatic Origin election or failover. Redundancy can be built by composition:

```text
Origin A ---- Wire A ---- Nodes
Origin B ---- Wire B ---- Nodes
```

Application or Transport policy decides when commands from one source are accepted instead of another.

A future Transport could explicitly support command reception from multiple Origins and switch source based on health/timeouts, but that is higher-level policy rather than Wire routing.

## 25.3 Transport is chosen by Service semantics

Transport selection is not an infrastructure detail to be defaulted upward. It belongs to the Service, because only the Service knows what its data means when it is late, duplicated, or missing. The most important consequence is counterintuitive enough to state as a rule:

> **Reliability is not assumed to be safer than loss with freshness detection.**

A reliable Transport converts loss into delay. For a control loop, that trade is often the wrong one:

```text
unreliable + freshness    a missing command is detected as missing;
                          the receiver enters a defined degraded state

reliable                  a missing command is retransmitted and
                          applied late, against a world that moved
```

A setpoint that arrives 200 ms after it was produced, having been faithfully retransmitted, can be more dangerous than one that never arrived at all — because the receiver noticed the second case. Retransmission also consumes the bandwidth and queue depth that the *next* update needed, so a congested reliable stream degrades in the direction of stale data rather than absent data.

This is why the base Transport is an Unreliable Datagram and why congestion is a normal send outcome (§26.1) rather than something the stack hides. For periodic state, control commands, and telemetry, the usual right answer is an unreliable Transport, latest-value replacement (§14.3), and receiver-side freshness handling (§26.5).

Reliability earns its place where the data is not periodic and every byte matters:

```text
prefer unreliable + freshness   periodic state, setpoints, telemetry,
                                heartbeats, sampled measurements

prefer reliable                 firmware images, files, logs, configuration
                                writes, request/response with side effects
```

Two related cautions. Choosing a reliable Transport does not remove the need for freshness handling; it changes staleness from a delivery gap into a latency distribution, which is harder to observe. And ordering guarantees are a separate axis from delivery guarantees — a Service that needs to reject out-of-order updates should say so rather than assume a reliable Transport implies it.

---

# 26. Services and Application Expectations

A **Service** is reusable functionality exposed through one or more network-visible Endpoints and local implementation interfaces. The Service model should be agnostic to whether the implementation is C++ firmware, host software, softcore software, or RTL.

## 26.1 Send failure handling

Services must not assume `send()` always succeeds. In particular `kCongestedLink` / `E_FULL` is an expected runtime outcome.

The Service/Transport chooses whether to retry later, drop a latest-only update, coalesce state, propagate an application error, or pause a reliable transfer. It should not interpret ordinary queue congestion as proof that the driver is corrupt and must be reset.

## 26.2 Service traffic declarations

For future static capacity checking, Services should ideally expose conservative traffic claims: maximum TX rate, maximum PDU size, QoS, and an optional burst bound. The exact schema belongs to future tooling/Manifest work.

## 26.3 Standard one-device Services

A compelling base ecosystem should make one PC-connected device useful immediately. Likely standard/common Services:

- stable device identity;
- software/build/version information;
- heartbeat / uptime / reset reason;
- text logs;
- structured events;
- Link health/telemetry;
- firmware update / object transfer;
- application-specific telemetry and control.

A device should not need complicated network configuration merely to expose these over one Link.

## 26.4 Broader ecosystem candidates

```text
Identity / device information
Build / version information
Heartbeat / health
Text logs
Structured system events
Link status / traffic counters
Telemetry / scalar snapshots
Crash / fault records
Firmware update
Bulk data transfer
Discovery / enumeration
Lifecycle / reset control
Persistent configuration
Time synchronization
RPC-style utilities
```

Service schemas should eventually have portable, deterministic definitions and useful code generation, but the schema language/toolchain is not frozen.

## 26.5 Freshness, staleness, and duplicates

A Service definition should cover more than message layout. Alongside meaning and encoding, it should state:

```text
timing        expected production rate or cadence
freshness     how long a value remains usable
authority     who may produce it
failure       what a receiver does when it is absent or stale
```

Freshness is the receive-side counterpart to the choices this architecture makes elsewhere. Because loss is permitted (§25), because a queue may replace a pending latest-value update (§14.3), and because a polled Link delivers data up to one cadence late (§2.8), a value can legitimately be older than the receiver assumes. The architecture therefore has to make that visible instead of leaving each Service to notice on its own:

> **A Service that acts on received state should be able to tell how old that state is, and must define what it does when the answer is "too old".**

The practical minimum is unglamorous and cheap: a receiver that expects periodic data times out on absence and enters a defined degraded state, rather than continuing to act on the last value indefinitely. That single behavior is what makes an unreliable Transport safe (§25.3), and it is worth more than most delivery guarantees.

Duplicates need the same treatment. Non-transactional multi-egress (§12.2), retried request/response, and future redundancy compositions (§30.2) can all present the same logical update twice. A Service that is not idempotent must say how duplicates are recognized and suppressed, and that suppression belongs to the Service or the composition — not to the Router, which deliberately has no duplicate-suppression state (§12.4).

Both outcomes are observable error categories (§18.1), so a system can report stale-data rejections and detected duplicates as counters rather than discovering them by behavior.

What remains open is representation: whether freshness is expressed as a Service-declared interval checked locally against a monotonic clock, or needs a canonical timestamp or sequence field carried in a header extension, and how that interacts with time synchronization (§26.4). Nothing here requires a new canonical field yet, and none should be added until a Service demonstrates that local timing cannot answer the question.

---

# 27. Host Tooling and Maintenance-Port Model

Host tooling is a first-class part of WireSpaces, not demo scaffolding.

```text
PC
 |
USB / UART / Ethernet / FTDI FIFO
 |
Device
```

Illustrative CLI shape:

```text
ws devices
ws info <device>
ws logs <device>
ws events <device>
ws monitor <device>
ws update <device> image.bin
ws links <device>
ws services <device>
ws capture
```

Exact command names are illustrative. Other useful functions include Service discovery, Wire topology view, packet capture, routing/alias inspection, and configuration generation.

One long-term strength of WS is that the same host tool can understand one dev board over USB, a multicore MCU, several CAN buses, a gateway, FPGA RTL Endpoints, and remote links. The front-door Link changes while the Service model stays familiar.

## 27.1 One maintenance port for a whole machine

```text
Engineer PC
    |
USB / Ethernet / other maintenance Link
    |
Main SoC / FPGA gateway
    +-- CAN_A -> Nodes
    +-- CAN_B -> Nodes
    +-- RS-485 -> Nodes
    +-- internal FPGA/CPU domains
```

The same host tooling can inspect/update downstream devices through one connector.

## 27.2 FTDI FIFO

An FTDI FIFO is a particularly attractive FPGA/host integration because it provides a fast, simple PC pipe without requiring a CPU or full network stack in the FPGA.

```text
PC WS tools
    |
USB
    |
FTDI FIFO
    |
FPGA LLL / Router
    +-- RTL Endpoints
    +-- CAN gateways
    +-- other Links
```

This is a strong early demonstration of WS running end-to-end with pure RTL in the device data path.

## 27.3 Remote maintenance

The same Service model can work across Ethernet, VPN, radio, or other remote Links. Remote maintenance should emphasize retained logs/events, identity/version, health, configuration, resumable firmware/object transfer, and selected telemetry.

High-rate internal Wires should not automatically be mirrored over narrow radio/remote Links, and a remote link should not splice every internal Wire.

## 27.4 Promiscuous and bring-up operation

Auto-Wiring (§19) deliberately *creates* configuration. A separate and complementary capability is deliberately *ignoring* it.

In promiscuous/bring-up mode, an implementation may:

- observe PDUs regardless of whether local configuration recognizes them;
- log unknown WireNumbers, WireAliases, NodeIds, Endpoints, and Services;
- interrogate observed nodes;
- perform explicitly privileged exploratory transmissions.

Two boundaries are firm:

> **Promiscuous observation never creates persistent Wiring.** It only reports. Auto-Wiring is the separate mechanism that deliberately installs temporary configuration.

> **Promiscuous mode is a host-tooling and gateway capability.** Ordinary embedded Nodes remain strictly configured: the Dispatcher still rejects and counts unknown EIDs (§9.1), and a Node does not consume traffic it was not wired to receive.

The capability must be optional and removable — compiled out or disabled for constrained targets and production builds — and it is privileged in the same sense as configuration operations (§29).

This pairs naturally with the counters and drop journals in §18: a gateway that can already count what it rejects is most of the way to reporting what it observed.

**Open:** how far promiscuous capability may extend on a gateway that is simultaneously carrying production traffic, and the exact build-time removal rules.

---

# 28. FPGA / RTL Architecture

WireSpaces is intentionally designed so software and RTL can participate in the same communication model.

```text
CAN FD LLL ----\
CAN XL LLL -----+--> canonical PDU / Router --> Ethernet / FTDI LLL
UART LLL -------+
RTL Endpoint ---+
```

## 28.1 Hardware routing primitive

The central routing operation is hardware-friendly:

```text
WireNumber lookup
    -> ingress Link Interface index
    -> egress bitmask
    -> optional spliceWire
```

A generated table can live in registers/BRAM and produce an egress mask in a small number of cycles.

## 28.2 RTL Endpoint model

A pure RTL Endpoint can consume WS PDUs, produce telemetry/events, implement control/status registers, and participate in standard Services where the schema/state machine is suitable.

The host should not need to know whether an Endpoint is implemented in C++ or SystemVerilog.

## 28.3 Physical placement is not protocol topology

Routing a message across a large FPGA does not require a new Wire merely because the physical interconnect is longer. A logical Wire may be realized through multiple pipeline stages, local switches/crossbars, clock-domain crossings, wide internal FIFOs, and distributed registers.

The implementation can make the Link physically sophisticated while the Wire remains one logical bus.

## 28.4 High-throughput gateway/analyzer concept

A compelling eventual demonstration could be an FPGA/softcore embedded router fabric with many CAN FD/XL interfaces, one or more Ethernet links, high-rate WS forwarding, host tapping/packet capture, logic-analyzer capture on selected buses, bus utilization/timing diagnostics, physical-layer diagnostics on selected channels, and an FTDI FIFO or Ethernet connection to WS PC tools.

The device itself could use WS internally between RTL blocks, softcore Services, field buses, and host tools.

## 28.5 WireSpaces and IP

WireSpaces is not inherently "faster than IP." Purpose-built IP switching hardware can operate at enormous throughput. The advantage WS may offer in hardware is **simplicity for closed embedded systems**.

A WS hardware datapath may be approximately:

```text
parse small WS header -> WireNumber table lookup -> egress mask -> stream payload
```

A general IP endpoint/router may need MAC addressing, ARP/ND, IPv4/IPv6 parsing, TTL, checksums, subnet/prefix routing, UDP/TCP demultiplexing, ports, and socket conventions. A fixed-purpose FPGA can simplify IP substantially, but WS is designed from the start around known embedded relationships.

Where generic routed infrastructure matters, WS can simply use UDP/IP as its carrier.

---

# 29. Security Scope

WireSpaces does not currently define a complete general security architecture. This is deliberate.

Configuration operations must be treated as trusted/privileged:

- assigning NodeIds;
- naming Wires;
- changing alias maps;
- changing forwarding tables;
- installing or changing splices;
- privileged device management.

Development Auto-Wiring is appropriate for physically trusted benches/labs unless a product supplies authentication/authorization.

Remote or hostile Links should use an appropriate secure boundary: VPN/secure tunnel, an authenticated Link profile, or a product-specific secure gateway. A remote maintenance link should not blindly expose every internal Wire.

The debug-Wire/splice model is useful partly because the scope boundary where private traffic becomes externally visible is explicit.

Native CAN CRC and the aggregate PDU CRC detect accidental corruption only. They provide no authentication, authorization, confidentiality, freshness, or replay protection.

Do not invent new cryptography merely to complete the base architecture.

## 29.1 Privileged capabilities require a separate build

Because the base architecture provides no authentication, the only enforcement mechanism actually available to it is what was compiled in. That yields one cheap and strong rule:

> **A privileged capability must not be reachable in a normal build. Runtime configuration alone must never be able to enable one.**

Capabilities that should be absent from ordinary builds, not merely disabled:

```text
shell / command execution
memory and MMIO read/write
register peek/poke and calibration
raw frame injection on a Link
replay of captured traffic
firmware update in unconstrained form
fault injection and forced-fault control
promiscuous observation (§27.4)
```

Each needs an explicit development or engineering build to exist at all. The distinction matters because a configuration-gated capability is one bad forwarding table, one mis-scoped splice, or one compromised host away from being reachable — whereas a capability that was never compiled cannot be reached by any sequence of messages.

Related trust boundaries are outside WireSpaces but should be treated as privileged by any product using it: SWD/JTAG, bootloader entry, persistent configuration writes, and anything that can change what runs on the device.

For initial implementations and examples this means: favor receive-only observation and bounded diagnostics, keep remote and IP-facing exposure out of the recommended default feature set, and require explicit configuration for every route, splice, and observation path rather than providing a broad default. §27.4 already applies this rule to promiscuous mode; this section is the general form of it.

---

# 30. Reliability, Restart, and Redundancy Direction

WireSpaces should favor recoverable, bounded components, but detailed restart policy is component-specific.

Likely recoverable units: LLL instances, Link Interfaces/drivers, Services, and Endpoint Domains in suitable systems. The base Router can be close to static state and may have little dynamic state to restart.

The broader project direction also favors Link status counters, health reporting, bounded logging, and non-recursive diagnostic failure paths.

## 30.1 Link/status observability after failure

Because Link Telemetry can be reported over other healthy Links or internal debug paths, a device with multiple Links can often expose failure data even when one Link has failed.

## 30.2 Redundancy by composition

The base Wire is not multipath and has exactly one Origin. Redundancy should initially be composed from multiple Wires rather than by giving one Wire hidden failover semantics. This keeps authority and failure behavior explicit.

Composition is not free, though — it relocates work rather than eliminating it. Anything that coordinates redundant Wires must own, explicitly:

```text
message-instance correlation   recognizing that two arrivals are one update
duplicate suppression          which copy is acted on, and what happens to the rest
stale-copy rejection           discarding a late copy that lost to a newer one
failover / active-standby      when a path is considered failed, and what switches
per-Wire health                each member's own liveness, independently observed
per-sink coverage              which sinks are actually covered by which members
```

That list is the price of admission. A design that claims redundancy without answering all six has usually just built duplicate traffic. Note also that none of it lives in the Router: the forwarding path has no duplicate-suppression state (§12.4), so this coordination is Service- or composition-level and must flatten into ordinary Wires and bounded local state per §20.5.

Two distinctions keep the model honest:

- **Path redundancy versus replicated sources.** Ordinary redundant member Wires carry the *same* producer's data over different paths, and normally share one semantic producer identity even though they are distinct configured Wires with separate health. Voting or arbitration across *genuinely independent* producers is a different problem with different failure modes, and should not be described in the same terms.
- **Observation is not coverage.** A promiscuous or diagnostic observer (§27.4) that happens to see a Wire's traffic provides no redundancy coverage, is not a failover path, and never participates in duplicate suppression. Only configured members count.

Cyclic and redundant forwarding profiles at the routing level remain deferred (§0.2); the acyclic realization rule (§12.4) still holds, and redundancy composed above Wires does not violate it because each member Wire is independently acyclic.

---

# 31. Implementation Language and API Direction

The first implementation should favor **C++** to obtain working results quickly. There is no current need to maintain parallel C and C++ cores from day one.

A good hedge is to keep important core data shapes and functions reasonably C-compatible where practical:

```cpp
struct WsPdu;
struct WsRouter;
enum class WsSendResult : uint8_t;

WsSendResult wsRoute(...);
```

while still using C++ internally for RAII, templates where they materially help static sizing, `constexpr` configuration, stronger types, compile-time validation, and host-side convenience.

A separate C implementation or C ABI can be added later when an actual target requires it.

The embedded baseline should continue to avoid mandatory RTTI, exceptions, and heap allocation, consistent with the project's general C++ rules.

---

# 32. Conformance and Test Strategy

Because WS is intended to span C++, host implementations, Python tooling, and RTL, **conformance vectors should be created early**.

Useful vectors:

- canonical PDU descriptor encode/decode examples;
- WireAlias canonicalization examples;
- splice application on ingress and egress, including rejection of anonymous LocalBus splices and device-private egress without a splice;
- Router table examples with expected egress masks;
- LocalBus configured/unconfigured behavior;
- Classical CAN PDUA fragmentation/reassembly sequences;
- generation-wrap/stale-fragment tests;
- CRC golden vectors for both CRC-8 and CRC-16 cases;
- per-N capacity boundaries from §22.9, including oversize rejection before TX;
- Endpoint dispatch examples;
- congestion/send-result behavior;
- Link telemetry snapshot examples.

This helps prevent software and RTL implementations from quietly becoming different dialects.

## 32.1 No interoperability is claimed

Until the Link profiles are frozen and the vectors above exist, the position must be stated negatively as well as positively:

> **This architecture claims no wire interoperability.** Independently developed implementations must not be presumed compatible merely because both follow this document.

Everything a second implementation would need in order to match the first is still open: byte-exact Link encodings, CRC parameters, CAN identifier bit ordering, framing choices, and the registries for TransportType and Namespace 3. Two teams reading this document carefully will produce incompatible bytes, and that is expected at this stage rather than a defect.

The practical rule for now is that a deployment is interoperable only across implementations that share generated definitions or have been tested against the same vectors. Claiming more than that — in a datasheet, an interface document, or a project plan — is the failure mode this section exists to prevent, because it is discovered late and expensively.

Host simulation should be a first-class development path. Local UDP, virtual CAN, PTYs, shared-memory channels, and simulated Links can exercise most of the architecture before hardware is available.

---

# 33. Implementation Scaling Profiles

The same conceptual architecture should scale through substantially different implementations.

| Target | Likely implementation |
|---|---|
| Tiny bare-metal MCU | copies, one Link, switch/linear EID dispatch, LocalBusOnly, no locks |
| Normal single-core MCU | fixed tables, copy queues, mutex/critical sections, several Services |
| Multicore MCU | internal shared-memory Links, Service inboxes, concurrent routing/dispatch |
| Embedded gateway | many Link tasks, read-mostly tables, direct forwarding, optional seqlocks |
| Host PC | conventional threads/queues/maps; optimize only if needed |
| FPGA softcore | generated tables, DMA/FIFOs, potentially zero-copy |
| Pure RTL | BRAM routing tables, Link Engines, RTL Endpoints, streaming datapath |

The protocol does not require every row to implement the mechanisms used by every other row.

## 33.1 Small MCU profile

A tiny implementation may have:

```text
one Physical Link
one Endpoint Domain
few Services
copies everywhere
no dynamic allocation
no Router task
no network task
no locks
```

Receive: frame RX -> decode PDU -> switch/linear lookup on EID -> callback.  
Transmit: Service -> construct small packet -> `driver.send()`.

A constrained CAN node may support only `WireAlias = kLocalBus`, optimized N=1, Namespace 0, and EID < 128, and still interoperate meaningfully with richer WS hosts and gateways.

The implementation should not be forced to instantiate abstract runtime objects that exist only to model features it cannot use.

## 33.2 "Microkernel-like" execution shape

WireSpaces is not an operating system, but a capable implementation has a similar execution shape:

```text
Active entities:      Service tasks, Link-driver tasks
Passive infrastructure: routing tables, dispatch tables, static bindings, bounded queues
```

Data moves directly from producer context toward its configured destination. This makes the cost of communication visible: queues exist where a scheduling or ownership boundary actually requires them, not because the framework mandates a central broker.

---

# 34. Provisional Capacity / Fact Sheet

| Characteristic | Current direction |
|---|---|
| Canonical message type | bounded datagram |
| Canonical base descriptor | 40 bits / 5 bytes |
| Canonical header incl. ordinary extensions | ~8 bytes target |
| Namespace | 2 bits / 4 spaces |
| EndpointId | 16 bits per Namespace; 0 invalid |
| FOSS ecosystem Namespace | Namespace 3 |
| TransportType | 3 bits / up to 8 values |
| QoS | 2 bits / 4 classes (Critical, High, Normal, Background) |
| QoS profiles | QoS-Minimal (Normal only), QoS-Full (all four) |
| WireNumber | 10 bits |
| Device-private Wires | 896..1022 provisional (127 values) |
| `kLocalDomain` | 1023 provisional; never enters a Link Interface |
| Direction | OriginToNode / NodeToOrigin |
| NodeId | 5 bits; 0 = broadcast (OtN) / invalid (NtO) |
| Nodes per Wire | up to 31 Nodes + one Origin |
| Conventional NodeIds | 30 diagnostic/test, 31 development |
| Classical CAN identifier | QoS 2 + Direction 1 + WireAlias 3 + NodeId 5 |
| CAN WireAlias | 0 = `kLocalBus`, 1..7 named aliases per Link |
| CAN optimized N=1 | NS0, EID 1..127, 0..7 payload bytes, no aggregate CRC |
| CAN General PDUA | Namespace 0..3, EID 1..1023 |
| CAN PDUA max frames | 8 (N <= 4 normal target) |
| CAN aggregate CRC | none / CRC-8 / CRC-16 by N (§22.8) |
| CAN net PDU bytes | 5 / 11 / 18 / 25 / 31 / 38 / 45 / 52 for N = 1..8 |
| Generic gateway unit | complete canonical WS PDU |
| Gateway baseline | flood-and-filter within a Wire's realization |
| Wire representation change | only via explicit splice, applied before egress |
| Egress representation | Link Interface bitmask |
| Runtime routing model | caller-context routing over read-mostly tables |
| Buffer ownership | copy-based first-class; zero-copy deferred |
| Delivery policies | `Inline`, `Serialized` |
| Static configuration | recommended medium-term; not required |
| Runtime reconfiguration | permitted |
| Flow control | optional, per Link profile; 2 B or 8 B credit extension |
| Future CAN growth path | 29-bit Classical CAN profile planned; layout unspecified |
| Usage maturity model | Levels 0-5 (§1.5); Levels 0-1 must stay easy |
| Promiscuous/bring-up mode | host tooling and gateways only; never creates Wiring |
| Identity scope | one WireSpace; joining two needs a translating gateway |
| Local interface concept | Port (§2.6); never appears in a PDU |
| Baseline Transport | Unreliable Datagram + receiver freshness handling |
| Master-initiated Links | I2C, SPI in scope; LLL polls without becoming producer |
| Privileged capabilities | separate build required; config cannot enable them |
| Interoperability | not claimed (§32.1) |
| RTL support | future first-class target |

---

# 35. Worked Examples

## 35.1 One dev board

```text
PC
 |
USB/UART
 |
MCU
```

MCU:

```text
one Endpoint Domain
one Link
Identity Service
Health Service
Log Service
Firmware Update Service
```

Behavior:

- anonymous `kLocalBus`;
- no WireNumber configuration;
- direct/linear dispatch;
- copy-based send;
- PC tool immediately shows identity, health, logs, and update capability.

This is not a demo subset that violates the architecture. It is the architecture at its smallest useful scale.

## 35.2 Multicore MCU with telemetry

```text
                    MCU

Core 0 Domain                Core 1 Domain
-------------                -------------
SensorService                EstimatorService
ControlService               PlannerService
      \                          /
       \                        /
        shared-memory WS Link
                 |
           Core 2 Domain
           -------------
           HealthService
           LogService
           Telemetry Link
                 |
                CAN
                 |
                 PC
```

Internal application messages use device-private Wires. Same-domain traffic dispatches `Inline` or through `Serialized` Service inboxes. Cross-core traffic uses shared-memory Link Interfaces.

Selected health/debug traffic uses the Internal Debug Wire, which is spliced to the host-facing telemetry Wire so it becomes visible on CAN as a network-visible WireNumber.

No separate IPC middleware is required.

## 35.3 CAN-to-Ethernet gateway

```text
CAN_A ----\
CAN_B -----\
CAN_C ------+--> MCU/FPGA gateway --> Ethernet --> Host
CAN_D -----/
```

Receive path:

```text
CAN frame(s)
    |
CAN LLL / PDUA
    |
complete WS PDU
    |
Router table
    |
    +--> Ethernet TX
    |
    `--> optional local Endpoint Domain tap
```

Ethernet can coalesce multiple small PDUs. The gateway does not need to understand the application Service carried by every Wire.

A copy-based MCU gateway may only need CAN LLL buffers, route lookup, an Ethernet TX copy queue, and a local tap copy if configured. A future FPGA implementation can replace the copies with streaming/buffer-descriptor mechanisms without changing the topology model.

---

# 36. Core Architectural Invariants for Agents and Implementers

Particularly important when generating code/designs from this document.

1. **A Wire has exactly one Origin and zero or more Nodes.**
2. **Origin is a per-Wire role, not a device class.**
3. **NodeId 0 is not a Node.** It represents broadcast in `OriginToNode` and is invalid in `NodeToOrigin`.
4. **The complete canonical PDU is the generic Router/gateway forwarding unit.**
5. **The LLL owns Link mechanics, not generic Wire routing policy.**
6. **WireAlias is Link-scoped.** It is never a globally canonical Wire identity.
7. **`kLocalBus` is alias 0.** If mapped, canonicalize immediately; if unmapped, RX remains locally usable but cannot be generically forwarded.
8. **Anonymous LocalBus TX is allowed only when one eligible Link makes it unambiguous.**
9. **Device-private Wires never leave the device as device-private WireNumbers.** The only way their traffic reaches an external Link is an explicitly configured splice to a network-visible Wire.
10. **A splice applies before egress and after ingress canonicalization.** The scope check runs on the post-splice representation, and only the Wire representation changes.
11. **At most one splice per local routing step.** No chained or recursive splices.
12. **An anonymous (unmapped) LocalBus is not spliceable.**
13. **A spliced Wire must still have exactly one Origin and unique NodeIds across all its segments.**
14. **`kLocalDomain` never enters a Link Interface and is never spliced.**
15. **Inter-core communication channels are ordinary Links/Link Interfaces.**
16. **No generalized RouterPort abstraction is currently required.**
17. **The Router should not require a central routing task.** Caller-context concurrent routing against read-mostly state is preferred.
18. **Failure on one egress does not cancel other successful egresses.**
19. **Base multihop Wire realization is acyclic**, including through splices.
20. **Copy-based buffer ownership is the first-class baseline.** A successful `send()` means the caller may reuse its buffer; it does not mean delivery.
21. **Autonomous Service TX uses an injected binding, not a hard-coded Wire.**
22. **Declared delivery policy is preserved across placement changes.** A `Serialized` Service stays serialized when its peer moves on-chip.
23. **QoS-Minimal and QoS-Full are the two standard implementation profiles currently favored.**
24. **Queue-full policy is configurable per Link instance.**
25. **Congestion is a normal send outcome, not automatically a Link fault.**
26. **Link-level flow control is optional and profile-specific.**
27. **Classical CAN Nodes are not required to implement generic LLL credit flow control.**
28. **Queue utilization/overuse and congestion drops must be observable.**
29. **Static traffic/capacity checking is preferred over adding complex adaptive congestion protocol to Core.**
30. **Classical CAN PDUA MaxN is 8; N <= 4 is the normal target.**
31. **Classical CAN aggregate CRC policy is N=1 none, N=2..4 CRC-8, N=5..8 CRC-16**, and usable capacity is the net column of §22.9.
32. **Classical CAN FrameControl uses 3 MessageGeneration bits and 3 FramesRemaining bits.**
33. **No Endpoint truncation, implicit aliasing, or silent remapping.** An unrepresentable canonical value fails placement or TX before a frame is emitted.
34. **No partial PDU is ever visible above the LLL.**
35. **Long FPGA physical paths do not imply more Wires.**
36. **Origin failover/election is not a base Wire feature.**
37. **Do not require static Manifests, Flows, or WireContracts for basic communication.**
38. **Structural validity is always enforced; contract-level policy is always optional.** The two lists are in §20.4.
39. **`kLocalBus` is the compression code for the Link's own physical Wire**, not an arbitrary reserved number, and not a claim that the bus is Wire 0.
40. **Promiscuous observation never creates persistent Wiring**, and is a host-tooling/gateway capability rather than ordinary Node behavior.
41. **Ephemeral auto-Wiring must announce itself** and must never be silently promoted to authoritative configuration.
42. **Commissionability is independent of routing capability.** A LocalBusOnly or N=1-only node may still accept NodeId/Wire assignment.
43. **A gateway reports local facts; the Organizer decides.** Gateways do not infer or repair routes.
44. **A Port is local; an Endpoint is network-visible.** No canonical field carries Port identity, and `TxBinding` is an output Port.
45. **Link independence is bounded by declared size, timing, and transport compatibility.** An unplaceable Service fails configuration or TX; it is never silently degraded.
46. **Transport is selected by Service semantics, and reliability is not assumed safer than loss with freshness detection.**
47. **A receiver of periodic state must define its behavior when that state is absent or stale.** Duplicate suppression belongs to the Service or composition, never to the Router.
48. **Initiating a transfer does not confer producer authority.** An LLL may poll or autonomously schedule; the source Endpoint remains the producer.
49. **Electrical visibility grants nothing.** Attachment to a medium confers no membership, delivery, forwarding, or transmit authority; only configuration does.
50. **Physical proximity does not imply `kLocalDomain` or a device-private Wire.**
51. **Higher-level composition must flatten into ordinary Endpoints, Wires, bindings, and bounded local state.** The Router and LLL never learn a composition pattern.
52. **Redundancy composed above Wires owns correlation, duplicate suppression, stale rejection, failover, per-Wire health, and per-sink coverage.** Observers provide no coverage.
53. **Privileged capabilities must be absent from normal builds.** Runtime configuration alone must never enable one.
54. **Canonical identity is scoped to one WireSpace.** Connecting two WireSpaces requires explicit identity translation, not plain forwarding.
55. **No wire interoperability is claimed.** Independent implementations are not presumed compatible without shared definitions or common vectors.

---

# 37. Explicitly Superseded / Do-Not-Reintroduce Without Review

Older documents may contain these concepts. They should not be assumed current:

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
- the older adapter draft's Namespace-0 allocation (`1..32` public, `33..991` deployment, `992..1023` public General-only). The current working plan is §21.3.
- `Tap` / `Relay` / `Replicator` as base architectural roles. Local taps and gateway forwarding are ordinary routing outcomes (§12.3); re-originating traffic is an application-level service.
- **Ephemeral route repair / learned forwarding.** An earlier change statement proposed that a gateway with no entry for a Wire, observing that Wire's traffic on two eligible interfaces, could infer continuity and install a temporary learned splice, aged out or discarded on reboot. This is rejected: it puts topology inference in the data plane, conflicts with the acyclic-realization and no-duplicate-suppression rules (§12.4), and makes forwarding depend on observed traffic rather than installed configuration. A gateway reports what it observed (§17.1, §27.4); the Organizer installs routes (§19.1).
- Every physical bus having canonical WireNumber 0 because it uses `kLocalBus`. Alias 0 compresses the Link's *own* number, whatever that number is (§6.2).
- `Route` as an object distinct from the Wire it realizes, with its own terminals and Direction. Forwarding is a local table (§11), and Direction is a per-PDU field on a Wire (§4.1); there is no separate cross-domain path object.
- `ParticipantId` as a third identity space alongside Endpoint and Node identity. It existed to name members inside a communication component; that catalog is deferred (§0.2), and if such components arrive they flatten into ordinary Endpoints and Wires (§20.5).
- **Reachability requiring a Manifest.** An earlier generation held that reachability and authority exist only where a Manifest configures them. Deliberately relaxed; see §20. Structural validity is still always enforced (§20.4).
- A **Wire Space as a security or memory-protection boundary.** Recovered only as an identity scope (§5.6). It is not claimed to enforce anything, and §29 remains the security position.

If an implementation task appears to require one of these, first verify that the current architecture genuinely cannot solve the problem without it.

---

# 38. Important Open Questions

## Canonical / allocation

- Final exact WireNumber allocation fences.
- Whether canonical header extensions need additional standardized common fields before freeze.
- Exact TransportType registry.
- Namespace 3 registry process (allocation authority, experimental reclamation, tombstones).

## Splicing

- Whether a splice is one bidirectional mapping or two directional route entries.
- Whether multiple devices may splice onto one shared external Wire by default, and how NodeId coordination is validated if so.
- Whether `InternalDebugWire` receives a standard reserved device-private WireNumber.
- Splice representation in generated/static configuration.

## Router / concurrency

- Seqlock vs immutable-table swap as the preferred reference implementation.
- Exact representation of local Endpoint Domain delivery in routing tables.
- Sparse vs dense tables on constrained targets.
- Exact behavior during live route-table reconfiguration.
- Endpoint lifetime/safe-publication mechanism if runtime Service replacement is introduced.

## QoS / queues

- Whether unsupported QoS collapses to Normal or is rejected on QoS-Minimal Links.
- Exact default queue-full policies.
- Whether latest-value replacement is keyed by Service/Endpoint stream, and how that key is defined.
- Whether any standard shedding/admission hooks are needed beyond implementation-specific policy.

## Flow control

- Exact credit accounting (cumulative grant, watermark, or another scheme).
- Update cadence and standalone credit-frame format.
- Whether credit is measured in bytes, fixed buffer units, or profile-defined quanta.
- How much downstream pressure a gateway should reflect into upstream credits.

## Ownership

- Zero-copy destination-owned allocation API, if and when it is justified.
- Whether source-owned immutable buffers with multi-driver completion tracking are ever worth the complexity.

## Telemetry

- Exact Domain Local Link Telemetry Service schema.
- Exact minimum counters required by conformance classes.
- Exact per-Wire top-talker/drop-report format.

## Classical CAN

- Final CRC-8 algorithm/parameters; SAE J1850 is the current implemented candidate.
- Final CRC-16 algorithm/parameters.
- Aggregate CRC placement, coverage, and byte order.
- Final byte-exact General PDUA and optimized-N1 encoding.
- Final CAN-ID bit ordering, and the canonical-QoS-to-arbitration mapping for the current field layout (§22.1).
- Whether the aggregate CRC and PDUA generation counter are sufficient to detect the duplicate case now named in §18.1, or whether that is purely a Service-level concern.
- Exact DLC rules, length derivation, padding, and short-continuation handling.
- Reassembly timeout rules and rate assumptions.
- Whether CAN FD/XL share the same PDUA concepts or receive simpler native-PDU profiles.
- Whether the planned 29-bit Classical CAN profile (§22.11) is standardized, and its identifier layout.

## UART / byte streams

- COBS vs HDLC-style framing.
- CRC-16 polynomial/parameters.
- Maximum frame/PDU size.
- Flow-control extension encoding and interaction with framing.

## Discovery / configuration

- Exact pre-addressing commissioning algorithm per Link type.
- NodeId conflict/rejoin behavior.
- Alias/config generation and update rules.
- Persistence/lease semantics for ephemeral host configuration.
- Exact discovery Service schema.
- Exact gateway-management Service schema, including the "make interface N carry Wire W" operation.
- Policy for cyclic and non-tree topologies during auto-Wiring.
- Scope and build-time removal rules for promiscuous/bring-up mode (§27.4).

## Freshness and transport semantics

- How a Service declares freshness: a locally checked interval, or a canonical timestamp/sequence in a header extension.
- Whether any standard "state is stale" notification exists, or whether each Service defines its own degraded behavior.
- Whether ordering guarantees are declared separately from delivery guarantees.
- How polled-Link cadence (§2.8) is expressed in Link capabilities so freshness bounds can be checked statically.

## Ports and composition

- Whether Port becomes a generated API concept or stays an architectural term.
- Whether input Ports need a naming/binding mechanism symmetric with `TxBinding`.
- The first composition pattern to attempt under §20.5, and whether it genuinely flattens.

## Master-initiated Links

- I2C and SPI transaction formats, PDU delimiting, and the idle/"nothing to send" response.
- Who owns polling cadence configuration, and whether it is a Link-profile or Wiring property.
- Whether SPI needs its own framing and CRC choices or can reuse the byte-stream profile (§24).

## Identity scope

- Whether a WireSpace needs an explicit identifier, or remains an implicit deployment-wide scope.
- What a cross-WireSpace translating gateway looks like, and whether it is worth specifying before a real case exists.
- Whether any detection is possible when two WireSpaces are accidentally bridged by a plain forwarding gateway (§5.6).

## Higher layers

- Reliable Transport(s).
- Multi-Origin command-source policy if useful.
- Service version/schema compatibility mechanism.
- Formal Manifest, Flow, WireContract, and static analysis models.
- Security/authentication profiles.
- Redundant/cyclic Wire realization if ever justified.
- Whether a Domain Control layer (§0.2) is ever needed for group management of Services.
- Whether a communication-component catalog is worth establishing once §20.5 has been exercised.
- Mining the remaining `WS_old/network/` sibling documents (front matter).

---

# 39. Near-Term Implementation Guidance

The architecture should now be tested by building, not expanded indefinitely on paper.

A strong first implementation sequence:

```text
 1. Canonical PDU type + 40-bit descriptor helpers
 2. Endpoint Domain Dispatcher (Inline + Serialized delivery)
 3. read-mostly Router + static forwarding table
 4. copy-based send() abstraction and TX binding injection
 5. one simple host/serial or UDP Link
 6. one Classical CAN/vcan Link with PDUA + aggregate CRC
 7. LocalBus configured/unconfigured behavior
 8. gateway forwarding between unlike Links
 9. device-private Wires + splice to an external Wire
10. multicore/shared-memory Link simulation
11. Link counters/telemetry + queue-pressure reporting
12. host discovery/config tooling
```

Early concurrency should use mutexes, critical sections, bounded queues, and copies. More advanced routing tables, seqlocks, lock-free MPSC queues, per-Link pools, and zero-copy should be added after measurements demonstrate a need.

Only after the above exist should the project freeze more advanced details such as UART framing, Link credits, richer Transport behavior, or static Manifest traffic analysis.

Agents should produce **small concrete reference implementations and tests**, not generalized framework hierarchies, unless the same abstraction is already demanded by more than one real Link/target.

---

# 40. Compact Architecture Summary

```text
                         Endpoint Domain
                              |
                           Dispatcher
                              |
                         Endpoint/Service
                              |
                              v
                            Router
                     shared read-mostly state
                    /          |          \
                   /           |           \
              Link IF      Link IF      Link IF
                 |             |            |
                LLL           LLL          LLL
                 |             |            |
               CAN        shared memory   Ethernet
```

The PDU carries:

```text
Namespace
TransportType
QoS
Header-extension flag
WireNumber
Direction
NodeId
EndpointId
```

A Wire provides the logical bus: one Origin, zero or more Nodes.

A constrained Link may encode a Wire using a local alias. `kLocalBus` makes one-Link prototypes work before a canonical WireNumber exists. Device-private Wires and `kLocalDomain` reuse the same model for multicore/internal communication, and a splice is the one explicit place where an internal Wire becomes externally visible.

The Router forwards complete PDUs through bounded local tables. Link queues expose congestion rather than hiding it. Optional hop-by-hop credits can backpressure fast aggregation Links. Static configuration/tooling should prevent normal steady-state saturation. Richer systems add diagnostics and host tooling without forcing that complexity onto tiny Nodes.

> **WireSpaces is intended to make embedded communication look like a small set of logical buses carried by interchangeable Links, with host-side intelligence building simple local forwarding state that can be executed efficiently in MCU software, multicore systems, Linux gateways, and RTL.**

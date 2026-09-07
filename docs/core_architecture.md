# WireSpaces — Core Architecture

**Status:** Private first draft; working architecture; provisional but intended to be buildable
**Scope:** The protocol model and host runtime. Everything here is meant to be implementable now
**Excluded:** Per-carrier encodings (`LINK`), configuration and tooling (`DEPLOY`), test vectors (`CONFORM`), implementation notes (`IMPL`), undesigned material (`FUTURE`)

This is the main WireSpaces document. It is **not a normative protocol specification**: byte-exact Link encodings, CRC parameters, API signatures, timing requirements, and registry allocations belong to `LINK` or to narrower profile specifications. Where this document gives a concrete field width or behavior, it means "current agreed direction" unless the text says it is frozen.

The governing rule for changes is in `INTRO §3`: do not add a new protocol abstraction until a concrete implementation or use case demonstrates that the current model cannot solve the problem cleanly.

---

# Part I — Model and Vocabulary

# 1. Layering and Terminology

The preferred mental model is:

```text
Physical Link
    CAN / UART / RS-485 / USB / Ethernet / shared memory / FPGA FIFO / ...
        |
        v
Link Interface
    one host's local attachment to that Physical Link
        |
        v
Logical Link Layer (LLL)
    framing / validation / PDU fragmentation and reassembly
    Link Binding, field elision, and host projection
        |
        v
complete canonical WireSpaces PDU
    Wire + source HostId + destination HostId
        |
        v
Router
    propagates the PDU across the configured Wire topology
    and selects destination Endpoint Domain(s) for local delivery
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

## 1.1 Physical Link

A **Physical Link** is the actual communication medium, bus, connection, or channel: Classical CAN, CAN FD/XL, UART, RS-485, USB, Ethernet, I2C, SPI, a shared-memory queue, an FPGA FIFO or streaming interconnect, a custom serial PHY, or a radio. A Physical Link may be point-to-point or bus-like.

It may also be **master-initiated**, meaning data moves only when one side initiates a transaction. I2C and SPI are the clearest cases, and some RS-485 turnaround schemes behave the same way. See §1.7.

## 1.2 Link Interface

A **Link Interface** is one host's local attachment to a Physical Link.

A gateway with four external buses and one inter-core shared-memory channel therefore has five Link Interfaces. Link Interfaces may be indexed locally beginning at zero; a small integer such as `uint8_t` is expected to be sufficient for normal systems.

No globally persistent Link Interface identifier is required by the base architecture.

Inter-core communication does not need a separate routing abstraction. A shared-memory or FIFO channel is simply another Physical Link with Link Interfaces on its ends.

## 1.3 Logical Link Layer (LLL)

The **LLL** adapts a Physical Link to canonical WireSpaces PDUs.

Typical responsibilities:

- framing and delimiting;
- integrity validation where the Link profile defines it;
- fragmentation and reassembly when the carrier is smaller than a PDU;
- aggregation when the carrier is much larger than a PDU;
- Link-local control metadata;
- reconstructing an unambiguous canonical Wire, source HostId, and destination HostId from the Link Binding and received representation;
- eliding canonical fields on egress where the Link Binding makes them uniquely recoverable;
- projecting Link-local host codes to canonical HostIds where a constrained profile requires it;
- optional Link-level flow control;
- bounded scheduling or polling where the carrier is master-initiated (§1.7);
- native Link error/status accounting.

The LLL **does not generally own Wire routing policy**. It owns carrier representation and the Link Binding needed to reconstruct canonical identity. The generic decision about which other Links carry a Wire, and which local host Domain may accept the destination, belongs to the Router.

The generic routing boundary is the **complete canonical PDU**, not an arbitrary carrier fragment. Before a PDU reaches the Router or canonical dispatch, ingress has reconstructed its full canonical Wire, source HostId, and destination HostId. This is what permits heterogeneous gatewaying without every pair of Links requiring a custom bridge protocol.

## 1.4 Router

The **Router** owns local Wire forwarding decisions.

It should be thought of primarily as:

- a read-mostly table/database;
- a routing function;
- local forwarding policy;

not necessarily as a thread or task.

The preferred software direction is for Link RX paths and Endpoint Domains to invoke routing in **their own execution context** against shared read-mostly tables. This avoids introducing a single central routing task as an artificial throughput bottleneck.

## 1.5 Endpoint Domain

An **Endpoint Domain** is a local Endpoint ownership and dispatch boundary.

Examples:

- one MCU application domain;
- one CPU core's set of Endpoints;
- one isolated partition;
- one host process/domain;
- one RTL subsystem.

Every Endpoint Domain has a **Dispatcher** that resolves received PDU identity to the appropriate local Endpoint and records/reports invalid delivery attempts.

An Endpoint Domain is **logical**. It is not necessarily a device, a CPU, an MCU, a core, a process, or a physical network attachment. A small MCU usually holds exactly one; a larger SoC, FPGA, or host may hold several for partitions, cores, processes, or hardware subsystems. Conversely, several execution contexts may deliberately *share* one Endpoint Domain when they share its Endpoint namespace, dispatch authority, and access model.

The Dispatcher need not be one task or one object. It may be a generated table, a `switch`, a distributed implementation, or a hardware block. What matters is that the Domain has **one coherent dispatch and authority decision** — two mechanisms independently deciding what a Domain accepts is two Domains.

An Endpoint Domain is also a **concurrency scope**, covering acceptance into every declared ingress element behind its Endpoints. Where its Endpoints permit several writers (§9.6), the Domain must provide a mechanism that serializes them:

> **An Endpoint Domain provides whatever mechanism serializes concurrent writers to its Endpoints. The mechanism is implementation-defined; WireSpaces specifies the required property, never the primitive.**

On a single-core or AMP MCU a short interrupt-masked critical section is usually sufficient, because another core is not a concurrent writer into this Domain's storage — it arrives through an inter-core Link whose local receive driver is an ordinary producer here (§13.2). On SMP hardware, masking interrupts on one core excludes nothing on another, so a genuinely inter-core primitive is required; a bounded operation under a short platform-appropriate lock is a perfectly good implementation and no lock-free structure is implied. In RTL there is no lock at all — a single write port or an arbiter satisfies the requirement directly, which is why it is stated as serialization rather than mutual exclusion.

What the serialized region may contain is constrained, and this is what keeps it analyzable: obtain exclusion, check storage state, copy one bounded item or retain an explicitly owned reference, publish, release. No blocking wait, no allocation, no arbitrary application code, statically bounded copy and bookkeeping, explicit failure when capacity is unavailable. WireSpaces sets no universal time limit such as "under a microsecond"; each platform establishes its own worst-case bound against its interrupt-latency requirements.

Two cautions about the word "Domain":

- **A Domain is not a security boundary by itself.** Typed APIs and static configuration reduce accidental misuse, but enforcement against compromised or untrusted code needs a real protection boundary: MPU/MMU isolation, process isolation, hardware partitioning, or separation into independently protected Domains. See §22.
- **A Domain is not necessarily a device.** Each independently routed and dispatchable Endpoint Domain has exactly one deployment-scoped HostId and uses that same HostId on every Wire in which it participates (§3.1). One device may host several Endpoint Domains; several execution contexts may share one when they share dispatch and authority.

## 1.6 Endpoint and Service

An **Endpoint** is a network/message-visible termination identified by `Namespace + EndpointId`. It may belong to an MCU Service, a host application, a hardware/RTL block, a diagnostic utility, or a gateway-local observer.

Endpoint identity identifies the protocol or recipient semantics. It does **not** by itself determine the Wire on which an autonomous transmission should occur (see §10).

An Endpoint is also the local object a Service touches. It exposes one Transport Entity boundary with declared, bounded ingress storage (§9.4–§9.5). A simple datagram Endpoint has one Queue or Snapshot; a Transport Entity may classify messages into several declared storage elements behind that same Endpoint. The network-visible Endpoint remains one identity and one acceptance boundary.

A **Service** is reusable functionality exposed through one or more Endpoints. Each Endpoint has one Transport Entity boundary; the simple Unreliable Datagram case may be implemented directly by its Queue or Snapshot without a separate runtime object (§20). A Service may run inline in bare metal, in its own RTOS task, with several Services in one task, on another core, on another ECU, in a softcore, or in pure RTL. WireSpaces should not require a specific task model.

### WireSpaces does not define the Service-to-application interface

WireSpaces does not define how a Service exposes itself to application code. That interface is invisible on the wire and may differ between bare-metal firmware, RTOS tasks, host software, and RTL.

WireSpaces does define the Endpoint/Transport Entity boundary below the Service. Receive acceptance validates, classifies, and stores with statically bounded framework work; protocol processing and Service callbacks occur later in the owning execution context (§9.4). On transmit, ownership and admission are explicit (§16). Service code cannot inject callbacks, blocking work, allocation, or protocol state transitions into receive acceptance.

### The Endpoint API is the portability surface

The silence above the Service must not be mistaken for silence below it. The Endpoint API is where a Service meets WireSpaces, and that surface is intended to be **portable**:

> **A Service's WireSpaces-facing code should compile and behave identically across implementations on comparable technology stacks.** Bounded delivery, Queue and Snapshot semantics, declared writer concurrency, and transmit Endpoints are a contract to Service authors, not merely a description of what an implementation happens to provide.

Three layers, with different rules, and it is worth being explicit about which is which:

```text
application code        WireSpaces defines nothing here
    |
Service                 portable within a declared resource and timing envelope
    |
Endpoint API            the portability contract
    |
Router / LLL / drivers  freely different per implementation and target
```

A low-end and a high-end 32-bit MCU can run **identical** Service source over completely different network stacks, task models, and drivers. That is the point: without it there is no Service ecosystem, because every shared Service would need reimplementing per stack, and `SVC-7`'s schema-over-bytes contract would guarantee only that two incompatible implementations agreed on the bytes.

Portability is bounded rather than absolute, in exactly the way link independence is (`SVC-2`). A Service remains portable within its declared **resource and timing envelope**: Endpoint storage that does not fit the target's RAM, or a consumer cadence the target's scheduler cannot meet, makes a Service unplaceable there no matter how unchanged its source is. And a Service reaching outside WireSpaces — for timers, storage, GPIO, an RTOS API — is portable only as far as those dependencies are, which is the Service author's problem and is why composed or injected dependencies matter more here than they would in application code.

This raises the stakes on the Endpoint API's shape. Names, capacity declaration, and the way the decoded representation is expressed stop being cosmetic once they are the surface Services are written against, which is why they are tracked as open questions rather than left to the first implementation (`REG §6.12`).

## 1.7 Master-initiated and polled Links

Not every carrier lets both sides transmit whenever they choose. On I2C and SPI, and on some half-duplex turnaround schemes, data moves only when one side initiates a transaction. A host on such a Link cannot push a publication at the moment it is produced; the traffic appears only when the initiating side polls for it.

This is a Link mechanic, and it belongs entirely to the LLL and driver. The LLL may therefore:

- schedule transmission of Service-owned state on its own timetable;
- initiate a bus transaction purely to collect whatever the remote host has queued;
- poll at a cadence unrelated to when any Service called `send()`.

None of that makes the LLL a producer:

> **Initiating a transfer is not authoring a message.** Producer authority stays with the source Endpoint that supplied the payload, regardless of which side of the Link caused the bytes to move.

The practical consequences are worth stating, because they are easy to get wrong:

- A polled host's publication is stale by up to one polling interval. That latency is a Link capability fact (§17), not a Service behavior, and it is exactly the kind of thing freshness handling (§21.4) exists to expose.
- The initiator's polling cadence bounds the remote host's effective TX rate, so static capacity checking (`DEPLOY §2.2`) has to account for it.
- A poll that returns nothing is not an error. An empty response is the normal case on a mostly idle Link and must not be counted as a Link fault (§18.1).
- QoS on a polled Link is limited by cadence, not arbitration. A Critical publication cannot beat the next poll, so such Links are usually QoS-Minimal (§14.1).

Exact I2C and SPI transaction formats are `LINK §7` work; only their architectural placement is settled here.

## 1.8 Canonical and retired terminology

The canonical identity fields are `SrcHostId` and `DestHostId`. `HostId` identifies one Endpoint Domain throughout a deployment, independently of Wire membership and physical-device identity. `Participant`, `ParticipantId`, `SrcParticipantId`, and `DestParticipantId` are superseded names; historical documents retain them for provenance.

`Origin`, `Node`, `NodeId`, and canonical `Direction` remain retired. Other retired canonical terms include `PathTag`, `RoutingProfile`, `RoutingCode`, `PeerId`, `WireBand`, `Main`, **Link Engine** as a routing catch-all, **Router Port**, and **Port**. Use Link driver/LLL for carrier mechanics, Router for propagation, and Endpoint for the Service's communication boundary.

`WireAlias`, VCN, and Direction may occur in the CAN11 profile (`LINK §2`). They are Link-local representation fields and never canonical identity. `MainA`, `MainB`, and `NodeN` in its default map name profile positions, not canonical roles or authority.

---

# 2. Canonical PDU Descriptor

The accepted canonical descriptor semantics are the fields `Control`, `WireNumber`, `SrcHostId`, `DestHostId`, and `Endpoint`. Their widths are not part of that semantic acceptance. The current preferred provisional packing is **48 bits / 6 bytes**:

```text
Control: 8 bits
    QoS                   2
    Reserved              2  // zero on transmit; nonzero is invalid
    HasHeaderExtensions   1
    TransportType         3

WireNumber:               8 bits
SrcHostId:         8 bits
DestHostId:        8 bits

Endpoint: 16 bits
    Namespace             2
    EndpointId           14
--------------------------------
Base descriptor:         48 bits
```

Every displayed width and partition above is **provisional pending topology-corpus and headroom evidence**. In particular, neither the WireNumber width nor the HostId width/allocation is accepted as final. Exact bit placement and final registry identifiers must be frozen only after the concurrently updated registry and bit-layout documents agree; profile identifier layouts remain a `LINK` concern.

**Byte order is settled.** Wherever any representation serializes a literal multi-byte numeric value, WireSpaces uses **little-endian** order. Under the current packing candidate, a literal descriptor is ordered `Control`, `WireNumber`, `SrcHostId`, `DestHostId`, then the low and high bytes of `Endpoint`.

Settling this early costs nothing and removes a whole class of divergence between the project's C++, Python, and RTL implementations. Two boundaries keep it honest:

- It applies to **literal multi-byte numeric values only.** Bit placement inside a byte is a separate decision, and constrained-Link identifier layouts remain profile work.
- **Native object layout is never a wire representation.** A C++ `struct`, its padding, its enum widths, its bitfield allocation order, and the host's endianness define nothing. Encode and decode explicitly, and test on a big-endian model as well as a little-endian one (`CONFORM §2`).

**Reserved fields are rejected, not ignored.** A reserved field is zero on transmit, and a receiver that sees a nonzero reserved field **drops the PDU and counts it** (§18.1) rather than masking the field and proceeding. This is the choice that keeps future field assignment safe: a receiver that ignores reserved bits today cannot be given new meaning for them tomorrow without silently misreading traffic from every older device. Rejecting costs nothing while the fields are unused and preserves the ability to use them.

## 2.1 Preferred provisional address-space snapshot

| Concept | Current preferred provisional capacity |
|---|---:|
| Namespaces | 4 |
| EndpointId width | 14 bits per Namespace |
| Valid EndpointIds | 1..16383 |
| Usable Namespace/EID combinations | 65,532 |
| WireNumber width | 8 bits |
| HostId width | 8 bits |
| Ordinary HostIds | `0x00..0xFE` |
| Broadcast destination | `0xFF` |
| QoS field | 2 bits / 4 canonical values |
| TransportType | 3 bits / up to 8 values |

`EndpointId == 0` is invalid in every Namespace. Under the current HostId allocation candidate, `0x00..0xFE` are ordinary identities and `0xFF` represents `kBroadcastHost`, which is invalid as a source and reserved exclusively as a broadcast destination. The destination-only broadcast semantic is accepted; its numeric value and the number of ordinary identities remain provisional with the HostId width.

WireNumber width, allocation, and partitioning likewise remain provisional pending topology-corpus and headroom evidence. Reserved local and device-private scope values are described semantically in §4 without freezing numeric fences here.

## 2.2 Carrier encodings need not be literal

Under the current packing candidate, a Physical Link is **not required to transmit the six-byte sequence literally**. A Link profile may:

- encode some fields in native metadata;
- elide values uniquely implied by its Link Binding;
- project a Link-local host code to a canonical HostId;
- use a compact representation;
- reconstruct the complete canonical descriptor before handing a PDU to the Router or Endpoint Domain Dispatcher.

**Elision and projection are distinct.** Elision omits a canonical value because Link context supplies that same value, such as a CAN11 Link Binding supplying its one WireNumber. Projection translates a constrained Link-local code through configured state to a canonical HostId. Neither changes canonical semantics.

## 2.3 Header extensions

The base descriptor semantics include `HasHeaderExtensions`. Under the current 6-byte packing candidate, the compact-header target is that the **canonical WS header remains approximately 9 bytes or less including ordinary WS header extensions**.

This target is useful for logging and bounded-buffer implementations. It is not yet a frozen byte-exact extension format.

### The block is self-describing in length

One property of the extension format is worth fixing ahead of its contents, because everything else depends on it: **an extension block states its own total size, so a parser can skip it and find the payload without understanding any of it.**

The working direction is a 2-bit length code in the first extension byte:

```text
length code 00    1 extension byte total
length code 01    2 extension bytes total
length code 10    3 extension bytes total
length code 11    reserved -> reject
```

which yields the current candidate's 9-byte figure: a 6-byte base descriptor plus at most 3 extension bytes. The bits left over after the length code — 6, 14, or 22 — are available to separately standardized fields.

The value of self-description is that it decouples the payload boundary from the extension registry. A device built today can receive a PDU carrying an extension defined next year, locate the Service payload correctly, and dispatch it. Without a length, the same device has to reject anything it does not recognize, which makes every extension a flag-day change across the whole deployment.

Two clarifications follow from the encoding:

- **`HasHeaderExtensions` and the length code carry no meaning about content.** Neither indicates a timestamp, a sequence number, security metadata, or fragmentation. Presence and size only.
- **Extension bytes are canonical header, not Service payload.** They are inside the header budget, they are not delivered to the Service as data, and a Service's size accounting must include them where a profile permits or requires them (§21.2).

Complete-PDU forwarding preserves well-formed extension bytes even when the forwarding implementation does not interpret them. A receiving Endpoint may reject an unrecognized extension where that extension's definition requires understanding for safe acceptance. Forwarding and local acceptance are therefore deliberately different decisions.

**Link-local LLL extensions are separate.** For example, an 8-byte QoS-Full flow-control credit extension on an Ethernet LLL frame is Link metadata, not part of the canonical WS PDU header budget.

---

# Part II — Wires and Identity

# 3. Wire Model

A **Wire** is a loop-free **Logical Bus**: a connected logical propagation domain realized by one or more configured Links. A complete PDU injected onto a Wire propagates across that Wire's configured Link topology. `DestHostId` controls which host accepts the PDU; it does not normally select the forwarding path.

A Wire is not a list of pairwise routes. It may coincide with one Physical Link, select hosts across several Links, or overlap another Wire in host or Link membership. Overlap is useful when broad dissemination and narrow high-rate traffic need different propagation scopes.

```text
BroadStatusWire:  Controller, MotorA, MotorB, Logger
MotorControlWire: Controller, MotorA

Controller and MotorA participate in both Wires.
High-rate motor traffic uses the smaller MotorControlWire.
```

> **Prefer the smallest Wire that usefully represents the required communication or broadcast scope.** Span several Links when a shared Logical Bus is useful, not merely because forwarding makes it possible.

## 3.1 Host identity

A **Host** is an independently routed and dispatchable Endpoint Domain in a WireSpaces deployment. It is not necessarily a physical device or a PC: one MCU may contain one Host, and a multicore SoC may contain several. Several execution contexts sharing one coherent Endpoint namespace and authority boundary may constitute one Host. In tooling prose, a *host computer* means the PC running tools; that machine may itself contain one or more WS Hosts.

`Host`, `HostId`, `SrcHostId`, and `DestHostId` replace the former Participant terminology. This is a naming change to the existing identity model, not a new address space or a change to descriptor widths.

Every independently routed and dispatchable Endpoint Domain has exactly one deployment-scoped `HostId`. The Domain uses that same HostId on every Wire in which it participates; HostId is not allocated separately per Wire.

The current preferred provisional allocation is:

```text
0x00..0xFE   ordinary HostId
0xFF         kBroadcastHost; destination only
```

The width, ordinary range, and numeric broadcast value remain provisional pending topology/headroom evidence. Host identity is nevertheless semantically stable across Wire membership, replies, forwarding, and placement changes.

`SrcHostId` identifies the host Domain that authored the PDU. `DestHostId` identifies the host Domain allowed to accept it, or `kBroadcastHost` for broadcast. The broadcast sentinel is structurally invalid as a source; under the current allocation candidate its value is `0xFF`.

Host identity does not identify a device, Link, process, core, or Service. A device may contain several Endpoint Domains and therefore several HostIds. Conversely, one Endpoint Domain may span execution contexts if they share one coherent dispatch and authority boundary.

## 3.2 Source, destination, and acceptance

Directed and broadcast traffic use the same canonical PDU:

```text
directed:
    SrcHostId  = authoring Domain
    DestHostId = one ordinary HostId

broadcast:
    SrcHostId  = authoring Domain
    DestHostId = kBroadcastHost
```

A directed PDU still propagates over the Wire's configured topology. Only the destination host Domain may accept it for ordinary local dispatch. Other attached hosts may physically observe it, and a Router may forward it to every configured branch of that Wire, but observation does not confer acceptance or authority (§12.6).

Broadcast is bounded fan-out to the configured local host Domains that belong to the Wire. It is not "every electrically attached device," does not discover membership, and does not grant recipients transmit authority.

Source and destination say nothing about request/response, client/server, command/status, initiator/responder, or upstream/downstream. Those are Service semantics. A response is a new PDU authored by the responding Domain, with the request's source selected as its destination under an authorized reply binding (§10).

## 3.3 Wire structural invariants

Current strong invariants:

- every Endpoint Domain has one deployment-scoped HostId and uses it on every Wire;
- the reserved broadcast host sentinel is destination-only and invalid as a source;
- a Wire realization is connected and loop-free;
- ordinary generic forwarding is keyed by `WireNumber`, or by `(WireNumber, ingress Link Interface)` when topology requires ingress-sensitive fan-out; destination is not an ordinary next-hop key;
- complete-PDU forwarding preserves source HostId, destination HostId, Endpoint identity, TransportType, QoS, extensions, payload, and source lineage;
- a configured splice may deliberately change only Wire scope while preserving host identity and every other PDU semantic (§6);
- a component that consumes and authors semantically new traffic is composition, not forwarding, and uses its own source HostId;
- physical realization and Link-local representation do not change canonical Wire or host identity.

## 3.4 Why a Logical Bus rather than pairwise edges

One publication to several hosts is one Wire, not a set of pairwise network objects:

```text
Host 0x12
    |
    +-- Host 0x27
    +-- Host 0x41
    `-- Host 0x83
```

This matches CAN, RS-485, shared-memory broadcast, and FPGA interconnects while remaining valid across point-to-point Link topologies. Directed destination acceptance and Wire-wide propagation are deliberately separate.

## 3.5 Physical realization and overlapping Wires

The simplest Wire coincides with one physical bus, but the concepts are not identical. A Physical Link is a carrier; a Wire is configured logical propagation scope. One Link may carry several Wires where its profile can represent them, and one Wire may span several Links through Routers.

Multiple Wires may overlap:

- in host membership;
- on some but not all Links;
- at a Router or Endpoint Domain;
- as broad and narrow scopes for different traffic.

The resulting forwarding topology is validated independently for each Wire. A loop in one Wire is invalid even when the underlying physical graph is useful for another Wire.

Constrained profiles may support less than the canonical model. Native CAN11 can represent overlapping Wires through distinct WireAliases; a Guest allocation supplies one Wire (§5.3). Use CAN29 or another richer profile when alias/VCN capacity or migration headroom is insufficient.

## 3.6 What a Wire does not guarantee

A Wire defines propagation scope and configured host membership. A Physical Link defines carrier reachability. Neither, on its own, supplies any of the following:

```text
reliability
ordering
freshness
deadlines
exactly-once delivery
flow control
redundancy or failover
duplicate suppression
```

Each exists only where something explicitly provides it: the selected Transport (§20), the Service contract (§21), composition above the Wire (§19.2), a Link profile's flow control (§15.4), or deployment analysis (`DEPLOY §2.2`). Membership means the host may take part under configured bindings; it does not imply that every Endpoint is reachable. Multipath wiring does not create redundancy — that requires explicit composition (§23.11).

---

# 4. Wire Scope and WireNumber Allocation

WireSpaces needs several scopes of communication without inventing separate message formats.

The current scope ladder is:

```text
kLocalDomain
    one Endpoint Domain only

kLocalBus
    one canonical local-only Wire bound to at most one local Link Interface

Device-private Wire
    one device / SoC, possibly crossing cores/domains

Named network Wire
    routable across devices and Physical Links
```

## 4.1 Named network Wires

A named Wire has a canonical `WireNumber`. Generic routing and cross-Link forwarding use that canonical identity.

Named Wire metadata may eventually include a stable UUID, a human-readable name, and configuration/version identity. The exact metadata schema is not part of the base PDU.

The UUID and name exist for the same reason device UUIDs do (`DEPLOY §1.7`): they provide **continuity in tooling and logs even when the short WireNumber changes**. A WireNumber is a compact routing identity and may legitimately be reassigned between deployments, during commissioning, or when a topology is reorganized. Historical captures, drop journals (§18.3), and exported Wiring should remain interpretable across such a change, which requires an identity that is not the 8-bit number.

## 4.2 Device-private Wires

A portion of WireNumber space is reserved for **device-private Wires**. Exact widths, numeric fences, and the partition between network-visible, device-private, `kLocalDomain`, and `kLocalBus` values remain provisional pending topology-corpus analysis. This document relies on the scope categories, not on provisional registry numbers.

A device-private Wire is a real Logical Bus with ordinary source, destination, Endpoint, and propagation semantics. It:

- can cross internal Physical Links such as shared memory;
- can span CPU cores or Endpoint Domains;
- **must not be emitted onto an external Link Interface as a device-private WireNumber**;
- must not be accepted as externally addressable network state.

The single sanctioned exception is a **Wire Splice** (§6): a device-private Wire may be spliced to a network-visible Wire, and the splice rewrites the Wire representation *before* egress. What leaves the device is therefore always a network-visible WireNumber, never a device-private one. The scope check is evaluated on the post-splice Wire.

Absent such a splice, an external LLL/Router boundary treats a device-private Wire on external ingress or egress as invalid configuration/traffic.

## 4.3 Local-only scopes

`kLocalDomain` is a reserved Wire value meaning:

> Deliver within the originating Endpoint Domain; do not enter a Link Interface.

This is useful when several Endpoints in one Domain communicate using ordinary Endpoint/PDU APIs but the communication should never leave the Domain. With static tables this can approach the cost of a table lookup plus a function call.

The same `kLocalDomain` value is used independently in every Endpoint Domain because its scope is inherently local. Unlike a device-private Wire, `kLocalDomain` is **never** spliceable and never reaches a Link Interface.

`kLocalBus` is also a reserved canonical WireNumber, but it includes one local Physical Link:

```text
kLocalDomain:
    no Physical Link involved

kLocalBus:
    canonical local-only Wire
    bound to at most one local Link Interface per Router/Endpoint Domain
    fully canonical on ingress
    dispatchable locally
    never transparently forwarded or spliced as itself
```

The detailed `kLocalBus` rules are in §5.

## 4.4 FPGA interpretation

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

## 4.5 Proximity does not imply locality

§4.4 says that physical distance does not create Wires. The inverse also holds, and is easier to get wrong:

> **Physical closeness does not make something local.**

A peripheral on the same PCB, reached over SPI or I2C, is normally a **separate Endpoint Domain** on an ordinary Physical Link, using an ordinary network-visible Wire. "Board-local", "on the same die", or "same connector" implies none of the following:

```text
kLocalDomain
a device-private WireNumber
skipping the Endpoint storage boundary
skipping Endpoint dispatch
skipping structural validity checks
```

`kLocalDomain` (§4.3) means *within this Endpoint Domain's dispatch scope*, which is an authority and dispatch property, not a distance. Two cores 3 mm apart that own separate Endpoint sets are two Endpoint Domains communicating over a Link; one core and an RTL block sharing a dispatch scope may be one Endpoint Domain even if the routing between them crosses the die.

The device-private range (§4.2) is likewise scoped by *device*, not by proximity. A companion chip on the same board is not inside the device boundary just because it is inside the enclosure, and traffic reaching it needs an ordinary Wire or an explicit splice (§6).

## 4.6 One deployment identity universe

Canonical HostId assignment is coordinated within one deployment identity scope. WireNumber and Endpoint identity are interpreted within that same configured WireSpace. They mean nothing in an independently assigned universe.

```text
Link-scoped        Link-local projected codes, if any
Domain-scoped      kLocalDomain
Local-Link-scoped  canonical kLocalBus use
Device-scoped      device-private WireNumbers
Deployment-scoped  HostId, WireNumber, Namespace/EndpointId
```

The operational rule is the same shape as splicing:

> **Two independently engineered WireSpaces do not become one by being connected.** Joining them requires a gateway that explicitly translates identity, in the same way a device-private Wire requires an explicit splice to become externally visible.

A plain forwarding gateway (§12) is **not** such a translator. It preserves canonical identity by design (§3.3), so wiring two independently assigned WireSpaces together with one can create silent collisions in WireNumbers, HostIds, and Endpoint identities.

Plain forwarding also never merges independently assigned HostId universes merely because two Links are connected. Identity translation, where needed, is explicit composition that authors new traffic; it is not a splice and not transparent forwarding.

A WireSpace is **not** claimed here as a security or memory-protection boundary; it is the scope within which identity is meaningful and uniqueness is checkable. Most systems are one WireSpace and can ignore this section. What a translating gateway would look like is `FUTURE §12`.

---

# 5. `kLocalBus`, Link Bindings, and Constrained Profiles

`kLocalBus` is a reserved **canonical local-only WireNumber**. It exists for low-configuration bring-up and permanently local Link use without weakening the universal canonical-ingress rule.

An ingress LLL bound to `kLocalBus` produces a complete canonical PDU:

```text
Wire                  = kLocalBus
SrcHostId      = reconstructed ordinary HostId
DestHostId     = reconstructed ordinary HostId or kBroadcastHost
Endpoint / class data = canonical values
```

There is no anonymous or partially canonical LocalBus state. A valid local source HostId remains required.

## 5.1 Binding and scope

A Router/Endpoint Domain may bind `kLocalBus` to **at most one local Link Interface at a time**. This keeps an autonomous `send(..., Wire = kLocalBus)` unambiguous without selecting a Link from traffic history, interface order, or heuristics.

`kLocalBus` may:

- enter through its one bound Link Interface;
- be dispatched to the destination local Endpoint Domain;
- use ordinary Endpoint storage and validation;
- be transmitted on that same bound Link when authorized.

`kLocalBus` must not:

- be transparently forwarded to another Link Interface;
- be spliced as `kLocalBus`;
- be promoted into a named Wire because traffic was observed;
- be used without a valid ordinary source HostId.

To extend the same Service relationship beyond this local Link, configure an ordinary named Wire. Composition may consume local traffic and author new traffic on another Wire, but that is visible re-origination with the composition Domain's source HostId, not transparent forwarding.

## 5.2 Carrier elision and host projection

Carrier compression is expressed through the Link Binding:

- **field elision** omits a canonical field because the binding or frame context supplies the same value unambiguously;
- **host projection** translates a constrained Link-local host code through configured state to a canonical HostId.

Projection is not aliasing a Wire and is not a new host universe. Every ingress path reconstructs the deployment-canonical HostIds before Router or dispatch processing. Every egress verifies that the canonical PDU is representable under the selected Link Binding before emitting a frame.

**CAN11 update boundary:** Native VCN changes deploy under a spare immutable alias (`LINK §2.14`); ordinary traffic never changes an active alias in place. Guest map cutover and exact retirement, drain/flush, and stale-frame exclusion mechanisms remain open. Other constrained profiles must define their own update boundaries before supporting live changes.

## 5.3 CAN11 qualification

CAN11 uses one addressing model, **VCN + Link-local Direction**, with Guest and Native encodings (`LINK §2`). A VCN names a configured Host relationship, not an Endpoint or a route above the LLL.

Guest VCN owns an explicitly allocated CAN-ID block. Its Link Binding supplies one Wire and fixed QoS; Guest VCN relationship meanings are coordinated throughout the deployment. Guest-4 is the initial 16-ID profile; Guest-5/6 are growth directions.

Native VCN uses the preferred provisional `QoS[2] + WireAlias[3] + VCN[5] + Direction[1]` identifier. Each active WireAlias selects exactly one canonical Wire and one VCN mapping. Several aliases may represent different Wires, or the same Wire during mapping migration. Thus one native CAN11 interface may carry several Wires.

An active alias binding is immutable. Replacement uses a spare alias, with explicit migration and deterministic TX selection; the frame's alias isolates representation-dependent reassembly. This changes Link representation, not Wire identity or forwarding. `kLocalBus` may be the initial alias-0 binding only under §5.1; it remains local-only even though alias 0 may later be assigned a named Wire under the migration rules.

The former separate Compact/General Host-compressed format is outside the baseline. It is retained only as an experimental comparison requiring topology evidence (`REG §6.8`). CAN29 is preferred when VCN capacity, alias capacity, migration headroom, or configuration cost makes CAN11 unsuitable. Exact control reservations, framing, CRC, and commissioning remain `LINK` work.

## 5.4 Small LocalBus-only hosts

A tiny host may permanently use one `kLocalBus` Link Binding, one Endpoint Domain, a fixed HostId, and generated direct dispatch. It need not implement generic forwarding, a dynamic Wire database, or host projection.

Being commissionable is independent of being routable. Management may assign or update its HostId and Link Binding without requiring gateway behavior. Once configured, the LLL still reconstructs the same complete canonical PDU seen by richer implementations.

---

# 6. Wire Splicing

## 6.1 Semantic definition

A **Wire Splice** is a configured Wire-scope projection between two canonical WireNumbers. It makes two differently numbered local scopes behave as segments of one intended communication domain while preserving host identity and all PDU semantics except the deliberate Wire scope change.

```text
Wire X, Hosts 0x12 / 0x27
            <------ configured splice ------>
Wire Y, Hosts 0x12 / 0x27 / 0x41
```

> A splice is **not** host translation and is not application-level composition. Only Wire scope changes. Source HostId, destination HostId, Endpoint identity, class, extensions, payload, and lineage are preserved.

The HostId universe must already be coordinated across both sides. A splice does not merge independently assigned identity universes and does not resolve HostId collisions.

## 6.2 A splice applies before egress

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
egress LLL encodes Y under its Link Binding
    |
transmit
```

So internally the traffic is Wire X; what leaves the device is Wire Y. The device-private scope rule in §4.2 is not weakened, because it is evaluated after the splice, on the representation actually emitted.

On the receive path the mapping is applied in the mirror position, after the ingress LLL has reconstructed the complete canonical PDU and before routing:

```text
external frames
    |
ingress LLL
    |
reconstruct canonical Wire Y, source, destination
    |
splice applied: Y -> X               <-- before Router
    |
Router / local delivery on X
```

Rules:

- A splice is applied **once** per local routing step. Chained or recursive splices, where the output of one splice is re-resolved through another, are not permitted in the base model. The simplest implementations should not re-enter routing after applying a splice.
- Complete ingress canonicalization always happens before splicing. Egress Link encoding happens after splicing. The LLL deals in Link representation; the splice deals only in canonical WireNumbers.
- The scope validity check (device-private vs network-visible) is evaluated on the representation that crosses the boundary, not the one the producer used.

**Open:** whether a splice is best expressed as one bidirectional mapping in the Wire/route entry, or as two directional route entries. A single bidirectional mapping is easier to validate and harder to configure asymmetrically by accident; two entries are more uniform with the rest of the route table. This should be settled by the first gateway implementation.

## 6.3 `spliceWire` implementation concept

A routing-table entry may carry an optional field conceptually named:

```text
spliceWire
```

This is the implementation mechanism for changing the local Wire representation as a PDU crosses the splice. See §11.3 for its position in the route entry.

## 6.4 What a splice preserves

A splice preserves:

```text
SrcHostId
DestHostId
Namespace
EndpointId
TransportType
QoS
header extensions
payload
source lineage
```

Only the Wire representation changes, as required by the local topology. This is the carve-out named in §3.3.

## 6.5 Intended initial use

The common and easiest case is:

```text
populated Wire
    <splice>
empty/new Wire segment
```

which minimizes identity and topology conflicts.

Splicing two already-populated segments is not fundamentally impossible, but management policy remains provisional. Conservative tooling may initially reject it unless it can validate:

- one coordinated HostId universe across both segments;
- no HostId collision or translation;
- compatible source/destination and Endpoint expectations;
- no forwarding loops.

## 6.6 Validation responsibilities

Host tooling, not the runtime data plane, should establish before installing a splice:

- both sides have canonical Wire and host identity;
- host identity assignments are coordinated across the combined scope;
- the combined realization remains acyclic (§12.4);
- both Links can represent the resulting PDUs (§17);
- the external side is a network-visible WireNumber, so nothing device-private is emitted.

If several devices splice device-private Wires onto one shared external Wire, their HostIds must already be deployment-coordinated. Giving each device its own external Wire avoids accidental scope merging and is the recommended default during bring-up.

Tooling should reject a splice that would merge independently assigned HostId universes. Identity coordination or explicit composition must happen before a splice can be valid.

---

# 7. Default Internal Debug Wire

A conventional use of device-private Wires and splicing is a **default debug/maintenance path**: Services publish to a device-private `InternalDebugWire`; an explicitly configured splice before egress makes selected traffic visible on a host-facing Wire. Without a splice, device-private traffic stays on the device by construction.

The splice is the trust boundary where private traffic becomes externally visible. A remote maintenance link should not blindly splice every internal Wire (§22). Host tooling conventions, default `debug_tx` bindings, and the telemetry path are in `DEPLOY §3.2`–`§3.3`.

**Open:** whether `InternalDebugWire` receives a standard reserved device-private WireNumber, or remains a per-deployment convention (`REG §6.2`).

---

# 8. Endpoint Namespaces and Address Allocation

## 8.1 Preferred provisional partition

```text
Namespace       2 bits
EndpointId     14 bits
```

Under the current preferred candidate these fields share a 16-bit `Endpoint` word. The 2/14 partition and resulting 65,532 usable Namespace/EID combinations are provisional pending topology/headroom evidence; the accepted semantic field is `Endpoint`. `EndpointId == 0` remains invalid in every Namespace.

## 8.2 Namespace allocation

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

Namespaces 0-2 provide deployments with large independent user spaces and room for staged upgrades, migrations, or coexistence between generations. Namespace 3 treats the open-source ecosystem as a first-class audience rather than an afterthought. Its registry *process* is `FUTURE §7`; the allocation above is the working plan.

Constrained-Link encoding limits and Namespace 0 compact Endpoint allocation are profile and ecosystem policy (`LINK`, `FUTURE §7`, `FUTURE §8`). `CORE` records the current preferred provisional partition in §8.1–8.2.

---

# Part III — Host Runtime

# 9. Endpoint Domains and Dispatch

An Endpoint Domain owns a set of local Endpoints and provides their dispatch boundary.

```text
canonical PDU
    |
Router selects destination Endpoint Domain(s)
    |
Endpoint Domain Dispatcher
    |
(Namespace, EndpointId)
    |
Endpoint
```

## 9.1 Dispatcher responsibilities

The Dispatcher should:

- verify that `DestHostId` is this Domain's HostId or broadcast;
- resolve `(Namespace, EndpointId)` to a registered Endpoint;
- offer the complete PDU/message to that Endpoint for acceptance;
- reject or count unknown EIDs;
- reject malformed/unsupported local delivery;
- provide diagnostics/status for delivery errors;
- support `kLocalDomain` and locally bound `kLocalBus` delivery.

A richer implementation may also validate expected TransportType, allowed message sizes, source/destination bindings, and local Service state.

The Router selects local destination Domains before dispatch. For an ordinary destination it selects the one local Domain with that HostId. For `DestHostId == kBroadcastHost`, it performs bounded fan-out to the configured local host Domains on that Wire. The Dispatcher does **not** decide cross-Link routing, and it does **not** execute Service code (§9.4). Local delivery being "fast" means it may traverse no framing, not that it skips the storage boundary.

## 9.2 Endpoint registration

The exact API remains implementation-specific. Expected implementations include static generated tables, fixed arrays, compile-time registration, and host dictionaries/maps on larger systems. The embedded baseline should avoid dynamic allocation requirements.

## 9.3 Delivered metadata

A received message should have enough context to identify its author and addressing unambiguously:

```text
WireNumber
SrcHostId
DestHostId
Namespace + EndpointId
```

`kLocalBus` uses the same canonical metadata. There is no anonymous source form: ingress reconstructs a valid source HostId before dispatch.

An Endpoint therefore holds **more than a payload.** The consumer can read, where the Endpoint declares it:

```text
address       WireNumber, SrcHostId, DestHostId
class         QoS, TransportType
extensions    presence, and access to recognized extension content
arrival       the acceptance timestamp or tick (§9.4)
```

`Namespace` and `EndpointId` are the Endpoint's own identity and need no per-message storage.

### Metadata survives acceptance through explicit ownership

> **A borrowed view into ingress storage cannot survive the storage boundary.** In the copy baseline, metadata and retained payload are copied at acceptance before the ingress buffer may be reclaimed. A reference path instead transfers an explicit owned handle or bounded immutable lease whose lifetime includes later processing; the backing storage cannot be reclaimed while retained (§16). A raw pointer or span alone does not establish that lifetime.

Opaque read-only accessors are backed by copied fields or explicitly retained immutable storage, never by an expired ingress borrow.

### Metadata is declared, because it is not free

The per-message cost is small but not nothing. The base addressing and class metadata varying at one Endpoint can be represented in **4 bytes** (`Control`, Wire, source, destination), since the Endpoint's own identity is constant, and an acceptance timestamp adds typically four more. Around 8 bytes per slot before extension storage and alignment is a fair estimate.

That is negligible on a 512-byte bootloader segment and roughly a doubling on a queue of 8-byte commands, which is exactly the profile where storage was already the constraint (§9.5). So metadata follows the same rule as everything else here — **declared per Endpoint as an immutable property of the Service definition**, not carried unconditionally:

```text
payload only      no per-message metadata storage
with source       source identity, enough to distinguish and answer peers
full              source, class, extensions, arrival
```

A statically wired single-peer state consumer needs none of it. A Service that validates by source, answers requests, or reasons about staleness declares what it uses and pays for that. Names here are not frozen (`REG §6.12`).

One asymmetry is deliberate: **a Snapshot Endpoint always carries arrival time**, because latest-value semantics without a time basis cannot support freshness at all (§21.4), and a Snapshot is the storage class whose whole purpose is holding state that can go stale. On a Queue, arrival time is declared like the rest.

### Reading a source is not permission to answer it

Source metadata is informational. It is not a transmit capability:

> **Reading source metadata confers no transmit authority.** A reply is authorized by the receiving Endpoint's registration, never by the fact that a message arrived carrying a Wire and source HostId the Service can read.

The distinction is between two different objects. Metadata is readable and inert. A reply context is opaque, bounded, single-use, and revalidated at transmit (§10.6). A Service that remembers a peer in order to answer it later (§10.2) retains a reply context, not a copy of the source fields it read — and where both exist, the source fields may only *select* among peers the registration already permits, never widen that set.

## 9.4 Delivery crosses a bounded storage boundary

Delivery to a local Endpoint is a storage operation, not a call into the destination:

> **Endpoint delivery never synchronously executes Service application code.** The Dispatcher resolves the destination and asks that Endpoint to accept the message; the Endpoint performs only bounded framework-controlled work. The owning Transport Entity processes what was accepted later; Service consumption and callbacks run in that execution context or a subsequently scheduled Service context.

```text
canonical PDU
    |
Router                          caller's context
    |
Endpoint Domain Dispatcher      caller's context
    |
Endpoint / Transport ingress    acceptance ends caller-context delivery
    |
    |   later, in the owning execution context
    v
Transport Entity processing
    |
Service
```

Routing itself stays synchronous and cheap. A Link RX path still calls the Router and the Dispatcher directly against read-mostly tables (§11.2), and no central networking task is introduced. The line is narrower than "no synchronous work":

> **Caller-context routing is permitted. Caller-context execution of destination Service code is not.**

### Why the boundary is mandatory rather than optional

`Inline` delivery (withdrawn) made a Link's worst-case execution time depend on every Service that might be delivered to — unanalyzable and deployment-dependent. Bounded storage fixes that:

- modular WCET for Link RX paths;
- coherent model across firmware, host, and RTL (no callback into an RTL Endpoint);
- traded latency: consumer scheduling delay enters the loop; same-loop Link service then Endpoint drain minimizes extra delay (`CONFORM §3`).

Rationale and history: `HIST §4.2`; full decision record in [`archive/decision-bounded-endpoint-delivery.md`](archive/decision-bounded-endpoint-delivery.md).

### What acceptance includes

Acceptance is framework work, and two parts of it are required rather than optional.

**Arrival time is captured at acceptance** wherever it is carried at all. With consumer latency now inside the delivery path, a Service can no longer distinguish "produced late" from "consumed late" by observing when it dequeued something, so recording it later is worthless. Freshness handling (§21.4) depends on it — a timestamp or a monotonic tick, per the platform's available time base. Snapshot Endpoints always carry it; a Queue declares it with the rest of its metadata (§9.3).

**Acceptance may classify, but does not execute protocol semantics.** A framework receive hook may check configured connection identity and a bounded fixed transport prefix, choose one declared storage element, copy the required fields or transfer an explicitly owned packet reference, update counters, and return. Length validation precedes prefix access. It does not process ACKs, advance windows, interpret duplicates, execute SETUP/completion transitions, run retries, semantically coalesce protocol control, or call Service code. Flash programming is outside this path.

Protocol processing happens later through a bounded operation in the Transport Entity's serialized execution context (§20). An optional reference path must obey §16; the copy baseline remains valid. A receive hook is a framework acceptance mechanism, not an application callback registration API.

## 9.5 Endpoint storage semantics

Every Endpoint exposes **one Transport Entity boundary** and declares the bounded storage behind it. A simple datagram Endpoint uses one storage element. A compound Transport may use several when its protocol requires distinct ingress classes, with an explicit classification rule and a bound and exhaustion policy for every element. Their semantics are fixed by the Service/Transport definition, not silently substituted by deployment.

For example, the BITS proposal uses a non-overwriting segment mailbox or Queue and one FIFO shared by protocol control and user sideband datagrams. This is two ingress workstreams behind one Endpoint, not two Endpoint identities or two independent Transport bindings. Classification preserves FIFO arrival order within a workstream; it does not imply a total processing order across workstreams. Detailed BITS protocol and wire-format integration remains separate work (`REG §6.16`).

Two storage models are defined:

A storage element holds **declared metadata plus payload**, or an explicitly owned reference preserving their lifetime, not an unscoped borrowed view (§9.3, §16). Where working names below write a payload type, that type is the payload representation only; it is not the whole slot, and it is not the wire contract (`SVC-7`).

### Queue

History-preserving delivery. Accepted messages are retained in order in statically bounded storage, capacity is known before runtime, and acceptance is non-blocking from the caller's perspective. When no storage is available the message is rejected, and **`Full` is an ordinary bounded-resource outcome, not a fault** (§14.4). Logical capacity means usable slots; no implementation is required to sacrifice one to distinguish full from empty, and the internal representation — a count, monotonic counters, anything bounded — is not part of the contract.

Queues carry events, non-idempotent commands, requests, relative motion, log records, and anything where intermediate values matter. `MoveRelative(+10)` three times is not `MoveRelative(+10)` once.

### Snapshot

Latest-value delivery. The Endpoint holds one coherent complete value, a newly accepted value replaces the previous one, and intermediate values are intentionally coalesced. A **generation counter is required, not optional** — it is the only mechanism by which a reader can distinguish "no new value" from "I missed values," and a Snapshot with no generation is indistinguishable from one whose producer died. A seqlock-style optimistic read is the expected implementation where the value is too large to read under exclusion.

Snapshots carry sensor values, estimated pose, desired actuator state, operating mode, configuration, and setpoints. `SetSpeed(1000, 1100, 1200)` may legitimately be observed only as `SetSpeed(1200)`.

### Choosing between them

The test is semantic, and message naming does not decide it:

> **Snapshot is valid only when processing the newest complete value, without processing every intermediate value, preserves the intended application semantics.**

`SetValvePosition(37%)` is a command that represents state and may use a Snapshot. `AdvanceValve(+1 step)` is a command that represents an event and requires a Queue. This is `OWN-5` at the Endpoint: the two are not interchangeable, and substituting one loses a property the Service was relying on with no counter and no error.

### Capacity is per storage element

Depth is declared for each storage element behind each Endpoint, never globally, because storage multiplies across Services: ten 48-byte queues of depth three already commit about 1.4 KiB to payload slots before transmit storage. A bootloader can need a depth-1 segment mailbox holding one large block alongside a depth-4 FIFO holding small control/sideband packets. A compound Transport can place both behind one Endpoint; separate datagram Endpoints remain valid when the Service defines separate interactions.

Two consequences of a shallow queue are worth naming. **Depth is a statement about the application protocol:** a depth-1 Endpoint asserts that the protocol is lock-step, since a peer that sends block `N+1` before block `N` is consumed will have every other block rejected. That is what bootloaders do anyway, and a rejected block must not advance transfer state (`QOS-9`). And the segment case is where the copy-based baseline costs most — a fragmented block is reassembled in an LLL context and then copied into the Endpoint slot, so the precious buffer exists twice on the target least able to afford it. That is the strongest concrete motivation for the eventual ownership-transfer path (`FUTURE §2.1`).

Whether a small documented default capacity is offered for convenience, or every Queue must spell out its depth, is open (`REG §6.12`).

### Overload behavior

```text
Queue      try_deliver -> Accepted | Full
           diagnostics: accepted, rejected_full, high_water_mark

Snapshot   try_deliver -> Accepted (replacing)
           diagnostics: updates, replacements, generation
```

A Snapshot has no full condition; successful replacement is successful delivery under its declared semantics. It does have a silent-loss mode, which is what the generation counter and the replacement counter exist to expose.

## 9.6 Producer and consumer multiplicity

Storage semantics and concurrency are separate dimensions, and both are immutable properties of the Service/Transport definition. Because they are fixed at definition time rather than configured per deployment, there is nothing for tooling to preserve across a placement change — the guarantee is structural rather than checked.

| | Writers | Readers |
|---|---|---|
| **Receive Endpoint** | framework only: one or several Link drivers and local producers | one Service (Queue) or any number (Snapshot) |
| **Transmit Endpoint** | exactly one Service, optionally several contexts within it | framework only: the one bound Wire path |

Stated as invariants:

> **Exactly one Service may write a transmit Endpoint, and exactly one Service may read a Queue Endpoint. Any number of Services may read a Snapshot Endpoint.**

> **For every externally producing `(Endpoint Domain, Namespace, EndpointId)` there is exactly one externally visible producer Endpoint identity.** Configuration must reject two Endpoint implementations in one Domain claiming the same producing identity.

For a compound Transport, the owning Transport Entity consumes each protocol ingress queue on behalf of its Service. It is the sole consumer of that queue; the Service does not independently drain the same queue. Protocol processing and Service consumption may therefore be separate stages without creating a multi-reader Queue.

The first rule makes the second enforceable by ownership at configuration time instead of resting on trust in synchronized writers. An Endpoint address is not a shared mailbox any local component may publish under. Without this rule, two Services in one Domain can both emit under the same HostId and Endpoint identity, and a receiver cannot tell which authored a value.

Note that framework producers are not Services. Several Link drivers delivering into one receive Endpoint is normal and is exactly what the multi-writer case exists for — the AMP arrangement in §13.2 relies on it. Similarly, several execution contexts *within* one Service may write a transmit Endpoint under the Domain's serialization (§1.5); they remain one Service and one external producer.

Two rules were deliberately dropped. **Multi-reader Queues are not supported:** draining is destructive, so two consumers silently split the stream, which works in test and loses messages in production. It is a worker-pool construct with no meaning in RTL. And **Link fan-out does not happen at the Endpoint** — the Router realizes one bound Wire across its configured Links. Publishing semantically on several independent Wires uses separate transmit Endpoints or explicit composition; a splice only projects Wire scope (§6).

The reason multi-reader Snapshots are safe where Queues are not is that reading is non-destructive and each reader tracks its own progress. That implies an implementation rule worth stating, because the obvious first attempt gets it wrong:

> **The "have I seen this" watermark lives in each reader, never in the Endpoint.** A `new_data` flag that the Endpoint clears on read works perfectly with one consumer and silently starves the second.

Finally, dispatch changes nothing about any of this. **The Dispatcher does not silently make an Endpoint safe for arbitrary concurrent access;** wiring and generated code preserve the declared semantics and neither add nor remove them.

## 9.7 Dispatch table synchronization and Endpoint lifetime

The Dispatcher can use the same read-mostly design philosophy as the Router (§11.2). A capable multicore system may use seqlocks or snapshots; a small static MCU may use a small `const` array with linear search and no lock.

A subtle lifetime issue exists if a runtime update can remove or destroy an Endpoint while another thread has just read its callback pointer. Early implementations should prefer stable Endpoint registrations, or adopt a safe publication/lifetime mechanism before introducing runtime Service replacement.

## 9.8 Endpoint naming is not authority

Endpoint identity and communication authority are separate, and conflating them is the single easiest way to build an accidentally open system.

> **Endpoint identity provides naming. Wires, Endpoint bindings, and typed local access provide authority.**

Knowing a `(Namespace, EndpointId)` does not imply that every local component may invoke it, that every remote host may reach it, that every Wire may carry it, that it accepts arbitrary concurrent producers, or that physical receipt makes a message valid (§12.6).

Two consequences are practical rather than philosophical.

First, **a Link or a Wire reaching a device does not expose that device's whole Endpoint namespace.** Membership on Wire 42 permits the traffic that Wire 42 is configured to carry, not arbitrary access to every Endpoint the peer happens to host. The Dispatcher still rejects and counts what it was not configured to accept (§9.1).

Second, ordinary application components should hold **typed handles for only the operations they may perform**, rather than a general-purpose escape hatch:

```cpp
// what a Service should hold
TransmitEndpoint telemetry_tx_;

// what it should not hold
router.send(arbitrary_endpoint_id, arbitrary_payload);
```

This is worth doing and worth being honest about: typed handles are capability-*like* API discipline, not isolation. Code that can reach the Router or shared memory directly can bypass them. They prevent mistakes, not attacks — §22 remains the security position.

---

# 10. Transmit Endpoints

Receiving naturally tells a Service where a packet came from. Autonomous transmission does not. WireSpaces therefore needs explicit **transmit Endpoints**, preferably injected into the Service.

For simple datagram storage, a transmit Endpoint has the same shape as a receive Endpoint with the roles reversed. The storage and concurrency axes of §9.5 and §9.6 apply; only the identity of the writer and the reader swaps. Compound Transport APIs may add submission and protocol-processing stages while preserving declared ownership and bounds. That symmetry is the reason no separate local-interface concept is needed on either side (§1.6):

```text
receive Endpoint    framework writes, Service reads
transmit Endpoint   Service writes, the bound Wire path reads
```

## 10.1 Reply-only Service

A Service that only replies can use the receive context:

```text
request:
    Wire = 12
    SrcHostId  = 0x27
    DestHostId = local HostId

reply:
    Wire = 12
    SrcHostId  = local HostId
    DestHostId = 0x27
```

No separate static output Wire is required. A reply is a newly authored PDU: it uses the responding Endpoint Domain's HostId as source and the validated request source as destination. It does not preserve the request source or reverse a Direction field.

## 10.2 Remembered peer/session

A Service may explicitly retain a recent reply/destination context:

```text
client contacts Service
    |
Service stores destination context
    |
later publishes progress/status to that peer
```

This is Service policy, not an implicit WS global behavior. Because it is the one mode where transmit context comes from *traffic* rather than from configuration, it carries obligations the other modes do not; they are in §10.6.

What the Service retains is a reply context, not the source fields it read from delivered metadata. Those fields are readable and inert, and reading them authorizes nothing (§9.3).

## 10.3 Autonomous transmission

A Service that transmits on its own initiative needs a configured destination Wire and destination HostId (or broadcast). Rather than hard-code those into reusable Service logic, the preferred architecture is an injected handle:

```cpp
class TemperatureService {
public:
    explicit TemperatureService(TransmitEndpoint telemetry_tx);

    void tick();

private:
    TransmitEndpoint telemetry_tx_;
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

The Service's source code names its outputs; the Wiring names the Wire, destination host, and Endpoint each one becomes. The source is always the HostId of the Endpoint Domain that owns the transmit Endpoint.

## 10.4 Transmit Endpoint storage semantics

A transmit Endpoint declares Queue or Snapshot storage exactly as a receive Endpoint does, and the choice determines what a send means.

**Queue transmit** is the explicit-acceptance path for events, commands, and requests. The Service submits, the Endpoint accepts or rejects against bounded capacity, and the bound Wire path drains it later. Acceptance means the transmit path took ownership, never that anything reached the medium (§16.1), and a rejected submission must not advance protocol state (`QOS-9`).

**Snapshot transmit** is the publication path for periodic state. The Service writes current state whenever convenient and the LLL samples it on its own schedule (§1.7). There is no queue, no per-send copy, and no accept/reject — coalescing is the declared semantics rather than a loss, and this is what periodic telemetry on a rate-limited Link actually wants.

Snapshot transmit changes what the ownership rules are about, which is worth stating precisely: the Service's write is a publication, not a submission, so `OWN-1` and `OWN-4` govern the PDUs the LLL *generates from samples*, not the writes that were designed to be overwritten. Nothing about a publication reaches a terminal outcome, because a publication is not a PDU.

### Confirmation and staleness on Snapshot transmit

A Snapshot transmit Endpoint has no natural failure signal, and that is a genuine hazard rather than a theoretical one. A Queue transmit Endpoint that is unbound, unscheduled, or attached to a dead driver fills and starts rejecting, so the Service finds out. A Snapshot transmit Endpoint that was never bound to a Wire behaves *identically to a working one*: the Service publishes, publishing succeeds, and nothing ever leaves the device.

The Endpoint therefore publishes back what the framework has done with it:

```text
last_sampled_generation   the generation most recently read by the LLL
last_sent_generation      the generation most recently confirmed transmitted,
                          where the Link can report TX completion (§17)
```

These reuse the Snapshot generation counter rather than adding a mechanism, so a Service compares them against its own last write with no correlation state: the delta is directly how far behind the wire is, and a counter that never advances is an unbound or dead path. Where a Link cannot report completion, `last_sent_generation` is simply unavailable rather than faked.

Both are **16 bits and wrapping** by default, narrowable by profile on constrained targets. Two rules come with that and must be honored or the mechanism silently misleads:

- **comparison is modular, never ordered** — the usual signed-difference idiom, not `>`;
- **the delta is meaningful only if read more often than the counter wraps.** At 16 bits and a 1 kHz publisher that is roughly a minute; at 8 bits it would be a quarter of a second.

Name these for what they measure. `sampled` and `sent` are honest; anything called "confirmation" would contradict `OWN-1`, since a sample can be built into a PDU and then lose its CAN mailbox without ever reaching the medium.

Queue transmit Endpoints deliberately get no equivalent mechanism, because accept/reject already carries the same information.

### One Wire per transmit Endpoint

> **Every transmit Endpoint has exactly one consuming Wire binding, including its allowed destination host selection.**

For Snapshot this keeps the sampled and sent generations single scalar fields instead of per-reader state. For Queue it follows more strongly, since two readers draining one queue is the same destructive-split problem that rules out multi-reader receive Queues (§9.6). One Wire may fan out across several Links through Router forwarding. Publishing onto several independent Wires uses separate transmit Endpoints or explicit composition.

## 10.5 Defaults and safety

Useful defaults reduce configuration during bring-up:

- one canonical `kLocalBus` Link Binding (§5.1);
- the device-private Internal Debug Wire (§7), which is the natural default for `debug_tx`.

Safety- or control-critical application outputs should generally require explicit bindings rather than silently falling back to a development/debug route.

## 10.6 Binding modes

The preceding subsections describe three ways a Service obtains transmit context. Naming the full set as **binding modes** turns that into something checkable, because the mode determines what a registration is allowed to do and what it must bound:

| Mode | Where transmit context comes from |
|---|---|
| **Static** | generated configuration; a fixed Wire, Endpoint, and Transport, or a fixed bounded set selected by explicit Service policy |
| **Learned-from-ingress** | a bounded number of peer selections retained from validated ingress (§10.2) |
| **Request-scoped** | a reply context supplied with one validated request, valid for that exchange only (§10.1) |
| **Receive-only** | nowhere; the Endpoint consumes configured traffic and never transmits |
| **Transmit-only** | generated configuration; the Endpoint originates traffic and takes no context from ingress |

> **Each Endpoint registration declares exactly one mode.** An Endpoint that needs both request-scoped replies and autonomous publication uses **separate registrations** — one per mode, each with its own authority and lifetime. Collapsing them is what turns "may answer when asked" into "may transmit whenever."

Static, Receive-only, and Transmit-only need nothing further; their authority is entirely in the configuration. The other two are where the care goes.

### Request-scoped replies

WireSpaces makes replying structurally simple because the request already carries a canonical Wire and source HostId. An authorized reply context selects that Wire and source as the reply destination, while transmission writes the local Domain's HostId as the new source.

> **Reply authority is never inferred purely from received addressing.** A reply is authorized because the receiving Endpoint's registration permits it, not because a request arrived.

The practical requirements on a reply context are that it be:

- **opaque to Service code**, so a handler cannot retarget it at a different Endpoint, Wire, or host;
- **bounded** in lifetime and storage, with a defined expiry;
- **revalidated when transmission actually happens**, not only when it was created;
- **usable for exactly one exchange**, and unusable for a different operation or registration;
- **invalidated by restart and by configuration change.**

One lifetime trap deserves naming. If a response can outlive the handler that received the request — a slow query, a deferred computation, anything queued to another task — then holding a reference to transient ingress state is invalid, and the fact that it usually works makes it worse. Such a Service needs either a bounded retained reply handle whose lifetime is explicit, or a different binding mode.

### Learned-from-ingress bindings

This mode exists because it is genuinely useful: a Service asked to report progress needs somewhere to report it, and the requester is the obvious answer. But it is the only mode where **traffic influences future transmit behavior**, which makes it the only mode that can be steered by whoever can put a frame on the medium.

> **Learned-from-ingress binding is disabled on unauthenticated multi-access Links.** It may be enabled on a statically single-peer ingress, or under a named authenticated-source profile, and only within a preconfigured allowlist of permitted Wires, hosts, Endpoint identities, and Transports.

On a shared CAN bus with no authentication, anything that can transmit can present itself as the peer worth remembering. That is not a bug to be hardened against; it is what an unauthenticated multi-access medium means (§22), so the mode is simply not available there.

Where it is enabled, every learned binding defines:

```text
finite entry capacity
finite lifetime and expiry
deterministic replacement or eviction
behavior across restart: invalidated, or explicitly restored
auditable learn / replace / expire / reject / clear events
```

And four things it must never do, each of which would convert a bounded convenience into a routing mechanism:

```text
create a Wire or a route entry
broaden Endpoint authority beyond the registration
change how a Wire, Link Binding, or host projection is interpreted
authorize a Transport the registration does not permit
```

Observed traffic informs; the Endpoint binding defines what transmission is permitted. A Service that needs richer multi-party sessions keeps explicit bounded session state of its own, and still must not treat recent traffic as permission to transmit.

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

The smallest routing key is `WireNumber`. Where the forwarding topology depends on which branch supplied the PDU, use `(WireNumber, ingress_link_interface_index)`:

```text
WireNumber
    or
(WireNumber, ingress_link_interface_index)
    ->
{
    egress_link_interface_bitmask
    local host-Domain membership set
    optional spliceWire
}
```

`DestHostId` is not an ordinary forwarding key. It controls acceptance. A Router may use destination information for safe early filtering or an equivalent optimization only when the result is behaviorally identical to propagating on the configured Wire and filtering at hosts.

For local delivery, the Router compares an ordinary destination with local Domain HostIds, or expands broadcast to the bounded configured set of local host Domains on that Wire. The Dispatcher then resolves Endpoint identity within each selected Domain.

The API does not need a generalized `RouterPort` type. Link ingress is identified by Link Interface; local Endpoint-originated routing can use local call context/domain information directly. `kLocalBus` is handled by its one local Link Binding and must never produce a generic cross-Link egress.

`spliceWire`, when present, rewrites only Wire scope as described in §6.2 — after ingress has reconstructed the complete canonical PDU and before the egress LLL applies field elision or host projection.

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

The generic gateway forwarding unit is a **complete canonical PDU**, including canonical Wire, source HostId, destination HostId, and source lineage.

```text
Physical-Link-native frames
    |
ingress LLL
    |
reconstruct Wire + source + destination
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
constrained-Link transfer units
    -> profile reassembly and canonical reconstruction
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

If Wire 42 spans several branches, the gateway may forward a directed PDU to every configured branch of Wire 42 and allow non-destination host Domains to reject it.

A more detailed host-location map may prune egresses by destination if bandwidth pressure justifies the complexity. That is an optimization, not a change to the generic forwarding key or Wire semantics.

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

Directed local delivery selects the one destination Domain by HostId. Broadcast local delivery is bounded fan-out to the configured local host Domains on the Wire; each Domain performs its own Endpoint dispatch and acceptance.

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
a configured host on a Wire carried by that Link
a legitimate recipient of PDUs addressed elsewhere
entitled to forward what it observed
entitled to transmit on that Wire
part of a broadcast recipient set
```

Membership comes from configuration — an assigned HostId, Wire membership, Link Binding, and installed forwarding entry — never from wiring or from what happened to arrive. The two ideas separate cleanly by level:

```text
realization level   a PDU may reach devices that are not its destination
semantic level      only the addressed host is a recipient
```

This is what makes §12.1 acceptable. Flooding is a bandwidth trade, not a widening of authority, and destination-aware pruning later changes efficiency without changing who may accept the PDU.

Two consequences are worth naming. First, a Domain that consumes directed traffic addressed to another HostId is misbehaving, even though nothing on a shared bus can stop it. Second, observation without configured membership is exactly what promiscuous mode (`DEPLOY §3.1`) is for, and that mode is deliberately restricted to host tooling and gateways and never creates Wiring.

The same rule applies to broadcast, which deserves its own list because it is the case most often over-read. `DestHostId == kBroadcastHost` addresses the configured host Domains of one Wire. It does not:

```text
mean every device electrically present on the carrying medium
imply a reverse Wire from any recipient
grant any recipient transmit authority on that Wire
discover its membership dynamically
```

Broadcast membership is configuration, established when the Wire is defined and changed only by reconfiguring it. Physical fan-out and semantic broadcast are independent in both directions: a shared medium may carry a Wire with exactly one intended recipient, and a broadcast Wire may be realized over point-to-point Links, several gateway branches, or a splice.

One forward-looking consequence, recorded here because it is cheap now and expensive later: **a point-to-point reliable Transport must not be applied to a broadcast Wire by sharing one acknowledgement state across recipients.** A Transport that serves multiple sinks has to define delivery, sequencing, duplicate, and error behavior for multiple sinks explicitly (§20, `FUTURE §3.2`). Where a Service needs per-recipient confirmation, use directed PDUs from each host, not a multi-target ACK.

## 12.7 Source authority and lineage

A receiver needs to know who produced a PDU, and no forwarding step may blur that. The producing identity of a received PDU is:

```text
SrcHostId                which host Domain authored it
Wire                            propagation scope
Namespace + EndpointId          Endpoint semantics
```

That identity is established by configuration, not by whatever the PDU passed through on the way. Every mechanism in this document preserves it:

```text
local dispatch              preserves it
gateway forwarding          preserves it (§3.3)
splicing                    preserves it; only the Wire representation changes (§6.4)
fanout to several egresses  preserves it in every copy (§16.3)
Link retransmission         preserves it
broadcast                   one producer across all recipients
local taps and observers    preserve it; observation changes nothing (§12.6)
LLL polling or scheduling   preserves it (§1.7)
```

The one thing that does not preserve it is **composition**. A component that consumes traffic and authors semantically new traffic — transforming, filtering, aggregating, re-timing, or translating identity — writes its own Domain HostId as `SrcHostId`. That is not forwarding; it needs its own Endpoint identity and configured binding, and it must be visible in the Wiring as a producer rather than hidden inside a forwarding entry.

A related rule that is easy to miss: **communication *with* a gateway is not communication *through* it.** Reading a gateway's Link telemetry, changing its forwarding table, or asking it to enumerate a downstream Link uses a separate Endpoint on a Wire where that gateway is a host. It does not ride the Wires being forwarded, and a device's presence in a forwarding path grants it no Endpoint on those Wires.

No separate runtime producer token is required for ordinary configured traffic, because the fields above plus ingress context and the installed configuration already resolve the producer.

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
Core 0 Domain (HostId 0x12)
    |
device-private ControlInternalWire
    |
shared-memory Link Interface
===============================
shared-memory Link Interface
    |
device-private ControlInternalWire
    |
Core 2 Domain (HostId 0x27)
```

The Router does not need special "core routing" concepts. If either Domain also participates on an external Wire, it uses that same HostId there.

## 13.2 Multiple Endpoint Domains

A multicore device can look like:

```text
CPU0 Endpoint Domain --\
                       shared internal Wires / Links
CPU1 Endpoint Domain ---+--> external Links as configured
                        |
CPU2 Endpoint Domain --/
```

Each Domain has its own Dispatcher and one deployment-scoped HostId. Routing tables may be shared across cores and read concurrently.

## 13.3 Inter-core queue serialization

The Router lookup itself can run concurrently, but an inter-core queue may have simpler concurrency constraints.

If the shared-memory primitive is SPSC, several approaches are valid:

- one SPSC queue per producer/domain;
- a local inter-core output task that serializes multiple local producers into one SPSC queue;
- an MPSC queue on platforms where its complexity is justified.

The protocol should not require one implementation. For high-integrity MCU code, one producer task per inter-core link can be attractive because it preserves a very simple shared-memory boundary even if it serializes that specific internal path.

### One serialized mutable context per LLL instance

The Router reads shared state concurrently (§11.2), but an LLL is the opposite case and the distinction matters:

> **Each LLL instance has exactly one logical execution context that mutates its parser state, reassembly state, transmit scheduling state, timers, and owned queues and pools.**

It may be a bare-metal loop, a dedicated task, or any explicitly serialized executor — but there is one of it. Other tasks and cores reach that instance only through bounded queues, immutable snapshots, or explicitly synchronized control requests. They do not call state-mutating LLL operations concurrently.

The reason is that reassembly is inherently stateful: a fragment only means anything relative to the context it belongs to, and two contexts advancing the same reassembly is not a race that produces a slightly wrong answer, it is a race that produces a plausible-looking PDU assembled from two different ones. Read-mostly routing tables tolerate concurrency because nobody is writing them; an LLL is being written constantly.

Read-only status and latest-value data may still be published concurrently through a coherent snapshot, a seqlock, or another documented mechanism. Note the direction of that dependency: **a seqlock requires a serialized writer.** It is a tool for publishing safely from one writer to many readers, and it does not create multi-writer ownership.

### Interrupt context

Where an LLL is fed by an interrupt, the ISR's job is narrow:

```text
acknowledge the hardware
capture one bounded receive unit or completion record
publish it to the owning context
wake that context
```

What does not belong in an ISR: parsing a complete PDU, Endpoint dispatch, arbitrary callbacks, blocking, and allocator or free-list manipulation. An implementation may define a stricter ISR-safe profile and verify it, but that is a local exception rather than a relaxation of the general contract.

Publication from ISR to task needs the platform's documented atomic and memory-ordering operations. **`volatile` is not a synchronization primitive** — it constrains the compiler's caching of a variable and says nothing about ordering between writes, about other cores, or about caches.

### Cross-core publication

Cross-core transfer needs an explicit publication boundary. The producer finishes every payload write and then performs any required cache clean or release operation; the consumer performs the matching acquire or invalidate before reading. Getting this backwards produces a queue that works on a coherent development part and fails on the target.

Whatever queue or mailbox implements the boundary must state:

```text
producer and consumer identities
ordering and atomic-width assumptions
cache and coherency requirements
full and empty behavior
what happens when either side resets with records in flight
```

The last one is the one usually left out, and it is the one that matters during a fault.

### Platform contracts that must be answered, not assumed

The mechanisms above depend on platform properties that a portable document cannot decide. These are **explicit gaps, not details that may be assumed away** — each platform adapter should document and test its answers before its cross-core or DMA-backed paths are treated as stable:

```text
minimum atomic widths, alignment, and lock-free assumptions
release/acquire operations and required interrupt or inter-core barriers
cache coherence assumptions for shared queues and snapshots
DMA clean/invalidate ownership transitions and descriptor ordering
whether shared packet storage may be cached
allocator lifetime and reclamation across a Link restart
handle generation / ABA protection where compact indices are reused
behavior when a producer or consumer resets with records in flight
whether borrowing across an asynchronous DMA boundary is permitted
MPU/MMU permissions for shared storage
required behavior on non-coherent multicore parts
whether diagnostic counters are atomic, serialized, or snapshot-copied
```

A single-core MCU can answer most of these with "not applicable," which is a fine answer and much better than an unexamined one.

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
Service A -> local Wire -> Service B
```

without changing the application-level message schema or Endpoint identity. The deployment/Wiring changes; the Service protocol need not.

Note what the last step is *not*. Collapsing two Services onto one MCU does not turn asynchronous message delivery into a function call; the destination Endpoint's storage semantics are part of its contract at every placement (§9.4). That is now structural rather than a policy tooling has to preserve, because there is only one delivery model and its semantics are fixed by the Service definition rather than the deployment.

What has to survive the move is the *externally visible* behavior: the same canonical PDU and Endpoint identity, the same Wire and binding, the same Transport interpretation, the same validation, explicit bounded acceptance and backpressure, and the same four-way distinction between **accepted, rejected, delivered, and consumed**. The mechanism underneath is free to change completely.

That last distinction is worth keeping sharp, because collapsing any two of the four states produces a Service that misreports its own progress:

```text
accepted   the local path took ownership of the PDU
rejected   it did not; nothing entered the path
delivered  it reached the destination Endpoint
consumed   the destination Service actually processed it
```

There is one optimization to refuse explicitly: **making same-core delivery reentrant when the equivalent cross-core path would be deferred.** It is tempting, since a direct call is obviously cheaper than a queue, but it changes when a Service's code runs relative to its own state — and a Service tested on one core and deployed across two then meets an execution model it was never tested against. The bounded storage boundary (§9.4) exists precisely so that this optimization has no legitimate form to take.

### Independence is bounded, not absolute

The claim above holds only inside a compatibility envelope, and stating it without the qualifier invites a real failure:

> **A Service is link-independent only within its declared size, timing, and transport compatibility.**

Three things can make an otherwise identical Service relationship unplaceable on a given Link:

```text
size        the PDU does not fit the Link's usable capacity
timing      the Link cannot meet the rate, latency, or cadence the Service needs
transport   the Link or profile cannot provide the required Transport or QoS
```

A Service exchanging 40-byte PDUs cannot move onto a constrained Link profile whose reassembled-PDU limit is smaller, no matter how unchanged its schema is. A control loop that works over shared memory may be unplaceable on a polled SPI Link whose cadence sets the floor on latency (§1.7). A Service that needs Critical QoS is not portable to a QoS-Minimal Link without an explicit decision about what happens to its priority.

The machinery to detect all of this already exists — Link capabilities (§17) plus static capacity checking (`DEPLOY §2.2`) — and the required behavior is unambiguous: an unrepresentable placement **fails at configuration or before TX**, never by truncation or silent degradation.

So the accurate framing is that WireSpaces removes *placement* from the Service's source code, not that every Service runs over every Link. The Service stops naming its Link; it does not stop having requirements.

## 13.5 No special many-core assumptions

Device-private Wire allocation remains provisional and should be validated against real topology corpora. A hypothetical very-large-many-core processor can justify an extended profile later rather than consuming canonical header bits before the need is demonstrated.

---

# 14. QoS Model

The canonical descriptor carries a 2-bit **QoS** value, providing four classes:

| QoS | Class |
|---:|---|
| `0` | Critical |
| `1` | High |
| `2` | Normal |
| `3` | Background / bulk |

The field is just called QoS in ordinary use. The reason `0` is the *highest* priority rather than the lowest is worth stating once, because it looks backwards until you know it:

> **QoS is the number of classes with strictly higher priority than yours.** Critical has none above it, so it is 0. Background has three above it, so it is 3.

Read that way the numbering is not an arbitrary convention to memorize — it is a count, and "lower number wins" follows from what the number means. It also lines up directly with how several Link technologies arbitrate. Exact mapping to Link-native priority is a profile concern.

Not every implementation must provide four separate schedulers/queues.

One boundary is worth stating before it gets blurred: **QoS is not flow control.** QoS decides which of the PDUs already accepted for a Link goes out first. It never decides whether a sender may produce, whether a receiver can absorb what arrives, or what happens when it cannot. Those are §15's subject, and they are separate contracts from local send admission (§14.4) and congestion reporting (§18).

## 14.1 Standard implementation profiles

| Profile | Implemented scheduling classes |
|---|---|
| **QoS-Minimal** | Normal only |
| **QoS-Full** | Critical, High, Normal, Background |

Intermediate profiles such as `Normal+Background` are intentionally not standardized unless implementation experience shows a real need.

A Minimal implementation may collapse canonical QoS values to Normal internally, or may reject unsupported QoS according to the eventual Link profile. Exact behavior remains to be frozen; a Service asking for behavior a Link cannot provide should not be silently misrepresented.

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

### Two named disciplines

Leaving inter-queue scheduling entirely open makes it impossible for a Service to state what it needs or for tooling to check that it got it. Two named disciplines cover the useful cases:

| Discipline | Behavior |
|---|---|
| `StrictPriority` | at every local scheduling opportunity, select the highest-priority non-empty eligible queue: Critical, else High, else Normal, else Background |
| `WeightedFair` | provide weighted progress among all continuously backlogged eligible queues while transmit opportunities continue |

`StrictPriority` is the simplest thing that respects QoS, and under sustained higher-priority load it will starve lower-priority traffic. That is a **property of the discipline, not a defect** — choosing it is choosing that behavior, which is often exactly right for a control bus.

`WeightedFair` must not intentionally starve an eligible lower-priority queue. Weighted round-robin or any behaviorally equivalent bounded scheduler qualifies. An implementation offering it declares its weight range and its **accounting unit** — weighting by frames and weighting by bytes produce very different outcomes when PDU sizes differ, and a Background bulk stream against Critical single-frame traffic is precisely the case where the difference shows.

Neither discipline provides a deadline, a latency bound, bus-wide fairness, or a real-time guarantee. Those require the deployment-level analysis in §15.2, and no local queue arrangement substitutes for it.

### What local scheduling can and cannot deliver

A local scheduler must preserve canonical QoS ordering among the PDUs it holds, and where the medium has its own arbitration it must account for it rather than fight it. Beyond that, three limits are real and should be designed around rather than discovered:

- **The in-progress unit is not preemptible.** Once a frame is handed to a controller and begins transmitting, a Critical PDU arriving behind it waits. On a slow CAN bus, one 8-byte frame is a meaningful delay, and a multi-frame PDU is several of them.
- **Controller capability bounds the achievable ordering.** A device with one transmit mailbox is a FIFO regardless of how many queues software maintains: a Background PDU already committed to hardware blocks everything behind it. Where the controller offers priority-aware selection, several pending objects, or safe cancellation and replacement, an implementation should use them to avoid *preventable* local priority inversion. Where it does not, the limitation should be documented rather than presented as QoS.
- **Local ordering is not a bus-wide guarantee.** Scheduling controls what this device offers to the medium. It says nothing about what other devices offer, so it delivers neither bus-wide fairness nor a latency bound. Latency guarantees on a shared medium come from deployment-level traffic engineering (§15.2), not from a local queue.

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

Latest-value replacement is the reason receivers need freshness handling (§21.4): a Link may legitimately discard a pending update in favor of a newer one.

### Replacement is only valid for traffic whose latest value is the whole message

There is a real constraint on which traffic a queue may coalesce, and it comes from the Service rather than the Link:

> **State may be replaced. Events, commands, and protocol state may not.** A newer temperature reading supersedes an older one and losing the older one costs nothing. A newer button-press event does not supersede an older one — replacing it deletes something that happened.

Which means **queue exhaustion must never quietly become latest-value replacement.** They are different outcomes and a Service can only be correct about one of them. If an event queue fills, the honest results are to reject, to drop with a counter, or to enter a declared fault state; converting the overflow into a replacement turns a countable loss into a silent one, and the receiver has no way to tell.

**Open detail:** blind replacement of the literal last queue entry can delete an unrelated Service's PDU. A robust latest-value implementation likely needs a stream/logical key such as `(Wire, SrcHostId, DestHostId, Namespace, EndpointId)` plus a per-Endpoint declaration of whether its traffic is replaceable at all. Resolve this in implementation rather than standardizing it accidentally.

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

### A rejected send must not advance protocol state

The corollary is a Service-side obligation, and it is the one most often missed:

> **A Service must not advance protocol state as though a rejected PDU had entered the transport.**

The classic instance is a sequence number or generation counter incremented at the top of a send function, before the call that fails. The Service now believes it emitted something it never emitted, and every subsequent message is misnumbered — a fault that first becomes visible at the receiver, far from its cause. Increment after acceptance, or retain the exact operation and retry it, or expose the rejection to the caller. What a Service must not do is treat "I tried" as "it went."

Two distinctions keep this reasoning straight:

- **Rejection is not a timeout.** Rejection means the PDU never entered the local transmit path; a timeout applies only *after* acceptance. Conflating them produces retry logic that waits for a response to a message that was never sent.
- **Rejection says nothing about the remote receiver.** Local acceptance reflects local bounded capacity only. It is not evidence that the peer has room, and remote capacity is the separate concern in §15.

### Every accepted PDU reaches exactly one terminal outcome

Acceptance means ownership transferred and the PDU entered the local path (§16.1). It does not mean transmitted, delivered, or acknowledged. What acceptance *does* promise is that the PDU will not simply disappear:

> **After acceptance, an implementation eventually reports exactly one of: transmission complete, cancelled by an explicit stop or restart, or a terminal local fault.**

Without that guarantee, buffer reclamation has no defined point and diagnostics cannot balance — every counter discrepancy becomes ambiguous between "still in flight" and "lost track of." The middle outcome is the one worth designing for deliberately, since restart with work in flight is normal (§23.4) and each of those PDUs needs its storage released and its loss counted.

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

Congestion is a bounded TX-resource outcome (§14.4), not a Wire property. Multiple Wires may converge on one slow egress Link:

```text
Wire A --\
Wire B ---\
Wire C ----+--> CAN_A TX queues --> bus capacity
Wire D ---/
```

Link drivers/interfaces are the authoritative source for queue occupancy, free TX storage, utilization, drop/reject counts, and link state.

## 15.2 Static prevention is the preferred serious-deployment mechanism

For controlled embedded systems, Services can eventually declare bounds such as:

```text
max frequency
max encoded PDU size
QoS
burst assumptions
```

Wiring tooling can project those claims across configured Wires and Links and estimate worst-case Link utilization (`DEPLOY §2.2`). The objective is not perfect network calculus in the first version; even conservative summation catches obvious impossible deployments.

> **Engineer normal traffic below capacity; use runtime backpressure and drops for bursts/faults, not as the primary steady-state scheduler.**

## 15.3 Optional hop-by-hop Link backpressure

Some Links benefit strongly from credit-based flow control.

```text
PC host
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

**CAN profiles:** any flow-control behavior is defined by the selected `LINK` profile; gateway upstream credit may still use §15.3.

**CAN FD / CAN XL:** may justify richer Link-level flow control in some profiles; still optional.

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

### Credit counts storage, not PDUs

One accounting decision can be settled now, because the alternative does not actually work:

> **Credit represents a bounded quantity of receiver storage, measured in a profile-defined fixed quantum.** It is not an unqualified count of PDUs.

Counting PDUs fails at the only moment it matters. If a receiver grants 8 PDUs of credit and PDUs range from 4 to 1500 bytes, it has granted somewhere between 32 bytes and 12 kB — so to be safe it must reserve for the worst case, which means the mechanism reserves 12 kB to admit 32 bytes of traffic. Granting quanta instead makes the grant mean the same thing to both ends regardless of what the sender chooses to send.

### Requirements for any credit scheme

A Link or profile that claims receiver-credit flow control satisfies all of the following. Most are ordinary bounded-state hygiene, but the last two are the ones that prevent a deadlock rather than a leak:

- a sender consumes sufficient granted credit **before** transmitting, and never exceeds, infers, or invents the peer's grant;
- credit is returned only according to defined receiver storage release and publication rules;
- counters, grants, updates, in-flight accounting, overflow, underflow, and reset behavior are finite and **fail closed on ambiguity**;
- credit eligibility is independent of QoS: a Critical PDU cannot bypass, manufacture, or reinterpret receiver credit (§14, `QOS-6`);
- a bounded **standalone** credit update exists, so a quiet peer can reopen a stalled data path without waiting for reverse user traffic;
- credit restoration and essential Link management have a **lifeline** path that does not depend on ordinary data credit.

The lifeline is the non-obvious requirement, and it exists because credit flow control has a natural deadlock. Suppose the sender has exhausted its credit and the receiver's credit update is itself subject to credit — now neither side can move, and the link is permanently stalled with both ends behaving correctly. A lifeline breaks the cycle by reserving receiver capacity that ordinary traffic can never consume.

Three properties keep a lifeline from becoming a loophole: its capacity is **permanently reserved** from ordinary traffic rather than borrowed from it, it **never manufactures ordinary credit**, and it **carries only Link control and credit restoration** — never application or bulk traffic. A "lifeline" that ordinary traffic can fill is not a lifeline.

Credit flow control is hop-by-hop. A gateway may reduce or withhold ingress credit as its forwarding storage fills (§15.3), and that is useful, but it is not a global congestion algorithm and not an end-to-end delivery guarantee.

## 15.6 Coarse backpressure is acceptable initially

A shared upstream Link may carry traffic destined for both a congested and an uncongested downstream Link. One coarse credit pool can therefore throttle unrelated traffic. This head-of-line/backpressure coupling is acknowledged.

Do not add per-Wire congestion signaling until a concrete system demonstrates that the coarse model is inadequate. More granular queues/credit domains can be an implementation extension later.

## 15.7 Every delivery path is bounded

The preceding sections describe bounds on Link queues. The general rule is broader and applies with no exceptions:

> **Every storage element on a delivery path has a bound fixed by implementation or configuration, and a defined behavior when that bound is reached.**

The bound must exist for all of them, including the ones that are easy to forget because they are not "the queue":

```text
Link TX and RX queues
Endpoint Queue and Snapshot storage (§9.5)
fragment reassembly contexts (LINK §2)
Transport retry windows and reassembly state (§20)
gateway forwarding buffers (§12)
observation, tap, and promiscuous-capture buffers (DEPLOY §3.1)
credit and flow-control accounting state (§15.4)
diagnostic counters and event storage (§18)
```

Exhaustion behavior is chosen per path and may be rejection, drop, backpressure, retry, overwrite of stale snapshot state, or fault reporting — but it is chosen, not emergent. "It allocates when it needs to" is not an exhaustion policy, and neither is unbounded growth under a fault storm.

Two corollaries follow directly. A gateway does not create implicit end-to-end reliability or unbounded buffering: it forwards within its configured bounds and drops or backpressures according to policy (§12.4). And an observation path never gains storage priority over a delivery path — a capture buffer filling up must degrade capture, never delivery.

Static configuration should validate that configured bounds are sufficient for declared rates and sizes wherever the deployment supplies enough information (`DEPLOY §2.2`).

---

# 16. Buffer Ownership and the Copy-Based Baseline

Copy-based operation is the preferred initial implementation strategy, and the architecture should make copying efficient rather than treating it as an inferior temporary mode.

This is especially appropriate for Classical CAN, CAN FD, small MCU datagrams, Endpoint storage, early multicore implementations, and most control/status traffic.

Zero-copy is deliberately not an initial focus; the eventual destination-owned direction is `FUTURE §2`.

## 16.1 Link send contract

A Link TX path should conceptually expose something like:

```cpp
SendResult send(PacketView packet);
```

For a copy-based profile, a successful return means the caller may immediately reuse or destroy the source buffer.

The underlying implementation may copy directly into hardware/controller memory, into a bounded queue, or into driver-owned storage.

This is the same call whose result taxonomy is described in §14.4: success means "admitted and your buffer is free," not "delivered."

This contract governs submissions. A Snapshot transmit Endpoint has no submission — the Service publishes and the LLL samples — so the ownership and terminal-outcome rules below apply to the PDUs the LLL generates from those samples, not to the publications themselves (§10.4).

### A view is not ownership

Even in a copy-based baseline it is worth separating two things that a `PacketView` conflates by looking like one:

```text
a view      states which bytes may be accessed
ownership   states who owns the storage, for how long, and who releases it
```

A byte span, a scatter/gather list, or a payload descriptor answers only the first. Every backing object has **exactly one owner at a time**, unless it is explicitly published as an immutable bounded shared lease. That gives two API shapes, and the choice should be visible in the signature rather than in the documentation:

- **Borrowed view** — immutable for one documented call or lease scope. The callee must not retain it afterward or mutate the storage. A borrowed submission may be *accepted* only if the implementation copies what it needs before returning, or documents a bounded lease extending through a later completion event.
- **Owned handle** — ownership transfers explicitly. The receiver may retain or enqueue it, and must eventually release or transfer it.

Three rules make ownership traceable, and the first is the one that causes real corruption when it is left implicit:

> **A rejected submission leaves ownership with the caller. An accepted owning submission transfers ownership exactly once. Mutating storage after publication is prohibited unless an explicit protocol transfers ownership back.**

The rejection case is where a caller most often either frees a buffer the callee is still holding or leaks one it assumed was taken. The rule has to hold for *every* rejection reason, including congestion (§14.4), which is the frequent path.

Two lifetime consequences are easy to miss:

- **Receive storage retained by application code must outlive a Link restart.** If a Service is holding a received buffer when its Link resets, that buffer must remain valid until the Service releases it — which means the allocator's lifetime is broader than the restartable Link state that filled it (§23.7). An allocator torn down with the Link produces dangling references in correct application code.
- **Reclamation goes through the allocator's defined owner.** Arbitrary contexts do not manipulate a shared free list, and an ISR does not (§13.3).

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

An implementation that wants to avoid the copies has exactly three safe options: complete the whole fan-out synchronously inside one borrow scope, use an immutable bounded shared lease, or copy. A lease has to state its maximum consumer count, how releases are tracked, who reclaims the storage, and what happens on restart or when the sharing bound is exceeded. Unbounded reference counting is not required and not encouraged.

> **Copying is the required-safe fallback whenever a target cannot prove bounded sharing.**

## 16.4 Storage semantics and ownership are separate axes

Above the Link, each declared Endpoint/Transport ingress element stores a PDU or decoded value under **three independent choices**, not one enumerated storage class:

```text
semantics      Queue (history-preserving) or Snapshot (latest-value)   (9.5)
ownership      value copy, or ownership transfer of a handle or lease
concurrency    single writer, or serialized concurrent writers        (9.6)
```

Keeping them separate is what makes the copyless roadmap tractable. An earlier version of this section listed four storage classes — snapshot, value queue, ownership-transfer queue, event queue — which conflated the first two axes and left "event queue" as a semantic duplicate of "value queue." Factored properly, moving from copies to handles is a change on the ownership axis alone:

> **Copyless delivery changes storage ownership, not Endpoint or Transport semantics.** A Queue stays history-preserving and bounded, a Snapshot stays latest-value, concurrency rules are unchanged, and full and replacement behavior are unchanged. Only what occupies the slot changes.

The strictness rule survives the refactoring and applies on every axis:

> **No position on any axis may masquerade as another.** A Snapshot replacement is not a Queue delivery. A value copy is not ownership transfer. A handle queue is not a borrow. And Queue exhaustion is never latest-value replacement (§14.3).

Each substitution silently removes a property a Service is relying on. Events, commands, and protocol state require non-replacing storage, so implementing an event stream over a Snapshot loses occurrences with no counter and no error; describing a value copy as ownership transfer leaves both sides expecting the other to release the buffer.

A Snapshot needs enough metadata for a consumer to judge what it is looking at: the required generation counter (§9.5), a producer restart epoch where restart matters, arrival time captured at acceptance (§9.4), and decode or integrity status where available. Without those, a reader cannot distinguish fresh data from a value that stopped updating an hour ago (§21.4). Replacing an unread value is normal and worth counting.

One last clarification, because it recurs: **a notification mechanism defines nothing about storage or ownership.** Being told that something arrived answers neither who owns the buffer nor how long it is valid.

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
Wire elision and host-projection capabilities
supported TransportTypes
nominal/usable link rate
polling cadence, for master-initiated Links (§1.7)
Link-specific limits such as CAN MaxN
```

Useful additions once implementations exist, mostly concerning how a PDU can be handed across the boundary rather than what fits in one:

```text
transfer-unit kind (byte span, frame, datagram, FIFO entry, DMA descriptor, ...)
maximum reassembled PDU size, where it differs from the transfer unit
alignment and contiguous-storage constraints
scatter/gather and zero-copy support
full-duplex, half-duplex, or simplex
ordering guarantees, and whether duplication is possible
link-provided error detection, if any
whether TX completion notification and timestamps are available
whether DMA, ISR production, cross-core access, or cache maintenance is involved
bounded queue depth or other acceptance limits
```

An LLL additionally declares its scheduling and flow-control position, **even when the answer is "none"** — an omitted field reads as an unmade decision, while an explicit "unsupported" is information:

```text
which QoS values it supports, and how they map to link-native priority
which scheduling disciplines it supports (§14.2), with queue bounds,
    weight range, and accounting unit where applicable
whether receiver flow control is absent or present; if present, its
    credit quantum, accounting rule, update path, and reset behavior
which local congestion observations and remote reports it provides
which Link facilities its scheduling claims depend on: multiple pending
    TX objects, priority-aware selection, preemption, safe cancellation
```

That last line is what makes a scheduling claim checkable. An LLL claiming `StrictPriority` on a controller with one transmit mailbox is claiming something it cannot deliver (§14.2), and naming the facilities it relies on is how that gets caught during integration rather than during a fault.

The purpose is practical:

- prevent routing an unrepresentable PDU onto a Link;
- validate auto-Wiring and splices;
- validate static configuration;
- support host inspection;
- make heterogeneous gateways predictable.

This should remain a **capability descriptor**, not a large negotiation protocol. Capabilities describe a local implementation: they are neither a substitute for a Link profile nor an invitation to negotiate at runtime (`LINK-8`).

What a gateway reports for discovery purposes, including per-interface Wire membership, is `DEPLOY §1.10`.

---

# 18. Error Semantics and Link Telemetry

WireSpaces should make errors and resource pressure highly observable while allowing tiny implementations to expose only small counters.

## 18.1 Representative error/status categories

```text
TX congestion / queue full
Link unavailable/down
Unsupported PDU/profile/capability
Unknown, unbound, or unrepresentable Wire
Invalid internal/external Wire scope
Unknown EndpointId
Invalid source/destination HostId
PDUA/reassembly timeout
aggregate integrity failure
Link-native framing / CRC / bus error
queue overuse / high-water condition
duplicate delivery detected
stale/expired data rejected
reassembly context pool exhausted
reserved-field violation
transmit-ownership conflict on a shared medium
mid-PDU transmit abort
Link stopped/reset/restarted, state discarded as uncertain
```

The exact enumeration can be refined, but **congestion must be distinguishable from actual Link malfunction**. WS should not disguise local rejection as a generic transport timeout.

Several categories exist because this architecture deliberately permits loss, latest-value replacement (§14.3), and non-transactional multi-egress (§12.2). A system that allows those outcomes must be able to *report* the receive-side consequences rather than leaving them invisible; see §21.4. The last group covers the cases where a device knows it has lost information but cannot say what was lost — a reset Link with a half-assembled PDU must count a discard rather than deliver a guess.

An empty response from a polled Link is not an error (§1.7).

### Rejection is silent on the wire

> **Malformed, unrepresentable, or unauthorized traffic is counted and dropped. No error response is generated to the sender.**

Report upward locally via counters and the telemetry Service (`DEPLOY §3.3`). A local send failure is a synchronous return value to the calling Service (§14.4), not wire traffic.

### Diagnostic containment

A deployment may nonetheless want a host to *report* faults remotely, and that is legitimate — as a deliberately configured Service on its own Wire, not as a reflex. The distinction is that a Service's reports are wired, rate-controlled, and addressed to whoever is meant to receive them, while a reflex is none of those.

Any such reporting satisfies the following, and the first is what separates a diagnostic path from a positive feedback loop:

- **An error report never generates another error report.** If reporting a fault can itself fail in a way that is reported, a single bad PDU can sustain traffic indefinitely. The recursion has to be structurally impossible, not merely unlikely.
- **Local counters remain the source of truth.** Losing the remote diagnostic path must not erase local evidence — that is precisely when the evidence matters.
- Reporting is bounded, rate-limited, aggregated when repeated, and normally **lower priority than control traffic**. Suppressed counts stay observable, so a reader can tell "no errors" from "too many errors to report."
- **A malformed or babbling peer cannot force unbounded diagnostic work.** The work per received PDU is bounded regardless of how bad the PDU is.
- **Ordinary Endpoints do not receive other nodes' errors.** Diagnostics go where they were wired to go.

The pattern that fits these constraints well is to publish a **small bounded summary periodically and return larger bounded detail only on request**. The summary is cheap enough to send unconditionally; the detail is generated at the rate a host chooses to ask for it, and a busy or absent host simply gets less detail rather than the bus getting more traffic.

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
<= 9-byte WS header/descriptor
-----------------------------
~17 bytes per drop record before local reason/index fields and alignment
```

A file can wrap and overwrite old data indefinitely. If one journal is maintained per Link Interface, Link identity can live in file metadata instead of every record; a combined journal can add local Link index and reason code as needed.

This enables postmortem queries such as:

```text
Which Wires caused CAN_A saturation?
Which EIDs were being dropped?
Which QoS class dominated the burst?
When did the congestion begin and end?
```

Host-facing telemetry Service conventions and optional per-Wire top-talker reporting are in `DEPLOY §3.3`.

## 18.4 Live and latched status have different lifetimes

Status divides into two kinds with genuinely different availability, and merging them produces a diagnostic surface that goes blank at exactly the moment it was needed.

**Live status** describes the current runtime and may stop being observable when that runtime fails:

```text
Link and restart-unit lifecycle state (§23.1)
enabled / configured / operational flags
current queue and pool occupancy, congestion state
heartbeat and last-progress time
counters scoped to the current runtime generation (§23.3)
```

**Latched status** is evidence, and is stored where a failure cannot take it with it:

```text
last fault code, source, and timestamp
restart counts
last recovery reason, action, and result
error and drop totals intended to survive restart
count of suppressed or rate-limited diagnostics
```

> **A live snapshot that has stopped changing does not mean healthy.** It is at least as likely to mean the thing producing it stopped.

That inference is the reason the split matters: a reader that only has live status cannot distinguish a quiet system from a dead one, whereas latched fault and restart records remain readable from outside the failed runtime and answer the question directly. Which storage is durable across what boundary is set in §23.7.

## 18.5 Unavailable is not zero

A telemetry consumer needs to distinguish several conditions that a naive encoding flattens into one:

```text
valid          observed now, at the source
stale          last known good, with an age
unavailable    the source could not be observed
not applicable this Link or profile has no such thing
omitted        the selected telemetry schema does not carry this field
```

> **None of these may be encoded as an ordinary zero, false, or healthy value.**

The concrete failure is worth naming because it is easy to write by accident. A Link with no credit-based flow control (§15.4) has no credit balance at all; reporting it as *zero credit* says the link is stalled and out of credit, which is a different and alarming condition. The same applies to a per-QoS queue depth on a Link whose classes share one inseparable hardware queue: reporting a fabricated split is worse than reporting that the split is not observable.

Age carries a matching subtlety. It is elapsed monotonic time from the observation to the point the report was assembled, not wall-clock time and not evidence of clock synchronization, and a saturated maximum means "this old or older" rather than a specific value. This is the same freshness discipline Services owe their own data (§21.4), applied to the diagnostics.

How these states are encoded — validity bits, reserved codes, per-section maps — is a schema decision (`REG §6.7`). That they are *distinguishable* is not.

---

# Part IV — Contracts and Policy

# 19. Structural Validity and Composition

Two rules govern what the runtime must always check, and what any future higher-level pattern is allowed to require of it. Both are guardrails: they constrain present implementation even though they describe boundaries with future work.

## 19.1 Structural validity versus contract

A useful line separates checks the implementation must always perform from policy that a future WireContract might add.

**Structural validity** — required for the representation itself to function, and therefore always enforced:

```text
one deployment-scoped HostId per Endpoint Domain
the same Domain HostId on every Wire
ordinary and broadcast HostId values valid under the selected allocation
the broadcast host sentinel used only as a destination, never as source
one externally visible producer per (HostId, Endpoint)
complete canonical Wire/source/destination reconstructed before routing/dispatch
destination equals the local Domain HostId or broadcast before dispatch
valid Wire representation for the selected Link profile
representable Endpoint identity for the selected Link profile
deterministic forwarding action set for each Wire or (Wire, ingress)
loop-free forwarding realization for every Wire
valid forwarding and splice configuration
no transparent merge of independent HostId universes
kLocalBus bound to at most one local Link and never forwarded/spliced as itself
bounded storage configured for every accepted delivery path
```

These are structural protocol/configuration constraints. They are not a contract and are not optional. Registry invariant identifiers are being updated concurrently, so semantic prose governs where a cross-document identifier is not yet final.

Deterministic resolution deserves a note because it is the one that fails quietly. One lookup may intentionally produce a set of actions — several egress Links plus local delivery is normal. What is invalid is table-order-dependent interpretation of the same Wire or ingress, conflicting splice projections, duplicate local HostId ownership, or any action set that cannot be described uniquely from configuration.

### Acceptance is set-valued, and the sets are coupled

A binding does not have to admit exactly one value per field. It may legitimately admit sets or ranges: several Wires, a range of host identities, more than one Transport, a span of QoS values, a range of message lengths, several Link representations. That flexibility is useful and should be supported.

It also introduces a validation trap that is worth naming, because the obvious implementation gets it wrong:

> **Checking each field against its own permitted set is not sufficient.** Constraints couple across fields, so a tuple can be rejected even when every value in it is individually admitted.

Concretely: a binding may permit Critical QoS, and separately permit PDUs up to 40 bytes, without permitting a 40-byte Critical PDU — because Critical traffic has a stricter PDUA depth on that Link (`LINK §2.9`). Per-field validation passes; the combination is not admissible. The same shape appears between Transport and length, between Wire and Link representation, and between Endpoint identity and profile (`LINK §2.7`).

So validation operates on the **combination**, and a binding representation has to be able to express coupling rather than only a product of independent sets. This applies identically at configuration time in tooling (`DEPLOY §2.3`) and at runtime on the receive path.

The tooling-level checks that go beyond what one host can verify locally are listed in `DEPLOY §2.3`.

**Contract-level policy** — optional, additive, and never implied:

```text
which Services/Endpoints may transmit
which hosts may consume or invoke them
rate limits
message-size limits
QoS expectations
version compatibility
resource bounds
redundancy requirements
runtime enforcement rules
```

A beginner must be able to put devices on a Wire, exchange Service messages, discover devices, and build a small system without declaring anything in the second list. Conversely, no implementation may skip the first list on the grounds that no contract was supplied.

## 19.2 Higher-level composition must flatten

Patterns above the bare Wire will eventually be wanted — request/response correlation, redundancy across member Wires, closed-loop feedback pairs, freshness monitoring, reliable bulk transfer. The governing rule for all of them is:

> **A composition pattern must reduce, at generation or configuration time, to ordinary Endpoints, Wires, bindings, and bounded local state.** The Router and the LLL must never need to understand the pattern.

So a request/response pair flattens into two directed exchanges on existing Wires plus correlation state held by the requesting Service. A redundancy group flattens into several ordinary member Wires plus the coordination state described in §23.11. In both cases the data plane sees nothing new: the same canonical PDUs, the same forwarding table, the same dispatch.

What this buys is specific. It keeps the forwarding path from accumulating cases, so a gateway written today still forwards correctly for patterns invented later, and it keeps a tiny host from paying for abstractions it never uses. The complete PDU stays the generic forwarding unit.

Composition that emits a new PDU always authors it with the composition Endpoint Domain's HostId. Preserving an upstream source through a semantic transformation would falsely claim lineage and is not permitted.

Two boundaries follow:

- If a proposed pattern *cannot* flatten, and genuinely requires the Router or LLL to learn a new concept, that is the signal to re-read the design rule in `INTRO §3` and look for a formulation that composes instead. It is not automatically forbidden, but it is a much larger change than it looks.
- Flattening is a property of configuration, not of runtime. Nothing here implies dynamic composition, pattern discovery, or a runtime component graph.

A catalog of such components is explicitly **not** established here; see `FUTURE §13`. Only the flattening constraint is adopted now, precisely because it is the part that protects the current architecture from the catalog.

---

# 20. Transport Layer Responsibilities

`TransportType` identifies the transport protocol responsible for interpreting a PDU and providing its delivery semantics. An Endpoint has one Transport Entity boundary. A Transport Entity may implement several closely related message classes under one TransportType; internal code reuse does not require another public wire-level TransportType.

A simple datagram Endpoint may implement that boundary directly. More involved transports accept into declared ingress storage (§9.5), then process protocol state serially in their owning context. A bounded processing call handles a declared maximum amount of ingress and timer/TX work; it does not drain an arbitrarily large queue. Service callbacks occur there or in a later Service context, never in Link RX/dispatch acceptance. A statically bounded bank of point-to-point transport connections may sit behind the one boundary without changing Endpoint identity.

This boundary accommodates the proposed BITS finite-object transport. Its detailed state machine, message encodings, and TransportType number are not standardized by adopting the boundary (`REG §6.16`).

The canonical descriptor reserves a 3-bit `TransportType`, but the exact registry is not frozen.

The baseline Transport is an **Unreliable Datagram** style:

- bounded PDU;
- no mandatory retransmission;
- delivery failure is possible;
- application/Service chooses semantics.

Other Transports may provide reliable segmented transfer, receiver windows/credits, request/response retry behavior, sequence/E2E integrity, or specialized command-source selection. No additional protocol is standardized in the main set yet; BITS is a proposal awaiting detailed integration. The sequenced / end-to-end-protected datagram is a separate candidate (`FUTURE §3.1`); adopting the Transport Entity boundary does not select a protocol implementation order.

One boundary is firm regardless: firmware images, files, logs, and similar bulk data belong to a segmenting Transport, **not** to a constrained LLL. Growing a constrained-Link reassembly mechanism into a large transport protocol is the wrong direction.

### Hop integrity and end-to-end integrity are different claims

A Link CRC and an end-to-end check are not two strengths of the same protection; they cover different spans, and confusing them produces a system that believes it is protected where it is not:

```text
Link integrity   detects corruption within one Link transfer,
                 or within one profile's reassembly

E2E integrity    detects corruption between the producing Endpoint
                 and the consuming Endpoint, across every hop
```

The gap is at a gateway. A gateway validates the CRC on ingress, reassembles, re-encodes, and computes a **fresh** CRC on egress (§12.5) — so the egress CRC attests to the bytes the gateway produced, not to the bytes the source produced. Anything that corrupted the PDU inside the gateway, in its buffers, or during its re-encoding is then covered by a valid checksum. Each hop is verified; the path is not.

For a single-Link deployment this is a distinction without a difference, which is why it is easy to lose. It becomes real as soon as traffic crosses a gateway, and the check must be computed by the producing Endpoint and verified by the consuming one to close it — hence the Transport above.

Neither mechanism is authentication. A CRC detects accidental corruption and is trivially recomputed by anyone modifying the data deliberately (§22).

## 20.1 Producer failover is not a base Wire feature

A Wire has no distinguished central host, and the Router does not elect or replace a producer. If a command or state producer fails, the Wire continues to propagate other hosts' traffic; only the Service behavior that depended on that source is absent.

Redundant producers are explicit composition, commonly across separate Wires or separate Endpoint identities:

```text
Producer A ---- Wire A ---- consuming composition
Producer B ---- Wire B ---- consuming composition
```

Application or Transport policy decides which source is authoritative, how health is measured, and when selection changes. Any selected or combined output is a newly authored PDU whose source is the composition Domain's HostId. Producer selection is higher-level policy, not Wire routing.

## 20.2 Transport is chosen by Service semantics

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

This is why the base Transport is an Unreliable Datagram and why congestion is a normal send outcome (§21.1) rather than something the stack hides. For periodic state, control commands, and telemetry, the usual right answer is an unreliable Transport, latest-value replacement (§14.3), and receiver-side freshness handling (§21.4).

Reliability earns its place where the data is not periodic and every byte matters:

```text
prefer unreliable + freshness   periodic state, setpoints, telemetry,
                                heartbeats, sampled measurements

prefer reliable                 firmware images, files, logs, configuration
                                writes, request/response with side effects
```

Two related cautions. Choosing a reliable Transport does not remove the need for freshness handling; it changes staleness from a delivery gap into a latency distribution, which is harder to observe. And ordering guarantees are a separate axis from delivery guarantees — a Service that needs to reject out-of-order updates should say so rather than assume a reliable Transport implies it.

---

# 21. Services and Application Expectations

A **Service** is reusable functionality exposed through one or more network-visible Endpoints (§1.6). How it presents itself to the application code that uses it is deliberately outside the architecture. The Service model should be agnostic to whether the implementation is C++ firmware, host software, softcore software, or RTL.

## 21.1 Send failure handling

Services must not assume `send()` always succeeds. In particular `kCongestedLink` / `E_FULL` is an expected runtime outcome.

The Service/Transport chooses whether to retry later, drop a latest-only update, coalesce state, propagate an application error, or pause a reliable transfer. It should not interpret ordinary queue congestion as proof that the driver is corrupt and must be reset.

## 21.2 Service traffic declarations

For future static capacity checking, Services should ideally expose conservative traffic claims: maximum TX rate, maximum PDU size, QoS, and an optional burst bound. The exact schema is `FUTURE §4`.

## 21.3 A Service contract is a schema over bytes

The single most consequential Service-level rule concerns what defines the message:

> **A Service protocol is defined by a canonical schema over bytes, never by a C or C++ ABI.** Generated structs and classes are *views* or codecs derived from that schema. Native padding, enum width, field alignment, and host endianness define nothing.

The temptation is obvious and the failure is quiet. Casting a received buffer to a `struct` works perfectly between two builds of the same firmware with the same compiler and the same flags, and then produces subtly wrong values against the Python host tool, the RTL host, or the same firmware built for a different target. Since WireSpaces expects exactly that heterogeneity (§1.1), the schema has to be the contract.

A Service specification that is ready to implement states:

```text
exact serialization: byte order, bit numbering, signed representation,
    and how lengths are interpreted
every field width, range, enum value, and invalid value
engineering units, scale, offset, clock domain, validity metadata
what a transmitter writes and a receiver does for padding and reserved fields
which message types are required, optional, and safely ignorable
behavior on malformed input, unknown version, unknown type, unsupported feature
freshness, sequence, correlation, duplicate, and restart semantics (§21.4)
```

Sub-byte integers, explicitly sized enums, fixed-point values, and reserved fields are first-class schema features rather than optimizations — on a Classical CAN Link, the difference between a packed and a naturally-aligned layout is the difference between one frame and three. Bounded arrays, strings, and opaque byte sequences are fine; unbounded allocation on an MCU is not. Decoders handle unaligned data safely and **validate lengths before accessing fields**, in that order.

No schema language is selected yet.

### A message prefix worth defaulting to

Most Services benefit from a small common prefix:

```text
u8  protocol_version
u8  message_type
```

`protocol_version` marks an **incompatible** reinterpretation of the protocol; it does not move for an implementation-only fix. `message_type` distinguishes requests, successful responses, Service errors, snapshots, feedback, and events — and incidentally catches Endpoint and profile misconfiguration cheaply, since a plausible-looking payload arriving at the wrong Endpoint usually fails the type check immediately. A separate magic field is optional and usually not worth its bytes.

### Three separate identities

Version questions get confused because three different identities are all colloquially "the version," and conflating them produces either false incompatibility or, worse, false compatibility:

| Identity | What it covers | Changes when |
|---|---|---|
| **Protocol version** | wire-semantic interpretation, carried in the message | the protocol is reinterpreted incompatibly |
| **Compatibility fingerprint** | the schema plus every communication-relevant profile and configuration choice | anything wire-visible changes |
| **Build identity** | the exact software build | any code changes at all |

So an internal hotfix moves build identity alone. A build option that changes anything wire-visible moves the fingerprint while the protocol version stays valid. An incompatible redesign moves both the fingerprint and the protocol version.

> **A build hash is not a substitute for a schema compatibility check.** It is both too sensitive and not sensitive enough: it changes when nothing wire-visible changed, and two builds sharing a hash can still be configured incompatibly.

The fingerprint is the one that should be generated rather than maintained by hand, and it is what `DEPLOY §2.4` compares.

Level-0 Service expectations, archetypes, and the broader ecosystem catalog are in `FUTURE §8`.

## 21.4 Freshness, staleness, and duplicates

A Service definition should cover more than message layout. Alongside meaning and encoding, it should state:

```text
timing        expected production rate or cadence
freshness     how long a value remains usable
authority     who may produce it
failure       what a receiver does when it is absent or stale
```

Freshness is the receive-side counterpart to the choices this architecture makes elsewhere. Because loss is permitted (§20), because a queue may replace a pending latest-value update (§14.3), and because a polled Link delivers data up to one cadence late (§1.7), a value can legitimately be older than the receiver assumes. The architecture therefore has to make that visible instead of leaving each Service to notice on its own:

> **A Service that acts on received state should be able to tell how old that state is, and must define what it does when the answer is "too old".**

The practical minimum is unglamorous and cheap: a receiver that expects periodic data times out on absence and enters a defined degraded state, rather than continuing to act on the last value indefinitely. That single behavior is what makes an unreliable Transport safe (§20.2), and it is worth more than most delivery guarantees.

Duplicates need the same treatment. Non-transactional multi-egress (§12.2), retried request/response, and future redundancy compositions (§23.11) can all present the same logical update twice. A Service that is not idempotent must say how duplicates are recognized and suppressed, and that suppression belongs to the Service or the composition — not to the Router, which deliberately has no duplicate-suppression state (§12.4).

Both outcomes are observable error categories (§18.1), so a system can report stale-data rejections and detected duplicates as counters rather than discovering them by behavior.

What remains open is representation: whether freshness is expressed as a Service-declared interval checked locally against a monotonic clock, or needs a canonical timestamp or sequence field carried in a header extension. Nothing here requires a new canonical field yet, and none should be added until a Service demonstrates that local timing cannot answer the question.

---

# 22. Security Scope

WireSpaces does not currently define a complete general security architecture. This is deliberate.

Configuration operations must be treated as trusted/privileged:

- assigning HostIds;
- naming Wires;
- changing Link Bindings or host-projection maps;
- changing forwarding tables;
- installing or changing splices;
- privileged device management.

Development Auto-Wiring is appropriate for physically trusted benches/labs unless a product supplies authentication/authorization.

Remote or hostile Links should use an appropriate secure boundary: VPN/secure tunnel, an authenticated Link profile, or a product-specific secure gateway. A remote maintenance link should not blindly expose every internal Wire.

The debug-Wire/splice model is useful partly because the scope boundary where private traffic becomes externally visible is explicit.

Native CAN CRC and the aggregate PDU CRC detect accidental corruption only. They provide no authentication, authorization, confidentiality, freshness, or replay protection.

Do not invent new cryptography merely to complete the base architecture.

## 22.1 Privileged capabilities require a separate build

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
promiscuous observation (DEPLOY §3.1)
```

Each needs an explicit development or engineering build to exist at all. The distinction matters because a configuration-gated capability is one bad forwarding table, one mis-scoped splice, or one compromised host away from being reachable — whereas a capability that was never compiled cannot be reached by any sequence of messages.

Related trust boundaries are outside WireSpaces but should be treated as privileged by any product using it: SWD/JTAG, bootloader entry, persistent configuration writes, and anything that can change what runs on the device.

For initial implementations and examples this means: favor receive-only observation and bounded diagnostics, keep remote and IP-facing exposure out of the recommended default feature set, and require explicit configuration for every route, splice, and observation path rather than providing a broad default.

---

# 23. Lifecycle, Restart, and Redundancy Boundaries

WireSpaces should favor recoverable, bounded components. Likely recoverable units are LLL instances, Link Interfaces/drivers, Services, and Endpoint Domains; the base Router is close to static state and may have little dynamic state to restart.

Exact lifecycle enumerations, transition APIs, supervisor interfaces, and escalation parameters are implementation-owned and remain undesigned (`FUTURE §6`). What follows is the part that is not free to vary, because breaking it is visible to peers, to application code, or to whoever is reading the evidence after the failure.

## 23.1 A Link and its restart unit have separate lifecycles

Two different things can be down, and conflating them leaves a system whose only recovery is a reboot.

A **Link** is a configured attachment that can be administratively enabled or disabled and can also fail on its own:

```text
disabled -> starting -> up
    ^          |         |
    |          v         v
    +------ stopping <- degraded / faulted
```

A **restart unit** is the execution and fault-containment scope owning that Link's mutable state — parser and reassembly state, TX scheduling, timers, queues:

```text
stopped -> starting -> running -> stopping -> stopped
                          |
                          v
                       faulted -> restarting -> starting
```

State names are implementation-defined. The contract is not:

```text
explicit start, stop, reset, enable, and disable requests
idempotent handling, or explicit rejection, of a redundant request
bounded acceptance and a completion report for every accepted request
a defined terminal result for TX/RX work interrupted by stop or restart
rejection of new work while the target cannot accept it
a runtime generation separating before-restart from after-restart (§23.3)
```

> **Lifecycle control is not only a fault path.** Power management, maintenance, commissioning (`DEPLOY §1.2`), and reinitialization use the same operations — not only fault recovery.

## 23.2 The restart unit

The LLL instance is the natural default (`§13.3`, `LINK-11`). Grouping is permitted where scheduling or fault containment justifies it; **every mutable state item has exactly one owning restart unit**.

A restart unit owns local Link state, not Wires: restarting makes paths temporarily unavailable without redefining a Wire or transferring producer authority (§1.7). In-flight traffic is lost and counted.

## 23.3 Runtime generation

> **A restart unit exposes a generation value that changes on every restart, and no observation may be compared or combined across a change in it.**

Without it, counters, high-water marks, and handles suffer quiet mis-attribution across restart (including ABA on slot indices). Generation is local, not a clock; distinct from Endpoint snapshot generation (§10.4).

## 23.4 Bounded quiesce, and discarding uncertain work

> **Stopping must not wait indefinitely for a failed driver or an absent peer.**

Declare a quiesce deadline; interrupted transmits reach a defined terminal outcome (§16.1).

> **Transient state whose validity cannot be established is discarded, not reconstructed from partial evidence.**

Half-parsed frames, incomplete reassembly, unknown TX completion: discard and count (§18.1). Continuity across restart is end-to-end (§21.4), not Link-local guessing.

## 23.5 Recovery escalates from the smallest scope

```text
1  clear or reinitialize the Link driver
2  disable and re-enable the Link
3  rebuild the owning restart unit's transient state
4  restart a larger communications subsystem, if the platform has one
5  reset the device
```

An escalation policy declares attempt limits, backoff, and promotion rules. Avoid recovery busy-loops and diagnostic storms; do not skip escalation levels (a driver error should not default to device reset).

## 23.6 Restart isolation is a claim, and must be declared

> **Restarting one Link should not stop healthy sibling Links** — unless shared hardware, clocks, memory, or driver state genuinely prevents isolation, in which case that dependency is declared in configuration (`DEPLOY §2.2`).

## 23.7 What outlives a restart, and its reset boundary

A restart discards:

```text
partial frames and decoder state
incomplete reassembly contexts
pending timers and transient scheduler state
in-progress transfers that cannot be completed safely
queued work whose cancellation result has been reported
counters explicitly scoped to the runtime generation
```

The following has to live somewhere a restart cannot reach:

```text
static configuration and profile selection
diagnostic counters required to survive restart
latched fault and restart records (§18.4)
supervisor state and recovery attempt history
resources shared with other restart units
storage backing buffers already transferred to application ownership
```

Buffers transferred to application ownership must survive the Link restart that filled them (§23.7, `OWN-3`).

> **"Persistent" here means surviving a restart unit's reconstruction. It does not mean surviving power loss** — each counter declares which boundary applies.

## 23.8 A supervisor must sit outside what it supervises

A restart unit cannot be relied on to detect all of its own failures, because the interesting cases are precisely the ones where the thing that would have noticed is the thing that stopped.

What an external supervisor should be able to observe:

```text
missing execution heartbeat, or a missed deadline
no RX/TX progress where progress was expected
driver fault, or repeated restart
invariant violation
sustained queue or pool exhaustion
explicit transition to faulted
```

> **A heartbeat written by the failed context and read only by that same context is not supervision.**

Progress expectations, timeouts, and restart authority are configuration (`DEPLOY §2`). Conclusions are recorded outside state about to be discarded.

## 23.9 An out-of-band debug path

Low-complexity path independent of LLL parsing, dispatch, main buffer pool, telemetry, and restartable runtime state — complementary to the in-band Internal Debug Wire (`§7`, `DEPLOY §3.2`). Privileged capability (§22.1).

## 23.10 Link/status observability after failure

> **A failed Link is not required to report its own failure.**

A healthy sibling Link or debug path can carry latched fault records and unavailable live state (`§18.4`, `DEPLOY §3.3`).

## 23.11 Redundancy by composition

The base Wire has one loop-free propagation topology and no hidden redundant-path behavior. Redundancy is composed from multiple Wires with explicit coordination of the six responsibilities below — not hidden failover inside one Wire.

```text
message-instance correlation   recognizing that two arrivals are one update
duplicate suppression          which copy is acted on, and what happens to the rest
stale-copy rejection           discarding a late copy that lost to a newer one
failover / active-standby      when a path is considered failed, and what switches
per-Wire health                each member's own liveness, independently observed
per-sink coverage              which sinks are actually covered by which members
```

Router forwarding has no duplicate-suppression state (§12.4); coordination is Service- or composition-level (§19.2).

- **Path redundancy ≠ replicated sources** — one Endpoint Domain may author copies on member Wires with the same HostId, while independently implemented producers necessarily have distinct HostIds and need explicit source-selection composition.
- **Observation is not coverage** — promiscuous observers (`DEPLOY §3.1`) are not failover paths.

When composition selects, combines, or otherwise creates an output from redundant inputs, that output is newly authored with the composition Domain's HostId. Forwarding alone preserves each input's original source and destination.

Cyclic/redundant *routing* profiles remain deferred; acyclic realization (§12.4) still holds for composed redundancy.

---

# Part V — Targets and Implementation

# 24. FPGA / RTL Architecture

WireSpaces is intentionally designed so software and RTL can participate in the same communication model.

```text
CAN FD LLL ----\
CAN XL LLL -----+--> canonical PDU / Router --> Ethernet / FTDI LLL
UART LLL -------+
RTL Endpoint ---+
```

## 24.1 Hardware routing primitive

The central routing operation is hardware-friendly:

```text
(WireNumber)
    or
(WireNumber, ingress Link Interface index)
    -> egress bitmask
    -> bounded local host-Domain set
    -> optional spliceWire
```

A generated table can live in registers/BRAM and produce an egress mask in a small number of cycles.

## 24.2 RTL Endpoint model

A pure RTL Endpoint can consume WS PDUs, produce telemetry/events, implement control/status registers, and participate in standard Services where the schema/state machine is suitable.

The host should not need to know whether an Endpoint is implemented in C++ or SystemVerilog.

# 25. Small MCU Profile

```text
one Physical Link, one Endpoint Domain, few Services
copies everywhere; no dynamic allocation; no Router/network task; no locks
```

Receive: frame RX → reconstruct canonical Wire/source/destination → select the destination Domain → dispatch on Endpoint identity → copy into Endpoint storage. Transmit: Service → transmit Endpoint → author with the local Domain HostId → LLL drains or samples. The storage boundary applies (§9.4) — typically one slot per Endpoint; cost is one copy and a loop iteration.

A constrained CAN11 Host may use a default VCN map on `kLocalBus` and generated direct dispatch. The Link reconstructs canonical Wire and Host identity before dispatch. A native interface may add alias-selected named Wires; a Guest allocation supplies one Wire. CAN29 is preferred when alias/VCN capacity or configuration cost is unsuitable (§5.3).

A conforming implementation need not instantiate runtime Link/LLL/Router objects if generated static equivalents preserve semantics, bounds, ownership, diagnostics, and compatibility fingerprint (`DEPLOY §2.4`).

Collapsing to a `switch` and constants is fine, but **semantic and authority distinctions must survive the fold** — one HostId for the Domain on every Wire, destination-controlled acceptance, configured Endpoint acceptance only, and bindings that grant only declared transmit rights.

Scaling profiles for other targets are in `IMPL`.

---

# 26. Conformance

WireSpaces spans C++ firmware, host software, Python tooling, and RTL within one project. Shared reference cases are required so those implementations do not quietly diverge.

Vector catalogs, boundary-test policy, loss-amplification notes, and exit criteria for dropping the provisional label are in `CONFORM`. The simulator should implement an MVP subset; see the [simulator README](../sim/README.md).

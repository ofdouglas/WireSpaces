# Application Protocols and Services

**Status:** Consolidated design contract<br>
**Maturity:** Provisional; suitable for prototype alignment, not a frozen API or wire specification<br>
**Scope:** Bounded WireSpaces application protocols, Service composition, local endpoint behavior, brokerless pub/sub, and provisional Domain Control

Within WireSpaces, a **Wire Space** is one bounded, statically configured
communication universe. Endpoint and routing identities are meaningful within
that Wire Space; unrelated Wire Spaces may reuse the same numeric values. This
document owns application, Service, Communication Component, and provisional
Domain Control semantics for that fabric. [Endpoint Domains, Wires, Routes, and Endpoint Access](endpoint_domains_wires_routes_and_access.md)
owns the semantic Wiring model, endpoint authority, and source lineage.
[Core protocol and routing](core_protocol_and_routing.md) owns canonical routing
representation, Control policy fields, and header-extension rules. [Logical
links and transports](logical_links_and_transports.md) owns delivery, transport,
and Link contracts; [prototype and validation](prototype_and_validation.md)
defines how the requirements below are proved. The [architecture
overview](architecture_overview.md) and [document map](README.md) define the
wider scope, authority, and [source-update precedence](README.md#authority-consolidated-set-versus-source-drafts).

The shapes shown here are semantic examples. They are not final APIs, field
layouts, wire encodings, token formats, command values, security mechanisms, or
guarded-transition sequences.

## 1. Role-directed protocols

Every service protocol defines two stable roles:

- **Upstream** originates stimulus, requests, commands, or orchestration.
- **Downstream** receives that stimulus and performs the responder or controlled role.

The roles belong to the Service interaction, not to physical topology, device
class, Wire direction, or Route `Direction`. A downstream Endpoint may reply
immediately, publish feedback later, or originate substantial asynchronous
traffic without changing the application roles. Route `Direction` remains the
structural A-to-B or B-to-A traversal of cross-domain infrastructure; neither
value intrinsically means upstream or downstream.

Every Wire is directed and has exactly one authoritative source Endpoint
instance. A bidirectional Service therefore uses at least two Wires, one for
each communication direction. Request and response, command and status, or two
halves of a symmetric exchange are separate Wires even when they share one
Route. A convenience API may present them as one duplex facility, but their
source authority, endpoint bindings, transport state, and policy remain
separate.

## 2. Endpoint identity and service instances

The canonical endpoint identity and Endpoint Router lookup key are:

```text
(Namespace, EndpointId)
```

`EndpointId` remains a canonical `uint16_t` above the Logical Link Layer. The
official Namespace-0 allocation is:

```text
EndpointId 0            Invalid / reserved
EndpointId 1..32        Public/Common, compact-capable allocation
EndpointId 33..991      Deployment/organization-owned allocation
EndpointId 992..1023    Public/Common, General-only allocation
EndpointId 1024..65535  Canonical extended allocation
```

Namespace-0 `EndpointId` 0 never identifies an Endpoint. Allocation policy for
Namespaces 1 through 3 remains open. The low public range is reserved for
stable Services that materially benefit from compact constrained-Link
representation; public allocation does not follow merely from publishing a
library, and third-party EndpointIds should normally remain configurable. The
deployment-owned region remains contiguous even though values 33..127 may also
meet the EndpointId part of an optimized CAN representation.

Committed 11-bit Classical CAN General PDUA directly represents EndpointIds
1..1023; its optimized Namespace-0 form can represent only the narrower subset
defined by the experimental [CAN PDU adapter](can_pdu_adapter_spec.md).
EndpointIds above 1023 remain valid canonical identities but are not directly
representable on committed 11-bit CAN. Wiring and tooling reject an
unrepresentable placement; values are never truncated, implicitly aliased, or
assigned a fallback representation from observed traffic. Another explicitly
specified Link representation or an explicit gateway/Replicator is required.
“Compact-capable” is therefore allocation policy, not a promise that every
message, transport, extension choice, route, or Link profile will fit a compact
encoding.

`(Namespace, EndpointId)` identifies an application-protocol Endpoint instance
within an Endpoint Domain and Wire Space, not a device, Wire, Route, Service
name, Communication Component participant, software build, or permission. A
protocol may use several endpoint keys, and a deployment may instantiate the
same protocol several times under different keys or in different Endpoint
Domains. For every externally producing
`(Endpoint Domain, Namespace, EndpointId)`, exactly one externally visible
producer Endpoint identity exists. Static Wiring validation rejects duplicate
producer owners.

The authoritative source of every message is the source Endpoint instance of
its resolved Wire. Its source lineage includes the source Endpoint Domain,
source `(Namespace, EndpointId)`, and configured Wire. Route `Direction`,
`RoutingWord`, ingress context, and Link metadata help resolve that Wire; they
do not independently confer producer authority.

A Service instance therefore binds, at minimum:

- one directed protocol role;
- one Service Interface with typed directional Ports;
- one or more Endpoint instances and canonical `(Namespace, EndpointId)`
  identities;
- any Direct-Service bindings or Composite-Service Communication Components;
- explicitly configured source and sink Wires;
- a declared transport and size profile;
- configured endpoint-binding, routing, QoS, and compatibility policy;
- bounded RX, TX, and local-delivery behavior;
- service-specific authority, freshness, and error rules.

Default IDs supplied by a reusable library are conveniences only. The deployment owns final allocation and collision checks.

### Local Wires, typed handles, and IPC

A local Wire has its source and all sinks in one Endpoint Domain and needs no
cross-domain Route. Ordinary application code should receive generated or
typed local Endpoint handles rather than construct a `RoutingWord` or call an
unrestricted router API. Such handles preserve each Endpoint's read, write,
invoke, concurrency, and lifetime policy.

Communication between distinct Endpoint Domains, including two domains on one
MCU or SoC, uses explicit cross-domain Wires and configured Routes. Shared
memory, an RTOS queue, a FIFO, or another IPC mechanism may implement a Link;
that implementation choice does not merge the Endpoint Domains, create
authority, or change the source Endpoint instance of the resolved Wire.

## 3. Service interfaces and communication composition

A **Service Interface** is the set of typed directional **Ports** presented by
a Service instance. A Port is a local software- or RTL-facing boundary such as
`command_in`, `feedback_out`, or `fault_out`; it is not a network-visible
Endpoint identity. A simple Port may bind directly to an Endpoint and Wire. A
richer Port may be implemented through a static hierarchy that ultimately
flattens to ordinary Endpoints and directed Wires.

A **Communication Component** is a reusable communication composition that
exposes Ports and is implemented using Endpoints, Wires, and optionally other
Communication Components. Redundancy harnesses, multi-participant
command/feedback buses, request/response channels, and freshness monitors are
candidate component behaviors rather than new primitive Wire types. Component
hierarchy is local architecture; the Endpoint Router and Link Engines operate
on the flattened Endpoint/Wire configuration.

The descriptive terms are:

- **Direct Service** — implements its public Ports directly with one or more
  Endpoint/Wire bindings.
- **Composite Service** — implements some or all public Ports through one or
  more Communication Components.

These are not separate protocol classes, and this document does not freeze a
class hierarchy or construction API.

### Component-local participants

A multi-participant Communication Component assigns a bounded
`ParticipantId` within that component instance. `ParticipantId` is not global,
is not inherently transmitted, and is independent of routing `PeerId`. Static
component configuration maps each participant to the Endpoint roles it
provides, for example:

```text
ParticipantId Pump:
    command sink    -> Endpoint identity X
    feedback source -> Endpoint identity Y
    fault source    -> Endpoint identity Z
```

One participant role may use several Wires, routing contexts, Links, or
redundant paths without changing `ParticipantId`. Per-participant feedback,
freshness, health, accepted-command generation, completion, and path coverage
are keyed by `ParticipantId`, not by `PeerId`; `PeerId` remains only a field in
selected routing representations. Generation and tooling must validate role
mappings and finite participant bounds without prescribing an exact API or
numeric representation.

## 4. Five bounded binding modes

Each endpoint registration declares exactly one of five bounded binding modes.
These modes describe where transmission context comes from; they do not create
general routing.

1. **Static binding** — the permitted peer Wire, canonical `RoutingWord`,
   endpoint key, transport, and applicable ingress/egress context are fixed by
   generated configuration. A fixed bounded set may be selected by explicit
   application policy. Local delivery selects an authorized local Wire without
   constructing cross-domain routing infrastructure.
2. **Learned-from-ingress binding** — retains a finite configured number of
   peer selections derived only from validated ingress that resolves to a
   pre-authorized Wire and allowable `RoutingWord`, endpoint, Link, interface,
   and origin constraints.
3. **Request-scoped binding** — may reply only through opaque `ReplyContext`
   supplied with a valid incoming request. The context is bounded, cannot be
   synthesized by Service code, selects an explicitly configured reply Wire,
   and expires at a defined scope boundary.
4. **Receive-only** — accepts configured traffic and never transmits protocol
   messages.
5. **Transmit-only** — originates configured traffic through statically
   configured Wires and does not acquire peer authority from ingress.

Learned-from-ingress binding is disabled on unauthenticated multi-access Links.
It may be enabled only on a statically single-peer ingress or under a named
authenticated-origin profile, and only within a preconfigured allowlist. The
allowlist constrains the resolved Wire, source Endpoint Domain, source Endpoint
identity, `RoutingWord`, Route `Direction`, Link, interface, transport, and
origin values as applicable. Routing, endpoint-binding, interface, and
descriptor validation are not authentication. Every enabled learned mode
defines finite capacity and lifetime, deterministic replacement or eviction,
restart invalidation or restoration, and auditable learn, replace, expire,
reject, and clear events. Learning never creates a Wire or Route, changes
Route terminals, broadens endpoint authority, changes `RoutingWord`
interpretation, or authorizes a different transport. Prototype conformance
tests all enabled binding modes; learned-from-ingress mode is not assumed to
be unconditionally enabled.

Protocols needing richer multi-party sessions maintain explicit bounded session
state above these primitives. They must not treat arbitrary recent traffic as
permission to transmit.

### Replies and autonomous downstream transmission

The receive path supplies parsed Service data separately from an opaque
`ReplyContext`. After ingress, Wire, and endpoint-binding validation,
infrastructure constructs that context with enough bounded router state to
select a separately authorized reply Wire and its canonical `RoutingWord`,
endpoint identity, transport, QoS policy, and correlation state. The request
and reply are distinct Wires. Reply authority is not inferred merely by
reversing the incoming Route `Direction`.

A request-scoped responder can return a success or Service error without
interpreting or synthesizing raw routing, alias, interface, or endpoint fields.
For a local request, the context selects an authorized local reply Wire and
does not synthesize cross-domain infrastructure. The router revalidates the
context when transmission occurs.

Delayed work must make the lifetime rule explicit. If a response can outlive the callback or dispatch operation, the infrastructure must provide a bounded retained reply handle or require another binding mode; retaining references to transient ingress state is invalid.

Request-scoped context authorizes only the corresponding response flow and is
unusable for a different operation, Endpoint binding, or Wire.
Periodic feedback, completion events, unsolicited alarms, and other autonomous
downstream messages require a static or learned-from-ingress binding and
separate source Wire configured for that purpose. A successful request must
not silently install a durable peer binding unless the Endpoint registration
is explicitly configured for learned-from-ingress binding.

A protocol requiring both request-scoped replies and autonomous transmission
uses separate endpoint registrations or explicit bounded named sub-bindings
that each have one declared binding mode, authority, and lifetime. Separate
registrations are recommended for the initial prototype. This is a composition
rule, not frozen API spelling.

## 5. Service message contracts

The default service-message prefix is semantically:

```text
u8 protocol_version
u8 message_type
```

`protocol_version` identifies an incompatible interpretation of the service protocol. It does not change for an implementation-only hotfix. `message_type` distinguishes requests, successful responses, service errors, snapshots, feedback, and events and also helps detect endpoint/profile misconfiguration. A separate magic field is optional, not standard.

Each serious service is defined by a canonical schema over bytes, not by a C/C++ ABI. Its specification must state:

- exact serialization, byte order, bit numbering, signed representation, and length interpretation;
- every field width, range, enum value, and invalid value;
- engineering units, scale, offset, clock domain, and validity metadata where applicable;
- transmitter values and receiver behavior for padding and reserved fields;
- required, optional, and safely ignorable message types;
- malformed-input, unknown-version, unknown-type, and unsupported-feature behavior;
- freshness, sequence, correlation, duplicate, and restart semantics where applicable.

Generated classes and structs are views or codecs derived from the schema; native padding, enum layout, alignment, and endianness never define the wire contract. Decoders must safely handle unaligned data and reject lengths before field access. Bounded arrays, strings, and opaque byte sequences are allowed; unbounded MCU allocation is not.

Sub-byte integers, explicitly sized enums, fixed-point values, and reserved fields are first-class schema features. Compact encoding is encouraged when it preserves clarity and is justified by a constrained profile, but this document does not select a final schema language or serialization format.

### Version, compatibility, and build identity

Three identities remain separate:

- **Protocol version** — the major wire-semantic interpretation carried in each service message.
- **Compatibility fingerprint** — a stable, preferably generated fingerprint over the canonical schema and all communication-relevant profile/configuration choices. DEETS or equivalent management infrastructure may expose it.
- **Build identity** — the exact software build, reported by BIO or equivalent.

An internal hotfix may change build identity without changing compatibility. A wire-visible build option changes compatibility even if the protocol version remains valid. An incompatible redesign changes protocol version and compatibility. A build hash must not be used as a substitute for schema compatibility.

## 6. Service archetypes, QoS, size, and profile rules

The following archetypes are reusable policy starting points, not distinct
transport types:

- **State publication / snapshot** — publishes the latest coherent value;
  intermediate replacement may be acceptable, while freshness, validity, and
  restart metadata remain explicit.
- **Command / control** — requests a bounded state change with explicit
  authority, acceptance, idempotency or duplicate behavior, and commitment
  rules.
- **RPC / query** — uses separate request and response Wires, correlation when
  needed, bounded responder work, and explicit timeout and Service-error
  behavior. RPC does not imply a reliable transport.
- **Event stream** — preserves discrete occurrences in a bounded queue and
  defines ordering, gap detection, overflow, and restart behavior.
- **Health / heartbeat** — reports bounded liveness, readiness, degraded state,
  and fault summaries. Receipt is evidence only under the Service's stated
  freshness and clock assumptions.
- **Logging / diagnostics** — emits bounded, rate-limited, aggregatable records
  and must not block control work indefinitely.
- **Bulk transfer** — uses small control/status messages around a separately
  bounded segmented or object-transfer mechanism.

Brokerless pub/sub commonly uses state or event archetypes. Request/response,
command/status, and client/server interactions are compositions of directed
Wires rather than special routing modes.

### QoS recommendations

The canonical `Control` metadata carries one of the QoS classes defined by
[core protocol and routing](core_protocol_and_routing.md). Application and
deployment configuration use the symbolic names `Background / Bulk`, `Normal`,
`High Priority`, and `Critical Priority`; this document does not duplicate
their core representation or Link-native mapping.

Reasonable defaults are:

- state snapshots, ordinary commands, RPC, events, and health: `Normal`;
- logging, diagnostics detail, and bulk transfer: `Background / Bulk`;
- control-relevant commands, urgent events, or health faults: `High Priority`
  only when deployment analysis justifies promotion;
- safety-critical traffic: `Critical Priority` only under explicit admission,
  resource, starvation, and system safety analysis.

The deployment may override these defaults per Wire, message class, or named
profile. A higher QoS expresses relative scheduling intent only. It does not
guarantee latency, deadlines, bandwidth, delivery, freshness, or freedom from
starvation; those properties require Link policy, transport behavior, resource
analysis, and end-to-end validation.

### Size and profile rules

Services use one of three sizing patterns:

- **Fixed-small** — every message has a modest fixed maximum, preferably efficient on constrained links.
- **Bounded-profile** — a finite named profile changes capacities such as maximum entries, samples, or string bytes while preserving message meaning and state-machine behavior.
- **Bulk-transfer** — small control/status messages coordinate a separately bounded segmented or object-transfer facility.

A profile may alter capacity, batching, or storage bounds. It must not silently remove semantic fields, redefine units, or change protocol behavior merely to fit a link. Published profiles are named, immutable, and independently testable; incompatible changes require a new profile identity.

Every Service instance declares maximum encoded payload and message size,
supported transport profiles, expected and maximum rates, burst depth,
freshness/deadline requirements, and compatible Logical Link Layer capacities.
Broad portability does not require every Service to fit every Link.

Application sizing includes the canonical core header and, when selected by a
named profile, its bounded header-extension block. A Service or deployment
profile states whether extensions are forbidden, optional, or required and
budgets their maximum overhead on every applicable Link. Extension presence
does not assign application meaning by itself. The core specification owns
extension bounds, parsing, registries, and compatibility behavior; Service
payload remains outside that mechanism unless a separately standardized
extension explicitly says otherwise.

Classical CAN remains useful design pressure, especially at small aggregation depths, but its consolidated PDU format is unfinished. Capacity estimates from draft CAN layouts are not stable application contracts; see the [experimental CAN PDU adapter specification](can_pdu_adapter_spec.md).

## 7. Bounded local endpoint behavior

Network delivery and local concurrency are separate policies selected for each service instance.

### Latest-value snapshots

A latest-value endpoint retains one coherent decoded snapshot. One receive/publish writer may update it while multiple local readers observe either the previous or next complete value, never a torn mixture. Suitable implementations include a seqlock or double buffer, subject to the platform memory and cache-coherency contract.

Snapshots carry enough metadata for consumers to assess validity: receive or source sequence, time/age basis, producer restart epoch where required, and decode/E2E status where available. A snapshot is not an event history. Replacing an unread value is normal and is counted when diagnostically useful.

### Bounded event queues

An event endpoint uses a fixed-capacity FIFO or ring when every retained event matters. It defines:

- producer and consumer concurrency;
- ordering and sequence/gap behavior;
- overflow policy: reject newest, drop oldest, coalesce defined events, or enter a fault state;
- observable overflow and high-water counters;
- restart and retained-state behavior.

Queue capacity is a deployment/profile parameter with a static resource cost. "Queue until memory is available" is not a valid MCU policy.

### Single-writer and multi-caller TX

A **single-writer TX endpoint** is owned by one task, thread, core, or serialized execution context. A **multi-caller TX endpoint** adds a bounded thread-safe funnel in front of the same protocol endpoint.

Multi-caller support must define arbitration, ordering, per-call result, ISR allowance, wakeup behavior, and storage ownership. It must not convert an application endpoint into an unbounded multi-producer queue. When ordering or caller authority affects service meaning, those semantics belong to the service protocol rather than being hidden in the funnel.

The `GloballyWritableEndpoint<T>` conceptual name denotes this multi-caller
case: several synchronized local callers feed one Endpoint implementation and
one externally visible producer identity. It is an access/concurrency shape,
not global network reachability, multiple Wire sources, or permission for
arbitrary producers.

## 8. TX rejection and congestion

Transmit submission can fail during normal operation. Every transmitting service defines bounded behavior for local rejection and does not busy-spin.

Candidate result categories include accepted, replaced/coalesced, congested, no
permitted Wire, Route unavailable, invalid message/profile, and Link
unavailable. Names and API representation are provisional. Local rejection
means the message never entered the transport; a post-acceptance failure
reported by a defined transport is a distinct state. These states must not be
conflated.

Required service policies:

- **Latest value:** retain at most one pending value; a newer value may replace it; retry on notification, tick, or timer.
- **Command or high-integrity operation:** expose rejection to the caller or retain the exact bounded operation; do not advance protocol state before the defined commitment point.
- **Event stream:** enqueue in a bounded FIFO and apply the declared overflow policy.
- **Best-effort diagnostic:** drop, aggregate, or suppress under congestion and increment counters.
- **DRIP detail response:** retain a bounded cursor, pause generation, and resume when capacity returns.

Successful local enqueue is the earliest normal TX-state advancement point. A
protocol may deliberately require a later point, such as Service acceptance,
but Reliable transport and its commitment rules remain wholly TBD.

The Link Engine and Endpoint Router path should expose bounded capacity
notification and metrics such as occupancy, high-water mark, rejection, drop,
replacement, and completion timestamp where supported. Services must remain
correct if notifications are coalesced or delayed.

## 9. Pub/sub, redundancy, Broadcast, Taps, Relays, and Replicators

Pub/sub is brokerless and statically configured:

- each publication Wire has exactly one authoritative source Endpoint instance
  and one or more explicit semantic sink Endpoint instances;
- publication attribution is the source Endpoint instance of the resolved
  Wire, including its Endpoint Domain, `(Namespace, EndpointId)`, and
  configured Wire;
- local dispatch, Link fan-out, Route traversal, and Relay forwarding preserve
  that source lineage;
- any number of authorized local readers may consume a shared snapshot or
  bounded event queue when the sink Endpoint contract permits it;
- one source Endpoint may publish through several explicitly configured Wires;
- a Replicator consumes one Wire and originates a different Wire under new
  source authority and new lineage.

`RoutingWord`, Route `Direction`, ingress context, and endpoint metadata are
inputs to resolving the Wire; none is a substitute for source authority. A
known endpoint identity or physically received frame does not create a
subscription.

### Redundancy components

Redundancy is a Communication Component composed from multiple member Wires;
it is not a property of one primitive Wire. Member Wires normally share the
same source Endpoint identity because they carry redundant copies from one
semantic producer. The Wires remain distinct so the component can track
coverage, path health, failover, skew, and disagreement. A separately named
voting or replicated-source abstraction may coordinate genuinely independent
producers, but it is not ordinary path redundancy.

Duplicate suppression and stale-copy rejection use semantic generation,
sequence, epoch, or equivalent message-instance metadata owned by the
Communication Component or selected transport as applicable. Path identity,
arrival order, and different EndpointIds are not substitutes for this semantic
identity. The component defines duplicate, restart, disagreement, and
per-participant coverage behavior within finite state and resource bounds.

### Broadcast recipients and Taps

Broadcast is the one-source, one-to-many Wire case. Every Broadcast recipient
is an intended semantic sink explicitly listed by the Wiring Manifest and
participates according to the selected transport semantics. Broadcast does not
grant a recipient a reverse Wire, transmit authority, or reply authority.

A Tap receives a passive, receive-only copy of configured Wire traffic. A Tap
is not a semantic Wire sink or transport peer and gains no transmit, reply,
acknowledgment, retry, flow-control, Relay, replication, or source authority.
Tap selection is explicit and may be Endpoint- and Direction-scoped. Any Tap
buffering and loss behavior is bounded and observable. In particular, a Tap is
not a redundancy member, does not add path coverage, does not participate in
failover or duplicate suppression, and never counts as an independent delivery
path.

Electrical visibility is authority for neither role. A logger able to see a
CAN frame is not a Broadcast recipient or Tap unless the Wiring Manifest
explicitly configures that role. A physical broadcast medium may carry a
semantically single-sink Wire, while a Broadcast Wire may be realized over
point-to-point Links, several Route branches, or Relay fan-out.

Unreliable Datagram may support Broadcast. Point-to-point delivery state must
not be shared across Broadcast recipients. Recipient confirmations, when a
Service requires them, use separate return Wires unless a future
multicast-aware transport explicitly defines other semantics. A Tap never
participates in stateful transport behavior merely because it receives a copy.

### Relays and Replicators

A Relay carries authorized traffic through a configured Route while preserving
canonical routing semantics, Route `Direction`, Endpoint identity, Wire source
lineage, payload, and end-to-end transport semantics as applicable. It may
replace hop-local framing or a scoped `RoutingAlias`, but it is not thereby a
source, sink, or transport peer.

Every replication edge is explicit, bounded, observable, and checked for
cycles and conflicting producer authority. It declares eligible input message
types, input Wire and source lineage, output Wire and producer authority,
provenance representation, transformation, rate, freshness, failure, and
conflict policy. Commands, RPC requests or responses, Service errors, and
arbitrary traffic are forbidden on such an edge unless a Service defines
explicit proxy semantics, authority, correlation, failure behavior, and
bounds. Control-relevant state requires representable original provenance. A
Replicator does not create implicit reachability or general
Endpoint-Domain-to-Endpoint-Domain routing.

Dynamic subscriber discovery, runtime Wire or Route creation, a mandatory
broker, and implicit fan-out inferred from endpoint identity are outside the
base model. See [Endpoint Domains, Wires, Routes, and Endpoint
Access](endpoint_domains_wires_routes_and_access.md) for the controlling
Broadcast, Tap, Relay, Replicator, authority, and lineage semantics.

## 10. Provisional Domain Control

Domain Control is a small, local management-intent plane for one Endpoint
Domain. Control Groups contain Service instances rather than individual
Endpoints, Wires, Ports, or Communication Components, and groups may overlap.
Regardless of overlap, each Service has one effective desired Domain-Control
command.

`DomainControlCore` serializes accepted requests in one Endpoint-Domain order
and stamps them with generations. Callers do not provide authoritative
generations. A newer accepted generation supersedes an older applicable
request for a Service even when the requests came through different groups.
Convergence is not distributed atomic execution; a provisional
`StateSummary` may expose `Satisfied`, `Pending`, `Failed`, and `Superseded` so
an older request does not appear pending forever after replacement.

Gating or quiescing ordinary Service activity must leave the management path
needed to issue further Domain Control requests and observe convergence
reachable. Depending on deployment, that path includes the Core, an optional
remote Domain Control Service, its configured Wires, and required Link-control
or status lifelines.

A configured safety-favoring transition may be direct and idempotent. Which
direction is safety-favoring is deployment-specific. Guarded transitions and
narrowly exposed authority are future implementation directions; higher
assurance deployments may realize them with typed handles, object ownership,
partitions, privileges, or independent request sources. A rich
`DomainControlManager` may serialize product requests, enforce interlocks, and
hold product mode policy above the small Core. A safety-favoring path that
bypasses manager queuing still enters the Core's single acceptance order.

This section intentionally does not freeze APIs, tokens or epochs, command
values, a security model, guarded-transition steps or timing, remote protocol
encoding, or generation wrap/restart behavior.

## 11. Service-level errors and diagnostics

Service-level failure belongs to the service stream. A protocol may define an error-response message alongside request and success types, optionally using a reusable compact representation such as BRO. Useful categories include malformed request, unsupported version/type/operation, invalid argument or state, permission denied, busy, and rejected. Correlation data is included only where the protocol semantics require it.

Infrastructure faults such as no route, unknown endpoint, transport mismatch, malformed envelope, reassembly failure, and unexpected ingress are recorded by generic metrics/diagnostics even when the application endpoint is receive-only or does not implement error messages. They are not automatically converted into service responses.

Diagnostic containment is mandatory:

- an error report never recursively generates another remote error report;
- local counters and latched evidence remain the source of truth;
- remote reporting is optional, bounded, rate-limited, and normally lower priority than control traffic;
- repeated errors are aggregated and suppressed counts remain observable;
- malformed or babbling peers cannot force unbounded diagnostic work;
- ordinary endpoints do not receive every other node's errors.

DRIP is the preferred pattern: publish a small bounded summary periodically, then return larger bounded detail only on explicit request. Loss of the remote diagnostic path must not erase local evidence.

## 12. Safe use of the service catalog

The catalog in the source drafts is a set of candidates, not an automatic standard library or allocation authority. A service should be promoted only when it has a clear semantic boundary, bounded schema, explicit privilege model, compatibility metadata, resource profile, test vectors, and at least one real deployment.

Useful catalog guidance:

- MENU describes configured Endpoint instances; it does not enable them or
  create Wires or Routes.
- DEETS reports communication compatibility; BIO reports build identity; WHO reports hardware identity.
- CAPS, if retained, selects among finite precompiled profiles; it does not negotiate arbitrary encodings.
- TABS is suitable for disposable scalar prototypes. Features graduate to dedicated services when they need transactions, timing, richer state, or interaction.
- BRO is an optional service-error shape, not a substitute for local infrastructure diagnostics.
- TELL/DRIP-style logs and diagnostics are bounded and best effort; they never block critical work indefinitely.
- SUS, AMA, ROSE, fault injection, firmware update, raw capture/injection, and
  similar dangerous or privileged services are outside the initial prototype.
  They require a separate development build, build-time enablement, and
  local-only, non-forwardable, allowlisted operations. Runtime configuration
  alone cannot enable them.

Catalog names remain provisional until their contracts and endpoint allocations are published. The local/private security posture and gateway risks are discussed in [rationale, use cases, and risks](rationale_use_cases_and_risks.md).

## 13. Invariants and validation guidance

The following application-level invariants are mandatory:

1. A Service exposes typed directional Ports, while network-visible behavior
   resolves to configured Endpoint instances and directed Wires.
2. A Communication Component hierarchy flattens without creating implicit
   Wires, Routes, producer authority, or runtime graph construction.
3. `ParticipantId` is component-local and all multi-participant bookkeeping is
   independent of routing `PeerId`.
4. Redundant member Wires normally share one source Endpoint identity.
   Duplicate suppression uses component/transport semantic instance metadata,
   and Taps never contribute redundancy coverage.
5. Namespace-0 allocation follows the official ranges in Section 2.
   EndpointIds above 1023 remain canonical but cannot be placed directly on
   committed 11-bit CAN.
6. Domain Control gives every Service one effective desired command in the
   Core-stamped Endpoint-Domain order, and its management/observation path
   remains reachable while ordinary Services are gated.

Static validation and generated configuration must, as applicable:

- reject EndpointId 0, duplicate producers, public-range misuse, allocation
  collisions, and any Link placement that cannot exactly represent the
  canonical identity;
- distinguish compact allocation eligibility from complete encoded-message
  representability and reject committed-CAN EndpointIds above 1023 without
  truncation, implicit aliasing, or fallback;
- verify Port direction/type compatibility, complete Endpoint-role bindings,
  Direct/Composite Service flattening, finite component depth and resources,
  and complete `ParticipantId` role mappings;
- verify redundant-member source identity, per-sink coverage, semantic
  generation/sequence handling, duplicate and stale-copy behavior,
  same-generation disagreement handling, failover, and Tap exclusion;
- verify overlapping Control Groups, Core generation ordering, supersession
  summaries, bounded convergence state, and management-path reachability in
  every gate/quiesce state;
- include WireBand/routing representations, Endpoint allocation, component
  mappings, Control Groups, Link profiles, schemas, and relevant bounds in the
  generated compatibility fingerprint.

Executable fixtures and negative tests belong in [prototype and
validation](prototype_and_validation.md). Committed-Classical-CAN
representability and framing tests additionally follow the experimental [CAN
PDU adapter specification](can_pdu_adapter_spec.md); that profile remains
experimental and cannot override these canonical allocation or Service
semantics.

## 14. Required declaration for each service

Before a service is considered implementable, its specification records:

- roles, authority, Service Interface, typed Port directions, binding mode,
  permitted autonomous traffic, and separate Wires for every communication
  direction;
- whether it is Direct or Composite, its Communication Components, flattened
  Endpoint/Wire bindings, and any bounded component-local `ParticipantId`
  mappings to command, feedback, fault, or other Endpoint roles;
- Endpoint instances, Endpoint Domains, Wire Space scope, canonical EndpointId
  allocation region, and per-Link representability;
- message types, canonical schema, units, reserved fields, and serialization;
- protocol version, compatibility inputs, and profile identity;
- state, event, command, RPC, or object semantics;
- size/rate/burst bounds, freshness, ordering, loss, duplicate, and restart behavior;
- Control QoS default and permitted deployment overrides;
- bounded core-header-extension policy and total encoded-size budget;
- local snapshot/queue/concurrency policy;
- TX rejection, congestion, timeout, and overflow behavior;
- service-error and nonrecursive diagnostic behavior;
- transport, Link, Link Engine, IPC, and resource compatibility;
- source and sink Wires, Broadcast recipients, Taps, Relays, Replicators,
  source lineage, provenance, capture, and security policy;
- redundancy membership, common producer identity, semantic
  generation/sequence metadata, duplicate/stale/disagreement policy,
  per-participant coverage, and explicit Tap exclusion;
- Domain Control group membership and convergence behavior where managed,
  including supersession and the always-reachable management path;
- applicable `WireBand`, `RoutingCode`, `RoutingAlias`, Wiring Manifest, and
  generated-artifact compatibility constraints.

Reliable transport is wholly TBD. CAN and HDLC are experimental until their
owning documents define immutable profiles and golden vectors. No Service may
claim reliable or cross-Link wire interoperability merely by referring to this
document or an exploratory profile.

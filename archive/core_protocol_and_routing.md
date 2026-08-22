# Core Protocol and Routing

## 1. Status, authority, and scope

**Status:** Provisional core specification.

This document is normative for the prototype where it uses **SHALL**, **SHALL
NOT**, **SHOULD**, or **MAY**. It defines the canonical base descriptor,
`RoutingWord`, WireBands, endpoint identity, core routing validation,
peer binding, header extensions, and core error behavior. It does not freeze a
wire protocol.

[Endpoint Domains, Wires, Routes, and Access](endpoint_domains_wires_routes_and_access.md)
is authoritative for Endpoint Domain, Endpoint Router, Service, Endpoint,
Wire, Route, Direction, Broadcast, Tap, Relay, Replicator, Wiring Manifest,
endpoint authority, and source-lineage semantics. This document owns their
canonical routing representation and protocol consequences.

Companion specifications are:

- [Document map](README.md) — authority, terminology, maturity, and open gaps.
- [Architecture overview](architecture_overview.md) — scope, layering,
  deployment boundaries, and system-wide invariants.
- [Application protocols and services](application_protocols_and_services.md)
  — Endpoint contracts, service patterns, schemas, and application errors.
- [Logical links and transports](logical_links_and_transports.md) — Logical
  Link Layer contracts, transport behavior, QoS scheduling, and concurrency.
- [Link Engine runtime and status](link_engine_runtime_and_status.md) — Link
  Engine ownership, lifecycle, recovery, supervision, and status.
- [Experimental CAN PDU adapter](can_pdu_adapter_spec.md) — provisional
  Classical-CAN representation, framing, and reassembly.
- [Experimental HDLC logical-link profile](hdlc_logical_link_profile.md) —
  provisional serial/FIFO representation and framing.
- [Prototype and validation](prototype_and_validation.md) — implementation
  boundary, tests, conformance evidence, and promotion criteria.

Reliable transport remains wholly TBD. CAN and HDLC profiles remain
experimental. This document, by itself or in combination with those
experimental profiles, makes no wire-interoperability claim.

## 2. Canonical base descriptor

### 2.1 Logical allocation

The canonical base descriptor is exactly 40 bits:

```text
Control: 8 bits
    Namespace             2
    TransportType         3
    QoS                   2
    HasHeaderExtensions   1

RoutingWord: 16 bits
    WireBand              2
    Reserved              1
    RoutingCode          13

EndpointId:              16
--------------------------------
Base descriptor:         40 bits / 5 bytes
```

This is the canonical logical allocation. WireSpaces serialization order for
literal multi-byte numeric values is little-endian. Therefore, once a named
representation has fixed field significance, a literal five-byte base
descriptor is serialized as `Control`, low then high byte of `RoutingWord`,
and low then high byte of `EndpointId`.

This byte-order rule does not assign physical bit positions or field
significance within `Control` or `RoutingWord`; those packing details remain
open until an immutable named representation defines them. It also does not
require every Link representation to carry the literal five-byte descriptor.

A Logical Link Layer MAY carry the canonical fields literally, place some
fields in link-native metadata, represent a directly encodable subset, or omit
values that static configuration reconstructs unambiguously. Before core
routing, Endpoint dispatch, or Relay processing, it SHALL reconstruct the same
canonical values.

No link representation may silently truncate, reinterpret, or guess a
canonical field.

### 2.2 Control

`Control` contains endpoint namespace, selected transport, QoS, and extension
presence. None of those fields is part of `RoutingWord`.

`Namespace` is the 2-bit namespace component of endpoint identity.

`TransportType` selects a named transport profile allowed by the resolved Wire
and supported by every applicable Link and Relay. Numeric assignments and the
complete registry remain open. Unreliable Datagram is the only completed
transport contract. Sequenced/E2E-Protected Datagram remains provisional, and
Reliable transport remains wholly TBD.

The four canonical QoS values are:

```text
QoS 0   Background / Bulk
QoS 1   Normal
QoS 2   High Priority
QoS 3   Critical Priority
```

`Normal` is the default for ordinary Services. These values express relative
traffic intent at the canonical interface. Queueing, arbitration mappings,
weights, starvation behavior, admission, flow control, and timing analysis
belong to [Logical Links and Transports](logical_links_and_transports.md) and
the selected Link profile. A Link MAY map the canonical values to a different
native numeric ordering, but it SHALL preserve their meaning.

`HasHeaderExtensions` means only that an extension block immediately follows
the base descriptor. It does not identify any particular extension feature.

## 3. RoutingWord

### 3.1 First-class representation

`RoutingWord` is the canonical first-class 16-bit routing representation:

```text
RoutingWord: uint16_t
```

Conceptually:

```cpp
using RoutingWord = uint16_t;
```

The conceptual type states the value width. A representation that serializes
this numeric value literally SHALL use WireSpaces little-endian order.
Implementations MAY use a stronger wrapper type while preserving the exact
16-bit value domain.

Logically:

```text
RoutingWord = WireBand[2] + Reserved[1] + RoutingCode[13]
```

The Reserved bit SHALL be zero in every valid v1 value. A receiver SHALL reject
a nonzero Reserved bit.

`RoutingWord` equality compares the complete canonical routing
representation. It requires no QoS masking because QoS is in `Control`.

The pair `{WireBand, RoutingCode}` identifies a canonical **routing context**,
not Wire identity. One routing context MAY serve multiple Wires, and one Wire
MAY admit multiple routing contexts. Resolving a Wire additionally uses
Endpoint identity, Direction, ingress/terminal context, the configured Route,
and Wiring bindings. A `RoutingWord` is therefore neither a Wire, Route,
Endpoint identity, nor grant of authority.

### 3.2 WireBand

`WireBand` selects the interpretation of the 13-bit `RoutingCode`:

```text
Band 0   Standard structured routing
Band 1   User-defined
Band 2   User-defined; opaque/generated routing is permitted
Band 3   User-defined; opaque/generated routing is permitted
```

A deployment MAY define Band 1 by cloning the exact Band-0 decoder as a
separate bank. Such a clone:

- uses the same field widths and value meanings;
- has an independent `PathTag` allocation;
- is still a user-defined Band-1 contract;
- does not make Band 1 a second standardized band.

Bands 2 and 3 remain user-defined options and MAY use structured, opaque,
generated, aliased, or other explicitly specified routing semantics. A
user-defined WireBand SHALL have a named immutable definition, deterministic
validation, static compatibility checks, and an unambiguous reconstruction of
the canonical `RoutingWord`. Unsupported WireBands SHALL be rejected.

### 3.3 Band-0 RoutingCode

Band 0 defines one standard structured 13-bit `RoutingCode`:

```text
Direction   1 bit
PathTag     8 bits
PeerId      4 bits
------------------
Total      13 bits
```

This field list is logical. It does not define physical bit order.
Alternative splits are not Band-0 variants; they require a user-defined
WireBand.

`Direction` is the structural traversal orientation of a Route:

```text
A -> B
B -> A
```

The numeric association of the Direction bit with those orientations remains
part of the final packed-representation definition. Direction is independent of
requester/responder, client/server, producer/consumer, command/status,
initiator/responder, and similar Service roles.

`PathTag` is an 8-bit logical path or grouping selector:

```text
PathTag 0        Local
PathTag 1..254   Normal logical paths
PathTag 255      Invalid / reserved
```

`PathTag` is not a physical bus number, Link identifier, hop count, or
deployment-global Route name. The same logical path may use one Link, several
Links, different Link technologies, or Relays.

`PathTag::Local` selects local processing in the current Endpoint Domain. It
does not create a cross-domain Route. A Wiring Manifest SHALL NOT assign a
cross-domain Route to this value. The final Direction and `PeerId` constraints
for Local, including any reconstruction rules for local Link profiles, remain
open and SHALL be fixed before a wire profile claims interoperability.

`PeerId` is a 4-bit terminal selector:

```text
PeerId 0       Broadcast
PeerId 1..15   Individual peers
```

An individual `PeerId` resolves only in the selected WireBand, `PathTag`,
Direction, ingress/terminal context, Route, and Wiring Manifest. The number is
not a deployment-global Endpoint Domain identity.

## 4. Endpoint identity

`EndpointId` is always a canonical unsigned 16-bit value above the Logical Link
Layer:

```text
EndpointId: uint16_t
```

Canonical endpoint identity is:

```text
(Namespace, EndpointId)
```

For Namespace 0, the official allocation policy is:

```text
EndpointId 0            Invalid
EndpointId 1..32        Stable public/Common; optimized-capable
EndpointId 33..991      Deployment-owned
EndpointId 992..1023    Stable public/Common; General-only
EndpointId 1024..65535  Canonical, outside committed 11-bit CAN direct representation
```

Namespace-0 `EndpointId` 0 SHALL NOT identify an Endpoint. Allocation policy
for Namespaces 1 through 3 remains open.

Endpoint identity provides naming, not access permission, routing authority,
source authority, or physical location. Endpoint reachability requires an
explicit Wire and valid endpoint binding.

A constrained Link profile MAY directly represent only a subset of the
canonical value. Core defines no default `0..255` CAN representation and no
implicit endpoint aliasing. The current optimized and General direct ranges
and their other eligibility constraints are defined only by the
[Experimental CAN PDU adapter](can_pdu_adapter_spec.md). Values outside a
selected Link representation's capacity SHALL fail static validation or fail
locally at transmission; they SHALL NOT be truncated, aliased implicitly, or
reinterpreted.

## 5. Wires, Routes, and routing authority

### 5.1 Separation of semantics and representation

The architecture separates:

```text
Wire
    directed one-source communication semantics

Route
    cross-domain infrastructure that realizes Wires

RoutingWord
    canonical routing representation

WireBand
    interpretation of RoutingCode

RoutingCode
    band-specific routing selector

Routing context
    canonical {WireBand, RoutingCode} pair

RoutingAlias
    link-scoped alternate representation
```

A Route MAY have a human-readable name or generated key in a Wiring Manifest.
That name or key is manifest-local only, need not be stable across generated
manifests, has no required packet representation, and is not a
deployment-global protocol identity. Generated local table indexes have only
the scope of their generated artifact.

Canonical packet metadata, whether carried literally or reconstructed under a
named Link representation, states what a packet **claims** to be. The Wiring
Manifest validates whether the claimed combination is legal, where it may be
forwarded, and whether it may dispatch to an Endpoint. It SHALL NOT treat a
syntactically valid claim as authority.

Acceptance is set-valued where appropriate. A binding MAY admit sets or ranges
of routing contexts, transports, QoS values, message lengths and forms,
schemas, and Link representations. Constraints MAY couple those dimensions;
membership of each value in an independent set is insufficient when the
combined tuple is not admitted.

The Endpoint Router resolves Endpoint identity, Direction, routing context,
ingress/terminal context, Route, and the applicable Wiring bindings to a
permitted Wire realization. Physical receipt, electrical visibility, a known
endpoint key, or a syntactically valid `RoutingWord` is insufficient authority.

### 5.2 Direction and source attribution

Direction identifies a Route's structural source terminal and sink terminal.
It does not identify the authoritative source Endpoint by itself.

The authoritative source of a message is the source Endpoint instance of the
resolved Wire. Its source lineage includes:

```text
source Endpoint Domain
source Namespace + EndpointId
configured Wire
```

For cross-domain traffic, the Route and Direction establish the source
Endpoint Domain terminal. The resolved Wire and endpoint binding establish the
source Endpoint instance. `RoutingWord` and ingress context are evidence used
to resolve that configuration; they do not independently confer producer
authority.

Local dispatch, Link retransmission, Broadcast fan-out, and Relay forwarding
preserve source lineage. Tap delivery does not change it. A Replicator starts
new lineage under a separately authorized Wire.

### 5.3 RoutingAlias

A constrained Link MAY use a `RoutingAlias` as an alternate representation of
a canonical routing context. An alias:

- is scoped to its configured Link or named representation context;
- has no meaning outside that scope;
- SHALL be expanded or reconstructed before any core decision requiring the
  canonical `RoutingWord`;
- SHALL be generated or validated consistently at every participant;
- SHALL fail closed if unknown, ambiguous, colliding, missing, or mismatched.

An implementation SHALL NOT silently select another WireBand, reinterpret an
alias, or negotiate a fallback from traffic.

## 6. Broadcast, Tap, Relay, and Replicator

### 6.1 Broadcast

In Band 0, `PeerId::Broadcast` selects the statically configured semantic
recipients associated with the selected `PathTag`, Direction, Wire, and
routing context.

Broadcast:

- has exactly one authoritative Wire source;
- delivers to one or more explicitly configured semantic Wire sinks;
- does not imply a reverse Wire;
- does not grant a sink transmit or reply authority;
- does not dynamically discover recipients;
- does not mean every device electrically able to receive a frame;
- does not make every participant on a physical broadcast medium a recipient.

A physical broadcast medium may carry a semantically single-sink Wire.
Conversely, one Broadcast Wire may be realized over point-to-point Links,
several Route branches, or Relay fan-out.

Unreliable Datagram and a fully specified multicast-capable transport may
support Broadcast. Point-to-point reliable state SHALL NOT be shared across
Broadcast recipients. If recipient confirmations are required, they use
separately configured return Wires unless a future multicast-aware reliable
transport explicitly defines other behavior.

### 6.2 Tap

A Tap is a passive receive-only copy of configured Wire traffic. It is not a
semantic Wire sink and does not become a transport peer.

A Tap SHALL NOT gain transmit, reply, acknowledgment, retry, forwarding,
replication, flow-control, or source authority merely by receiving a copy. Tap
visibility may be Endpoint- and Direction-scoped and is always explicit in the
Wiring Manifest.

Broadcast recipients and Taps are distinct: Broadcast recipients are intended
semantic sinks; Taps only observe configured copies. Electrical visibility is
authority for neither role.

### 6.3 Relay

A Relay forwards traffic through a configured Route while preserving:

- canonical `RoutingWord` semantics;
- Route Direction;
- endpoint identity;
- Wire source lineage;
- end-to-end transport semantics and payload, as applicable.

A Relay MAY replace hop-local framing or link-scoped representations. It is not
the Wire source or sink and does not become an end-to-end transport peer merely
by forwarding. Communication with a gateway or Relay itself uses a separate
Endpoint and Wire.

### 6.4 Replicator

A Replicator consumes traffic and re-originates semantically new traffic under
a different Wire, routing context, or authority. It:

- is the authoritative source of each re-originated Wire;
- starts new source lineage;
- requires explicit source and sink authority;
- MAY transform, filter, aggregate, or independently schedule data according
  to its Service contract;
- SHALL NOT masquerade as the original source.

Each replication edge SHALL be explicit, bounded, observable, and checked for
cycles and conflicting producer authority.

## 7. Endpoint peer binding and replies

Peer binding is Endpoint Router and Wiring Manifest configuration, not another
canonical descriptor field. One Endpoint registration SHALL have exactly one
of these five modes:

1. **Static binding** — the permitted peer Wire, accepted routing-context set,
   endpoint key, transport set, and applicable ingress/egress constraints are
   fixed by generated configuration.
2. **Learned-from-ingress binding** — a bounded peer selection may be learned
   only from validated ingress that resolves to a pre-authorized Wire and
   allowable `RoutingWord`, endpoint, Link, interface, and origin constraints.
3. **Request-scoped binding** — a validated request yields an opaque
   `ReplyContext` for one operation or a bounded lifetime.
4. **Receive-only** — the Endpoint consumes configured traffic and has no
   network transmit peer.
5. **Transmit-only** — the Endpoint originates configured traffic and has no
   inbound peer binding.

Learned-from-ingress binding SHALL be disabled on unauthenticated multi-access
Links. It MAY be enabled for a statically single-peer ingress or a named
authenticated-origin profile, within a preconfigured allowlist.

Every learned mode SHALL define:

- finite entry capacity;
- finite lifetime and expiry;
- deterministic replacement or eviction;
- restart invalidation or restoration behavior;
- auditable learn, replace, expire, reject, and clear events.

Learning SHALL NOT create a Wire or Route, alter Route terminals, broaden
endpoint authority, change a `RoutingWord` interpretation, or authorize a
different transport.

An Endpoint needing request-scoped replies and autonomous transmission SHALL
use separate registrations or explicit bounded named sub-bindings with
independent authority and lifetime.

`ReplyContext` SHALL contain or reference enough validated router state to
select an explicitly configured reply Wire and construct its canonical
`RoutingWord`, endpoint identity, transport, QoS policy, and correlation state.
The reply Wire is distinct from the request Wire. A reply context SHALL NOT
infer authority merely by reversing the incoming Direction.

`ReplyContext` SHALL be:

- opaque to ordinary Service code;
- bounded in lifetime and storage;
- invalidated by documented restart and configuration changes;
- unusable for a different operation, Endpoint binding, or Wire;
- checked again when transmission occurs.

For a local request, the reply context selects an authorized local reply Wire;
it does not synthesize cross-domain infrastructure. Its exact API
representation remains open.

## 8. Header extensions

### 8.1 Presence and size

In a representation that carries the literal canonical header, when
`HasHeaderExtensions` is zero the header ends after the five-byte base
descriptor. When it is one, an extension block immediately follows the base
descriptor.

The first extension byte contains a 2-bit length code:

```text
ExtensionLengthCode 00   1 extension byte total
ExtensionLengthCode 01   2 extension bytes total
ExtensionLengthCode 10   3 extension bytes total
ExtensionLengthCode 11   Reserved
```

Therefore:

```text
literal base descriptor       5 bytes
maximum v1 extension          3 bytes
maximum literal v1 header     8 bytes / 64 bits
```

The length code determines only the total extension-block size. The presence
bit and length code do not indicate timestamps, security, fragmentation,
routing, or any other feature.

A parser SHALL reject length code `11` in v1 and SHALL reject a truncated or
overlong block relative to the selected named profile. Extension bytes are
part of the canonical header, not application payload.

### 8.2 Extension semantics

The remaining extension bits are available to separately standardized fields
or flags:

```text
1-byte block   6 bits after the length code
2-byte block  14 bits after the length code
3-byte block  22 bits after the length code
```

Large transport state, authentication data, timestamps, certificates, and
other feature-specific metadata should normally live in the applicable
transport, security layer, Service payload, or outer carrier.

The extension registry, field ordering, compatibility rules, and required
behavior for unknown but well-formed extensions remain open. In particular,
this version does not decide when an implementation may preserve and forward
unknown extensions or must reject them. A named profile SHALL define that
behavior before claiming interoperability.

## 9. Static validation

The Wiring toolchain SHALL reject invalid or ambiguous configurations.
Validation includes, as applicable:

- a descriptor allocation inconsistent with the 40-bit base definition;
- nonzero `RoutingWord` Reserved bits;
- unsupported `WireBand` values or user bands without immutable
  definitions and compatibility identifiers;
- `RoutingCode` values invalid under their selected WireBand;
- Band-0 `PathTag` 255;
- use of `PathTag::Local` as cross-domain Route infrastructure;
- Namespace-0 `EndpointId` 0, misuse of the public/Common or deployment-owned
  ranges, or any endpoint allocation outside its declared policy;
- a Link representation unable to preserve or reconstruct the complete
  canonical values;
- truncation of `EndpointId`, `RoutingWord`, or another canonical field;
- ambiguous or incompatible `RoutingAlias` mappings;
- packet metadata claims outside the Wiring binding's accepted sets or ranges,
  including a combination rejected by coupled constraints;
- a Wire with zero or multiple authoritative sources;
- a Wire with no semantic sink;
- an attempted bidirectional Wire instead of separate directed Wires;
- remote Endpoint reachability without an explicit Wire;
- an Endpoint reference not hosted in the stated Endpoint Domain;
- multiple implementations claiming the same externally producing
  `(Endpoint Domain, Namespace, EndpointId)`;
- ambiguous resolution for the same Endpoint identity, Direction, routing
  context, ingress/terminal context, Route, and Wiring bindings;
- a Route Direction inconsistent with its structural A/B terminals;
- invalid, unreachable, cyclic, or incomplete Route and Relay paths;
- a Broadcast recipient that is not an explicit semantic Wire sink;
- point-to-point reliable state applied to Broadcast recipients;
- a Tap that gains participant, transport-peer, or transmit authority;
- a Relay that changes source lineage or terminates end-to-end transport
  without being configured as another semantic role;
- a Replicator without explicit new-source authority or with a cyclic,
  conflicting, or unbounded replication edge;
- learned binding on an ineligible ingress, outside its allowlist, or without
  finite capacity, lifetime, eviction, and restart behavior;
- an endpoint binding incompatible with its Wire, `RoutingWord`, Namespace,
  `EndpointId`, transport, QoS, Link, or ingress/egress constraints;
- a malformed extension policy or use of reserved extension length code `11`;
- incompatible Link profile, PDU size, transport, buffer, flow-control, or QoS
  capability;
- inconsistent generated manifests, WireBand definitions, bindings, mappings,
  or compatibility fingerprints.

Route names and generated keys need be unique only in the manifest or artifact
scope that resolves them. Validation SHALL NOT impose deployment-global
protocol identity on those names.

Every deployed participant SHOULD expose a version, digest, fingerprint, or
equivalent integration check covering its applicable WireBands, Wires,
endpoint bindings, aliases, and Link capabilities. A mismatch SHALL fail
closed or enter an explicitly configured degraded mode; it SHALL NOT authorize
guessed semantics or silent fallback.

## 10. Runtime validation and error handling

Receivers, Link Engines, Relays, and Endpoint Routers SHALL validate canonical
packet claims against the Wiring binding's accepted sets and coupled
constraints, including ingress/terminal context, Wire authority, Route
Direction, endpoint binding, transport, extensions, bounds, and applicable
Link capabilities, before dispatch or forwarding.

Invalid traffic SHALL be dropped or rejected at the detecting boundary and
counted under a stable local reason. It SHALL NOT create routing state,
endpoint authority, peer bindings, reply authority, or partial Service
delivery.

Core reason categories SHOULD include:

- malformed or incomplete base descriptor;
- nonzero Reserved bit;
- unsupported `WireBand` or invalid `RoutingCode`;
- invalid `PathTag`, `PeerId`, Direction, or Local use;
- unknown Namespace or `EndpointId`;
- no Wire, ambiguous Wire, or source-authority mismatch;
- missing endpoint binding or unexpected ingress;
- unavailable Route, invalid next hop, or Relay-path failure;
- unsupported transport, QoS, Link, or Link-representation capability;
- alias or configuration-fingerprint mismatch;
- malformed, reserved-length, unsupported, or policy-rejected extension;
- Broadcast, Tap, Relay, or Replicator policy violation;
- learned-binding or `ReplyContext` violation;
- source or producer conflict;
- congestion, exhaustion, oversize PDU, reassembly failure, or Link failure.

Unknown but well-formed extension handling remains governed by the future
registry and named profile; it SHALL NOT be guessed from payload bytes.

Transmit congestion and temporary Route or Link unavailability are normal
runtime outcomes. A transmit API SHALL report non-acceptance explicitly. A
Service SHALL NOT advance protocol state as though rejected traffic entered
the transport. Retry, replacement, queueing, drop, and backpressure behavior
shall be bounded and defined by the selected Service, transport, and Link
policy.

Remote infrastructure-error reports are optional. If enabled, they SHALL be
bounded, rate-limited, aggregatable, and unable to recursively trigger another
infrastructure-error report. Local counters remain authoritative.

A Link or Link Engine MAY restart without resetting its Endpoint Domain.
Restart SHOULD discard uncertain transient framing, reassembly, queue,
learned-binding, and hop-local state while preserving static configuration and
the longer-lived state defined by the Link Engine runtime contract. Services
and transports own any required conversation recovery.

## 11. Core invariants

1. The canonical base descriptor is exactly 40 bits: 8-bit `Control`, 16-bit
   `RoutingWord`, and 16-bit `EndpointId`.
2. The allocation is logical. Literal multi-byte numeric serialization is
   little-endian, while physical bit packing remains representation-owned and
   open; a Link need not carry the literal five-byte descriptor.
3. `Control` contains `Namespace[2]`, `TransportType[3]`, `QoS[2]`, and
   `HasHeaderExtensions[1]`.
4. `RoutingWord` is a first-class `uint16_t` representation containing
   `WireBand[2]`, one mandatory-zero Reserved bit, and
   `RoutingCode[13]`.
5. QoS is not part of `RoutingWord`.
6. Band 0 has exactly one structured layout:
   `Direction[1] + PathTag[8] + PeerId[4]`.
7. Band-0 `PathTag` 0 is Local, 1..254 are normal, and 255 is invalid.
8. Band-0 `PeerId` 0 is Broadcast and 1..15 select individuals.
9. Band 1 is user-defined and may clone Band 0 only as a separate bank; Bands
   2 and 3 remain user-defined options that may be opaque.
10. Direction is structural A-to-B or B-to-A and is independent of Service
    roles.
11. `EndpointId` is canonical `uint16_t`; endpoint identity is
    `(Namespace, EndpointId)`.
12. Namespace-0 `EndpointId` 0 is invalid; 1..32 is stable public/Common and
    optimized-capable; 33..991 is deployment-owned; 992..1023 is stable
    public/Common and General-only; 1024..65535 remains canonical but is
    outside committed 11-bit CAN direct representation.
13. `{WireBand, RoutingCode}` is a routing context, not Wire identity. One
    context may serve multiple Wires, and one Wire may admit multiple contexts;
    resolution additionally uses Endpoint identity, Direction,
    ingress/terminal context, Route, and Wiring.
14. Packet metadata states claims; Wiring validates those claims against
    accepted sets, ranges, and coupled constraints and supplies authority.
15. Every Wire is directed, has exactly one authoritative source Endpoint
    instance, and has one or more explicit semantic sinks.
16. Route names and generated keys are manifest-local and have no required
    canonical packet identity.
17. Source attribution comes from the resolved Wire; Route Direction
    contributes structural source-terminal context.
18. Broadcast recipients are semantic Wire sinks and are distinct from Taps
    and electrical visibility.
19. A Tap is passive and receive-only, a Relay preserves source lineage, and a
    Replicator starts new lineage under a new Wire.
20. Peer binding has exactly one of five bounded modes, and request-scoped
    authority is represented by an opaque bounded `ReplyContext`.
21. In a literal-header representation, `HasHeaderExtensions` indicates only
    block presence; a v1 extension block is 1..3 bytes and the maximum literal
    v1 canonical header is 8 bytes.
22. Reserved values, ambiguous mappings, unsupported WireBands, and invalid
    authority fail closed.
23. All queues, learned state, reassembly, retries, diagnostics, and forwarding
    resources have explicit finite behavior.
24. Reliable transport remains TBD, CAN and HDLC remain experimental, and no
    wire interoperability is claimed.

## 12. Open issues

The following are explicit specification gaps:

1. Freeze physical bit positions, field significance, numeric Direction
   orientation, literal-descriptor packing, and immutable named representation
   versions. Literal multi-byte numeric serialization is already little-endian.
2. Complete allocation policy for Namespaces 1 through 3.
3. Freeze `TransportType` assignments and complete Sequenced/E2E-Protected and
   Reliable transport specifications; Reliable remains wholly TBD.
4. Complete Band-0 Local validation, including Direction, `PeerId`,
   ingress, egress, and reconstruction rules.
5. Define registration, compatibility, migration, and conformance rules for
   user-defined WireBands and opaque routing.
6. Define the header-extension registry, field ordering, compatibility rules,
   and unknown-extension preservation or rejection behavior.
7. Freeze Classical-CAN and HDLC representations, canonical-field mappings,
   reconstruction, framing, reassembly, integrity, restart, and conformance
   vectors.
8. Specify exact `ReplyContext` API, storage, lifetime, correlation,
   invalidation, and restart behavior.
9. Define immutable configuration-fingerprint coverage and mismatch handling.
10. Complete QoS scheduling disciplines, Link-native mappings, admission,
    flow-control interaction, and conformance requirements in the Logical Link
    specifications.
11. Specify multicast-aware reliable delivery, acknowledgments, retries,
    duplicate behavior, and resource ownership if such a transport is needed.
12. Define redundant Route selection, failover, duplicate suppression, and
    transport-state recovery policies.
13. Finalize generated Wiring Manifest schemas, including accepted-set and
    coupled-constraint representation, diagnostics, and routing API wrapper
    types.

No resolution may weaken explicit Wire authority, single-source lineage,
structural Direction, bounded resource behavior, or the distinctions among
Broadcast, Tap, Relay, and Replicator.

## Appendix A. Non-normative examples

### A.1 Point-to-point command and status

A manifest-local Route named `MainMotor` has structural terminals Main and
Motor. `MotorCommandWire` uses the Main-to-Motor Direction and
`MotorStatusWire` uses the Motor-to-Main Direction. They are separate Wires
even if both use the same Route, PathTag, and individual `PeerId`; their
opposite Directions make their routing contexts distinct.

The words "command" and "status" describe Service semantics. They do not
define Direction.

### A.2 Broadcast is not physical visibility

For a Band-0 `PathTag` named `Motors`, `PeerId::Broadcast` resolves through
the Wiring Manifest to Motor A, Motor B, and Motor C as semantic sinks of one
Wire. A logger electrically able to receive the same CAN frames receives
nothing semantically unless configured as a Tap or a sink of another Wire.

The three motors are recipients. The logger, if configured as a Tap, is only a
passive copy consumer.

### A.3 Band-1 clone

A deployment uses Band 0 for operational paths and defines Band 1 with the
exact Band-0 field decoder as a separate diagnostics bank. `PathTag` 7 in Band
0 and `PathTag` 7 in Band 1 are independent allocations.

This is a deployment-defined Band-1 contract, not standardized Band-1
semantics.

### A.4 Routing contexts are many-to-many

A Band-0 routing context is used by both `TemperatureWire` and `VoltageWire`.
Their different Endpoint identities and Wiring bindings resolve them to
different Wires. During migration, `TemperatureWire` accepts both that context
and a new Band-2 context; Wiring selects the permitted Route and
ingress/terminal binding for each.

The accepted receive-context set does not cause duplicate transmission. The
Wiring policy names the preferred transmit context and any bounded fallback.

### A.5 Relay across unlike Links

One Route crosses CAN, a gateway, and HDLC. The CAN and HDLC Links use
different local representations. The Relay reconstructs the canonical
`RoutingWord` at ingress, validates the packet's claims against Wiring,
preserves the resolved Wire, Direction, endpoint identity, source lineage,
transport semantics, and payload, then applies the egress representation.

The Relay is neither the source Endpoint nor an addressed transport peer.

### A.6 Replication

A Replicator receives `MotorStatusWire` and emits a bounded summary through
`FleetSummaryWire`. The summary has new producer authority and source lineage.
The configuration identifies the input, output, rate, freshness, conflict,
failure, and provenance policies.

### A.7 Request-scoped reply

An incoming request resolves to `ConfigurationRequestWire`. The Endpoint
Router creates a bounded `ReplyContext` that references the separately
authorized `ConfigurationResponseWire` and its canonical `RoutingWord`. The
Service cannot modify the context to target another Endpoint or Wire, and the
router revalidates it before transmission.

### A.8 Header extension bounds

In a literal-header representation, a descriptor with
`HasHeaderExtensions = 1` and length code `10` has three extension bytes and an
eight-byte canonical header. A v1 parser can locate the payload boundary even
when the future registry does not assign semantics it understands; the named
profile determines whether it preserves or rejects the unknown extension.

### A.9 Literal numeric serialization

After a named representation fixes field significance, a literal base
descriptor with `RoutingWord = 0x1234` and `EndpointId = 0xABCD` carries those
numeric fields as bytes `34 12` and `CD AB`. A Link representation such as
committed Classical CAN may instead carry and reconstruct selected canonical
fields without transmitting that literal five-byte sequence.

## 13. Supersession statement

This specification supersedes the legacy Routing Host and deployment-unique
`RouteId` model. `RouteId` is retired as canonical identity. New packets,
Wiring Manifests, APIs, and normative specifications use manifest-local Route
names only for configuration references and use `RoutingWord`,
`WireBand`, and `RoutingCode` for canonical routing representation.

The explicit legacy terminology replacements are:

```text
Routing Host          -> Endpoint Domain
Route Binding         -> Wire
Observer              -> Tap
transparent forwarder -> Relay
RoutingProfile        -> WireBand
RouteProfile          -> WireBand
RouteCode             -> RoutingCode
RouteAlias            -> RoutingAlias
Link Entity           -> Link Engine
```

The former `VCN`, `Index`, `UpstreamHost`, `UpstreamHostAlias`, `kLocalHost`,
`kMainCompute`, paired local-host markers, and derived runtime `ProducerKey`
models are also superseded and have no normative meaning.

Legacy names may appear in migration adapters, archived design history, and
this supersession section only. They SHALL NOT be reintroduced as canonical
identity, Wire authority, or new generated configuration vocabulary.

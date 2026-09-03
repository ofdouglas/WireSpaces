# WireSpaces Change Request — Global Participant Identity, Multi-Initiator Wires, and 48-bit Base Descriptor

**Status:** Proposed for architectural review; intended to become the implementation plan if accepted  
**Revision:** 3 — supersedes the earlier per-interaction-Origin draft  
**Date:** 2026-08-23  
**Scope:** Participant identity, Wire initiation semantics, canonical addressing, base PDU layout, Endpoint addressing, internal/device-private Wires, constrained Classical CAN 11-bit representation, and repository migration  
**Explicitly out of scope:** Mandatory source authorization, mandatory permitted-Origin tables, mandatory static deployment manifests, controller election/failover protocols, cryptographic authentication, multicast/group addressing, and a final generic header-extension format

---

# 1. Executive summary

This change request replaces the current **one-Origin / many-Node** Wire model and per-Wire `NodeId` identity with a simpler model:

```text
Endpoint Domain
    exactly one deployment-global ParticipantId

Physical device / SoC
    may contain one or more Endpoint Domains / Participants

Wire
    logical communication domain
    multiple Participants may independently transmit/initiate

canonical addressed PDU
    WireNumber
    SrcParticipantId
    DestParticipantId / broadcast
    EndpointAddress
    control metadata
```

The topology experiments support the architectural conclusion that a Wire should not permanently assign one Participant as Origin. They do **not** require the canonical protocol to preserve an Origin/Participant interaction anchor.

Accordingly, canonical routing becomes ordinary source/destination addressing. `Direction` is removed from the canonical PDU model and retained only where a constrained Link encoding needs it to reconstruct source and destination from asymmetric compressed address fields.

The new canonical base descriptor is deliberately expanded from **40 bits / 5 bytes** to **48 bits / 6 bytes**:

```text
QoS                    2
Namespace              2
HasHeaderExtensions    1
TransportType          3
WireNumber             10
SrcParticipantId       10
DestParticipantId      10
EndpointId             10
--------------------------------
Base descriptor:       48 bits / 6 bytes
```

This extra byte removes the need to aggressively ration canonical Participant, Wire, or Endpoint identity in ordinary deployments:

- approximately 1,000 Participant identities;
- 1,024 Wire numbers;
- 1,024 Endpoint IDs per Namespace;
- four Namespaces;
- no address-space extension required for realistic current targets.

The descriptor is intentionally straightforward rather than maximally compressed. Restricted Links may use more compact profile-specific representations.

A project-level design rule is also adopted:

> **All else being equal, prefer a simple protocol invariant that eliminates configuration over a configurable mapping that expresses the same thing.**

This rule directly informs the Classical CAN 11-bit profile: its compact participant fields encode restricted ranges of the canonical ParticipantId directly rather than introducing an alias table.

---

# 2. Decisions requested

Approve the following as the new strong/current architectural direction.

## 2.1 Deployment-global `ParticipantId`

Every Endpoint Domain has exactly one `ParticipantId` within one WireSpaces deployment / WireSpace identity scope.

Within that scope:

- distinct Endpoint Domains have distinct `ParticipantId` values;
- one Endpoint Domain uses the same `ParticipantId` on every Wire it participates in;
- a physical device or SoC containing several Endpoint Domains therefore has several Participant IDs;
- `ParticipantId` identifies the independently routed/dispatchable Endpoint Domain, not necessarily the manufactured physical device.

`ParticipantId` is not required to be globally unique across the world.

Physical-device identity remains separate. A replacement ECU may take over the same deployment Participant assignment while having a different UUID, serial number, hardware identity, or version inventory.

The assignment mechanism is not fixed by the base protocol. A ParticipantId may come from:

- a compile-time constant;
- generated deployment data;
- board straps or manufacturing configuration;
- boot-time assignment;
- commissioning;
- discovery;
- another profile-specific mechanism.

Static configuration is not required.

## 2.2 One ParticipantId per Endpoint Domain, including internal domains

The project adopts the following simple identity rule:

> **One independently routed Endpoint Domain equals one Participant.**

This applies even when several Endpoint Domains reside inside one multicore SoC or MCU.

Example:

```text
Central SoC
    Application Domain      ParticipantId 12
    Safety Domain           ParticipantId 13
    Networking Domain       ParticipantId 14
    Diagnostics Domain      ParticipantId 15
```

This makes IPC and off-device networking use the same identity model.

It also means logs, faults, health, and other platform data can identify the actual software/fault-containment domain that authored them instead of collapsing everything to one ECU identity.

The experiments identified a possible expressive cost: a single Endpoint Domain cannot expose several independent network source identities without being split into several Domains. Record this as a conscious trade and reopen condition in `REG`; do not silently erase it.

## 2.3 Retire canonical `Node` and `NodeId`

`Node` is retired as a formal Wire role.

`NodeId` is retired as canonical identity.

Do not carry a compatibility `NodeId` API into the first implementation.

Restricted Link profiles may represent only a subset of canonical Participant IDs, but that is a profile representability rule rather than a second canonical identity namespace.

## 2.4 Multi-initiator Wires

A Wire no longer has exactly one permanent Origin.

Multiple Participants may independently source traffic on one Wire.

The base protocol does not require:

- a `PermittedOrigins` table;
- an Endpoint/source ACL;
- a static Wire-membership table;
- a Manifest;
- an Organizer-generated authorization matrix.

A conventional master/controller system remains simple: only the controller happens to initiate the relevant traffic.

A peer/distributed system no longer needs an artificial permanent Origin merely to fit the Wire abstraction.

## 2.5 Canonical routing uses source and destination

The canonical PDU carries:

```text
SrcParticipantId
DestParticipantId
```

not:

```text
Origin
Participant
Direction
```

Examples:

```text
A -> B:
    SrcParticipantId  = A
    DestParticipantId = B

B -> A:
    SrcParticipantId  = B
    DestParticipantId = A

A -> broadcast:
    SrcParticipantId  = A
    DestParticipantId = kBroadcast
```

The base descriptor does not distinguish a request-scoped reply from a new independent interaction using routing fields alone.

If a Service or Transport needs request correlation, retry identity, transaction state, sequence state, epochs, or other interaction context, that belongs to the corresponding Service/Transport mechanism.

## 2.6 `Direction` is not canonical PDU semantics

Remove Direction from the canonical base descriptor and from the core Wire model.

A Link profile may still define a Direction bit where it provides real encoding value.

For the proposed Classical CAN 11-bit compact profile, Direction means only:

```text
CompactToGeneral:
    source = CompactParticipantId
    dest   = GeneralParticipantId

GeneralToCompact:
    source = GeneralParticipantId
    dest   = CompactParticipantId
```

This is an encoding mechanism, not request/reply, client/server, command/status, or authorization semantics.

## 2.7 Retain Wire as a first-class canonical routing concept

A Wire remains a logical communication domain that may:

- occupy one Physical Link;
- share a Physical Link with other Wires;
- span several heterogeneous Links;
- cross gateways through ordinary forwarding;
- have a different route, failure, membership, or broadcast scope from another Wire on the same medium.

Unlike the intermediate 40-bit redesign considered during this review, **WireNumber remains in the canonical base descriptor**.

The canonical PDU is therefore self-contained for ordinary forwarding.

---

# 3. Design principle: eliminate configuration with invariants

Add the following project-level design rule:

> **When two designs provide comparable capability and cost, prefer the one that eliminates configuration by defining a simple protocol invariant.**

This is not a ban on configuration.

Configuration is justified when it buys meaningful capability.

It does mean that alias tables, role maps, translation tables, generated ACLs, or negotiated state should not be introduced merely because they are more general than a simple rule that already covers the target use case.

Examples:

```text
Preferred baseline CAN11 rule:
    CompactParticipantId directly represents ParticipantId 0..3

Not preferred by default:
    arbitrary ParticipantId -> 2-bit OriginAlias table
```

and:

```text
Preferred:
    every Endpoint Domain has one stable ParticipantId

Not preferred:
    identity depends on which Wire the Domain is currently using
```

This principle belongs in `REG` or the project's design-principles section.

---

# 4. Canonical base descriptor

## 4.1 Base layout

The canonical base descriptor becomes **48 bits / 6 bytes**.

Normative field order:

| Field | Bits |
|---|---:|
| QoS | 2 |
| Namespace | 2 |
| HasHeaderExtensions | 1 |
| TransportType | 3 |
| WireNumber | 10 |
| SrcParticipantId | 10 |
| DestParticipantId | 10 |
| EndpointId | 10 |
| **Total** | **48** |

Conceptually:

```text
Control
    QoS                    2
    Namespace              2
    HasHeaderExtensions    1
    TransportType          3

Routing / addressing
    WireNumber            10
    SrcParticipantId      10
    DestParticipantId     10

Endpoint
    EndpointId            10
```

The encoded byte/bit packing must follow the project's canonical bit/byte ordering rules and be specified explicitly in `BITS` and conformance vectors.

The specification should not force artificial semantic names such as `RoutingWord` for arbitrary byte/word boundaries if the 10-bit fields cross those boundaries. The fields above are the normative abstraction.

## 4.2 Why 48 bits instead of preserving 40 bits

The 40-bit candidates forced premature tradeoffs among:

- Participant count;
- Wire count;
- Endpoint count;
- address-space extension bits;
- out-of-band Wire context;
- special casing of internal Endpoint Domains.

One additional byte makes those issues largely disappear in the base profile.

The intended target is a boring common case:

```text
~1000 Participants
~1000 Wires
~1000 Endpoints per Namespace
```

before any header extension is required.

For WireSpaces, the extra fixed byte is preferred over making the base addressing model depend on escape/extension mechanisms.

## 4.3 `ParticipantId`

Canonical Participant identity is 10 bits on the wire.

Preferred reservation:

```text
0..1022   ParticipantId
1023      broadcast destination encoding
```

`1023` is not a valid source Participant.

This gives 1,023 ordinary Participant identities within one deployment identity scope.

The implementation type should be at least `uint16_t`; the valid range remains a protocol invariant.

If review prefers a different broadcast sentinel, it must be chosen explicitly before codec implementation.

## 4.4 `WireNumber`

`WireNumber` is 10 bits:

```text
0..1023
```

This intentionally provides enough room for:

- machine-spanning application Wires;
- subsystem-local Wires;
- device-private/internal Wires;
- future system growth;

without making ordinary deployments depend on Wire-number extensions.

No special reduction of internal Wire count is needed merely to conserve the canonical field.

## 4.5 `EndpointId` and `EndpointAddress`

`EndpointId` becomes 10 bits per Namespace:

```text
0..1023
```

Existing EndpointId reservation rules, if any, should be migrated deliberately rather than silently changed.

`Namespace` remains 2 bits in the Control byte.

The semantic dispatch identifier remains a single `EndpointAddress` value in the API:

```text
EndpointAddress = { Namespace[1:0], EndpointId[9:0] }
```

Suggested logical representation:

```text
EndpointAddress = (Namespace << 10) | EndpointId
```

A `uint16_t` is the natural storage type even though only 12 bits are currently defined. The upper four bits remain zero/reserved in the semantic representation unless deliberately assigned later.

This yields:

```text
4 Namespaces
1024 Endpoint IDs per Namespace
4096 defined base EndpointAddress values
```

The project currently has no evidence that 1,024 Endpoint IDs in a single Namespace is a realistic constraint.

## 4.6 Control byte

The Control byte remains conceptually clean and unchanged in width:

```text
QoS                    2
Namespace              2
HasHeaderExtensions    1
TransportType          3
```

This preserves:

- four QoS classes;
- four Namespaces;
- eight TransportType values;
- explicit extension presence.

## 4.7 Header extensions

`HasHeaderExtensions` remains part of the base descriptor.

This change request deliberately **does not** allocate header-extension bits to extend Participant, Wire, or Endpoint identity. The 48-bit base descriptor is chosen specifically so realistic systems do not need extensions for basic addressing.

A bounded extension encoding was considered during design discussion, including a 1–3-byte form with an explicit size code, but is not adopted here.

The generic extension mechanism should be reviewed separately against concrete extension use cases.

Desired properties include:

- bounded parsing;
- inexpensive rejection of invalid encodings;
- no TLV machinery unless evidence requires it;
- reserved capacity for genuinely uncommon PDU metadata rather than merely finishing undersized base identifiers.

---

# 5. Internal and device-private Wires

## 5.1 Internal Wires remain useful

This change retains device-private/internal Wires as a useful first-class pattern.

They are not an alternative to assigning Participant IDs to internal Endpoint Domains.

Both concepts coexist cleanly:

```text
ParticipantId
    says WHO authored / receives the PDU

Wire
    says WHICH logical communication domain it belongs to

EndpointAddress
    says WHAT interface / service endpoint it addresses
```

## 5.2 `InternalDebugWire`

Retain `InternalDebugWire` as a recommended device-private Wire pattern.

A platform/library may automatically bind standard platform/debug publishers from each Endpoint Domain to this Wire, for example:

```text
TextLog
Event
Health
OS telemetry
version/build information
fault/crash records
Link Entity status
```

The exact service list remains a platform/service concern rather than a base-protocol invariant.

The important architectural property is that every internal publisher retains its own ParticipantId.

Example:

```text
PID 12 Application Domain  -> InternalDebugWire / TextLog
PID 13 Safety Domain       -> InternalDebugWire / Event
PID 14 Networking Domain   -> InternalDebugWire / LinkStatus
```

A logger can therefore identify the actual Endpoint Domain that authored each record without inventing a second component-identity field.

## 5.3 Splicing an internal debug Wire

A device-private debug Wire may be spliced/forwarded toward an external logging sink under the existing splice/forwarding model.

The splice must preserve canonical Participant source identity for ordinary forwarding.

Do not solve internal logging by collapsing all domains to one device-level ParticipantId and then adding another ad-hoc `ComponentId` namespace.

## 5.4 Internal Wire numbering

The 10-bit WireNumber space is intentionally large enough that internal/device-private Wires do not need a special compressed identity model merely to conserve numbers.

Whether device-private WireNumbers are locally scoped until spliced or deployment-global from the beginning should continue to follow/reconcile with the existing device-private Wire and splice rules. This change does not introduce a second Wire identity namespace solely for optimization.

---

# 6. Canonical routing and forwarding

The canonical forwarding identity is:

```text
WireNumber
SrcParticipantId
DestParticipantId / broadcast
EndpointAddress
```

with the Control metadata and payload.

## 6.1 Ordinary forwarding

Ordinary forwarding preserves:

```text
WireNumber
SrcParticipantId
DestParticipantId
Namespace
EndpointId
QoS
TransportType
header-extension semantics
payload
```

Only Link/profile-local representation changes.

## 6.2 Composition

If application code consumes one PDU and authors another, the new PDU naturally carries the composing Participant as source.

Example:

```text
Plant -> Gateway
    src  = Plant
    dest = Gateway

Gateway consumes objective

Gateway -> Leaf
    src  = Gateway
    dest = Leaf
```

The second PDU is composition, not forwarding.

No special re-origination field is required.

## 6.3 Request-scoped replies

A request-scoped reply API can capture enough receive context to produce:

```text
reply.src   = local ParticipantId
reply.dest  = received.src
reply.wire  = received.wire
```

plus whatever Service/Transport correlation state is required.

The base routing header does not need to preserve an interaction anchor to support this API pattern.

---

# 7. Broadcast

Canonical broadcast is represented as:

```text
SrcParticipantId  = actual source
DestParticipantId = kBroadcast
```

Preferred base value:

```text
kBroadcast = 1023
```

Broadcast is scoped by `WireNumber`.

It does not imply:

- every device electrically hearing the frame is a semantic member;
- every Participant implements the Endpoint;
- reliable multi-recipient acknowledgement;
- source authorization;
- multicast groups.

No multicast/group-address feature is added by this change.

---

# 8. Authorization and configuration boundary

## 8.1 No mandatory remote source authorization

This request does not define:

- Endpoint/source ACLs;
- permitted-Origin/source sets;
- authorization classes;
- Organizer-generated permission matrices.

A permissive development deployment must work without them.

The canonical source identity provides enough information for a future/application policy to make such a decision.

In particular:

```text
structurally valid PDU
    != authenticated sender
    != authorized operation
```

## 8.2 Preserve local binding authority

Do not remove the useful authority supplied by local APIs/bindings.

The intended distinction is:

```text
EndpointAddress
    naming

TX bindings / typed local access
    constrain what local code can transmit through a given API/binding

remote-source admission
    optional / outside this change
```

Existing request-scoped reply-binding rules remain useful.

The change removes permanent Wire-Origin authority; it does not remove all binding semantics.

## 8.3 Record source admission as an open optional capability

The experiments showed that serious deployments may benefit from auditable source admission.

Record this in `REG` rather than suppressing it:

> Optional standardized Wire- or Endpoint-level source admission may be useful. Its format, enforcement point, static/dynamic nature, and relationship to security remain open.

This is not a prerequisite for base communication.

## 8.4 Static configuration remains optional

No base communication rule may require:

- a static Manifest;
- an Organizer;
- static Wire membership;
- static source authorization.

A Link profile may impose deterministic representability constraints without requiring a configuration database.

---

# 9. Classical CAN 11-bit compact profile direction

Classical CAN 11-bit remains the main constrained stress profile.

It does **not** carry the full 48-bit canonical descriptor literally in its arbitration identifier. It projects the canonical addressing relation into a restricted direct representation.

## 9.1 Conceptual identifier budget

The current preferred budget is:

```text
QoS                         2
CompactParticipantId        2
GeneralParticipantId        5
Direction                    1
ProfileBit                   1
--------------------------------
                            11
```

The exact physical bit order remains a `LINK` decision, except that QoS must retain the intended arbitration significance.

## 9.2 Direct ParticipantId ranges; no Origin alias table

`CompactParticipantId` directly represents canonical:

```text
ParticipantId 0..3
```

`GeneralParticipantId` directly represents canonical:

```text
ParticipantId 0..30
```

with:

```text
31 = broadcast destination
```

These are not separate identities and not aliases.

Participants `0..3` are simply the canonical IDs that this profile can place in the compact two-bit field.

This eliminates an otherwise unnecessary alias/mapping configuration layer.

## 9.3 Representability rule

An ordinary addressed PDU is representable by this compact CAN11 form only if:

1. both Participants are in canonical ID range `0..30`; and
2. at least one of source/destination is in canonical ID range `0..3`.

Thus a CAN11 segment can naturally support approximately:

```text
4 compact/controller-like Participants
31 ordinary addressed Participant values
```

with bidirectional traffic between compact and general Participants.

Two Participants both outside `0..3` cannot directly communicate in this compact form.

That is a profile limitation, not a limitation of canonical WireSpaces.

## 9.4 Direction is profile-local compression information

Direction reconstructs source and destination:

```text
CompactToGeneral:
    src  = CompactParticipantId
    dest = GeneralParticipantId

GeneralToCompact:
    src  = GeneralParticipantId
    dest = CompactParticipantId
```

For broadcast:

```text
GeneralParticipantId = 31
Direction             = CompactToGeneral
```

Therefore broadcast sources on this profile must be compact-range Participants.

## 9.5 Deterministic encoding when both Participants are compact

If both source and destination are in `0..3`, the codec must have one canonical encoding rather than an arbitrary choice.

Preferred invariant:

```text
CompactParticipantId = min(src, dest)
GeneralParticipantId = max(src, dest)

Direction =
    CompactToGeneral if src == CompactParticipantId
    GeneralToCompact otherwise
```

This should be reviewed in `LINK`, but some deterministic no-configuration rule is preferred.

## 9.6 Coexistence profile

When the bus must coexist with non-WS 11-bit CAN traffic:

```text
ProfileBit = WS protocol discriminator
```

The deployment/user takes responsibility for establishing ParticipantId assignments compatible with the compact profile and for avoiding incompatible CAN-ID use on the shared bus.

This does not mean IDs must be manually assigned. They may come from generated firmware, manufacturing configuration, straps, or another provisioning mechanism.

It means **this profile does not provide WireSpaces in-band ParticipantId commissioning**, because the final identifier bit is being spent on coexistence.

## 9.7 WS-exclusive profile

When WireSpaces owns the CAN11 identifier space:

```text
ProfileBit = ordinary-data vs commissioning/control discriminator
```

The recovered bit may be used for the WireSpaces in-band commissioning/pre-addressing channel.

The exact commissioning algorithm remains a separate design item, but carry forward these requirements:

- unconfigured devices can participate without pretending to have a normal assigned ParticipantId;
- commissioning/control traffic is structurally separate from ordinary traffic;
- commissioning traffic should arbitrate below ordinary traffic, including ordinary Background traffic where practical;
- commissioning establishes canonical Participant IDs directly rather than creating a permanent alias layer.

The previously explored anonymous-response/prefix-search algorithm may be adapted, but is not frozen by this change.

## 9.8 Profile meaning is a Link invariant

The ProfileBit interpretation is not negotiated independently by each participant.

A bus/profile instance is one of:

```text
CAN11-WS-Coexistence
CAN11-WS-Exclusive
```

All WS participants on that bus use the same interpretation.

This is another deliberate preference for a profile invariant over runtime negotiation/configuration.

## 9.9 Wire identity on CAN11 remains a real profile design item

The compact 11-bit budget above has no old 3-bit `WireAlias` field.

This is a known cost.

The CAN11 profile must still determine how `WireNumber` is represented when multiple logical Wires share one physical CAN bus.

Possible approaches include:

- one Wire per particular compact CAN profile instance;
- Wire selection in PDUA/payload framing;
- another profile-specific context mechanism;
- a separate richer CAN profile.

Do not silently collapse Wire semantics merely to fit the arbitration ID.

This is one of the principal remaining `LINK` design questions after the core change.

## 9.10 Arbitration constraints

The final CAN field ordering must preserve:

- four QoS classes;
- deterministic arbitration;
- intended low priority for commissioning/control traffic;
- no accidental reintroduction of configuration solely to arrange priority.

The topology experiments deliberately did not price identifier-bit cost, so the CAN11 profile must be reviewed on its own merits.

---

# 10. Wire boundaries and degraded state

Multi-initiator adoption changes the guidance for creating Wires.

Do not create a new Wire merely because a different Participant produces or initiates a Service.

Stronger reasons to split Wires include:

- different route/forwarding realization;
- different meaningful Participant scope;
- different physical or operational failure boundary;
- different broadcast scope where Wire-wide broadcast is actually used;
- a real composition boundary.

Different Endpoint recipient sets alone are not automatically a reason to split.

However, broader Wires have a real cost observed in the experiments:

> A Wire can remain configured-valid while gateway or Link failure partitions which Participants can currently reach one another.

Therefore future diagnostics/tooling should not assume a single Wire `up/down` flag captures all degraded states.

Useful distinctions may include:

```text
Participant availability
Link/segment availability
forwarding-edge availability
reachable Wire component(s)
Service/runtime authority availability
```

No new base-protocol mechanism is required by this change.

---

# 11. Redundancy and runtime controller ownership

Retire the old implication:

```text
one Origin per Wire
therefore redundant controllers require separate Wires
```

Multiple potential controllers may share one Wire.

If only one is valid at a time, the accepted controller/term/epoch is application or Service state unless a future feature deliberately standardizes it.

WireSpaces does not provide in this change:

- leader election;
- term allocation;
- split-brain prevention;
- physical multipath redundancy;
- duplicate suppression;
- automatic controller failover.

The BESS experiment supports the desired layering:

```text
Participant identity    unchanged
Wire identity           unchanged
Wire membership         unchanged
routing                  unchanged
active controller        changes in Service/runtime state
```

Multi-initiator semantics do not by themselves imply physical redundancy.

---

# 12. Structural validity

Base structural validation covers protocol/encoding correctness, including:

- valid QoS/Transport values;
- valid Namespace and EndpointId representation;
- valid WireNumber range;
- valid source ParticipantId;
- destination is a valid ParticipantId or broadcast;
- broadcast is never used as source;
- header-extension presence is encoded consistently;
- no silent truncation/remapping occurs.

For CAN11 compact specifically, structural validity additionally includes:

- compact Participant field is `0..3`;
- general Participant field is `0..30` or broadcast `31`;
- at least one addressed endpoint is compact-range;
- broadcast uses the permitted profile form;
- both-compact canonical encoding rule is obeyed;
- ProfileBit interpretation matches the selected Link profile.

The following are not base structural validity:

- source appears in an authorization table;
- source is the current redundant-controller leader;
- source is allowed by security policy;
- Participant appeared in a static Manifest.

---

# 13. Library/API consequences

Suggested semantic types:

```cpp
struct ParticipantId {
    uint16_t value;  // valid canonical range defined by protocol
};

struct WireNumber {
    uint16_t value;  // 10-bit canonical range
};

struct EndpointAddress {
    uint16_t value;  // Namespace[1:0] + EndpointId[9:0]

    Namespace name_space() const;
    uint16_t endpoint_id() const;
};

struct PduDescriptor {
    Qos qos;
    bool has_header_extensions;
    TransportType transport;

    WireNumber wire;
    ParticipantId src;
    ParticipantId dest;
    EndpointAddress endpoint;
};
```

Exact wrapper style is a `LIB` implementation matter.

Important consequences:

- no canonical `NodeId` type;
- no canonical `Direction` enum;
- source metadata is directly `descriptor.src`;
- WireNumber remains directly available in the descriptor;
- Endpoint dispatch uses one `EndpointAddress` semantic value;
- CAN-profile-local Direction may use a separate profile-specific enum.

Example profile-local type:

```cpp
enum class Can11Direction {
    kCompactToGeneral,
    kGeneralToCompact,
};
```

---

# 14. Experimental evidence and evidence limits

## 14.1 Global ParticipantId

The multicore gateway remaps showed material benefit from global Endpoint-Domain identity even where multi-initiator behavior contributed little.

This supports Participant identity independently from the Wire initiation change.

## 14.2 Peer CAN

The peer-CAN case showed that permanent single-Origin topology creates artificial structure in genuinely peer-like systems.

## 14.3 AMR

The AMR generalized the result beyond a flat peer bus: several production controllers naturally initiate different interactions across hierarchical physical topology.

## 14.4 Excavator

The excavator is physically hierarchical but still contains VCU-, Safety-, BMS-, gateway-, and leaf-initiated interactions.

This is strong evidence that multi-initiator Wires are not merely a peer-network feature.

## 14.5 BESS

The BESS showed that controller failover can change runtime acceptance without changing Participant identity, Wire identity, or routing.

## 14.6 What the experiments do not prove

The remap experiment assumed an Origin/Direction model and explicit origin-admission policy as an accounting device.

It therefore does not prove that either mechanism belongs in the canonical protocol.

This change keeps the experimentally supported topology conclusion — multiple independent initiators on one Wire — while simplifying canonical addressing to source/destination.

The experiments also deliberately fenced off bit-layout cost, so the 48-bit descriptor and CAN11 representation require independent architectural review.

The planned RS-485 negative-control remap and old redundant-gateway remap were not completed. Record this as an evidence limitation rather than hiding it.

---

# 15. Required repository changes

## 15.1 `README.md`

Replace the one-Origin / many-Node summary with:

- Endpoint Domain / Participant identity;
- multi-initiator Wire semantics;
- source/destination addressing;
- 48-bit base descriptor at the appropriate level of detail;
- constrained Link representations as profile-specific.

## 15.2 `docs/introduction.md` (`INTRO`)

Update the mental model and examples.

Show both:

1. a conventional one-controller system;
2. a distributed/multi-initiator system.

Make clear that the second does not require mandatory authorization configuration.

## 15.3 `docs/core_architecture.md` (`CORE`)

### Terminology

Retire as canonical roles/fields:

```text
Origin
Node
NodeId
OriginToNode
NodeToOrigin
OriginToParticipant
ParticipantToOrigin
```

Use:

```text
Endpoint Domain
Participant
ParticipantId
source
Destination
Wire
broadcast
```

`origin` may remain ordinary English where it does not imply a formal permanent role.

### Wire model

Remove exactly-one-Origin invariants.

State that multiple Participants may independently source traffic on one Wire.

### Canonical PDU

Adopt the 48-bit descriptor in §4.

### Endpoint identity

Keep Namespace and EndpointId as encoded fields but expose their combination as `EndpointAddress` in the semantic/API model.

### Internal Wires

Retain device-private/internal Wire semantics and document the `InternalDebugWire` pattern without making its exact service list a core-protocol requirement.

### Routing and forwarding

Route on Wire + destination Participant, while preserving source.

Rewrite forwarding/composition examples in source/destination terms.

### Static configuration

Remove wording that implies only static configuration can establish valid communication.

## 15.4 `docs/architecture_register.md` (`REG`)

This change is incomplete until `REG` is updated.

### New strong/current facts

Record:

```text
one ParticipantId per Endpoint Domain
ParticipantId stable across Wires in one deployment
multi-initiator Wires
canonical source/destination addressing
48-bit / 6-byte base descriptor
WireNumber 10
SrcParticipantId 10
DestParticipantId 10
Namespace 2
EndpointId 10
QoS 2
HasHeaderExtensions 1
TransportType 3
```

### Superseded facts

Retire:

- one Origin per Wire;
- formal Node role;
- per-Wire canonical NodeId;
- canonical Direction;
- old 40-bit descriptor;
- old `RoutingWord = NodeId + Direction + WireNumber`;
- 5-bit NodeId / 31-Node limit;
- current CAN11 `QoS + Direction + WireAlias + NodeId` allocation;
- preferred NodeId 30/31 conventions tied to the old model.

Do not reuse retired invariant IDs for changed meanings.

### Findings/open questions to retain

Record:

- optional standardized source/Endpoint admission may be useful but is not required;
- one ParticipantId per Endpoint Domain may be limiting if a future Domain needs several independent source identities;
- broader Wires require richer degraded/reachability reporting than one `up/down` flag;
- CAN11 multi-Wire representation remains unresolved because the old WireAlias bits are no longer available in the preferred compact address budget;
- generic header-extension structure remains open;
- the experiments validated multi-initiator topology, not mandatory authorization policy.

### Design principle

Add the configuration-elimination rule from §3.

## 15.5 `docs/bit_layout.md` (`BITS`)

Replace the 40-bit descriptor with the 48-bit field layout in §4.

Specify exact canonical bit/byte order and masks.

Remove canonical NodeId and Direction.

Add golden examples including cross-byte 10-bit field boundaries.

## 15.6 `docs/link_profiles.md` (`LINK`)

Rework Classical CAN 11-bit around §9.

Define/consider two profile variants:

```text
CAN11-WS-Coexistence
CAN11-WS-Exclusive
```

Do not introduce an Origin alias table.

Carry forward commissioning-priority requirements.

Treat CAN11 Wire representation/multiplexing as an explicit remaining design item.

Revalidate PDUA/CRC/aggregation details that depended on the old CAN identifier layout.

## 15.7 `docs/deployment.md` (`DEPLOY`)

Replace per-Wire NodeId allocation with deployment-global ParticipantId assignment.

Strengthen that static configuration remains optional.

Document that every Endpoint Domain, including internal domains on a multicore SoC, has its own ParticipantId.

For CAN11 coexistence:

- deployment/user is responsible for establishing profile-compatible IDs;
- no in-band WS commissioning is provided by that profile.

For CAN11 exclusive:

- WS may provide in-band ParticipantId commissioning.

Do not add a generic canonical Participant alias layer without a concrete need.

## 15.8 `docs/library_architecture.md` (`LIB`)

Update types/examples to §13.

Remove canonical Direction and NodeId from the core descriptor.

Add `EndpointAddress` semantic type.

Make CAN Direction profile-local.

## 15.9 `docs/conformance.md` (`CONFORM`)

Retire old 5-byte/NodeId/one-Origin vectors.

Add base descriptor vectors covering:

- all four QoS values;
- all Namespaces;
- extension-presence bit;
- TransportType boundaries;
- WireNumber 0 and 1023;
- ParticipantId boundaries;
- broadcast destination;
- EndpointId 0 and 1023;
- cross-byte packing of the 10-bit fields.

Add semantic tests:

1. multiple Participants can source traffic on one Wire;
2. A->B and B->A require no Wire role reconfiguration;
3. forwarding preserves Wire/src/dest/Endpoint identity;
4. composition changes source naturally;
5. one Endpoint Domain keeps the same ParticipantId across Wires;
6. several Endpoint Domains on one device have distinct ParticipantIds;
7. InternalDebugWire publications retain each domain's ParticipantId;
8. permissive development communication requires no ACL or Manifest.

Add CAN11 vectors after the profile is frozen.

## 15.10 `docs/implementation.md` (`IMPL`)

Update memory/capacity assumptions for:

- 10-bit Wire numbers;
- 10-bit Participant IDs;
- 10-bit Endpoint IDs;
- 6-byte canonical base descriptor;
- one Participant per Endpoint Domain.

Measure implementation storage separately from encoded size; do not assume a 6-byte descriptor implies a 6-byte aligned C++ struct.

## 15.11 `docs/future_work.md` (`FUTURE`)

Update redundancy text and retain optional source authorization as future work rather than a prerequisite.

Track generic header-extension design only when concrete extension uses justify it.

## 15.12 Provisioning notes

Rewrite NodeId provisioning around ParticipantId.

Do not blindly translate old NodeId broadcast-sentinel mechanisms into canonical Participant semantics.

For exclusive CAN11, adapt the commissioning design around the dedicated ProfileBit/control subspace.

For coexistence CAN11, state explicitly that the profile has no WireSpaces in-band Participant commissioning.

## 15.13 `history.md`

Record the semantic and encoding break, rationale, and affected documents.

No compatibility shim is required while the project still has no frozen interoperability commitment and no implemented canonical PDU stack requiring migration.

---

# 16. Proposed replacement invariants

Final IDs belong in `REG`; retired IDs must not be reused.

## Participant identity

**PARTICIPANT-A**

> Every Endpoint Domain has exactly one ParticipantId within a WireSpaces deployment identity scope and uses it consistently across all Wires it participates in.

**PARTICIPANT-B**

> ParticipantId identifies an Endpoint Domain, not necessarily a physical device. Several Endpoint Domains on one device have distinct ParticipantIds.

**PARTICIPANT-C**

> The canonical ParticipantId field is 10 bits. The all-ones destination value is reserved for broadcast if the preferred broadcast encoding is accepted and is invalid as a source.

## Wire semantics

**WIRE-A**

> A Wire is a logical communication domain of Participants and has no permanent single-Origin role.

**WIRE-B**

> Multiple Participants may independently source traffic on one Wire.

**WIRE-C**

> Device-private/internal Wires are ordinary logical Wires and may contain several independently identified Participants.

## PDU addressing

**PDU-A**

> The canonical base descriptor directly carries WireNumber, source Participant, destination Participant/broadcast, Namespace, EndpointId, QoS, TransportType, and extension presence.

**PDU-B**

> Direction is not canonical PDU semantics. A Link profile may use Direction only as necessary to reconstruct canonical source/destination.

**PDU-C**

> The canonical base descriptor is 48 bits / 6 bytes with 10-bit WireNumber, source ParticipantId, destination ParticipantId, and EndpointId fields.

## Forwarding

**ROUTE-A**

> Ordinary forwarding preserves WireNumber, source Participant, destination Participant/broadcast, EndpointAddress, QoS, TransportType, applicable extensions, and payload.

**ROUTE-B**

> Application composition consumes one interaction and creates another; the composing Participant is the source of the new PDU.

## Configuration

**CONFIG-A**

> Static Manifest/Wiring is not required for base communication.

**CONFIG-B**

> Remote source authorization is not required for structural PDU validity.

**CONFIG-C**

> All else being equal, a simple protocol invariant that eliminates configuration is preferred over an equivalent configurable mapping.

## CAN11 compact profile

**CAN11-A**

> CompactParticipantId directly represents canonical ParticipantIds 0..3; it is not an alias.

**CAN11-B**

> GeneralParticipantId directly represents canonical ParticipantIds 0..30; value 31 represents broadcast.

**CAN11-C**

> An addressed compact-profile frame is representable only if at least one endpoint is in ParticipantId range 0..3 and both endpoints are in range 0..30.

**CAN11-D**

> The CAN11 Direction bit only determines whether the compact or general participant is the canonical source.

**CAN11-E**

> Coexistence and WS-exclusive CAN11 buses are distinct Link profiles with one common ProfileBit interpretation per bus.

---

# 17. Known costs and open questions

## 17.1 One ParticipantId per Endpoint Domain

Accepted provisionally because it keeps identity aligned with the routing/dispatch/fault-containment domain.

Reopen if a concrete system needs several independent source identities inside one Endpoint Domain without a meaningful Domain split.

## 17.2 Broadcast sentinel

Preferred canonical destination value:

```text
1023
```

Review before codec freeze.

## 17.3 Generic header-extension format

The base header keeps an extension-presence bit, but the generic extension encoding is not frozen by this CR.

Do not allocate extension address bits merely because the mechanism exists.

## 17.4 CAN11 Wire representation

The preferred compact CAN identifier spends its available bits on QoS, asymmetric Participant representation, Direction, and ProfileBit.

How several Wires share one physical 11-bit CAN Link remains unresolved.

This must be solved without discarding the logical Wire abstraction by accident.

## 17.5 Endpoint/source authorization

Potentially useful for production/auditing; deliberately not designed as a mandatory base feature.

## 17.6 Degraded reachability

Broad multi-Link Wires may be partitioned by failures while remaining configured.

Tooling/telemetry likely needs richer reachability state than one Wire Boolean.

## 17.7 Internal Wire scope

Retain the existing device-private Wire concept and `InternalDebugWire` pattern.

If future pressure appears around WireNumber assignment across many devices, revisit whether private Wire numbers need local scope before splicing. The 10-bit field means there is no current need to invent such complexity for capacity reasons.

---

# 18. Implementation sequence

## Step 1 — approve the semantic and base-layout change

Review together:

- one ParticipantId per Endpoint Domain;
- multi-initiator Wire semantics;
- source/destination canonical addressing;
- 48-bit descriptor;
- 10-bit Wire/src/dest/EID fields;
- InternalDebugWire/internal-domain treatment;
- optional authorization boundary;
- CAN11 direct-range compression direction.

## Step 2 — update `REG`

Retire old invariants and record open findings before implementation details drift.

## Step 3 — update `CORE`, `INTRO`, `README`, `DEPLOY`

Remove permanent Origin/Node semantics comprehensively.

## Step 4 — update `BITS` and `CONFORM`

Freeze exact 48-bit packing and golden vectors.

## Step 5 — update `LIB`

Implement canonical ParticipantId, WireNumber, EndpointAddress, and PDU descriptor types directly.

Do not implement a NodeId/Direction compatibility layer.

## Step 6 — rework `LINK`

Specify the CAN11 compact participant encoding and profile split.

Resolve CAN11 Wire representation before declaring that profile complete.

## Step 7 — implement the first vertical slice

Use the new canonical descriptor in host simulation/core routing and Endpoint dispatch.

Prove simple permissive operation before adding optional deployment policy.

## Step 8 — implement constrained Link profiles

Add CAN11 only after its remaining Wire/commissioning details are reviewed.

---

# 19. Review checklist

## Core identity/model

- [ ] Is one ParticipantId per Endpoint Domain accepted, including multiple domains on one SoC?
- [ ] Is ParticipantId clearly distinct from physical-device identity?
- [ ] Can multiple Participants independently source traffic on one Wire?
- [ ] Does a conventional single-controller system remain simple?
- [ ] Is source/destination sufficient as the canonical routing relation?
- [ ] Is canonical Direction removed?

## Base descriptor

- [ ] Is 48 bits / 6 bytes accepted?
- [ ] Are `WireNumber`, `SrcParticipantId`, `DestParticipantId`, and `EndpointId` each 10 bits?
- [ ] Are QoS=2, Namespace=2, H=1, TransportType=3 accepted?
- [ ] Is approximately 1,000 of each primary routing resource sufficient without extensions?
- [ ] Is `1023` accepted as destination broadcast or replaced explicitly?
- [ ] Is `EndpointAddress` retained as the semantic `{Namespace, EndpointId}` value?

## Internal Wires

- [ ] Are device-private/internal Wires retained?
- [ ] Does each internal Endpoint Domain retain its own ParticipantId?
- [ ] Is `InternalDebugWire` retained as a recommended standard pattern?
- [ ] Can it be spliced/forwarded to a logging sink without inventing a second component identity?

## Configuration/policy

- [ ] Can development communication work without a Manifest?
- [ ] Can it work without a source ACL/permitted-Origin table?
- [ ] Are local TX/reply bindings still allowed to constrain local code?
- [ ] Is optional remote source admission recorded rather than suppressed?
- [ ] Is the invariant-over-configuration design principle accepted?

## CAN11

- [ ] Is direct canonical PID `0..3` compact representation accepted?
- [ ] Is direct canonical PID `0..30` general representation accepted?
- [ ] Is `31` broadcast accepted for the profile?
- [ ] Is Direction clearly profile-local?
- [ ] Is the at-least-one-compact-participant constraint acceptable?
- [ ] Is a deterministic both-compact encoding required?
- [ ] Are coexistence and WS-exclusive modes separate Link profiles?
- [ ] Is the ID-assignment responsibility boundary clear?
- [ ] Is the missing WireAlias capacity explicitly tracked?
- [ ] Is commissioning/control priority retained as a requirement?

## Repository consistency

- [ ] No current normative one-Origin-per-Wire claims remain.
- [ ] No canonical NodeId remains.
- [ ] No canonical Direction remains.
- [ ] All current descriptor-size claims say 48 bits / 6 bytes.
- [ ] `REG`, `CORE`, `BITS`, `LINK`, `LIB`, `DEPLOY`, and `CONFORM` agree.
- [ ] `history.md` records the break.

---

# 20. Search terms for migration review

Search the non-archive repository for at least:

```text
exactly one Origin
one Origin
Origin role
OriginToNode
NodeToOrigin
OriginToParticipant
ParticipantToOrigin
NodeId
NodeId 0
zero or more Nodes
31 Nodes
5 bytes
40 bits
RoutingWord
WireAlias
Namespace
EndpointId
configured Nodes
unique NodeIds
NodeId collision
relocate its Origin
permitted Origin
PermittedOrigins
InternalDebugWire
```

Every remaining match should be intentionally:

- history/superseded design;
- optional future policy;
- CAN-profile-local Direction;
- or current revised terminology.

---

# 21. Acceptance effect

If accepted:

1. Every Endpoint Domain has one deployment-global ParticipantId, including domains inside one multicore SoC.
2. Canonical Node/NodeId is removed.
3. A Wire has no permanent Origin and may have multiple independent initiators.
4. Canonical addressing becomes source Participant + destination Participant/broadcast.
5. Canonical Direction is removed.
6. The canonical base descriptor becomes 48 bits / 6 bytes.
7. WireNumber, source ParticipantId, destination ParticipantId, and EndpointId each receive 10 bits.
8. QoS remains 2 bits, Namespace 2 bits, `HasHeaderExtensions` 1 bit, and TransportType 3 bits.
9. `EndpointAddress` remains the semantic combination of Namespace and EndpointId.
10. Device-private/internal Wires remain part of the architecture.
11. `InternalDebugWire` remains a recommended pattern; internal domains publish under their own Participant identities and may be forwarded/spliced toward logging infrastructure.
12. Header extensions remain available but are no longer needed merely to rescue undersized base routing fields; their generic format remains a separate decision.
13. Static deployment configuration and remote source authorization remain optional.
14. Local TX/reply bindings continue to provide local capability constraints.
15. WireSpaces adopts the design preference to eliminate configuration with simple protocol invariants when capability/cost are otherwise comparable.
16. Classical CAN 11-bit uses direct restricted canonical ParticipantId ranges rather than Participant aliases.
17. CAN11 Direction exists only to reconstruct source/destination from asymmetric compact/general fields.
18. CAN11 coexistence spends the final identifier bit on a WS discriminator and relies on externally established compatible Participant IDs.
19. CAN11 WS-exclusive operation may spend that bit on an in-band commissioning/control subspace.
20. CAN11 multi-Wire representation remains a deliberate follow-on design item rather than being silently dropped.
21. Controller election/failover, mandatory source admission, multicast, and generic extension encoding remain outside this core change.

The intended architectural direction is:

> **WireSpaces should give every independently routed Endpoint Domain a stable identity, carry ordinary source/destination addressing over explicit logical Wires, and spend a small fixed amount of header space to keep the common case simple. Constrained Links may compress that model with deterministic profile invariants, but should not force alias/configuration machinery into the canonical protocol.**

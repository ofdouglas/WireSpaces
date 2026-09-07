# WireSpaces Change Request — Logical-Bus Wires, Canonical Source/Destination, Link Projection, and CAN11 Profiles

**Status:** Proposed for architectural review; intended to become the implementation plan if accepted  
**Revision:** 6 — supersedes revision 5 and earlier Origin/Node drafts  
**Date:** 2026-08-23  
**Scope:** Participant identity, Logical Bus/Wire semantics, canonical source/destination addressing, 48-bit canonical descriptor, forwarding, overlapping Wires, constrained-Link address elision/projection, Classical CAN 11-bit shared/exclusive profiles, commissioning direction, and repository migration  
**Explicitly out of scope:** Mandatory remote-source authorization, cryptographic authentication, controller election/failover protocols, dynamic distributed routing, cyclic Wire forwarding, multicast/group addressing beyond Wire-wide broadcast, full CAN commissioning message/state-machine details, final CAN29 payload packing, and immediate standardization of I2C/RTL/wireless Link profiles

---

# 1. Executive summary

Revision 6 keeps the strongest decisions from revision 5:

```text
Endpoint Domain
    exactly one deployment-scoped ParticipantId

canonical addressed PDU
    WireNumber
    SrcParticipantId
    DestParticipantId / broadcast
    Endpoint
    control metadata

Direction
    not canonical semantics
    may exist inside constrained Link encodings
```

The main architectural change is a sharper definition of **Wire**.

A Wire is a **Logical Bus**: a loop-free logical broadcast/propagation domain realized by one or more Physical Links. A PDU injected onto a Wire is propagated across that Wire's configured Link topology. `DestParticipantId` determines which Participant accepts the PDU; it is not normally used to choose the forwarding path.

This deliberately favors simple embedded forwarding:

```text
WireNumber -> member Link bitmask
```

or, where the local topology needs the more general form:

```text
(WireNumber, IngressLink) -> EgressLinkBitmask
```

The resulting data plane remains bounded, read-mostly, and cheap on MCUs.

Wires may overlap. A broad Wire can be used for commands that must reach several branches while narrower Wires carry branch-local status traffic. A "composite Wire" may be convenient configuration syntax, but after configuration it is just another ordinary Wire.

Revision 6 also generalizes constrained-Link compression:

> The canonical PDU always has an unambiguous `(Wire, SrcParticipant, DestParticipant)` meaning, but a Link representation need not literally transmit fields that can be uniquely reconstructed from Link Binding or frame context.

Two mechanisms are explicitly separate:

1. **Wire elision** — the Link Binding implies the Wire.
2. **Participant projection** — a Link-local participant code maps to a canonical `ParticipantId`.

For Classical CAN 11-bit:

- Wire elision is mandatory.
- One physical CAN11 profile instance carries at most one Wire.
- Participant projection is optional.
- Direct participant encoding is the default.
- Projected encoding is the capacity escape hatch when a deployment has more globally assigned Participants than the direct 5-bit CAN field can represent.
- CAN29 is preferred when the CAN topology or communication graph is too complex for the compact asymmetric profile.

The canonical descriptor remains **48 bits / 6 bytes**.

---

# 2. Decisions requested

Approve the following as the strong/current architecture.

## 2.1 Deployment-scoped `ParticipantId`

Every independently routed/dispatchable Endpoint Domain has one `ParticipantId`.

```text
Physical device / SoC
    may contain 1..N Endpoint Domains

Endpoint Domain
    exactly one ParticipantId
```

Within one WireSpaces deployment identity universe:

- distinct Endpoint Domains have distinct Participant IDs;
- one Endpoint Domain uses the same Participant ID on every Wire it joins;
- physical-device identity remains separate;
- replacement hardware may take over the same deployment role/Participant assignment.

Canonical field width is 8 bits:

```text
0x00..0xFE   ordinary ParticipantId
0xFF         broadcast destination sentinel
```

`0xFF` is invalid as a source.

## 2.2 Retire canonical Origin/Node roles

Retire as canonical architecture:

```text
permanent Origin
formal Node role
per-Wire NodeId
canonical OriginToNode / NodeToOrigin Direction
```

Multiple Participants may independently source traffic on one Wire.

A conventional controller/leaf system remains conventional because its Services and bindings naturally produce that traffic pattern; the routing model does not need to encode a permanent controller role.

## 2.3 Canonical source/destination

The canonical PDU carries:

```text
SrcParticipantId
DestParticipantId
```

Examples:

```text
A -> B:
    src  = A
    dest = B

B -> A:
    src  = B
    dest = A

A -> Wire-wide broadcast:
    src  = A
    dest = 0xFF
```

Request/reply identity, transaction state, freshness, terms, epochs, and similar interaction semantics belong to Services or Transports.

## 2.4 Direction is Link-local only

`Direction` is not part of the canonical WireSpace PDU.

A constrained Link profile may use a Direction bit when it helps reconstruct canonical source and destination from an asymmetric representation.

Such a Direction bit has no canonical request/reply, command/status, client/server, or authority meaning.

## 2.5 Wire is a Logical Bus

Adopt:

> **A Wire is a loop-free Logical Bus: a connected logical propagation domain realized by one or more Links. A PDU transmitted onto a Wire is propagated across that Wire; destination identity controls acceptance rather than ordinary forwarding.**

A Wire may:

- coincide with one Physical Link;
- select only some participants/traffic on a shared Physical Link;
- share Physical Links with other Wires;
- span several heterogeneous Links through transparent forwarding;
- exist entirely inside one device.

A Wire is not synonymous with a cable, CAN bus, Ethernet LAN, process boundary, or physical device.

## 2.6 Wire forwarding is propagation, not destination routing

Ordinary generic forwarding is based primarily on Wire identity and ingress topology.

The baseline model does not require:

```text
DestParticipantId -> next hop
```

Instead, ordinary forwarding uses:

```text
WireNumber -> local member Link set
```

or:

```text
(WireNumber, IngressLink) -> EgressLinkSet
```

`DestParticipantId` is an acceptance address.

A receiving LLL or lower layer may discard traffic early when it can prove that the local Participant is not a destination. Such filtering is an implementation optimization and does not change the Wire's logical propagation semantics.

## 2.7 Ordinary Wires are loop-free

The ordinary transparent-forwarding realization of one Wire shall be loop-free.

The baseline architecture does not require:

- TTL/hop-count forwarding;
- spanning tree;
- duplicate suppression for forwarding loops;
- distributed loop discovery;
- dynamic routing convergence.

If a future Link/profile deliberately supports cyclic/redundant realization, it must define the additional mechanism explicitly rather than weakening the baseline invariant.

## 2.8 Wires may overlap

Several Wires may use the same Physical Links.

Example:

```text
          Main
         /    \
       L1      L2
       |        |
     ECU1      ECU2

W1 = {L1}
W2 = {L2}
W3 = {L1, L2}
```

Typical use:

```text
Main commands:
    transmit on W3

ECU1 status:
    transmit on W1

ECU2 status:
    transmit on W2

ECU1 -> ECU2 when intentionally needed:
    use W3 or a separately bound interaction
```

`W3 = W1 + W2` may be expressed as configuration shorthand, but after configuration W3 is an ordinary Wire with its own WireNumber and Link membership.

## 2.9 Prefer locality; broad Wires are intentionally costly

A broad Wire is legal, but every PDU on it may consume capacity on every Link segment belonging to that Wire.

Therefore:

> **Prefer the smallest Wire that usefully represents the required communication/broadcast scope. Span several Links when a shared Logical Bus is useful, not merely because forwarding makes it possible.**

A deployment may begin with one broad/simple Wire and later introduce narrower or overlapping Wires when measured bandwidth, failure scope, or traffic locality justifies the additional configuration.

The architecture should not encourage machine-wide giant Wires as the default.

## 2.10 Constrained Links may reconstruct canonical address fields

Adopt:

> **On ingress, every Link representation must reconstruct an unambiguous canonical Wire/source/destination identity before generic forwarding or canonical dispatch. On egress, a Link profile may omit, derive, or project fields that are uniquely recoverable from the Link Binding and frame context.**

This is a representation rule, not a second semantic addressing model.

---

# 3. Canonical PDU descriptor

The ordinary canonical descriptor is **48 bits / 6 bytes**.

| Field | Bits |
|---|---:|
| QoS | 2 |
| Reserved | 2 |
| HasHeaderExtensions | 1 |
| TransportType | 3 |
| WireNumber | 8 |
| SrcParticipantId | 8 |
| DestParticipantId | 8 |
| Endpoint | 16 |
| **Total** | **48** |

The intended serialized shape is byte-oriented:

```text
Byte 0      Control
Byte 1      WireNumber
Byte 2      SrcParticipantId
Byte 3      DestParticipantId
Bytes 4-5   Endpoint, little-endian
```

with:

```text
Control
    QoS                    2
    Reserved               2
    HasHeaderExtensions    1
    TransportType          3

Endpoint
    Namespace              2
    Id                    14
```

Canonical multi-byte values use explicit serialization. Native C/C++ struct layout and compiler bitfields are not wire formats.

## 3.1 Control byte

Recommended bit allocation:

```text
bits 7:6    QoS
bits 5:4    Reserved
bit  3      HasHeaderExtensions
bits 2:0    TransportType
```

Reserved bits transmit as zero and are rejected/counted when nonzero until assigned a defined meaning.

## 3.2 Endpoint

Define:

```text
Endpoint = (Namespace << 14) | Id
```

where:

```text
Namespace    0..3
Id           1..16383
Id 0         invalid/reserved
```

Namespace 3 remains the public WireSpaces/FOSS ecosystem namespace.

## 3.3 WireNumber

`WireNumber` is 8 bits.

The exact allocation between network-visible, device-private, and local sentinel uses remains a separate numeric-allocation item.

The semantic distinction remains:

```text
named network Wire
device-private Wire
kLocalDomain
kLocalBus / native-Link bring-up context
```

Do not flatten these merely because the encoded field is one byte.

---

# 4. Logical Bus forwarding model

## 4.1 Local forwarding state

For a small gateway, the simplest useful representation is:

```text
WireNumber -> LinkBitmask
```

On ingress:

```text
egress = WireLinkMask[wire] & ~IngressLinkBit
```

plus any local-delivery decision.

This works naturally when every local interface attached to the same Wire should receive propagation from every other local member interface.

Where a deployment needs a more explicit local topology, use:

```text
(WireNumber, IngressLink) -> EgressLinkBitmask
```

The architecture permits either representation.

The semantic requirement is the configured Wire topology, not a particular table shape.

## 4.2 Static/read-mostly data plane

WireSpaces remains oriented toward static or slowly changing forwarding.

Forwarding state may come from:

- compile-time configuration;
- generated deployment data;
- commissioning;
- Organizer-driven setup;
- explicit bounded runtime configuration.

Dynamic setup may construct static/read-mostly forwarding state.

The baseline does not require a continuously converging distributed routing protocol.

## 4.3 Forwarding ignores ordinary destination identity

A generic transparent forwarder need not interpret Service semantics or maintain per-Participant next-hop routes.

Typical stages are:

```text
Link ingress
    reconstruct canonical Wire/src/dest
        |
        v
Wire forwarding
    Wire -> egress Link set
        |
        +--> local participant acceptance
        |
        +--> egress Links
                re-encode for each Link profile
```

Participant acceptance then uses:

```text
DestParticipantId == local PID
or
DestParticipantId == broadcast
```

Service dispatch uses `Endpoint`.

## 4.4 Link filtering

A Link implementation may filter traffic before canonical delivery when the filtering result is semantically equivalent.

Examples include:

- CAN acceptance filters;
- Ethernet/VLAN/multicast filtering where a profile maps appropriately;
- wireless MAC filtering;
- RTL address comparators;
- software filtering immediately after LLL decode.

Filtering does not change the conceptual Wire membership or propagation domain.

## 4.5 Forwarding preserves source lineage

Ordinary forwarding preserves:

```text
Wire identity
SrcParticipantId
DestParticipantId
Endpoint
QoS
TransportType
applicable canonical header extensions
payload
```

Only Link-local representation changes.

If application code consumes one interaction and authors another, that is **composition**, not forwarding, and the composing Participant becomes the source of the new PDU.

---

# 5. Wire design guidance

## 5.1 Reachability is not Wire membership

Physical reachability through gateways does not imply that all reachable Participants should share one Wire.

A gateway may participate in several Wires without merging them.

Cross-Wire application behavior normally uses composition.

## 5.2 Broad and narrow overlapping Wires

Overlapping Wires are a normal way to represent different propagation scopes over the same topology.

Example:

```text
W_LeftSubsystem
W_RightSubsystem
W_AllActuators
W_Diagnostics
```

These may share Links while carrying traffic with different locality requirements.

## 5.3 Start simple, split for a reason

Recommended evolution:

```text
prototype:
    one broad Wire

measure:
    traffic / latency / link utilization / failure scope

optimize:
    add narrower or overlapping Wires where useful
```

Do not create a combinatorial set of Wires merely because every traffic subset can be represented.

Additional Wire configuration should buy a concrete property such as:

- bandwidth locality;
- meaningful broadcast scope;
- failure containment;
- isolation;
- representability on a constrained Link;
- a real subsystem/purpose boundary.

## 5.4 Broadcast terminology

All PDUs propagate over their Wire.

Canonical destination broadcast:

```text
DestParticipantId = 0xFF
```

means all Participants on that Wire may accept the PDU.

A directed PDU still propagates over the Logical Bus but is accepted only by the addressed Participant, subject to optional early filtering.

There is one base canonical broadcast destination mode: Wire-wide participant broadcast.

---

# 6. Link Binding, elision, and participant projection

## 6.1 Link Binding

A Link Binding defines how one local Link Interface participates in one or more canonical Wires and how canonical identity is represented on that carrier.

A richer backbone Link may transmit the full canonical descriptor directly.

A constrained Link may exploit known context.

## 6.2 Wire elision

Wire elision applies when a Link/profile instance unambiguously carries one Wire.

Example:

```text
CAN11 interface X -> Wire 17
```

Frames on that profile instance do not need to transmit `WireNumber`.

On ingress:

```text
canonical.wire = 17
```

On egress, the Link Binding verifies that the PDU belongs to Wire 17.

Wire elision does not imply Participant projection.

## 6.3 Participant projection

Participant projection maps a Link-local participant code to canonical `ParticipantId`.

Example:

```text
local code 0  <-> PID 12
local code 1  <-> PID 73
local code 2  <-> PID 181
```

Projection:

- is optional unless a future Link profile states otherwise;
- is local to a Link/profile instance;
- is not part of canonical Participant identity;
- may differ on different Links carrying the same Wire;
- must decode to an unambiguous canonical PID before generic forwarding.

The mapping belongs to the Link Binding/profile configuration, not intrinsically to the Wire.

## 6.4 Direct representation remains preferred when sufficient

If a constrained carrier can directly encode the canonical PID values required by the deployment, no projection table is needed.

Projection is justified when it materially expands representable deployments or reduces constrained-link overhead enough to justify its configuration cost.

## 6.5 Future constrained-Link applicability

The same architecture is intentionally compatible with future profiles such as:

- I2C-style controller/leaf Links;
- LIN-like polled leaf buses;
- compact RTL fabrics;
- constrained wireless stars or meshes.

These are not committed immediate profiles in this change request.

The point is architectural: constrained carriers may exploit topology and lower-layer addressing to reconstruct canonical WS identity.

A generic remote-I2C transaction/RPC Service is a separate ecosystem Service concept and is not the same thing as an I2C WireSpaces Link profile.

---

# 7. Internal/device-private communication

Internal communication may use the same Participant/Wire/Endpoint semantics as off-device networking.

A multicore SoC may contain:

```text
Application Domain      PID 12
Safety Domain           PID 13
Networking Domain       PID 14
Diagnostics Domain      PID 15
```

and several device-private Wires.

Internal Wire count follows useful communication/failure structure rather than CPU-core count or physical interconnect hops.

`InternalDebugWire` remains a useful conventional device-private scope for platform publication such as:

- text logs;
- structured events;
- Health;
- OS/runtime telemetry;
- crash/fault records;
- Link Entity status;
- version/build information.

An explicit splice may expose such traffic to an external Wire while preserving the publishing Participant identity.

---

# 8. `kLocalBus` and low-configuration bring-up

`kLocalBus` remains the native-Link/low-configuration bring-up mechanism.

It means the native physical communication context of a Link when a separately named canonical Wire is not established yet.

A simple system should not require an Organizer or deployment manifest merely to communicate.

Observed traffic alone does not create persistent Wire membership or forwarding state.

Anonymous/uncommissioned Link-control traffic may exist below ordinary canonical Wire semantics where a Link profile requires it.

---

# 9. Transmit binding model

The five useful binding modes remain:

| Binding mode | Meaning |
|---|---|
| Static | Destination/Wire context comes from fixed/generated local configuration. |
| Transmit-only | Autonomous publication/command path. |
| Receive-only | No transmit capability. |
| Request-scoped | Validated ingress creates bounded opaque reply context. |
| Learned-from-ingress | Bounded validated peer selections may be retained under profile rules. |

Request-scoped reply conceptually reconstructs:

```text
reply.src  = local ParticipantId
reply.dest = received.src
reply.wire = received Wire
```

plus registration-defined Endpoint/Transport constraints and Service-level correlation state.

Reading received source metadata does not itself grant transmit authority.

Remote authentication/authorization remains separate and optional.

---

# 10. Classical CAN 11-bit

Classical CAN 11-bit is a deliberately constrained compatibility profile.

It does not carry the 48-bit canonical descriptor literally.

Revision 6 adopts two important rules:

1. **Wire elision is mandatory.**
2. **Participant projection is optional.**

A physical CAN11 profile instance maps to at most one canonical Wire.

If a deployment needs multiple independent Wires multiplexed on one CAN physical bus, use CAN29 or another richer profile rather than adding Wire selection back into the compact CAN11 identifier.

## 10.1 Common addressing principle

CAN11 uses asymmetric participant fields plus a Link-local Direction bit.

Conceptually:

```text
A-field
B-field
Direction
```

Direction determines which field is canonical source and destination.

One field is compact; the other is 5 bits wide.

The 5-bit general code reserves value 31 for Link-level broadcast/control encoding, leaving ordinary participant codes `0..30`.

Every ordinary addressed pair must therefore include one participant representable by the compact field.

This remains a deliberate constraint even when participant projection is enabled.

## 10.2 Direct mode — default

By default, participant projection is disabled.

The CAN participant fields directly represent canonical Participant IDs.

Consequences:

- ordinary directly visible canonical PIDs are limited to the low `0..30` range;
- at least one addressed endpoint must fit the compact field;
- deployments with many CAN-visible Participants may exhaust the low canonical PID range across several CAN buses.

Direct mode is preferred for small deployments because it requires no participant projection table.

## 10.3 Projected mode — optional

Projected mode provides a static Link-local mapping:

```text
CAN participant code <-> canonical ParticipantId
```

The CAN LLL:

- translates received participant codes to canonical PIDs;
- reconstructs the elided WireNumber from the Link Binding;
- presents a normal canonical PDU upward;
- maps canonical PIDs back to Link-local participant codes on egress.

With projection enabled:

- canonical Participant IDs remain deployment-global;
- different CAN buses may reuse the same local participant code values;
- a deployment may use the full ordinary canonical ParticipantId space while each CAN11 segment remains locally constrained;
- each individual CAN11 segment still supports at most 31 general participant codes and the asymmetric compact-set communication constraint.

All WS Participants sharing one projected CAN11 segment must use a consistent mapping for that segment.

The exact configuration-compatibility/fingerprint mechanism remains profile/tooling work.

## 10.4 CAN11-WS-Exclusive

When WireSpaces owns the relevant 11-bit identifier space:

```text
QoS                     2
WireIndexA              3
WireIndexB              5
Direction               1
-------------------------
                       11
```

`WireIndexA` is the compact field.

Direct mode:

```text
WireIndexA  -> canonical PID 0..7
WireIndexB  -> canonical PID 0..30
```

Projected mode:

```text
WireIndexA  -> local participant code 0..7 -> canonical PID
WireIndexB  -> local participant code 0..30 -> canonical PID
```

For ordinary addressed traffic, at least one endpoint must be representable by the A/compact field.

## 10.5 CAN11-WS-Shared

When WireSpaces must coexist with non-WS 11-bit CAN traffic, use provisionally:

```text
QoS                     2
ProtocolDiscriminator   1
CompactPID              2
GeneralPID              5
Direction               1
-------------------------
                       11
```

Direct mode:

```text
CompactPID -> canonical PID 0..3
GeneralPID -> canonical PID 0..30
```

Projected mode:

```text
CompactPID -> local participant code 0..3 -> canonical PID
GeneralPID -> local participant code 0..30 -> canonical PID
```

The `ProtocolDiscriminator` is Link-profile classification, not canonical WS PDU metadata.

Exact bit ordering and coexistence allocation remain Link-profile details.

## 10.6 Deterministic representation

If both source and destination fit the compact field, the profile shall define exactly one representation.

The current preferred rule remains:

```text
compact = min(src_code, dest_code)
general = max(src_code, dest_code)

Direction =
    CompactToGeneral if src_code <= dest_code
    GeneralToCompact otherwise
```

If exactly one endpoint fits the compact field, that endpoint occupies the compact field.

Freeze the final rule with conformance vectors.

## 10.7 Broadcast

Reserve general code `31` as the Link-level broadcast destination code.

Ordinary broadcast:

```text
GeneralCode = 31
Direction   = CompactToGeneral
CompactCode = source
```

canonicalizes to:

```text
DestParticipantId = 0xFF
```

Therefore the broadcast source must fit the compact field:

```text
Exclusive:
    compact source code 0..7

Shared:
    compact source code 0..3
```

This is an explicit compact-profile limitation.

Participant projection does not remove this limit; it only allows the compact local codes to map to arbitrary canonical PIDs.

## 10.8 Commissioning/control subspace

Reserve:

```text
GeneralCode = 31
Direction   = GeneralToCompact
```

for CAN11 Link-control / commissioning traffic.

This combination has no ordinary canonical PDU meaning because broadcast is never a canonical source.

Commissioning/control traffic is below ordinary Wire semantics.

## 10.9 Basic commissioning flow

The detailed state machine remains out of scope.

Current architectural direction:

1. Uncommissioned devices do not emit ordinary WS Service traffic.
2. A commissioner discovers devices using stable commissioning/device identity in the reserved Link-control subspace.
3. Collision-safe identical responses plus prefix/binary search remain the preferred discovery direction.
4. The allocator selects a canonical ParticipantId.
5. In direct mode, the canonical PID must satisfy the segment's direct representation constraints.
6. In projected mode, the canonical PID may be any available ordinary deployment PID, and the segment receives a suitable local participant-code mapping.
7. The selected device commits its assignment/configuration and may then use ordinary WS traffic.

Commissioning identity width, opcodes, persistence transaction, retry behavior, and commit protocol remain separate profile work.

## 10.10 CAN11 topology limits remain real

Projection solves the deployment-global low-PID pressure but does not make CAN11 a general peer network.

Per segment:

- the general ordinary participant-code range is `0..30`;
- every ordinary addressed pair must include a compact-code participant;
- shared mode has four compact codes;
- exclusive mode has eight compact codes;
- broadcast sources must be compact-code participants.

If a CAN network needs arbitrary peer-to-peer traffic among many participants, multiple logical Wires on one physical CAN segment, or generally richer routing representation, prefer CAN29.

---

# 11. CAN29 direction

CAN29 is the expected richer CAN profile.

The current architecture favors placing enough routing information in the 29-bit identifier to avoid CAN11-style participant projection for ordinary deployments.

Current direction remains approximately:

```text
QoS                    2
HasHeaderExtensions    1
ProtocolDiscriminator  1
WireNumber             8
SrcParticipantId       8
DestParticipantId      8
Reserved/profile       1
-------------------------
                       29
```

Service/dispatch metadata remains in the CAN data field.

Exact CAN29 payload packing and use of the remaining profile bit remain Link-profile work.

CAN29 is recommended over CAN11 participant projection when the richer identifier solves the deployment cleanly.

---

# 12. Failure, redundancy, and authority

Wire identity is not runtime authority.

Several controllers may legitimately source traffic on one Wire while Service state decides which command sources are currently accepted.

For redundant control:

```text
Participant identities    unchanged
Wire membership           unchanged
forwarding                unchanged
active term/controller    Service/runtime state
```

WireSpaces does not implicitly provide:

- leader election;
- split-brain prevention;
- term allocation;
- physical redundancy;
- duplicate suppression;
- automatic failover.

Likewise, a configured Wire may become physically partitioned when a Link or gateway fails. Diagnostics should therefore distinguish Wire configuration from actual Participant/Link/path reachability.

---

# 13. Structural validity

Base structural validity includes at least:

- reserved Control bits are zero;
- QoS/Transport values are supported as required;
- source PID is valid and not broadcast;
- destination PID is valid or broadcast;
- Endpoint ID is nonzero and representable;
- Namespace is valid;
- header extensions are well formed;
- Wire scope is valid for the local context;
- Link representation reconstructs canonical identity unambiguously;
- device-private/local scope rules are respected;
- no silent truncation or identity reinterpretation occurs.

Deployment validity additionally includes:

- ordinary Wire forwarding topology is loop-free;
- duplicate canonical Participant IDs do not exist in one deployment identity universe;
- Link Bindings agree with Wire membership;
- projected participant maps are valid/consistent for their Link;
- a canonical PDU is only transmitted on a constrained Link when that Link can represent it.

Remote-source authorization is not base structural validity.

---

# 14. Configuration philosophy

Retain:

> **When two designs provide comparable capability and cost, prefer a simple protocol invariant that eliminates configuration over a configurable mapping that expresses the same thing.**

The comparable-capability clause is essential.

Examples:

Prefer direct canonical PID representation on a small CAN11 deployment when it is sufficient.

Use participant projection when direct representation creates a real deployment-wide capacity problem.

Prefer one broad Wire during simple bring-up.

Add narrower/overlapping Wires when the bandwidth/locality benefit justifies them.

Do not introduce runtime-distributed routing merely because static/generated forwarding tables exist.

---

# 15. Library/API consequences

Suggested semantic types:

```cpp
struct ParticipantId {
    uint8_t value;
};

struct WireNumber {
    uint8_t value;
};

struct Endpoint {
    uint16_t value;

    Namespace name_space() const;
    uint16_t id() const;  // 14-bit, nonzero
};

struct PduDescriptor {
    Qos qos;
    bool has_header_extensions;
    TransportType transport;

    WireNumber wire;
    ParticipantId src;
    ParticipantId dest;
    Endpoint endpoint;
};
```

Canonical core/API consequences:

- no canonical `NodeId`;
- no canonical `Direction`;
- no permanent Origin role;
- source metadata is `descriptor.src`;
- Wire identity is directly available after Link ingress reconstruction;
- constrained-Link Direction is profile-local;
- participant projection is implemented below the canonical Router boundary;
- request-scoped reply behavior remains an Endpoint/API context property.

A Link implementation may expose profile-local types such as:

```cpp
enum class Can11Direction : uint8_t {
    kCompactToGeneral,
    kGeneralToCompact,
};
```

Projected CAN11 may use a bounded table such as:

```text
local participant code -> ParticipantId
```

The exact storage representation is an implementation matter.

---

# 16. Required repository changes

## 16.1 `docs/README.md`

Update the summary to introduce:

- Participant identity;
- Logical Bus/Wire semantics;
- source/destination canonical addressing;
- 48-bit descriptor;
- static/read-mostly Wire forwarding;
- constrained-Link elision/projection.

Avoid presenting Wire as a generic routed virtual network.

## 16.2 `docs/introduction.md`

Show:

1. a simple controller/leaf Logical Bus;
2. a multi-initiator Wire;
3. a hierarchical machine with several short Wires and a broader overlapping command Wire.

State explicitly that long Wires are possible but consume bandwidth across all member Links.

## 16.3 `docs/core_architecture.md`

Rewrite Wire around:

```text
Logical Bus
loop-free propagation domain
Wire-wide propagation
destination-based participant acceptance
overlapping Wires allowed
```

Routing/forwarding should no longer say that ordinary next-hop selection is based on destination PID.

Document both local forwarding forms:

```text
Wire -> LinkBitmask
```

and:

```text
(Wire, IngressLink) -> EgressLinkBitmask
```

with the first as the natural minimal representation where sufficient.

Preserve forwarding vs composition.

Add the canonical Link-reconstruction rule.

## 16.4 `docs/architecture_register.md`

Record as strong/current:

```text
one deployment-scoped ParticipantId per Endpoint Domain
Wire = loop-free Logical Bus / propagation domain
multiple initiators
canonical src/dest
48-bit descriptor
destination = acceptance identity, not ordinary forwarding selector
overlapping Wires
static/read-mostly Wire forwarding
Link-local Wire elision permitted
Link-local participant projection permitted
CAN11 requires Wire elision
CAN11 participant projection optional
```

Retire revision-5 claims that CAN11 deliberately has no participant projection and that the deployment-wide low-PID restriction is the only initial model.

## 16.5 `docs/bit_layout.md`

Use the 48-bit descriptor in §3.

Keep Namespace in the 16-bit Endpoint value.

Redesign CAN `PduControl` as already required by the Namespace move.

## 16.6 `docs/link_profiles.md`

Define a general constrained-Link representation principle:

```text
canonical semantics
    Wire + SrcPID + DestPID

Link representation may:
    carry directly
    elide from Link Binding
    derive from carrier context
    project through bounded static map
```

For CAN11:

- one Wire per physical profile instance;
- WireNumber elided;
- direct participant representation default;
- participant projection optional;
- shared/exclusive layouts from §10;
- broadcast/control general code 31;
- pairwise compact-role constraints;
- mapping consistency validation;
- commissioning behavior for direct vs projected mode.

Treat CAN29 as the preferred richer CAN profile.

## 16.7 `docs/deployment.md`

Organizer/configuration capabilities may include:

- assign Participant IDs;
- assign Wire numbers;
- define Wire member Links;
- validate loop freedom;
- generate Wire->Link masks or ingress/egress matrices;
- define overlapping Wires;
- define CAN11 direct/projected mode;
- allocate projected local participant codes;
- validate constrained-Link representability.

Do not require an Organizer for simple systems.

## 16.8 `docs/library_architecture.md`

Keep the canonical Router unaware of participant projection details.

Ingress LLL produces canonical descriptor values before forwarding/dispatch.

Egress LLL receives canonical values and decides whether/how they are representable on its Link.

## 16.9 `docs/conformance.md`

Add semantic tests for:

1. directed PDU propagates across all Link segments of its Wire but is accepted only by destination;
2. broadcast is accepted by all Wire Participants implementing the Endpoint;
3. forwarding uses Wire topology, not destination routing;
4. looped ordinary Wire topology is rejected by configuration validation;
5. overlapping Wires on the same Links remain distinct;
6. broad and narrow Wires forward according to their own masks;
7. source/destination survive heterogeneous forwarding;
8. composition changes source;
9. Wire elision reconstructs the correct Wire;
10. participant projection reconstructs the correct canonical PID;
11. two Links carrying the same Wire may use different projection tables.

CAN11 vectors should cover both direct and projected modes.

## 16.10 `docs/implementation.md`

Measure:

- forwarding-table RAM for dense `Wire -> mask`;
- forwarding-table RAM for `(Wire, ingress) -> mask`;
- projected-map ROM/RAM;
- canonical descriptor wire/storage size;
- cost of early destination filtering;
- CAN11 direct vs projected code size.

## 16.11 `docs/future_work.md`

Record possible future profiles:

- I2C WS leaf profile;
- LIN;
- compact RTL fabric;
- constrained wireless;
- richer CAN29/CAN-FD profiles.

Keep generic remote-I2C RPC as a Service/ecosystem item, not as a routing primitive.

## 16.12 `history.md`

Record that revision 6:

- sharpens Wire into a Logical Bus;
- makes loop freedom normative for ordinary Wire forwarding;
- adopts locality guidance and overlapping Wires;
- changes ordinary forwarding from destination-oriented wording to Wire propagation;
- introduces the general elision/projection representation model;
- resolves CAN11 Wire representation as one Wire per CAN11 profile instance;
- retains direct CAN11 addressing as default;
- adds optional participant projection;
- retains the 48-bit descriptor and global Participant identity.

---

# 17. Proposed replacement invariants

Final invariant IDs are assigned during `REG` editing.

## Participant identity

**PARTICIPANT-A**

> Every Endpoint Domain has exactly one ParticipantId within a WireSpaces deployment identity scope and uses that same identity on every Wire it participates in.

**PARTICIPANT-B**

> Participant identity is separate from physical-device identity.

**PARTICIPANT-C**

> `0xFF` is the canonical broadcast destination and is invalid as a source.

## Wire model

**WIRE-A**

> A Wire is a Logical Bus: a configured loop-free propagation domain realized by one or more Links.

**WIRE-B**

> A PDU injected onto a Wire is logically propagated across that Wire; `DestParticipantId` controls participant acceptance rather than ordinary next-hop forwarding.

**WIRE-C**

> Multiple Participants may independently source traffic on one Wire.

**WIRE-D**

> Several Wires may overlap on the same Physical Links.

**WIRE-E**

> Broad Wires are permitted but should be used deliberately because traffic may consume capacity on every Link segment belonging to the Wire.

## Forwarding

**ROUTE-A**

> Ordinary Wire forwarding is based on Wire identity and configured Link topology.

**ROUTE-B**

> Ordinary transparent Wire realization is loop-free.

**ROUTE-C**

> Ordinary forwarding preserves canonical Wire, source, destination, Endpoint, applicable control metadata, extensions, and payload; only Link-local representation changes.

**ROUTE-D**

> Composition authors a new PDU and therefore uses the composing Participant as source.

**ROUTE-E**

> Observed traffic alone does not create Wire membership or forwarding state.

## Link representation

**LINKREP-A**

> Every ingress Link representation reconstructs an unambiguous canonical Wire/source/destination identity before generic forwarding or dispatch.

**LINKREP-B**

> A Link profile may elide a canonical field when it is uniquely implied by the Link Binding or carrier context.

**LINKREP-C**

> A Link profile may project Link-local participant codes to canonical Participant IDs when the mapping is bounded and unambiguous.

**LINKREP-D**

> Link-local projection never changes canonical Participant identity.

## PDU

**PDU-A**

> The ordinary canonical descriptor is 48 bits / 6 bytes and contains Control, WireNumber, SrcParticipantId, DestParticipantId, and Endpoint.

**PDU-B**

> Direction is not canonical PDU semantics.

**PDU-C**

> Endpoint is a 16-bit value containing Namespace[2] and Id[14]; Id zero is invalid.

## Configuration

**CONFIG-A**

> A static Manifest or Organizer is not required for base communication.

**CONFIG-B**

> Prefer an invariant over configuration when capability and cost are comparable; use mappings/projections when they materially increase capability or constrained-Link efficiency.

---

# 18. Open/gating items

## 18.1 8-bit WireNumber allocation

Choose exact numeric partitions for:

```text
network-visible Wires
device-private Wires
kLocalDomain / local sentinels
```

This does not reopen the 8-bit width unless the resulting partition is demonstrably inadequate.

## 18.2 CAN11 exact bit ordering

Freeze exact arbitration-significant bit positions for shared and exclusive profiles.

Requirements include:

- QoS ordering remains meaningful;
- shared discriminator allocation is auditable;
- commissioning/control does not unexpectedly outrank ordinary traffic;
- deterministic both-compact representation.

## 18.3 CAN11 projection configuration validation

Define how peers/tools detect inconsistent projected participant maps.

The architecture requires consistency; exact fingerprint/version/commissioning mechanics remain open.

## 18.4 CAN11 `PduControl` and N=1 packing

Revalidate:

- Namespace placement;
- Endpoint subset;
- N=1 payload efficiency;
- aggregation/CRC framing;
- optimized tiny-node profile.

## 18.5 CAN11 commissioning protocol

Still open:

- commissioning identity width;
- opcodes;
- selection/session token;
- persistence/commit transaction;
- retry/failure recovery;
- replacement/service behavior.

## 18.6 Header extension semantics

Resolve forwarding/required-extension behavior before the first behavior-critical extension is standardized.

## 18.7 Optional source admission

Remain open/non-blocking.

## 18.8 Degraded/reachability telemetry

Define tooling/telemetry capable of distinguishing:

- configured Wire membership;
- Link health;
- Participant reachability;
- partitioned Wire components;
- Service/runtime authority availability.

No base-header field is required now.

## 18.9 Future constrained-Link profiles

Do not freeze I2C, LIN, RTL, or wireless profiles in this change request.

Use them as architectural measuring sticks for the elision/projection model.

---

# 19. Implementation sequence

## Step 1 — accept semantic model

Approve:

- global Participant identity;
- canonical src/dest;
- Logical Bus/Wire definition;
- Wire-wide propagation semantics;
- loop-free ordinary Wire topology;
- overlapping Wires;
- locality guidance;
- forwarding based on Wire topology;
- 48-bit descriptor.

## Step 2 — accept constrained-Link representation model

Approve:

- canonical identity remains full even when not literally transmitted;
- Wire elision;
- participant projection;
- ingress canonicalization before generic forwarding;
- egress Link-specific re-encoding.

## Step 3 — update `REG`, `CORE`, `INTRO`, `DEPLOY`

Make the semantic model internally consistent before expanding implementation.

## Step 4 — implement host/sim canonical codec and Router slice

Prove:

- encode/decode;
- local dispatch;
- directed acceptance;
- Wire flooding across a loop-free simulated topology;
- overlapping broad/narrow Wires;
- forwarding vs composition;
- simple `Wire -> LinkMask` implementation.

## Step 5 — validate forwarding table representations

Measure both:

```text
Wire -> LinkMask
```

and:

```text
(Wire, ingress) -> egress mask
```

Use the smaller form where it represents the required topology.

## Step 6 — freeze CAN11 details

Freeze:

- exclusive/shared exact bit ordering;
- direct mode;
- projected mode;
- one-Wire-per-CAN11 rule;
- broadcast/control subspace;
- projection-map consistency;
- `PduControl`;
- N=1 capacity.

## Step 7 — implement CAN11 and CAN29

Use one common asymmetric CAN11 participant codec parameterized by:

- compact width;
- shared/exclusive framing;
- direct/projected participant translation.

Use CAN29 as the normal richer escape path.

## Step 8 — revisit future Links only after core evidence

Consider I2C/LIN/RTL/wireless profiles only when a concrete prototype/use case makes them useful.

---

# 20. Review checklist

## Semantic core

- [ ] One ParticipantId per Endpoint Domain?
- [ ] Canonical source/destination?
- [ ] No permanent Origin/Node roles?
- [ ] Wire explicitly defined as Logical Bus / propagation domain?
- [ ] Destination interpreted as acceptance identity rather than ordinary next-hop selector?
- [ ] Ordinary Wire topology loop-free?
- [ ] Overlapping Wires supported?
- [ ] Locality guidance explicit?
- [ ] Forwarding vs composition preserved?

## Descriptor

- [ ] 48 bits / 6 bytes?
- [ ] Control = QoS[2] + Reserved[2] + H[1] + Transport[3]?
- [ ] WireNumber 8 bits?
- [ ] Src/Dest PID 8 bits?
- [ ] Endpoint = Namespace[2] + Id[14]?
- [ ] Endpoint Id zero invalid?
- [ ] `0xFF` broadcast destination?

## Forwarding

- [ ] `Wire -> LinkMask` allowed as minimal implementation?
- [ ] `(Wire, ingress) -> egress mask` available when needed?
- [ ] Generic forwarding independent of Service semantics?
- [ ] Generic forwarding does not require per-PID routes?
- [ ] Early destination filtering allowed as optimization?
- [ ] Loop validation required in configuration/tooling?

## Link representation

- [ ] Ingress reconstructs full canonical identity?
- [ ] Wire elision separate from participant projection?
- [ ] Projection local to a Link Binding, not intrinsic to Wire?
- [ ] Direct representation remains preferred when sufficient?
- [ ] Different segments may project the same canonical PID differently?

## CAN11

- [ ] One Wire per physical CAN11 profile instance?
- [ ] WireNumber always elided?
- [ ] Direct participant mode is default?
- [ ] Projected participant mode is optional?
- [ ] Exclusive = QoS2 + A3 + B5 + Direction1?
- [ ] Shared = QoS2 + discriminator1 + compact2 + general5 + Direction1?
- [ ] General code 31 reserved for broadcast/control?
- [ ] Ordinary broadcast requires compact source?
- [ ] Reserved opposite-direction subspace retained for commissioning?
- [ ] Projected mode removes deployment-global low-PID pressure but not per-segment compact topology limits?
- [ ] CAN29 recommended for complex CAN networks?

## Scope discipline

- [ ] I2C/LIN/RTL/wireless remain future measuring sticks rather than immediate mandatory scope?
- [ ] Remote-I2C RPC remains a separate Service concept?
- [ ] Dynamic routing, cyclic forwarding, multicast groups, and mandatory authorization remain out of scope?

---

# 21. Acceptance effect

If revision 6 is accepted:

1. Every Endpoint Domain has one deployment-scoped 8-bit ParticipantId.
2. `0xFF` is the canonical broadcast destination and is invalid as source.
3. Formal Origin/Node/NodeId roles remain retired.
4. Canonical routing remains explicit source/destination.
5. Direction remains absent from canonical semantics.
6. The canonical descriptor remains 48 bits / 6 bytes.
7. Control remains `QoS[2] + Reserved[2] + HasHeaderExtensions[1] + TransportType[3]`.
8. Endpoint becomes a 16-bit `Namespace[2] + Id[14]` value.
9. Wire is explicitly a **Logical Bus / propagation domain**.
10. Every ordinary Wire realization is loop-free.
11. A PDU on a Wire is propagated across that Wire; destination controls acceptance.
12. Ordinary generic forwarding is Wire/topology based rather than destination-route based.
13. Small MCUs may implement forwarding as `Wire -> LinkBitmask` where sufficient.
14. The more general `(Wire, ingress) -> egress bitmask` form remains available.
15. Static/read-mostly forwarding remains the default data-plane philosophy.
16. Wires may overlap on the same Physical Links.
17. Broad Wires are legal but locality is explicitly preferred for bandwidth/failure-scope reasons.
18. "Composite Wire" is configuration shorthand only; the configured result is an ordinary Wire.
19. Link profiles may elide canonical fields that are uniquely reconstructible.
20. Wire elision and participant projection are separate mechanisms.
21. Participant projection is local to a Link Binding and never changes canonical Participant identity.
22. CAN11 always elides WireNumber and carries at most one Wire per physical profile instance.
23. CAN11 direct participant encoding is the default.
24. CAN11 projected participant encoding is optional.
25. Projected CAN11 allows the deployment to use the full canonical ParticipantId space while each individual CAN11 segment retains its compact local limits.
26. CAN11 exclusive provisionally uses `QoS[2] + WireIndexA[3] + WireIndexB[5] + Direction[1]`.
27. CAN11 shared provisionally uses `QoS[2] + ProtocolDiscriminator[1] + CompactPID[2] + GeneralPID[5] + Direction[1]`.
28. The general CAN11 code `31` remains Link-level broadcast/control space.
29. CAN11 broadcast sources must fit the compact field.
30. CAN11 commissioning remains below ordinary Wire semantics.
31. CAN29 is recommended over CAN11 projection for sufficiently complex CAN networks.
32. The same elision/projection architecture may later support I2C, LIN, RTL, and constrained wireless profiles.
33. Those future profiles are not required for the first implementation.
34. No backward-compatibility Origin/Node shim is required before protocol interoperability is frozen.

The architectural thesis is:

> **WireSpaces uses a general canonical identity model and a deliberately simple embedded forwarding model. A Wire is a loop-free Logical Bus whose traffic propagates across its configured Links; source and destination identify authorship and acceptance, while Wire identity drives ordinary forwarding. Rich Links may carry the canonical descriptor directly. Constrained Links may safely elide or project information that their topology already provides, without changing the canonical meaning seen by the rest of the system. Locality is preferred, overlapping Wires provide a scalable bandwidth optimization, and complexity belongs in configuration/tooling rather than in the MCU data plane.**
::: ​​
# WireSpaces Revision 6 Refinement — Logical-Bus Clarifications and CAN11 Profile Direction

**Status:** Supplement to `WireSpaces Change Request — Revision 6`; proposed refinement for architectural review  
**Date:** 2026-08-23  
**Relationship to Revision 6:** This document does not supersede Revision 6 as a whole. It resolves or sharpens several acceptance-level findings and replaces the Revision-6 CAN11 profile direction where stated. If this document conflicts with Revision 6 on `kLocalBus`, CAN11 Wire representation, CAN11 coexistence, or CAN11 addressing profiles, this refinement takes precedence for the next repository update.

---

# 1. Purpose

Revision 6 establishes the intended high-level architecture:

```text
ParticipantId
    deployment-scoped Endpoint-Domain identity

Wire
    loop-free Logical Bus / propagation domain

canonical PDU
    WireNumber
    SrcParticipantId
    DestParticipantId
    Endpoint
    control metadata

ordinary forwarding
    based primarily on Wire topology

constrained Links
    may elide or project canonical addressing information
```

Independent review identified several points that should be resolved before the repository architecture is rewritten:

1. Anonymous `kLocalBus` is not precise enough under the universal ingress-canonicalization rule.
2. The locality strategy based on overlapping broad/narrow Wires cannot be realized on a CAN11 segment if CAN11 permits only one Wire.
3. The discriminator-based shared CAN11 profile is not a general legacy-coexistence mechanism.
4. The 8-bit Participant and Wire budgets need explicit topology/headroom validation before final freeze.
5. Identity-universe and splice boundaries need explicit replacement invariants.

The review also arrives while the CAN11 design is being simplified.

The preferred CAN11 direction is now:

```text
Physical CAN11 bus
    exactly one Wire
    WireNumber never transmitted

CAN11 addressing concepts
    1. VCN addressing
         - Guest encoding
         - Native encoding

    2. Participant-compressed addressing
         - Native only
         - direct mapping by default
         - optional Participant projection
```

This deliberately avoids accumulating many unrelated CAN addressing modes.

---

# 2. Refinement: `kLocalBus` is a canonical local-only WireNumber

## 2.1 Problem

Revision 6 requires every ingress Link representation to reconstruct an unambiguous canonical:

```text
Wire
SrcParticipantId
DestParticipantId
```

before generic forwarding or canonical dispatch.

At the same time, the previous `kLocalBus` wording allowed a native-Link context in which a named canonical Wire was not established.

That leaves an unnecessary ambiguity: either the ingress PDU is not fully canonical, or `kLocalBus` is effectively a Wire identity without being defined as one.

## 2.2 Decision

Define `kLocalBus` as a **reserved canonical WireNumber with local-only scope**.

Conceptually:

```text
kLocalBus
    valid canonical WireNumber
    locally bound to one native Link Interface
    valid for local canonical dispatch
    never transparently forwarded to another Link
    never spliced as kLocalBus
```

An ingress LLL bound to `kLocalBus` produces a fully canonical descriptor:

```text
Wire = kLocalBus
Src  = reconstructed canonical ParticipantId
Dest = reconstructed canonical ParticipantId / broadcast
```

This preserves the universal canonicalization boundary.

## 2.3 Unambiguous local binding

A Router / Endpoint Domain may bind `kLocalBus` to **at most one Link Interface at a time**.

Reason:

```text
send(..., Wire = kLocalBus)
```

must identify one unambiguous native Link.

A simple device with one CAN LLL may therefore use:

```text
CAN LLL -> kLocalBus
```

with no explicit named-Wire configuration.

A gateway or other multi-Link participant that needs transparent forwarding names the relevant Wires explicitly.

## 2.4 Promotion to a named Wire

`kLocalBus` is intentionally a bring-up/local-use scope.

If traffic must be transparently forwarded across another Link, the deployment binds the physical Link to an ordinary named WireNumber.

There is no implicit rule:

```text
kLocalBus -> some remote Wire
```

and observed traffic does not promote `kLocalBus` into a routed Wire.

## 2.5 Participant identity in LocalBus mode

`kLocalBus` removes the need to configure a named Wire; it does **not** remove the need for a valid source Participant identity.

The local ParticipantId may come from:

- compile-time configuration;
- board/manufacturing configuration;
- commissioning;
- another Link/profile-specific mechanism.

Uncommissioned CAN control traffic remains below ordinary canonical PDU semantics.

---

# 3. Refinement: one physical CAN11 bus equals one Wire

## 3.1 Decision

For all baseline CAN11 profiles:

> **One physical CAN11 bus / CAN11 Link-profile instance maps to exactly one canonical Wire.**

`WireNumber` is never represented in the 11-bit CAN identifier or CAN PDU metadata.

The Link Binding supplies it:

```text
CAN11 Link Binding:
    wire = W
```

Ingress:

```text
CAN frame
    -> decode CAN11 addressing
    -> canonical.wire = W
```

Egress:

```text
canonical PDU
    -> require pdu.wire == W
    -> encode CAN11 addressing
```

A CAN11 LLL with no explicit named-Wire configuration may bind to `kLocalBus` when the `kLocalBus` uniqueness rule is satisfied.

## 3.2 Intentional limitation

A CAN11 physical bus cannot simultaneously carry several independent WS Wires.

In particular, the broad/narrow overlapping-Wire optimization:

```text
          Main
         /    \
       L1      L2
       |        |
     ECU1      ECU2

W1 = L1
W2 = L2
W3 = L1 + L2
```

cannot be implemented with both `W1/W3` on one CAN11 branch and `W2/W3` on the other.

If `L1` and `L2` are CAN11 buses, each physical CAN11 bus has exactly one Wire identity.

## 3.3 Architectural consequence

This is accepted as a **CAN11 profile restriction**, not a restriction on the general Wire model.

Systems requiring multiple overlapping Wires on one physical CAN bus should normally use:

- CAN29;
- another richer Link profile;
- separate physical CAN11 buses;
- composition at the gateway/controller instead of transparent broad-Wire forwarding.

CAN11 is therefore not required to preserve every optimization available in the canonical architecture.

## 3.4 Why prefer this restriction

The alternative would require spending scarce CAN11 bits or CAN payload bytes on Wire selection and would create more profile modes.

The preferred rule is intentionally boring:

```text
CAN11:
    Link chooses Wire
    frame chooses participants
```

This gives CAN11 first-class compactness without distorting the canonical PDU or multiplying CAN11 Wire-addressing variants.

---

# 4. CAN11 addressing model overview

The CAN11 design should expose **two conceptual addressing models**, not a menu of unrelated formats.

```text
1. Virtual Circuit Number (VCN)
    configured mapping names a participant relationship
    used by both Guest and Native VCN encodings

2. Participant-Compressed
    frame names compact/general participant codes directly
    Native only
```

Guest vs Native VCN are two encodings of the **same VCN semantics**.

Direct vs projected participant-compressed operation are two Link-Binding configurations of the **same participant-compressed frame format**.

This distinction is intended to keep the profile set understandable.

---

# 5. VCN semantics

## 5.1 Definition

A `VirtualCircuitNumber` is a Link-local identifier for a configured participant relationship on one CAN11 Wire.

Conceptually:

```text
VCN -> {ParticipantA, ParticipantB}
```

The CAN11 `Direction` bit selects:

```text
A -> B
or
B -> A
```

The Link Binding supplies the Wire.

Therefore the full canonical reconstruction is:

```text
Wire = LinkBinding.wire

VCN lookup:
    A = ParticipantA
    B = ParticipantB

Direction:
    AToB -> Src=A, Dest=B
    BToA -> Src=B, Dest=A
```

## 5.2 VCNs are not Endpoints

A VCN names a participant relationship, not a Service.

All suitable Endpoints exchanged between the same pair may reuse that VCN.

This keeps VCN table size proportional primarily to the communication graph rather than the number of Services.

## 5.3 Broadcast VCN

A VCN may be configured as:

```text
{ParticipantA, kBroadcast}
```

For that entry, only the semantically valid broadcast direction is permitted:

```text
ParticipantA -> kBroadcast
```

The reverse direction is invalid/reserved for ordinary PDU use.

## 5.4 VCN configuration cost

VCN addressing trades identifier bits for configured edge state.

This is appropriate where:

- arbitrary canonical Participant IDs must communicate;
- arbitrary participant pairs are needed;
- the communication graph is modest;
- code generation or static configuration is acceptable.

Participant-compressed addressing remains preferable when its topology constraint fits naturally and avoiding per-pair VCN configuration is valuable.

---

# 6. Guest VCN profile

## 6.1 Purpose

Guest VCN is the preferred CAN11 coexistence mechanism for an existing bus whose owner allocates a small contiguous identifier range to WireSpaces.

This replaces the Revision-6 discriminator-based shared profile.

The profile does **not** claim that one discriminator bit makes arbitrary legacy coexistence safe.

Instead:

> **The bus owner explicitly allocates a CAN-ID block to WireSpaces.**

## 6.2 Baseline Guest VCN encoding

The first profile to define/implement should use four low-order guest bits:

```text
VirtualCircuitNumber    3
Direction               1
-------------------------
guest bits              4
```

The remaining high CAN-ID bits are fixed by the allocated block.

Conceptually:

```text
CAN ID = GuestBase | (VCN << 1) | Direction
```

with a 16-ID aligned allocation.

This provides:

```text
8 VCNs
2 directions per VCN
16 CAN identifiers
```

subject to VCN-entry semantic validity.

## 6.3 QoS

Guest VCN does not carry canonical QoS dynamically.

The Link Binding assigns one fixed canonical QoS interpretation for the Guest allocation.

Rationale:

- the guest use case is primarily experimentation, adoption trials, diagnostics, and non-critical integration;
- CAN identifier space is intentionally scarce;
- the bus owner already controls where the allocated range sits in arbitration priority.

A future design may refine the exact fixed-QoS/arbitration rule, but no per-frame QoS field is required in the baseline Guest profile.

## 6.4 Future scalable Guest allocations

The architecture should **not** make Guest adoption all-or-nothing.

The baseline implementation may define only the 4-bit guest allocation above, but the VCN representation naturally scales by widening only the VCN field.

Possible future allocations:

```text
4 guest bits total:
    VCN 3
    Direction 1
    16 CAN IDs
    8 VCNs

5 guest bits total:
    VCN 4
    Direction 1
    32 CAN IDs
    16 VCNs

6 guest bits total:
    VCN 5
    Direction 1
    64 CAN IDs
    32 VCNs
```

No new addressing semantics are introduced.

The future profile merely allocates a larger contiguous CAN-ID block and uses one or two additional VCN bits.

Do **not** define or implement all of these immediately. Record them as an obvious growth path.

## 6.5 Guest allocation should remain structurally simple

Avoid arbitrary per-bit placement configuration.

Prefer:

```text
one contiguous aligned CAN-ID block
low bits = VCN + Direction
high bits = allocated Guest prefix
```

This keeps the guest codec, bus-owner allocation, filters, and captures understandable.

---

# 7. Native VCN profile

## 7.1 Encoding

When WireSpaces owns the full relevant CAN11 identifier space:

```text
QoS                     2
VirtualCircuitNumber    8
Direction               1
-------------------------
                       11
```

This provides up to 256 Link-local VCN codes.

## 7.2 Canonical reconstruction

The Link Binding provides:

```text
WireNumber
VCN table
```

The frame provides:

```text
QoS
VCN
Direction
```

The LLL reconstructs:

```text
Wire
SrcParticipantId
DestParticipantId
QoS
```

before generic forwarding or dispatch.

## 7.3 Use case

Native VCN is the general configured CAN11 profile.

Prefer it when:

- arbitrary participant pairs are needed;
- canonical Participant IDs do not fit a useful direct compact topology;
- a VCN table is acceptable;
- CAN29 is unavailable or otherwise undesirable.

## 7.4 Arbitration note

Within one QoS class, VCN numeric allocation affects CAN arbitration priority.

This is not a reason to complicate VCN semantics, but deployment tooling should make the ordering visible and avoid accidental priority assignments.

Exact allocation policy remains later profile/tooling work.

---

# 8. Native Participant-Compressed profile

## 8.1 Purpose

Participant-compressed addressing is retained because it efficiently represents common hierarchical and controller/leaf communication graphs without requiring one configured VCN per participant pair.

It names **vertices** compactly rather than naming communication **edges**.

## 8.2 Encoding

Preferred native format:

```text
QoS                     2
CompactParticipantCode  3
GeneralParticipantCode  5
Direction               1
-------------------------
                       11
```

Equivalent provisional field names such as `WireIndexA/WireIndexB` may be used during profile work, but the semantics are compact/general participant codes.

## 8.3 Direct mapping — default

By default:

```text
Compact code 0..7  -> canonical PID 0..7
General code 0..30 -> canonical PID 0..30
```

No participant mapping table is required.

This is the simplest CAN11 deployment mode.

## 8.4 Optional Participant projection

A Link Binding may instead define:

```text
local participant code <-> canonical ParticipantId
```

Example:

```text
compact/general code 0 -> PID 12
code 1                 -> PID 73
code 2                 -> PID 181
...
```

The same frame format is used.

Projection changes representation only; it does not create a second canonical identity namespace.

Different CAN11 buses may reuse the same local codes for different canonical Participants.

## 8.5 Topology constraint

For any ordinary addressed PDU, at least one endpoint must fit the 3-bit compact set.

Therefore participant-compressed CAN11 naturally favors graphs such as:

```text
controller/gateway set
        <->
many leaves
```

and does not efficiently represent arbitrary large peer meshes.

Projection removes the deployment-global low-PID pressure but **does not remove this graph constraint**.

## 8.6 Broadcast

Reserve general code `31` for Link-level broadcast/control semantics.

Ordinary broadcast:

```text
GeneralCode = 31
Direction   = CompactToGeneral
CompactCode = source
```

Therefore the source of a Wire-wide broadcast must belong to the compact set.

With projection enabled, the compact source may map to any canonical PID.

## 8.7 Commissioning/control

Reserve the opposite invalid ordinary direction:

```text
GeneralCode = 31
Direction   = GeneralToCompact
```

for CAN11 Link-control / commissioning.

The detailed commissioning protocol remains separate work.

## 8.8 No Guest participant-compressed profile

Do not define a Guest version of participant-compressed CAN11 at this time.

Guest coexistence already has a clean VCN representation.

Adding a second Guest addressing family would increase configuration/profile complexity for little demonstrated benefit.

---

# 9. CAN11 profile selection

The expected decision path is:

```text
Need CAN11
    |
    +-- Existing/legacy bus?
    |       |
    |       +-- Guest VCN
    |             explicit allocated CAN-ID block
    |             baseline: 8 VCNs / 16 IDs
    |             future larger blocks possible
    |
    +-- WS-native bus
            |
            +-- communication graph fits compact/general topology?
            |       |
            |       +-- Native Participant-Compressed
            |             direct mapping by default
            |             optional projection
            |
            +-- arbitrary participant pairs needed?
                    |
                    +-- Native VCN
                          up to 256 configured circuits
```

Escalate to CAN29 when:

- several Wires must share one physical CAN bus;
- arbitrary richer routing identity is desirable;
- CAN11 VCN/configuration limits become awkward;
- participant-compressed topology constraints are a poor fit;
- CAN payload/metadata efficiency materially improves with the richer profile.

---

# 10. Disposition of acceptance-level review findings

## 10.1 Anonymous LocalBus contradicts universal ingress canonicalization

**Disposition: accept finding; resolve now.**

Revision 6's ambiguous native-Link envelope is replaced by:

```text
kLocalBus = reserved canonical local-only WireNumber
```

Ingress always reconstructs canonical Wire/source/destination.

`kLocalBus` is locally dispatchable but non-forwardable and non-spliceable as itself.

At most one local Link Binding may use it at a time.

A valid local ParticipantId remains required for ordinary canonical traffic.

## 10.2 Locality strategy is unavailable on a CAN11 branch

**Disposition: accept as an intentional CAN11 restriction.**

CAN11 is explicitly:

```text
one physical bus = one Wire
```

Therefore broad/narrow overlapping Wires cannot share a CAN11 branch.

This does not weaken the general Wire architecture.

Use CAN29 or a different architecture when same-bus overlapping Wires are important.

Documentation must state this directly rather than implying that every canonical locality optimization remains available on CAN11.

## 10.3 Shared CAN11 is not a generic legacy coexistence profile

**Disposition: accept; replace the profile.**

Drop the discriminator-based generic "shared" profile as the preferred coexistence design.

Replace it with **Guest VCN**:

```text
bus owner allocates explicit contiguous CAN-ID block
WS interprets only that block
```

This accurately expresses the required coexistence contract.

## 10.4 8-bit identity budgets are assumptions, not conclusions

**Disposition: material validation item; do not reopen immediately.**

Keep the Revision-6 six-byte descriptor as the current preferred design:

```text
WireNumber          8
SrcParticipantId    8
DestParticipantId   8
Endpoint           16
```

Before declaring the widths frozen, run a representative topology corpus that includes:

- multicore/internal Endpoint Domains;
- redundant controllers;
- gateways;
- several CAN buses;
- overlapping broad/narrow Wires;
- device-private Wires;
- debug/platform Wires;
- local/sentinel reservations;
- plausible product growth.

Record:

```text
peak ParticipantId consumption
peak WireNumber consumption
reserved/private allocation cost
remaining growth margin
```

The purpose is to validate useful headroom, not merely prove that one current sketch fits.

If the corpus shows poor margin, revisit allocation before interoperability freeze.

## 10.5 Identity-universe and splice exceptions need replacement invariants

**Disposition: accept; restore explicit invariants.**

Adopt:

> **Plain forwarding never merges independently assigned Participant identity universes.**

A Wire that transparently spans several Links assumes one coordinated canonical ParticipantId universe.

Interconnecting separately engineered WireSpaces requires one of:

- coordinated ParticipantId assignment;
- explicit identity translation;
- composition/application gateway boundary.

Also adopt:

> **A splice is the explicit configured Wire-scope projection boundary. It preserves canonical Participant source/destination identity, Endpoint, applicable control metadata/extensions, and payload while changing the Wire scope as defined by the splice.**

A splice does not silently solve ParticipantId collisions between independent identity universes.

---

# 11. Replacement / additional invariants

Final invariant IDs remain a `REG` editing task.

## LocalBus

**LOCALBUS-A**

> `kLocalBus` is a canonical local-only WireNumber bound to at most one local Link Interface in a Router/Endpoint Domain.

**LOCALBUS-B**

> A PDU on `kLocalBus` may be canonically dispatched locally but shall not be transparently forwarded or spliced as `kLocalBus`.

## CAN11 Wire scope

**CAN11-WIRE-A**

> One physical CAN11 Link-profile instance carries exactly one Wire; WireNumber is reconstructed from the Link Binding and is never represented in the CAN11 frame.

## VCN

**CAN11-VCN-A**

> A CAN11 VCN is Link-local configured identity for a participant relationship; Direction selects which configured endpoint is canonical source and destination.

**CAN11-VCN-B**

> Guest and Native VCN profiles use the same VCN semantics and differ only in identifier-space allocation and QoS representation.

## Participant compression

**CAN11-COMP-A**

> Participant-compressed CAN11 uses one compact and one general participant code; at least one endpoint of every ordinary addressed PDU must be representable by the compact field.

**CAN11-COMP-B**

> Participant projection, when enabled, is a Link-local representation mapping and never changes canonical Participant identity.

## Identity universe

**IDENTITY-SCOPE-A**

> Transparent forwarding does not merge independently assigned ParticipantId universes.

**SPLICE-A**

> A splice is an explicit configured Wire-scope projection that preserves canonical Participant identity and PDU lineage except for the deliberate Wire-scope change.

---

# 12. Required changes to Revision 6 implementation plan

When Revision 6 is applied to the repository, incorporate this supplement as follows.

## `CORE`

- Define `kLocalBus` as canonical local-only WireNumber.
- Require at most one local `kLocalBus` Link Binding.
- Retain universal ingress canonicalization.
- Restore explicit identity-universe and splice invariants.
- State CAN11's single-Wire limitation as profile behavior, not general Wire behavior.

## `LINK`

Replace the Revision-6 CAN11 shared/exclusive set with:

```text
Guest VCN
Native VCN
Native Participant-Compressed
```

Conceptually there are only two addressing models:

```text
VCN
Participant-Compressed
```

Document:

- one Wire per CAN11 bus;
- WireNumber always elided;
- Guest allocated-range precondition;
- Native VCN layout;
- participant-compressed direct/projected behavior;
- general-code broadcast/control convention;
- commissioning direction;
- CAN29 escalation criteria.

## `DEPLOY`

Add configuration objects for:

```text
CAN11 Link -> Wire

VCN -> {ParticipantA, ParticipantB}

participant-compressed optional:
    local code -> ParticipantId
```

Guest deployment additionally records:

```text
allocated CAN-ID base/range
fixed QoS
```

Tooling should validate that the Guest range is actually reserved by the bus owner.

## `CONFORM`

Add vectors for:

### LocalBus
- canonical ingress using `kLocalBus`;
- rejection of transparent `kLocalBus` forwarding;
- ambiguous second `kLocalBus` binding rejected.

### Guest VCN
- VCN A->B;
- VCN B->A;
- broadcast VCN valid direction;
- invalid reverse broadcast direction;
- CAN ID outside allocated block ignored/not classified as WS;
- fixed QoS reconstruction.

### Native VCN
- QoS values;
- VCN boundaries;
- Direction reconstruction;
- arbitrary high canonical PIDs through VCN mapping.

### Participant-Compressed
- direct mapping;
- projected mapping;
- both-compact deterministic encoding;
- unrepresentable general/general pair;
- broadcast source in compact set;
- commissioning/control reserved combination.

## `REG`

Record the 8-bit topology-corpus validation as a pre-freeze material item rather than an unresolved semantic objection.

---

# 13. Deferred items

This refinement intentionally does **not** freeze:

- 5-bit or 6-bit Guest VCN allocations;
- exact Guest fixed-QoS configuration rules;
- Native VCN allocation policy;
- VCN table storage format;
- participant-map fingerprint protocol;
- commissioning opcodes/state machine;
- CAN PDUA/Endpoint packing;
- CAN29 final representation;
- I2C, LIN, RTL, or wireless profiles.

The important near-term decisions are the semantic/profile boundaries, not every packing detail.

---

# 14. Compact resulting CAN11 model

```text
CAN11 physical bus
    exactly one Wire
    WireNumber implicit from Link Binding

If no named Wire is configured and only one eligible local Link exists:
    Wire = kLocalBus

Addressing model 1: VCN
    VCN -> {ParticipantA, ParticipantB}
    Direction -> A->B / B->A

    Guest:
        allocated contiguous CAN-ID block
        baseline:
            VCN 3
            Direction 1
        fixed QoS
        future VCN widening is straightforward

    Native:
        QoS 2
        VCN 8
        Direction 1

Addressing model 2: Participant-Compressed
    Native only:
        QoS 2
        CompactCode 3
        GeneralCode 5
        Direction 1

    direct mapping:
        local code == canonical PID

    optional projection:
        local code -> canonical PID

CAN29:
    preferred when one CAN bus needs several Wires
    or CAN11 addressing/configuration becomes awkward
```

The intended design principle is:

> **Keep the canonical model general, but make CAN11 deliberately small and explicit. One CAN11 bus carries one Wire. VCN mode compresses configured communication edges; participant-compressed mode compresses participant identities under a hierarchical graph constraint. Guest coexistence is an allocated CAN-ID range, not a magical discriminator. More capable CAN systems should move to CAN29 rather than accumulate additional CAN11 addressing modes.**

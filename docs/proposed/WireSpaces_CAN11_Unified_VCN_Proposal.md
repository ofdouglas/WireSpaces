# WireSpaces Proposal — Unified CAN11 VCN Addressing

**Status:** Proposed CAN11 refinement  
**Date:** 2026-08-23  
**Relationship to Revision 6:** This proposal refines the CAN11 direction developed after Revision 6. It favors a single VCN-based CAN11 addressing model for Guest and committed/native use, and recommends deprecating the separate Compact/General participant-compression mode unless later topology experiments show a material advantage that VCN addressing cannot provide cleanly.

---

# 1. Summary

Classical 11-bit CAN is the most constrained important WireSpaces Link.

The preferred direction is to use one addressing concept across CAN11:

> **A Virtual Circuit Number (VCN) is a compact Link-level identifier for a configured canonical Participant relationship.**

The same VCN concept scales from:

```text
small Guest trial
    ->
larger Guest allocation
    ->
committed/native CAN11
    ->
multiple Wires with custom mappings
```

without changing canonical WireSpaces semantics.

The proposed committed/native CAN11 identifier is:

```text
QoS                     2
WireAlias               3
VirtualCircuitNumber    5
Direction               1
-------------------------
                       11
```

The proposed Guest forms use an explicitly allocated CAN-ID block:

```text
Guest-4:
    VCN                 3
    Direction           1

Guest-5:
    VCN                 4
    Direction           1

Guest-6:
    VCN                 5
    Direction           1
```

Guest mode does not represent `WireAlias` or per-frame QoS. Its CAN-ID block determines arbitration placement, and the Link Binding supplies fixed/default QoS and local Wire context.

Committed/native mode adds:

- dynamic 2-bit QoS;
- 3-bit `WireAlias`;
- the full 5-bit VCN field.

The key configuration rule is:

> **In committed CAN11, a WireAlias selects both a canonical WireNumber and one VCN mapping. Active WireAlias bindings are immutable.**

This makes VCN-map migration safer: install a new mapping under a spare WireAlias, migrate users, then retire the old alias.

The default VCN mapping is standardized and deterministic, giving useful hierarchical communication with two optimized central Participants and no arbitrary VCN table.

The proposal recommends **deprecating the separate Compact/General mode** because the default VCN mapping recovers most of its important low-configuration hierarchical behavior while VCNs also support Guest operation, arbitrary configured relationships, multiple Wires, and controlled mapping migration.

CAN29 remains the richer escape path and normally carries full canonical addressing without VCN compression.

---

# 2. Goals

The CAN11 design should:

1. Preserve the general canonical WireSpaces model.
2. Be understandable enough to configure and debug without specialized routing theory.
3. Provide a useful zero-/low-configuration default for controller/leaf systems.
4. Support at least two commonly central Participants in the default mapping.
5. Support experimentation on an existing CAN bus with a small explicitly allocated ID block.
6. Allow Guest adoption to grow incrementally by adding one or two VCN bits.
7. Allow committed CAN11 to carry several Wires.
8. Permit arbitrary modest Participant relationships when custom configuration is justified.
9. Avoid per-LLL independently authored VCN maps.
10. Provide a safe migration mechanism for VCN-map changes.
11. Avoid maintaining several unrelated CAN11 addressing formats.
12. Escalate naturally to CAN29 when CAN11 becomes configuration-heavy or capacity-constrained.

---

# 3. Non-goals

This proposal does not attempt to make CAN11 equivalent to CAN29.

CAN11 is not required to provide:

- full 8-bit WireNumber directly;
- full 8-bit SrcParticipantId directly;
- full 8-bit DestParticipantId directly;
- arbitrary large communication graphs without configuration;
- unlimited Wires on one bus;
- dynamic routing;
- runtime VCN-map mutation;
- transparent interpretation across independently configured WireSpaces.

A deployment that needs substantially richer identity should use CAN29 or another richer Link profile.

---

# 4. Canonical semantics remain unchanged

Above the Link layer, the ordinary canonical identity remains:

```text
WireNumber
SrcParticipantId
DestParticipantId
Endpoint
QoS
TransportType
...
```

CAN11 compression does not introduce a second canonical addressing model.

The CAN11 LLL reconstructs canonical:

```text
{WireNumber, SrcParticipantId, DestParticipantId}
```

before generic forwarding or dispatch.

Likewise, on transmit, the CAN11 LLL accepts canonical identity and chooses the required compact representation.

`Direction` remains **Link-local compression metadata only**.

It does not mean:

- request/reply;
- command/status;
- client/server;
- authority;
- canonical Origin/Node role.

---

# 5. VCN semantics

## 5.1 A VCN identifies a Participant relationship

A VCN resolves to an unordered configured pair:

```text
VCN -> {ParticipantA, ParticipantB}
```

`Direction` determines canonical source and destination:

```text
Direction = AToB:
    src  = ParticipantA
    dest = ParticipantB

Direction = BToA:
    src  = ParticipantB
    dest = ParticipantA
```

The VCN is not an Endpoint.

All suitable Endpoints exchanged between the same Participant pair may reuse the same VCN.

## 5.2 Broadcast relationships

A VCN may contain:

```text
{ParticipantA, kBroadcast}
```

Only the meaningful direction is valid for ordinary traffic:

```text
ParticipantA -> kBroadcast
```

The reverse direction is structurally impossible as an ordinary canonical PDU because broadcast is never a valid source.

Such otherwise-invalid encoding space may be reserved for Link-control / commissioning purposes.

## 5.3 VCN is compression, not route identity above the LLL

Software above the LLL should not depend on VCN values.

Captures and tooling may display them because they are useful Link-level diagnostics, but canonical Services use Participant IDs.

---

# 6. Standard default VCN mapping

A major purpose of VCN addressing is to preserve a useful no-/low-configuration path.

The standard 5-bit default mapping is:

```text
VCN 0       MainA -> Broadcast
VCN 1       MainB -> Broadcast
VCN 2       MainA <-> MainB
VCN 3       Reserved / Link Control

VCN 4       Node0 <-> MainA
VCN 5       Node0 <-> MainB

VCN 6       Node1 <-> MainA
VCN 7       Node1 <-> MainB

VCN 8       Node2 <-> MainA
VCN 9       Node2 <-> MainB

...

VCN 30      Node13 <-> MainA
VCN 31      Node13 <-> MainB
```

Formulaically, for `VCN >= 4`:

```text
NodeIndex = (VCN - 4) / 2

even VCN:
    Node[NodeIndex] <-> MainA

odd VCN:
    Node[NodeIndex] <-> MainB
```

This supports:

```text
2 Main Participants
14 Node Participants
MainA broadcast
MainB broadcast
MainA <-> MainB
every Node <-> MainA
every Node <-> MainB
```

without an arbitrary per-edge VCN table.

---

# 7. MainA and MainB are CAN-profile positions, not architecture roles

`MainA` and `MainB` are names for optimized positions in the **default CAN11 VCN mapping**.

They are not canonical WireSpaces roles.

They do not reintroduce:

- permanent Origin;
- authority;
- leader;
- controller ownership;
- special canonical Participant types.

A deployment may assign these positions to:

- two embedded controllers;
- controller + Linux computer;
- robot computer + development PC;
- primary + redundant controller;
- gateway + service tool;
- any other Participants whose communication graph benefits from the default mapping.

A three-Main default is not proposed.

Three optimized Mains would consume substantially more of the 32-entry VCN space for Main/Main and Main/Node relationships, reducing useful leaf capacity. Systems requiring three or more highly connected central Participants may use a custom VCN map or CAN29.

---

# 8. Default Participant role binding

The default VCN **numbering** is protocol-defined.

The mapping from role positions to canonical Participant IDs belongs to deployment configuration.

Conceptually:

```text
MainA   -> PID
MainB   -> PID
Node0   -> PID
Node1   -> PID
...
```

A simple deployment may use a deterministic default such as low canonical PIDs.

A generated deployment may bind arbitrary canonical PIDs to the positions.

The important distinction is:

```text
default VCN topology:
    standardized

canonical PID assignment:
    deployment-specific
```

This preserves global canonical Participant identity while avoiding an arbitrary VCN edge table for common topologies.

---

# 9. Guest CAN11

## 9.1 Purpose

Guest mode allows WireSpaces to coexist on an existing CAN11 bus without taking over the entire 11-bit identifier space.

The bus owner explicitly allocates one contiguous CAN-ID block to WireSpaces.

This is a concrete coexistence contract rather than a generic discriminator-bit assumption.

## 9.2 Guest VCNs are WireSpace-global

Guest frames do not contain `WireAlias`.

Therefore there is no in-frame selector for multiple VCN mapping domains.

Adopt:

> **Guest VCN meanings are global within the WireSpace deployment.**

A Guest VCN value has one authoritative meaning wherever Guest CAN uses it in that WireSpace.

This intentionally limits Guest flexibility in exchange for simple configuration and reduced mismatch risk.

If Guest-global VCN allocation becomes constraining, that is a natural indication to move the bus to committed/native CAN11.

## 9.3 Guest-4 baseline

Minimum Guest allocation:

```text
VCN                     3
Direction               1
-------------------------
guest bits              4
```

Requires a 16-ID aligned allocated block.

Available default VCNs:

```text
0       MainA broadcast
1       MainB broadcast
2       MainA <-> MainB
3       reserved/control
4,5     Node0 <-> MainA/MainB
6,7     Node1 <-> MainA/MainB
```

Thus the smallest Guest profile can already exercise:

- two Mains;
- two Nodes;
- either Main broadcasting;
- MainA/MainB communication;
- either Main communicating bidirectionally with either Node.

This is enough to meaningfully trial hierarchical multi-initiator WireSpaces behavior.

## 9.4 Guest-5 growth

Future optional form:

```text
VCN                     4
Direction               1
-------------------------
guest bits              5
```

Consumes 32 CAN IDs.

Provides the same default mapping through VCN 15:

```text
2 Mains
6 Nodes
```

## 9.5 Guest-6 growth

Future optional form:

```text
VCN                     5
Direction               1
-------------------------
guest bits              6
```

Consumes 64 CAN IDs.

Provides the complete default mapping:

```text
2 Mains
14 Nodes
```

## 9.6 Monotonic Guest growth

The same VCN value always has the same default interpretation.

Growing:

```text
Guest-4
    ->
Guest-5
    ->
Guest-6
```

only exposes additional VCN values.

It does not reinterpret existing VCNs.

This is a deliberate compatibility property.

The actual CAN-ID allocation may need to move or expand, but the WireSpaces VCN semantics remain stable.

## 9.7 Guest QoS

Guest mode does not carry per-frame WireSpaces QoS.

The Guest Link configuration provides fixed QoS / arbitration interpretation.

The allocated CAN-ID range already determines the physical arbitration position relative to existing bus traffic.

Guest mode is primarily intended for:

- evaluation;
- experimentation;
- diagnostics;
- incremental adoption;
- modest non-critical integration.

Do not spend scarce Guest identifier bits on dynamic QoS unless a concrete need emerges.

---

# 10. Committed/native CAN11

## 10.1 Identifier layout

Preferred committed/native layout:

```text
QoS                     2
WireAlias               3
VirtualCircuitNumber    5
Direction               1
-------------------------
                       11
```

This provides:

```text
4 QoS classes
8 WireAlias values
32 VCN values per active alias binding
2 directions
```

## 10.2 WireAlias selects representation context

A committed CAN11 `WireAlias` does more than compress a canonical WireNumber.

Each active alias selects a complete Link representation binding:

```text
WireAliasBinding {
    canonical WireNumber
    VCN mapping
}
```

Therefore ingress conceptually performs:

```text
WireAlias
    -> {WireNumber, VcnMap}

VCN + Direction
    -> {SrcParticipantId, DestParticipantId}

=> canonical PDU identity
```

## 10.3 VCN is scoped by WireAlias, not by canonical WireNumber

VCN mapping is specifically associated with the **WireAlias binding**.

Do not define a general:

```text
(WireNumber, VCN)
```

canonical or global lookup rule.

Reason: two WireAliases may temporarily refer to the same canonical Wire while using different VCN mappings.

This is useful for safe representation migration.

## 10.4 Multiple aliases may map to the same Wire

Adopt:

> **Every active WireAlias has exactly one canonical Wire binding, but multiple active WireAliases may bind the same canonical Wire.**

Example:

```text
Alias 2:
    Wire = W17
    VCN map = generation A

Alias 5:
    Wire = W17
    VCN map = generation B
```

Both decode to the same canonical Wire identity.

The alias difference exists only at the CAN11 representation layer.

---

# 11. VCN configuration authority

## 11.1 One WireSpace source of truth

VCN definitions should not be independently authored inside each CAN LLL.

There is one authoritative WireSpace deployment configuration containing:

```text
Participants
Wires
CAN11 WireAlias bindings
VCN mappings
Guest VCN definitions
```

Per-device / per-LLL tables are generated slices or compiled representations of that authoritative configuration.

They are not independent architectural configuration domains.

## 11.2 Default mapping requires little configuration

For an alias using the standard default mapping, configuration need only identify role positions:

```text
WireAlias 1:
    Wire = W17
    VCN mapping = Default
    MainA = PID 4
    MainB = PID 9
    Node0 = PID 31
    Node1 = PID 32
    ...
```

No 32-entry edge table is required.

## 11.3 Custom mapping is exceptional

A committed alias may use an explicit VCN map where the default topology is a poor fit.

Example:

```text
Alias 3:
    Wire = W28
    VCN mapping = Explicit

    0  -> {PID 12, Broadcast}
    1  -> {PID 73, PID 91}
    2  -> {PID 44, PID 18}
    ...
```

Custom mapping should normally be generated by tooling rather than manually duplicated across ECUs.

---

# 12. Active WireAlias bindings are immutable

The largest VCN risk is silent semantic mismatch:

```text
sender:
    VCN 7 -> {A, B}

receiver:
    VCN 7 -> {C, D}
```

A valid CAN frame may then be interpreted as a valid canonical PDU for the wrong Participants.

To reduce this risk, adopt:

> **The meaning of an active WireAlias shall not be changed in place.**

An active binding includes:

```text
canonical WireNumber
VCN mapping/profile
associated representation parameters
```

Changing any of these creates a new representation binding.

---

# 13. VCN-map migration using spare WireAlias

Committed/native CAN11 provides a natural migration mechanism.

To replace VCN map A with map B:

```text
1. allocate an unused WireAlias
2. bind it to the same canonical Wire
3. install the new VCN map under that alias
4. deploy/enable participants using the new alias
5. allow old and new aliases to coexist during transition
6. remove the old alias after migration completes
```

Example:

```text
Alias 2:
    W17
    old map

Alias 5:
    W17
    new map
```

Both decode to canonical Wire `W17`.

This avoids redefining a live VCN namespace underneath existing devices.

It also reduces the need for sophisticated runtime table-generation/version switching.

A configuration fingerprint or generation marker may still be useful for diagnostics and fail-closed validation, but immutable alias bindings provide the architectural update primitive.

---

# 14. Reserved/default WireAlias behavior

For zero-/low-configuration committed CAN use, the initial Link configuration may define:

```text
WireAlias 0:
    Wire = kLocalBus
    VCN map = Default
```

Other aliases are invalid/unconfigured.

Once a named deployment configuration is installed, alias 0 need not be permanently reserved for `kLocalBus`.

For example:

```text
Alias 0 -> W17
Alias 1 -> W22
...
```

may be valid.

The principle is:

> **The default is convenient configuration, not a permanent semantic restriction.**

`kLocalBus` itself remains a canonical local-only WireNumber according to the broader architecture.

---

# 15. Link-control / commissioning space

The default mapping reserves VCN 3 for Link-control / future use.

Additionally, broadcast VCN entries contain one impossible ordinary direction because broadcast cannot be a source.

These encodings provide candidate Link-control space.

Exact commissioning/control allocation should remain profile work until the complete CAN PDU/commissioning design is reviewed.

The desired property is:

- Link control must remain interpretable independently of arbitrary custom VCN entries where practical;
- two nodes with mismatched ordinary VCN configuration should still have a way to diagnose configuration incompatibility;
- ordinary canonical traffic must never interpret broadcast as a source.

Do not spend more CAN-ID space than needed merely to reserve commissioning functionality.

---

# 16. Why VCN subsumes the useful Compact/General case

The previous native participant-compressed proposal used:

```text
QoS                     2
CompactParticipantCode  3
GeneralParticipantCode  5
Direction               1
-------------------------
                       11
```

Its strongest property was low-configuration representation of hierarchical graphs:

```text
small set of controllers/gateways
    <->
larger set of leaves
```

The standard VCN mapping now directly optimizes a similar important topology:

```text
MainA/MainB
    <->
up to 14 Nodes
```

while also providing:

- MainA broadcast;
- MainB broadcast;
- MainA <-> MainB;
- Guest operation with the same addressing vocabulary;
- arbitrary configured relationships;
- several Wires in committed mode;
- controlled VCN-map migration through WireAlias;
- one standardized CAN11 addressing concept.

Therefore the primary rationale for carrying a second Compact/General encoding is substantially reduced.

---

# 17. Remaining advantage of Compact/General

Compact/General still has one meaningful theoretical advantage:

> It represents a potentially dense bipartite communication graph by naming participant vertices rather than allocating one VCN per relationship.

For example, several compact Participants may communicate with many general Participants without one configured edge entry per pair.

It is also more self-describing in raw CAN captures because participant codes appear directly.

These are real benefits.

However, they become most valuable for communication graphs with significantly more than 32 useful pair relationships per Wire.

Such graphs should be evaluated against the intended CAN11 profile boundary.

If realistic systems routinely need many dozens or hundreds of pair relationships on one CAN11 Wire while still strongly benefiting from Classical CAN, Compact/General may deserve retention.

If not, CAN29 is likely the cleaner answer.

---

# 18. Proposal: deprecate Compact/General from the baseline

Recommended direction:

> **Do not carry Compact/General as a baseline CAN11 addressing mode unless topology experiments demonstrate a clear material benefit over the unified VCN model.**

For now:

```text
preferred CAN11 addressing:
    VCN

Compact/General:
    deprecated candidate
    retained only as design history / fallback idea
```

Before deleting it permanently, run representative CAN11 topology tests and record:

```text
number of required relationships per Wire
number of Wires per physical CAN bus
fraction handled by standard default VCN mapping
fraction requiring custom VCN mapping
largest custom VCN count
cases exceeding 32 relationships
configuration size / complexity
```

A second frame format should survive only if those results show that it materially expands useful Classical-CAN systems.

---

# 19. CAN29 boundary

CAN29 does not need VCN compression for ordinary addressing.

The richer identifier can afford the important canonical routing fields directly.

Preferred architectural split:

```text
CAN11:
    compressed Link representation
    WireAlias + VCN + Direction
    Guest VCN subset

CAN29:
    canonical Wire/source/destination represented directly
    no VCN required for ordinary addressing
```

Therefore CAN11 does not need to solve every large graph elegantly.

When VCN allocation, WireAlias count, or CAN11 configuration becomes awkward, migration to CAN29 is an intended solution rather than a profile failure.

---

# 20. Scale-up story

The resulting path is:

```text
Small trial
    Guest-4
    global default VCNs
    2 Mains + 2 Nodes
    fixed QoS
    16 allocated CAN IDs

Larger trial
    Guest-5
    same VCN meanings
    2 Mains + 6 Nodes
    32 CAN IDs

Substantial Guest use
    Guest-6
    same VCN meanings
    full 5-bit default mapping
    2 Mains + 14 Nodes
    64 CAN IDs

Committed CAN11, simple
    QoS + WireAlias + VCN + Direction
    default VCN mapping
    WireAlias 0 may default to kLocalBus

Committed CAN11, structured
    several WireAliases
    several Wires
    per-alias default VCN mappings

Committed CAN11, custom
    custom VCN mapping on selected aliases
    arbitrary modest Participant relationships
    alias-based map migration

Richer CAN system
    CAN29
    full canonical routing identity
    no ordinary VCN compression
```

Configuration complexity therefore appears only when the deployment asks for additional capability.

---

# 21. Proposed invariants

Final invariant IDs are assigned during architecture-register editing.

## VCN

**CAN11-VCN-A**

> A VCN is Link-level compressed identity for a configured Participant relationship; Direction selects canonical source and destination.

**CAN11-VCN-B**

> VCN identity is not visible above the LLL and does not replace canonical Participant identity.

## Default mapping

**CAN11-VCN-DEFAULT-A**

> The standard default VCN mapping has stable meanings independent of Guest VCN field width.

**CAN11-VCN-DEFAULT-B**

> `MainA`, `MainB`, and `NodeN` are CAN-profile mapping positions, not canonical WireSpaces authority or topology roles.

## Guest

**CAN11-GUEST-A**

> Guest VCN meanings are WireSpace-global because Guest frames contain no WireAlias selector.

**CAN11-GUEST-B**

> Increasing Guest VCN width exposes additional default VCN values without changing the meaning of existing VCN values.

## Committed/native

**CAN11-ALIAS-A**

> In committed CAN11, each active WireAlias selects exactly one canonical WireNumber and one VCN mapping.

**CAN11-ALIAS-B**

> Multiple WireAliases may bind the same canonical WireNumber.

**CAN11-ALIAS-C**

> The interpretation of an active WireAlias is immutable; a changed VCN mapping is deployed under a different alias.

## Configuration authority

**CAN11-CONFIG-A**

> VCN mappings have one authoritative WireSpace deployment definition; per-device LLL tables are generated/compiled representations of that definition.

## CAN29

**CAN29-ADDR-A**

> Ordinary CAN29 addressing should carry canonical Participant identity directly rather than requiring CAN11 VCN compression.

---

# 22. Open items

This proposal intentionally leaves open:

1. Exact Guest-4/5/6 standardization schedule.
2. Exact CAN-ID alignment/base rules for each Guest width.
3. Whether VCN 3 is permanently Link-control or merely reserved initially.
4. Exact Link-control/configuration-fingerprint protocol.
5. Exact default PID binding convention for MainA/MainB/NodeN in zero-config deployments.
6. Whether custom VCN maps may partially inherit the standard default map or must define their entire usable range.
7. Exact WireAlias configuration/fingerprint format.
8. Exact arbitration-aware VCN allocation policy for custom maps.
9. Final CAN PDUA/Endpoint packing.
10. Whether topology testing finds any remaining justification for Compact/General.

None of these require keeping two ordinary CAN11 addressing formats in the baseline today.

---

# 23. Recommended next test

Before freezing this proposal, apply the unified VCN scheme to representative CAN11 topologies.

For each physical CAN bus, measure:

```text
default mapping fits completely?
if not:
    number of custom VCN entries required

number of WireAliases required
number of canonical Wires represented
need for simultaneous old/new alias during migration
Guest-4 / Guest-5 / Guest-6 usability
whether any realistic graph requires >32 relationships per Wire
```

The key decision criterion for Compact/General is:

> **Does Compact/General materially improve realistic Classical-CAN deployments that remain good CAN11 candidates after considering the 32-VCN and 8-WireAlias limits?**

If the answer is no, remove it.

---

# 24. Recommended direction

Adopt one conceptual CAN11 addressing model:

```text
VCN + Direction
```

with progressively richer carrier encodings.

Guest:

```text
allocated CAN prefix
VCN[3..5]
Direction
fixed QoS
global Guest VCN meaning
```

Committed/native:

```text
QoS[2]
WireAlias[3]
VCN[5]
Direction[1]
```

where:

```text
WireAlias -> {canonical WireNumber, VCN mapping}
```

Use the standardized two-Main default mapping wherever it fits.

Use custom VCN mappings only where needed.

Treat active alias bindings as immutable and migrate VCN maps using spare aliases.

Use CAN29 when direct canonical addresses are more appropriate.

Deprecate Compact/General unless concrete topology evidence demonstrates that maintaining a second native CAN11 frame format earns its complexity.

The design principle is:

> **CAN11 should have one compact addressing vocabulary that scales from trial use to committed deployment. VCNs provide that vocabulary: deterministic defaults for common hierarchy, explicit mappings for unusual relationships, alias-scoped configuration for several Wires and safe migration, and a clean escalation path to CAN29 when compression stops being worthwhile.**

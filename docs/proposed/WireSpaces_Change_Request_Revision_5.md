# WireSpaces Change Request — Global Participant Identity, Multi-Initiator Wires, 48-bit Base Descriptor, and CAN11 Profiles

**Status:** Proposed for architectural review; intended to become the implementation plan if accepted  
**Revision:** 5 — supersedes revision 4, revision 3, and the earlier per-interaction-Origin drafts  
**Date:** 2026-08-23  
**Scope:** Core participant identity, Wire initiation semantics, canonical source/destination addressing, the revised 48-bit canonical base descriptor, Endpoint address allocation, internal/device-private Wires, binding-mode migration, zero-configuration bring-up semantics, Classical CAN 11-bit shared/exclusive compact profiles, basic CAN commissioning architecture, and repository migration  
**Explicitly out of scope:** Mandatory remote-source authorization, mandatory permitted-source tables, mandatory static deployment manifests, controller election/failover protocols, cryptographic authentication, multicast/group addressing, the full CAN commissioning state machine/message set, and final resolution of multi-Wire multiplexing in the compact CAN11 profiles

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
    multiple Participants may independently source traffic

canonical addressed PDU
    WireNumber
    SrcParticipantId
    DestParticipantId / broadcast
    EndpointAddress
    control metadata
```

The topology experiments support the architectural conclusion that a Wire should not permanently assign one Participant as Origin. They do **not** establish that a canonical PDU needs to retain a separate Origin/Participant interaction anchor.

Canonical routing therefore becomes ordinary source/destination addressing. `Direction` is removed from the canonical PDU model and retained only where a constrained Link representation needs it to reconstruct source and destination from asymmetrically compressed fields.

The canonical base descriptor expands from **40 bits / 5 bytes** to **48 bits / 6 bytes**:

| Field | Bits |
|---|---:|
| QoS | 2 |
| Reserved | 2 |
| HasHeaderExtensions | 1 |
| TransportType | 3 |
| WireNumber | 8 |
| SrcParticipantId | 8 |
| DestParticipantId | 8 |
| Namespace | 2 |
| EndpointId | 14 |
| **Total** | **48** |

The intended serialized form is deliberately byte-friendly:

```text
Byte 0      Control
Byte 1      WireNumber
Byte 2      SrcParticipantId
Byte 3      DestParticipantId
Bytes 4-5   EndpointAddress, little-endian
```

where:

```text
Control:
    QoS                    2
    Reserved               2
    HasHeaderExtensions    1
    TransportType          3

EndpointAddress: uint16_t
    Namespace              2   // bits 15:14
    EndpointId            14   // bits 13:0
```

This allocation intentionally gives **more durable address space to Endpoint identities than to deployment-local routing identities**:

- Wire numbers are deployment-scoped and may be reassigned;
- Participant IDs are deployment-scoped and may be reassigned;
- Namespace-3 Endpoint IDs are intended to form a long-lived public/FOSS ecosystem registry and are difficult to reclaim after publication.

The proposal therefore prefers approximately 256 deployment-local Wires, 255 ordinary Participants, and 16,383 valid Endpoint IDs per Namespace over the aesthetically symmetric but less useful `10/10/10/10` allocation considered in revision 3.

A project-level design rule is also adopted:

> **All else being equal, prefer a simple protocol invariant that eliminates configuration over a configurable mapping that expresses the same thing.**

The phrase **all else being equal** is load-bearing. A scope projection or mapping is justified when it materially increases representable capability; configuration is not forbidden merely because a more restrictive invariant exists.

For Classical CAN 11-bit, the current preferred direction intentionally accepts a compact-profile limitation instead of adding a Participant alias table: only a low range of deployment-global Participant IDs is directly representable. Two CAN11 profiles share one asymmetric addressing model and the same two-bit QoS semantics:

```text
CAN11-WS-Exclusive
    QoS                    2
    CompactParticipantId   3
    GeneralParticipantId   5
    Direction              1

CAN11-WS-Shared
    QoS                    2
    ProtocolDiscriminator  1
    CompactParticipantId   2
    GeneralParticipantId   5
    Direction              1
```

Both reconstruct the same canonical `{src,dest}` pair. Shared operation pays one compact-PID bit to coexist with non-WS traffic; exclusive operation uses that bit for a larger compact-source/destination population. In both profiles, the otherwise-invalid `GeneralParticipantId=broadcast` plus `Direction=GeneralToCompact` subspace is reserved for CAN Link-control/commissioning traffic below ordinary Wire semantics.

These are deliberately constrained compatibility profiles, not the canonical capacity floor for WireSpaces. CAN29 and richer Links are the intended escape paths when the compact CAN11 limits are unsuitable.

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

The base canonical field is 8 bits. Proposed canonical values:

```text
0x00..0xFE   valid ParticipantId
0xFF         broadcast destination sentinel
```

`0xFF` is not a valid source Participant.

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

Adopt:

> **One independently routed Endpoint Domain equals one Participant.**

This applies even when several Endpoint Domains reside inside one multicore SoC, MCU, FPGA, or host.

Example:

```text
Central SoC
    Application Domain      PID 12
    Safety Domain           PID 13
    Networking Domain       PID 14
    Diagnostics Domain      PID 15
```

This gives IPC and off-device networking the same identity model and means logs, faults, health, and platform telemetry identify the actual Domain that authored them.

A single Endpoint Domain cannot expose several independent canonical source identities without being split into several Domains. Earlier sketches identified this as a possible expressive cost. Record it in `REG` as a conscious trade and reopen condition rather than silently erasing it.

## 2.3 Retire canonical `Node`, `NodeId`, and permanent `Origin`

Retire as canonical architecture:

```text
Origin as a permanent per-Wire role
Node as a formal per-Wire role
NodeId as canonical identity
OriginToNode / NodeToOrigin
OriginToParticipant / ParticipantToOrigin
```

Do not carry a compatibility `NodeId` API into the first implementation.

The ordinary English words *origin*, *originating*, *node*, or *direction* may still appear where they are not formal protocol roles.

## 2.4 Multi-initiator Wires

A Wire no longer has exactly one permanent Origin.

Multiple Participants may independently source traffic on one Wire.

The base protocol does **not** require:

- a `PermittedOrigins` or `PermittedSources` table;
- an Endpoint/source ACL;
- a global static Wire-membership manifest;
- an Organizer;
- an Organizer-generated authorization matrix.

A conventional master/controller system remains simple because only the controller happens to initiate the relevant traffic.

A peer or distributed system no longer needs an artificial permanent Origin merely to fit the Wire abstraction.

## 2.5 Canonical routing uses source and destination

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

A -> broadcast:
    src  = A
    dest = kBroadcast
```

The base routing header does not distinguish a request-scoped reply from a new independent interaction between the same two Participants.

If a Service or Transport needs request correlation, retry identity, transaction state, sequence state, epochs, freshness state, or similar interaction context, that belongs to the corresponding Service/Transport mechanism.

## 2.6 `Direction` is not canonical PDU semantics

Remove Direction from the canonical base descriptor and from the core Wire semantic model.

Reason:

> Once both endpoints are named explicitly, the canonical routing layer only needs to identify who authored the PDU and who it is addressed to. “Who initiated this higher-level interaction” is Service/API state that the Router cannot validate and does not need for forwarding.

A Link profile may still define a Direction bit where it has real encoding value. In the proposed CAN11 compact form it means only which asymmetric participant field reconstructs canonical source versus destination.

## 2.7 Adopt the 48-bit / 6-byte canonical base descriptor

Adopt the field allocation in §4 as the new canonical base descriptor.

This is a deliberate break from the current 40-bit layout while interoperability is still unfrozen and before the canonical PDU stack is implemented in the prototype.

## 2.8 Keep Wire first-class and self-contained in the base descriptor

`WireNumber` remains a canonical field in the 6-byte descriptor.

A generic Router therefore does not need out-of-band metadata merely to determine which Wire a PDU belongs to.

Wire remains distinct from Physical Link and may:

- coincide with one Physical Link;
- select a subset of one Physical Link;
- share one Link with other Wires;
- span several heterogeneous Links through forwarding/splicing.

## 2.9 Preserve `EndpointAddress` as one 16-bit semantic value

Define:

```text
EndpointAddress = uint16_t

bits 15:14   Namespace
bits 13:0    EndpointId
```

The application/dispatcher-facing identity is one value even though Namespace and EndpointId remain separately meaningful subfields.

`EndpointId == 0` remains invalid in every Namespace unless separately reconsidered later.

Therefore each Namespace has:

```text
1..16383   valid EndpointId
```

or **16,383 valid Endpoint IDs per Namespace**.

Namespace 3 remains the public WireSpaces/FOSS ecosystem namespace.

---

# 3. Design principle: eliminate configuration with invariants

Add the following project-level principle:

> **When two designs provide comparable capability and cost, prefer the one that eliminates configuration by defining a simple protocol invariant.**

This principle is not a ban on configuration.

Configuration is justified when it buys meaningful capability, topology freedom, isolation, safety, security, or representability.

A projection between identity scopes is not “generality for its own sake” if it prevents a real capacity loss.

Examples of the intended bias:

```text
Prefer:
    one stable ParticipantId per Endpoint Domain

Over:
    identity that changes depending on which Wire is in use
```

```text
Prefer:
    deterministic encoding when two CAN compact fields could be arranged two ways

Over:
    a configuration bit choosing which arrangement to use
```

But:

```text
Do not reject a Link-local projection merely because it requires mapping
if the projection materially expands the systems the profile can represent.
```

For the initial CAN11 compact profiles, this proposal nevertheless chooses a direct low-global-ID restriction because the project is willing to accept the resulting deployment-wide CAN11 limit and provide CAN29/richer profiles as the escape path. Shared and exclusive CAN11 use the same asymmetric compact/general addressing model rather than two unrelated encodings. That trade must be documented as a limitation, not described as capability-equivalent to a carrier-local alias scheme.

---

# 4. Canonical base descriptor

## 4.1 Normative field allocation

The canonical base descriptor is **48 bits / 6 bytes**:

| Field | Bits |
|---|---:|
| QoS | 2 |
| Reserved | 2 |
| HasHeaderExtensions | 1 |
| TransportType | 3 |
| WireNumber | 8 |
| SrcParticipantId | 8 |
| DestParticipantId | 8 |
| Namespace | 2 |
| EndpointId | 14 |
| **Total** | **48** |

Normative conceptual layout:

```text
Control: 8 bits
    QoS                    2   // MSBs
    Reserved               2
    HasHeaderExtensions    1
    TransportType          3   // LSBs

WireNumber                 8
SrcParticipantId           8
DestParticipantId          8

EndpointAddress: 16 bits
    Namespace              2   // bits 15:14
    EndpointId            14   // bits 13:0
```

Canonical serialization remains little-endian for literal multi-byte numeric values.

Recommended serialized byte order:

```text
Byte 0      Control
Byte 1      WireNumber
Byte 2      SrcParticipantId
Byte 3      DestParticipantId
Byte 4      EndpointAddress[7:0]
Byte 5      EndpointAddress[15:8]
```

No implementation may use native C/C++ struct layout or compiler bitfields as the wire representation.

## 4.2 Why 48 bits

The extra byte is accepted because it removes multiple recurring pressures at once:

- enough Participant identity for multicore/internal Endpoint Domains without address extensions;
- enough Wire identity that internal Wires do not immediately pressure the deployment space;
- direct canonical src/dest instead of asymmetric canonical roles;
- preservation of a large Endpoint registry;
- simple byte-oriented codec structure.

For UART/HDLC, Ethernet, UDP, shared-memory, host simulation, FPGA FIFOs, and similar carriers, one additional canonical byte is a small absolute cost.

Classical CAN already requires a profile-specific compact representation, so the canonical descriptor should not be distorted merely to fit the CAN11 arbitration ID.

## 4.3 Why `8/8/8/{2|14}` and why EndpointId becomes 14 bits

The four address-like resources do not have the same lifetime.

```text
WireNumber
    deployment-local
    reassignable as topology changes

ParticipantId
    deployment-local
    reassignable across deployments/replacement planning

EndpointId, especially Namespace 3
    ecosystem-visible
    potentially permanent once published
```

The current architecture gives canonical EndpointId 16 bits per Namespace. Revision 5 deliberately reduces that to 14 bits: from 65,535 valid nonzero Endpoint IDs per Namespace to 16,383.

That is a real 4x reduction in the durable registry and is accepted for two reasons:

1. 16,383 permanent IDs per Namespace remains very large for the intended embedded/FOSS ecosystem; and
2. the two recovered bits enable a simple byte-oriented 48-bit descriptor with full-byte Wire/source/destination fields, avoiding awkward cross-byte 10-bit routing fields and leaving Participant/Wire capacity comfortably above observed machine scale.

This is not presented as “more Endpoint headroom than today.” It is an intentional registry-space trade for a cleaner canonical routing format while retaining substantially more registry capacity than the rejected 10-bit EndpointId proposal.

Compared with revision 3's symmetric `10/10/10/10` allocation, revision 5 gives:

| Resource | Revision 5 base capacity |
|---|---:|
| WireNumber encoding | 256 values |
| ParticipantId | 255 ordinary values + broadcast sentinel |
| EndpointId | 16,383 valid values per Namespace |
| Namespaces | 4 |

A public registry is a one-way door: assignments may be reserved, deprecated, partitioned into ranges, or allocated to third parties and should not be expected to be reclaimed. Spare Endpoint space is therefore more valuable than excess machine-local routing capacity, but not infinitely valuable; 14 bits is the chosen balance for the 6-byte base descriptor.

## 4.4 Control byte

Define:

```text
bits 7:6    QoS
bits 5:4    Reserved
bit  3      HasHeaderExtensions
bits 2:0    TransportType
```

Reserved bits:

- transmit as zero;
- a receiver seeing a nonzero reserved value rejects/counts the PDU under the existing reserved-field rule;
- do not allocate them merely because they are available.

Namespace moves out of Control and into EndpointAddress.

## 4.5 `ParticipantId`

Canonical field width: 8 bits.

Proposed values:

```text
0..254   ordinary Participants
255      broadcast destination sentinel
```

PID 0 is valid. This is important for compact Link profiles that deliberately reserve low IDs.

`255` is invalid as a source.

## 4.6 `EndpointAddress`

Define:

```text
EndpointAddress = (Namespace << 14) | EndpointId
```

where:

```text
Namespace      0..3
EndpointId     1..16383
EndpointId 0   invalid/reserved in every Namespace
```

This yields:

```text
16,383 valid Endpoint IDs / Namespace
65,532 valid Namespace/EID combinations
```

Namespace assignment remains:

```text
Namespace 0    default/user; compact/common-service policy may reserve subranges
Namespace 1    user-defined
Namespace 2    user-defined
Namespace 3    public WireSpaces / FOSS ecosystem
```

The registry process remains separate from this change.

## 4.7 `WireNumber`

Canonical encoded width: 8 bits.

This change **does not flatten the current Wire scope model**.

The following semantic classes remain:

```text
named network Wire

device-private Wire
    real Wire inside one physical device/SoC
    may be spliced to a network-visible Wire
    must not escape externally under its private number

kLocalDomain
    local Endpoint-Domain sentinel
    never enters a Link Interface
    never spliceable

kLocalBus
    Link-local representation of the native physical Wire
    not itself a canonical WireNumber allocation
```

Because the canonical field shrinks from 10 to 8 bits, the existing numeric split (`0..895`, `896..1022`, `1023`) cannot survive literally.

**Gating allocation item:** before `BITS`/`CORE` freeze the new field, choose an 8-bit partition that preserves:

- a substantial network-visible range;
- a useful contiguous device-private range;
- one top-level `kLocalDomain` sentinel;
- the existing rule that device-private values never leave the device except through an explicit splice to a network-visible Wire.

Do not silently treat all 256 values as one flat network-visible namespace.

Whether `InternalDebugWire` receives a standard reserved device-private number remains the existing open question unless separately resolved.

---

# 5. Header extensions

## 5.1 Retain extension presence in the base descriptor

`HasHeaderExtensions` remains a base bit.

The larger base header means extensions are no longer needed merely to rescue undersized Participant/Wire/Endpoint fields.

Extensions should therefore be reserved for genuinely uncommon canonical metadata.

## 5.2 Preserve the current self-describing-length direction

The current architecture already has a useful working direction:

```text
first extension byte:
    length code 00    1 extension byte total
    length code 01    2 extension bytes total
    length code 10    3 extension bytes total
    length code 11    invalid/reserved -> reject
```

This yields 6, 14, or 22 content bits after the size code.

Revision 4 does not replace that mechanism.

Because the base descriptor grows from 5 to 6 bytes, retaining the existing maximum three extension bytes changes the practical compact-header ceiling from approximately 8 bytes to **9 bytes**.

That is an explicit cost of the 48-bit base descriptor and should be reflected in `CORE`/`REG`. It is not, by itself, a reason to squeeze the base header back to 5 bytes.

## 5.3 Extension forwarding rule remains open

Preserve the current open question:

- generic forwarding likely preserves well-formed unknown extensions;
- local dispatch may reject a PDU whose unrecognized extension is required for safe interpretation;
- this should be decided per extension or by an explicit extension policy before the first extension is standardized.

---

# 6. Internal and device-private Wires

## 6.1 Internal Wires remain first-class

Device-private/internal Wires remain useful and are not an alternative to assigning Participant IDs to internal Endpoint Domains.

```text
ParticipantId
    WHO authored / receives the PDU

Wire
    WHICH logical communication domain

EndpointAddress
    WHAT Endpoint/service interface
```

A multicore SoC may therefore contain several internal Participants sharing a small number of internal Wires.

Internal Wire count should follow logical communication structure, not physical distance, register stages, cores, or NoC hops.

## 6.2 Preserve the existing `InternalDebugWire` pattern

`InternalDebugWire` is already an established device-private convention and should be **preserved/promoted**, not presented as a newly invented mechanism.

Typical standard publishers may include:

```text
TextLog
Event
Health
OS telemetry
version/build information
fault/crash records
Link Entity status
```

The exact service list is platform/service policy, not a base-protocol invariant.

Each internal Domain retains its own ParticipantId, so an external logger can distinguish the actual Domain that authored a record after an explicit splice exposes the traffic.

## 6.3 Splicing internal debug traffic

A device-private debug Wire may be spliced to a network-visible Wire before external egress.

Ordinary forwarding preserves:

```text
src ParticipantId
EndpointAddress
payload
```

and the splice changes only the Wire representation/scope as allowed by splice rules.

The splice remains an explicit trust/scope boundary where private traffic becomes externally visible.

## 6.4 Global Participant IDs simplify splicing

Retire the current cross-splice requirement to coordinate per-Wire NodeIds across separately authored device-private segments.

Distinct Endpoint Domains already have deployment-global Participant IDs, so splicing no longer creates a new canonical NodeId collision problem.

The splice still must validate:

- both sides have usable Wire identity;
- the resulting forwarding realization obeys routing-loop constraints;
- both Links can represent the canonical PDU;
- the egress Wire scope is externally valid;
- no device-private WireNumber escapes externally unchanged.

---

# 7. `kLocalBus`, Level-0 bring-up, and membership

## 7.1 Preserve the zero-Wire-configuration path

The current `kLocalBus` concept remains valuable and should survive the Origin/Node removal.

`kLocalBus` continues to mean the Link-local representation of **this Link's native physical Wire**.

It does not consume a globally special canonical WireNumber.

When the native Wire has a canonical WireNumber, the LLL reconstructs that canonical identity.

When it does not, the Anonymous LocalBus remains locally meaningful but:

- local delivery is allowed;
- generic cross-Link forwarding is not allowed;
- splicing is not allowed until canonical Wire identity is established.

## 7.2 What “zero configuration” means after global ParticipantId

Level-0/bring-up should continue to require no Organizer and no Manifest.

It does **not** mean a multi-access bus may emit ordinary traffic with colliding or nonexistent canonical source identities.

A simple device may obtain its ParticipantId from a compile-time/default/profile-specific source without any runtime deployment tooling.

Examples:

```text
point-to-point / board-local development link:
    fixed trivial PIDs are sufficient

single anonymous physical Wire:
    kLocalBus avoids WireNumber configuration

shared unconfigured CAN:
    commissioning Link-control traffic occurs before ordinary PDU identity is enabled
```

Commissioning traffic is below canonical Wire semantics and therefore does not need an “anonymous ParticipantId” sentinel in the base descriptor.

## 7.3 Membership is not learned from observed traffic

Preserve the current rule:

> **A Router/gateway must not infer Wire membership or forwarding authority merely because it observed traffic.**

Static global configuration remains optional, but membership can be established through different mechanisms depending on the Wire:

- a simple physical/native Wire may have membership implied by the Link/profile and attached participants;
- a configured Virtual Wire, subset Wire, splice, or cross-Link Wire uses explicit/generated/commissioned routing state;
- Organizer-driven dynamic setup may install ordinary read-mostly/static forwarding state;
- observed traffic alone never creates a route or membership relation.

This reconciles low-configuration development with predictable forwarding.

---

# 8. Canonical routing, forwarding, and composition

The canonical forwarding identity is:

```text
WireNumber or locally valid native-Wire context
SrcParticipantId
DestParticipantId / broadcast
EndpointAddress
```

with Control metadata and payload.

## 8.1 Ordinary forwarding

Ordinary forwarding preserves:

```text
Wire identity
SrcParticipantId
DestParticipantId
EndpointAddress
QoS
TransportType
recognized/forwardable header extensions
payload
```

Only Link/profile-local representation may change.

## 8.2 Composition

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

No special “re-originated” bit is required.

## 8.3 Source lineage

Forwarding preserves the original canonical source.

Composition does not: the composing Domain is the source of the new PDU.

This cleanly preserves the useful current source-lineage distinction without a permanent Origin role.

---

# 9. Transmit binding model after Direction removal

Removing canonical Direction requires an explicit rewrite of the current five binding modes. The binding family remains; its context becomes simpler.

## 9.1 Static

Transmit context comes from explicit/generated local configuration or a fixed bounded set selected by Service policy.

The binding supplies, as applicable:

```text
Wire
DestParticipantId / bounded destination set
EndpointAddress
TransportType
```

The local Endpoint Domain supplies `SrcParticipantId`.

Static configuration remains one mechanism, not a prerequisite for all communication.

## 9.2 Transmit-only

An autonomous publisher/command Endpoint takes no context from ingress.

Its Wire/destination context is supplied through its transmit binding/profile/defaults.

The local Domain supplies source PID.

## 9.3 Receive-only

No transmit context exists.

## 9.4 Request-scoped

A validated ingress creates an **opaque, bounded, single-use reply context**.

Conceptually the reply context contains enough validated information to produce:

```text
reply.wire = received wire
reply.dest = received src
reply.src  = local ParticipantId
```

plus any fixed Endpoint/Transport constraints and Service/Transport correlation state required by the registration.

The Service does not gain arbitrary transmit authority merely by reading source metadata.

The reply context remains revalidated at transmit.

The previous useful property survives:

> A reply-only Endpoint does not need a separately configured autonomous destination merely to answer a validated request.

This is now a **portable Endpoint-API property**, not a special property encoded by Direction in the PDU.

## 9.5 Learned-from-ingress

A registration may retain a bounded number of validated peer selections derived from ingress.

The retained object is a bounded framework capability/context, not an arbitrary copied PID that automatically becomes permission to transmit.

Preserve the existing safety rule:

> Learned-from-ingress is disabled on unauthenticated multi-access Links unless a profile/security mechanism makes the peer identity trustworthy enough for that use.

## 9.6 Metadata is informational

A received source PID may be exposed as metadata.

Reading it does not itself confer transmit authority.

This preserves the intent of existing `DISP-7`/`DISP-15` while removing Direction/NodeId terminology.

## 9.7 One binding mode per registration remains

Retain the rule that each Endpoint registration declares one binding mode.

If one Service needs both request-scoped replies and autonomous publication, use separate registrations/bindings so the bounded authority/lifetime of the two paths remains explicit.

---

# 10. Broadcast semantics

Canonical broadcast is:

```text
src  = actual ParticipantId
 dest = 0xFF
```

Broadcast is scoped by the Wire.

It does not imply:

- every device electrically hearing the frame is a semantic member;
- every Wire member implements the Endpoint;
- reliable multi-recipient acknowledgement;
- multicast groups;
- remote-source authorization.

Ordinary forwarding of a broadcast preserves source and broadcast destination while preserving the same logical Wire identity.

A gateway may not silently turn a broadcast on one Wire into broadcast on a different Wire through ordinary forwarding. Crossing a composition boundary produces a new PDU and the composing Participant becomes source.

There is only one base broadcast mode: **Wire-wide broadcast**.

A narrower audience is represented by directed sends, repeated directed sends, or future separately designed multicast/group semantics.

---

# 11. Authorization and configuration boundary

## 11.1 No mandatory remote-source authorization

This request does not define:

- Endpoint/source ACLs;
- permitted-source sets;
- authorization classes;
- Organizer-generated permission matrices.

A permissive development deployment must work without them.

Canonical source identity provides enough information for a future/application policy to make such a decision.

```text
structurally valid PDU
    != authenticated sender
    != authorized operation
```

## 11.2 Preserve local authority mechanisms

Do not rewrite `DISP-5` as “WireSpaces has no authority model.”

The intended distinction is:

```text
EndpointAddress
    naming

TX bindings / typed local access
    constrain what local code can transmit through a given registration/API

remote-source admission
    optional and outside this change
```

## 11.3 Record optional source-admission vocabulary

The experiments repeatedly found a useful conceptual distinction:

```text
permitted source
    statically/structurally acceptable in some supported state

accepted source
    currently accepted under Service/runtime state
    (term, mode, freshness, maintenance state, etc.)
```

Record this vocabulary as **informative/open** in `REG`/`FUTURE`.

Do not standardize a Manifest schema, ACL format, or runtime predicate language in this change.

## 11.4 Static configuration remains optional

No base communication rule may require:

- a static Manifest;
- an Organizer;
- a global static Wire membership database;
- static source authorization.

This does not prevent gateways, Virtual Wires, or advanced deployments from requiring explicit routing state where the topology itself requires it.

---

# 12. Classical CAN 11-bit compact profiles and commissioning direction

Classical CAN 11-bit remains the project's most constrained important profile.

It does not carry the 48-bit canonical descriptor literally in the arbitration identifier. The CAN Link layer reconstructs canonical source/destination identity from an asymmetric compact representation.

The shared and exclusive variants deliberately use **the same addressing model and the same two-bit QoS width**. Shared mode buys coexistence by reducing the compact Participant field by one bit.

## 12.1 Intentional deployment-wide low-PID restriction

The initial CAN11 direction represents **low global Participant IDs directly**, without a Participant alias table.

This means the restriction is deployment-wide, not per CAN bus.

That is a real capability trade and is accepted provisionally because:

- many target systems will have fewer than ~30 CAN11-visible Endpoint Domains total;
- richer CAN29 support is planned;
- CAN FD/extended-ID and other Links do not inherit the restriction;
- avoiding an alias layer simplifies commissioning, captures, gateway state, and failure modes.

Do not describe this as capability-equivalent to a carrier-local projection.

A future profile may add local projections if concrete deployments show the low-global-ID budget is too restrictive.

Recommended allocation policy:

> Reserve low Participant IDs preferentially for Endpoint Domains that require CAN11 compact visibility. Do not casually consume them on Ethernet-only/internal-only Participants.

This is deployment allocation guidance, not a second identity namespace.

## 12.2 Common asymmetric addressing model

Both profiles encode one Participant in a **compact** field and the other in a 5-bit **general** field.

`Direction` reconstructs canonical source and destination:

```text
CompactToGeneral:
    src  = CompactParticipantId
    dest = GeneralParticipantId

GeneralToCompact:
    src  = GeneralParticipantId
    dest = CompactParticipantId
```

Direction is Link-local compression information only. It has no canonical request/reply, client/server, command/status, or authority meaning.

The general field directly represents canonical:

```text
PID 0..30
31 = Link-level broadcast destination code
```

The Link-level broadcast code canonicalizes to `DestParticipantId = 0xFF`; it is not canonical ParticipantId 31.

## 12.3 CAN11-WS-Exclusive

When WireSpaces owns the relevant 11-bit CAN identifier space:

```text
CAN11-WS-Exclusive
------------------
QoS                         2
CompactParticipantId        3
GeneralParticipantId        5
Direction                    1
--------------------------------
                            11
```

The compact field directly represents canonical:

```text
PID 0..7
```

An ordinary addressed PDU is representable only if:

1. both Participants are in canonical PID range `0..30`; and
2. at least one endpoint is in PID range `0..7`.

Consequences:

- up to eight Participants may occupy the compact role;
- up to 31 global Participants are eligible for direct CAN11 visibility;
- traffic between two Participants both outside `0..7` is not representable in this profile;
- a fully pairwise-connected peer group can contain at most **nine** Participants: the eight compact-range Participants plus at most one general-only Participant.

The nine-peer bound is a consequence of pair representability, not a canonical WireSpaces limit.

## 12.4 CAN11-WS-Shared

When WireSpaces must coexist with non-WS 11-bit CAN traffic:

```text
CAN11-WS-Shared
---------------
QoS                         2
ProtocolDiscriminator       1
CompactParticipantId        2
GeneralParticipantId        5
Direction                    1
--------------------------------
                            11
```

The compact field directly represents canonical:

```text
PID 0..3
```

An ordinary addressed PDU is representable only if:

1. both Participants are in canonical PID range `0..30`; and
2. at least one endpoint is in PID range `0..3`.

Consequences:

- up to four Participants may occupy the compact role;
- the same ~31-participant deployment-wide CAN11-visible pool remains;
- traffic between two Participants both outside `0..3` is not representable;
- a fully pairwise-connected peer group can contain at most **five** Participants.

The ProtocolDiscriminator is Link-profile framing/classification, not canonical PDU metadata.

The exact bit position and legacy-ID allocation rule remain a `LINK` design item. Shared-mode coexistence must be specified in a way that gives the bus owner an auditable non-WS/WS identifier allocation; do not assume one discriminator bit automatically makes arbitrary legacy coexistence trivial.

## 12.5 QoS is consistently two bits

Both CAN11 profiles preserve the canonical four QoS classes.

Do not define a one-bit-QoS CAN11 variant merely to gain Participant capacity unless a future profile is separately justified.

This is a deliberate consistency rule:

> A canonical QoS class has the same four-class meaning on CAN11 shared, CAN11 exclusive, CAN29, and other ordinary WS Links that implement QoS.

The exact arbitration-order field placement is still profile-specific, but QoS remains at the most arbitration-significant end of ordinary WS traffic.

## 12.6 Deterministic encoding when both Participants fit the compact range

If both Participants fit the compact field, use one deterministic representation.

For either profile:

```text
CompactParticipantId = min(src, dest)
GeneralParticipantId = max(src, dest)

Direction =
    CompactToGeneral if src <= dest
    GeneralToCompact otherwise
```

Thus `src == dest`, if canonical self-addressed traffic is permitted, uses `CompactToGeneral` and has a single representation.

If exactly one Participant fits the compact range, that Participant occupies the compact field.

Freeze this rule with golden vectors in `LINK`/`CONFORM`.

## 12.7 Broadcast semantics and profile limits

Ordinary Wire-wide broadcast uses:

```text
GeneralParticipantId = 31
Direction             = CompactToGeneral
CompactParticipantId  = source
```

Therefore the broadcast source must fit the compact field:

```text
CAN11-WS-Exclusive    source PID 0..7
CAN11-WS-Shared       source PID 0..3
```

This is an explicit compact-profile limitation.

It matters particularly for peer-style systems: in shared mode only four Participants can originate true Wire-wide CAN11 broadcast; in exclusive mode only eight can. Other Participants may still send addressed publications, but that is not canonical Wire-wide broadcast.

The peer-CAN sketch that motivated multi-initiator Wires (3–5 peers, each potentially initiating) therefore fits shared mode only at its upper edge and fits exclusive mode with more headroom. That limitation must remain visible in `LINK`/`REG` rather than being hidden behind observation semantics.

## 12.8 Commissioning/control uses the otherwise-invalid broadcast-source subspace

Both profiles reserve the structurally invalid ordinary-addressing combination:

```text
GeneralParticipantId = 31          // broadcast code
Direction             = GeneralToCompact
```

for **CAN11 Link Control / commissioning**.

Reason:

- broadcast may be a destination;
- broadcast is never a canonical source;
- therefore “broadcast -> compact Participant” has no ordinary PDU meaning.

Ordinary broadcast remains the opposite Direction (`CompactToGeneral`).

Within the reserved commissioning subspace, the compact field and CAN payload are interpreted by the CAN commissioning protocol rather than as canonical Participant addressing.

This reservation works in both shared and exclusive profiles. Shared mode therefore does **not** have to give up in-band commissioning merely because one identifier bit is spent on the WS discriminator.

Commissioning/control frames are below ordinary Wire semantics and are not canonical WS Service PDUs.

## 12.9 Basic commissioning flow

The full commissioning message set/state machine is out of scope, but the architecture is:

1. **Uncommissioned devices emit no ordinary WS Service traffic.** They do not invent an anonymous canonical ParticipantId.
2. A commissioner uses the reserved CAN11 Link-control subspace to discover unassigned devices by a stable commissioning/device identity whose exact width/format is profile work.
3. The previously explored collision-safe discovery technique remains the preferred direction: query an identity prefix; every matching anonymous device emits the same bit-identical response; use prefix/binary search until one device is isolated.
4. A deployment-level allocator chooses a globally unused canonical `ParticipantId` compatible with the CAN11 profile. The allocator must understand scarce compact ranges (`0..7` exclusive, `0..3` shared) and reserve them for Participants that actually need compact-role capabilities such as broad communication or Wire-wide broadcast.
5. The commissioner assigns/commits that canonical ParticipantId to the selected device; after successful commissioning, the device may emit ordinary WS traffic.

The CAN-local discovery mechanism and the deployment-global allocation policy are distinct responsibilities.

A commissioner attached through a gateway may perform local CAN discovery while a machine-level allocator decides which global PID to assign.

No permanent CAN Participant alias table is introduced by this flow.

## 12.10 Commissioning arbitration requirement

Commissioning/control traffic should lose CAN arbitration to every ordinary WS frame, including ordinary Background traffic, where the final identifier ordering permits it.

The reserved combination (`GeneralParticipantId=31`, `GeneralToCompact`) was chosen partly because the all-ones general field can naturally place commissioning late within a QoS class.

The final physical bit ordering must prove this requirement for each profile rather than merely state it.

For shared mode, the placement of `ProtocolDiscriminator` must also satisfy the bus-owner coexistence/allocation rule; shared and exclusive profiles may therefore use different exact bit positions while retaining identical compact/general/Direction semantics.

## 12.11 Profile selection is a Link invariant

A CAN11 Link/profile instance is defined as one of:

```text
CAN11-WS-Exclusive
CAN11-WS-Shared
```

All WS Participants on one physical CAN segment use the same profile interpretation.

This is not negotiated independently per Participant or per frame.

## 12.12 Gating issue: Wire identity on CAN11

Neither proposed CAN11 identifier has room for the old 3-bit `WireAlias`.

This remains the principal unresolved blocker for declaring the compact CAN11 profiles complete.

WireSpaces must not silently collapse “Wire” into “physical CAN bus” merely because the arbitration ID is full.

Candidate directions to evaluate in the focused CAN encoding review:

1. **Single-Wire-per-CAN11-profile instance** as an explicit compact-profile limitation.
2. **Carry Wire identity in PDUA/data framing**, preserving several logical Wires on one physical bus at some payload cost.
3. **Define a richer CAN11 representation** that trades another property/capacity for Wire selection.
4. **Use CAN29** where same-bus multi-Wire multiplexing is important.

The current bit budget makes single-Wire-per-profile the simplest direction, but this request does not silently adopt it. `LINK` must choose explicitly and measure the impact against sketches that place several Wires on one CAN segment.

Commissioning traffic does not require a WireNumber because it operates below Wire semantics.

## 12.13 `PduControl`, Endpoint placement, and optimized N=1 must be redesigned/revalidated

Moving Namespace from the canonical Control byte into `EndpointAddress` breaks the current `BITS §4` optimization where canonical Control and CAN `PduControl` intentionally share Namespace / extension / Transport bits.

The CAN profile must therefore redesign `PduControl`; this is not a mask-only update.

The natural current direction is still that CAN represents only a compact Endpoint subset directly and rejects/unplaces canonical EndpointAddresses that cannot be represented by the selected CAN PDU form.

The focused profile review must explicitly measure:

- N=1 payload capacity;
- the directly representable Endpoint subset;
- where Namespace is carried in CAN PDUA;
- where Wire identity is carried;
- CRC/aggregation consequences;
- whether an N=1-only tiny node remains a valid first-class WS device.

Do not allow the canonical 14-bit Endpoint space to accidentally destroy the small-message CAN profile without measuring the trade.

---

# 13. Wire boundaries and degraded state

Do not create a new Wire merely because a different Participant produces or initiates a Service.

Stronger reasons to split Wires include:

- different route/forwarding realization;
- different meaningful Participant scope;
- different physical or operational failure boundary;
- different Wire-wide broadcast scope;
- a real composition boundary.

Different directed Endpoint recipient sets alone are not automatically a reason to split.

Broader Wires have a cost observed repeatedly in the sketches:

> A Wire may remain configured-valid while gateway or Link failure partitions which Participants can currently reach one another.

Therefore diagnostics/tooling should not model a Wire with only one boolean `up/down` state.

Useful future reporting distinctions include:

```text
Participant availability
Link/segment availability
forwarding-edge availability
reachable component(s) of a Wire
Service/runtime authority availability
```

Record this as a tooling/telemetry requirement/open item; no new base PDU field is required now.

---

# 14. Redundancy and runtime controller ownership

Retire the old implication:

```text
one Origin per Wire
therefore redundant controllers require separate Wires
```

Multiple potential controllers may share one Wire.

If only one should be accepted at a time, active controller/term/epoch/mode is Service/application state unless a future feature deliberately standardizes it.

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

# 15. Structural validity

The change removes Origin/Node-specific structural rules but does **not** narrow structural validation generally.

Base structural validity includes, at minimum:

- reserved Control bits are zero;
- QoS/Transport values are valid/supported as required;
- Wire scope is valid for the ingress/egress context;
- source is a valid ParticipantId and not broadcast;
- destination is a valid ParticipantId or broadcast;
- EndpointId is nonzero and representable;
- Namespace is valid;
- header-extension presence/length is well-formed;
- canonical identity resolves unambiguously for the given ingress;
- the existing one-producer-per-external-Endpoint-identity rules remain unless separately changed;
- device-private and `kLocalDomain` scope rules remain enforced;
- no silent truncation or identity reinterpretation occurs.

For a constrained Link profile, profile representability is also structural validity.

For CAN11 compact this includes:

- general PID `0..30` or broadcast code `31`;
- exclusive compact PID `0..7` or shared compact PID `0..3`, according to the selected Link profile;
- at least one addressed endpoint in that profile's compact range;
- deterministic both-compact representation;
- valid shared ProtocolDiscriminator classification where applicable;
- reserved commissioning/control combination interpreted below ordinary PDU semantics;
- valid Wire representation once §12.12 is resolved.

The following are **not** base structural validity:

- source appears in an authorization table;
- source is the currently active redundant controller;
- source is authorized by a security policy;
- Participant appears in a static global Manifest.

Duplicate deployment Participant IDs are invalid deployment state. Detection/recovery may be tooling/commissioning behavior rather than a wire-level reply protocol.

---

# 16. Experimental evidence and limits

## 16.1 Global Participant identity

The multicore gateway remaps showed material benefit from global Endpoint-Domain identity even where multi-initiator behavior contributed little.

This supports Participant identity independently from the Wire-initiation change.

## 16.2 Peer CAN

The peer-CAN case showed that permanent single-Origin topology creates artificial structure in genuinely peer-like systems.

The CAN11 encoding work adds a separate profile constraint that was not priced by the remap: shared CAN11 supports a fully pairwise-connected peer group of at most five Participants and true broadcast from only four compact-range Participants; exclusive CAN11 raises those limits to nine and eight respectively. These are Link-profile limitations, not evidence against the multi-initiator semantic model, and must be judged independently.

## 16.3 AMR

The AMR generalized the result beyond a flat peer bus: several production controllers naturally initiate different interactions across hierarchical physical topology.

## 16.4 Excavator

The excavator is physically hierarchical but still contains VCU-, Safety-, BMS-, gateway-, and smart-leaf-initiated interactions.

This is strong evidence that multi-initiator Wires are not merely a peer-network feature.

## 16.5 BESS

The BESS showed that controller failover can change runtime acceptance without changing Participant identity, Wire identity, membership, or routing.

It also showed that serious production deployments may want both static source capability and runtime accepted-source state. This proposal records that architectural need without standardizing the policy mechanism.

## 16.6 What the experiments do not prove

The remap experiment assumed an Origin/Direction model and explicit origin-admission policy as an accounting device.

It therefore does not prove that either mechanism belongs in the canonical protocol.

The experiments also deliberately fenced off bit-layout costs, so the 48-bit descriptor and CAN11 representation need independent encoding/profile review.

No remap directly tested canonical src/dest with Direction removed; that change is justified by routing/API analysis rather than by the remap experiment itself.

The planned RS-485 negative-control remap and old redundant-gateway remap were not completed. Record this as an evidence limitation. Before final semantic freeze, either run the RS-485 negative control or record an explicit waiver/rationale against the preregistered decision rule.

---

# 17. Library/API consequences

Suggested semantic types:

```cpp
struct ParticipantId {
    uint8_t value;
};

struct WireNumber {
    uint8_t value;
};

struct EndpointAddress {
    uint16_t value;

    Namespace name_space() const;
    uint16_t endpoint_id() const;  // 14-bit, nonzero
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

Exact wrapper/constructor style remains a `LIB` implementation matter.

Important consequences:

- no canonical `NodeId` type;
- no canonical `Direction` enum;
- source metadata is directly `descriptor.src`;
- `WireNumber` remains directly available in the descriptor for named Wires;
- Endpoint dispatch uses `EndpointAddress`;
- CAN-profile Direction is a Link-profile-specific enum only;
- request/reply capability is represented by Endpoint API context rather than canonical Direction.

Example profile-local type:

```cpp
enum class Can11Direction : uint8_t {
    kCompactToGeneral,
    kGeneralToCompact,
};
```

For Anonymous LocalBus, the runtime may carry Link-relative Wire context until canonical Wire identity exists, exactly as the current architecture already distinguishes local anonymous traffic from generic forwarded canonical traffic.

---

# 18. Required repository changes

This section is the implementation checklist for the repository update.

## 18.1 `docs/README.md`

Update the project summary to introduce:

- Endpoint Domain / Participant identity;
- multi-initiator Wires;
- source/destination canonical addressing;
- the 48-bit base descriptor at summary level;
- Link profiles as allowed to use compact representations.

Do not call the formal model “multi-Origin” after Origin is retired.

## 18.2 `docs/introduction.md` (`INTRO`)

Update mental models and examples.

Show both:

1. a conventional one-controller system;
2. a distributed/multi-initiator system.

Preserve the usage-level principle that Levels 0–1 are protected from advanced-feature/configuration complexity.

Update Level-0 text so `kLocalBus` still removes Wire configuration while canonical Participant identity is supplied by a trivial/profile-specific mechanism.

## 18.3 `docs/core_architecture.md` (`CORE`)

This is the largest semantic edit.

### Layering / Endpoint Domain

- make `ParticipantId` the identity of each Endpoint Domain;
- remove the current statement that `ParticipantId` is retired terminology;
- retain Endpoint Domain as dispatch/concurrency boundary;
- state that physical closeness still does not collapse Domains.

### Canonical descriptor

Replace the 40-bit descriptor with §4.

Update:

- little-endian byte order example;
- address-space snapshot;
- metadata-size discussion;
- extension maximum/header-budget consequence.

### Wire model

Replace one-Origin/many-Node with multi-initiator Participant membership.

Remove Direction semantics from canonical Wire architecture.

Rewrite “what a Wire guarantees” so membership/routability remains distinct from application authorization.

### Wire scope

Preserve:

- named network Wire;
- device-private Wire;
- `kLocalDomain`;
- `kLocalBus`.

Re-derive the numeric 8-bit WireNumber partition rather than flattening current scope rules.

### `kLocalBus`

Preserve anonymous native-Wire semantics and local-only forwarding restrictions.

Rewrite `WireAlias` material as needed by Link-profile redesign rather than deleting zero-config behavior accidentally.

### Splicing

Remove:

- exactly-one-Origin validation;
- cross-splice NodeId uniqueness/coordination.

Retain:

- canonical Wire identity requirement;
- scope checks;
- representation/capability validation;
- routing-loop/acyclicity rules where still current.

### Internal Debug Wire

Preserve the existing §7 convention and update it for per-Domain Participant identity.

Keep the reserved device-private number question open unless separately resolved.

### Endpoint namespaces

Change canonical EndpointId from 16 to 14 bits while retaining `EndpointId == 0` invalid.

Keep Namespace 3 as FOSS ecosystem.

Expose `(Namespace, EndpointId)` as `EndpointAddress` in semantic/API prose.

### Receive metadata

Replace `Direction + NodeId` source metadata with `SrcParticipantId`.

For anonymous LocalBus, retain ingress-Link-relative context where canonical Wire identity is not yet established.

### Transmit Endpoints / binding modes

Rewrite all five modes using §9.

Preserve:

- opaque request-scoped reply contexts;
- learned-from-ingress bounds;
- learned-from-ingress restrictions on unauthenticated multi-access Links;
- one binding mode per Endpoint registration;
- source metadata does not itself confer transmit authority;
- one consuming Wire per transmit Endpoint unless separately reconsidered.

### Routing / forwarding

Route using Wire + destination; preserve source lineage.

Composition makes the composing Domain the source of the new PDU.

### Structural validity

Retire only Origin/Node-specific checks. Preserve all unrelated structural checks.

### Redundancy

Remove the claim that base-Wire one-Origin semantics force redundancy composition from separate Wires.

Keep physical redundancy, duplicate suppression, and failover mechanisms distinct from multi-initiator addressing.

## 18.4 `docs/architecture_register.md` (`REG`)

The change is incomplete until `REG` reflects it.

### Strong/current direction

Replace the model summary with:

```text
one deployment-global ParticipantId per Endpoint Domain
multi-initiator Wires
canonical src/dest addressing
48-bit / 6-byte base descriptor
8-bit WireNumber
8-bit SrcParticipantId
8-bit DestParticipantId
16-bit EndpointAddress = Namespace[2] + EndpointId[14]
Direction only where a Link profile uses it as compression metadata
```

### Superseded / retire

Retire without reusing invariant IDs:

- one Origin per Wire;
- formal Node role;
- canonical per-Wire NodeId;
- canonical Direction;
- 40-bit / 5-byte descriptor;
- `RoutingWord = Direction + NodeId + WireNumber`;
- 5-bit NodeId / 31-Node canonical limit;
- current CAN11 `QoS + Direction + WireAlias + NodeId` allocation;
- preferred NodeId conventions tied to the old model.

### Preserve/open

Record:

- optional standardized source admission may be useful;
- permitted-source vs accepted-source vocabulary;
- one ParticipantId per Endpoint Domain is a deliberate trade/reopen condition;
- broad Wires require richer degraded/reachability reporting;
- exact 8-bit network-visible/device-private Wire partition is open/gating;
- CAN11 multi-Wire representation is open/gating;
- CAN11 deployment-wide low-ID restriction is intentional and should be reevaluated only against concrete deployments;
- CAN11 broadcast-source restriction is a profile cost;
- extension-content semantics remain open;
- maximum canonical header becomes 9 bytes if the current 1–3-byte extension format is retained.

### Design principle

Add §3's invariants-over-configuration rule including the “comparable capability and cost” proviso.

## 18.5 `docs/bit_layout.md` (`BITS`)

Replace the canonical descriptor with the byte-oriented 48-bit layout.

Specify:

```text
Control byte
WireNumber byte
SrcParticipantId byte
DestParticipantId byte
EndpointAddress little-endian uint16
```

Define masks/constants for:

- QoS;
- Reserved bits;
- HasHeaderExtensions;
- TransportType;
- Namespace extraction;
- 14-bit EndpointId;
- broadcast PID.

Retain reserved-field rejection.

Remove canonical NodeId and Direction.

Update the extension-size section for a 6-byte base / 9-byte maximum if the 1–3-byte format is retained.

**Explicitly redesign current `BITS §4` CAN `PduControl`.** Moving Namespace out of canonical Control means the existing shared-bit mask/OR optimization no longer applies, and `EndpointId[9:8]` must be reconciled with the new 14-bit canonical EndpointId. This is a real CAN layout change, not a mechanical field rename.

## 18.6 `docs/link_profiles.md` (`LINK`)

Treat the old 11-bit Classical-CAN identifier allocation as superseded.

Define the common asymmetric addressing semantics and separately specify:

```text
CAN11-WS-Exclusive
    QoS[2] + CompactPID[3] + GeneralPID[5] + Direction[1]

CAN11-WS-Shared
    QoS[2] + ProtocolDiscriminator[1] + CompactPID[2] + GeneralPID[5] + Direction[1]

CAN29 general/richer profile
```

Preserve/revalidate:

- four QoS classes in both CAN11 profiles;
- deterministic CAN arbitration;
- general PID `31` as Link-level broadcast code;
- `GeneralPID=31 + GeneralToCompact` as reserved commissioning/control subspace;
- commissioning priority below ordinary traffic where the final ordering permits it;
- PDUA/CRC/aggregation behavior;
- optimized N=1 capability;
- shared/coexistence identifier allocation with legacy bus owners.

Explicitly document:

- deployment-wide low-global-PID restriction;
- exclusive compact range `0..7`;
- shared compact range `0..3`;
- general range `0..30`;
- pairwise representability limits;
- shared/exclusive peer-mesh and broadcast-source limits;
- Direction as Link-local source/destination reconstruction only.

Do not add a Participant alias table in these initial compact profiles.

The compact CAN11 profiles remain **not complete until Wire identity/multiplexing is resolved**.

Redesign CAN `PduControl` because canonical Namespace no longer resides in Control.

## 18.7 `docs/deployment.md` (`DEPLOY`)

Replace NodeId assignment with ParticipantId assignment.

Preserve:

- static configuration optional;
- Organizer optional for basic communication;
- four-phase commissioning concept for shared media;
- commissioning below ordinary Wire semantics;
- dynamic setup installs ordinary bounded/read-mostly forwarding state rather than learned routing.

Update Organizer capabilities:

- assign ParticipantIds;
- assign/name Wires;
- configure Link-profile compact representations where needed;
- validate PID compatibility for CAN11 compact;
- reserve low global PIDs for CAN11-requiring Participants;
- validate splices/forwarding.

Both CAN11 shared and exclusive profiles may commission Participant IDs in-band through the reserved `GeneralPID=31 + GeneralToCompact` Link-control subspace.

Shared mode additionally requires an explicit WS/non-WS identifier allocation/discriminator rule agreed for that bus.

Do not infer membership/routes from observed traffic.

## 18.8 `docs/library_architecture.md` (`LIB`)

Update core types to §17.

Remove canonical NodeId and Direction from the core descriptor/API.

Add `ParticipantId`, `EndpointAddress`, and revised `WireNumber` types.

Make CAN11 Direction profile-local.

Update reply-context API description rather than replacing it with raw source metadata.

## 18.9 `docs/conformance.md` (`CONFORM`)

Retire vectors based on:

- old 5-byte descriptor;
- NodeId;
- canonical Direction;
- one-Origin Wire assumptions.

Add canonical descriptor vectors for:

- every QoS value;
- reserved-field rejection;
- extension-present bit;
- Wire values/boundaries once partition chosen;
- PID 0, PID 254, broadcast 255;
- broadcast invalid as source;
- all Namespace values;
- EID 0 invalid;
- EID 1 and EID 16383;
- EndpointAddress little-endian encoding.

Add semantic tests:

1. A and B may independently source traffic on one Wire.
2. B->A needs no canonical Direction/Origin state.
3. request-scoped reply context targets the validated ingress source without granting arbitrary learned authority.
4. forwarding preserves src/dest/Wire/Endpoint.
5. composition changes source naturally.
6. same PID is used across several Wires.
7. several Domains on one SoC have distinct PIDs.
8. Level-0 anonymous LocalBus works without Organizer/Manifest.
9. membership is not learned from observed traffic.
10. InternalDebugWire forwarding preserves the publishing Domain's PID.

Add CAN11 vectors after the compact profile framing is frozen, including:

- exclusive compact->general and general->compact;
- shared compact->general and general->compact;
- both-compact deterministic encoding, including `src == dest`;
- unrepresentable general->general pairs;
- exclusive broadcast sources `0..7`;
- shared broadcast sources `0..3`;
- shared ProtocolDiscriminator classification;
- reserved `GeneralPID=31 + GeneralToCompact` commissioning/control recognition;
- ordinary broadcast as `GeneralPID=31 + CompactToGeneral`;
- commissioning arbitration ordering;
- shared coexistence identifier-allocation vectors once exact bit positions are frozen.

## 18.10 `docs/implementation.md` (`IMPL`)

Update memory/capacity assumptions tied to:

- descriptor 5 -> 6 bytes;
- source metadata;
- 8-bit PID/Wire types;
- EndpointAddress 16-bit representation;
- 14-bit EndpointId validity;
- maximum header 9 bytes if existing extension format retained.

Measure actual RAM object sizes independently from encoded descriptor size; native alignment may make the one-byte wire increase negligible in some implementations.

## 18.11 `docs/future_work.md` (`FUTURE`)

Update future work so it does not reintroduce permanent Origin/Node semantics.

Record optional source-admission standardization as future/open, not required.

Keep richer CAN29 and possible future carrier-local projection profiles as escape paths if CAN11 direct-low-ID limits prove painful.

## 18.12 CAN commissioning working notes

Rewrite old NodeId/broadcast-direction assumptions around the common CAN11 commissioning subspace.

For **both** CAN11 shared and exclusive:

```text
GeneralParticipantId = 31
Direction             = GeneralToCompact
```

is Link-control/commissioning, not an ordinary canonical PDU.

Ordinary broadcast is:

```text
GeneralParticipantId = 31
Direction             = CompactToGeneral
```

Commissioning architecture to retain:

- no anonymous canonical PID is required;
- uncommissioned devices send no ordinary WS Service traffic;
- discovery uses stable physical/commissioning identity below Wire semantics;
- collision-safe identical anonymous responses plus prefix/binary search remain the preferred discovery direction;
- a deployment-level allocator chooses a globally unused PID and understands compact-range scarcity;
- assignment produces the canonical ParticipantId directly, not a permanent CAN alias;
- commissioning should arbitrate below all ordinary WS traffic where the final identifier ordering permits it.

Do not freeze the exact commissioning opcodes, identity width, persistence transaction, retry state machine, or commit protocol in this change request.

## 18.13 Synthesis/sketch findings

Update the synthesis/findings register so findings tied to deleted concepts are explicitly dispositioned rather than left dangling.

At minimum disposition findings covering:

- global identity benefit;
- one-PID-per-Domain expressive cost;
- permanent-Origin artificiality;
- operation/source-admission pressure;
- broad-Wire degraded-state complexity;
- forwarding vs composition;
- redundancy/failover layering.

Do not rewrite the historical sketch documents merely to make them use the new vocabulary; they are evidence records. Add supersession notes where needed.

## 18.14 `history.md`

Record:

- adoption date;
- semantic break;
- descriptor 40 -> 48 bits;
- Node/Origin retirement;
- global PID introduction;
- EndpointAddress reallocation;
- documents affected;
- that no interoperability promise existed, so no compatibility shim is required.

---

# 19. Proposed replacement invariants

Final IDs are assigned during `REG` editing. Existing invariant IDs whose meanings change are retired, never reused.

## Participant identity

**PARTICIPANT-A**

> Every Endpoint Domain has exactly one ParticipantId within a WireSpaces deployment identity scope and uses that same identity on every Wire it participates in.

**PARTICIPANT-B**

> ParticipantId identifies a deployment Endpoint Domain, not necessarily a physical device. Physical-device identity is separate.

**PARTICIPANT-C**

> `0xFF` is the canonical broadcast destination sentinel and is invalid as a source ParticipantId.

## Wire model

**WIRE-A**

> A Wire is a logical communication domain of Participants and has no permanent single-Origin role.

**WIRE-B**

> Multiple Participants may independently source traffic on one Wire.

**WIRE-C**

> Physical realization does not change Wire identity. A Wire may share, subset, or span Physical Links.

## PDU identity

**PDU-A**

> The canonical base descriptor directly carries WireNumber, source ParticipantId, destination ParticipantId/broadcast, and EndpointAddress.

**PDU-B**

> Direction is not canonical PDU semantics. A Link profile may use a Direction field only to reconstruct canonical source/destination or for another explicitly profile-local encoding purpose.

**PDU-C**

> EndpointAddress is a 16-bit value composed of Namespace[1:0] and EndpointId[13:0]; EndpointId zero is invalid in every Namespace.

**PDU-D**

> Reserved base-header bits are zero on transmit and cause rejection/counting when nonzero on receive until assigned a defined meaning.

## Forwarding

**ROUTE-A**

> Ordinary forwarding preserves Wire identity, source Participant, destination Participant/broadcast, EndpointAddress, class/transport metadata, applicable canonical extensions, and payload; only Link-local representation may change.

**ROUTE-B**

> Composition consumes one interaction/PDU and authors another; the composing Endpoint Domain is the source of the new PDU.

**ROUTE-C**

> Observed traffic does not create Wire membership or forwarding state.

## Binding / authority

**DISP-A**

> Reading received source metadata confers no transmit authority. Request-scoped and learned transmit contexts are bounded framework capabilities defined by Endpoint registration.

**DISP-B**

> Each Endpoint registration declares one binding mode; autonomous and request-scoped authority use separate registrations when both are needed.

**POLICY-A**

> Structural source identity does not by itself authenticate the sender or authorize an application operation.

## Scope

**SCOPE-A**

> `kLocalDomain` never enters a Link Interface and is never spliceable.

**SCOPE-B**

> Device-private WireNumbers do not leave the device unchanged; external exposure requires an explicit splice to a network-visible Wire.

**SCOPE-C**

> Anonymous `kLocalBus` traffic may be locally delivered but is not generically forwarded or spliced until canonical Wire identity exists.

## Configuration philosophy

**CONFIG-A**

> A static Manifest or Organizer is not required for base communication.

**CONFIG-B**

> All else being equal, prefer a simple invariant that eliminates configuration; mappings/projections remain justified when they materially increase capability or preserve a necessary identity-scope distinction.

---

# 20. Open/gating items

The proposal is comprehensive but does not pretend every profile detail is already solved.

## 20.1 8-bit WireNumber scope partition — gating for `BITS`/`CORE`

Before freezing the codec, choose the exact ranges for:

```text
network-visible Wires
device-private Wires
kLocalDomain
```

Preserve the existing scope model.

The new global Participant identity reduces pressure to spend private Wire numbers merely to distinguish internal Domains; that should be considered when shrinking the old device-private numeric range into 8 bits.

This is a numeric-allocation question, not a reason to revisit the 8-bit field width unless the resulting partition is demonstrably inadequate.

## 20.2 CAN11 Wire representation — gating for compact `LINK` profiles

Resolve §12.12 before calling either CAN11 compact profile complete.

This does not block host/sim/core semantic implementation.

## 20.3 CAN11 compact topology/broadcast limits — explicit profile review

Accept or revise the following as one coherent trade:

```text
Shared:
    compact PID range 0..3
    max fully pairwise-connected peer group ≈ 5
    true Wire-wide broadcast sources 0..3

Exclusive:
    compact PID range 0..7
    max fully pairwise-connected peer group ≈ 9
    true Wire-wide broadcast sources 0..7
```

These limits land directly on the peer-CAN archetype and must be accepted knowingly. CAN29 is the intended richer escape path.

## 20.4 Shared CAN11 discriminator placement / coexistence allocation

Freeze an exact shared identifier layout that:

- keeps QoS at the arbitration-significant end;
- gives the bus owner an auditable WS/non-WS identifier allocation;
- retains the common compact/general/Direction semantics;
- preserves the reserved commissioning/control subspace;
- does not accidentally make commissioning outrank ordinary Background traffic.

Shared and exclusive may use different exact bit positions even though their addressing semantics are the same.

## 20.5 CAN11 commissioning message protocol

The reserved subspace and basic discovery/allocation architecture are accepted direction.

Still open before production use:

- commissioning identity width/format;
- exact discovery/query/response opcodes;
- selection token/session details;
- assignment persistence/commit behavior;
- retries/timeouts/fault recovery;
- interaction with replacement/service tooling.

These do not block canonical PDU implementation.

## 20.6 Header-extension forwarding semantics

Resolve before the first extension with behavior-critical meaning is standardized.

## 20.7 Optional source admission

Keep open. Do not block core implementation.

## 20.8 Degraded/reachability reporting

Define tooling/telemetry expectations as implementations mature. Do not add base-header fields now.

---

# 21. Implementation sequence

## Step 1 — accept semantic model and base field allocation

Review/approve:

- global ParticipantId;
- one PID per Endpoint Domain;
- multi-initiator Wire;
- canonical src/dest;
- removal of canonical Direction/Node/Origin;
- 48-bit descriptor shape;
- 8/8/8/{2|14} allocation;
- `EndpointId == 0` invalid;
- `0xFF` broadcast destination;
- binding-mode migration;
- preservation of internal/private Wire semantics and `kLocalBus`.

## Step 2 — update `REG`, `CORE`, `INTRO`, `DEPLOY`

Make the semantic model internally consistent before profile implementation.

Do not wait for CAN11 Wire multiplexing to update the canonical architecture.

## Step 3 — resolve 8-bit WireNumber partition

Choose network-visible/device-private/`kLocalDomain` numeric ranges.

Then freeze `BITS` base descriptor and golden vectors.

## Step 4 — implement host/sim canonical codec and Router slice

Implement the new descriptor directly; do not add NodeId/Direction compatibility APIs.

Prove:

- encode/decode;
- local dispatch;
- src/dest routing;
- request-scoped reply context;
- forwarding vs composition;
- LocalBus behavior.

## Step 5 — update conformance and library architecture

Add semantic and byte-exact tests.

## Step 6 — freeze CAN11 shared/exclusive profile details

Run focused `LINK` review of:

- Wire multiplexing / Wire identity;
- shared vs exclusive exact bit ordering;
- shared ProtocolDiscriminator coexistence allocation;
- compact PID constraints;
- peer-mesh and broadcast-source limits;
- reserved commissioning/control subspace;
- commissioning arbitration priority;
- redesigned CAN `PduControl`;
- N=1/PDUA capacity.

## Step 7 — implement CAN11 shared/exclusive and CAN29 richer profiles

Implement one common asymmetric CAN11 address codec parameterized by compact-field width and shared/exclusive framing.

CAN29 should be treated as the normal escape path for deployments that exceed the intentional CAN11 compact limits or require richer same-bus Wire multiplexing.

## Step 8 — revisit optional policy/tooling

Only after core/profiles produce implementation evidence, revisit:

- source admission standardization;
- degraded-state tooling;
- richer mappings/projections if CAN11 low-ID limits prove painful.

---

# 22. Review checklist

## Semantic core

- [ ] Is one ParticipantId per Endpoint Domain accepted?
- [ ] Is physical-device identity clearly separate?
- [ ] Can multiple Participants independently source traffic on one Wire?
- [ ] Does a conventional one-controller system remain simple?
- [ ] Is canonical src/dest sufficient for routing?
- [ ] Is canonical Direction removed without losing binding/API properties?
- [ ] Are all five binding modes explicitly migrated?
- [ ] Is source metadata still non-authorizing?
- [ ] Are forwarding and composition distinct?

## Descriptor

- [ ] Is 48 bits / 6 bytes accepted?
- [ ] Is Control = QoS[2] + Reserved[2] + H[1] + Transport[3] accepted?
- [ ] Is WireNumber 8 bits accepted?
- [ ] Are source and destination PIDs 8 bits accepted?
- [ ] Is `0xFF` broadcast destination accepted?
- [ ] Is EndpointAddress = Namespace[2] + EndpointId[14] accepted?
- [ ] Is EndpointId zero still invalid?
- [ ] Is the Namespace-3 registry rationale accepted?
- [ ] Is the 9-byte max header consequence accepted if 3-byte extensions remain?

## Wire scope / bring-up

- [ ] Are named network, device-private, `kLocalDomain`, and `kLocalBus` still distinct?
- [ ] Is exact 8-bit Wire partition tracked as gating?
- [ ] Does Level-0 still work without Organizer/Manifest?
- [ ] Is anonymous LocalBus still non-forwardable/non-spliceable until named?
- [ ] Is membership never inferred merely from traffic?
- [ ] Is existing `InternalDebugWire` preserved rather than reinvented?

## Configuration / policy

- [ ] Is mandatory remote source authorization absent?
- [ ] Is optional permitted/accepted-source vocabulary recorded rather than suppressed?
- [ ] Does the configuration-elimination principle include the capability-equivalence proviso?

## CAN11

- [ ] Is the deployment-wide low-PID restriction explicit and intentional?
- [ ] Do shared and exclusive preserve the same asymmetric addressing semantics?
- [ ] Is QoS consistently 2 bits in both profiles?
- [ ] Is exclusive compact PID `0..7` accepted?
- [ ] Is shared compact PID `0..3` accepted?
- [ ] Is general PID `0..30` plus Link broadcast code `31` accepted?
- [ ] Is Direction clearly profile-local source/destination reconstruction only?
- [ ] Is both-compact encoding deterministic, including `src == dest`?
- [ ] Are the shared five-peer / four-broadcast-source limits explicit?
- [ ] Are the exclusive nine-peer / eight-broadcast-source limits explicit?
- [ ] Is shared ProtocolDiscriminator placement tracked separately from addressing semantics?
- [ ] Is `GeneralPID=31 + GeneralToCompact` reserved for commissioning/control in both profiles?
- [ ] Is ordinary broadcast `GeneralPID=31 + CompactToGeneral`?
- [ ] Is the basic anonymous discovery -> global PID allocation -> assignment flow accepted?
- [ ] Is no anonymous canonical PID required?
- [ ] Is Wire multiplexing marked unresolved/gating?
- [ ] Is CAN `PduControl` explicitly slated for redesign?
- [ ] Is N=1/PDUA capacity explicitly revalidated?
- [ ] Is CAN29 named as the normal richer escape path?

## Repository consistency

- [ ] `REG` retires rather than silently mutates old invariant IDs.
- [ ] No current normative one-Origin-per-Wire claim remains.
- [ ] No canonical NodeId remains.
- [ ] No canonical Direction remains.
- [ ] `CORE` binding modes no longer depend on Direction/NodeId.
- [ ] `kLocalBus`, `kLocalDomain`, device-private scope, and InternalDebugWire still exist coherently.
- [ ] `BITS`, `LIB`, `DEPLOY`, `CONFORM`, and `LINK` agree with the new model.
- [ ] Sketch/synthesis findings tied to superseded concepts are dispositioned.
- [ ] `history.md` records the break.

---

# 23. Migration search terms

Search non-archive normative documents for at least:

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
RoutingWord
Direction
WireAlias
5 bytes
40 bits
ParticipantId retired
relocate its Origin
unique NodeIds
NodeId collision
```

Do **not** blindly require zero matches for terms that remain valid in narrower contexts:

```text
Direction       valid inside CAN/profile-specific material
WireAlias       may remain where a Link profile still uses it
Namespace       still current
EndpointId      still current
InternalDebugWire current
```

Exclude archive/history/sketch evidence from mechanical “must disappear” checks; those documents may intentionally describe the superseded model.

---

# 24. Acceptance effect

If this change request is accepted:

1. Every Endpoint Domain receives one deployment-global 8-bit ParticipantId.
2. Internal Endpoint Domains on multicore SoCs are ordinary Participants.
3. Formal canonical Node/NodeId roles are retired.
4. Wires no longer have one permanent Origin.
5. Multiple Participants may independently source traffic on one Wire.
6. Canonical routing becomes source/destination.
7. Direction is removed from canonical semantics and retained only where a Link profile needs it.
8. The canonical base descriptor becomes 48 bits / 6 bytes.
9. The base descriptor is byte-oriented: Control, Wire, Src, Dest, EndpointAddress.
10. Control becomes `QoS[2] + Reserved[2] + HasHeaderExtensions[1] + TransportType[3]`.
11. WireNumber becomes 8 bits; its scope partition is re-derived while preserving network/private/local semantics.
12. SrcParticipantId and DestParticipantId become 8 bits.
13. `0xFF` is the proposed canonical broadcast destination and is invalid as source.
14. EndpointAddress remains a `uint16_t` containing Namespace[2] + EndpointId[14].
15. EndpointId zero remains invalid; each Namespace has 16,383 valid IDs.
16. Namespace 3 retains a large long-lived FOSS/public registry space.
17. The current self-describing 1–3-byte header-extension direction is retained provisionally; maximum canonical header becomes 9 bytes if unchanged.
18. `kLocalBus`, device-private Wires, `kLocalDomain`, and the existing InternalDebugWire pattern remain part of the architecture.
19. Request-scoped and learned-from-ingress bindings survive through bounded API contexts rather than canonical Direction.
20. Static configuration and remote-source authorization remain optional.
21. The design principle prefers invariants over configuration only when capability/cost are comparable.
22. The initial CAN11 compact profiles deliberately use low global PIDs and accept a deployment-wide ~31-participant direct-visibility limit.
23. CAN11 shared and exclusive use one asymmetric compact/general/Direction addressing model and retain 2-bit QoS in both.
24. CAN11 exclusive uses CompactPID[3] (`0..7`) + GeneralPID[5] (`0..30`) + Direction; shared spends one bit on a ProtocolDiscriminator and reduces CompactPID to 2 bits (`0..3`).
25. Pairwise peer topology and broadcast-source limits are explicit profile costs: approximately nine/eight for exclusive and five/four for shared respectively.
26. `GeneralPID=31 + CompactToGeneral` is ordinary Link-level broadcast; `GeneralPID=31 + GeneralToCompact` is reserved Link-control/commissioning space in both profiles.
27. CAN11 commissioning is below Wire semantics: anonymous devices are discovered by stable commissioning identity, a deployment-level allocator selects a globally valid/representable ParticipantId, and successful assignment enables ordinary WS traffic.
28. No anonymous canonical ParticipantId or permanent CAN Participant alias table is required by the initial commissioning direction.
29. CAN11 multi-Wire representation and shared discriminator/coexistence placement remain gating `LINK` design items and are not silently collapsed.
30. CAN `PduControl` and optimized N=1/PDUA packing must be redesigned/revalidated for Namespace-in-EndpointAddress.
31. CAN29 and richer Links are the intended escape path when CAN11 compact limits are unsuitable.
32. No backward-compatibility NodeId/Origin shim is required because interoperability is not frozen and the prototype has not implemented the canonical PDU stack.

The architectural thesis is:

> **Wire topology, participant identity, message source/destination, Endpoint identity, and application authority are separate concerns. WireSpaces should encode the first four simply and directly, while allowing products to add the fifth only where they need it. The canonical format should spend durable address space on durable ecosystem identity, and constrained Link profiles should be allowed to make explicit, well-documented capability tradeoffs rather than distorting the whole protocol around their smallest carrier.**

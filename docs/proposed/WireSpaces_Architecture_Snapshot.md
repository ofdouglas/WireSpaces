# WireSpaces Architecture Snapshot

**Status:** Pre-prototype architecture snapshot for independent review  
**Purpose:** Describe WireSpaces as a standalone embedded networking architecture at its current design point  
**Scope:** Core model, canonical addressing, routing, Endpoint semantics, representative platform facilities, internal Wires, and current CAN profile direction

WireSpaces (WS) is a small embedded networking and IPC architecture intended to use the same logical model across heterogeneous carriers and across Endpoint Domains that may live either on separate devices or inside one multicore SoC.

The architecture is aimed at embedded machines with hierarchical or distributed control, small and moderate MCUs, gateways, Linux hosts, and selected FPGA/RTL implementations. It is intended to support simple bare-metal systems without requiring a deployment tool, while allowing more structured routing and commissioning in larger systems.

---

# 1. Core model

## 1.1 Endpoint Domain / Participant

The independently routed and dispatched software unit is an **Endpoint Domain**. On the network, an Endpoint Domain is a **Participant**.

Each Participant has one deployment-scoped `ParticipantId` and uses the same identity on every Wire it joins.

```text
Physical device / SoC
    may contain 1..N Endpoint Domains

Endpoint Domain
    exactly one ParticipantId
```

A multicore SoC may therefore appear as several Participants:

```text
Central SoC
    Application Domain      PID 12
    Safety Domain           PID 13
    Networking Domain       PID 14
    Diagnostics Domain      PID 15
```

Participant identity is separate from physical-device identity. A device UUID, serial number, hardware revision, or inventory identity may group several Participants or change when hardware is replaced without changing a deployment role.

Canonical `ParticipantId` is 8 bits. `0xFF` is reserved as the broadcast destination; ordinary Participants use `0x00..0xFE`.

Participant IDs are unique within one WireSpace/deployment identity universe. Plain forwarding does not merge independently assigned identity universes; interconnecting separately engineered WireSpaces requires coordinated identity assignment or an explicit translation/composition boundary.

## 1.2 Wire

A **Wire** is a logical communication domain among Participants. It is not synonymous with a cable, CAN bus, Ethernet LAN, process boundary, or physical device.

A Wire may:

- occupy one Physical Link;
- share a Physical Link with other Wires;
- select only some Participants attached to a shared medium;
- span several heterogeneous Links through gateway forwarding;
- exist entirely inside one device.

A Wire has no permanent controller or Origin role. Any Participant may source traffic when the application/service model calls for it.

A conventional controller-plus-leaves system therefore remains conventional, while peer and distributed systems do not need to invent a protocol master merely to fit the Wire abstraction.

`WireNumber` is 8 bits in the canonical descriptor. The Wire-number space includes network-visible and device-private uses plus a local-domain sentinel; the exact numeric partition is still being finalized.

## 1.3 Source and destination

Canonical routing explicitly carries:

```text
SrcParticipantId
DestParticipantId
```

There is no canonical Direction field and no permanent Origin/Node relation.

```text
A -> B:
    src  = A
    dest = B

B -> A:
    src  = B
    dest = A

A -> broadcast:
    src  = A
    dest = 0xFF
```

Request/reply state, transaction identity, freshness, terms, epochs, and similar interaction semantics belong to Services or Transports rather than to the basic routing relation.

Broadcast is scoped by the Wire. A broadcast does not imply that every device electrically hearing a frame is a semantic member or implements the addressed Endpoint.

---

# 2. Canonical PDU descriptor

The ordinary canonical descriptor is **48 bits / 6 bytes**.

| Field | Bits |
|---|---:|
| QoS | 2 |
| Namespace | 2 |
| HasHeaderExtensions | 1 |
| TransportType | 3 |
| WireNumber | 8 |
| SrcParticipantId | 8 |
| DestParticipantId | 8 |
| EndpointId | 16 |
| **Total** | **48** |

The intended serialized shape is byte-oriented:

```text
Byte 0      Control
Byte 1      WireNumber
Byte 2      SrcParticipantId
Byte 3      DestParticipantId
Bytes 4-5   EndpointId, little-endian
```

with:

```text
Control
    QoS                    2
    Namespace              2
    HasHeaderExtensions    1
    TransportType          3
```

Canonical multi-byte values use explicit serialization; compiler bitfields/native struct layout are not wire formats.

## 2.1 QoS

WS defines four QoS classes. Link profiles may map them onto their native priority mechanism, but the four-class meaning is intended to remain consistent across ordinary profiles, including CAN.

## 2.2 Namespace and EndpointId

Endpoint dispatch uses the pair:

```text
(Namespace, EndpointId)
```

There are four Namespaces. The intended allocation model is:

```text
Namespaces 0..2    user/deployment/application use
Namespace 3        public WireSpaces/FOSS ecosystem
```

`EndpointId` is 16 bits. Endpoint ID zero is reserved/invalid; other allocation rules are namespace-specific.

Namespace 3 is intended for long-lived public service/interface assignments, so Endpoint IDs there are treated as durable ecosystem identifiers rather than deployment-local routing numbers.

## 2.3 Header extensions

`HasHeaderExtensions` indicates bounded metadata beyond the base descriptor.

The current extension direction is a short self-describing 1–3-byte sequence. Exact extension meanings are intentionally allocated only when a concrete feature needs them; the base address fields do not depend on extensions for ordinary system scale.

---

# 3. Endpoints, Services, and Transports

A Participant owns Endpoints. An Endpoint is identified by `(Namespace, EndpointId)` and has local receive/transmit behavior and storage semantics.

Typical Service families include both application-specific services and platform services such as:

- identity and version inventory;
- health / heartbeat;
- bootloader / firmware update;
- OS/runtime telemetry;
- Link Entity telemetry;
- text logs;
- structured events and fault history;
- time synchronization;
- machine-specific command, state, and telemetry services.

The default Transport is an unreliable datagram. Additional Transport types may provide reliable segmented transfer or other bounded delivery semantics where needed.

WS does not require every Service to be globally standardized. Namespaces provide room for deployment-private interfaces as well as common ecosystem services.

---

# 4. Local transmit bindings

Transmit capability is represented by Endpoint registration/binding rather than inferred from received source metadata.

The architecture supports five useful binding modes:

| Binding mode | Meaning |
|---|---|
| **Static** | Destination/Wire context comes from fixed or generated local configuration. |
| **Transmit-only** | Autonomous publication/command path; no receive-derived context. |
| **Receive-only** | No transmit capability. |
| **Request-scoped** | A validated ingress creates a bounded opaque reply context targeting that ingress source. |
| **Learned-from-ingress** | A bounded set of validated peer selections may be retained according to the Link/deployment rules. |

A request-scoped reply conceptually uses:

```text
reply.src  = local ParticipantId
reply.dest = received.src
reply.wire = received Wire
```

plus registration-defined Endpoint/Transport constraints and any Service-level correlation state.

Reading `SrcParticipantId` as metadata does not by itself grant transmit authority.

Remote-source authentication and application authorization are separate optional concerns; they are not mandatory configuration in the base routing model.

---

# 5. Forwarding, splicing, and composition

WS distinguishes three common gateway behaviors.

## 5.1 Ordinary forwarding

A gateway carries the same canonical interaction across Link Interfaces.

Ordinary forwarding preserves:

```text
Wire identity
SrcParticipantId
DestParticipantId
Namespace / EndpointId
QoS
TransportType
applicable header extensions
payload
```

Only the carrier-specific representation changes.

## 5.2 Splice

A **splice** is the sanctioned boundary for exposing a device-private Wire onto a different Wire scope. It preserves message/source lineage while applying the configured Wire projection required by the splice.

A device-private Wire number is not allowed to escape the device unchanged.

## 5.3 Composition

Application code may terminate one interaction and author another.

```text
Plant -> Gateway
    src = Plant

Gateway consumes request

Gateway -> Actuator
    src = Gateway
```

This is composition, not forwarding. The newly authored PDU uses the composing Participant as source and may use another Wire and Endpoint.

Cross-Wire application relay is therefore normally composition unless an explicit splice defines the scope projection.

---

# 6. Internal/device-private communication

Internal communication uses the same Participant and Wire concepts as off-device networking.

A device may contain several Participant Domains and several device-private Wires. Internal Wire count follows useful communication/failure structure rather than CPU-core count or physical interconnect hops.

## 6.1 InternalDebugWire

`InternalDebugWire` is the conventional device-private path for platform/debug publication.

Platform integration can bind standard per-Domain publishers to it, for example:

```text
TextLog
Event
Health
OS/runtime telemetry
fault/crash records
Link Entity status
version/build information
```

Each Domain publishes under its own ParticipantId. A configured splice can expose this traffic toward a network logging/diagnostic sink while preserving the publishing Domain's identity.

The exact service set and reserved private Wire number are platform/deployment details rather than core routing semantics.

## 6.2 kLocalBus and local bring-up

`kLocalBus` remains the low-configuration/native-Link bring-up mechanism. It denotes the native physical communication context of a Link when a separately named Wire is not needed yet.

This supports simple bring-up without an Organizer or a global deployment manifest. Anonymous/local traffic is not silently promoted into generic cross-Link routing; named/cross-Link behavior is established when the deployment needs it.

`kLocalDomain` remains local-only and does not enter a Link Interface.

---

# 7. Configuration and deployment philosophy

WS supports static/generated configuration, commissioning, and richer deployment tooling, but basic communication is not required to depend on them.

A design preference is:

> **When two designs provide comparable capability and cost, prefer a simple protocol invariant that eliminates configuration over a configurable mapping that expresses the same thing.**

This is not a ban on configuration. Mappings are justified when they materially increase representable capability or bridge genuinely different identity scopes.

Wire membership and forwarding are not inferred merely by observing traffic. They may instead come from the Link/profile topology, compile-time definitions, generated deployment data, commissioning, or explicit runtime setup.

An optional Organizer can assign identities, build routing/forwarding projections, validate topology, generate static tables, and assist commissioning without becoming a prerequisite for small systems.

---

# 8. CAN profiles

CAN is treated as a family of Link profiles rather than as the canonical protocol format itself.

## 8.1 Classical CAN 11-bit

CAN11 is intentionally a constrained compatibility profile. It uses asymmetric participant compression plus a Link-local Direction bit to reconstruct canonical source/destination.

### WS-exclusive CAN11

```text
QoS                    2
CompactParticipantId   3
GeneralParticipantId   5
Direction               1
-------------------------
                       11
```

Directly representable ranges:

```text
Compact PID     0..7
General PID     0..30
General value 31 = Link-level broadcast code
```

Every addressed pair must include at least one Participant in `0..7`.

### Shared/coexistence CAN11

```text
QoS                    2
ProtocolDiscriminator  1
CompactParticipantId   2
GeneralParticipantId   5
Direction               1
-------------------------
                       11
```

Directly representable compact range:

```text
Compact PID     0..3
General PID     0..30
General value 31 = Link-level broadcast code
```

Every addressed pair must include at least one Participant in `0..3`.

The shared/exclusive profiles intentionally keep the same addressing style and all four QoS classes. Shared operation pays one compact-PID bit for coexistence with non-WS 11-bit traffic.

The low PID ranges are deployment-global canonical Participant IDs, not per-bus aliases. CAN11 therefore has a deliberately scarce deployment-wide compatibility range. Systems that outgrow it are expected to use CAN29 or another richer Link profile rather than requiring CAN11 to represent every WS topology.

`Direction` exists only inside the CAN11 encoding:

```text
CompactToGeneral
GeneralToCompact
```

It has no canonical request/reply or authority meaning.

### CAN11 broadcast and commissioning

Ordinary CAN11 broadcast uses:

```text
GeneralParticipantId = 31
Direction             = CompactToGeneral
```

so the broadcast source must fit the compact PID field.

The otherwise-invalid ordinary combination:

```text
GeneralParticipantId = 31
Direction             = GeneralToCompact
```

is reserved for CAN11 Link-control / commissioning traffic in both shared and exclusive profiles.

Basic commissioning flow:

1. uncommissioned devices do not emit ordinary WS Service traffic;
2. the commissioner discovers anonymous devices using a stable commissioning/device identity;
3. collision-safe identical responses and prefix/binary search are the current discovery direction;
4. a deployment allocator chooses a globally unused ParticipantId compatible with the CAN11 profile and its scarce compact range;
5. the selected device commits that canonical ParticipantId and can then use ordinary WS traffic.

The exact commissioning opcodes, identity width, persistence transaction, retries, and failure recovery are not yet frozen.

The exact representation of multiple logical Wires on one physical CAN11 segment is also still profile-level work. CAN11 may deliberately accept stronger limitations here than richer profiles.

## 8.2 CAN 29-bit

CAN29 is intended to be a substantially less constrained, more first-class CAN profile.

The current direction places the routing information needed for arbitration/forwarding directly in the 29-bit identifier:

```text
QoS                    2
HasHeaderExtensions    1
ProtocolDiscriminator  1
WireNumber             8
SrcParticipantId       8
DestParticipantId      8
Reserved / profile bit 1
-------------------------
                       29
```

Service/dispatch metadata remains in the CAN data field, including `TransportType`, `Namespace`, and `EndpointId`. A straightforward packing consumes about three Classical-CAN data bytes before application payload, so payload efficiency remains an explicit CAN29 profile trade.

This gives CAN29 direct 8-bit Wire/source/destination representation with no Direction field and no low-Participant-ID restriction. The exact in-frame compact packing and use of the remaining identifier/profile bit are still being evaluated, particularly for Classical CAN's 8-byte payload budget.

CAN29 is the expected escape path when CAN11's compact participant or Wire restrictions are inappropriate.

---

# 9. Failure and redundancy model

WireSpaces does not equate routing identity with runtime authority or redundancy state.

Several controllers may legitimately be able to source traffic on one Wire while application state decides which commands are currently accepted.

For example, a redundant plant-control pair may keep:

```text
Participant identities      unchanged
Wire membership             unchanged
routing                     unchanged
```

while a Service-level term/epoch determines which controller's command is currently accepted.

Leader election, term allocation, split-brain prevention, physical redundancy, and duplicate suppression are not provided implicitly by the base Wire abstraction.

Likewise, a Wire may remain configured while a gateway/Link failure partitions its current reachability. Diagnostics therefore may need Participant/Link/path-level health rather than treating a Wire as only one boolean up/down object.

---

# 10. Representative system shapes

The same model is intended to cover several classes without changing the canonical routing concepts.

## Conventional controller bus

```text
Controller
   |
   +---- sensors / actuators
```

The controller naturally sources most commands; leaves publish or reply as their Services require.

## Distributed peer subsystem

```text
A ----- B ----- C ----- D
```

Several Participants may independently source addressed traffic on the same Wire. No artificial permanent master is required.

## Hierarchical gateway machine

```text
Main compute
     |
  Ethernet
     |
  Gateway
  /  |  \
CAN RS485 CAN
```

A logical Wire may span several Links through ordinary forwarding. Application transformations at the gateway are composition rather than forwarding.

## Multicore SoC

```text
SoC
  PID 20 Application
  PID 21 Safety
  PID 22 Network
  PID 23 Diagnostics
```

Internal and external communication use the same Participant identity model. Device-private Wires can carry platform/debug traffic and be selectively spliced outward.

---

# 11. Deliberately open design areas

The following are not assumed to be fully specified by this snapshot:

- exact 8-bit `WireNumber` allocation between network-visible, device-private, and local sentinel ranges;
- exact CAN11 representation of multiple Wires on one physical bus;
- exact CAN11 bit ordering needed to satisfy coexistence and commissioning arbitration requirements;
- full CAN11 commissioning message/state-machine details;
- exact CAN29 in-frame service/Endpoint packing and payload-efficiency optimizations;
- concrete header-extension assignments;
- optional source/authentication/authorization mechanisms;
- detailed degraded-reachability tooling and telemetry;
- final service registry allocation policy.

These are intended to refine the architecture rather than silently change the core meanings of Participant, Wire, source/destination, Endpoint, forwarding, splice, and composition.

---

# 12. Compact architecture summary

```text
Physical device
    1..N Endpoint Domains / Participants

Participant
    deployment-global 8-bit ParticipantId
    same identity across every Wire

Wire
    logical communication domain
    8-bit WireNumber
    no permanent Origin/master role

Canonical PDU
    QoS                 2
    Namespace           2
    HeaderExtensions    1
    TransportType       3
    WireNumber          8
    Source PID          8
    Destination PID     8
    EndpointId         16
    ----------------------
    48 bits / 6 bytes

Routing
    explicit source + destination
    broadcast destination = 0xFF

Endpoint
    (Namespace, EndpointId)
    local binding/storage semantics

Gateway behavior
    forwarding  = same canonical interaction
    splice      = explicit Wire-scope projection
    composition = consume + author a new interaction

Configuration
    optional where topology/profile invariants suffice
    static/generated/commissioned where useful

CAN11
    constrained asymmetric compact/general PID profile
    shared and exclusive variants
    Link-local Direction only
    in-band commissioning via reserved invalid ordinary subspace

CAN29
    full 8-bit Wire/src/dest representation
    intended richer CAN profile
```

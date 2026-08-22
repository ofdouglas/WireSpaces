# WireSpaces Change Statement — Simplified Wire Model, CAN Wire Numbering, and Lab Auto-Wiring

## Status

Provisional design change.

This document captures a simplification of the WireSpaces topology and routing model intended to make the framework useful from first experiments through advanced statically engineered deployments.

The guiding principle is:

> WireSpaces should be easy to use before a user is ready to fully model, constrain, and statically analyze an entire system.

Strong static configuration and analysis remain important capabilities, but they are not prerequisites for basic communication, prototyping, demos, development benches, or small projects.

---

## 1. Simplified Wire Model

A `Wire` is a logical broadcast bus containing:

- Exactly one distinguished `Main` participant. A better final term may be chosen later.
- Zero or more `Peer` participants.
- A `WireNumber`.
- One or more physical Link segments that realize the logical Wire.

For the constrained Classical CAN profile described here, a Wire may contain up to 31 peers.

A Wire is fundamentally a connectivity and broadcast-domain abstraction. It is no longer defined as a single directed producer-to-sink relationship.

### 1.1 Communication semantics

A Wire supports two structural traffic directions:

- `MainToPeer`
- `PeerToMain`

For `MainToPeer` traffic:

- `PeerId == 0` means broadcast from Main to all peers on the Wire.
- `PeerId != 0` selects one peer.

For `PeerToMain` traffic:

- `PeerId` identifies the transmitting peer.
- Main necessarily receives the traffic.
- Other peers may also observe and consume the traffic when the underlying Link and receive filtering permit it.

This allows simple peer-to-peer interaction on a shared bus without requiring a separate routed relationship for every peer pair. The Main remains an observer of such traffic.

Endpoint IDs and Service/message semantics determine the purpose of the received traffic.

### 1.2 Main is a per-Wire role

`Main` is not necessarily an intrinsic device type. A device may be Main on one Wire and a Peer on another.

The role provides useful structure for:

- Broadcast source uniqueness.
- Peer addressing.
- Network organization.
- Discovery and lab auto-configuration.
- Tree-oriented gateway topologies.

The final terminology for this role remains open.

---

## 2. Physical and Virtual Wires

A physical bus is itself a Wire.

A `Virtual Wire` is a Wire whose logical broadcast domain differs from exactly one complete physical bus. A Virtual Wire may be formed by:

- Taking the union of multiple physical buses.
- Selecting a subset of participants from one physical bus.
- Selecting subsets from multiple physical buses and joining them through gateways.

Therefore:

- Every physical bus has a physical Wire identity.
- Some logical Wires may span several physical Link segments.
- Some physical participants may be excluded from a Virtual Wire even though they share one constituent physical bus.

All Wires have source/configuration metadata such as:

- Stable UUID.
- Human-readable name.
- `WireNumber`.

The UUID/name provide continuity in tooling and logs even if the short WireNumber changes.

---

## 3. WireNumber

`WireNumber` is the canonical logical identity used by routing and higher layers.

The full WireNumber namespace is not limited by the Classical CAN 11-bit representation.

A system may therefore contain many physical buses and many Wires even though the committed 11-bit CAN profile can directly encode only a small number of non-local WireNumbers.

### 3.1 `kLocalBus`

`kLocalBus == 0` is a Link-relative compression code used by constrained Link profiles.

It does **not** mean that every physical bus has canonical WireNumber zero.

For a Classical CAN Link:

#### Transmit

If the canonical WireNumber being transmitted is the WireNumber of this physical CAN bus:

```text
canonical WireNumber = this CAN Link's physical WireNumber
        ->
encoded WireNumber = kLocalBus
```

If the message belongs to a directly representable non-local Wire:

```text
canonical WireNumber in [1, 7]
        ->
encoded WireNumber = canonical WireNumber
```

#### Receive

```text
encoded WireNumber == kLocalBus
        ->
Link Engine restores this physical Link's canonical WireNumber

encoded WireNumber in [1, 7]
        ->
leave WireNumber unchanged
```

Example:

```text
CAN_A physical WireNumber = 107

RX encoded WN = 0
    -> canonical WN = 107

RX encoded WN = 5
    -> canonical WN = 5
```

This allows physical CAN buses to have arbitrary canonical WireNumbers while preserving a compact local representation on the wire.

---

## 4. Committed Classical CAN 11-Bit Projection

The proposed 11-bit CAN identifier allocation is:

```text
QoS         2
Direction   1
WireNumber  3
PeerId      5
----------------
            11
```

Exact physical bit ordering within the CAN identifier may be frozen separately.

### 4.1 PeerId

The 5-bit `PeerId` allows:

```text
0       broadcast / no individual peer
1..31   individual peers
```

For small systems, PeerIds may be globally stable across Wires:

```text
Wire X / Peer 7
Wire Y / Peer 7
```

may refer to the same device.

This is a convenience, not a requirement of the general WireSpaces model.

### 4.2 Named-Wire limit

Only three CAN-ID bits are available for WireNumber, so committed 11-bit CAN can directly represent:

```text
0       kLocalBus
1..7    explicitly named non-local Wires
```

This is an intentional constrained-profile limitation.

It does **not** limit:

- The total number of physical CAN buses in a system.
- The full canonical WireNumber namespace.
- The number of local physical Wires.

Each physical CAN bus can use `kLocalBus`, with its Link Engine restoring its actual canonical WireNumber.

The scarce values `1..7` are used when a Wire identity must remain explicit while traffic crosses a CAN segment.

### 4.3 Future 29-bit CAN profile

A future 29-bit CAN profile may allocate enough identifier space to carry a much larger or full WireNumber directly.

That profile can remove the seven-named-Wires limitation while retaining the same canonical Wire semantics.

The 11-bit profile remains valuable as the compact compatibility floor.

No 29-bit layout is specified by this change.

---

## 5. Structural Validity Versus WireContract

A `WireContract` is **not required** to create or use a Wire.

There is no implicit generated WireContract.

This is deliberate.

A beginner should be able to:

- Put devices on a Wire.
- Send and receive Service messages.
- Discover devices.
- Run demos.
- Build small systems.

without first defining application authority, traffic budgets, redundancy rules, or a complete statically analyzable communication architecture.

### 5.1 Basic structural rules

The implementation still enforces rules necessary for the representation itself to function, such as:

- One Main role per Wire.
- Valid PeerId values.
- No duplicate PeerId assignment where uniqueness is required.
- Valid WireNumber representation for the selected Link profile.
- Valid forwarding configuration.

These are structural protocol/configuration constraints, not a WireContract.

### 5.2 Optional WireContract

Advanced deployments may add a `WireContract` describing allowed behavior such as:

- Which Services or Endpoints may transmit.
- Which participants may consume or invoke them.
- Rate limits.
- Message-size limits.
- QoS expectations.
- Version compatibility.
- Resource bounds.
- Redundancy requirements.
- Runtime enforcement rules.

A WireContract can enable stronger compile-time and runtime validation.

It remains optional.

---

## 6. LocalBus-Only Devices

A valid WireSpaces device does not necessarily need to understand arbitrary Wire assignment or multihop routing.

A simple device may be `LocalBusOnly`.

Such a device:

- Communicates only on its directly attached physical Wire.
- Can use the `kLocalBus` representation.
- May implement only a small subset of WireSpaces.
- Does not need gatewaying capability.

This is particularly useful for tiny MCUs, fixed-function devices, RTL-native nodes, private peripheral buses, and simple dev-kit demonstrations.

Where useful, even an N=1-only CAN node may support changing its PeerId and/or assigned Wire identity without supporting gateway behavior.

---

## 7. Optional Lab Discovery and Auto-Wiring

WireSpaces should support configuration-light use in labs, benches, demos, prototypes, and evaluation environments.

This is implemented using optional Services and host-side tooling.

It is not intrinsic dynamic-routing behavior in the mandatory WireSpaces runtime.

### 7.1 Architecture

A developer PC runs a WireSpaces network organizer/toolkit.

The host acts as the temporary configuration authority.

Embedded nodes optionally expose Services that allow the host to:

- Enumerate devices on a local physical Wire.
- Read stable device identity.
- Enumerate gateways.
- Query gateway interfaces.
- Assign WireNumbers.
- Assign or update PeerIds where supported.
- Inspect observed participants.
- Update ephemeral gateway forwarding configuration.

The mapping algorithm runs on the host, not as a distributed embedded routing protocol.

This keeps embedded implementations lean and deterministic while allowing the PC tooling to use richer algorithms and user interaction.

---

## 8. Device Identity During Discovery

Nodes should preferably expose a globally unique stable identity, such as a 128-bit UUID.

The UUID is distinct from:

- WireNumber.
- PeerId.
- Current topology.
- Current route assignment.

Tooling can therefore record:

```text
Wire 107 / Peer 2
    UUID = 0fe0120f-aa00-01ee-...
```

If the topology is later changed:

```text
Wire 5 / Peer 9
    UUID = 0fe0120f-aa00-01ee-...
```

logs and historical data can still identify the same participant.

---

## 9. Local Enumeration

For each physical Wire directly available to Main, the host can operate using the local-bus representation.

Possible operations include:

1. Enumerate devices on the physical Wire.
2. Read their UUIDs and other identity information.
3. Determine current PeerIds.
4. Assign new PeerIds where desired and supported.
5. Assign or associate a canonical WireNumber with the bus.
6. Enumerate gateway-capable nodes.

A physical Wire may remain anonymous/local if no explicit non-local Wire assignment is needed.

Static PeerId configuration remains available and may be preferable for systems requiring stable assignments.

---

## 10. Gateway Discovery

A gateway is a node with two or more WireSpaces-enabled network interfaces and the ability to forward Wire traffic between them.

A gateway discovery response should expose enough information for host tooling to continue mapping the network.

At minimum this may include:

- Gateway stable identity.
- Number of WireSpaces-capable interfaces.
- Current Wire membership/configuration for those interfaces.

Additional useful information may include:

- Link/interface type.
- Link state.
- Observed devices.
- Current PeerIds.
- Link capabilities.
- Existing forwarding configuration.

Gateways do not need to run a general dynamic-routing algorithm.

---

## 11. Recursive Auto-Wiring

A simple lab discovery sequence can operate recursively.

Example:

```text
Host
 |
CAN_A
 |
Gateway G1
 |
CAN_B
 |
Gateway G2
 |
UART_C
 |
Sensor
```

The host may:

1. Enumerate CAN_A.
2. Discover G1.
3. Query G1's other interfaces.
4. Command G1 to associate its downstream interface with a WireNumber.
5. Have G1 enumerate or relay enumeration on that downstream physical Wire.
6. Discover G2.
7. Repeat until the desired topology has been mapped.

For example:

```text
Main -> G1:
    "Make network_interfaces[0] Wire 5."
```

G1 may then:

- Update its local forwarding state for Wire 5.
- Assign or communicate Wire 5 to devices on that downstream segment where necessary.
- Report observed participants to Main.

Once Wire 5 is established, Main can send discovery/management traffic onto Wire 5 and continue searching outward.

---

## 12. Auto-Wiring Scope

Initial automatic mapping is intentionally constrained.

The target is:

- One central host-side organizer.
- Small total hop counts.
- Machine/robot/vehicle-like fieldbus topology.
- Primarily tree-shaped networks.
- Development and lab use.

It is **not** intended to be a general distributed routing protocol for arbitrary large or cyclic networks.

To avoid accidental loops, gateway responses should identify existing Wire memberships/configuration so that the host does not blindly bridge a Wire back into itself.

Cyclic topologies may require explicit static configuration or a future more sophisticated lab tool.

No spanning-tree-like embedded protocol is implied by this design.

---

## 13. Dynamic Construction of Static Wiring

Auto-Wiring does not change the basic data-plane philosophy.

The host dynamically discovers and configures the system, but the resulting forwarding behavior remains ordinary fixed configuration.

Conceptually:

```text
discover physical topology
        |
        v
choose/assign Wires
        |
        v
generate temporary local forwarding projections
        |
        v
install them on gateways
        |
        v
ordinary fixed forwarding
```

An embedded gateway may still reduce forwarding to a small lookup such as:

```text
(WireNumber, ingress interface) -> egress interface(s)
```

or another statically represented equivalent.

No continuously converging routing protocol is required.

---

## 14. Ephemeral Versus Static Deployment Configuration

Auto-generated lab configuration should normally be treated as ephemeral.

Recommended behavior:

- Clearly report that automatic/ephemeral Wiring is active.
- Keep discovered/configured state inspectable from host tooling.
- Do not silently treat an ephemeral lab map as an authoritative production configuration.
- Allow the host tool to save/export the discovered topology.
- Permit review and editing before generating a static deployment.

A useful workflow is:

```text
plug devices in
    ->
discover
    ->
auto-wire
    ->
experiment
    ->
save/export discovered Wiring
    ->
review/edit
    ->
generate static deployment
```

This makes strong static configuration a natural maturation path rather than a prerequisite for first use.

---

## 15. Promiscuous and Bring-Up Operation

The auto-Wiring capability complements a separate diagnostic/promiscuous operating style.

Development tooling may optionally:

- Observe packets regardless of whether the local static configuration recognizes them.
- Log unknown WireNumbers, PeerIds, Endpoints, and Services.
- Interrogate observed nodes.
- Perform explicitly privileged exploratory transmissions.

Promiscuous observation does not itself create persistent Wiring.

Auto-Wiring is the mechanism that can take discovered information and deliberately create temporary network configuration.

Both capabilities should be optional and removable/disabled for constrained or production deployments.

---

## 16. Route Repair and Learned Forwarding

A gateway may optionally support development-only route diagnosis or ephemeral repair.

For example, if a gateway has no forwarding entry for Wire 5 but observes Wire-5 traffic on two different eligible interfaces, it may infer that those interfaces are intended to provide continuity for that Wire.

In a deliberately enabled lab/recovery mode it could:

- Report the suspected missing route.
- Install a temporary forwarding splice.
- Mark that route as learned/ephemeral.
- Age it out or discard it on reboot.

This is not a replacement for general auto-Wiring.

It is most useful for local repair of an otherwise configured path.

Automatic repair should be conservative around multiple candidate interfaces, cyclic topologies, existing valid static forwarding, and ambiguous Wire membership.

---

## 17. Development Tooling

The host SDK/toolkit is expected to be an important part of the WireSpaces experience.

Potential tools include:

- Network discovery and mapping.
- Device enumeration.
- Gateway/interface inspection.
- WireNumber and PeerId assignment.
- Interactive route inspection and modification.
- Promiscuous traffic observation.
- Service telemetry.
- Wiring export.
- Static configuration generation.
- Graph visualization.
- Configuration diffing.
- Device UUID/history lookup.

This tooling allows the first-use experience to remain simple while preserving a path toward advanced system-level analysis.

---

## 18. Security Scope

WireSpaces does not currently define a general network security architecture.

Discovery, arbitrary configuration, and live route modification are privileged capabilities.

Development Auto-Wiring should therefore be treated as:

- Intended for physically trusted lab/bench environments.
- Unsafe on hostile networks.
- Normally disabled or compiled out in deployment builds unless a product supplies suitable authentication/authorization around it.

Security mechanisms may be layered in future profiles without making them part of this initial change.

---

## 19. Entry-Level to Advanced Usage

The intended WireSpaces progression is approximately:

```text
Level 0
    Connect devices.
    Send messages.
    Minimal or no authored topology.

Level 1
    Discover devices.
    Assign identities and Wires.
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

Advanced features must not make Level 0 or Level 1 unnecessarily difficult.

---

## 20. Design Principles Captured by This Change

### WireSpaces is useful before full static design

A user does not need a complete system Manifest or WireContract before basic communication works.

### Static configuration is a capability, not an admission requirement

Static Wiring provides reproducibility, legibility, validation, and eventually stronger assurance.

It is not mandatory for demos and experimentation.

### Dynamic configuration can still produce a static data plane

Host-driven lab discovery may construct routing dynamically while leaving embedded forwarding simple and fixed after configuration.

### Physical buses are first-class Wires

The simplest Wire is just a physical bus.

Virtual Wires add multi-link and subset composition only when needed.

### Constrained profiles may deliberately have representability limits

Classical CAN with 11-bit identifiers supports only seven explicitly named non-local Wires.

That limitation is acceptable.

A future 29-bit CAN profile can remove it.

### Advanced analysis is additive

WireContracts, authority restrictions, schedulability analysis, redundancy analysis, and assurance-oriented features build on the basic Wire model rather than defining the minimum viable WireSpaces user experience.

---

## 21. Open Items

The following remain intentionally unresolved:

- Final replacement term, if any, for `Main`.
- Exact physical bit ordering of the 11-bit CAN identifier fields.
- Full canonical WireNumber width and allocation policy.
- Exact discovery Service schemas.
- Exact gateway-management Service schemas.
- PeerId allocation/conflict-resolution protocol.
- Detailed persistence rules for lab-assigned configuration.
- 29-bit CAN identifier layout.
- Policies for cyclic topologies.
- Authentication/authorization for any future deployment use of configuration Services.
- Exact semantics and scope of optional WireContract enforcement.
- Exact route-learning/repair rules.

These should be resolved through implementation and concrete use cases rather than expanding the mandatory architecture prematurely.

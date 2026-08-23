# Sketch 05 — Multicore AMP Gateway

**Mapping:** Three AMP Endpoint Domains are joined by shared-memory Links; four Control-origin operational Wires span the control/I/O boundary to heterogeneous fieldbuses, while upstream plant, maintenance, and device-private supervision relationships remain separate Wires.
**Worst friction:** Configuration burden — Mild.
**Main lesson:** Wire identity follows authority and survives both inter-core and external forwarding; the core that owns a driver or initiates an RS-485 poll does not thereby become the semantic producer.
**Configs:** A — one three-core gateway with four fieldbuses and upstream Ethernet.

**Intent:** Stress WireSpaces with a serious multicore gateway containing several dispatch domains, internal shared-memory Links, heterogeneous external Links, repeated forwarding projections, and timing-sensitive control traffic.

The system is illustrative rather than a safety design. “ASIL-A-ish” below means that timing, freshness, bounded failure, and restart dependencies matter enough to state explicitly; it is not an ASIL allocation, safety case, or WireContract.

---

# Configuration A — Three AMP domains, four fieldbuses

## 1. What changed and maturity

Relative to the single-domain gateway in sketch 01-B:

- one MCU runs three asymmetric-multiprocessing domains with independent dispatch and scheduling;
- inter-domain communication uses three bounded shared-memory/FIFO Links;
- the gateway controls four named heterogeneous fieldbuses;
- upstream plant control and service tooling share Ethernet but have different authority;
- local telemetry logging continues without a connected service workstation.

**Maturity level: Level 3.** This deployment needs generated bindings, one authoritative Wiring source, explicit resource/timing claims, and repeatable static routing. It does not require WireContracts or a formal safety profile.

## 2. Native communication model

Without WireSpaces terminology:

- The **Real-Time Control core** owns machine behavior. It computes drive commands, interprets safety I/O, consumes sensor data, and issues requests to the vision subsystem.
- The **Field I/O core** owns the CAN, CAN FD, RS-485, and subsystem-Ethernet peripherals. It performs framing, schedules controller access, polls the RS-485 chain, and moves complete messages between field devices and the other cores.
- The **Host/Diagnostics core** owns upstream Ethernet, local persistent logging, host sessions, and maintenance tooling.
- A plant controller sends high-level modes and production requests to the Real-Time Control core. These are not the hard real-time field commands.
- A service workstation can inspect or update the gateway and downstream devices. It is not part of normal machine control.
- Field telemetry is consumed by Real-Time Control and copied to Host/Diagnostics for logging without asking a field device to publish twice.
- The machine continues local control if the Host/Diagnostics core or upstream Ethernet fails.
- Failure of the Real-Time Control core removes control authority; no other core silently takes over.
- Each external Link should be restartable independently where hardware permits, but all four fieldbus drivers still share one I/O core and therefore have a declared common failure dependency.

The natural communication relationships are:

1. four separate field-control domains with one real-time controller;
2. one plant-to-gateway supervisory relationship;
3. one workstation-to-device maintenance relationship;
4. one internal gateway-supervision relationship.

The shared-memory channels are implementation placement. They do not create new application authorities.

## 3. Obvious conventional implementation

> **Obvious conventional implementation:** per-bus CAN/CAN-FD/RS-485/Ethernet drivers on an I/O core, custom shared-memory mailboxes between AMP cores, an upstream TCP/UDP or proprietary Ethernet protocol, a maintenance proxy, and hand-maintained routing between external message IDs and internal IPC queues.

That design already needs:

- fieldbus message/identifier allocation;
- three inter-core queue contracts and cache-coherency rules;
- routing from each peripheral to the correct core;
- bounded queues and overload behavior;
- host request routing to gateway-local or downstream targets;
- freshness checks and restart handling;
- configuration agreement among three separately built images.

WireSpaces adds explicit Endpoint Domains, shared-memory Link Interfaces, canonical Wires across the internal/external boundary, and generated forwarding state. It replaces message-specific IPC translation with same-PDU forwarding where the Service semantics are unchanged.

## 4. WireSpaces mapping

### 4.1 Device-centric topology

```text
 Plant controller                         Service workstation
        \                                       /
         +---------- upstream Ethernet --------+
                              |
                    Host/Diagnostics Domain
                    - upstream Ethernet LLL
                    - persistent telemetry log
                    - maintenance endpoints
                       |                 |
              SHM Host-Control     SHM Host-I/O
                       |                 |
              Real-Time Control       Field I/O Domain
              Domain                 - Router + field LLLs
              - control Services     - no application re-originator
              - four Wire Origins        |
                       |                  +-- CAN FD: DriveBus
                 SHM Control-I/O          +-- Classical CAN: SafetyIOBus
                       |                  +-- RS-485: SensorBus
                       +------------------+-- Ethernet: VisionBus
```

Each box is one Endpoint Domain with one coherent Dispatcher and concurrency scope. Each shared-memory connection is an ordinary Physical Link with one Link Interface in each adjacent Domain (`CORE §13`).

The Field I/O Domain owns mutable driver/LLL state, but it is not the Origin of the operational Wires. It forwards complete canonical PDUs between Real-Time Control and field devices.

### 4.2 Gateway domains and Link Interfaces

| Device / Domain | Endpoint Domains | Link interfaces | Role summary |
|---|---|---|---|
| Gateway — Real-Time Control core | One AMP Domain | SHM Control-I/O; SHM Host-Control | Origin of four operational Wires and internal supervision |
| Gateway — Field I/O core | One AMP Domain | SHM Control-I/O; SHM Host-I/O; CAN FD; Classical CAN; RS-485; subsystem Ethernet | Owns field LLLs and same-Wire forwarding |
| Gateway — Host/Diagnostics core | One AMP Domain | upstream Ethernet; SHM Host-Control; SHM Host-I/O | Upstream gateway, local logger, maintenance participant |
| Plant controller | One host/PLC Domain | upstream Ethernet | Origin of PlantSupervisionWire |
| Service workstation | One host-tool Domain | upstream Ethernet | Origin of MaintenanceWire |
| Drive ECUs | One Domain per ECU | CAN FD | Nodes on DriveControlWire and MaintenanceWire |
| Safety/I/O ECUs | One Domain per ECU | Classical CAN | Nodes on SafetyIOWire and MaintenanceWire |
| Sensor nodes | One Domain per sensor | RS-485 | Polled Nodes on SensorAcquisitionWire and MaintenanceWire |
| Vision subsystem | One Domain | dedicated subsystem Ethernet | Node on VisionControlWire and MaintenanceWire |

The three gateway Domains are one physical device but separate dispatch/concurrency scopes. Stable device identity groups them in tooling; Wire-local delivery still needs generated, unambiguous Domain/Endpoint bindings.

### 4.3 Physical Links

| Physical Link | Participants | Character | Main bounds/concerns |
|---|---|---|---|
| Upstream Ethernet | Plant controller, service workstation, Host Domain | Multi-access, high bandwidth | Authentication/product security outside base WS; two authority Wires share it |
| SHM Control-I/O | Control Domain, I/O Domain | Point-to-point shared memory | Bounded queues, release/acquire, cache policy, behavior across either core restart |
| SHM Host-Control | Host Domain, Control Domain | Point-to-point shared memory | Plant supervision, gateway maintenance, internal supervision |
| SHM Host-I/O | Host Domain, I/O Domain | Point-to-point shared memory | Maintenance fan-out and operational observation copies |
| DriveBus | I/O Domain, drive ECUs | CAN FD | High-rate bounded setpoints/state; controller mailbox priority behavior |
| SafetyIOBus | I/O Domain, safety/I/O ECUs | Committed Classical CAN | Compact PDUs, PDUA bounds, high-priority event latency |
| SensorBus | I/O Domain, sensor nodes | Half-duplex polled RS-485 profile | Poll cadence limits freshness; empty poll response is normal |
| VisionBus | I/O Domain, vision subsystem | Dedicated Ethernet WS profile | Larger datagrams, bounded aggregation latency |

### 4.4 Wires — operational

| Wire (name / #) | Origin | Nodes | Physical realization | Notes |
|---|---|---|---|---|
| DriveControlWire / assigned | Real-Time Control Domain | Drive ECUs | SHM Control-I/O + CAN FD | Host Domain receives an explicit observation tap over SHM Host-I/O |
| SafetyIOWire / assigned | Real-Time Control Domain | Safety/I/O ECUs | SHM Control-I/O + Classical CAN | Compact safety-related state/events; no safety certification claim |
| SensorAcquisitionWire / assigned | Real-Time Control Domain | RS-485 sensor nodes | SHM Control-I/O + polled RS-485 | I/O LLL initiates polls but does not author sensor data |
| VisionControlWire / assigned | Real-Time Control Domain | Vision subsystem | SHM Control-I/O + subsystem Ethernet | Requests and bounded perception results |
| PlantSupervisionWire / assigned | Plant controller | Gateway control participant | Upstream Ethernet + SHM Host-Control | Host Domain forwards and may log; it does not re-author |

The four field Wires are separate because the real system has four participant sets, carrier/capability envelopes, load budgets, and failure boundaries. They are not four Wires merely because four cables exist.

The Host Domain is a configured local observation tap on the four operational Wires (`CORE §12.3`, SF-016), not another semantic destination and not a second transmit binding at each field device. Field `NodeToOrigin` telemetry is transmitted once on its native bus, then copied by the gateway to both Control and Host.

### 4.5 Wires — maintenance and internal

| Wire (name / #) | Origin | Nodes | Physical realization | Notes |
|---|---|---|---|---|
| MaintenanceWire / assigned | Service workstation | Gateway participant and selected downstream devices | Upstream Ethernet + SHM Host-Control + SHM Host-I/O + four fieldbuses | One authority-shaped Wire across heterogeneous branches; static forwarding remains acyclic |
| GatewaySupervisionWire / device-private | Real-Time Control Domain | Field I/O Domain; Host Domain | SHM Control-I/O + SHM Host-Control | Internal heartbeat, restart request, and latched-status access; never emitted externally |

MaintenanceWire permits direct end-to-end Service interaction with a downstream device. The gateway validates and forwards the PDU; it does not terminate firmware transport or translate a host opcode into a different fieldbus message.

GatewaySupervisionWire is device-private because its participants and authority are entirely inside the gateway. It is not spliced to MaintenanceWire. External diagnostics query each Domain's configured telemetry/identity endpoints instead of exposing the internal supervision bus wholesale.

### 4.6 Why three shared-memory Links do not create three new Wires

The internal physical topology is a triangle, but each Wire uses an acyclic subset:

```text
Operational field Wires:
    Control -- SHM Control-I/O -- I/O -- fieldbus
                                      \
                                       SHM Host-I/O -- Host observation tap

PlantSupervisionWire:
    Plant -- upstream Ethernet -- Host -- SHM Host-Control -- Control

MaintenanceWire:
    Workstation -- upstream Ethernet -- Host
                                         +-- SHM Host-Control -- Control
                                         `-- SHM Host-I/O -- I/O -- fieldbuses

GatewaySupervisionWire:
    I/O -- SHM Control-I/O -- Control -- SHM Host-Control -- Host
```

No Wire is installed on all three internal Links, so the physical triangle does not become a forwarding loop (`CORE §12.4`). The unused Link for each Wire is an intentional route-table fact, not a different Wire identity.

### 4.7 Primary interactions and timing intent

The rates and ages below are illustrative traffic claims for placement/capacity analysis, not normative Service requirements.

| Interaction | Producer | Consumer(s) | Wire | Direction | Notes |
|---|---|---|---|---|---|
| Drive setpoint | Control Domain | selected drive ECU | DriveControlWire | OriginToNode | 100 Hz example; High QoS; Snapshot state; reject if older than 20 ms |
| Drive state | drive ECU | Control Domain; Host tap | DriveControlWire | NodeToOrigin | 100 Hz example; E2E sequence/CRC; control rejects stale state |
| Safety output state | Control Domain | Safety/I/O ECUs | SafetyIOWire | OriginToNode | Compact latest-value state; High or Critical only where justified |
| Safety input snapshot | Safety/I/O ECU | Control Domain; Host tap | SafetyIOWire | NodeToOrigin | 100 Hz example; bounded age; missing/stale enters defined degraded state |
| Fault event | any field ECU | Control Domain; Host tap | Its operational Wire | NodeToOrigin | Queue semantics; Critical/High; event sequence and duplicate policy |
| Sensor sample | sensor node | Control Domain; Host tap | SensorAcquisitionWire | NodeToOrigin | Produced by sensor, transferred when I/O LLL polls; measurement age and communication age remain distinct (SF-011) |
| Sensor poll transaction | I/O Link scheduler (not PDU producer) | sensor Link | SensorAcquisitionWire | N/A — Link mechanic | 20 ms example cadence; initiating transfer grants no producer authority |
| Vision request | Control Domain | vision subsystem | VisionControlWire | OriginToNode | 30 Hz example; bounded request queue |
| Vision result | vision subsystem | Control Domain; Host tap | VisionControlWire | NodeToOrigin | Larger E2E datagram; 100 ms example freshness limit |
| Plant mode/recipe | plant controller | Control Domain | PlantSupervisionWire | OriginToNode | 10 Hz or event-driven; local control defines behavior when stale |
| Aggregate machine status | Control Domain | plant controller | PlantSupervisionWire | NodeToOrigin | Snapshot publication; not a replacement for field-level control freshness |
| Identity/link telemetry query | service workstation | selected gateway Domain or field node | MaintenanceWire | OriginToNode | Request-scoped reply; one telemetry Service per Endpoint Domain |
| Firmware image | service workstation | selected gateway Domain or field node | MaintenanceWire | OriginToNode | Background reliable segment transport; profile placement checked end-to-end |
| Domain heartbeat/status | I/O or Host Domain | Control Domain | GatewaySupervisionWire | NodeToOrigin | 10 Hz example; stale after 300 ms; external supervisor still required for Control |
| Domain restart/enable request | Control Domain | I/O or Host Domain | GatewaySupervisionWire | OriginToNode | Privileged, bounded lifecycle request with terminal result |

Reliability is not automatically chosen for control. Periodic state and setpoints use freshness and replacement semantics; firmware and configuration objects use reliable segmentation (`CORE §20.2`).

### 4.8 Gateway forwarding projections

Rows are grouped by repeated behavior. A generated implementation may expand them into per-Wire/per-ingress route entries.

| Ingress link / source | Wire(s) | Egress link(s) | Local delivery? | Splice? | Notes |
|---|---|---|---|---|---|
| Control Domain local TX | Four operational field Wires | SHM Control-I/O | N/A — locally produced | No | Control remains Origin across the internal hop |
| SHM Control-I/O at I/O Domain | Corresponding operational Wire | Its fieldbus; SHM Host-I/O for configured tap | Only for explicit I/O-Domain endpoints | No | Same PDU; Host copy is observation |
| Any fieldbus at I/O Domain | Corresponding operational Wire | SHM Control-I/O; SHM Host-I/O tap | Only for explicit local endpoints | No | `NodeToOrigin` reaches Control and local logger without duplicate field TX |
| SHM Host-I/O at Host Domain | Four operational field Wires | None | Observation tap only | No | Host cannot command field Nodes on these Wires |
| Upstream Ethernet at Host Domain | PlantSupervisionWire | SHM Host-Control | Optional configured logging | No | Plant Origin traffic passes to Control |
| SHM Host-Control at Host Domain | PlantSupervisionWire | upstream Ethernet | Optional configured logging | No | Control `NodeToOrigin` returns to Plant |
| Upstream Ethernet at Host Domain | MaintenanceWire | SHM Host-Control and/or SHM Host-I/O | Yes when Host is target | No | Destination map may prune branches; flood-and-filter is valid within bounds |
| SHM Host-I/O at I/O Domain | MaintenanceWire | selected fieldbus branch(es) | Yes when I/O Domain is target | No | No forwarding to SHM Control-I/O, preventing the internal triangle from closing |
| Any fieldbus at I/O Domain | MaintenanceWire | SHM Host-I/O | No unless explicit observer binding | No | Downstream replies/events continue toward workstation Origin |
| SHM Host-Control or SHM Host-I/O at Host Domain | MaintenanceWire | upstream Ethernet | No unless explicit observer binding | No | Gateway-domain and downstream `NodeToOrigin` traffic returns to workstation Origin |
| Control Domain local TX | GatewaySupervisionWire | SHM Control-I/O; SHM Host-Control | N/A — locally produced | No | Device-private fan-out |
| SHM internal ingress at Control | GatewaySupervisionWire | None | Yes | No | Domain status terminates at internal Origin |

No splice is needed in the chosen mapping. Network-visible Wires retain canonical identity across shared memory and external carriers; the one device-private Wire never crosses the device boundary.

### 4.9 Endpoint Domains, concurrency, and ownership

Each Domain owns its own Dispatcher and Endpoint storage:

- **Control:** control inputs/outputs, plant supervision, internal supervisor;
- **I/O:** Link-local telemetry and management Endpoints, not application control logic;
- **Host:** upstream sessions, persistent log tap, maintenance and host telemetry.

The Router tables may be shared/read-mostly, but each LLL instance has one serialized mutable owner. An ISR captures one bounded hardware unit and wakes that owner; it does not parse complete PDUs or dispatch Services (`CORE §13.3`).

Every shared-memory Link contract states:

```text
producer and consumer identities
queue capacity and full/empty behavior
release/acquire and cache-maintenance rules
restart generation and records-in-flight behavior
which restart unit owns each queue and allocator
```

Copy-based bounded queues are the baseline. A cross-core zero-copy lease would require explicit allocator lifetime, cache ownership, bounded consumers, and restart-safe reclamation; this sketch does not need it.

### 4.10 Failure and restart behavior

- **One field LLL fails:** restart that LLL first; the other field Links and Wires remain up if hardware dependencies permit.
- **Field I/O core fails:** all four field paths and the Host observation feed fail together. The Wiring does not pretend the four Wires are physically independent.
- **Host core or upstream Ethernet fails:** PlantSupervisionWire and external MaintenanceWire become unreachable, and persistent host logging stops. Real-Time Control and field Wires continue; a full/dead observation path drops its own copy and never backpressures or cancels Control delivery.
- **Control core fails:** all four operational Wires retain their configured Origin identity, but the Origin is offline. Field Nodes may still publish valid `NodeToOrigin` state; no I/O or Host Domain becomes Origin automatically (SF-012).
- **One shared-memory Link fails:** only Wires realized across that Link lose the affected path; route identity and producer authority do not change.
- **Restart with queued work:** uncertain fragments and in-flight records are discarded and counted; accepted work receives a terminal local outcome.
- **Supervision:** the Control Domain cannot be its own complete supervisor. A hardware watchdog or another Domain outside its restart scope must observe Control progress and retain the fault conclusion (`CORE §23.8`).

Shared clocks, DMA engines, RAM banks, interrupt controllers, and the common I/O core may defeat nominal per-Link isolation. Those dependencies belong in generated configuration and failure analysis, not in optimistic route diagrams.

## 5. Rejected alternatives

| Alternative | Benefit | Why it is not the primary mapping |
|---|---|---|
| One Wire per shared-memory Link and one per external bus | Mirrors physical topology | Requires cross-Wire translation at every core boundary and changes producer lineage merely because code moved cores |
| Make Field I/O Domain Origin of all field Wires | Driver ownership and CAN/RS-485 initiation look simple | The I/O core does not author control commands or sensor values; it would turn mechanics into semantic authority |
| One giant OperationalWire across all four fieldbuses | Fewer Wire entries | Merges unrelated participant sets, capacity envelopes, failure domains, and broadcast recipients |
| Route every Wire over all three shared-memory Links | Uniform table generation | Closes a forwarding cycle inside the physical triangle and requires duplicate suppression absent from the base Router |
| Re-publish field telemetry on a separate LoggingWire | Host logging has an obvious destination | Duplicates traffic and changes source lineage; a configured local observation tap already supplies one copy |
| Splice GatewaySupervisionWire to MaintenanceWire | Makes internal status remotely visible | Joins two populated Wires with different Origins and exposes internal control authority; explicit telemetry Endpoints are safer and clearer |
| One generic gateway broker task | Centralizes all routing and policy | Creates an avoidable throughput/WCET bottleneck and couples healthy Links to one execution context |

## 6. Friction signals

| Signal | Rating | Reason |
|---|---|---|
| Artificial Origin | None | Control, plant, service workstation, and internal supervisor authorities all exist in the native system |
| Artificial Wire | None | Every Wire corresponds to a distinct participant, authority, scope, or failure/capability relationship |
| Wire proliferation | None | Four field Wires reflect four real subsystems; plant, maintenance, and supervision are separate native relationships |
| Forwarding tax | None | The conventional AMP gateway already crosses IPC and external links; same-PDU forwarding replaces message-specific translation |
| Identity awkwardness | Mild | One physical gateway contains three Endpoint Domains, so tooling must present domain-specific Services without implying three unrelated devices |
| Interaction awkwardness | Mild | Host observation and per-Domain maintenance are regular but depend on generated tap/binding support |
| Configuration burden | Mild | The route projection is large, but highly regular and generated from topology the conventional system must also encode |
| Role instability | None | Driver/core restart never transfers Origin authority; offline Origins remain configured |
| Failure mismatch | Mild | Four logical field Wires share one I/O-core failure dependency that must be declared outside the Wire topology |

The Mild ratings are about presentation and generated configuration seams, not data-plane complexity. The chosen mapping does not require a new protocol abstraction.

## 7. Model pressure

- **If one WireSpaces concept could change:** no protocol concept is missing. Tooling could provide a Wire-group authoring shorthand for “these operational Wires share the same SHM trunk and Host observation tap,” then flatten it into ordinary routes. The group must not become a runtime Router concept.
- **Useful distinction exposed by WS:** semantic production, Link initiation, and driver ownership remain separate. The Control Domain is Origin, sensor nodes produce samples, and the I/O LLL merely initiates RS-485 transfers and forwards PDUs. The model also exposes that logical Wire separation does not imply restart or hardware fault isolation.

## 8. Open questions

- How does one gateway participant expose one Link Telemetry Service per Endpoint Domain without ambiguous source/EID identity: distinct NodeIds, distinct EndpointIds, or a generated instance convention?
- What compact Wiring syntax describes repeated operational-Wire projections over SHM Control-I/O and the Host observation tap without obscuring the expanded acyclic routes?
- How are shared restart dependencies — I/O core, clocks, DMA, RAM, interrupt controller — represented so tooling does not infer false per-Link independence?
- Should the Host observation tap receive all operational PDUs or only declared telemetry/fault Endpoints, and where is that filter generated?
- Can static capacity checking combine four field Wires on each shared-memory queue while preserving per-QoS burst and freshness claims?
- Which Domain owns latched fault evidence when the failed Domain's restart reconstructs its own live telemetry?
- What protection boundary prevents an upstream maintenance build from exposing internal supervision or raw Link control (`CORE §22.1`)?
- When a field device is reachable through MaintenanceWire and its operational Wire, how does tooling present the two authorities without suggesting duplicate Services?

## 9. Spec findings

- **SF-001 / SF-016:** local observation remains useful: one field publication feeds control and logging without duplicate device TX.
- **SF-003:** the RS-485 poll initiator and the semantic Wire Origin are deliberately different.
- **SF-012:** loss of the Control Origin creates an explicit degraded state; it does not trigger role reassignment.
- **SF-019:** one telemetry Service per Endpoint Domain needs an unambiguous network-visible instance identity when several Domains share one device and MaintenanceWire.


# Archetype 10 — Automotive Zone + Central Compute Slice

**ID:** 10  
**Convergence test:** **Yes** — assign 2–3 independent mapping agents  
**Suggested Link mix:** Shared memory / IPC + Automotive Ethernet + CAN-FD / CAN29 + committed CAN11 + Guest CAN11 + LIN peripheral buses  
**Stress:** Advanced heterogeneous vehicle slice — multi-domain SoCs, several gateways and fieldbuses, dense standard-Service use, mixed local/global traffic, CAN11 default-VCN positive control, Guest coexistence, degraded operation, and configuration auditability

**Relationship to archetype 09:** Archetype 09 deliberately concentrates interaction density on one Classical CAN segment. This archetype is larger and more system-like: central compute, telematics, one full zone controller, multiple physical buses, internal shared-memory communication, transparent network forwarding, peripheral-gateway composition, and repeated platform Services across ~20 production WS Participants. CAN11 is important but is **not** the center of the exercise.

**Scale target:** ~20 production WS Participants, ~10 physical Links, three multicore/multi-domain devices, three CAN buses, three LIN peripheral buses, and enough cross-cutting Services that the mapping should be auditable but not mentally trivial.

---

## Configurations

### Config A — Production

Production vehicle slice.

Central compute, telematics, one zone controller, all fieldbus ECUs, and all local peripheral buses are present.

No developer/service PC is required for runtime operation.

### Config B — Bring-up / service

Same as Config A, plus an optional **DevPC / service tool** on the Ethernet backbone.

The DevPC may:

- inspect Identity / Version / Health;
- request diagnostic and fault information;
- inspect Link status;
- request logs where supported;
- initiate update/service workflows through the production diagnostic path.

The DevPC is **not** a runtime coordinator and has no direct production actuation authority.

Do not automatically add direct DevPC VCN relations to every CAN11 ECU. A mapper may choose direct access if justified, but should compare it with access through the normal diagnostic/update Service path.

---

## Physical topology

```text
                                  Config B only
                                    DevPC
                                      |
                               Automotive Ethernet
                                      |
     +------------------------------+------------------------------+
     |                              |                              |
     |                              |                              |
Telematics ECU                Central Compute SoC             Zone Controller SoC
     |                              |                              |
   TcuSHM                        CoreSHM                        ZoneSHM
  /      \                  /       |       \              /       |        \
TcuNet  CloudServices    CoreNet VehicleControl DiagOTA  ZoneNet ZoneControl ZonePlatform
                             \                    /           |
                              \__________________/            |
                                  Ethernet                   |
                                                             +----------------------+
                                                             |          |           |
                                                          PowerFD     BodyCAN11   LegacyAuxCAN
                                                           CAN-FD     committed      Guest
                                                             |          |           |
                                                             |          |           |
                                                     +-------+---+   +--+--+--+--+  +---+---+
                                                     |       |   |   |  |  |  |  |  |       |
                                                  Smart   Thermal Sensor Door Wiper Access Light HVAC AuxSense Washer
                                                  PDU     Pump    Hub   GW   ECU  ECU  GW    GW   GW       ECU
                                                                    |             |     |
                                                                  DoorLIN     LightingLIN HVACLIN
                                                                   / | \          / | \    / | | \
                                                                  ... ...        ... ...  ... ... ...
```

The drawing is schematic, not a required Wire mapping.

Important physical facts:

- `CoreNet`, `ZoneNet`, and `TcuNet` own or terminate their devices' external network interfaces.
- Application/platform domains communicate internally over shared-memory/IPC Links rather than each owning a separate Ethernet or CAN controller.
- Transparent WS forwarding may preserve the original Participant source through networking domains where the architecture permits it.
- LIN peripherals are conventional peripheral devices unless the trial brief explicitly provides a WS-LIN profile. Do not invent one merely to make every leaf a WS Participant.
- `BodyCAN11` is a committed WS CAN11 bus.
- `LegacyAuxCAN` is an existing mixed-use CAN11 bus with an explicitly allocated Guest WS identifier block.
- `PowerFD` uses CAN-FD with 29-bit identifiers / the richer CAN profile.

---

## Device capabilities

### Central compute

| Domain / device | Role |
|---|---|
| `CoreNet` | Owns Ethernet-facing networking for the central SoC; transparent WS forwarding between CoreSHM and backbone where valid; Link status |
| `VehicleControl` | Vehicle-level state, mode, user/vehicle intent, zone-level command/state interactions; source of system time for this slice |
| `DiagOTA` | Vehicle diagnostic aggregation, software inventory, firmware/update orchestration, fault queries, service-tool entry point |
| `Telemetry` | Collects selected high-rate/engineering telemetry and prepares it for logging/cloud export; no control authority |

All four are separate Endpoint Domains / Participants.

### Telematics ECU

| Domain / device | Role |
|---|---|
| `TcuNet` | Ethernet/network-facing domain; internal forwarding to CloudServices |
| `CloudServices` | Cloud telemetry upload, remote update package/catalog handoff, remote diagnostic session support; no direct actuator authority |

`TcuNet` and `CloudServices` communicate over `TcuSHM`.

### Zone controller

| Domain / device | Role |
|---|---|
| `ZoneNet` | Owns zone Ethernet + CAN controllers; transparent forwarding where appropriate; exposes Link status |
| `ZoneControl` | Local body/zone control and state; continues useful local operation during central/backbone loss |
| `ZonePlatform` | Zone-controller-local Health, fault history, runtime/platform diagnostics, watchdog/reset records, local logging |

All three are separate Endpoint Domains / Participants and communicate over `ZoneSHM`.

### `PowerFD` smart ECUs

| Device | Role |
|---|---|
| `SmartPDU` | Smart fuse / switched-power outputs, current sensing, load faults |
| `ThermalPump` | Pump/compressor/valve control for local thermal loop; status and telemetry |
| `SensorHub` | Aggregates several higher-rate analog/digital sensors; publishes selected zone telemetry |
| `ActuatorHub` | Several smart auxiliary actuators; accepts bounded commands and reports state |

These are native WS-capable ECUs.

### `BodyCAN11` committed CAN ECUs

| Device | Role |
|---|---|
| `DoorGateway` | Door/body function controller; gateways `DoorLIN` |
| `WiperECU` | Wiper/washer coordination and status |
| `AccessECU` | Lock/access sensors and actuators |
| `LightingGateway` | Exterior/marker lighting controller; gateways `LightingLIN` |
| `HVACGateway` | Local HVAC flap/air-distribution controller; gateways `HVACLIN` |

These are native WS-capable devices on one committed Classical CAN11 segment.

### `LegacyAuxCAN` Guest WS nodes

The bus also contains several pre-existing **non-WS** legacy CAN devices whose traffic is outside this assignment.

Two devices have gained Guest WireSpaces support:

| Device | Role |
|---|---|
| `AuxSenseGW` | Auxiliary sensor/status aggregation; low-rate telemetry and diagnostics |
| `WasherECU` | Auxiliary pump/washer control and status; firmware/version/health support |

Guest WS must coexist with unrelated existing CAN identifiers.

### LIN peripheral devices

LIN leaves are deliberately simple peripheral devices.

Example population:

```text
DoorLIN:
    window motor
    mirror module
    latch / handle module

LightingLIN:
    left lamp driver
    right lamp driver
    marker / ambient driver

HVACLIN:
    flap actuator 0
    flap actuator 1
    flap actuator 2
    local air-quality / temperature module
```

These leaves are **not required to be WS Participants**.

Their CAN gateway exposes the useful higher-level behavior to WireSpaces.

---

## Existing physical links

| Link | Type | Attachments | Config |
|---|---|---|---|
| `CoreSHM` | Shared memory / IPC | `CoreNet`, `VehicleControl`, `DiagOTA`, `Telemetry` | A, B |
| `ZoneSHM` | Shared memory / IPC | `ZoneNet`, `ZoneControl`, `ZonePlatform` | A, B |
| `TcuSHM` | Shared memory / IPC | `TcuNet`, `CloudServices` | A, B |
| `BackboneEth` | Automotive Ethernet | `CoreNet`, `ZoneNet`, `TcuNet`, optional DevPC | A, B |
| `PowerFD` | CAN-FD / 29-bit CAN profile | `ZoneNet`, `SmartPDU`, `ThermalPump`, `SensorHub`, `ActuatorHub` | A, B |
| `BodyCAN11` | Classical CAN11, committed WS | `ZoneNet`, `DoorGateway`, `WiperECU`, `AccessECU`, `LightingGateway`, `HVACGateway` | A, B |
| `LegacyAuxCAN` | Classical CAN11, existing mixed-use bus; Guest WS allocation | `ZoneNet`, `AuxSenseGW`, `WasherECU`, plus non-WS legacy nodes | A, B |
| `DoorLIN` | LIN peripheral bus | `DoorGateway` + 3 simple leaves | A, B |
| `LightingLIN` | LIN peripheral bus | `LightingGateway` + 3 simple leaves | A, B |
| `HVACLIN` | LIN peripheral bus | `HVACGateway` + 4 simple leaves | A, B |

Do not add physical Links merely to make the WS mapping easier.

---

## Standard Service expectations

This archetype deliberately includes repeated platform Services.

Unless a device capability makes a Service unreasonable, assume the following expectations.

| Service / capability | Expected providers / consumers |
|---|---|
| Identity / Version | Every production WS Participant exposes basic identity/version information |
| Health | Every production WS Participant exposes basic health/status |
| Fault / DTC information | All intelligent ECUs and platform domains expose useful fault information |
| Firmware / software update | All realistically field-updatable WS ECUs; simple exceptions may be justified |
| Link status | `CoreNet`, `ZoneNet`, `TcuNet`, and CAN/LIN gateway-capable domains/devices |
| Time synchronization | `VehicleControl` is time source for this slice; most smart ECUs consume where useful |
| Telemetry | Selected domains/ECUs only; do not make all telemetry globally broadcast |
| Logs / events | Richer domains and gateway/platform ECUs; tiny/simple nodes may expose structured fault/status instead |
| Diagnostic RPC/query | `DiagOTA`, zone/platform devices, and smart ECUs as appropriate |

Do not create one distinct Wire per standard Service merely because the Service exists.

The trial should test whether many Services remain easy to integrate without causing semantic Wire proliferation.

---

## Required interactions

Count only required application relations. Transparent forwarding hops are not separate application interactions.

### Central compute ↔ zone control

| Interaction | From | To | Rate / trigger | Notes |
|---|---|---|---|---|
| Vehicle/zone intent | `VehicleControl` | `ZoneControl` | ~20–50 Hz + event | User/vehicle mode, body intent, power-state intent |
| Zone state | `ZoneControl` | `VehicleControl` | ~10–20 Hz | Summarized body/zone status |
| Time synchronization | `VehicleControl` | interested zone/field participants | ~1–10 Hz / sync protocol | Standard time Service semantics |
| Degraded-mode notification | either | other | Event | Loss/degradation state; no automatic authority invention |

### Zone local control — `PowerFD`

| Interaction | From | To | Rate / trigger | Notes |
|---|---|---|---|---|
| Power commands | `ZoneControl` | `SmartPDU` | ~20 Hz + event | Switched loads / power state |
| PDU status/current/fault | `SmartPDU` | `ZoneControl` | ~20–50 Hz | Some selected telemetry also reaches `Telemetry` |
| Thermal command/state | `ZoneControl` ↔ `ThermalPump` | bidirectional | ~10–50 Hz | Local loop remains useful with backbone unavailable |
| Sensor state | `SensorHub` | `ZoneControl` | ~50–100 Hz | Selected subset only |
| Actuator command/state | `ZoneControl` ↔ `ActuatorHub` | bidirectional | ~20–50 Hz | Bounded local actuators |

### Zone local control — `BodyCAN11`

| Interaction | From | To | Rate / trigger | Notes |
|---|---|---|---|---|
| Door/body command + state | `ZoneControl` ↔ `DoorGateway` | bidirectional | ~10–20 Hz + event | Gateway translates to local LIN actions |
| Wiper command + state | `ZoneControl` ↔ `WiperECU` | bidirectional | ~10–20 Hz + event | |
| Access command + state | `ZoneControl` ↔ `AccessECU` | bidirectional | Event + ~5–10 Hz | |
| Lighting command + state | `ZoneControl` ↔ `LightingGateway` | bidirectional | ~10–20 Hz + event | |
| HVAC command + state | `ZoneControl` ↔ `HVACGateway` | bidirectional | ~5–20 Hz | |

### Zone local control — Guest `LegacyAuxCAN`

| Interaction | From | To | Rate / trigger | Notes |
|---|---|---|---|---|
| Auxiliary state | `AuxSenseGW` | `ZoneControl` | ~2–10 Hz | Guest WS traffic coexists with unrelated legacy CAN |
| Auxiliary command/status | `ZoneControl` ↔ `WasherECU` | bidirectional | Event + low rate | |
| Diagnostic/version access | `DiagOTA` ↔ each Guest WS node | On demand | Through normal production network path |

The minimum Guest allocation should be genuinely usable for these interactions.

### Diagnostics / identity / update

| Interaction | From | To | Rate / trigger | Notes |
|---|---|---|---|---|
| Identity/version inventory | `DiagOTA` | all production WS Participants | Startup / on demand | May be cached/aggregated; preserve canonical identity |
| Health query/publication | participants | `DiagOTA` or local interested consumers | ~1 Hz / event / query | Do not require one central health collector if unnecessary |
| Fault/DTC query | `DiagOTA` ↔ smart ECUs/platform domains | On demand | Direct Service access where natural |
| Update orchestration | `DiagOTA` -> update-capable participants | Service event | Normally one/few targets at a time; large data transfer |
| Update package handoff | `CloudServices` ↔ `DiagOTA` | On demand | Cloud protocol itself is out of scope |
| Zone platform fault/status | `ZonePlatform` ↔ `DiagOTA` | Event + query | Zone SoC faults, reset history, runtime state |

### Telemetry

| Interaction | From | To | Rate / trigger | Notes |
|---|---|---|---|---|
| Selected high-rate telemetry | `SensorHub`, `SmartPDU`, `ThermalPump` | `Telemetry` | ~20–100 Hz depending signal | Avoid needlessly flooding low-rate body links |
| Zone summary telemetry | `ZoneControl` | `Telemetry` | ~5–20 Hz | |
| Cloud-export subset | `Telemetry` -> `CloudServices` | Batched / periodic | Not all raw telemetry is uploaded |

### Telematics

| Interaction | From | To | Rate / trigger | Notes |
|---|---|---|---|---|
| Cloud connection/status | `CloudServices` | `DiagOTA`, `Telemetry` | Low rate + event | |
| Remote diagnostic session request | `CloudServices` -> `DiagOTA` | On demand | `DiagOTA` remains vehicle-side diagnostic authority/path |
| Selected telemetry upload | `Telemetry` -> `CloudServices` | Periodic/batched | |

### LIN gateway behavior

The WS-visible interactions are with the CAN gateway Participants.

Examples:

```text
ZoneControl -> DoorGateway:
    command window/mirror/latch behavior

DoorGateway -> ZoneControl:
    aggregate door state/faults

DiagOTA -> DoorGateway:
    query gateway + subordinate peripheral status

DoorGateway:
    performs native LIN transactions internally
```

Do not automatically assign canonical Participant IDs to every LIN motor/driver.

If a mapper chooses to expose a particular LIN peripheral as a separately addressable application entity, explain why and distinguish Service-level representation from a nonexistent/unassigned WS-LIN profile.

### Bring-up / service only — Config B

| Interaction | From | To | Rate / trigger | Notes |
|---|---|---|---|---|
| Vehicle inventory | DevPC | `DiagOTA` | On demand | Prefer production diagnostic path |
| Health/fault inspection | DevPC ↔ `DiagOTA` / selected platform domains | On demand | |
| Link diagnostics | DevPC | `CoreNet`, `ZoneNet`, `TcuNet` | On demand | |
| Engineering telemetry | DevPC | `Telemetry` | On demand / streaming | |
| Targeted update/service | DevPC -> `DiagOTA` -> selected ECU | On demand | No direct actuator control |

**Explicitly not required:**

- DevPC directly addressing every field ECU;
- CloudServices directly commanding actuators;
- Telemetry owning control authority;
- every LIN peripheral becoming a WS Participant;
- one giant all-participant broadcast Wire;
- one Wire per Service;
- full all-to-all communication among ~20 Participants;
- direct high-rate telemetry on the Classical CAN11 buses;
- any nominated canonical Origin role.

---

## Interaction structure (reference)

The physical topology is heterogeneous, but the application graph is structured.

Reference grouping:

```text
Central/zone control:
    VehicleControl <-> ZoneControl

PowerFD:
    ZoneControl <-> 4 smart ECUs

BodyCAN11:
    ZoneControl <-> 5 body ECUs

Guest CAN:
    ZoneControl <-> 2 Guest nodes
    DiagOTA    <-> 2 Guest nodes

Diagnostics / update:
    DiagOTA interacts with most production Participants,
    but primarily through standardized Services rather than bespoke relations

Telemetry:
    selected producers -> Telemetry
    Telemetry -> CloudServices

LIN:
    WS interactions terminate at Door/Lighting/HVAC gateway Participants
```

### `BodyCAN11` VCN reference

A useful positive-control question is whether the standardized two-Main default mapping naturally fits this bus.

One plausible role assignment is:

```text
MainA = ZoneControl
MainB = DiagOTA

Node0 = DoorGateway
Node1 = WiperECU
Node2 = AccessECU
Node3 = LightingGateway
Node4 = HVACGateway
```

This would allow:

```text
ZoneControl <-> every body node
DiagOTA     <-> every body node
ZoneControl <-> DiagOTA
both Main broadcast relations
```

inside the default 5-bit VCN scheme.

**Do not treat this role assignment as mandatory.** It is a diagnostic reference. Mappers should choose the simplest valid mapping and record if a different mapping is more natural.

### Guest VCN reference

`LegacyAuxCAN` intentionally has only two Guest WS leaf nodes.

The smallest Guest profile should therefore be a realistic candidate:

```text
MainA = ZoneControl
MainB = DiagOTA
Node0 = AuxSenseGW
Node1 = WasherECU
```

The non-WS CAN traffic is outside the VCN map.

---

## Failure and redundancy assumptions

- Loss of `BackboneEth`: the zone loses central/telematics connectivity, but `ZoneControl` should continue meaningful local operation over its fieldbuses.
- Loss of `VehicleControl`: zone-local functions may continue in a defined degraded mode; no automatic replacement controller is assumed.
- Loss of `ZoneNet`: zone external Links become unavailable even if `ZoneControl` / `ZonePlatform` remain alive internally; document the resulting local limitations.
- Loss of `ZoneControl`: fieldbus ECUs may remain powered and diagnosable, but coordinated local body control is lost/degraded.
- Loss of `DiagOTA`: production control continues; centralized diagnostics/update inventory is unavailable.
- Loss of `Telemetry`: control is unaffected; high-rate collection/export is unavailable.
- Loss of `CloudServices` or the whole telematics ECU: no required local production control is lost.
- Loss of one CAN bus affects that bus and any LIN buses subordinate to gateways on it, not unrelated fieldbuses.
- Loss of `DoorGateway`, `LightingGateway`, or `HVACGateway` also removes access to its subordinate LIN peripherals.
- Loss of a simple LIN leaf affects only the function(s) attached to that peripheral unless the gateway's application logic dictates otherwise.
- DevPC absent/unplugged: no production impact.
- No physical redundant Ethernet path is assumed for this slice.
- No runtime leader election or automatic ParticipantId reassignment is assumed.

Agents should identify whether their Wire decomposition makes these failure boundaries understandable or obscures them.

---

## Bandwidth and timing constraints

These are approximate stress inputs, not a detailed vehicle network budget.

### Shared memory / IPC

- Core and zone internal state may include **100 Hz–1 kHz** local publications.
- Shared memory has ample bandwidth relative to the fieldbuses.
- Avoid forcing high-rate internal telemetry onto external Wires merely because it exists.

### Ethernet backbone

- Assume at least **100BASE-T1-class** capacity for this slice.
- Normal WS control/diagnostic traffic is tiny relative to link capacity.
- Telemetry and firmware update can produce bursts of hundreds of kbit/s or more.
- Two extra canonical header bytes are irrelevant at this scale.

### `PowerFD`

- CAN-FD with 29-bit identifiers.
- Typical control/state rates: **20–100 Hz**.
- Fault burst + diagnostic traffic may briefly increase load.
- Selected sensor telemetry may use FD payload capacity.
- Local control latency target: roughly **<10–20 ms** for the functions modeled here.

### `BodyCAN11`

- Classical CAN11, roughly **500 kbit/s-class**.
- Normal body/control traffic: approximately **50–150 frames/s**.
- Diagnostics/update may create temporary additional load.
- High-rate raw telemetry should not be routed here unnecessarily.
- Default VCN mapping should be evaluated before custom-map invention.

### `LegacyAuxCAN`

- Existing mixed-use Classical CAN bus.
- WS owns only the explicitly allocated Guest identifier block.
- Low-rate WS traffic: generally **<20 frames/s**, excluding temporary service activity.
- Existing non-WS bus load must be treated as already present.

### LIN buses

- Assume conventional **~19.2 kbit/s-class** LIN.
- Peripheral polling/commands are local and modest-rate.
- LIN bandwidth should not drive global WS Wire structure.

---

## Configuration / accounting requirements

This assignment is large enough that the mapper must provide an auditable inventory.

At minimum report:

```text
Production WS Participants:
    count

Physical Links:
    count by type

Canonical Wires:
    count
    member Links / major Participant scope
    which are device-private vs network-visible

Gateway / forwarding state:
    approximate entries or Link masks

BodyCAN11:
    WireAliases used
    default-map aliases
    custom-map aliases
    VCNs used per alias
    whether second alias is needed for ordinary production

LegacyAuxCAN:
    Guest width selected
    Guest VCNs used / available
    whether default Guest mapping suffices

CAN-FD:
    note whether direct normal-range addressing is sufficient

Standard Services:
    which are ubiquitous
    which are intentionally limited to richer nodes
```

Do not hide a large configuration burden behind "tooling can generate it." Tooling may generate it, but the underlying configuration still counts.

---

## Expected diagnostic outcomes

### Question A — R6 scale / Logical Bus model

Does the Participant + Logical Bus model remain understandable when the system has:

- ~20 Participants;
- internal and external Links;
- multiple gateways;
- multiple physical buses;
- local degraded operation;
- repeated standard Services?

A healthy result should not require one Wire per interaction or one giant machine-wide Wire for everything.

### Question B — intra-device + inter-device continuity

Do `CoreSHM`, `ZoneSHM`, and `TcuSHM` fit naturally into the same Service model as Ethernet/CAN communication?

The trial should expose whether moving a Service boundary across:

```text
same SoC
    ->
Ethernet-connected ECU
    ->
CAN-connected ECU
```

requires application-level reinvention or mostly deployment changes.

### Question C — committed CAN11 VCN

`BodyCAN11` is intended as a **positive control** for the unified VCN default.

Valid outcomes include:

```text
"Two-Main default is natural; one alias covers production body + diagnostics."

"Default works for ordinary control, but one additional semantic Wire / alias
 improves locality or service separation."

"Default MainA/MainB roles feel artificial even in this controller + diagnostic
 pattern — evidence against the proposed default."

"Custom VCN mapping is simpler than the default for this bus."
```

Record VCN/alias counts.

### Question D — Guest CAN

Does the smallest or next-smallest Guest VCN allocation provide useful incremental WS capability on `LegacyAuxCAN` without taking over the bus?

The desired evidence is about **trial/adoption usability**, not maximum capacity.

### Question E — Service density

Does exposing Identity/Version/Health/Fault/Update across many heterogeneous Participants feel repetitive in a useful way, or does each Link/device class require bespoke integration?

This is a central project-level question.

### Question F — failure/degraded behavior

Does the Wire mapping make:

```text
backbone lost, zone still useful
one fieldbus lost, others unaffected
one LIN gateway lost, only subordinate functions disappear
telematics lost, local operation unchanged
```

easy to reason about?

### Question G — configuration auditability

Can a competent embedded engineer inspect the final Participant/Wire/alias/VCN configuration and understand the communication structure?

A technically valid but opaque deployment is a negative finding.

---

## Valid experiment outcomes

Examples:

```text
"R6 remains clean at ~20 Participants; Wires track real propagation/locality
 boundaries; standard Services reuse well across SHM, Ethernet, FD, and CAN11."

"Canonical architecture is clean, but the CAN11 body bus is the only place
 that requires meaningful representation configuration."

"Default VCN mapping fits the realistic body bus and Guest-4 fits the legacy
 auxiliary trial; no Compact/General mode appears necessary."

"Several overlapping Wires are useful on Ethernet/shared memory, while CAN11
 stays intentionally more constrained."

"LIN leaves are naturally represented behind application gateways rather than
 forcing every peripheral into the canonical Participant model."

"Too many Wires are required mainly to contain telemetry or diagnostics;
 locality model may be over-structuring the system."

"Configuration is mechanically possible but hard to audit at this size;
 deployment model/tooling needs simplification."
```

---

## What this archetype does not include

- The entire vehicle.
- Camera/radar/lidar sensor streaming.
- ADAS perception compute.
- Infotainment/audio/video distribution.
- Full chassis brake/steer safety loops.
- High-voltage traction-control architecture.
- Vehicle-wide redundant Ethernet backbone.
- Dynamic wireless/ad-hoc topology.
- A WS-native LIN profile unless separately supplied by the experiment overlay.
- Every physical peripheral as a WS Participant.
- Extended canonical address ranges; this slice should fit comfortably in the normal 8-bit Participant/Wire allocation.
- A requirement that every network Link use the same Wire decomposition.
- Any assumption that DevPC or CloudServices is a production controller.

---

## Comparison to archetype 09

| | **09 — Distributed Chassis CAN Cell** | **10 — Automotive Zone + Central Compute Slice** |
|---|---|---|
| Production WS Participants | 9 | ~20 |
| Physical Links | 1 | ~10 |
| Shared-memory IPC | No | Core, Zone, and Telematics |
| Ethernet | No | Automotive backbone |
| CAN11 | One committed bus; primary stress | One committed + one Guest bus |
| CAN-FD / CAN29 | Optional escape path | Normal production Link |
| LIN | No | Three peripheral buses behind CAN gateways |
| Gateways | None | Several transparent/application gateway boundaries |
| Primary CAN11 question | VCN ceiling / alias overflow | Default VCN usability + Guest adoption |
| Standard Service density | Limited | Deliberately high |
| Failure topology | Single-bus cell | Backbone, bus, gateway, and local-degraded boundaries |
| Primary role | CAN11 density stress | Advanced heterogeneous system / ecosystem stress |

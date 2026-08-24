# Sketch — Archetype 10: Automotive Zone + Central Compute Slice

```text
**Archetype:** archetypes/10_automotive_zone_core_slice.md
**Agent:** cursor_B
**Output file:** sketches/10_automotive_zone_core_slice_cursor_B.md
**Date:** 2026-08-23
```

---

## Executive summary

```text
**Minimum mapping:** 20 production Participants, 6 Logical Buses across 7 WS carrier Links plus 3 gateway-local LIN buses; BodyCAN11 uses one committed default-map alias and LegacyAuxCAN uses Guest-3.
**Question A (R6):** Natural — heterogeneous scope and failure boundaries remain auditable.
**Question B (CAN11):** Default — BodyCAN11 is a genuine two-Main positive control; Guest-3 covers the two-node legacy adoption case.
**Worst friction (minimum path):** Configuration burden — Moderate.
**Main lesson:** Six scope-shaped Wires preserve application identity across SHM, Ethernet, CAN-FD, and CAN11 without making LIN leaves Participants; CAN11 representation is simpler than the canonical forwarding configuration at this system scale.
```

---

## Disposition block

| Area | Assessment |
|---|---|
| Participant identity | Natural |
| Wire decomposition | Natural |
| Forwarding | Moderate |
| Non-CAN configuration | Moderate |
| CAN11 VCN fit | Default |
| Better with CAN29? | no |

**Explanation:** Question A: twenty production Endpoint Domains retain one ParticipantId across internal IPC and external networks; six Wires correspond to control, fieldbus, platform, and telemetry scopes rather than physical cables or individual Services. Three networking domains transparently forward canonical PDUs, while the three LIN gateways compose higher-level behavior instead of pretending simple LIN leaves are WS Participants. The resulting configuration is substantial but inspectable, and the Wires preserve the stated backbone, fieldbus, and gateway failure boundaries.

Question B: BodyCAN11 naturally assigns `ZoneControl` as MainA, `DiagOTA` as MainB, and five body ECUs as Nodes; one default alias carries 11 required ordinary relations, including MainA↔MainB, with no custom table. LegacyAuxCAN uses a separate Guest-3 classifier and four deployment-global relations within its seven-relation capacity; unrelated legacy identifiers remain non-WS. CAN29 is already used where richer addressing and telemetry are natural (`PowerFD`), but would not improve the modest BodyCAN11 or Guest buses.

---

## 1. Native communication model

This vehicle slice consists of three multi-domain computers connected by Automotive Ethernet:

- **Central compute:** a network domain, vehicle-control application, diagnostic/update application, and telemetry collector communicate internally over shared memory.
- **Zone controller:** a network domain, zone-control application, and zone-platform application communicate internally over shared memory. The network domain owns Ethernet and three fieldbus controllers.
- **Telematics ECU:** a network domain and cloud-services application communicate internally over shared memory.

The zone controller owns:

- **PowerFD:** four native smart ECUs on CAN-FD/29-bit CAN;
- **BodyCAN11:** five native WS body ECUs on committed Classical CAN11;
- **LegacyAuxCAN:** two Guest-WS nodes sharing an existing bus with unrelated legacy CAN traffic.

Three BodyCAN11 devices gateway conventional LIN peripherals. Door, lighting, and HVAC behavior is exposed at the gateway ECU; the simple LIN motors, lamp drivers, and flap actuators are not independently network-addressed entities.

VehicleControl and ZoneControl exchange intent, state, time, and degraded-mode notifications. ZoneControl directly coordinates field ECUs and remains useful when the Ethernet backbone is unavailable. DiagOTA inventories and services production Participants through their normal network paths. Selected PowerFD and zone telemetry reaches the central Telemetry domain and a filtered subset reaches CloudServices; raw high-rate data is not sent onto Classical CAN11.

Config B adds a DevPC on Ethernet. It uses DiagOTA, Telemetry, and network-domain Link-status Services rather than acquiring direct field actuation or a relation to every CAN ECU.

---

## 2. Obvious conventional implementation

> **Obvious conventional implementation:** SOME/IP or custom Ethernet services between SoCs; generated shared-memory IPC schemas inside each SoC; CAN-FD and CAN databases for zone ECUs; a reserved 11-bit CAN block for incremental legacy-bus support; UDS/DoIP-style diagnostics and update proxying; LIN schedules and signal databases contained in body gateways.

> **What structure does a conventional design use?** Per-network signal/service databases, gateway routing tables, diagnostic target inventories, software-component IDs, IPC channel definitions, CAN ID allocations, and LIN schedules. Cross-network identity and forwarding are usually encoded separately in each middleware or gateway.

WireSpaces adds one deployment Participant identity per Endpoint Domain, named Logical Buses spanning unlike Links, canonical forwarding state, and CAN11 projection tables. It removes several bespoke link-to-link identity translations, but does not remove the need to inventory approximately twenty domains, six communication scopes, and three network routers.

---

## 3. Minimum mapping (required first)

### 3.1 Participants

| ParticipantId | Endpoint Domain | Device | Notes |
|---|---|---|---|
| 0x01 | CoreNet | Central compute SoC | CoreSHM↔Backbone forwarding; Link status |
| 0x02 | VehicleControl | Central compute SoC | Vehicle state/intent; time source |
| 0x03 | DiagOTA | Central compute SoC | Diagnostic aggregation and update orchestration |
| 0x04 | Telemetry | Central compute SoC | Selected telemetry collector; no control authority |
| 0x05 | TcuNet | Telematics ECU | TcuSHM↔Backbone forwarding; Link status |
| 0x06 | CloudServices | Telematics ECU | Cloud handoff; no actuator authority |
| 0x07 | ZoneNet | Zone controller SoC | ZoneSHM↔Ethernet/CAN forwarding; Link status |
| 0x08 | ZoneControl | Zone controller SoC | Local zone control; continues during backbone loss |
| 0x09 | ZonePlatform | Zone controller SoC | Local health, faults, logs, reset history |
| 0x0A | SmartPDU | PowerFD ECU | Power outputs/current/faults |
| 0x0B | ThermalPump | PowerFD ECU | Local thermal loop |
| 0x0C | SensorHub | PowerFD ECU | Selected higher-rate telemetry |
| 0x0D | ActuatorHub | PowerFD ECU | Smart auxiliary actuators |
| 0x0E | DoorGateway | BodyCAN11 ECU | Door/body control; composes DoorLIN behavior |
| 0x0F | WiperECU | BodyCAN11 ECU | Wiper/washer control |
| 0x10 | AccessECU | BodyCAN11 ECU | Lock/access functions |
| 0x11 | LightingGateway | BodyCAN11 ECU | Lighting control; composes LightingLIN behavior |
| 0x12 | HVACGateway | BodyCAN11 ECU | HVAC distribution; composes HVACLIN behavior |
| 0x13 | AuxSenseGW | LegacyAuxCAN Guest node | Low-rate auxiliary sensing |
| 0x14 | WasherECU | LegacyAuxCAN Guest node | Washer command/status |
| 0x15 | DevPC | Service tool (Config B only) | Ethernet maintenance; no production actuation |

**Production WS Participants: 20.** Config B has 21 with DevPC. The LIN leaves are conventional peripherals and receive no ParticipantIds.

### 3.2 Wires

| Wire (#) | Participants | Physical links | Purpose |
|---|---|---|---|
| W1 VehicleZone | 0x02, 0x08 | CoreSHM, BackboneEth, ZoneSHM | Vehicle/zone intent, state, time, degraded notification |
| W2 PowerServices | 0x02, 0x03, 0x08, 0x0A–0x0D | CoreSHM, BackboneEth, ZoneSHM, PowerFD | Power/thermal/sensor/actuator control, time, diagnostics/update |
| W3 BodyServices | 0x03, 0x08, 0x0E–0x12 | CoreSHM, BackboneEth, ZoneSHM, BodyCAN11 | Body control plus normal diagnostic/update path |
| W4 AuxServices | 0x03, 0x08, 0x13, 0x14 | CoreSHM, BackboneEth, ZoneSHM, LegacyAuxCAN | Guest auxiliary control, status, diagnostics/version |
| W5 PlatformServices | 0x01–0x09; 0x15 in Config B | CoreSHM, BackboneEth, ZoneSHM, TcuSHM | Platform identity/health/faults, cloud package handoff, Link status, service entry |
| W6 EngineeringTelemetry | 0x04, 0x06, 0x08, 0x0A–0x0C; 0x15 in Config B | CoreSHM, BackboneEth, ZoneSHM, TcuSHM, PowerFD | Selected high-rate/engineering telemetry and cloud-export subset |

These are **scope-shaped**, not one per Service:

- W1 isolates central/zone coordination from fieldbus details.
- W2–W4 each represent a real fieldbus control and failure scope while spanning IPC/Ethernet so canonical application identities reach field ECUs.
- W5 groups repeated low-rate platform Services across the three SoCs.
- W6 is justified by bandwidth and locality: selected telemetry reaches central/telematics consumers without propagating unrelated control traffic or raw telemetry onto BodyCAN11/LegacyAuxCAN.

No Wire traverses a LIN bus. DoorGateway, LightingGateway, and HVACGateway consume WS commands and author native LIN transactions: composition, not transparent forwarding.

### 3.3 Interactions (primary)

| Interaction | Src | Dest | Wire | Broadcast? | Notes |
|---|---|---|---|---|---|
| Vehicle/zone intent and state | 0x02 ↔ 0x08 | peer | W1 | No | 10–50 Hz plus events |
| Time/degraded state | 0x02 ↔ 0x08 | peer | W1 | No | VehicleControl remains time source |
| Power command/status | 0x08 ↔ 0x0A | peer | W2 | No | 20–50 Hz |
| Thermal command/state | 0x08 ↔ 0x0B | peer | W2 | No | Local loop survives backbone loss |
| Sensor state | 0x0C | 0x08 | W2 | No | 50–100 Hz selected control state |
| Actuator command/state | 0x08 ↔ 0x0D | peer | W2 | No | 20–50 Hz |
| PowerFD time | 0x02 | 0x0A–0x0D | W2 | No | Only interested smart ECUs |
| PowerFD diagnostics/update | 0x03 ↔ 0x0A–0x0D | peer | W2 | No | Standard Services; usually one target |
| Body command/state | 0x08 ↔ 0x0E–0x12 | peer | W3 | No | Five MainA↔Node relations |
| Body diagnostics/update | 0x03 ↔ 0x0E–0x12 | peer | W3 | No | Five MainB↔Node relations |
| Zone diagnostic/platform relation | 0x08 ↔ 0x03 | peer | W3 | No | Uses default MainA↔MainB VCN |
| Auxiliary state | 0x13 | 0x08 | W4 | No | Guest relation |
| Washer command/status | 0x08 ↔ 0x14 | peer | W4 | No | Guest relation |
| Guest diagnostic/version | 0x03 ↔ 0x13, 0x14 | peer | W4 | No | Two Guest relations |
| SoC identity/health/fault | 0x01–0x09 | 0x03/local peers | W5 | No | Standard endpoints reuse participant relations |
| Update package/catalog handoff | 0x06 ↔ 0x03 | peer | W5 | No | Cloud protocol outside WS |
| Cloud status/remote session | 0x06 | 0x03, 0x04 | W5 | No | DiagOTA remains vehicle-side path |
| Selected PowerFD telemetry | 0x0A–0x0C | 0x04 | W6 | No | 20–100 Hz; no BodyCAN flooding |
| Zone summary telemetry | 0x08 | 0x04 | W6 | No | 5–20 Hz |
| Cloud export subset | 0x04 | 0x06 | W6 | No | Batched/periodic |
| DevPC inventory/service (B) | 0x15 ↔ 0x03 | W5 | No | Production diagnostic path |
| DevPC Link diagnostics (B) | 0x15 ↔ 0x01, 0x05, 0x07 | W5 | No | Network-domain Services |
| DevPC engineering telemetry (B) | 0x15 ↔ 0x04 | W6 | No | Optional streaming |

Different standard Services between an existing participant pair reuse the same canonical relation; no VCN is allocated per Endpoint.

### 3.4 Forwarding

Transparent network domains preserve canonical Wire, source, destination, Endpoint, QoS, and payload. Approximate ingress-sensitive forwarding masks:

| Router | Wire | Member Link interfaces | Approx. ingress masks | Notes |
|---|---|---|---:|---|
| CoreNet (0x01) | W1–W4 | CoreSHM, BackboneEth | 8 | Two directions per Wire |
| CoreNet (0x01) | W5 | CoreSHM, BackboneEth | 2 | Platform scope |
| CoreNet (0x01) | W6 | CoreSHM, BackboneEth | 2 | Telemetry scope |
| ZoneNet (0x07) | W1 | BackboneEth, ZoneSHM | 2 | |
| ZoneNet (0x07) | W2 | BackboneEth, ZoneSHM, PowerFD | 3 | Three-branch fan-out |
| ZoneNet (0x07) | W3 | BackboneEth, ZoneSHM, BodyCAN11 | 3 | CAN projection on egress |
| ZoneNet (0x07) | W4 | BackboneEth, ZoneSHM, LegacyAuxCAN | 3 | Guest projection on egress |
| ZoneNet (0x07) | W5 | BackboneEth, ZoneSHM | 2 | |
| ZoneNet (0x07) | W6 | BackboneEth, ZoneSHM, PowerFD | 3 | |
| TcuNet (0x05) | W5–W6 | BackboneEth, TcuSHM | 4 | Two directions per Wire |

**Approximate forwarding inventory:** 32 ingress-interface masks across three routers, before local-delivery masks and implementation compression. This is generated/read-mostly configuration, but it remains real audit burden.

LIN behavior is not in this forwarding table:

```text
canonical WS PDU to DoorGateway
    -> DoorGateway Service consumes it
    -> DoorGateway authors native LIN transactions
```

The same applies to lighting and HVAC. Source identity does not transparently cross into a nonexistent WS-LIN profile.

### 3.5 Link profiles

| Physical link | Profile | WS Wire(s) | Notes |
|---|---|---|---|
| CoreSHM | Shared-memory/IPC | W1–W6 | Full canonical identity; high local bandwidth |
| ZoneSHM | Shared-memory/IPC | W1–W6 | Full canonical identity |
| TcuSHM | Shared-memory/IPC | W5, W6 | Full canonical identity |
| BackboneEth | Automotive Ethernet WS datagram/segment | W1–W6 | Full canonical fields; reliable segment for bulk update |
| PowerFD | CAN-FD / 29-bit richer profile | W2, W6 | Normal-range direct addressing sufficient |
| BodyCAN11 | Committed CAN11 | W3 | One default-map alias |
| LegacyAuxCAN | Guest-3 CAN11 | W4 | One allocated 16-ID block; non-WS IDs outside it |
| DoorLIN | Native LIN, not WS | none | Gateway-local peripheral protocol |
| LightingLIN | Native LIN, not WS | none | Gateway-local peripheral protocol |
| HVACLIN | Native LIN, not WS | none | Gateway-local peripheral protocol |

**Physical-link inventory:** 10 total: 3 shared-memory/IPC, 1 Ethernet, 1 CAN-FD/29-bit, 2 Classical CAN11, and 3 LIN. Seven carry WS directly; three are application-gateway-local peripheral buses.

### 3.6 CAN11 bindings

#### BodyCAN11 — committed

```text
Profile:          committed
WireAliases used:             1 / 8
  default map:                  1
  custom map:                   0

Per alias:
  Alias:                        1
  Canonical Wire:               W3 (BodyServices)
  Mapping:                      Default
  Ordinary VCNs used:           11 / 31   (VCN 3 reserved)
  VCNs available, unused:       VCN 0, VCN 1; VCN 14–31
  Default map sufficient?       yes
  Custom entries (if Explicit): 0
  MainA PID / MainB PID:         0x08 / 0x03
  Node positions used:          5 / 14
```

| VCN | Relation | Minimum-mapping use |
|---:|---|---|
| 0 | 0x08 → kBroadcast | Available; unused |
| 1 | 0x03 → kBroadcast | Available; unused |
| 2 | 0x08 ↔ 0x03 | ZoneControl ↔ DiagOTA |
| 3 | — | Reserved Link control |
| 4 / 5 | 0x0E ↔ 0x08 / 0x03 | DoorGateway control / diagnostics |
| 6 / 7 | 0x0F ↔ 0x08 / 0x03 | WiperECU control / diagnostics |
| 8 / 9 | 0x10 ↔ 0x08 / 0x03 | AccessECU control / diagnostics |
| 10 / 11 | 0x11 ↔ 0x08 / 0x03 | LightingGateway control / diagnostics |
| 12 / 13 | 0x12 ↔ 0x08 / 0x03 | HVACGateway control / diagnostics |

This sketch **does exercise** the two-Main default allocation. MainA and MainB correspond to real application relationships: local control and vehicle diagnostics/update. They are CAN-profile positions, not canonical hierarchy.

No second BodyCAN11 alias is needed for ordinary production or Config B: DevPC uses DiagOTA rather than acquiring five direct body-node relations.

#### LegacyAuxCAN — Guest

```text
Profile:          Guest
Guest width:      3 VCN bits
GuestBase:        0x500   (illustrative aligned 16-ID allocation)
Ordinary VCNs required:       4
Ordinary capacity:            7
Default/global mapping sufficient?  yes — deployment-global four-relation table
```

| Guest VCN | Relation | Use |
|---:|---|---|
| 0 | 0x08 ↔ 0x13 | ZoneControl ↔ AuxSenseGW |
| 1 | 0x08 ↔ 0x14 | ZoneControl ↔ WasherECU |
| 2 | 0x03 ↔ 0x13 | DiagOTA ↔ AuxSenseGW |
| 3 | 0x03 ↔ 0x14 | DiagOTA ↔ WasherECU |
| 7 | — | Reserved Guest Link control |

VCNs 4–6 are unused ordinary capacity. The four meanings are deployment-global as required by the Guest profile. “MainA/MainB/Node” is only a useful conceptual description here; Guest frames carry no WireAlias and the overlay does not define the committed default positional map for Guest.

LegacyAuxCAN uses **one Guest ingress classifier** for its allocated block. Existing unrelated CAN IDs are non-WS. There is no committed-format WS traffic on this physical bus, so overlay §13 is satisfied.

#### Membership vs presence

Production Config A omits DevPC (0x15) from W5/W6. Config B installs a maintenance configuration that includes 0x15 on those Ethernet Wires. Once Config B is installed, unplugging the PC changes reachability, not Wire membership; production operation continues.

### 3.7 Standard Service accounting

| Service | Scope in minimum mapping | Intentionally limited behavior |
|---|---|---|
| Identity / Version | All 20 production Participants | Ubiquitous; queried over the Participant's existing W2–W5 relation |
| Health | All 20 production Participants | Ubiquitous basic status; no requirement for one central periodic collector |
| Fault / DTC | All intelligent ECUs and platform domains | Tiny nodes may expose structured fault/status rather than rich logs |
| Firmware / software update | SoC application/platform domains and field-updatable smart ECUs | Network-domain firmware may be updated as part of its device image; no separate LIN-leaf WS update Service |
| Link status | CoreNet, ZoneNet, TcuNet, DoorGateway, LightingGateway, HVACGateway | Only domains/devices that own meaningful external Links |
| Time synchronization | VehicleControl provider; ZoneControl and selected PowerFD consumers | Not forced onto BodyCAN11, LegacyAuxCAN, or every Participant |
| Telemetry | SmartPDU, ThermalPump, SensorHub, ZoneControl → Telemetry → CloudServices | Selected data only; W6 excludes low-rate body and Guest buses |
| Logs / events | DiagOTA, ZonePlatform, CloudServices, richer gateway/platform ECUs | Simpler field nodes expose faults/status instead |
| Diagnostic RPC/query | DiagOTA plus zone/platform and smart ECUs as appropriate | DevPC uses DiagOTA; no direct DevPC relation to every field ECU |

Repeated Services reuse the six Wire scopes and existing participant relations; they do not create one Wire or VCN per Service.

---

## 4. Optional optimizations

### 4.1 Preconfigure DevPC in production

Keep 0x15 as a W5/W6 member in Config A and accept that it is usually unreachable. **Problem solved:** no maintenance configuration transition during service. **Trade-off:** one dormant Participant and relation set in every production manifest. It does not consume BodyCAN11 or Guest VCNs.

### 4.2 Separate W5 into diagnostics and platform-health Wires

Split service-tool/DiagOTA traffic from ubiquitous SoC health and Link status. **Problem solved:** independent failure or update-transfer containment if W5 traffic becomes operationally coupled. **Not selected:** Ethernet/SHM bandwidth is ample, and the current split would mostly mirror Services rather than a measured propagation boundary.

### 4.3 Add direct DevPC field-ECU access

Join DevPC to W2–W4 and add direct field relations. **Problem solved:** bypass DiagOTA for low-level engineering. **Rejected for minimum:** archetype prefers the production diagnostic path; direct access adds configuration and, on CAN11, VCN pressure without runtime value.

### 4.4 Custom BodyCAN11 map

Replace the default map with only the eleven used relations. **Problem solved:** none; it removes unused default capabilities but adds an explicit table. The default two-Main map is more concise and auditable.

### 4.5 Move selected body telemetry to W6

Bind W6 to BodyCAN11 with a second alias. **Problem solved:** direct body engineering streams to Telemetry. **Rejected:** archetype explicitly excludes direct high-rate telemetry on Classical CAN11; body gateways expose low-rate diagnostics through W3.

### 4.6 Promote LIN leaves to Participants

Not valid without a supplied WS-LIN profile or another concrete WS Link. Service-level subcomponent identifiers inside gateway payloads remain application semantics, not canonical Participant identities.

---

## 5. Friction signals (minimum mapping happy path)

| Signal | None / Mild / Significant | One-sentence justification |
|---|---|---|
| Artificial Wire | None | Each of six Wires corresponds to a control, fieldbus, platform, or bandwidth/locality scope. |
| Wire proliferation | Mild | Six overlapping Wires are substantial, but W2–W4 preserve real fieldbus failures and W6 prevents telemetry flooding; no Wire exists per Service. |
| Artificial hierarchy | None | CAN MainA/MainB positions match real control/diagnostic relations and do not alter canonical peer identity. |
| VCN pressure | None | BodyCAN11 uses 11/31 ordinary slots; Guest-3 uses 4/7 relations. |
| WireAlias pressure | None | BodyCAN11 uses one of eight aliases; LegacyAuxCAN uses Guest with no alias. |
| Configuration burden | Moderate | Twenty Participants, six Wire scopes, approximately 32 forwarding masks, one default binding, one Guest table, and repeated Service bindings are real but auditable. |
| Failure/topology mismatch | Mild | W2–W4 expose individual bus loss and gateway composition; shared W5/W6 depend on the nonredundant backbone exactly as the physical system does. |

---

## 6. Model pressure

- **If you could change one WS concept** to simplify this system, what would it be? No protocol change is justified; the pressure is on deployment presentation—a generated per-Participant view should collapse repeated forwarding masks and standard-Service bindings while retaining the six canonical scopes for audit.
- **Did WS expose a useful distinction** the conventional model obscures? Yes: transparent forwarding preserves VehicleControl, ZoneControl, DiagOTA, and Telemetry identity across SHM/Ethernet/CAN, while Door/Lighting/HVAC gateways visibly compose new LIN transactions. It also separates electrical legacy coexistence (Guest block) from canonical membership.

---

## 7. Open questions

- Which PowerFD ECUs actually consume VehicleControl time directly versus receiving a ZoneControl-derived local time? The minimum permits direct canonical time on W2.
- Exact CAN-FD/29-bit profile encoding is outside the overlay; this sketch assumes direct normal-range Participant addressing is available.
- Whether W5 should retain one broad low-rate platform scope after measured update traffic exists, or split bulk update from health/Link status.
- Whether Identity/Health queries are directed live to all field ECUs or cached by DiagOTA; both use the same W2–W4 relations.
- GuestBase `0x500` is illustrative; production allocation must be chosen against the existing legacy CAN database.
- Config B policy: maintenance-only manifest versus permanently preconfigured but usually unreachable DevPC.
- The 32 forwarding-mask estimate is a configuration-size proxy, not an implementation-specific table format.

---

## 8. Spec findings (optional)

None filed. The archetype exercises documented forwarding, committed default VCN, Guest coexistence, and composition boundaries without exposing a new normative gap.

---

## 9. Diagrams

### Wire and Link realization

```text
Central compute                 Backbone                  Zone controller
┌──────────────────┐       Automotive Ethernet       ┌──────────────────┐
│ VehicleControl 02├─W1/W2──────────┬────────W1/W2───┤08 ZoneControl    │
│ DiagOTA       03 ├─W2-W5──────────┼────────W2-W5───┤09 ZonePlatform   │
│ Telemetry     04 ├─W5/W6──────────┼────────W5/W6───┤07 ZoneNet        │
│ CoreNet       01 │                 │                └──┬────┬────┬─────┘
└──── CoreSHM ─────┘                 │                   │    │    │
                                     │          PowerFD W2│ W3 │W4 │
Telematics                           │              / W6 │    │    │
┌──────────────────┐                 │                   │    │    │
│ TcuNet        05 ├─W5/W6──────────┘              CAN-FD  CAN11 Guest-3
│ CloudServices 06 │                               4 ECUs 5 ECUs 2 nodes
└──── TcuSHM ──────┘

W3 body gateways compose to DoorLIN / LightingLIN / HVACLIN.
LIN leaves have no canonical ParticipantIds.
```

### Failure view

```text
BackboneEth lost:
    ZoneControl + PowerFD + BodyCAN11 + LegacyAuxCAN remain locally useful
    Central intent/DiagOTA/Telemetry/Cloud paths unavailable

One fieldbus lost:
    only W2, W3, or W4 physical branch is lost
    other fieldbus Wires continue

Door/Lighting/HVAC gateway lost:
    gateway Participant + subordinate LIN functions disappear
    unrelated BodyCAN11 nodes remain

Telematics lost:
    W5/W6 cloud branch disappears
    local production control unchanged
```

### Configuration inventory

```text
Production Participants:       20
Config B Participants:         21
Canonical Wires:                6
Physical Links:                10
  WS carrier Links:             7
  native LIN peripheral buses:  3
Forwarding routers:             3
Approx. ingress masks:         32

BodyCAN11:
  aliases:                      1 / 8
  mapping:                      Default
  ordinary VCNs:              11 / 31
  MainA / MainB:             0x08 / 0x03
  Nodes:                        5 / 14

LegacyAuxCAN:
  profile:                    Guest-3
  ordinary VCNs:               4 / 7

PowerFD:
  CAN-FD / 29-bit direct normal-range addressing
```

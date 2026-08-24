# Sketch — Archetype 11: Redundant eVTOL Flight-Control + Distributed Propulsion

```text
**Archetype:** archetypes/11_redundant_evtol_flight_control.md
**Agent:** cursor_B
**Output file:** sketches/11_redundant_evtol_flight_control_cursor_B.md
**Date:** 2026-08-23
**Variant:** P8 (Config A production minimum); P4/P16 scaling noted in §3.8
```

---

## Executive summary

```text
**Minimum mapping:** P8 — 30 production Participants, 13 Logical Buses (3 device-private SHM, 3 local-sensor, 3 pair-wise peer RS-485, 2 redundant actuator groups, avionics, ground radio); CAN-FD/29-bit on four actuator buses; no CAN11.
**Question A (R6):** Natural — three symmetric FC peers without canonical master or Origin.
**Question B (CAN11):** N/A.
**Worst friction (minimum path):** Configuration burden — Significant.
**Main lesson:** R6 models triply redundant flight control, dual-homed actuator buses, and Ground Station test authority as explicit Participant relations and Service acceptance — but the RS-485 triangle forces three pair Wires plus Platform forwarding, and P8→P16 scales configuration linearly rather than combinatorially.
```

---

## Disposition block

| Area | Assessment |
|---|---|
| Participant identity | Natural |
| Wire decomposition | Natural |
| Forwarding | Moderate |
| Non-CAN configuration | High |
| CAN11 VCN fit | N/A |
| Better with CAN29? | N/A |

**Explanation:** Question A: thirty production Endpoint Domains map to deployment-global ParticipantIds with no nominated primary flight controller. Lockstep hardware stays inside each physical device (two domains per FC, one per propulsion/elevon/BMS). Three FC_Control peers exchange state over three **pair** Logical Buses on the RS-485 triangle — loop-free, not one flooded ring Wire. Actuator command intent is authored by FC_Control domains; redundant A/B CAN paths are one Wire per actuator group spanning both buses. Ground Station reachability is separate from flight-mode acceptance of its commands.

Question B: no Classical CAN11 segments in this archetype. Actuator and redundant fieldbuses use CAN-FD with 29-bit direct Participant addressing. CAN11 VCN/default-map questions do not apply.

---

## 1. Native communication model

A human-capable experimental eVTOL carries **three symmetric flight-control computers** (FC-A, FC-B, FC-C). Each is a four-core safety MCU arranged as two lockstep pairs exposing **two** usable WS domains: a **Control** domain (estimation, guidance, mode, actuator command intent) and a **Platform** domain (external Links, health, configuration, update, log/telemetry staging). The pairs communicate over device-local shared memory.

Each FC has a dedicated **IMU** and **GNSS** on local serial Links. Three FC Platform domains form an **RS-485 triangle** (A↔B, B↔C, C↔A) for peer flight-control state, health, voting data, and mode coordination. There is no firmware-designated bus master; redundancy logic is application-level.

**Forward** and **aft** actuator groups each attach to **two independent CAN-FD buses** (A and B). Every FC Platform domain is dual-homed to all four buses. Each group includes quadrant propulsion controllers (P8: two per quadrant), one elevon per quadrant, and one BMS per quadrant. Propulsion inner loops remain local; WS carries command, state, health, and capability.

A **CompactAvionics** display SoC shows summarized aircraft state and accepts pilot inputs; it is not in the inner control loop. An onboard **Telemetry Unit** bridges **Avionics Ethernet** to a **Ground Radio** for selected telemetry, diagnostics, mission/ODD configuration, test inputs, and update transfer. The **Ground Station** is a genuine WS Participant when connected; its authority over flight behavior is **Service and flight-mode dependent**, not a network role.

Loss of one FC, one RS-485 leg, one CAN bus, one propulsion node, one BMS, the ground link, or the display each produce defined degraded boundaries. The aircraft does not require Ground Station, DebugPC, or display for basic flight-control operation.

**Config A:** production minimum (P8, one Telemetry Unit, Ground Station when mission requires).  
**Config B:** optional second Telemetry Unit and backup Ground Radio.  
**Config C:** optional DebugPC on Avionics Ethernet for flight test.

---

## 2. Obvious conventional implementation

> **Obvious conventional implementation:** Triple modular redundancy with application voting; per-bus CAN databases; RS-485 peer state frames among FC gateways; ARINC-style or custom Ethernet for avionics; MAVLink or proprietary radio protocol to ground; UDS/update agents per ECU; separate IPC between control and comms cores on each FC.

> **What structure does a conventional design use?** Per-network ID tables, gateway routing matrices, mode/authority state machines, triple-redundant command arbiters at actuators, and ground-command enable flags — usually split across middleware layers with implicit “active channel” selection.

WireSpaces adds deployment-global ParticipantIds, named Logical Buses, explicit forwarding tables, and canonical Src/Dest on every PDU. Authority stays in application Services and mode state; the network does not elect a master FC or embed redundancy into the protocol. The trade is a large but inspectable forwarding and Wire inventory (~30 Participants, 13 Wires, four CAN-FD profiles, three FC routers with heavy fan-out).

---

## 3. Minimum mapping (required first)

### 3.1 Participants — Variant P8, Config A

| ParticipantId | Endpoint Domain | Device | Notes |
|---|---|---|---|
| 0x01 | FC_A_Control | FC-A SoC | State estimate, control law, actuator command intent |
| 0x02 | FC_A_Platform | FC-A SoC | External Links, peer transport, platform Services |
| 0x03 | FC_B_Control | FC-B SoC | |
| 0x04 | FC_B_Platform | FC-B SoC | |
| 0x05 | FC_C_Control | FC-C SoC | |
| 0x06 | FC_C_Platform | FC-C SoC | |
| 0x07 | IMU_A | IMU (FC-A local) | |
| 0x08 | GNSS_A | GNSS (FC-A local) | |
| 0x09 | IMU_B | IMU (FC-B local) | |
| 0x0A | GNSS_B | GNSS (FC-B local) | |
| 0x0B | IMU_C | IMU (FC-C local) | |
| 0x0C | GNSS_C | GNSS (FC-C local) | |
| 0x0D | PropFL1 | Propulsion (FL) | Forward CAN group |
| 0x0E | PropFL2 | Propulsion (FL) | |
| 0x0F | PropFR1 | Propulsion (FR) | |
| 0x10 | PropFR2 | Propulsion (FR) | |
| 0x11 | PropRL1 | Propulsion (RL) | Aft CAN group |
| 0x12 | PropRL2 | Propulsion (RL) | |
| 0x13 | PropRR1 | Propulsion (RR) | |
| 0x14 | PropRR2 | Propulsion (RR) | |
| 0x15 | Elevon_FL | Elevon (FL) | Forward |
| 0x16 | Elevon_FR | Elevon (FR) | Forward |
| 0x17 | Elevon_RL | Elevon (RL) | Aft |
| 0x18 | Elevon_RR | Elevon (RR) | Aft |
| 0x19 | BMS_FL | Battery (FL) | Forward |
| 0x1A | BMS_FR | Battery (FR) | Forward |
| 0x1B | BMS_RL | Battery (RL) | Aft |
| 0x1C | BMS_RR | Battery (RR) | Aft |
| 0x1D | CompactAvionics | Display SoC | Summaries and pilot input |
| 0x1E | TelemetryUnit_A | Telemetry ECU | Ground radio gateway |
| 0x1F | GroundStation | External (when connected) | Test/mission/telemetry; not required for flight |

**Production WS Participants (P8 Config A): 30.** Ground Station (0x1F) is external but participating when connected for mission/test — not required for airborne minimum operation.

**Config B add:** `TelemetryUnit_B` (0x20).  
**Config C add:** `DebugPC` (0x21).

Lockstep replica cores are **not** separate Participants.

### 3.2 Wires

| Wire (#) | Participants | Physical links | Purpose |
|---|---|---|---|
| W1 FC_A_Internal | 0x01, 0x02 | FC_A_SHM | Control ↔ Platform on FC-A |
| W2 FC_B_Internal | 0x03, 0x04 | FC_B_SHM | Control ↔ Platform on FC-B |
| W3 FC_C_Internal | 0x05, 0x06 | FC_C_SHM | Control ↔ Platform on FC-C |
| W4 Sense_A | 0x07, 0x08, 0x01 | IMU_A_Link, GNSS_A_Link | Local sensors → FC-A Control |
| W5 Sense_B | 0x09, 0x0A, 0x03 | IMU_B_Link, GNSS_B_Link | Local sensors → FC-B Control |
| W6 Sense_C | 0x0B, 0x0C, 0x05 | IMU_C_Link, GNSS_C_Link | Local sensors → FC-C Control |
| W7 Peer_AB | 0x01–0x04 | FC_A_SHM, FC_AB_RS485, FC_B_SHM | FC-A ↔ FC-B peer redundancy |
| W8 Peer_BC | 0x03–0x06 | FC_B_SHM, FC_BC_RS485, FC_C_SHM | FC-B ↔ FC-C peer redundancy |
| W9 Peer_CA | 0x01, 0x02, 0x05, 0x06 | FC_A_SHM, FC_CA_RS485, FC_C_SHM | FC-C ↔ FC-A peer redundancy |
| W10 FwdActuation | 0x01–0x06, 0x0D–0x10, 0x15, 0x16, 0x19, 0x1A | FC_*_SHM, FwdCAN_A, FwdCAN_B | Forward propulsion, elevons, BMS |
| W11 AftActuation | 0x01–0x06, 0x11–0x14, 0x17, 0x18, 0x1B, 0x1C | FC_*_SHM, AftCAN_A, AftCAN_B | Aft propulsion, elevons, BMS |
| W12 Avionics | 0x02, 0x04, 0x06, 0x1D, 0x1E; 0x1F when connected; 0x21 in Config C | AvionicsEth | Summaries, pilot input, platform Services, update staging |
| W13 GroundLink | 0x1E, 0x1F | GroundRadio_A | Selected telemetry/commands to/from ground |

**Config B add:** W14 GroundLink_B — 0x20, 0x1F on GroundRadio_B.

#### Wire design rationale

| Decision | Choice | Why |
|---|---|---|
| RS-485 triangle | **Three pair Wires** (W7–W9), not one ring Wire | Physical loop exists; each canonical Wire's forwarding graph must be loop-free (`CORE §3`). One flooded peer Wire around the triangle would violate this without inventing dynamic routing. |
| Peer traffic path | Control authors; Platform forwards SHM ↔ RS-485 | Preserves Control/Platform fault boundary; peer frames egress from Platform domains. |
| Redundant CAN A/B | **One Wire per group** spanning both buses | Loss of FwdCAN_A leaves W10 reachable via FwdCAN_B; separate Wires per physical bus would obscure redundant-path semantics. |
| Actuator command source | All three FC_Control on W10/W11 | No WS “active master”; actuators accept per application voting/mode. Canonical Src identifies authoring FC. |
| Ground traffic | Separate W13 (scarce radio) | Wireless bandwidth/latency unlike AvionicsEth; Telemetry Unit composes/forwards selected subsets — not all W12 traffic on radio. |
| Raw IMU on Ethernet | **Excluded** | High-rate sensor data stays on W4–W6; diagnostics may query via Platform on internal/peer paths. |

### 3.3 Interactions (primary) — P8 Config A

| Interaction | Src | Dest | Wire | Broadcast? | Notes |
|---|---|---|---|---|---|
| Control ↔ Platform (×3 FC) | 0x01↔0x02, etc. | peer | W1–W3 | No | 100 Hz–1 kHz local |
| IMU/GNSS state | 0x07/0x08 → 0x01 (etc.) | directed | W4–W6 | No | IMU ~100–1000 Hz; GNSS ~5–20 Hz |
| Peer FC state / voting / mode | 0x01/0x03/0x05 ↔ peers | peers | W7–W9 | No | 50–200 Hz; **symmetric peers** — A↔B on W7, B↔C on W8, C↔A on W9 |
| Peer Health / fault | FC Control or Platform | peers | W7–W9 | No | 10–50 Hz + event |
| Propulsion/elevon command | FC_*_Control | actuators | W10/W11 | No | 100–500 Hz; any FC may source; acceptance is application |
| Actuator state / capability | actuators | FC_*_Control (+ peers observe) | W10/W11 | No | 50–200 Hz |
| BMS pack/limit/fault | BMS_* | FC_Control, CompactAvionics | W10/W11 | No | Quadrant-local consumers |
| Aircraft summary / warnings | FC_Control (via Platform) | 0x1D | W12 | No | 10–50 Hz |
| Pilot input | 0x1D | FC_Control Service consumers | W12 | No | Event; not network authority |
| Selected telemetry | aircraft → 0x1E → 0x1F | W12→W13 | No | Rate-limited for radio |
| Mission/ODD / test / override | 0x1F | FC configuration/test Services | W13→W12→W1–W3 | No | **Reachability ≠ authorization** |
| Update transfer | 0x1F → 0x1E → target | W13→W12→… | No | Service operation; flight may be restricted during update |
| Identity/Health/fault (standard) | all production | query/local | existing relations | No | Reuse participant relations; no Wire per Service |

**Explicitly absent:** nominated FC master; Origin/Main semantics; all-to-all mesh; raw IMU on radio; lockstep cores as Participants; one Wire around RS-485 ring.

### 3.4 Forwarding

Each FC_Platform is the primary router for external Links. Approximate ingress-sensitive masks:

| Router | Wire | Egress Link interfaces | Approx. masks |
|---|---|---:|---:|
| FC_A_Platform (0x02) | W7 | FC_A_SHM, FC_AB_RS485 | 2 |
| | W9 | FC_A_SHM, FC_CA_RS485 | 2 |
| | W10 | FC_A_SHM, FwdCAN_A, FwdCAN_B | 3 |
| | W11 | FC_A_SHM, AftCAN_A, AftCAN_B | 3 |
| | W12 | FC_A_SHM, AvionicsEth | 2 |
| FC_B_Platform (0x04) | W7, W8 | SHM + 2× RS-485 | 4 |
| | W10, W11 | SHM + 4× CAN | 6 |
| | W12 | SHM + Eth | 2 |
| FC_C_Platform (0x06) | W8, W9 | SHM + 2× RS-485 | 4 |
| | W10, W11 | SHM + 4× CAN | 6 |
| | W12 | SHM + Eth | 2 |
| TelemetryUnit_A (0x1E) | W12↔W13 | AvionicsEth, GroundRadio_A | 2 |

**Approximate total ingress masks (P8 Config A): ~35** across four routers — before compression, local delivery, and observation taps. This is the primary configuration burden; it is generated/read-mostly but must be auditable.

**RS-485 loop note:** Physical triangle; logical forwarding is **acyclic per Wire**. A→C peer traffic uses W9 (CA leg) directly, not a multi-Wire relay — unless application chooses store-and-forward at B (composition), which is not required for minimum mapping.

**Redundant CAN note:** W10/W11 egress selects FwdCAN_A or FwdCAN_B (or both for broadcast observation) per Link Binding; loss of one bus removes one egress branch, not Wire membership.

### 3.5 Link profiles

| Physical link | Profile | WS Wire(s) | Notes |
|---|---|---|---|
| FC_*_SHM (×3) | Shared-memory / IPC | W1–W3, W7–W11 | Full canonical identity |
| FC_*_RS485 (×3) | RS-485 framed datagram | W7–W9 | Master-initiated or datagram profile; no ring Wire |
| AvionicsEth | Embedded Ethernet WS | W12 | 100 Mbit/s class |
| FwdCAN_A, FwdCAN_B | CAN-FD / 29-bit | W10 | Redundant pair; direct Participant addressing |
| AftCAN_A, AftCAN_B | CAN-FD / 29-bit | W11 | Redundant pair |
| IMU_*_Link, GNSS_*_Link (×6) | Local serial / UART | W4–W6 | Bounded master-polled or streaming |
| GroundRadio_A | Wireless framed datagram | W13 | Scarce; lossy; not all Services continuous |
| GroundRadio_B | Wireless framed datagram | W14 | Config B only |

**Physical Links (Config A): 18** — 3 SHM, 3 RS-485, 1 Ethernet, 4 CAN-FD, 6 serial, 1 radio.

**CAN11:** Not used. **CAN11 VCN accounting:** N/A.

### 3.6 CAN11 bindings

Not applicable. All fieldbus actuation uses CAN-FD with 29-bit identifiers and direct canonical ParticipantId addressing. No committed or Guest CAN11 segments in this archetype.

### 3.7 Standard Service accounting

| Service | Ubiquitous? | Notes |
|---|---|---|
| Identity / Version | All 30 production Participants | Queried over existing relations |
| Health | All 30 production | |
| Fault / diagnostic | All intelligent ECUs/sensors/FC domains | |
| Firmware / update | Field-updatable nodes + FC/Telemetry/Avionics | Staged via W12/W13; not during arbitrary in-flight critical update |
| Time sync | FC_Control, sensors, actuators, Telemetry | Vehicle time from FC peer coordination |
| Link status | FC_Platform (×3), TelemetryUnit(s), CompactAvionics | Per owned Link |
| Logging / events | FC domains, Telemetry, Avionics, BMS/actuators as capable | |
| Configuration | FC, Avionics, Telemetry, selected actuators | |
| Telemetry | Selected producers → Telemetry → Ground | **Not** all traffic on W13 |
| Flight mode/state | FC_Control, CompactAvionics, Ground (read) | |
| Energy state | BMS + FC/Avionics consumers | Quadrant-local on W10/W11 |
| Diagnostic RPC | Smart devices + FC/Telemetry | Ground uses W13 path |

Services reuse Wire scopes; no Service-driven Wire proliferation.

**Membership vs presence:** Ground Station (0x1F) is a configured W13/W12 member when mission-connected; disconnect changes reachability, not aircraft Wire membership. DebugPC (Config C) same pattern on W12.

### 3.8 Scaling — P4 / P8 / P16

| Variant | Propulsion count | Production Participants | Δ from P8 |
|---|---|---:|---|
| P4 | 4 (1/quadrant) | **26** | −4 propulsion PIDs |
| P8 | 8 (2/quadrant) | **30** | baseline |
| P16 | 16 (4/quadrant) | **38** | +8 propulsion PIDs |

**Qualitative architecture unchanged across variants:**

- Same 13 Wires (Config A); W10/W11 gain repeated propulsion members only.
- Same three pair peer Wires; RS-485 and FC count fixed.
- CAN-FD bindings grow linearly (add propulsion node entries per bus group).
- Forwarding masks grow ~linearly with propulsion count (more CAN local-delivery entries), not combinatorially with peer mesh.

P16 stresses **bus load** (~2× propulsion frames on W10/W11) — a bandwidth/implementation concern, not a new WireSpaces abstraction.

---

## 4. Optional optimizations

### 4.1 Single peer Wire with static forwarding tree on RS-485

One W_PeerRed spanning all six FC domains with a chosen loop-free forwarding tree (e.g. A→B→C only on RS-485, CA leg standby for failover Wire). **Problem solved:** fewer named peer Wires. **Rejected for minimum:** obscures direct A↔C peer path on CA leg; failover requires Wire rebinding or alternate configuration — three pair Wires match physical legs and failure boundaries more clearly.

### 4.2 Separate Wires per CAN bus (FwdCAN_A, FwdCAN_B, …)

Four Wires instead of W10/W11. **Problem solved:** per-bus failure isolation in Wire namespace. **Rejected:** redundant A/B paths are **one logical actuator group**; dual physical Links are alternate realizations of the same propagation scope when either path suffices.

### 4.3 Ground Station on W12 only (no W13)

Route all ground traffic over Ethernet assumption. **Rejected:** GroundRadio is physically distinct, scarce, and lossy; W13 models that boundary.

### 4.4 Collapse FC_Control and FC_Platform to three Participants

**Rejected:** archetype requires six FC Participants; collapses fault-containment distinction.

### 4.5 Direct Ground Station membership on W10/W11

Ground commands actuators without FC. **Rejected:** violates archetype — Ground Station uses test/configuration Services through FC; no production actuation authority.

### 4.6 Config B/C

- **Config B:** W14 + TelemetryUnit_B — independent ground path; no change to flight-control Wires.
- **Config C:** DebugPC on W12 — rich local diagnostics; bypasses W13 bandwidth limits for engineering telemetry.

---

## 5. Friction signals (minimum mapping happy path)

| Signal | None / Mild / Significant | One-sentence justification |
|---|---|---|
| Artificial Wire | None | Each Wire maps to SHM island, sensor locality, peer leg, actuator group, avionics, or radio scope. |
| Wire proliferation | Mild | Thirteen Wires is substantial, but driven by loop-free peer decomposition and radio scarcity — not per-device or per-Service sprawl. |
| Artificial hierarchy | None | No MainA/MainB, Origin, or nominated master FC; symmetric peers throughout. |
| VCN pressure | N/A | No CAN11. |
| WireAlias pressure | N/A | No CAN11. |
| Configuration burden | Significant | ~30 Participants, 13 Wires, ~35 forwarding masks, four CAN-FD groups, repeated standard-Service bindings — auditable but large even when generated. |
| Failure/topology mismatch | Mild | W7–W9 and W10/W11 make RS-485 leg and single-CAN-bus loss visible; losing two RS-485 legs or both CAN buses in a group is documentable; triple FC loss is application degraded mode, not WS role reassignment. |

---

## 6. Model pressure

- **If you could change one WS concept** to simplify this system, what would it be? A documented **redundant-Link deployment pattern** (one Wire, multiple egress branches, explicit failover binding) to reduce repeated forwarding-mask authoring across three identical FC_Platform routers — not a protocol change.
- **Did WS expose a useful distinction** the conventional model obscures? Yes: **reachability vs authority** for Ground Station; **symmetric FC peer identity** without electing a master; **composition at Telemetry** vs transparent forwarding for scarce radio; lockstep vs Participant boundary.

---

## 7. Open questions

- Whether peer Health on W7–W9 is authored by Control or Platform — minimum uses Platform egress with Control as primary Src for state/voting PDUs.
- Actuator redundant command consumption: last-writer vs voted vs priority-arbitration — application policy on W10/W11; WS delivers all configured sources.
- Exact RS-485 profile (polled vs datagram) provisional; framing belongs in LINK.
- Whether CompactAvionics receives BMS data directly on W10/W11 via Platform observation or only summarized via FC_Control on W12.
- P16 bus-load mitigation: QoS, separate engineering Wire on CAN — optimization only if measured saturation.
- Ground Station preconfigured on W13 when disconnected vs mission-configuration export only.

---

## 8. Spec findings (optional)

| ID | Finding |
|---|---|
| SF-R6-009 | Physical RS-485 **loop** among three peers requires explicit **pair-Wire** decomposition for loop-free Logical Buses; one ring Wire is not valid without dynamic routing. Valid pattern: three pair Wires on three legs (`archetype 11`). |
| SF-R6-010 | **Redundant CAN A/B** maps naturally as one Logical Bus per actuator group spanning both Links; failure of one physical bus is one egress-branch loss, not Wire partition. |

---

## 9. Diagrams

### System overview (P8)

```text
                    GroundStation 0x1F
                          |
                   GroundRadio_A (W13)
                          |
                   TelemetryUnit_A 0x1E
                          |
    +---------------------AvionicsEth (W12)---------------------+
    |          |              |              |                  |
CompactAvionics  FC-A Plat 0x02  FC-B Plat 0x04  FC-C Plat 0x06

FC-A: Control 0x01 <--W1 SHM--> Plat 0x02 ----+
FC-B: Control 0x03 <--W3 SHM--> Plat 0x04 ----+---- RS-485 triangle W7/W8/W9
FC-C: Control 0x05 <--W5 SHM--> Plat 0x06 ----+

IMU/GNSS --> Control (W4/W5/W6 local serial)

FC Platforms --W10--> FwdCAN_A + FwdCAN_B --> FL/FR propulsion, elevons, BMS
FC Platforms --W11--> AftCAN_A + AftCAN_B  --> RL/RR propulsion, elevons, BMS
```

### RS-485 loop-free representation

```text
Physical (triangle):          Logical Wires (loop-free each):

    A ========= B               W7: A <--> B  (FC_AB_RS485)
    | \       / |               W8: B <--> C  (FC_BC_RS485)
    |   \   /   |               W9: C <--> A  (FC_CA_RS485)
    |     X     |
    |   /   \   |               No single Wire traverses A->B->C->A
    | /       \ |
    C =========

Peer state A<->C uses W9 directly (CA leg).
```

### Failure view

```text
One FC lost:           remaining pair Wires continue; degraded 2-of-3
One RS-485 leg lost:   one pair Wire unavailable; other two legs route peer traffic
One CAN bus lost:      W10 or W11 still reachable on alternate A/B branch
Both CAN in group lost: that actuator group unreachable; other group OK
Ground link lost:      W13 down; W1-W11 onboard flight control continues
CompactAvionics lost:  W12 branch lost; inner loop unaffected
```

### Configuration inventory (P8 Config A)

```text
Production Participants:     30
Canonical Wires:             13
Physical Links:              18
Forwarding routers:           4  (3 FC_Platform + TelemetryUnit_A)
Approx. ingress masks:       ~35

P4 Participants:             26
P16 Participants:            38

CAN11:                       none
CAN-FD actuator buses:        4 (2 forward + 2 aft, redundant pairs)
```

---

## Devices (reference) — P8

| Device | Endpoint Domains | Link interfaces | Role summary |
|---|---|---|---|
| FC-A | Control, Platform | SHM, RS-485×2, CAN×4, Eth | W1, W7, W9, W10, W11, W12 |
| FC-B | Control, Platform | SHM, RS-485×2, CAN×4, Eth | W2, W7, W8, W10, W11, W12 |
| FC-C | Control, Platform | SHM, RS-485×2, CAN×4, Eth | W3, W8, W9, W10, W11, W12 |
| IMU/GNSS ×3 | one each | local serial to paired FC | W4–W6 |
| Propulsion ×8 | one each | Fwd or Aft CAN | W10 or W11 |
| Elevon ×4 | one each | Fwd or Aft CAN | W10 or W11 |
| BMS ×4 | one each | Fwd or Aft CAN | W10 or W11 |
| CompactAvionics | one | AvionicsEth | W12 |
| TelemetryUnit_A | one | Eth + GroundRadio | W12, W13 |
| Ground Station | one | GroundRadio | W13 (when connected) |

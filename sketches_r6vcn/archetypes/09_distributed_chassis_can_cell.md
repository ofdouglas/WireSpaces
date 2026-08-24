# Archetype 09 — Distributed Chassis CAN Cell

**ID:** 09  
**Convergence test:** **Yes** — assign 2–3 independent mapping agents  
**Suggested Link mix:** Committed CAN11  
**Stress:** Credible distributed chassis cell — structured interaction density, CAN11 explicit-map ceiling, plausible WireAlias overflow at bring-up

**Relationship to archetype 07:** Archetype 07 is the lighter **peer negative control** (chassis mesh + corner-local companion domains). This archetype adds **real interaction structure** — two independent four-peer meshes, local dual-core pairs, a global state producer, and bring-up traffic that can push past one 32-VCN alias. Do **not** model a full eight-domain all-to-all mesh unless the archetype requires it; count **required** relations only.

**Density target:** ~29 ordinary VCNs in production minimum mapping on one Wire / one CAN bus; ~33 with bring-up DevPC queries — near or over one WireAlias, testing second-alias split vs CAN29.

---

## Configurations

### Config A — Production

Nine production Participants on CellCAN. No developer PC. All required plant and supervisor traffic below.

### Config B — Bring-up / service

Same as Config A, plus optional **DevPC** on CellCAN via USB-CAN adapter. DevPC queries each corner **Supervisor** domain (not chassis torque paths). DevPC is not a runtime coordinator.

---

## Physical topology

```text
Five ECUs + optional PC on one Classical CAN bus (11-bit):

Four dual-core corner MCUs (FL, FR, RL, RR). Each MCU hosts two Endpoint Domains on CellCAN:

  FL_Chassis, FL_Supervisor
  FR_Chassis, FR_Supervisor
  RL_Chassis, RL_Supervisor
  RR_Chassis, RR_Supervisor

One single-core VehicleState ECU (fused motion / traction state).

Optional: DevPC on CellCAN (Config B only).

Still one electrical CAN segment. No gateway. No nominated bus master.
```

---

## Device capabilities

| Domain / device | Role |
|---|---|
| `*_Chassis` (×4) | Wheel speed, driver state, torque enable/setpoint/reset; peer chassis commands; reacts to neighbor chassis faults |
| `*_Supervisor` (×4) | Local fault management, DTCs, calibration; **cross-corner** health/fault exchange; degraded-mode coordination; coordinated reset/recovery **permission** — **not** chassis torque commands |
| `VehicleState` | Fused IMU / yaw / acceleration; vehicle-motion estimate; global traction state; optional time-base publication — **no command authority** over corners |
| DevPC (Config B) | Queries each Supervisor for logs, DTCs, calibration — maintenance only |

Each corner MCU shares one CAN transceiver; Chassis and Supervisor both transmit on CellCAN.

---

## Existing physical links

| Link | Type | Attachments | Config |
|---|---|---|---|
| CellCAN | Classical CAN 11-bit | 4 corner MCUs (8 domains), VehicleState, optional DevPC | A, B |

---

## Required interactions

Count only these relations in minimum-mapping VCN accounting unless you justify additional traffic.

### Chassis peer mesh (four domains)

| Interaction | From | To | Rate / trigger | Notes |
|---|---|---|---|---|
| Chassis status | each `*_Chassis` | all `*_Chassis` peers | ~10 Hz | Peer consumption |
| Peer torque / setpoint | any `*_Chassis` | any other `*_Chassis` | Event + periodic | No off-corner chassis commander |
| Chassis fault reaction | on peer chassis fault | local `*_Chassis` | Event | Safety-relevant |

### Supervisor peer mesh (four domains)

| Interaction | From | To | Rate / trigger | Notes |
|---|---|---|---|---|
| Supervisor health / fault exchange | each `*_Supervisor` | all `*_Supervisor` peers | ~2–5 Hz | Cross-corner degraded-mode awareness |
| Coordinated reset permission | `*_Supervisor` | peer `*_Supervisor` | Event | Permission / handshake, not chassis actuation |
| Supervisor status broadcast | each `*_Supervisor` | interested peers | ~2 Hz | Optional directed equivalents if mapper prefers — do not double-count |

### Local dual-core pairs (same corner MCU)

| Interaction | From | To | Rate / trigger | Notes |
|---|---|---|---|---|
| Chassis ↔ Supervisor handoff | `*_Chassis` ↔ `*_Supervisor` | same corner only | Event | Fault escalation, mode, recovery |

### VehicleState (global participant)

| Interaction | From | To | Rate / trigger | Notes |
|---|---|---|---|---|
| Motion / traction state | `VehicleState` | each `*_Chassis` | ~20 Hz | Directed state feed |
| Chassis feedback | each `*_Chassis` | `VehicleState` | ~10 Hz | Directed |
| Global traction broadcast | `VehicleState` | broadcast consumers | ~10 Hz | One broadcast publication source |

### Bring-up only (Config B)

| Interaction | From | To | Rate / trigger | Notes |
|---|---|---|---|---|
| Supervisor query | DevPC | each `*_Supervisor` | On demand | Logs, DTCs, calibration |
| Supervisor response | each `*_Supervisor` | DevPC | On demand | |

**Explicitly not required:**

- `*_Supervisor` → peer `*_Chassis` commands (supervisors do not command other corners' chassis)
- `VehicleState` → torque / enable commands to chassis
- DevPC → chassis torque paths
- Any nominated coordinator or MainA/MainB hierarchy
- Full 9×9 or 8×8 all-to-all mesh

**No firmware-designated bus master.** Arbitration is CAN's electrical broadcast.

---

## Interaction structure (reference)

Two independent four-peer graphs on the same physical CAN, plus four local cross-domain pairs and VehicleState spokes:

```text
Chassis peer mesh:          C(4,2) = 6 pair relations
Chassis broadcasts:         4 (one per Chassis domain)
Supervisor peer mesh:       C(4,2) = 6 pair relations
Supervisor broadcasts:      4 (one per Supervisor domain)
Local Chassis↔Supervisor:   4 pair relations (one per corner)
VehicleState↔Chassis:       4 pair relations
VehicleState broadcast:     1
-------------------------------------------------
Production minimum:        29 ordinary VCNs (if enumerated explicitly)

Config B add DevPC↔Supervisor:  +4 pair relations
Bring-up total:                33 ordinary VCNs
```

```text
FL_Chassis -------- FR_Chassis          FL_Supervisor ----- FR_Supervisor
    |\                 /|                    |\                 /|
    | \               / |                    | \               / |
    |  RL_Chassis--RR_Chassis                |  RL_Supervisor--RR_Supervisor

FL_Chassis <-> FL_Supervisor     (×4 corners)

VehicleState <-> each Chassis      (×4)
VehicleState -> Broadcast

Config B: DevPC <-> each Supervisor (×4)
```

Mappers may share one VCN per unordered pair (both directions) per overlay rules — do not count A→B and B→A as two VCNs unless your minimum mapping requires separate relations.

---

## Failure and redundancy assumptions

- Loss of one corner MCU: both Chassis and Supervisor on that corner offline; degraded meshes on remaining corners.
- Loss of `VehicleState`: chassis peer mesh may continue; global traction estimate unavailable.
- Loss of one domain on a dual-core ECU: document whether the sibling domain continues (typical: Supervisor loss may limit degraded-mode coordination; Chassis may still run locally).
- No gateway, no coordinator failover, no role reassignment.
- DevPC absent (Config A or unplugged): no effect on production traffic.

---

## Bandwidth and timing constraints

- Chassis status ~10 Hz × 4 → ~40 frames/s baseline.
- Supervisor + VehicleState traffic adds ~30–60 frames/s depending on rates.
- Peer command bursts and fault storms: plan **~150–250 frames/s** sustained on CellCAN.
- Chassis peer command latency < 20 ms for torque-enable paths.
- Conventional design: structured CAN ID matrix per relation class — often 35–50+ IDs when fully enumerated.

---

## Expected diagnostic outcomes

**Question A (R6):** Participants and one Logical Bus (CellCAN) should remain straightforward — nine production Participants, explicit interactions, no artificial coordinator.

**Question B (CAN11):** Default VCN map is **clearly unsuitable** (no node↔node slots for two peer meshes). Production ~29 explicit VCNs should fit **one** WireAlias with little headroom. Bring-up ~33 VCNs plausibly forces:

- second WireAlias binding to the same canonical Wire (split VCN map), **or**
- CAN29.

Valid experiment outcomes:

```text
"R6 clean; CAN11 production fits one alias with custom map; bring-up needs
 alias 2 or CAN29 — evidence for intentional WireAlias migration feature."

"R6 clean; CAN29 is simpler than maintaining 29+ relation VCN entries."

"R6 clean; relation-level VCN is coarser than per-message CAN IDs but acceptable."
```

---

## What this archetype does not include

- Domain controller with command authority
- Gateway or second CAN bus
- Supervisor commanding peer chassis
- Full mesh among all nine production Participants
- Ethernet or CAN29 unless mapper concludes CAN11 bring-up overflow warrants it

---

## Comparison to archetype 07

| | **07** | **09** |
|---|---|---|
| Production Participants | 8 | 9 |
| Peer meshes | 1 (chassis) | 2 (chassis + supervisor) |
| Global state ECU | No | VehicleState |
| Companion domain | Corner-local diagnostics | Distributed supervisor |
| Typical production VCNs | Lower if only required relations | ~29 enumerated |
| Bring-up alias overflow | Unlikely | Plausible at 33 VCNs |
| Primary role | Negative control / peer stress | Credible cell + CAN11 ceiling |

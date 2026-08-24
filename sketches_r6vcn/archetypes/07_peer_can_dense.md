# Archetype 07 — Dense Peer CAN Network (Negative Control)

**ID:** 07  
**Convergence test:** **Yes** — assign 2–3 independent mapping agents  
**Suggested Link mix:** Committed CAN11  
**Stress:** **Negative control** — symmetric peers, no natural coordinator

**Relationship to archetype 09:** Archetype 09 (`09_distributed_chassis_can_cell.md`) is the structured-density step-up — supervisor peer mesh, VehicleState, and ~29 production VCNs. This archetype remains the lighter peer negative control; count **required** relations only (do not assume full 8×8 mesh).

---

## Physical topology

```text
Four dual-core automotive ECUs on one Classical CAN bus (11-bit).

Corners of a small mobile platform. Each MCU hosts two Endpoint Domains on the same bus:

  FL_Chassis, FL_Diagnostics
  FR_Chassis, FR_Diagnostics
  RL_Chassis, RL_Diagnostics
  RR_Chassis, RR_Diagnostics

Eight production Participants on CellCAN. No domain controller, no gateway, no nominated master.

Optional: developer PC on CAN via USB adapter for promiscuous capture during bring-up only.
PC is not part of normal runtime control.
```

---

## Device capabilities

| Domain | Device | Role |
|---|---|---|
| `*_Chassis` (×4) | Corner MCU, real-time core | Wheel speed, driver state, fault flags; torque enable/setpoint/reset; reacts to neighbor faults |
| `*_Diagnostics` (×4) | Corner MCU, companion core | DTCs, self-test status, calibration metadata; lower-rate reporting |
| PC (optional) | USB-CAN adapter | Passive log capture only |

Each corner MCU shares one CAN transceiver; both domains transmit and receive on CellCAN.

---

## Existing physical links

| Link | Type | Attachments |
|---|---|---|
| CellCAN | Classical CAN 11-bit | FL, FR, RL, RR (8 domains); optional PC |

---

## Required interactions

| Interaction | From | To | Rate / trigger | Notes |
|---|---|---|---|---|
| Chassis status | each `*_Chassis` | all `*_Chassis` peers | ~10 Hz | Interested peers consume; electrically visible to all |
| Peer torque command | any `*_Chassis` | any other `*_Chassis` | Event + periodic setpoints | Core negative-control path — no coordinator |
| Fault reaction | on neighbor fault indication | local `*_Chassis` | Event | Safety-relevant local action |
| Diagnostics report | each `*_Diagnostics` | optional PC | ~1 Hz | Lower rate; not a command authority |
| Intra-MCU handoff | `*_Chassis` ↔ `*_Diagnostics` | same corner only | Event | Same PCB; still uses CellCAN (not modeled as a separate physical link) |

**Explicitly not required:**

- `*_Diagnostics` → peer `*_Chassis` commands (diagnostics does not command other corners)
- Any domain → coordinator or gateway (none exist)
- Cross-corner diagnostics commands

**No firmware-designated bus master.** Arbitration is CAN's electrical broadcast; application treats frames as "from whoever sent; act if addressed to me."

---

## Failure and redundancy assumptions

- Loss of one corner MCU: both domains on that corner offline; remaining six chassis domains continue with degraded peer mesh.
- Loss of one domain on a dual-core ECU (e.g. `FL_Diagnostics` only): `FL_Chassis` may continue on CellCAN.
- No gateway, no coordinator failover, no role reassignment.
- PC optional — absence does not affect peers.

---

## Bandwidth and timing constraints

- Eight chassis status streams at ~10 Hz → ~80 frames/s baseline on CellCAN.
- Four diagnostics streams at ~1 Hz → ~4 frames/s additional.
- Peer command bursts (any chassis → any other chassis) add variable load; plan for **~100–200 frames/s** sustained with headroom for fault storms.
- Peer command latency should be < 20 ms for torque-enable paths.
- Conventional design: fixed CAN ID matrix per (source, destination, message class) — often 40+ IDs for this interaction surface.

---

## Expected diagnostic outcomes

A healthy architecture evaluation may conclude:

```text
"R6 fits symmetric multi-initiator peers on one Logical Bus, but the default
 VCN map (MainA/MainB + Node slots) cannot express a dense node↔node mesh.
 A full 8-peer chassis command mesh needs on the order of 28 pair relations
 plus broadcast/status VCNs — likely exceeding one 32-VCN alias. Custom
 explicit map, multiple WireAliases, CAN29, or a flat CAN ID matrix may
 be more honest than forcing MainA/MainB hierarchy."
```

That is **reassuring**, not a failure of the experiment.

**Scale reference (for reviewers, not a mapping prescription):**

```text
Required relations only (companion domain corner-local, not supervisor mesh):
  Chassis peer pairs:           C(4,2) = 6
  Chassis broadcasts:           4
  Local Chassis↔Diagnostics:    4
  --------------------------------
  Total (typical honest minimum): ~14 ordinary VCNs

Full 8-domain all-to-all mesh (~36 VCNs) is over-modeling — see archetype 09
for structured multi-mesh density.
```

---

## What this archetype does not include

- Domain controller, plant coordinator, or nominated master
- Gateway or forwarding to another physical network
- Multiple CAN buses
- `*_Diagnostics` commanding peer corners

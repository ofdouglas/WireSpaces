# Archetype 02 — Raspberry Pi Robot + ECUs + Dev PC

**ID:** 02  
**Convergence test:** **Yes** — assign 2–3 independent mapping agents  
**Suggested Link mix:** Committed CAN11 on robot bus; Ethernet (bench); intermittent WiFi (field)  
**Stress:** Linux gateway; maintenance vs plant scope; intermittent upstream link

---

## Physical topology

```text
Small differential-drive mobile robot:

  Raspberry Pi — motion planner, estimator, logger
  DriveLeft ECU — motor driver + encoder (CAN)
  DriveRight ECU — motor driver + encoder (CAN)
  Aux ECU — bumpers, IMU (CAN)

One Classical CAN bus connects Pi (CAN interface) and three ECUs.

Developer PC:
  - Bench: wired Ethernet to Pi
  - Field: WiFi to Pi (intermittent)

Product rule (interaction authority — not physical attachment):
  The PC is not permitted to participate directly in ECU-facing plant
  interactions. All PC access to ECUs is mediated by Pi application Services.

Physical fact: the PC has no CAN interface and never attaches to RobotCAN.
That physical constraint does not, by itself, determine WireSpaces topology —
a Wire may span heterogeneous Links (Ethernet → Pi → CAN) without the PC
touching CAN. The product rule above is what excludes the PC from plant scope.
```

---

## Device capabilities

| Device | Role |
|---|---|
| Raspberry Pi | 50 Hz control loop; logs **all** telemetry locally continuously |
| Drive ECUs | Torque/speed control; status ~100 Hz |
| Aux ECU | Bumper events; IMU ~100 Hz |
| Developer PC | Bench teleop + maintenance tools; field telemetry when link up |

---

## Existing physical links

| Link | Type | Attachments |
|---|---|---|
| RobotCAN | Classical CAN 11-bit | Pi, DriveLeft, DriveRight, Aux |
| BenchEth | Wired Ethernet | PC, Pi |
| FieldWiFi | WiFi / UDP (intermittent) | PC, Pi |

---

## Required interactions

| Interaction | Pattern | Notes |
|---|---|---|
| Wheel torque commands | Pi → drive ECUs | ~50 Hz |
| Drive status | ECUs → Pi | ~100 Hz; Pi logs locally always |
| Bumper / IMU | Aux → Pi | Events + periodic |
| Bench teleop | PC → Pi → ECUs | Through Pi only |
| Bench maintenance | PC ↔ Pi | Identity, logs, firmware update to ECUs via Pi |
| Field telemetry | Pi → PC | When WiFi up; coalesce under loss |
| Autonomous operation | Pi ↔ ECUs | PC may be absent |

---

## Failure and redundancy assumptions

**Config A — Bench:** PC and Pi on Ethernet; normal teleop and maintenance.

**Config B — Field:** WiFi intermittent; **RobotCAN and Pi control unchanged** when WiFi down; Pi continues logging locally.

**Config C — Pi absent:** Pi powered off or removed (repair, shipping without compute). ECUs remain on CAN in safe/disabled state. **No PC path to ECUs.** No automatic reassignment of "who is coordinator" — the physical Pi is simply gone.

---

## Bandwidth and timing constraints

- Control loop 50 Hz; CAN sufficient for three ECUs.
- WiFi telemetry is best-effort; local Pi logging is authoritative for post-mortem.
- Maintenance firmware update may be large; tolerates slower reliable transfer when bench link available.

---

## What this archetype does not include

- Multiple Endpoint Domains on Pi (single Linux runtime for now)
- PC as a member of the plant communication scope (ECU-facing interactions)
- Redundant Pi

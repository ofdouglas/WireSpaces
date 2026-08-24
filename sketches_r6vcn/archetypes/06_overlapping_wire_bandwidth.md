# Archetype 06 — Overlapping Logical Scope for Bandwidth Isolation

**ID:** 06  
**Convergence test:** No  
**Suggested Link mix:** One physical CAN11 bus; question is whether **multiple communication scopes** are justified  
**Stress:** Deliberate overlap — same devices, different traffic classes

---

## Physical topology

```text
One mobile work platform with a single Classical CAN bus (11-bit).

Devices on the bus:
  Motion MCU — trajectory and drive commands
  Safety MCU — independent estop / speed limit enforcement
  DriveLeft, DriveRight — motor inverters
  Telemetry aggregator MCU — high-rate logging to removable storage

All five devices share one electrical CAN segment. There is no second CAN cable.
```

---

## Device capabilities

| Device | Role |
|---|---|
| Motion MCU | 100 Hz motion; commands drives |
| Safety MCU | Independent safety checks; may command torque limits; reads drive feedback |
| Drives | ~500 Hz local loops; status to bus ~200 Hz each |
| Telemetry MCU | Logs all visible CAN traffic + selected aggregates ~1 kHz effective receive |

---

## Existing physical links

| Link | Type | Attachments |
|---|---|---|
| ChassisCAN | Single Classical CAN 11-bit | All five devices |

---

## Required interactions

### Safety-critical (small, deterministic)

| Interaction | Pattern | Notes |
|---|---|---|
| Safety limits | Safety ↔ drives | Must not starve behind telemetry |
| Safety ↔ motion coordination | Safety ↔ Motion | Handshake on mode changes |

### High-volume telemetry (large, tolerates latency)

| Interaction | Pattern | Notes |
|---|---|---|
| Drive status to logger | Drives → Telemetry | High rate |
| Motion debug trace | Motion → Telemetry | Bursty |
| Post-mission upload | Telemetry → PC via WiFi dock | Not on CAN |

### Normal plant

| Interaction | Pattern | Notes |
|---|---|---|
| Motion commands | Motion → drives | ~100 Hz |

---

## Failure and redundancy assumptions

- Safety MCU is independent watchdog — not a hot standby for Motion.
- Telemetry loss does not stop motion.
- Motion loss triggers safety safe state.
- Single bus — electrical partition affects all devices equally.

---

## Bandwidth and timing constraints

**Central tension:** One CAN bus must carry ~400+ frames/s for logging while safety traffic requires bounded worst-case latency.

Conventional designs often:

- throttle telemetry,
- use separate physical buses,
- or prioritize IDs so safety wins arbitration.

The archetype asks whether a **logical** separation (two communication scopes over the same wire) reduces configuration awkwardness vs one flat bus model — not whether WS invents a second cable.

---

## What this archetype does not include

- Second physical CAN
- WiFi on-robot (dock upload only mentioned)
- Redundant Motion MCU

---

## Mapper note

Minimum mapping may use **one** Logical Bus if honest. Overlapping/narrower Wires are **optional optimization** only if bandwidth or failure-scope justification is documented (per experiment brief §5 step 7).

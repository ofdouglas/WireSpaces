# Archetype 05 — Heterogeneous Gateway (CAN + UART + Shared Memory + Linux)

**ID:** 05  
**Convergence test:** **Yes** — assign 2–3 independent mapping agents  
**Suggested Link mix:** CAN11 on fieldbuses; RS-485 polled; shared memory between cores; Ethernet to cell + SCADA + laptop  
**Stress:** Complexity — multiple domains, heterogeneous links, observer vs maintainer

---

## Physical topology

```text
One dual-core AMP gateway SoC on a machine cell (single PCB):

  Core0 (supervisor) — plant sequencer, Ethernet to plant switch
  Core1 (fieldbus engine) — PlantCAN, AuxCAN, SensorRS485 drivers
  Shared memory — inter-core link (not a fieldbus)

Four named fieldbuses:
  PlantCAN — 2 motor inverters
  AuxCAN — valve + hydraulic pump
  SensorRS485 — 4 polled sensors (half-duplex)
  CellLink — Ethernet to downstream cell gateway (2 remote nodes)

Plant Ethernet switch:
  Gateway (Core0), SCADA HMI (read-only), service laptop (maintenance).
```

---

## Device capabilities

| Device | Role |
|---|---|
| Core0 | 100 Hz plant sequencer; Ethernet; CellLink owner |
| Core1 | CAN + RS-485 LLL; polls RS-485; bridges fieldbuses to Core0 via shared memory |
| SCADA | Configured read-only observer |
| Service laptop | Maintenance authority when online; may be offline |
| Cell gateway | Remote handshake partner |
| Field devices | As listed per bus |

---

## Existing physical links

| Link | Type | Initiator |
|---|---|---|
| InterCore | Shared memory queue | Both cores |
| PlantCAN | Classical CAN | Core1 LLL |
| AuxCAN | Classical CAN | Core1 LLL |
| SensorRS485 | Polled half-duplex | Core1 master |
| CellLink | Ethernet WS-capable | Core0 |
| EthPlant | Ethernet | Core0 to switch |

---

## Required interactions

| Interaction | Pattern | Rate | Notes |
|---|---|---|---|
| Drive torque | Core0 → Core1 → PlantCAN | ~100 Hz | Cross-core hop |
| Aux commands | Core0 → Core1 → AuxCAN | ~50 Hz | |
| Sensor read | RS-485 poll → Core1 → Core0 | ~10 Hz poll | |
| Cell setpoints | Core0 ↔ CellLink | Event + periodic | |
| SCADA observe | Plant traffic → SCADA | Read-only tap | |
| Service FW update | Laptop → any node | On demand | Spans fieldbuses |
| Core1 link telemetry | Maintainers read Core1 health | Low rate | Distinct from Core0 |

---

## Failure and redundancy assumptions

- Single gateway MCU — no partner pair.
- Core1 hang: fieldbuses stall; Core0 detects via shared-memory heartbeat.
- Service laptop offline: plant continues; maintenance unavailable.
- SCADA loss: no effect on control.

---

## Bandwidth and timing constraints

- Drive command freshness ≤ 5 ms including Core0→Core1 hop (~0.1–0.5 ms budget on shared memory).
- RS-485 latency bounded by poll cadence (~100 ms), not plant loop.
- Distinguish communication age vs sensor sample timestamp in documentation.

---

## What this archetype does not include

- Redundant gateway pair (see archetype 03)
- Formal ASIL safety case
- Commissioning narrative detail

---

## Profile note for mappers

CellLink and EthPlant may justify **CAN29 or Ethernet-native WS profile** rather than CAN11. Use honest profile choice per link; this archetype tests heterogeneous binding rather than CAN11 alone.

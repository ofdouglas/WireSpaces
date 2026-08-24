# Archetype 03 — Dual-Controller Partitioned CAN Machine

**ID:** 03  
**Convergence test:** No  
**Suggested Link mix:** Committed CAN11 on plant buses; Ethernet between controllers  
**Stress:** Fault partitioning vs partner reachability vs redundancy — do not conflate

---

## Physical topology

```text
Two identical dual-core gateway MCUs on one machine cell:

  Gateway A — plant coordinator role in application firmware
  Gateway B — partner; reaches A's buses only via inter-gateway link

Four fieldbuses, exclusive attachment (2+2 split for hardware fault isolation):

  Gateway A only:
    PlantCAN — motor inverters (2 nodes)
    SensorRS485 — polled sensor chain (4 nodes)

  Gateway B only:
    AuxCAN — valve + hydraulic pump (2 nodes)
    CellCAN — material-handling cell controller (2 nodes)

PartnerEth — dedicated Ethernet A ↔ B (both cores' supervisor ports).

Plant Ethernet switch:
  Gateway A, Gateway B, SCADA HMI (read-only), service laptop.
```

---

## Device capabilities

| Device | Role |
|---|---|
| Gateway A Core0 | Plant sequencer; PlantCAN + SensorRS485 ownership; PlantNet authority |
| Gateway B Core0 | Local aux/cell ownership; forwards/reaches partner buses when link up |
| SCADA | Read-only plant observer on Ethernet |
| Service laptop | Maintenance when connected; may be offline |
| Field devices | As per bus attachment above |

---

## Existing physical links

| Link | Type | Owner gateway |
|---|---|---|
| PlantCAN | Classical CAN | A |
| SensorRS485 | Half-duplex RS-485, master-polled | A |
| AuxCAN | Classical CAN | B |
| CellCAN | Classical CAN | B |
| PartnerEth | Ethernet | A ↔ B |
| EthPlant | Ethernet via switch | A, B, SCADA, laptop |

---

## Required interactions

| Interaction | Pattern | Notes |
|---|---|---|
| Drive commands | A → PlantCAN nodes | High rate |
| Sensor snapshots | RS-485 poll → A | ~10 Hz poll |
| Aux commands | B → AuxCAN | |
| Cell handshake | B ↔ CellCAN | |
| Cross-partner plant visibility | A ↔ B via PartnerEth | B needs visibility/commands for A-local nodes when coordinating full cell |
| SCADA observation | Field → SCADA | Read-only |
| Service maintenance | Laptop → any node | Via appropriate gateway port on switch |

---

## Failure and redundancy assumptions

**Baseline:** PartnerEth up; A is application plant coordinator.

**PartnerEth down:** Each gateway serves **only its local two buses**. A cannot reach B-local nodes; B cannot receive plant commands for partner buses.

**Gateway A offline:** B retains AuxCAN + CellCAN. A-local buses and plant coordinator function are **gone** — not recovered by promoting B (different physical buses). Field nodes on A's buses may still electrically transmit; no role reassignment in this archetype.

**Important:** Partner link provides **reachability**, not **redundant attachment** to the same fieldbus. Do not label this "redundant drives" without alternate physical paths.

---

## Bandwidth and timing constraints

- Drive loop ≤ 5 ms budget on A.
- RS-485 delivery bounded by poll cadence ~100 ms.
- PartnerEth carries forwarded plant traffic; must not create multipath delivery storms with EthPlant for maintenance routing.

---

## What this archetype does not include

- Duplicate attachment of same CAN bus to both gateways
- Automatic controller failover
- Formal safety case

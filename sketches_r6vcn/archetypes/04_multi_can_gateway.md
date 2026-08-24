# Archetype 04 — Several CAN Buses Behind Embedded Gateway

**ID:** 04  
**Convergence test:** No  
**Suggested Link mix:** Multiple committed CAN11 segments; upstream USB or Ethernet  
**Stress:** Gateway aggregates multiple CAN plants without collapsing scope; modest dual-core split

**Stress target:** Three committed CAN11 segments → three Link-binding / WireAlias decisions. Gateway composes across segments via internal cross-core routing without treating one physical CAN cable as one semantic scope. Each CAN segment is small enough that **default VCN per binding** should suffice — this archetype tests multi-binding discipline, not VCN exhaustion.

---

## Configurations

### Config A — Bench (USB upstream)

Developer PC on USB CDC to the gateway. Used for bring-up, discovery, logs, and firmware update. Three CAN fieldbuses run production plant traffic through the gateway.

### Config B — Production (Ethernet upstream)

Same plant behavior as Config A. Upstream to plant infrastructure is **Ethernet** instead of USB. Bench USB is not connected. Wiring intent for plant CAN segments is unchanged; only the upstream Physical Link differs.

---

## Physical topology

```text
One dual-core embedded gateway MCU on a small machine (single PCB):

  Core0_Application — plant sequencer (~100 Hz), CellCAN, upstream (USB or Ethernet)
  Core1_Fieldbus    — PlantCAN and AuxCAN link-layer drivers only
  InterCore         — shared memory between cores (real hop, not a bespoke IPC API)

Three Classical CAN segments (11-bit), all terminating on the gateway:

  PlantCAN — 2 motor drives
  AuxCAN   — operator display + discrete I/O module
  CellCAN  — downstream cell gateway + 1 remote node

No CAN bridge between segments — only the gateway routes between them.

Upstream (one active at a time in baseline):
  Config A: USB CDC to developer PC (bench)
  Config B: Ethernet to plant switch (production)

Remote field devices are single-core, one Endpoint Domain each.
```

---

## CellCAN participants (reference counts)

| Segment | Participants on bus | Notes |
|---|---|---|
| PlantCAN | Gateway Core1 + 2 drives | 3 |
| AuxCAN | Gateway Core1 + display + I/O | 3 |
| CellCAN | Gateway Core0 + cell gateway + 1 remote | 3 |
| Upstream | Gateway Core0 + PC (Config A only) | 2 |

---

## Device capabilities

| Domain / device | Role |
|---|---|
| Core0_Application | Plant sequencer; CellCAN owner; upstream (USB or Eth); composes across segments |
| Core1_Fieldbus | PlantCAN + AuxCAN LLL; forwards canonical traffic to/from Core0 via InterCore |
| Motor drives (×2) | High-rate torque/status on PlantCAN |
| Display + I/O | HMI and discrete I/O on AuxCAN |
| Cell gateway + remote | Handshake with material handler on CellCAN |
| Developer PC | Bench maintenance (Config A); not a plant coordinator |

---

## Existing physical links

| Link | Type | Attachments | Config |
|---|---|---|---|
| InterCore | Shared memory queue | Core0 ↔ Core1 | A, B |
| PlantCAN | Classical CAN 11-bit | Core1 gateway + 2 drives | A, B |
| AuxCAN | Classical CAN 11-bit | Core1 gateway + display + I/O | A, B |
| CellCAN | Classical CAN 11-bit | Core0 gateway + cell GW + remote | A, B |
| UpstreamUSB | USB CDC | Core0 ↔ PC | A |
| UpstreamEth | Ethernet | Core0 ↔ plant switch | B |

---

## Required interactions

| Interaction | Path | Rate / trigger | Notes |
|---|---|---|---|
| Drive control | Core0 → Core1 → PlantCAN | ~100 Hz | Cross-core hop |
| HMI / discrete I/O | Core0 → Core1 → AuxCAN | ~10 Hz | |
| Cell coordination | Core0 ↔ CellCAN | Event + periodic | Core0 owns CellCAN |
| Cross-segment composition | Core0 internal | Plant loop | Aux I/O (e.g. enable) influences drive commands on PlantCAN |
| Bench maintenance | PC ↔ Core0 (USB) | On demand | Discovery, logs, FW update to any CAN node via gateway |
| Production upstream | Core0 ↔ Ethernet | Telemetry / SCADA-adjacent | Config B only; no separate SCADA observer archetype |
| Core1 fieldbus health | Core1 → Core0 | Low rate | Link/diagnostics for maintainer |

**No nominated plant coordinator off-gateway.** Core0 is the application authority; field devices do not command each other across segments without gateway composition.

---

## Failure and redundancy assumptions

- Single gateway MCU — loss stops all coordinated motion.
- **PlantCAN down:** AuxCAN and CellCAN may continue; no drive command path.
- **AuxCAN down:** PlantCAN and CellCAN may continue; HMI/discrete I/O unavailable.
- **CellCAN down:** Plant and Aux may continue; cell handshake unavailable.
- **Core1 hang or loss:** PlantCAN + AuxCAN stall; Core0 may still reach CellCAN and upstream until watchdog action.
- **Core0 loss:** entire plant coordination stops.
- USB and Ethernet upstream are **alternate** attachments (Config A vs B), not simultaneous redundant paths in baseline.
- PC absent (Config A): production CAN plant unaffected if PC was maintenance-only.

---

## Bandwidth and timing constraints

- Three independent CAN buses — no cross-segment bandwidth contention on the wire.
- Plant loop budget ~10 ms including Core0 → Core1 → PlantCAN round trip.
- InterCore hop budget ~0.1–0.5 ms (order-of-magnitude; not a safety case).
- CellCAN and AuxCAN are lower rate than PlantCAN.

---

## Expected diagnostic outcomes

Mappers may converge on:

- One plant-scoped Logical Bus spanning all three CAN segments **plus** forwarding through the gateway, **or**
- Separate Logical Buses per CAN segment with explicit composition at Core0.

Either is valid if segment boundaries and binding count are accounted honestly. Forcing **one Wire per cable** with gateway translation between Wires is a rejected pattern worth recording if considered.

---

## What this archetype does not include

- SCADA read-only observer (see archetype 05)
- RS-485 or polled sensor chain (see archetype 05)
- Ethernet CellLink — CellCAN stays on Classical CAN here
- Partner / redundant gateway pair (see archetype 03)
- Dual-core domains on remote field devices (see archetype 07)
- Dense peer command mesh on any CAN segment

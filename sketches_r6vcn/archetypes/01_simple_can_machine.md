# Archetype 01 — Small Single-Controller CAN Machine

**ID:** 01  
**Convergence test:** No  
**Suggested Link mix:** Committed Classical CAN11  
**Stress:** Strong fit candidate — one plant authority, several field devices on one bus

---

## Physical topology

```text
One cabinet-mounted STM32-class controller (Main MCU).
One Classical CAN bus (11-bit) inside the cabinet.
Three field devices on the bus:
  - Motor drive inverter
  - Hydraulic valve controller
  - Operator panel (buttons, mode display, estop latch status)

Optional: service laptop on CAN via USB adapter during commissioning only.
```

---

## Device capabilities

| Device | Role |
|---|---|
| Main MCU | 100 Hz plant sequencer; commands drives and aux; reads panel and device status |
| Motor drive | ~500 Hz torque/velocity loop; status and faults at ~100 Hz |
| Valve controller | ~50 Hz commands; eventful status |
| Operator panel | Event-driven button presses; periodic mode display refresh ~5 Hz |
| Service laptop (optional) | Read logs, push firmware update during maintenance — not runtime coordinator |

---

## Existing physical links

| Link | Type | Attachments |
|---|---|---|
| PlantCAN | Classical CAN 11-bit, 500 kbit/s | Main MCU, drive, valve, panel; optional laptop |

---

## Required interactions

| Interaction | Pattern | Rate / trigger | Notes |
|---|---|---|---|
| Drive torque command | Main → drive | ~100 Hz | Time-sensitive |
| Drive status / faults | Drive → Main | ~100 Hz | Main may broadcast summary to panel |
| Valve command | Main → valve | ~50 Hz | |
| Valve status | Valve → Main | ~10–50 Hz | |
| Panel button events | Panel → Main | Event | Estop is hardwired; panel sends mode requests |
| Panel display | Main → panel | ~5 Hz | |
| Maintenance logs / FW update | Laptop ↔ Main | On demand | Only when laptop connected |

No peer commands between field devices; all plant traffic is mediated by Main MCU in normal operation.

---

## Failure and redundancy assumptions

- Single controller — no hot standby in baseline.
- Loss of Main MCU stops coordinated plant motion; drives enter safe state via hardware.
- Loss of one field device does not remove Main; degraded operation with faults surfaced to panel.
- Laptop absence does not affect production.

---

## Bandwidth and timing constraints

- Plant loop budget ≤ 10 ms including CAN round-trips.
- Drive path is highest priority on the bus.
- Panel traffic is low rate; may be dropped/coalesced under load in firmware (conventional design choice).

---

## What this archetype does not include

- Second CAN bus
- Gateway to another network
- Symmetric peer ECUs
- Redundant controllers

# Sketch — Archetype 06: Overlapping Logical Scope for Bandwidth Isolation

```text
**Archetype:** archetypes/06_overlapping_wire_bandwidth.md
**Agent:** cursor_A
**Output file:** sketches/06_overlapping_wire_bandwidth_cursor_A.md
**Date:** 2026-08-23
```

---

## Executive summary

```text
**Minimum mapping:** 5 Participants, 1 Logical Bus (W1 ChassisCAN), committed CAN11 default VCN with Motion=MainA and Safety=MainB; no forwarding; Telemetry observes existing drive→Motion PDUs.
**Question A (R6):** Natural.
**Question B (CAN11):** Default.
**Worst friction (minimum path):** Configuration burden — Mild.
**Main lesson:** Observation lets Telemetry consume an existing physically visible PDU without another VCN or transmission; overlapping Wires are separate authored propagation scopes, not free traffic-class labels.
```

---

## Disposition block

| Area | Assessment |
|---|---|
| Participant identity | Natural |
| Wire decomposition | Natural |
| Forwarding | Simple |
| Non-CAN configuration | Low |
| CAN11 VCN fit | Default |
| Better with CAN29? | no |

**Explanation:** Question A: one Wire matches the single electrical CAN propagation domain. Motion and Safety are independent initiators, Telemetry is a configured observer, and loss boundaries remain those of the one physical bus.

Question B: the actual relationship graph naturally matches the two-Main default: Motion↔Safety, both drives↔Motion, both drives↔Safety, and Telemetry↔Motion. MainA/MainB are compression positions, not bus-master roles. This is a strong validation of the two-Main allocation.

---

## 1. Native communication model

Five devices share one Classical CAN11 bus:

- Motion MCU commands both drives at ~100 Hz.
- Safety MCU independently enforces estop/speed limits, commands drive limits, reads drive feedback, and coordinates mode changes with Motion.
- DriveLeft and DriveRight run local loops and publish status at ~200 Hz each.
- Telemetry MCU logs visible CAN traffic and selected aggregates.

The bus carries 400+ frames/s while safety traffic needs bounded latency. Telemetry loss must not stop motion; Motion loss triggers a safety safe state; electrical bus loss affects all devices.

---

## 2. Obvious conventional implementation

> **Obvious conventional implementation:** one CAN database with low-numbered/high-priority IDs for safety, normal-priority motion commands/status, and low-priority or throttled telemetry; the logger uses acceptance filters or promiscuous capture.

> **What structure does a conventional design use?** Per-message CAN IDs, arbitration priority, and firmware receive filters. A logger can consume a frame addressed semantically to another ECU because the medium is broadcast.

WireSpaces adds Participant identity, one named Wire, a default VCN binding, canonical source/destination, and explicit observer configuration. It does not add bandwidth.

---

## 3. Minimum mapping

### 3.1 Participants

| PID | Endpoint Domain | Device | Notes |
|---|---|---|---|
| 0x01 | MotionCtrl | Motion MCU | MainA profile position |
| 0x02 | SafetyMon | Safety MCU | MainB profile position |
| 0x03 | DriveLeft | Left inverter | Node0 |
| 0x04 | DriveRight | Right inverter | Node1 |
| 0x05 | TelemetryLog | Telemetry MCU | Node2; observer |

### 3.2 Wires

| Wire | Participants | Physical Link | Purpose |
|---|---|---|---|
| W1 ChassisCAN | 0x01–0x05 | ChassisCAN | Plant, safety, debug, and observed logging traffic |

### 3.3 Canonical interactions

| Interaction | Src | Dest | Wire | Broadcast? | Notes |
|---|---|---|---|---|---|
| Motion commands | 0x01 | 0x03 / 0x04 | W1 | No | ~100 Hz |
| Drive status | 0x03 / 0x04 | 0x01 | W1 | No | ~200 Hz; canonical destination remains Motion |
| Safety limits | 0x02 | 0x03 / 0x04 | W1 | No | QoS Critical |
| Safety feedback | 0x03 / 0x04 | 0x02 | W1 | No | Drive↔Safety relations |
| Mode coordination | 0x01 | 0x02 (and reply) | W1 | No | Motion↔Safety |
| Motion debug trace | 0x01 | 0x05 | W1 | No | Bursty/background |

### 3.4 Observation

Observation is separate from canonical destination:

| Observer | Canonical PDU observed | Additional relationship/transmission? |
|---|---|---|
| Telemetry 0x05 | DriveLeft 0x03 → Motion 0x01 on W1 / VCN 4 | No |
| Telemetry 0x05 | DriveRight 0x04 → Motion 0x01 on W1 / VCN 6 | No |
| Telemetry 0x05 | Other selected W1 traffic | No; configured observation or below-WS capture |

The observed PDU remains:

```text
Src  = Drive
Dest = Motion
Wire = W1
```

Telemetry is an additional configured consumer (`CORE §12.6`), not `Dest=Telemetry`. Observation therefore consumes neither a drive↔Telemetry VCN nor another CAN transmission.

### 3.5 Forwarding and Link profile

No forwarding: one Wire on one physical bus.

| Physical Link | Profile | Wire |
|---|---|---|
| ChassisCAN | Committed CAN11 | W1 |

### 3.6 CAN11 accounting

```text
Profile:          committed
WireAliases used:             1 / 8
  default map:                  1
  custom map:                   0

Per alias:
  Alias:                        1
  Canonical Wire:               W1
  Mapping:                      Default
  Ordinary relationships used: 6
  Total VCN values:             32 (VCN 3 reserved; 31 usable ordinary values)
  Default map sufficient?       yes
  Custom entries:               0
  MainA PID / MainB PID:         0x01 / 0x02
  Node positions used:          3 / 14
```

| VCN | Relation | Use |
|---|---|---|
| 0 | Motion → Broadcast | Available, unused |
| 1 | Safety → Broadcast | Available, unused |
| 2 | Motion ↔ Safety | Used |
| 3 | Reserved | Link control |
| 4 | DriveLeft ↔ Motion | Used |
| 5 | DriveLeft ↔ Safety | Used |
| 6 | DriveRight ↔ Motion | Used |
| 7 | DriveRight ↔ Safety | Used |
| 8 | Telemetry ↔ Motion | Used |
| 9 | Telemetry ↔ Safety | Available, unused |

Both Main positions are naturally and genuinely exercised.

---

## 4. Optional optimizations

### 4.1 Overlapping PlantSafety and Telemetry Wires

An optional decomposition could define:

```text
W1 PlantSafety   {Motion, Safety, DriveLeft, DriveRight}
W2 TelemetryLog  {Motion, DriveLeft, DriveRight, Telemetry}
```

This does **not** reclassify a W1 PDU as W2. Wire identity is canonical:

```text
Option 1 — chosen minimum:
  Drive → Motion on W1
  Telemetry observes W1 PDU
  one CAN transmission

Option 2 — separate W2 publication:
  Drive → Motion on W1
  Drive → Telemetry/broadcast on W2
  two canonical PDUs and two CAN transmissions
```

Option 2 can be valid when W2 carries genuinely different authored aggregates or logging objects. It is counterproductive for duplicating the existing drive-status PDU: it adds a WireAlias/binding, another transmission, and more bus load while safety latency is already the concern.

### 4.2 Directed drive↔Telemetry relations

An Explicit map could add DriveLeft↔Telemetry and DriveRight↔Telemetry. This improves explicit destination auditability but adds two relations and duplicate transmissions. Observation is cheaper and faithful to the broadcast medium.

### 4.3 QoS separation

Keep W1; assign safety limits QoS Critical and telemetry/debug QoS Background. This is the appropriate mechanism for arbitration priority. A Wire split cannot create electrical bandwidth.

---

## 5. Friction signals

| Signal | Rating | Justification |
|---|---|---|
| Artificial Wire | None | W1 is the real shared propagation domain. |
| Wire proliferation | None | One Wire on the minimum path. |
| Artificial hierarchy | None | Motion and Safety naturally occupy the two independently initiating Main positions; neither becomes a network master. |
| VCN pressure | None | Six relationships; ample capacity. |
| WireAlias pressure | None | One alias. |
| Configuration burden | Mild | Default role-position assignments plus explicit observer configuration. |
| Failure/topology mismatch | None | One electrical failure domain remains explicit. |

---

## 6. Model pressure

- **If one concept changed:** no protocol change. Documentation should state clearly that overlapping Wires are separately authored propagation scopes, not traffic classes or alternate subscription labels.
- **Useful distinction:** canonical destination and configured observation are independent. Several consumers can use one physically visible CAN PDU without adding pair relationships or transmissions.

**Synthesis lesson:**

> When several consumers need the same physically visible CAN PDU, observation can be cheaper than additional relationships or overlapping Wires. Wires describe propagation scope; they are not filtered views of another Wire.

---

## 7. Open questions

- Should Telemetry observation be configured as selected Endpoint/VCN filters or implemented as a below-WS promiscuous capture facility?
- Does Safety consume the same drive→Motion status via observation, or require independently authored drive→Safety feedback for its assurance needs? The minimum assumes the latter.
- Are separately authored telemetry aggregates valuable enough to justify W2 despite the extra CAN transmission?

---

## 8. Spec findings

No new protocol feature is needed. Documentation should explicitly distinguish:

```text
observation of one canonical PDU
    != duplicate publication on another Wire
```

---

## 9. Diagram

```text
W1 ChassisCAN / committed CAN11

Motion 0x01 (MainA)  <------>  Safety 0x02 (MainB)
      |        \                 /        |
      |         \               /         |
 DriveLeft 0x03                 DriveRight 0x04
      \                              /
       \-- existing status PDUs ----/
                  |
                  +--> Telemetry 0x05 observes

No second Wire. No duplicate status transmission.
```

# Sketch — Archetype 01: Small Single-Controller CAN Machine

```text
**Archetype:** archetypes/01_simple_can_machine.md
**Agent:** cursor
**Date:** 2026-08-23
```

---

## Executive summary

```text
**Minimum mapping:** 5 Participants (4 always present + optional laptop), 1 Logical Bus (PlantCAN) on committed CAN11; no forwarding.
**Question A (R6):** Natural.
**Question B (CAN11):** Default.
**Worst friction (minimum path):** Artificial hierarchy — None.
**Main lesson:** R6 + default VCN does not make a simple controller/leaf CAN machine worse — but this sketch only exercises MainA and four Node slots; it does not stress MainB, peer relations, or the two-Main allocation as a general design.
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

**Explanation:** Question A: one cabinet, one CAN bus, one plant communication scope — a single Logical Bus with deployment-global ParticipantIds matches the physical model directly. Main MCU mediates all field traffic; no gateway, no overlapping scopes, no peer commands. Participant count equals device count (one Endpoint Domain each); roles are stable.

Question B: committed CAN11 with the overlay default VCN map covers all required relations using MainA only (Main MCU) and four Node positions (drive, valve, panel, laptop). Four ordinary VCNs of 32 suffice; one WireAlias binds the single Wire. VCN 0 (MainA broadcast) is available in the default map but unused in the minimum mapping. MainB is left unassigned — odd VCN slots stay unused, which is honest configuration, not a custom table. CAN29 would add identifier space with no benefit here.

---

## 1. Native communication model

One cabinet-mounted STM32-class controller (Main MCU) runs a ~100 Hz plant sequencer. It commands a motor drive inverter and a hydraulic valve controller, reads their status, and exchanges mode/button events and display updates with an operator panel. All devices share one Classical CAN bus at 500 kbit/s inside the cabinet.

The motor drive runs an internal ~500 Hz torque/velocity loop but exposes commands and status to Main at ~100 Hz. The valve controller accepts ~50 Hz commands and reports eventful status at ~10–50 Hz. The panel sends event-driven button presses and receives ~5 Hz display refreshes. Estop is hardwired; the panel sends mode requests only.

No field device commands another field device in normal operation — all plant traffic is mediated by Main MCU. During maintenance, an optional service laptop may attach to the same CAN bus via a USB adapter to read logs and push firmware updates to Main; the laptop is not a runtime coordinator and is absent in production.

Loss of Main MCU stops coordinated plant motion (drives enter hardware safe state). Loss of one field device leaves Main running with degraded operation and faults surfaced to the panel. Laptop absence does not affect production.

---

## 2. Obvious conventional implementation

> **Obvious conventional implementation:** Fixed 11-bit CAN message IDs per signal or per device–controller pair; Main MCU firmware maps opcodes to CAN IDs and demultiplexes inbound frames by ID; optional priority via lower CAN IDs for drive traffic.

> **What structure does a conventional design use?** Per-link CAN ID allocation table (source/dest or request/response pairs), signal dictionaries, and a periodic scheduler in Main firmware. No explicit "bus" abstraction beyond the physical CAN segment — membership is implicit (everyone hears everything electrically).

WireSpaces adds: named WireNumber, deployment-global ParticipantIds, Link Binding with WireAlias and VCN relation table (even if default-generated), and canonical Src/Dest on every PDU. For this archetype that is a modest, auditable layer over what the gateway firmware would already encode implicitly.

---

## 3. Minimum mapping (required first)

### 3.1 Participants

| ParticipantId | Endpoint Domain | Device | Notes |
|---|---|---|---|
| 0x01 | MainApp | Main MCU (STM32-class) | Plant sequencer; sole production coordinator |
| 0x02 | DriveCtrl | Motor drive inverter | Accepts torque commands; publishes status/faults |
| 0x03 | ValveCtrl | Hydraulic valve controller | ~50 Hz command path |
| 0x04 | PanelUI | Operator panel | Buttons in, display out |
| 0x05 | SvcLaptop | Service laptop (optional) | Maintenance only; may be absent at runtime |

One Endpoint Domain per device. Laptop is modeled as a Participant so maintenance traffic has canonical identity. **Configured Wire membership** (who belongs to W1 in the deployment) is separate from **physical presence/reachability** (whether the laptop is plugged in). Two valid deployment choices: (a) preconfigure 0x05 as a W1 member with Node3 bindings installed, accepting that it may be unreachable when absent; or (b) use a maintenance-only configuration variant that includes 0x05. Wire membership does not change at runtime when the cable is unplugged.

### 3.2 Wires

| Wire (#) | Participants | Physical links | Purpose |
|---|---|---|---|
| W1 PlantCAN | 0x01, 0x02, 0x03, 0x04, 0x05 | PlantCAN (Classical CAN 500 kbit/s) | All plant and maintenance traffic on the cabinet bus; 0x05 configured member, physically absent in production |

One Logical Bus matches one propagation domain: every device on the cabinet CAN segment participates in the same plant communication scope. Main mediates field traffic; maintenance is Laptop ↔ Main on the same bus — not a separate propagation scope warranting a second Wire in the minimum mapping.

### 3.3 Interactions (primary)

| Interaction | Src | Dest | Wire | Broadcast? | Notes |
|---|---|---|---|---|---|
| Drive torque command | 0x01 | 0x02 | W1 | No | ~100 Hz; QoS High or Critical on egress |
| Drive status / faults | 0x02 | 0x01 | W1 | No | ~100 Hz |
| Valve command | 0x01 | 0x03 | W1 | No | ~50 Hz |
| Valve status | 0x03 | 0x01 | W1 | No | ~10–50 Hz |
| Panel button events | 0x04 | 0x01 | W1 | No | Event-driven |
| Panel display refresh | 0x01 | 0x04 | W1 | No | ~5 Hz; coalescable under load |
| Maintenance logs / FW update | 0x05 ↔ 0x01 | 0x01 ↔ 0x05 | W1 | No | On demand; laptop present only |

Optional: Main → broadcast drive-status summary for panel consumption could use `Dest = kBroadcast` on W1 instead of Main absorbing status and sending a directed panel update. Minimum mapping uses directed paths only; the archetype allows either conventional coalescing in Main or a broadcast summary.

No peer interactions between 0x02, 0x03, 0x04, or 0x05.

### 3.4 Forwarding (if any)

None. Single physical CAN link, single Wire, no gateway.

### 3.5 Link profiles

| Physical link | Profile | WS Wire(s) | Notes |
|---|---|---|---|
| PlantCAN | Committed CAN11 | W1 | 500 kbit/s Classical CAN; archetype suggested profile |

### 3.6 CAN11 bindings (if CAN11 used)

**PlantCAN — committed CAN11**

```text
Profile:          committed
WireAliases used:             1 / 8
  default map:                  1
  custom map:                   0

Per alias:
  Alias:                        1
  Canonical Wire:               W1 (PlantCAN)
  Mapping:                      Default
  Ordinary VCNs used:           4 / 32   (minimum mapping only)
  VCNs available, unused:       VCN 0; VCN 2; odd MainB slots 5,7,9,11
  Default map sufficient?       yes
  Custom entries (if Explicit): 0
  MainA PID / MainB PID:         0x01 / (unassigned)
  Node positions used:          4 / 14
```

**VCN assignments (default map, MainA = 0x01):**

| VCN | Relation | Status in minimum mapping |
|---|---|---|
| 0 | 0x01 → kBroadcast | Available by default; **unused** |
| 3 | — | Reserved Link control |
| 4 | 0x02 ↔ 0x01 | **Used** — drive ↔ Main |
| 6 | 0x03 ↔ 0x01 | **Used** — valve ↔ Main |
| 8 | 0x04 ↔ 0x01 | **Used** — panel ↔ Main |
| 10 | 0x05 ↔ 0x01 | **Used** — laptop ↔ Main (maintenance) |

Odd VCNs (5, 7, 9, 11) for Node↔MainB are unused because MainB is unassigned. QoS differentiation (e.g. drive commands at QoS Critical) uses the 2-bit QoS field within the same alias binding, not extra VCNs or aliases.

**Guest CAN11:** Not used. No legacy coexistence requirement in the archetype; the entire bus is WireSpaces-governed committed CAN11.

---

## 4. Optional optimizations

Only documented if they solve a concrete problem the minimum mapping lacks.

### 4.1 QoS via frame bits (not a topology change)

Drive command/status can use QoS Critical/High within the existing VCN 4 binding. **Problem solved:** bus arbitration priority for the highest-rate plant path. **Minimum mapping gap:** none — QoS is available without extra Wires or aliases.

### 4.2 Separate maintenance Wire + Guest allocation — **not specifiable today**

If the physical bus must carry legacy non-WS CAN traffic alongside WireSpaces, one might want a Guest block for a narrow maintenance Wire while committed CAN11 carries W1 on the **same** physical CAN segment. **Problem it would solve:** legacy coexistence with a separate maintenance scope.

**Blocked:** the overlay and `LINK` do not define a classifier that lets one physical CAN11 bus simultaneously interpret some IDs as committed format and an allocated range as Guest format. Mixed-profile coexistence on one bus is an unstated assumption — not a valid optimization under current governed docs. Omit unless a future mixed-profile mechanism is specified.

### 4.3 Narrow PlantStatus Wire overlapping PlantCAN

A second Logical Bus W2 ⊂ {0x01, 0x02, 0x03, 0x04} for broadcast telemetry only, sharing PlantCAN via a second committed binding (WireAlias 2 → W2). **Problem solved:** isolate broadcast bandwidth from directed control VCNs. **Minimum mapping gap:** panel and valve rates are low; 500 kbit/s with five relations does not justify a second Wire or alias in this archetype.

---

## 5. Friction signals (minimum mapping happy path)

| Signal | None / Mild / Significant | One-sentence justification |
|---|---|---|
| Artificial Wire | None | W1 is the cabinet CAN plant bus — one meaningful propagation domain. |
| Wire proliferation | None | One Wire for all production and maintenance on this segment. |
| Artificial hierarchy | None | MainA maps to the real plant coordinator; MainB is simply unused, not a fictitious second authority. |
| VCN pressure | None | Four required Main↔Node relations fit comfortably in the default map with four Node slots; VCN 0 broadcast unused. |
| WireAlias pressure | None | One alias for one Wire on one bus. |
| Configuration burden | Mild | ParticipantIds, one default binding, and four Node position assignments are small but more explicit than a flat CAN ID spreadsheet. |
| Failure/topology mismatch | None | Single Wire failure domain matches single CAN segment; Main loss is visible as loss of all directed paths to MainA. |

---

## 6. Model pressure

- **If you could change one WS concept** to simplify this system, what would it be? Allowing a "single-Main" profile that omits MainB/odd-VCN slots from generated configuration views — cosmetic only; the map already works with MainB unassigned.
- **Did WS expose a useful distinction** the conventional model obscures? Yes: separating electrical visibility (all devices hear the bus) from configured Wire membership and directed vs broadcast acceptance (`CORE §3.2`, `§12.6`) makes the "Main mediates everything" rule explicit without pretending field devices are isolated.

**Synthesis note (Archetype 01 scope):** This sketch supports *R6 + default VCN does not make a simple controller/leaf machine worse*; it does **not** yet test whether the two-Main default VCN allocation is the right general design — MainB, peer relations, and broadcast VCN 0 are all absent or unused. Harder archetypes should answer that.

---

## 7. Open questions

- Laptop deployment variant: preconfigure 0x05 as a permanent W1 member (Node3 bindings always installed) vs a maintenance-only configuration export that adds 0x05 — affects whether Node position 3 is consumed in the production manifest.
- Panel display under bus load: coalesce in Main firmware (conventional) vs broadcast summary on VCN 0 — both valid; Service binding choice, not topology.
- Commissioning narrative (Unconfigured → Committed) omitted per experiment scope; pre-addressed Node slots assumed.

---

## 8. Spec findings (optional)

| ID | Finding |
|---|---|
| SF-R6-002 | Mixed Guest + committed CAN11 on the **same** physical bus lacks a specified ingress classifier (overlay §13). Not a valid optimization under current trial docs. |

---

## 9. Diagrams (optional)

### Device-centric

```text
┌─────────────────────────────────────────────────────────────┐
│  Cabinet                                                     │
│                                                              │
│  ┌──────────┐   PlantCAN (CAN11 committed, 500 kbit/s)     │
│  │ Main MCU │─── W1 PlantCAN ─────────────────────────────┐ │
│  │ PID 0x01 │    MainA                                     │ │
│  └──────────┘                                               │ │
│       │ directed / optional broadcast                       │ │
│       ├──────────┬──────────┬──────────┐                   │ │
│       │          │          │          │                   │ │
│  ┌────▼───┐ ┌────▼───┐ ┌────▼───┐ ┌────▼────┐ (optional)  │ │
│  │ Drive  │ │ Valve  │ │ Panel  │ │ Laptop  │              │ │
│  │ 0x02   │ │ 0x03   │ │ 0x04   │ │ 0x05    │              │ │
│  │ Node0  │ │ Node1  │ │ Node2  │ │ Node3   │              │ │
│  └────────┘ └────────┘ └────────┘ └─────────┘              │ │
│                                                              │ │
└──────────────────────────────────────────────────────────────┘ │
```

### Wire-centric (minimum mapping)

```text
Wire W1 PlantCAN (committed CAN11, Alias 1, Default VCN)
├── Participant 0x01  Main MCU     (MainA)
├── Participant 0x02  Motor drive  (Node0)  VCN 4
├── Participant 0x03  Valve          (Node1)  VCN 6
├── Participant 0x04  Panel          (Node2)  VCN 8
└── Participant 0x05  Laptop       (Node3)  VCN 10  [configured; often unreachable]

No gateway. No forwarding. No Guest.
VCN 0 (broadcast) available in default map but unused.
```

### Rejected alternative (physical-link decomposition)

| Approach | Wires | Issue |
|---|---|---|
| **Chosen:** scope-shaped PlantCAN | 1 | Matches plant propagation domain |
| Rejected: one Wire per device pair | 3–4 | Wire proliferation; no separate scopes |
| Rejected: USB Wire + CAN Wire for laptop | 2 | Laptop attaches to CAN, not a second cable; would invent a tunnel |

---

## Devices (reference)

| Device | Endpoint Domains | Link interfaces | Role summary |
|---|---|---|---|
| Main MCU | MainApp | PlantCAN | Plant sequencer; MainA on W1 |
| Motor drive | DriveCtrl | PlantCAN | Node0; torque loop local, WS at ~100 Hz |
| Valve controller | ValveCtrl | PlantCAN | Node1 |
| Operator panel | PanelUI | PlantCAN | Node2 |
| Service laptop | SvcLaptop | PlantCAN (USB-CAN adapter) | Node3; maintenance only |

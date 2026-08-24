# Sketch — Archetype 03: Dual-Controller Partitioned CAN Machine

```text
**Archetype:** archetypes/03_dual_controller_can.md
**Agent:** cursor
**Date:** 2026-08-23
```

---

## Executive summary

```text
**Minimum mapping:** 14 Participants, 2 Logical Buses (W1 PlantNet spanning five heterogeneous links with gateway forwarding; W2 EthPlant on the plant switch), committed CAN11 on three CAN segments.
**Question A (R6):** Natural.
**Question B (CAN11):** Default.
**Worst friction (minimum path):** Configuration burden — Moderate.
**Main lesson:** A spanning PlantNet Wire with per-gateway forwarding cleanly models partitioned fieldbuses plus partner reachability without conflating fault isolation with redundancy — but the forwarding tables and three independent CAN11 bindings are the real cost, not VCN allocation.
```

---

## Disposition block

| Area | Assessment |
|---|---|
| Participant identity | Natural |
| Wire decomposition | Natural |
| Forwarding | Moderate |
| Non-CAN configuration | Moderate |
| CAN11 VCN fit | Default |
| Better with CAN29? | no |

**Explanation:** Question A: the archetype has two distinct scopes — plant field coordination (spanning four exclusive fieldbuses plus an inter-gateway link) and Ethernet-side observation/maintenance. One authority-shaped PlantNet Wire (`W1`) realized across heterogeneous Links with forwarding at each gateway matches how the cell actually coordinates; a separate EthPlant Wire (`W2`) for SCADA and the service laptop avoids multipath between PartnerEth and the plant switch. Deployment-global ParticipantIds are stable across both Wires for the gateways. PartnerEth loss partitions `W1` into A-local and B-local segments without role reassignment — the model describes that honestly.

Question B: each Classical CAN segment uses committed CAN11 with the default VCN map — the bus-owning gateway as MainA and two Node positions per bus. Six ordinary VCNs across three buses (two per bus); one WireAlias per physical CAN bus binding the same canonical Wire `W1`. MainB is unassigned on all three buses; two-Main default allocation is not exercised. CAN29 would add identifier space with no benefit at two nodes per bus.

---

## 1. Native communication model

One machine cell has two identical dual-core gateway MCUs with a **2+2 exclusive fieldbus split** for hardware fault isolation:

- **Gateway A** owns PlantCAN (two motor inverters, high-rate drive loop ≤ 5 ms) and SensorRS485 (four polled sensors, ~10 Hz).
- **Gateway B** owns AuxCAN (hydraulic valve + pump) and CellCAN (two material-handling cell controllers, bidirectional handshake).

A dedicated **PartnerEth** link connects Gateway A and Gateway B supervisor ports. A shared **EthPlant** switch connects both gateways, a read-only SCADA HMI, and an intermittently connected service laptop.

**Gateway A** is the application plant coordinator. **Gateway B** coordinates its local aux and cell buses and needs cross-partner visibility and commands for A-local nodes when orchestrating the full cell.

Traffic patterns:

- A commands and reads two motor inverters on PlantCAN at high rate.
- A master-polls four sensors on RS-485; delivery bounded by ~100 ms poll cadence.
- B commands aux devices and exchanges handshake traffic with cell controllers.
- A and B exchange coordination and cross-partner plant traffic over PartnerEth when the link is up.
- SCADA observes plant state read-only via Ethernet.
- The service laptop, when connected, performs maintenance on any node via the appropriate gateway port on the switch.

**Failure assumptions (baseline: PartnerEth up, A is coordinator):**

- **PartnerEth down:** each gateway serves only its two local buses; no cross-partner reachability.
- **Gateway A offline:** B retains AuxCAN and CellCAN; A-local buses and coordinator function are gone — not recovered by promoting B (different physical buses).
- Partner link provides **reachability**, not redundant attachment to the same fieldbus.

---

## 2. Obvious conventional implementation

> **Obvious conventional implementation:** Per-bus CAN ID tables owned by each gateway; RS-485 Modbus or custom polled protocol on A; a private A↔B application protocol over PartnerEth for cross-partner commands and status mirroring; SCADA reads mirrored tags from A and/or B over OPC-UA/Ethernet; maintenance tools connect to gateway REST/CAN adapters on the plant switch.

> **What structure does a conventional design use?** Four independent fieldbus stacks, two gateway firmware images with explicit partner-bridge logic, per-link ID/signal dictionaries, and implicit "who forwards what to whom" encoded in gateway application code rather than named Logical Buses.

WireSpaces adds: deployment-global ParticipantIds, two named Logical Buses with explicit membership, per-Link Bindings (three CAN11 default VCN maps + RS-485 + Ethernet profiles), gateway forwarding tables preserving canonical Src/Dest across Links, and auditable observation taps for SCADA. The partner-bridge behavior that conventional firmware hides inside a socket handler becomes explicit forwarding policy on `W1`.

---

## 3. Minimum mapping (required first)

### 3.1 Participants

| ParticipantId | Endpoint Domain | Device | Notes |
|---|---|---|---|
| 0x01 | GwAPlant | Gateway A (Core0) | Plant coordinator; PlantCAN + SensorRS485 owner; `W1` + `W2` member |
| 0x02 | GwBPlant | Gateway B (Core0) | Aux/cell owner; partner reachability; `W1` + `W2` member |
| 0x03 | MotorInv1 | Motor inverter 1 | PlantCAN |
| 0x04 | MotorInv2 | Motor inverter 2 | PlantCAN |
| 0x05 | Sensor1 | RS-485 sensor 1 | Polled by 0x01 |
| 0x06 | Sensor2 | RS-485 sensor 2 | Polled by 0x01 |
| 0x07 | Sensor3 | RS-485 sensor 3 | Polled by 0x01 |
| 0x08 | Sensor4 | RS-485 sensor 4 | Polled by 0x01 |
| 0x09 | ValveCtrl | Hydraulic valve controller | AuxCAN |
| 0x0A | HydPump | Hydraulic pump controller | AuxCAN |
| 0x0B | CellCtrl1 | Cell controller 1 | CellCAN |
| 0x0C | CellCtrl2 | Cell controller 2 | CellCAN |
| 0x0D | ScadaHMI | SCADA HMI | Read-only observer; `W2` only |
| 0x0E | SvcLaptop | Service laptop | Maintenance; `W2` only; may be absent |

One Endpoint Domain per device. Gateways are the only Participants on both Wires.

**Membership vs presence:** SCADA (0x0D) and the service laptop (0x0E) are **preconfigured** `W2` members. When the laptop is unplugged, Wiring is unchanged; delivery to 0x0E fails or queues drain — they are not removed from Wire membership at runtime. Same for SCADA if powered off (unusual but valid).

### 3.2 Wires

| Wire (#) | Participants | Physical links | Purpose |
|---|---|---|---|
| W1 PlantNet | 0x01–0x0C | PlantCAN, SensorRS485, AuxCAN, CellCAN, PartnerEth | Plant field coordination spanning partitioned buses; cross-partner reachability |
| W2 EthPlant | 0x01, 0x02, 0x0D, 0x0E | EthPlant (Ethernet switch) | SCADA observation and service maintenance; no direct field-node attachment |

**W1** is the plant propagation domain — one Logical Bus spanning heterogeneous Links through gateway forwarding (`CORE §3.5`). It matches the archetype's "full cell coordination" scope without treating each cable as a separate semantic network.

**W2** is the plant-Ethernet scope for host-side participants. Field nodes are **not** `W2` members; maintenance and SCADA reach plant nodes through gateway forwarding/composition from `W2` → `W1`, avoiding multipath delivery of the same `W1` PDU on both PartnerEth and EthPlant.

### 3.3 Interactions (primary)

| Interaction | Src | Dest | Wire | Broadcast? | Notes |
|---|---|---|---|---|---|
| Drive torque commands | 0x01 | 0x03, 0x04 | W1 | No | High rate; ≤ 5 ms loop budget; QoS Critical on egress |
| Drive status / faults | 0x03, 0x04 | 0x01 | W1 | No | High rate |
| Sensor poll request | 0x01 | 0x05–0x08 | W1 | No | Master-initiated RS-485; ~10 Hz |
| Sensor snapshot response | 0x05–0x08 | 0x01 | W1 | No | Bounded by poll cadence ~100 ms |
| Aux valve/pump commands | 0x02 | 0x09, 0x0A | W1 | No | |
| Aux status | 0x09, 0x0A | 0x02 | W1 | No | |
| Cell handshake | 0x02 ↔ 0x0B, 0x0C | W1 | No | Bidirectional |
| Partner coordination | 0x01 ↔ 0x02 | W1 | No | Over PartnerEth |
| Cross-partner command to A-local node | 0x02 | 0x03, 0x04, 0x05–0x08 | W1 | No | Forwarded A←B via PartnerEth when link up |
| Cross-partner visibility of B-local | 0x09–0x0C | 0x01 | W1 | No | Observation / forwarded status to A |
| SCADA plant observation | 0x01, 0x02 | 0x0D | W2 | No | Gateway-composed or tapped telemetry summaries; read-only acceptance at SCADA |
| Service maintenance | 0x0E ↔ 0x01, 0x02 | W2 | No | Laptop ↔ gateway; gateway forwards/relays to `W1` targets as needed |
| FW update to field node | 0x0E → 0x01 or 0x02 → field | W2 then W1 | No | Reliable segment on upstream hop; gateway-mediated |

No field device commands another field device outside gateway mediation in normal operation. No automatic coordinator promotion on A loss.

### 3.4 Forwarding (if any)

**Gateway A — `W1` forwarding**

| Ingress | Wire | Egress | Notes |
|---|---|---|---|
| PlantCAN LI | W1 | PartnerEth LI | Local delivery to 0x01, 0x03, 0x04; forward to B branch |
| SensorRS485 LI | W1 | PartnerEth LI | Polled segment; forward sensor responses toward B if configured |
| PartnerEth LI | W1 | PlantCAN LI, SensorRS485 LI | B-originated PDUs toward A-local participants |
| EthPlant LI | W2 | W2 local | SCADA/laptop traffic; `W1`→`W2` observation tap for plant telemetry |

**Gateway B — `W1` forwarding**

| Ingress | Wire | Egress | Notes |
|---|---|---|---|
| AuxCAN LI | W1 | PartnerEth LI | |
| CellCAN LI | W1 | PartnerEth LI | |
| PartnerEth LI | W1 | AuxCAN LI, CellCAN LI | A-originated PDUs toward B-local participants |
| EthPlant LI | W2 | W2 local | Symmetric to A for SCADA/maintenance |

**Multipath avoidance:** `W1` is **not** bound to EthPlant. Plant PDUs traverse PartnerEth between gateways, not the plant switch. EthPlant carries only `W2` traffic plus gateway-authored observation summaries/taps derived from `W1` ingress — one deliberate path per plant PDU scope.

**PartnerEth down:** disable PartnerEth egress rows in both gateways' `W1` tables. A-local and B-local `W1` segments operate independently; cross-partner interactions fail closed.

**Gateway A offline:** B's `W1` forwarding continues on AuxCAN + CellCAN + PartnerEth (partner side idle); A-local PlantCAN/RS-485 participants unreachable; no B promotion to A coordinator role.

### 3.5 Link profiles

| Physical link | Profile | WS Wire(s) | Notes |
|---|---|---|---|
| PlantCAN | Committed CAN11 | W1 | 500 kbit/s Classical CAN; Gateway A MainA |
| SensorRS485 | RS-485 master-polled (`CORE §1.7`) | W1 | Half-duplex; A initiates all transactions |
| AuxCAN | Committed CAN11 | W1 | Gateway B MainA |
| CellCAN | Committed CAN11 | W1 | Gateway B MainA |
| PartnerEth | WS over Ethernet datagram | W1 | Point-to-point A ↔ B; not on plant switch |
| EthPlant | WS over Ethernet datagram | W2 | Shared switch; A, B, SCADA, laptop |

Non-CAN profiles carry full canonical WireNumber and ParticipantIds. RS-485 polling cadence is an LLL/driver concern; canonical PDUs still name Src/Dest on `W1`.

### 3.6 CAN11 bindings (if CAN11 used)

Each physical CAN bus has one committed binding to canonical Wire `W1`. VCN maps are scoped per WireAlias binding (overlay §3). MainB is unassigned on all buses — **two-Main default allocation is not exercised** by this sketch.

#### PlantCAN — committed CAN11

```text
Profile:          committed
WireAliases used:             1 / 8
  default map:                  1
  custom map:                   0

Per alias:
  Alias:                        1
  Canonical Wire:               W1 (PlantNet)
  Mapping:                      Default
  Ordinary VCNs used:           2 / 32   (minimum mapping only)
  VCNs available, unused:       VCN 0; VCN 2; odd MainB slots 5, 7
  Default map sufficient?       yes
  Custom entries (if Explicit): 0
  MainA PID / MainB PID:         0x01 / (unassigned)
  Node positions used:          2 / 14
```

| VCN | Relation | Status in minimum mapping |
|---|---|---|
| 0 | 0x01 → kBroadcast | Available by default; **unused** |
| 3 | — | Reserved Link control |
| 4 | 0x03 ↔ 0x01 | **Used** — MotorInv1 ↔ Gateway A |
| 6 | 0x04 ↔ 0x01 | **Used** — MotorInv2 ↔ Gateway A |

#### AuxCAN — committed CAN11

```text
Profile:          committed
WireAliases used:             1 / 8
  default map:                  1
  custom map:                   0

Per alias:
  Alias:                        1
  Canonical Wire:               W1 (PlantNet)
  Mapping:                      Default
  Ordinary VCNs used:           2 / 32
  VCNs available, unused:       VCN 0; VCN 2; odd MainB slots 5, 7
  Default map sufficient?       yes
  Custom entries (if Explicit): 0
  MainA PID / MainB PID:         0x02 / (unassigned)
  Node positions used:          2 / 14
```

| VCN | Relation | Status in minimum mapping |
|---|---|---|
| 0 | 0x02 → kBroadcast | Available by default; **unused** |
| 3 | — | Reserved Link control |
| 4 | 0x09 ↔ 0x02 | **Used** — ValveCtrl ↔ Gateway B |
| 6 | 0x0A ↔ 0x02 | **Used** — HydPump ↔ Gateway B |

#### CellCAN — committed CAN11

```text
Profile:          committed
WireAliases used:             1 / 8
  default map:                  1
  custom map:                   0

Per alias:
  Alias:                        1
  Canonical Wire:               W1 (PlantNet)
  Mapping:                      Default
  Ordinary VCNs used:           2 / 32
  VCNs available, unused:       VCN 0; VCN 2; odd MainB slots 5, 7
  Default map sufficient?       yes
  Custom entries (if Explicit): 0
  MainA PID / MainB PID:         0x02 / (unassigned)
  Node positions used:          2 / 14
```

| VCN | Relation | Status in minimum mapping |
|---|---|---|
| 0 | 0x02 → kBroadcast | Available by default; **unused** |
| 3 | — | Reserved Link control |
| 4 | 0x0B ↔ 0x02 | **Used** — CellCtrl1 ↔ Gateway B |
| 6 | 0x0C ↔ 0x02 | **Used** — CellCtrl2 ↔ Gateway B |

**Guest CAN11:** Not used. No legacy coexistence requirement; each CAN segment is fully WireSpaces-governed committed CAN11.

**Cross-bus note:** Gateway B appears as MainA on both AuxCAN and CellCAN bindings — two separate physical buses, two separate aliases, same canonical Wire `W1`. This is correct: VCN 4 on AuxCAN names `{0x09, 0x02}`; VCN 4 on CellCAN names `{0x0B, 0x02}` — scoped by binding, not globally ambiguous.

---

## 4. Optional optimizations

Only after §3; each solves a problem the minimum mapping lacks.

### 4.1 Per-bus Logical Buses (rejected alternative — documented for comparison)

Separate Wires: `W1a` PlantCAN, `W1b` SensorBus, `W1c` AuxCAN, `W1d` CellCAN, plus `W1e` PartnerBridge `{0x01, 0x02}` only.

**Problem it would solve:** simpler per-bus Link Bindings without a spanning Wire.

**Why rejected:** cross-partner commands from B to A-local nodes (0x02 → 0x03) require either (a) a spanning plant Wire with forwarding, or (b) composition at B where B authors coordination messages to A and A re-originates drive commands — losing direct canonical visibility. The archetype's "visibility/commands for A-local nodes" favors preserving Src/Dest on `W1`. Five Wires vs two adds Wire proliferation without clearer semantics.

### 4.2 Overlapping drive-status Wire on PlantCAN

Second Logical Bus `W3` ⊂ {0x01, 0x03, 0x04} for high-rate status only, sharing PlantCAN via WireAlias 2.

**Problem solved:** isolate broadcast/summary bandwidth from directed command VCNs.

**Minimum mapping gap:** two inverters on 500 kbit/s with QoS-bit differentiation on existing VCNs 4 and 6 suffices for the archetype's ≤ 5 ms drive loop; no measured bus pressure.

### 4.3 Split EthPlant into ScadaWire + MaintWire

`W2a` {0x01, 0x02, 0x0D} and `W2b` {0x01, 0x02, 0x0E}.

**Problem solved:** isolate large FW transfers from SCADA refresh latency.

**Minimum mapping gap:** plant switch Ethernet has ample bandwidth; laptop is intermittent.

### 4.4 Bind `W1` to EthPlant (multipath — rejected)

Attach `W1` to EthPlant in addition to PartnerEth so SCADA receives raw plant PDUs without gateway tap.

**Rejected:** creates dual paths for inter-gateway `W1` traffic (PartnerEth + switch) — violates archetype multipath constraint. Minimum mapping uses observation tap / composed summaries on `W2` only.

---

## 5. Friction signals (minimum mapping happy path)

| Signal | None / Mild / Significant | One-sentence justification |
|---|---|---|
| Artificial Wire | None | `W1` is the real plant coordination scope; `W2` is the real Ethernet host scope — both match physical relationships. |
| Wire proliferation | None | Two Wires for two distinct propagation domains; not one Wire per cable. |
| Artificial hierarchy | Mild | MainA on each CAN bus maps to the bus-owning gateway — natural, though B is MainA on two separate buses. |
| VCN pressure | None | Two Node relations per bus; six ordinary VCNs total across three buses with comfortable headroom. |
| WireAlias pressure | Mild | Three aliases (one per CAN bus) for the same canonical Wire — expected for heterogeneous gatewaying, not arbitrary. |
| Configuration burden | Moderate | Fourteen ParticipantIds, two Wire memberships, three CAN11 default bindings, RS-485 profile, two Ethernet bindings, and ingress-sensitive forwarding tables at both gateways — auditable but substantially more explicit than four independent CAN ID spreadsheets. |
| Failure/topology mismatch | Mild | PartnerEth partition is modeled cleanly; A-loss is honest (no failover fiction); RS-485 master-initiated segment behaves differently from CAN push — visible in LLL, not hidden. |

---

## 6. Model pressure

- **If you could change one WS concept** to simplify this system, what would it be? A concise gateway-forwarding table DSL or template for "partitioned fieldbuses + single partner link" archetypes — the semantics are natural but the tabular authoring burden is the main cost.
- **Did WS expose a useful distinction** the conventional model obscures? Yes: separating **reachability** (PartnerEth forwarding on `W1`) from **redundant attachment** (explicitly absent) and separating **configured Wire membership** from **runtime reachability** when PartnerEth or Gateway A fails (`CORE §3.2`, `§12.6`) — the conventional "mirror tags over a socket" conflates these.

---

## 7. Open questions

- SCADA telemetry path: gateway-composed summaries on `W2` (minimum mapping) vs configured observation tap forwarding raw `W1` PDUs — affects audit surface and bandwidth; read-only SCADA acceptance rules need Service-level policy.
- Cross-partner sensor visibility: are all four sensor snapshots forwarded to B over PartnerEth, or only on-demand coordination? Minimum mapping allows either; affects PartnerEth bandwidth, not topology.
- SensorRS485 non-CAN profile: exact framing/CRC per `LINK` byte-stream section is provisional; polling semantics are clear, encoding is not.
- Dual-core gateway: archetype names Core0 only; second core not modeled — assume shared Endpoint Domain with Core0 for minimum mapping.
- Commissioning narrative omitted per experiment scope; pre-addressed Node slots assumed on each CAN bus.

---

## 8. Spec findings (optional)

None filed for this sketch.

---

## 9. Diagrams (optional)

### Device-centric

```text
                    EthPlant (W2)                         PartnerEth (W1)
              ┌─────────────────────────┐                 ┌──────────────┐
              │ SCADA 0x0D   Laptop 0x0E│                 │              │
              └──────┬──────────────┬────┘                 │              │
                     │              │                      │              │
              ┌──────▼──────┐ ┌─────▼──────┐         ┌─────▼──────┐ ┌─────▼──────┐
              │ Gateway A   │ │ Gateway B  │◄───────►│ Gateway A  │ │ Gateway B  │
              │ 0x01        │ │ 0x02       │  W1     │ Partner LI │ │ Partner LI │
              └──┬──────┬───┘ └──┬────┬────┘         └────────────┘ └────────────┘
                 │      │        │      │
         PlantCAN│      │RS-485  │AuxCAN│CellCAN
            W1   │      │  W1    │ W1   │ W1
                 │      │        │      │
            ┌────▼──┐   │   ┌────▼──┐ ┌─▼─────┐
            │Inv1/2 │   │   │Valve/ │ │Cell   │
            │0x03/04│   │   │Pump   │ │0x0B/C │
            └───────┘   │   │0x09/A │ └───────┘
                        │   └───────┘
                   ┌────▼────────────┐
                   │ Sensors 0x05–08 │
                   └─────────────────┘
```

### Wire-centric (minimum mapping)

```text
Wire W1 PlantNet
├── 0x01 Gateway A      — PlantCAN MainA, RS-485 poller, PartnerEth, W1↔W2 tap
├── 0x02 Gateway B      — AuxCAN MainA, CellCAN MainA, PartnerEth, W1↔W2 tap
├── 0x03, 0x04          — Motor inverters (PlantCAN Node0, Node1)
├── 0x05–0x08           — Sensors (RS-485; master-polled)
├── 0x09, 0x0A          — Aux devices (AuxCAN Node0, Node1)
└── 0x0B, 0x0C          — Cell controllers (CellCAN Node0, Node1)

Physical realization: 5 Links, 2 gateways forward W1 across branches.
PartnerEth down → W1 partitions; no role reassignment.

Wire W2 EthPlant
├── 0x01 Gateway A
├── 0x02 Gateway B
├── 0x0D SCADA HMI      [read-only; preconfigured member]
└── 0x0E Service laptop [preconfigured; often unreachable]

Field nodes are NOT W2 members. Maintenance via gateway → W1.
```

### Rejected alternative (per-bus Wires)

| Approach | Wires | Issue |
|---|---|---|
| **Chosen:** spanning PlantNet + EthPlant | 2 | Preserves cross-partner canonical identity; matches cell coordination scope |
| Rejected: one Wire per fieldbus + partner bridge | 5+ | Wire proliferation; cross-partner field commands lose direct Src=0x02 → Inv visibility |
| Rejected: W1 also on EthPlant | 2 (+ multipath) | Dual inter-gateway paths; violates archetype constraint |

---

## Devices (reference)

| Device | Endpoint Domains | Link interfaces | Role summary |
|---|---|---|---|
| Gateway A | GwAPlant | PlantCAN, SensorRS485, PartnerEth, EthPlant | Plant coordinator; `W1` + `W2` |
| Gateway B | GwBPlant | AuxCAN, CellCAN, PartnerEth, EthPlant | Aux/cell owner; partner; `W1` + `W2` |
| Motor inverter 1–2 | MotorInv* | PlantCAN | Drive nodes |
| Sensors 1–4 | Sensor* | SensorRS485 | Polled leaves |
| Valve / pump | ValveCtrl, HydPump | AuxCAN | Aux nodes |
| Cell controllers 1–2 | CellCtrl* | CellCAN | Cell handshake nodes |
| SCADA HMI | ScadaHMI | EthPlant | Read-only `W2` observer |
| Service laptop | SvcLaptop | EthPlant | Intermittent `W2` maintenance |

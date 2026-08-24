# Sketch — Archetype 02: Raspberry Pi Robot + ECUs + Dev PC

```text
**Archetype:** archetypes/02_pi_robot.md
**Agent:** cursor
**Date:** 2026-08-23
```

---

## Executive summary

```text
**Minimum mapping:** 5 Participants, 2 Logical Buses (W1 RobotCAN on committed CAN11; W2 DevAccess on Ethernet/UDP), Pi gateway with composition only — no forwarding.
**Question A (R6):** Natural.
**Question B (CAN11):** Default.
**Worst friction (minimum path):** Configuration burden — Mild.
**Main lesson:** Plant vs host scope decomposes into two Wires; composition at Pi follows from interaction-authority rules, not from the PC lacking a CAN port — physical attachment, Wire membership, and allowed interaction are three separate choices.
```

---

## Disposition block

| Area | Assessment |
|---|---|
| Participant identity | Natural |
| Wire decomposition | Natural |
| Forwarding | Simple |
| Non-CAN configuration | Moderate |
| CAN11 VCN fit | Default |
| Better with CAN29? | no |

**Explanation:** Question A: the archetype has two real communication scopes — robot plant control and developer host access. Two Logical Buses (W1, W2) with Pi as the sole cross-scope Participant matches the **interaction-authority** product rule: the PC is not a member of ECU-facing plant scope. Bench Ethernet and field WiFi are alternative physical realizations of W2; intermittent WiFi affects reachability, not topology. Config C (Pi absent) leaves wiring static with no coordinator reassignment.

A single Wire spanning Ethernet and CAN through Pi **forwarding** is topologically valid under R6 (`CORE §3.5`) and would not require the PC to have a CAN interface — but it would make the PC a plant-scope Participant, which the archetype forbids. Composition at Pi is therefore required by application authority, not by physical link attachment.

Question B: RobotCAN uses committed CAN11 with the default VCN map — Pi as MainA, three ECUs as Node0–2. Three ordinary VCNs suffice; one WireAlias. MainB unassigned; VCN 0 broadcast available but unused. CAN29 offers no benefit for three leaf ECUs.

---

## 1. Native communication model

A small differential-drive mobile robot carries a Raspberry Pi (motion planner, estimator, logger at ~50 Hz), two drive ECUs (left/right motor + encoder), and an aux ECU (bumpers, IMU) on one Classical CAN bus. The Pi is the sole coordinator on CAN; ECUs do not command each other.

A developer PC attaches upstream of the Pi only:
- **Config A (bench):** wired Ethernet to the Pi for teleop, maintenance, and tooling.
- **Config B (field):** WiFi/UDP to the Pi; link is intermittent; Pi continues RobotCAN control and **local logging** when WiFi is down; telemetry to PC is best-effort and coalesced when the link returns.
- **Config C (Pi absent):** Pi powered off or removed; ECUs remain on CAN in safe/disabled state; **no PC path to ECUs**; no automatic reassignment of coordinator role.

**Product rule (interaction authority):** the PC is not permitted to participate directly in ECU-facing plant interactions. All PC access to ECUs — teleop, maintenance, firmware update — is mediated by Pi application Services.

The PC has no CAN interface (physical fact), but that alone does not determine Wire topology: under R6, a Wire may span heterogeneous Links, and a PC on Ethernet could theoretically be a plant-scope Participant on a Wire that propagates through Pi to CAN without the PC ever touching the bus. This archetype excludes that by **scope membership**, not by cable type.

Interactions:
- Pi commands drive ECUs (~50 Hz) and reads drive status (~100 Hz), bumper events, and IMU data.
- Pi logs all telemetry locally continuously (authoritative for post-mortem).
- PC sends bench teleop to Pi; Pi translates to wheel commands on CAN.
- PC exchanges maintenance traffic with Pi (identity, logs, firmware update relay to ECUs).
- PC receives field telemetry from Pi when the upstream link is up.

---

## 2. Obvious conventional implementation

> **Obvious conventional implementation:** Fixed CAN IDs for Pi↔ECU pairs on RobotCAN; Pi runs a ROS/custom control node with socket CAN; PC talks to Pi via TCP/UDP (Ethernet bench or WiFi field) with an application protocol for teleop, logs, and update relay; no cross-bus tunnel — Pi explicitly proxies ECU access.

> **What structure does a conventional design use?** Separate protocol stacks per link (CAN frame table + host socket API); implicit "Pi is gateway" in application code; deployment manifests as network addresses and CAN ID tables rather than named Logical Buses.

WireSpaces adds: deployment-global ParticipantIds, two named Wires with explicit membership, Link Bindings (CAN11 VCN map + host link profiles), and canonical Src/Dest on every PDU. The gateway boundary forces an explicit choice between forwarding (preserve PDU identity across Links) and composition (Pi authors new plant PDUs). Here, composition follows from the archetype's interaction-authority rule — not because the PC lacks a CAN port.

---

## 3. Minimum mapping (required first)

### 3.1 Participants

| ParticipantId | Endpoint Domain | Device | Notes |
|---|---|---|---|
| 0x01 | PiRuntime | Raspberry Pi | Plant coordinator on W1; host gateway on W2; one Linux Endpoint Domain |
| 0x02 | DriveLeft | DriveLeft ECU | Left wheel drive + encoder |
| 0x03 | DriveRight | DriveRight ECU | Right wheel drive + encoder |
| 0x04 | AuxSense | Aux ECU | Bumpers (events) + IMU (~100 Hz) |
| 0x05 | DevPC | Developer PC | Bench teleop, maintenance, field telemetry consumer |

Five Participants, one Endpoint Domain each. Pi is the only Participant on both Wires.

### 3.2 Wires

| Wire (#) | Participants | Physical links | Purpose |
|---|---|---|---|
| W1 RobotCAN | 0x01, 0x02, 0x03, 0x04 | RobotCAN (Classical CAN) | Plant control and sensing; autonomous operation |
| W2 DevAccess | 0x01, 0x05 | BenchEth (Config A) and/or FieldWiFi (Config B) | Teleop, maintenance, field telemetry — PC↔Pi only |

**W1** is the robot plant propagation domain. **W2** is the developer/host scope. Splitting reflects two distinct communication relationships and the interaction-authority product rule: the PC is not a member of ECU-facing plant scope (W1), regardless of whether a single Wire *could* span upstream Links and CAN through Pi forwarding.

**Config variants (link membership on W2, not Wire count):**

| Config | W2 active physical link | W1 | Notes |
|---|---|---|---|
| A — Bench | BenchEth | RobotCAN | Normal teleop and maintenance |
| B — Field | FieldWiFi (intermittent) | RobotCAN unchanged | WiFi loss = W2 unreachable; Pi logging on W1 continues |
| C — Pi absent | W2 unreachable (Pi down) | ECUs idle/safe on CAN | No coordinator; no PC→ECU path; **no role reassignment** |

Configured Wire membership is static across configs; reachability varies with power and link state.

### 3.3 Interactions (primary)

| Interaction | Src | Dest | Wire | Broadcast? | Notes |
|---|---|---|---|---|---|
| Left wheel torque command | 0x01 | 0x02 | W1 | No | ~50 Hz |
| Right wheel torque command | 0x01 | 0x03 | W1 | No | ~50 Hz |
| Drive status (L/R) | 0x02, 0x03 | 0x01 | W1 | No | ~100 Hz; Pi logs locally |
| Bumper events | 0x04 | 0x01 | W1 | No | Event-driven |
| IMU samples | 0x04 | 0x01 | W1 | No | ~100 Hz |
| Bench teleop commands | 0x05 | 0x01 | W2 | No | PC → Pi |
| Teleop → wheel commands | 0x01 | 0x02, 0x03 | W1 | No | **Composition** at Pi (new PDU, Src=0x01) |
| Maintenance (identity, logs) | 0x05 ↔ 0x01 | 0x01 ↔ 0x05 | W2 | No | Bench; reliable segment for bulk |
| FW update relay to ECUs | 0x05 → 0x01 → ECUs | — | W2 then W1 | No | **Composition** at Pi; PC not in W1 plant scope |
| Field telemetry | 0x01 | 0x05 | W2 | No | Coalesced when WiFi up; Pi log is authoritative |
| Autonomous operation | 0x01 ↔ ECUs | — | W1 | No | PC may be absent; W2 unused |

No ECU↔ECU interactions. No PC as Src or Dest on W1 plant interactions (authority rule, not physical CAN attachment).

### 3.4 Forwarding (if any)

None on the minimum mapping happy path.

Cross-scope traffic (teleop, telemetry export, FW relay) is **composition** at Pi: Pi consumes on W2 and authors new PDUs on W1 (or the reverse for telemetry).

**Why not forwarding?** Forwarding would preserve the PC as `SrcParticipantId` on plant-scope PDUs reaching ECUs — making the PC a de facto participant in ECU-facing plant interactions. That violates the archetype's interaction-authority rule. It is **not** ruled out because the PC lacks a CAN port: R6 explicitly allows a Wire to span heterogeneous Links (`CORE §3.5`), so a single plant Wire with members {PC, Pi, ECUs} propagated across BenchEth + RobotCAN via Pi forwarding is topologically valid. The archetype rejects that decomposition because the PC must not be in plant communication scope — a membership/authority choice, not a physical-link constraint.

Pi does not forward W1 PDUs onto W2 for PC observation in the minimum mapping; telemetry is Pi-authored from its local log buffer.

### 3.5 Link profiles

| Physical link | Profile | WS Wire(s) | Notes |
|---|---|---|---|
| RobotCAN | Committed CAN11 | W1 | 500 kbit/s typical; archetype suggested profile |
| BenchEth | WS over UDP/IP (or Ethernet datagram) | W2 | Config A; profile details provisional (`LINK §6`) |
| FieldWiFi | WS over UDP/IP | W2 | Config B; same W2 binding semantics, different Link Interface |

Non-CAN links carry full canonical WireNumber and ParticipantIds (no CAN11 projection). Bench and field may share one W2 Link Binding template with two Pi Link Interfaces — deployment selects which is active.

### 3.6 CAN11 bindings (if CAN11 used)

**RobotCAN — committed CAN11**

```text
Profile:          committed
WireAliases used:             1 / 8
  default map:                  1
  custom map:                   0

Per alias:
  Alias:                        1
  Canonical Wire:               W1 (RobotCAN)
  Mapping:                      Default
  Ordinary VCNs used:           3 / 32
  Default map sufficient?       yes
  Custom entries (if Explicit): 0
  MainA PID / MainB PID:         0x01 / (unassigned)
  Node positions used:          3 / 14
```

**VCN assignments (default map, MainA = 0x01):**

| VCN | Relation | Status in minimum mapping |
|---|---|---|
| 0 | 0x01 → kBroadcast | Available by default; **unused** |
| 3 | — | Reserved Link control |
| 4 | 0x02 ↔ 0x01 | **Used** — DriveLeft ↔ Pi |
| 6 | 0x03 ↔ 0x01 | **Used** — DriveRight ↔ Pi |
| 8 | 0x04 ↔ 0x01 | **Used** — Aux ↔ Pi |

Odd VCNs (5, 7, 9) for Node↔MainB unused. **Two-Main default allocation not exercised** — only MainA assigned.

**Guest CAN11:** Not used. RobotCAN is fully WireSpaces-governed committed CAN11.

---

## 4. Optional optimizations

### 4.1 Split W2 into TeleopWire + MaintWire

Two Logical Buses on the upstream links: W2a (low-latency teleop) and W2b (maintenance/bulk update). **Problem solved:** isolate large FW transfers from teleop latency. **Minimum mapping gap:** bench Ethernet has ample bandwidth; field teleop is typically absent. Not justified unless measured upstream congestion appears.

### 4.2 Overlapping drive telemetry Wire on RobotCAN

W3 ⊂ {0x01, 0x02, 0x03} for high-rate drive status, overlapping W1 via second committed binding (WireAlias 2). **Problem solved:** bandwidth isolation between ~100 Hz drive status and ~50 Hz commands. **Minimum mapping gap:** three ECUs on 500 kbit/s CAN is within archetype budget; QoS bits on existing VCNs suffice for command priority.

### 4.3 Single plant Wire spanning upstream Links (forwarding alternative)

One Logical Bus W1 spanning BenchEth/FieldWiFi + RobotCAN, with Pi forwarding PC-originated PDUs onto CAN with `Src=0x05`. **Topologically valid** under R6 — the PC need not have a CAN interface. **Rejected:** places the PC in ECU-facing plant scope, violating the interaction-authority product rule. The two-Wire decomposition (W1 plant + W2 host) is chosen because of scope membership, not because heterogeneous Link spanning is impossible.

### 4.4 Forwarding / observation tap (W1 → W2)

Configure Pi to forward ECU→Pi traffic onto W2 so the PC observes plant PDUs without Pi re-authoring. **Problem solved:** passive debug visibility. **Rejected:** still exposes raw plant-scope traffic to a host-scope Participant without Pi Service mediation; the archetype's telemetry path is Pi-authored on W2 from the local log buffer.

### 4.5 Guest CAN on RobotCAN

Not applicable — no legacy coexistence requirement; mixed Guest+committed on one bus is not specified (`overlay §13`, SF-R6-002).

---

## 5. Friction signals (minimum mapping happy path)

| Signal | None / Mild / Significant | One-sentence justification |
|---|---|---|
| Artificial Wire | None | W1 = plant CAN scope; W2 = PC↔Pi host scope — both match real communication boundaries. |
| Wire proliferation | None | Two Wires for two scopes; not per-cable or per-ECU splitting. |
| Artificial hierarchy | None | MainA = Pi as actual CAN coordinator; MainB unassigned, not fictitious. |
| VCN pressure | None | Three Main↔Node relations; default map with three Node slots. |
| WireAlias pressure | None | One alias for W1 on RobotCAN. |
| Configuration burden | Mild | ParticipantIds, two Wires, CAN11 default binding, plus provisional host link bindings for W2 — more explicit than separate CAN table + socket server, but modest. |
| Failure/topology mismatch | Mild | Config C (Pi absent) is a configured non-operational state, not a topology rewrite; worth documenting that W1 membership persists while Pi is unreachable. |

---

## 6. Model pressure

- **If you could change one WS concept** to simplify this system, what would it be? A documented "gateway composition" pattern in DEPLOY (Pi consumes W2 / produces W1) so authors don't have to derive it from forwarding-vs-composition rules each time — cosmetic documentation, not a protocol change.
- **Did WS expose a useful distinction** the conventional model obscures? Yes: three separable choices that conventional designs often conflate:
  ```text
  physical attachment  !=  Wire membership  !=  application authority
  ```
  Plant Wire vs host Wire separates autonomous operation (W1 only) from developer access (W2). The forwarding alternative (one Wire spanning Links) is topologically valid but rejected on authority grounds — a distinction invisible when "PC not on CAN" is stated only as a physical constraint.

**Synthesis note:** Like Archetype 01, this sketch uses MainA only and does not stress the two-Main VCN allocation. It **does** stress multi-Link gateway scope (two Wires, composition boundary) — evidence for R6 on Linux gateways, not for CAN11 VCN generality.

---

## 7. Open questions

- W2 host profile: single UDP binding for both BenchEth and FieldWiFi vs separate Link Bindings sharing WireNumber — deployment ergonomics, not semantic difference.
- Field telemetry: directed Pi→PC stream vs periodic snapshot Service — Endpoint binding choice on W2.
- Config C safe state: ECU firmware behavior when Pi stops commanding is outside WS; sketch only records that W1 wiring survives and no ParticipantId reassignment occurs.
- Reliable segment transport for FW update: crosses W2 then Pi composes onto W1 — segment ownership and retry scope at Pi boundary (Service-layer, not Wire topology).

---

## 8. Spec findings (optional)

| ID | Finding |
|---|---|
| SF-R6-005 | Archetype product rules stated as physical attachment ("PC not on CAN") under-specify mapping choices. Interaction-authority rules ("PC not in plant scope") are needed to justify composition vs forwarding on a heterogeneous-Link Wire. Archetype 02 updated accordingly. |

---

## 9. Diagrams (optional)

### Device-centric (Configs A/B)

```text
                    ┌─────────────┐
                    │  Dev PC     │
                    │  PID 0x05   │
                    └──────┬──────┘
                           │ W2 DevAccess
              Config A: BenchEth  ────┐
              Config B: FieldWiFi ────┤ (intermittent)
                                      │
                    ┌─────────────────▼───┐
                    │  Raspberry Pi       │
                    │  PID 0x01           │
                    │  (composition gw)   │
                    └───┬─────────────────┘
                        │ W1 RobotCAN (CAN11 committed)
            ┌───────────┼───────────┐
            │           │           │
       ┌────▼────┐ ┌────▼────┐ ┌────▼────┐
       │DriveLeft│ │DriveRght│ │  Aux    │
       │ 0x02    │ │ 0x03    │ │ 0x04    │
       │ Node0   │ │ Node1   │ │ Node2   │
       └─────────┘ └─────────┘ └─────────┘
```

### Config C (Pi absent)

```text
  Dev PC ──X── (no path) ──X── Pi [off]

  RobotCAN: ECUs 0x02, 0x03, 0x04 — safe/idle
            W1 membership unchanged; Pi 0x01 unreachable
            No coordinator reassignment
```

### Wire-centric

```text
W1 RobotCAN (CAN11 Alias 1, Default VCN)
├── 0x01  Pi          (MainA)
├── 0x02  DriveLeft   (Node0)  VCN 4
├── 0x03  DriveRight  (Node1)  VCN 6
└── 0x04  Aux         (Node2)  VCN 8

W2 DevAccess (UDP/IP — BenchEth or FieldWiFi)
├── 0x01  Pi
└── 0x05  Dev PC

Cross-scope: composition at 0x01 only. No forwarding. PC not in W1 plant scope (authority rule).
```

### Rejected alternatives

| Approach | Wires | Issue |
|---|---|---|
| **Chosen:** W1 plant + W2 host | 2 | Separates plant scope from host scope per interaction-authority rule |
| Rejected: one plant Wire spanning Ethernet + CAN via Pi forwarding | 1 | **Topologically valid** (heterogeneous Link spanning); rejected because PC would be a plant-scope Participant — authority/membership, not physical attachment |
| Rejected: one Wire per ECU pair | 3+ | Wire proliferation |
| Rejected: separate W2 per upstream link (bench vs WiFi) | 3 | WiFi ≠ Wire change; same PC↔Pi scope |

---

## Devices (reference)

| Device | Endpoint Domains | Link interfaces | Role summary |
|---|---|---|---|
| Raspberry Pi | PiRuntime | RobotCAN, BenchEth, FieldWiFi | W1 MainA; W2 gateway; composes cross-scope traffic |
| DriveLeft ECU | DriveLeft | RobotCAN | W1 Node0 |
| DriveRight ECU | DriveRight | RobotCAN | W1 Node1 |
| Aux ECU | AuxSense | RobotCAN | W1 Node2 |
| Developer PC | DevPC | BenchEth and/or FieldWiFi | W2 only; not in W1 plant scope (no CAN interface, but authority rule is the mapping driver) |

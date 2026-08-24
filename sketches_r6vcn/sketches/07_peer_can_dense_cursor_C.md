# Sketch — Archetype 07: Dense Peer CAN Network (Negative Control)

```text
**Archetype:** archetypes/07_peer_can_dense.md
**Agent:** cursor_C
**Date:** 2026-08-23
```

---

## Executive summary

```text
**Minimum mapping:** 8 production Participants (dual-core per corner), 1 Logical Bus (W1 CellCAN) on committed CAN11 with Explicit custom VCN map (14 ordinary VCNs); no forwarding; optional PC in bring-up config only.
**Question A (R6):** Natural.
**Question B (CAN11):** Custom.
**Worst friction (minimum path):** Configuration burden — Significant.
**Main lesson:** R6 cleanly models symmetric chassis peers and dual-core domains on one bus; CAN11 default VCN cannot express chassis node↔node pairs — custom enumeration is required, and a mistaken full 8-node peer mesh would exceed one WireAlias.
```

---

## Disposition block

| Area | Assessment |
|---|---|
| Participant identity | Natural |
| Wire decomposition | Natural |
| Forwarding | Simple |
| Non-CAN configuration | Low |
| CAN11 VCN fit | Custom |
| Better with CAN29? | Yes |

**Explanation:** Question A: eight Endpoint Domains on four corner MCUs map to eight deployment-global ParticipantIds with no nominated coordinator. Chassis domains form a symmetric multi-initiator peer mesh on one Logical Bus; diagnostics domains are separate initiators with corner-local and optional PC traffic only (`CORE §3.1`, `§3.4`). Dual-core corners share one CAN transceiver — two ParticipantIds, one Link Interface per MCU — which is ordinary R6. Fault reaction is configured consumption of neighbors' broadcast chassis status, not a topology extension.

Question B: the overlay default VCN map provides MainA/MainB↔Node slots only — **no node↔node pairs** (`overlay §5`). The required chassis peer command mesh among four chassis domains needs six explicit pair VCNs; intra-corner chassis↔diagnostics adds four more; per-chassis status broadcasts add four — 14 ordinary VCNs total, fitting one alias (14 / 32) but with a fully custom table. Default map is insufficient even if two chassis domains are mislabeled MainA/MainB (remaining chassis pairs stay unreachable). A hypothetical full 8-node peer-command mesh (~28 pairs + 8 broadcasts ≈ 36 VCNs) would exceed one alias — the archetype's stress boundary. CAN29 remains the honest escape for dense peer graphs (`overlay §10`). MainB and two-Main default allocation are **not exercised**.

---

## 1. Native communication model

Four dual-core automotive ECUs (front-left, front-right, rear-left, rear-right) share one Classical CAN bus on a small mobile platform. Each MCU hosts two Endpoint Domains on the same transceiver:

- `*_Chassis` — real-time core: wheel speed, driver state, fault flags; torque enable/setpoint/reset; reacts to neighbor faults.
- `*_Diagnostics` — companion core: DTCs, self-test status, calibration metadata; lower-rate reporting.

There is **no firmware-designated bus master** and no gateway. Chassis domains form a symmetric peer mesh: any chassis may command any other chassis; each publishes status at ~10 Hz to interested peers. Diagnostics domains do not command peer chassis or cross-corner diagnostics. Intra-corner handoff between chassis and diagnostics on the same PCB uses CellCAN (not a separate physical link). An optional developer PC may attach via USB-CAN during bring-up for passive capture and diagnostics log collection; it is not part of normal runtime control.

Loss of one corner MCU takes both domains offline; remaining six domains continue with degraded coordination. Loss of diagnostics only on one corner leaves that corner's chassis active on the bus.

---

## 2. Obvious conventional implementation

> **Obvious conventional implementation:** Fixed 11-bit CAN ID matrix — one ID per (source, destination, message class) or per source with address field in payload; symmetric interaction table for four chassis peers; separate IDs for intra-corner chassis↔diagnostics handoff; optional IDs for diagnostics→PC; broadcast or per-listener IDs for chassis status; no coordinator role in firmware.

> **What structure does a conventional design use?** Flat ID allocation table (often 40+ IDs for this interaction surface per archetype bandwidth notes); each ECU demultiplexes by CAN ID; peer symmetry is implicit in the matrix, not in a hierarchy. Dual-core domains on one MCU share the same transceiver driver with local demux to the correct core.

WireSpaces adds: named Wire, ParticipantIds, Link Binding with VCN relation table, canonical Src/Dest per PDU. For this archetype the WS layer largely **re-encodes** the same pairwise relations the CAN matrix already names — with the default VCN map unable to avoid custom enumeration for chassis node↔node paths.

---

## 3. Minimum mapping (required first)

### 3.1 Participants

| ParticipantId | Endpoint Domain | Device | Notes |
|---|---|---|---|
| 0x01 | FL_Chassis | FL corner MCU, RT core | Chassis peer; symmetric authority |
| 0x02 | FL_Diagnostics | FL corner MCU, companion core | Corner-local + optional PC reporting |
| 0x03 | FR_Chassis | FR corner MCU, RT core | Chassis peer |
| 0x04 | FR_Diagnostics | FR corner MCU, companion core | |
| 0x05 | RL_Chassis | RL corner MCU, RT core | Chassis peer |
| 0x06 | RL_Diagnostics | RL corner MCU, companion core | |
| 0x07 | RR_Chassis | RR corner MCU, RT core | Chassis peer |
| 0x08 | RR_Diagnostics | RR corner MCU, companion core | |
| 0x09 | DevPC | Developer PC (optional) | Bring-up passive capture / diag log sink only |

Eight production Participants. No coordinator. Optional PC (0x09) documented for bring-up configuration only — not a runtime command authority (`archetype §Physical topology`).

Chassis ParticipantIds: {0x01, 0x03, 0x05, 0x07}. Diagnostics: {0x02, 0x04, 0x06, 0x08}.

### 3.2 Wires

| Wire (#) | Participants | Physical links | Purpose |
|---|---|---|---|
| W1 CellCAN | 0x01–0x08 (production); 0x09 optional bring-up | CellCAN (Classical CAN) | Chassis peer mesh, intra-corner handoff, optional diagnostics→PC |

One Logical Bus matches one physical propagation domain. All eight production domains are electrically visible on CellCAN; interaction subsets differ by domain role, not by Wire membership.

Splitting chassis vs diagnostics into separate Wires on the same physical bus would be wire proliferation without a distinct propagation scope (`brief §5` step 7 not justified).

**Bring-up variant:** W1 membership may include 0x09 for passive observation and directed diagnostics reports during commissioning; production deployment omits 0x09 from configured interactions.

### 3.3 Interactions (primary)

| Interaction | Src | Dest | Wire | Broadcast? | Notes |
|---|---|---|---|---|---|
| Chassis status publication | 0x01, 0x03, 0x05, 0x07 (each) | kBroadcast | W1 | Yes | ~10 Hz per chassis; peers observe for fault reaction (`CORE §12.6`) |
| Peer torque command | Any chassis 0x01/03/05/07 | Any other chassis | W1 | No | Torque enable, setpoint, fault reset — core negative-control path |
| Fault reaction (local) | — | — | — | — | Consume neighbors' status/fault PDUs; local action — not a separate Wire interaction |
| Intra-MCU handoff | 0x01↔0x02, 0x03↔0x04, 0x05↔0x06, 0x07↔0x08 | same corner pair | W1 | No | Event; same PCB, still CellCAN per archetype |
| Diagnostics report | 0x02, 0x04, 0x06, 0x08 (each) | 0x09 | W1 | No | ~1 Hz; **bring-up configuration only** — not production minimum |
| Passive log capture | 0x01–0x08 (observed) | 0x09 | W1 | — | PC observes electrically visible traffic; bring-up only |

**Production minimum interaction counts:**

- Chassis peer command pairs: C(4,2) = **6** unordered pairs among {0x01, 0x03, 0x05, 0x07}
- Chassis status broadcast sources: **4** (one per chassis domain)
- Intra-corner chassis↔diagnostics pairs: **4**
- **Total ordinary VCNs (production): 14**

**Explicitly absent:** diagnostics→peer chassis commands; cross-corner diagnostics commands; coordinator or gateway traffic.

### 3.4 Forwarding (if any)

None. Single physical CAN, single Wire, no gateway.

### 3.5 Link profiles

| Physical link | Profile | WS Wire(s) | Notes |
|---|---|---|---|
| CellCAN | Committed CAN11 | W1 | Archetype suggested profile; **Explicit** VCN map required |

### 3.6 CAN11 bindings (if CAN11 used)

**CellCAN — committed CAN11**

```text
Profile:          committed
WireAliases used:             1 / 8
  default map:                  0
  custom map:                   1

Per alias:
  Alias:                        1
  Canonical Wire:               W1 (CellCAN)
  Mapping:                      Explicit
  Ordinary VCNs used:           14 / 32   (production minimum only)
  VCNs available, unused:       VCN 0 (MainA broadcast), VCN 1 (MainB broadcast),
                                  VCN 2 (MainA↔MainB), VCN 18–31 (spare slots)
  Default map sufficient?       no
  Custom entries (if Explicit): 14
  MainA PID / MainB PID:         (unassigned) / (unassigned)
  Node positions used:          0 / 14  (default Node slots not used)
```

**Explicit VCN table (production minimum):**

| VCN | Relation | Purpose |
|---|---|---|
| 3 | — | Reserved Link control |
| 4 | 0x01 ↔ 0x03 | FL ↔ FR chassis commands |
| 5 | 0x01 ↔ 0x05 | FL ↔ RL |
| 6 | 0x01 ↔ 0x07 | FL ↔ RR |
| 7 | 0x03 ↔ 0x05 | FR ↔ RL |
| 8 | 0x03 ↔ 0x07 | FR ↔ RR |
| 9 | 0x05 ↔ 0x07 | RL ↔ RR |
| 10 | 0x01 → kBroadcast | FL chassis status |
| 11 | 0x03 → kBroadcast | FR chassis status |
| 12 | 0x05 → kBroadcast | RL chassis status |
| 13 | 0x07 → kBroadcast | RR chassis status |
| 14 | 0x01 ↔ 0x02 | FL intra-corner handoff |
| 15 | 0x03 ↔ 0x04 | FR intra-corner |
| 16 | 0x05 ↔ 0x06 | RL intra-corner |
| 17 | 0x07 ↔ 0x08 | RR intra-corner |

VCNs 0–2 (default MainA/MainB broadcast and MainA↔MainB) **available in profile but unused** — no Main positions assigned. Two-Main default allocation **not exercised** and **not applicable** to this peer mesh.

**Bring-up additions (not counted in production minimum):**

| VCN | Relation | Purpose |
|---|---|---|
| 18 | 0x02 → 0x09 | FL diagnostics report |
| 19 | 0x04 → 0x09 | FR diagnostics report |
| 20 | 0x06 → 0x09 | RL diagnostics report |
| 21 | 0x08 → 0x09 | RR diagnostics report |

Bring-up total: 18 ordinary VCNs (18 / 32) — still one alias.

**Guest CAN11:** Not used.

**Membership vs presence:** Optional PC (0x09) is **preconfigured** on W1 in bring-up configuration with directed diagnostics sinks; when absent, delivery fails or queues drain — wiring persists (`brief §6.4`). Production committed configuration omits 0x09 and VCNs 18–21 entirely.

---

## 4. Optional optimizations

### 4.1 CAN29 instead of CAN11

Replace committed CAN11 Explicit map with CAN29 carrying full canonical ParticipantIds per frame. **Problem solved:** dense peer and dual-core identity without VCN enumeration or fake hierarchy. **Minimum mapping gap:** CAN11 custom map works for required interactions but re-implements the conventional CAN ID matrix; overlay §10 recommends CAN29 for arbitrary dense peer graphs on CAN.

### 4.2 Default VCN map with nominated MainA/MainB

Assign FL_Chassis=MainA, FR_Chassis=MainB, RL_Chassis=Node0, RR_Chassis=Node1. **Problem it would solve:** avoid custom VCN table for some chassis↔Main paths. **Rejected:** (a) invents hierarchy the firmware does not have; (b) **incomplete** — default map provides Node↔Main only, not Node↔Node (RL↔RR unreachable); (c) does not cover intra-corner chassis↔diagnostics or diagnostics traffic; (d) only five of six chassis peer pairs coverable even with this distortion.

### 4.3 Full 8-node peer-command mesh (mistaken mapping)

Treat all eight domains as chassis-equivalent peers: C(8,2) = 28 pair VCNs + 8 broadcast VCNs ≈ **36 ordinary VCNs**. **Problem it would solve:** none — archetype explicitly excludes diagnostics→peer chassis. **Outcome:** exceeds one 32-VCN WireAlias; requires second alias or CAN29. Documents the archetype's stress boundary without being the honest minimum mapping.

### 4.4 Second WireAlias for overflow

If §4.3 or directed status fan-out were chosen, split pair relations across Alias 1 and Alias 2 on the same WireNumber. **Problem it would solve:** stay on CAN11 when one alias is full. **Rejected for minimum mapping:** not needed for required interactions (14 VCNs); adds WireAlias pressure driven by CAN11 limits, not semantic Wires.

### 4.5 Separate Wire for diagnostics scope

W1 chassis + W2 diagnostics on the same physical bus. **Rejected:** wire proliferation — one propagation domain, no distinct failure or bandwidth scope in the archetype.

### 4.6 Guest CAN for optional PC

Not required — PC is passive observer; electrical visibility + observation config suffices. Mixed Guest+committed on one bus not specified (`overlay §13`, SF-R6-002).

---

## 5. Friction signals (minimum mapping happy path)

| Signal | None / Mild / Significant | One-sentence justification |
|---|---|---|
| Artificial Wire | None | W1 is the shared CAN cell — one meaningful propagation domain for all eight domains. |
| Wire proliferation | None | One Wire for eight Participants on one bus. |
| Artificial hierarchy | None | Minimum mapping uses Explicit VCN with no MainA/MainB assignment. |
| VCN pressure | Mild | 14 ordinary VCNs fit in one alias (14 / 32) but every chassis pair, broadcast source, and intra-corner pair must be enumerated; a full 8-node mesh would be Significant. |
| WireAlias pressure | None | One alias; custom map is driven by relation shape, not alias exhaustion on minimum path. |
| Configuration burden | Significant | 14-entry explicit VCN table plus eight Participant bindings re-implements the conventional CAN ID matrix with more structure and no coordinator shortcut. |
| Failure/topology mismatch | None | Corner MCU loss (both domains) and single-domain loss (diagnostics only) map naturally; no coordinator failover to misdescribe. |

---

## 6. Model pressure

- **If you could change one WS concept** to simplify this system, what would it be? A peer-graph VCN profile (or CAN29 as the default recommendation) that names participant pairs directly without Main/Node positions — the current default map assumes hierarchy this archetype explicitly lacks.
- **Did WS expose a useful distinction** the conventional model obscures? Yes: separating **R6 peer equality and dual-core Participant identity** (multi-initiator Logical Bus with eight domains works cleanly) from **CAN11 compression fit** (default VCN assumes controller/leaf star). The negative control works — awkwardness localizes to Question B, not Question A. Dual-core domains on one MCU are natural Participant boundaries without inventing separate physical links.

---

## 7. Open questions

- Archetype scale reference (~36 VCNs for 8-node mesh) assumes all eight domains in a peer-command mesh; required interactions restrict peer commands to four chassis domains — mappers should not inflate VCN count beyond what interactions require.
- Whether diagnostics should consume chassis broadcast status for DTC correlation without a dedicated VCN — likely observation of existing broadcast VCNs suffices.
- Intra-corner handoff on CellCAN vs local shared-memory Link on the same MCU — archetype mandates CellCAN; a product might optimize locally but that is outside this sketch.
- QoS on safety-critical torque-enable commands: QoS Critical bit within existing pair VCNs vs separate Wire — likely QoS bits suffice.
- PC bring-up: configured broadcast observer table vs promiscuous non-WS capture outside WireSpaces — tooling choice for passive PC.

---

## 8. Spec findings (optional)

| ID | Finding |
|---|---|
| SF-R6-006 | Default VCN map (`overlay §5`) provides Node↔MainA/MainB only — no node↔node slots. Chassis peer mesh on CAN11 requires Explicit custom entries or CAN29; assigning fake MainA/MainB is incomplete for ≥3 leaf peers. Confirmed with 8-domain dual-core archetype. |

---

## 9. Diagrams (optional)

### Device-centric

```text
CellCAN (Classical CAN 11-bit)
┌─────────────────────────────────────────────────────────────┐
│  FL MCU          FR MCU          RL MCU          RR MCU     │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐ │
│  │0x01 Chas │    │0x03 Chas │    │0x05 Chas │    │0x07 Chas │ │
│  │0x02 Diag │    │0x04 Diag │    │0x06 Diag │    │0x08 Diag │ │
│  └────┬─────┘    └────┬─────┘    └────┬─────┘    └────┬─────┘ │
│       └───────────────┴───────────────┴───────────────┘       │
│                    shared bus segment                          │
└─────────────────────────────────────────────────────────────┘
         optional: DevPC 0x09 (USB-CAN, bring-up only)
```

### Chassis peer command mesh (4 domains)

```text
        FL (0x01)
       / | \
      /  |  \
    FR--+--+-- RR
 (0x03) |  (0x07)
      \ | /
       RL (0x05)

6 undirected chassis pair relations → VCNs 4–9
Each chassis → kBroadcast → VCNs 10–13
```

### Wire-centric (production minimum)

```text
Wire W1 CellCAN (CAN11 Alias 1, Explicit VCN)
├── Chassis peers (symmetric commands)
│     0x01 FL   0x03 FR   0x05 RL   0x07 RR
│     pair VCNs 4–9; broadcast VCNs 10–13
├── Intra-corner (event handoff)
│     0x01↔0x02   0x03↔0x04   0x05↔0x06   0x07↔0x08
│     VCNs 14–17
└── (bring-up) diagnostics→PC VCNs 18–21, observer 0x09

No MainA/MainB. No gateway. No coordinator.
```

### Rejected alternatives

| Approach | Outcome | Issue |
|---|---|---|
| **Chosen:** W1 + Explicit 14-VCN map | Valid | Custom enumeration; CAN11 works but burdensome |
| Rejected: default VCN + fake MainA/MainB | Incomplete | RL↔RR unreachable; invented hierarchy |
| Rejected: one Wire per pair | 6+ Wires | Wire proliferation on one bus |
| Rejected: full 8-node peer mesh | ~36 VCNs | Exceeds one alias; not required by archetype |
| **CAN29** (optimization) | Likely better | Overlay-recommended for dense peer CAN |

---

## Devices (reference)

| Device | Endpoint Domains | Link interfaces | Role summary |
|---|---|---|---|
| FL corner MCU | FL_Chassis, FL_Diagnostics | CellCAN | W1 peers 0x01, 0x02 |
| FR corner MCU | FR_Chassis, FR_Diagnostics | CellCAN | W1 peers 0x03, 0x04 |
| RL corner MCU | RL_Chassis, RL_Diagnostics | CellCAN | W1 peers 0x05, 0x06 |
| RR corner MCU | RR_Chassis, RR_Diagnostics | CellCAN | W1 peers 0x07, 0x08 |
| Developer PC | DevPC | CellCAN (USB, bring-up) | Optional observer 0x09; not production |

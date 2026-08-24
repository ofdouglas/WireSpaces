# Sketch — Archetype 07: Dense Peer CAN Network (8-domain, dual-core)

```text
**Archetype:** archetypes/07_peer_can_dense.md
**Agent:** cursor
**Date:** 2026-08-23
```

---

## Executive summary

```text
**Minimum mapping:** 8 production Participants (dual-core per corner), 1 Logical Bus (W1 CellCAN) on committed CAN11 with Explicit custom VCN map (14 ordinary VCNs); no forwarding; optional PC in bring-up config only.
**Question A (R6):** Natural.
**Question B (CAN11):** Custom.
**Worst friction (minimum path):** Configuration burden — Mild.
**Main lesson:** R6 handles eight symmetric initiators and dual-core domains on one bus; default VCN still does not fit chassis node↔node pairs — but required interactions need only 14 / 32 VCNs; a mistaken full 8-node command mesh (~36 VCNs) is where alias pressure and CAN29 preference emerge.
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
| Better with CAN29? | no (preference-dependent at required scale; yes if full 8-node mesh) |

**Explanation:** Question A: eight Endpoint Domains on four corner MCUs map to eight deployment-global ParticipantIds with no nominated coordinator. Chassis domains {0x01, 0x03, 0x05, 0x07} form a symmetric multi-initiator peer mesh; diagnostics domains handle corner-local traffic and optional PC reporting only. Dual-core corners share one CAN transceiver — two ParticipantIds, one Link Interface per MCU — which is ordinary R6 (`CORE §3.1`).

Question B: default VCN provides MainA/MainB↔Node slots only — **no node↔node pairs** (`overlay §5`). Required interactions need 14 explicit ordinary VCNs (6 chassis pairs + 4 chassis broadcasts + 4 intra-corner pairs), fitting one alias (14 / 32). A VCN names a **relationship** shared by all Services on that pair — likely **fewer** topology entries than a 40+ ID conventional matrix for this interaction surface. Default map remains unusable without fake hierarchy. CAN29 is cleaner at scale but **not necessary** for the honest minimum; reserve CAN29 preference for full 8-node command mesh (~36 VCNs, exceeds one alias) or frequent topology change.

---

## 1. Native communication model

Four dual-core automotive ECUs (FL, FR, RL, RR) share one Classical CAN bus. Each MCU hosts two Endpoint Domains on the same transceiver:

- `*_Chassis` — real-time core: wheel speed, driver state, faults; torque enable/setpoint/reset; reacts to neighbor faults (~10 Hz status).
- `*_Diagnostics` — companion core: DTCs, self-test, calibration metadata (~1 Hz reporting).

**No firmware-designated bus master** and no gateway. Chassis domains form a symmetric peer mesh: any chassis may command any other chassis. Diagnostics domains do **not** command peer chassis or cross-corner diagnostics. Intra-corner chassis↔diagnostics handoff uses CellCAN per archetype (not a separate physical link).

An optional developer PC may attach via USB-CAN during bring-up for passive capture and diagnostics log collection; it is not part of normal runtime control.

Loss of one corner MCU takes both domains offline; remaining six domains continue with degraded coordination. Loss of diagnostics only on one corner leaves that corner's chassis active.

---

## 2. Obvious conventional implementation

> **Obvious conventional implementation:** Fixed 11-bit CAN ID matrix — per (source, destination, message class); symmetric table for four chassis peers; separate IDs for intra-corner handoff; optional diagnostics→PC IDs; broadcast for chassis status; no coordinator.

> **What structure does a conventional design use?** Flat ID allocation (often **40+ IDs** per archetype bandwidth notes); dual-core domains share one transceiver driver with local demux to the correct core.

WireSpaces adds named Wire, ParticipantIds, and VCN relation bindings. A VCN names a participant **relationship** — torque enable, setpoint, fault reset, diagnostics, firmware, and other Services between the same pair share one VCN. Fourteen static relation entries may be **less** configuration burden than a full conventional CAN database at product scale, even though the default hierarchical VCN map does not fit without an explicit table.

---

## 3. Minimum mapping (required first)

### 3.1 Participants

| ParticipantId | Endpoint Domain | Device | Notes |
|---|---|---|---|
| 0x01 | FL_Chassis | FL corner MCU, RT core | Chassis peer |
| 0x02 | FL_Diagnostics | FL corner MCU, companion core | Corner-local + optional PC |
| 0x03 | FR_Chassis | FR corner MCU, RT core | Chassis peer |
| 0x04 | FR_Diagnostics | FR corner MCU, companion core | |
| 0x05 | RL_Chassis | RL corner MCU, RT core | Chassis peer |
| 0x06 | RL_Diagnostics | RL corner MCU, companion core | |
| 0x07 | RR_Chassis | RR corner MCU, RT core | Chassis peer |
| 0x08 | RR_Diagnostics | RR corner MCU, companion core | |
| 0x09 | DevPC | Developer PC (optional) | Bring-up only; not production minimum |

Chassis peers: {0x01, 0x03, 0x05, 0x07}. Diagnostics: {0x02, 0x04, 0x06, 0x08}.

### 3.2 Wires

| Wire (#) | Participants | Physical links | Purpose |
|---|---|---|---|
| W1 CellCAN | 0x01–0x08 (production); 0x09 optional bring-up | CellCAN (Classical CAN) | Chassis peer mesh, intra-corner handoff, optional diagnostics→PC |

One Logical Bus — one propagation domain. Interaction subsets differ by domain role, not by Wire membership. Splitting chassis vs diagnostics into separate Wires on the same bus would be wire proliferation without distinct scope.

**Membership vs presence:** Optional PC (0x09) is preconfigured in bring-up configuration; production committed configuration omits 0x09.

### 3.3 Interactions (primary)

| Interaction | Src | Dest | Wire | Broadcast? | Notes |
|---|---|---|---|---|---|
| Chassis status | 0x01, 0x03, 0x05, 0x07 (each) | kBroadcast | W1 | Yes | ~10 Hz; peers observe for fault reaction |
| Peer torque command | Any chassis | Any other chassis | W1 | No | Core negative-control path |
| Fault reaction (local) | — | — | — | — | Consume neighbors' broadcast status |
| Intra-MCU handoff | 0x01↔0x02, 0x03↔0x04, 0x05↔0x06, 0x07↔0x08 | corner pair | W1 | No | Same PCB; CellCAN per archetype |
| Diagnostics report | 0x02, 0x04, 0x06, 0x08 | 0x09 | W1 | No | ~1 Hz; **bring-up only** |
| Passive log capture | 0x01–0x08 (observed) | 0x09 | W1 | — | Bring-up only |

**Production minimum VCN count:**

| Category | Count |
|---|---|
| Chassis peer command pairs C(4,2) | 6 |
| Chassis status broadcast sources | 4 |
| Intra-corner chassis↔diagnostics pairs | 4 |
| **Total ordinary VCNs** | **14** |

**Explicitly absent:** diagnostics→peer chassis; cross-corner diagnostics commands; coordinator or gateway.

### 3.4 Forwarding (if any)

None.

### 3.5 Link profiles

| Physical link | Profile | WS Wire(s) | Notes |
|---|---|---|---|
| CellCAN | Committed CAN11 | W1 | **Explicit** VCN map required |

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
  Ordinary VCNs used:           14 / 32   (production minimum)
  Default map sufficient?       no
  Custom entries (if Explicit): 14
  MainA PID / MainB PID:         (unassigned) / (unassigned)
  Node positions used:          0 / 14
```

**Explicit VCN table (production minimum):**

| VCN | Relation | Purpose |
|---|---|---|
| 3 | — | Reserved Link control |
| 4–9 | chassis pairs | FL↔FR, FL↔RL, FL↔RR, FR↔RL, FR↔RR, RL↔RR |
| 10–13 | chassis → kBroadcast | FL, FR, RL, RR status |
| 14–17 | intra-corner pairs | 01↔02, 03↔04, 05↔06, 07↔08 |

VCNs 0–2 available by default but **unused**. Two-Main default allocation **not exercised**.

**Bring-up additions (not in production minimum):** VCNs 18–21 for diagnostics 0x02/04/06/08 → PC 0x09 (18 / 32 total — still one alias).

**Guest CAN11:** Not used.

---

## 4. Optional optimizations

### 4.1 CAN29 instead of CAN11

**Problem solved:** direct Participant naming without relation compression. **Minimum mapping gap:** none for required 14 VCNs. **Preference-dependent** at this scale; **clearly preferred** if mapper adopts full 8-node command mesh (~36 VCNs).

### 4.2 Default VCN map with fake MainA/MainB

**Rejected:** invents hierarchy; RL↔RR unreachable among chassis peers; does not cover intra-corner or diagnostics paths.

### 4.3 Full 8-node peer-command mesh (mistaken mapping)

C(8,2) = 28 pair VCNs + 8 broadcasts ≈ **36 ordinary VCNs** — exceeds one 32-VCN WireAlias. Archetype scale reference; **not required** by interactions (diagnostics do not command peer chassis). Documents stress boundary: second WireAlias, split map, or CAN29.

### 4.4 Second WireAlias for overflow

Split relations across Alias 1 + Alias 2 on same WireNumber. **Not needed** for required 14 VCNs; relevant only if §4.3 or directed status fan-out inflates count.

### 4.5 Separate Wire for diagnostics

**Rejected:** wire proliferation — one propagation domain.

---

## 5. Friction signals (minimum mapping happy path)

| Signal | None / Mild / Significant | One-sentence justification |
|---|---|---|
| Artificial Wire | None | W1 is the shared CAN cell for all eight domains. |
| Wire proliferation | None | One Wire. |
| Artificial hierarchy | None | Explicit VCN; no MainA/MainB. |
| VCN pressure | Mild | 14 / 32 for required interactions; full 8-node mesh would be Significant (~36). |
| WireAlias pressure | None | One alias on minimum path. |
| Configuration burden | Mild | Fourteen static relation entries — modest graph written once; coarser than per-message CAN IDs and likely fewer entries than 40+ ID conventional matrix at product scale. |
| Failure/topology mismatch | None | Corner MCU loss vs single-domain loss maps naturally. |

---

## 6. Model pressure

- **If you could change one WS concept** to simplify this system, what would it be? Peer-graph VCN authoring (generate pair list from domain roles: chassis mesh + intra-corner pairs) — scales better than hand-numbering at 14+ entries.
- **Did WS expose a useful distinction** the conventional model obscures? Yes: **dual-core Participant identity** on one transceiver is natural in R6; **default VCN boundary** is separate from **explicit fallback cost** — at required interaction scale the fallback is still modest; alias overflow appears only if requirements are over-modeled.

**Synthesis note:** Progression: hierarchical machine → default map; small peer graph (4 chassis + handoff) → explicit map, 14 VCNs; full 8-node command mesh → alias overflow → CAN29. This archetype's density target is met at the **stress boundary** (§4.3), not at the honest minimum (14 VCNs).

---

## 7. Open questions

- Archetype scale reference (~36 VCNs) assumes all eight domains in a command mesh; required interactions restrict peer commands to four chassis domains — do not inflate VCN count beyond spec.
- Diagnostics consuming chassis broadcast for DTC correlation — likely observation, no extra VCN.
- Intra-corner on CellCAN vs local shared memory — archetype mandates CellCAN.

---

## 8. Spec findings (optional)

| ID | Finding |
|---|---|
| SF-R6-006 | Default VCN has no node↔node slots; confirmed with 8-domain dual-core archetype. |

---

## 9. Diagrams (optional)

### Chassis peer mesh (4 domains — command authority)

```text
        FL (0x01)
       / | \
    FR (0x03)── RR (0x07)
       \ | /
        RL (0x05)

6 chassis pair VCNs + 4 broadcasts = 10 VCNs
+ 4 intra-corner pairs = 14 VCNs total (production)
```

### Dual-core per corner

```text
CellCAN
  FL: 0x01 Chassis ←→ 0x02 Diagnostics   VCN 14
  FR: 0x03 Chassis ←→ 0x04 Diagnostics   VCN 15
  RL: 0x05 Chassis ←→ 0x06 Diagnostics   VCN 16
  RR: 0x07 Chassis ←→ 0x08 Diagnostics   VCN 17
```

### Stress boundary (not minimum)

```text
If all 8 domains in full command mesh:
  28 pair VCNs + 8 broadcasts = 36 > 32  →  2nd alias or CAN29
```

### Rejected alternatives

| Approach | Outcome | Issue |
|---|---|---|
| **Chosen:** W1 + Explicit 14-VCN map | Valid | Modest; healthy headroom (14 / 32) |
| Rejected: default VCN + fake MainA/MainB | Incomplete | RL↔RR unreachable; invented hierarchy |
| Rejected: full 8-node peer mesh | ~36 VCNs | Exceeds one alias; not required |
| CAN29 | Valid alternative | Preference-dependent at 14 VCNs; clearer at 36 |

---

## Devices (reference)

| Device | Endpoint Domains | Link interfaces | Role summary |
|---|---|---|---|
| FL corner MCU | FL_Chassis, FL_Diagnostics | CellCAN | W1: 0x01, 0x02 |
| FR corner MCU | FR_Chassis, FR_Diagnostics | CellCAN | W1: 0x03, 0x04 |
| RL corner MCU | RL_Chassis, RL_Diagnostics | CellCAN | W1: 0x05, 0x06 |
| RR corner MCU | RR_Chassis, RR_Diagnostics | CellCAN | W1: 0x07, 0x08 |
| Developer PC | DevPC | CellCAN (USB, bring-up) | Optional 0x09 |

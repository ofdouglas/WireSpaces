# Sketch — Archetype 07: Dense Peer CAN (4-corner, superseded scale)

> **Scale note:** This sketch maps the original four-ECU archetype (one domain per corner). Current archetype 07 defines **eight domains** (dual-core per corner) — see [`07_peer_can_dense_8d_cursor.md`](07_peer_can_dense_8d_cursor.md).

```text
**Archetype:** archetypes/07_peer_can_dense.md (4-participant scale — historical)
**Agent:** cursor
**Date:** 2026-08-23 (revised assessment)
```

---

## Executive summary

```text
**Minimum mapping:** 4 Participants (production), 1 Logical Bus (W1 CellCAN) on committed CAN11 with Explicit custom VCN map (10 ordinary VCNs); no forwarding; optional PC in bring-up config only.
**Question A (R6):** Natural.
**Question B (CAN11):** Custom.
**Worst friction (minimum path):** Configuration burden — Mild.
**Main lesson:** R6 fits symmetric peers on one Logical Bus; default VCN does not — but explicit relation maps are modest at four peers (10 / 32 VCNs); falling outside the default is cheap for small graphs.
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
| Better with CAN29? | no (preference-dependent; see explanation) |

**Explanation:** Question A: four symmetric peers on one shared CAN segment map cleanly to one Logical Bus with four deployment-global ParticipantIds and no nominated coordinator. Directed commands and broadcast status use canonical Src/Dest; multi-initiator sourcing is the intended R6 model (`CORE §3.2`, `§3.4`). Observation of neighbors' broadcast status for fault reaction is configured consumption, not a topology extension.

Question B: the overlay default VCN map connects MainA/MainB to Node0–13 only — it has **no node↔node slots**. A full 4-peer command mesh needs six explicit pair VCNs plus four per-participant broadcast VCNs for status (10 ordinary VCNs total). Default map is insufficient even if two peers are mislabeled MainA/MainB (RL↔RR remains unreachable). Custom Explicit map fits comfortably in one WireAlias (10 / 32). A VCN names a **participant relationship** reused by all Services/Endpoints on that pair — coarser than a per-message CAN ID matrix, which often needs 12–24+ IDs for the same surface. CAN29 is simpler (direct Participant naming) but **not necessary** at this scale.

---

## 1. Native communication model

Four STM32-class ECUs (front-left, front-right, rear-left, rear-right) share one Classical CAN bus on a small mobile platform. There is **no firmware-designated bus master**. Each ECU:

- Publishes wheel speed, driver state, and fault flags at ~10 Hz to interested peers.
- May command any other ECU (torque enable, setpoint, fault reset) — event-driven and periodic.
- Reacts to neighbors' fault indications locally (e.g. cut torque if any reports slip).

Arbitration is CAN's electrical broadcast; application logic treats frames as "from whoever sent; act if addressed to me." Loss of one corner ECU degrades coordination; the others continue. No gateway and no coordinator failover.

An optional developer PC may attach via USB-CAN adapter during bring-up for **passive promiscuous capture** only. It is not part of normal runtime control; absence does not affect peers.

---

## 2. Obvious conventional implementation

> **Obvious conventional implementation:** Fixed 11-bit CAN ID matrix — typically one ID per (source, destination, message class) or per source with address field in payload; symmetric 4×4 interaction table; broadcast or multicast IDs for status where useful; no coordinator role in firmware.

> **What structure does a conventional design use?** Flat ID allocation table — often 12–24+ IDs when counted per source, destination, or message class; each ECU demultiplexes by CAN ID; peer symmetry is implicit in the matrix, not in a hierarchy.

WireSpaces adds: named Wire, ParticipantIds, Link Binding with VCN relation table, canonical Src/Dest per PDU. A VCN names an unordered participant **relationship**; all Services between that pair (torque enable, setpoint, fault reset, diagnostics, firmware, etc.) share one VCN. Ten topology entries may be **less** configuration than a conventional CAN database once the Service set grows — though the default hierarchical VCN map still does not fit node↔node peers without an explicit map.

---

## 3. Minimum mapping (required first)

### 3.1 Participants

| ParticipantId | Endpoint Domain | Device | Notes |
|---|---|---|---|
| 0x01 | CornerFL | Front-left ECU | Peer; symmetric authority |
| 0x02 | CornerFR | Front-right ECU | Peer |
| 0x03 | CornerRL | Rear-left ECU | Peer |
| 0x04 | CornerRR | Rear-right ECU | Peer |
| 0x05 | DevPC | Developer PC (optional) | Bring-up passive capture only; not in production minimum |

Four production Participants. No coordinator. Optional PC (0x05) is documented for bring-up configuration only — preconfigured W1 observer, not a runtime command authority.

### 3.2 Wires

| Wire (#) | Participants | Physical links | Purpose |
|---|---|---|---|
| W1 CellCAN | 0x01, 0x02, 0x03, 0x04 | CellCAN (Classical CAN) | Full peer mesh — status, commands, fault propagation |

One Logical Bus matches one physical propagation domain.

**Bring-up variant:** W1 membership may include 0x05 for passive observation during commissioning; production deployment omits 0x05 from configured interactions.

### 3.3 Interactions (primary)

| Interaction | Src | Dest | Wire | Broadcast? | Notes |
|---|---|---|---|---|---|
| Corner status publication | 0x01–0x04 (each) | kBroadcast | W1 | Yes | ~10 Hz per ECU; peers observe (`CORE §12.6`) |
| Directed peer command | Any 0x01–0x04 | Any other 0x01–0x04 | W1 | No | Torque enable, setpoint, fault reset |
| Fault reaction (local) | — | — | — | — | Consume neighbors' status/fault PDUs; local action |
| Passive log capture | 0x01–0x04 (observed) | 0x05 (bring-up only) | W1 | — | PC observes electrically visible traffic |

**Directed pair count:** C(4,2) = 6 unordered peer pairs; each uses one VCN with Direction bit for AToB/BToA.

### 3.4 Forwarding (if any)

None. Single physical CAN, single Wire, no gateway.

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
  Ordinary VCNs used:           10 / 32
  Default map sufficient?       no
  Custom entries (if Explicit): 10
  MainA PID / MainB PID:         (unassigned) / (unassigned)
  Node positions used:          0 / 14
```

**Explicit VCN table (production minimum):**

| VCN | Relation | Purpose |
|---|---|---|
| 3 | — | Reserved Link control |
| 4 | 0x01 ↔ 0x02 | FL ↔ FR commands |
| 5 | 0x01 ↔ 0x03 | FL ↔ RL |
| 6 | 0x01 ↔ 0x04 | FL ↔ RR |
| 7 | 0x02 ↔ 0x03 | FR ↔ RL |
| 8 | 0x02 ↔ 0x04 | FR ↔ RR |
| 9 | 0x03 ↔ 0x04 | RL ↔ RR |
| 10 | 0x01 → kBroadcast | FL status |
| 11 | 0x02 → kBroadcast | FR status |
| 12 | 0x03 → kBroadcast | RL status |
| 13 | 0x04 → kBroadcast | RR status |

VCNs 0–2 available by default but **unused**. Two-Main default allocation **not exercised**.

**Guest CAN11:** Not used.

---

## 4. Optional optimizations

### 4.1 CAN29 instead of CAN11

CAN29 carries full canonical ParticipantIds per frame. **Minimum mapping gap:** none — CAN11 custom map is healthy at four peers (10 / 32). CAN29 is cleaner but **preference-dependent**, not required.

### 4.2 Default VCN map with nominated MainA/MainB

**Rejected:** invents hierarchy; **incomplete** — RL↔RR unreachable; only five of six chassis pairs coverable.

### 4.3 One Wire per peer pair

**Rejected:** wire proliferation on one propagation domain.

---

## 5. Friction signals (minimum mapping happy path)

| Signal | None / Mild / Significant | One-sentence justification |
|---|---|---|
| Artificial Wire | None | W1 is the shared CAN cell. |
| Wire proliferation | None | One Wire for four peers. |
| Artificial hierarchy | None | Explicit VCN with no MainA/MainB assignment. |
| VCN pressure | Mild | Ten ordinary VCNs (10 / 32); pairs and broadcasts enumerated once. |
| WireAlias pressure | None | One alias. |
| Configuration burden | Mild | Ten static relation entries — the communication graph written once; coarser than per-message CAN IDs. |
| Failure/topology mismatch | None | Peer degradation maps naturally. |

---

## 6. Model pressure

- **If you could change one WS concept** to simplify this system, what would it be? Peer-graph authoring view for Explicit VCN maps — mechanism works; tooling helps at larger scale.
- **Did WS expose a useful distinction** the conventional model obscures? Yes: R6 peer equality works; default VCN has a clear boundary; falling outside it is **cheap** for small peer networks.

**Synthesis note:** Hierarchical machine → default map; small unusual peer graph → explicit relation map; large/arbitrary peer graph → CAN29. This sketch sits in the middle bucket.

---

## 7. Open questions

- Broadcast vs directed status fan-out — broadcast chosen as minimum.
- QoS Critical on torque-enable within existing pair VCNs — likely sufficient.

---

## 8. Spec findings (optional)

| ID | Finding |
|---|---|
| SF-R6-006 | Default VCN map has no node↔node slots; fake MainA/MainB incomplete for ≥3 leaves. |

---

## 9. Diagrams (optional)

```text
Wire W1 CellCAN — 4 peers, 6 pair VCNs + 4 broadcast VCNs = 10 / 32
No MainA/MainB. No gateway. No coordinator.
```

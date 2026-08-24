# Sketch — Archetype 09: Distributed Chassis CAN Cell

```text
**Archetype:** archetypes/09_distributed_chassis_can_cell.md
**Agent:** cursor_B
**Output file:** sketches/09_distributed_chassis_can_cell_cursor_B.md
**Date:** 2026-08-23
```

---

## Executive summary

```text
**Minimum mapping:** Config A — 9 production Participants, 1 Logical Bus (W1 CellCAN) on committed CAN11 Explicit map (29 / 31 usable ordinary VCN slots); Config B adds DevPC (+4 relations → 33, exceeds one alias by 2 → second WireAlias or CAN29).
**Question A (R6):** Natural — strong pass.
**Question B (CAN11):** Custom — viable but near practical boundary.
**Worst friction (minimum path):** VCN pressure — Significant.
**Main lesson:** R6 stays clean (9 Participants, 1 Wire, 0 gateways) while CAN11's finite relation namespace constrains the deployment; Config B demonstrates multi-alias representation partitioning, not alias-resource exhaustion.
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

**Explanation:** Question A: nine production Endpoint Domains on five ECUs (four dual-core corners plus VehicleState) map to nine deployment-global ParticipantIds on one electrical CAN segment with no gateway and no nominated master. Two independent four-peer meshes (Chassis and Supervisor) plus four corner-local cross-domain pairs and VehicleState spokes are explicit interaction subsets on one Logical Bus — not a full 9×9 mesh. Three distinct authority patterns (symmetric chassis peers, symmetric supervisors with different responsibilities, global read-only VehicleState) coexist without a coordinator Participant. Dual-core corners share one transceiver with two ParticipantIds per MCU, which is ordinary R6 (`DEPLOY §1.6`). **Strong pass.**

Question B: the overlay default VCN map (`overlay §5`) provides Main↔Node star slots only — **no node↔node pairs** for either peer mesh. Production minimum requires **29 explicit ordinary VCNs**; with VCN 3 reserved for Link control, one alias has **31 usable ordinary slots** — Config A is **29 / 31** with **2 spare** (VCNs 1–2). Config B adds four DevPC↔Supervisor relations → **33 total**, **exceeding one alias by 2** — second WireAlias on the same Wire (representation partition) or CAN29 (`overlay §8`, `§10`). CAN11 has not failed: Config A is valid with almost no growth margin; Config B with two aliases is also valid. CAN29 becomes preferable when growth, routine service tooling, or additional domains are expected — not because CAN11 is invalid. Configuration authoring is **moderate** (29 graph edges, each shared by multiple Services via Endpoint); what is **significant** is that the finite VCN namespace is now binding. MainB and two-Main default allocation are **not exercised**.

---

## 1. Native communication model

Five ECUs share one Classical CAN bus (CellCAN) in a distributed chassis cell. Four dual-core corner MCUs (FL, FR, RL, RR) each host two Endpoint Domains on the same transceiver:

- `*_Chassis` — wheel speed, driver state, torque enable/setpoint/reset; symmetric peer commands among chassis domains; reacts to neighbor chassis faults.
- `*_Supervisor` — local fault management, DTCs, calibration; **cross-corner** supervisor peer mesh for health/fault exchange, degraded-mode awareness, and coordinated reset/recovery **permission** — supervisors do **not** command peer chassis.

A single-core **VehicleState** ECU publishes fused IMU/yaw/acceleration, vehicle-motion estimates, and global traction state at ~20 Hz; it has **no command authority** over corners. Chassis domains feed back to VehicleState at ~10 Hz.

There is **no firmware-designated bus master** and no gateway. Arbitration is CAN electrical broadcast; application logic acts on frames addressed to it.

**Config A (production):** nine Participants, no developer PC.

**Config B (bring-up / service):** same as Config A plus optional **DevPC** on CellCAN via USB-CAN adapter. DevPC queries each corner Supervisor for logs, DTCs, and calibration — maintenance only; not a runtime coordinator and not on chassis torque paths.

**Failure assumptions:**

- Loss of one corner MCU: both Chassis and Supervisor on that corner offline; remaining corners continue with degraded meshes.
- Loss of VehicleState: chassis peer mesh may continue; global traction estimate unavailable.
- Loss of Supervisor only on one corner (unusual): Chassis on that corner may continue locally; cross-corner degraded-mode coordination limited.
- DevPC absent: no effect on production traffic.

---

## 2. Obvious conventional implementation

> **Obvious conventional implementation:** Structured 11-bit CAN ID matrix — separate ID ranges per relation class (chassis peer commands, chassis status, supervisor peer health, supervisor broadcasts, corner-local handoff, VehicleState feeds, DevPC maintenance); ~35–50+ IDs when fully enumerated; each ECU demultiplexes by CAN ID with symmetric peer tables and no coordinator role.

> **What structure does a conventional design use?** Flat per-relation CAN ID allocation; dual-core domains on one MCU share one driver with local demux to the correct core; VehicleState as a broadcast + directed spoke pattern; bring-up PC uses maintenance ID block. Membership is implicit (everyone hears the bus electrically).

WireSpaces adds: named Wire, deployment-global ParticipantIds, Link Binding with Explicit VCN relation table, canonical Src/Dest per PDU. For this archetype the WS layer **re-encodes** the same pairwise and broadcast relations the CAN matrix already names — with 29+ explicit entries where the default VCN map offers no peer slots.

---

## 3. Minimum mapping (required first)

### 3.1 Participants

| ParticipantId | Endpoint Domain | Device | Notes |
|---|---|---|---|
| 0x01 | FL_Chassis | FL corner MCU, chassis core | Chassis peer mesh |
| 0x02 | FL_Supervisor | FL corner MCU, supervisor core | Supervisor peer mesh |
| 0x03 | FR_Chassis | FR corner MCU, chassis core | Chassis peer mesh |
| 0x04 | FR_Supervisor | FR corner MCU, supervisor core | Supervisor peer mesh |
| 0x05 | RL_Chassis | RL corner MCU, chassis core | Chassis peer mesh |
| 0x06 | RL_Supervisor | RL corner MCU, supervisor core | Supervisor peer mesh |
| 0x07 | RR_Chassis | RR corner MCU, chassis core | Chassis peer mesh |
| 0x08 | RR_Supervisor | RR corner MCU, supervisor core | Supervisor peer mesh |
| 0x09 | VehicleState | VehicleState ECU | Global state producer; no command authority |
| 0x0A | DevPC | Developer PC (Config B only) | Maintenance queries to Supervisors only |

**Chassis set:** {0x01, 0x03, 0x05, 0x07}. **Supervisor set:** {0x02, 0x04, 0x06, 0x08}.

Nine production Participants (Config A). DevPC (0x0A) is a tenth Participant in Config B only — not a production minimum member.

### 3.2 Wires

| Wire (#) | Participants | Physical links | Purpose |
|---|---|---|---|
| W1 CellCAN | 0x01–0x09 (Config A); 0x01–0x0A (Config B) | CellCAN (Classical CAN) | Chassis peer mesh, supervisor peer mesh, corner-local handoff, VehicleState spokes, optional DevPC maintenance |

One Logical Bus matches one physical propagation domain. Chassis and Supervisor traffic share the electrical segment but are **distinct interaction subsets** on the same Wire — not separate Wires (`brief §5` step 7 not justified).

### 3.3 Interactions (primary)

#### Config A — production minimum

| Interaction | Src | Dest | Wire | Broadcast? | Notes |
|---|---|---|---|---|---|
| Chassis status | each Chassis 0x01/03/05/07 | kBroadcast | W1 | Yes | ~10 Hz; peers observe for fault reaction (`CORE §12.6`) |
| Peer torque / setpoint | any Chassis | any other Chassis | W1 | No | Event + periodic; core symmetric path |
| Chassis fault reaction | — | — | — | — | Local consume of neighbor chassis status/fault PDUs; not a separate VCN |
| Supervisor health / fault | each Supervisor 0x02/04/06/08 | peer Supervisors | W1 | No | ~2–5 Hz; includes coordinated reset permission on same pair VCN |
| Supervisor status | each Supervisor | kBroadcast | W1 | Yes | ~2 Hz; interested peers consume |
| Chassis ↔ Supervisor handoff | 0x01↔0x02, 0x03↔0x04, 0x05↔0x06, 0x07↔0x08 | same corner | W1 | No | Event; fault escalation, mode, recovery |
| Motion / traction state | 0x09 | each Chassis 0x01/03/05/07 | W1 | No | ~20 Hz directed |
| Chassis feedback | each Chassis | 0x09 | W1 | No | ~10 Hz; same pair VCN as motion feed (bidirectional relation) |
| Global traction broadcast | 0x09 | kBroadcast | W1 | Yes | ~10 Hz; one broadcast source |

**Explicitly absent:** Supervisor → peer Chassis commands; VehicleState → torque/enable; DevPC → chassis; coordinator or gateway traffic; full 9×9 mesh.

#### Config B — bring-up additions (not in production minimum VCN count)

| Interaction | Src | Dest | Wire | Broadcast? | Notes |
|---|---|---|---|---|---|
| Supervisor query | 0x0A | each Supervisor 0x02/04/06/08 | W1 | No | On demand — logs, DTCs, calibration |
| Supervisor response | each Supervisor | 0x0A | W1 | No | On demand |

**Production minimum ordinary VCN count:**

| Category | Count | Basis |
|---|---|---|
| Chassis peer command pairs | 6 | C(4,2) among {0x01, 0x03, 0x05, 0x07} |
| Chassis status broadcast sources | 4 | One per Chassis domain |
| Supervisor peer pairs | 6 | C(4,2) among {0x02, 0x04, 0x06, 0x08} |
| Supervisor status broadcast sources | 4 | One per Supervisor domain |
| Local Chassis ↔ Supervisor pairs | 4 | One per corner |
| VehicleState ↔ Chassis pairs | 4 | One per corner chassis |
| VehicleState broadcast | 1 | Global traction |
| **Config A total** | **29** | |
| DevPC ↔ Supervisor pairs (Config B) | +4 | One per corner supervisor |
| **Config B total** | **33** | Exceeds one alias by 2 (31 usable ordinary slots per alias) |

### 3.4 Forwarding (if any)

None. Single physical CAN, single Wire, no gateway.

### 3.5 Link profiles

| Physical link | Profile | WS Wire(s) | Notes |
|---|---|---|---|
| CellCAN | Committed CAN11 | W1 | **Explicit** VCN map required; Config A one alias; Config B may need second alias |

### 3.6 CAN11 bindings (if CAN11 used)

#### Config A — production (one WireAlias)

```text
Profile:          committed
WireAliases used:             1 / 8
  default map:                  0
  custom map:                   1

Per alias:
  Alias:                        1
  Canonical Wire:               W1 (CellCAN)
  Mapping:                      Explicit
  Ordinary VCNs used:           29 / 31   (production minimum; VCN 3 reserved — not an ordinary slot)
  VCNs available, unused:       VCN 1, VCN 2 (default Main slots; 2 spare)
  Default map sufficient?       no
  Custom entries (if Explicit): 29
  MainA PID / MainB PID:         (unassigned) / (unassigned)
  Node positions used:          0 / 14
```

**Explicit VCN table (Config A production minimum):**

| VCN | Relation | Purpose |
|---|---|---|
| 0 | 0x09 → kBroadcast | VehicleState global traction broadcast |
| 3 | — | Reserved Link control |
| 4 | 0x01 ↔ 0x03 | FL ↔ FR chassis peer commands |
| 5 | 0x01 ↔ 0x05 | FL ↔ RL chassis |
| 6 | 0x01 ↔ 0x07 | FL ↔ RR chassis |
| 7 | 0x03 ↔ 0x05 | FR ↔ RL chassis |
| 8 | 0x03 ↔ 0x07 | FR ↔ RR chassis |
| 9 | 0x05 ↔ 0x07 | RL ↔ RR chassis |
| 10 | 0x01 → kBroadcast | FL chassis status |
| 11 | 0x03 → kBroadcast | FR chassis status |
| 12 | 0x05 → kBroadcast | RL chassis status |
| 13 | 0x07 → kBroadcast | RR chassis status |
| 14 | 0x02 ↔ 0x04 | FL ↔ FR supervisor peer |
| 15 | 0x02 ↔ 0x06 | FL ↔ RL supervisor |
| 16 | 0x02 ↔ 0x08 | FL ↔ RR supervisor |
| 17 | 0x04 ↔ 0x06 | FR ↔ RL supervisor |
| 18 | 0x04 ↔ 0x08 | FR ↔ RR supervisor |
| 19 | 0x06 ↔ 0x08 | RL ↔ RR supervisor |
| 20 | 0x02 → kBroadcast | FL supervisor status |
| 21 | 0x04 → kBroadcast | FR supervisor status |
| 22 | 0x06 → kBroadcast | RL supervisor status |
| 23 | 0x08 → kBroadcast | RR supervisor status |
| 24 | 0x01 ↔ 0x02 | FL chassis ↔ supervisor handoff |
| 25 | 0x03 ↔ 0x04 | FR corner local |
| 26 | 0x05 ↔ 0x06 | RL corner local |
| 27 | 0x07 ↔ 0x08 | RR corner local |
| 28 | 0x09 ↔ 0x01 | VehicleState ↔ FL chassis |
| 29 | 0x09 ↔ 0x03 | VehicleState ↔ FR chassis |
| 30 | 0x09 ↔ 0x05 | VehicleState ↔ RL chassis |
| 31 | 0x09 ↔ 0x07 | VehicleState ↔ RR chassis |

VCNs 1–2 (default MainA/MainB broadcast and MainA↔MainB) **available in profile but unused**. Two-Main default allocation **not exercised** and **not applicable**.

Multiple message classes (torque command vs fault reset vs health exchange vs reset permission) share one VCN per unordered pair; Endpoint and PDUA distinguish Services (`overlay §4`).

#### Config B — bring-up (WireAlias overflow)

```text
Profile:          committed
WireAliases used:             2 / 8
  default map:                  0
  custom map:                   2

Alias 1:  W1 CellCAN — production 29 VCNs (table above)
Alias 2:  W1 CellCAN — bring-up DevPC relations only

Per alias 2:
  Alias:                        2
  Canonical Wire:               W1 (CellCAN)
  Mapping:                      Explicit
  Ordinary VCNs used:           4 / 31   (Config B bring-up only; VCN 3 reserved)
  Default map sufficient?       no
  Custom entries:               4
```

| VCN (Alias 2) | Relation | Purpose |
|---|---|---|
| 4 | 0x0A ↔ 0x02 | DevPC ↔ FL Supervisor |
| 5 | 0x0A ↔ 0x04 | DevPC ↔ FR Supervisor |
| 6 | 0x0A ↔ 0x06 | DevPC ↔ RL Supervisor |
| 7 | 0x0A ↔ 0x08 | DevPC ↔ RR Supervisor |

**Config B total relations: 33** (29 on Alias 1 + 4 on Alias 2). Alias 1 is at **29 / 31** usable slots; adding four DevPC relations to Alias 1 would need 33 slots in one map — **overflow by 2**. Second alias is a **representation partition** when one VCN map is full — the same machinery as staged migration (`overlay §8`), but here motivated by relation-capacity, not map replacement:

```text
Alias 1 → W1, production relationships (29 / 31)
Alias 2 → W1, DevPC relationships (4 / 31)
```

Egress resolves `canonical {Wire, Src, Dest}` → `{WireAlias, VCN, Direction}` with no canonical ambiguity. This is **not** alias-resource pressure (`2 / 8` aliases used).

**Alternative for Config B:** CAN29 on CellCAN eliminates VCN enumeration and alias split (`overlay §10`) — preferable when bring-up maintenance is routine or growth is expected.

**Guest CAN11:** Not used. Mixed Guest + committed on one bus not specified (`overlay §13`).

**Membership vs presence:** Config A committed configuration omits 0x0A entirely. Config B **preconfigures** 0x0A as W1 member with Alias 2 bindings; when DevPC is unplugged, Wiring persists and delivery to 0x0A fails or queues drain — not removed from membership at runtime (`brief §6.4`).

---

## 4. Optional optimizations

### 4.1 CAN29 instead of CAN11 (Config A or B)

Replace committed CAN11 Explicit map with CAN29 carrying full canonical ParticipantIds. **Problem solved:** relation-capacity constraint and Config B alias partition disappear; peer meshes need no fake hierarchy; growth headroom for more domains or service tooling. **Minimum mapping gap:** CAN11 works for Config A production (**29 / 31**, almost no margin) and Config B (two aliases, valid); CAN29 is the obvious engineering choice when growth matters — overlay §10.

### 4.2 Default VCN map with nominated MainA/MainB

Assign FL_Chassis=MainA, FR_Chassis=MainB, RL/RR as Node0/Node1. **Rejected:** (a) invents hierarchy firmware lacks; (b) default map has no Node↔Node slots — RL↔RR and all supervisor peer pairs unreachable; (c) does not cover VehicleState spokes or supervisor mesh; (d) incomplete for both peer meshes.

### 4.3 Full 9×9 or 8×8 all-to-all mesh (mistaken mapping)

Treat all domains as symmetric peers. **Rejected:** archetype explicitly excludes supervisor→peer chassis, VehicleState commands, and full mesh; would inflate VCN count far beyond 33 with no semantic benefit.

### 4.4 Separate Wires for Chassis vs Supervisor scope

W1 chassis + W2 supervisor on same physical bus. **Rejected:** wire proliferation — one propagation domain, distinct interaction subsets already modeled by Participant role and VCN pairs.

### 4.5 Pack Config B DevPC relations into Alias 1 by dropping broadcasts

Replace four supervisor status broadcast VCNs with directed equivalents to free slots for DevPC pairs. **Problem solved:** stay on one alias for bring-up. **Rejected for minimum mapping:** archetype counts supervisor broadcasts in the 29 production VCNs; removing them changes the production minimum, not an honest Config A mapping. Directed-only supervisor status is an optional optimization only if broadcast VCNs are genuinely unused — they are required at ~2 Hz in the archetype.

### 4.6 Guest CAN for DevPC maintenance block

Allocate Guest-3 block for DevPC queries. **Rejected:** mixed Guest + committed ingress on one physical bus not specified (`overlay §13`); DevPC relations fit cleanly on Alias 2 committed map.

### 4.7 Move intra-corner Chassis↔Supervisor to local inter-core Link

Route the four corner-local handoff relations off CellCAN onto per-MCU shared-memory Links. **Problem solved:** reduces CAN VCN count from 29 to **25**; adds local Wires per corner MCU. **Minimum mapping gap:** archetype deliberately routes same-MCU traffic through CellCAN to stress CAN; a real product may optimize locally. Do not cite "credible systems naturally hit 29" without noting this assumption — but 25 is already substantial, and DevPC alone brings a 25-relation map to 29.

### 4.8 Multi-alias as first-class representation partition

Document explicitly that multiple WireAliases may bind the same canonical Wire for **relation-capacity partitioning** (not only staged VCN-map migration). **Problem solved:** makes Config B's Alias 2 pattern normative rather than accidental reuse of migration machinery.

---

## 5. Friction signals (minimum mapping happy path)

| Signal | None / Mild / Significant | One-sentence justification |
|---|---|---|
| Artificial Wire | None | W1 is the shared CAN cell — one meaningful propagation domain for all production Participants. |
| Wire proliferation | None | One Wire for nine production Participants on one bus. |
| Artificial hierarchy | None | Explicit VCN map with no MainA/MainB assignment; no nominated coordinator. |
| VCN pressure | Significant | 29 / 31 usable slots in Config A — CAN11's finite relation namespace is binding; Config B at 33 relations exceeds one alias by 2. |
| WireAlias pressure (Config A) | None | One alias; 2 / 31 spare VCN slots — no partition needed. |
| WireAlias pressure (Config B) | Mild | Second alias is representation partition (`2 / 8` aliases), not alias-resource exhaustion — architecturally interesting, not capacity pressure. |
| Configuration burden | Moderate | 29 graph edges (each shared by multiple Services via Endpoint) is noticeable if hand-authored but comparable to a ~35–50 ID conventional CAN database; generated config is tractable. |
| Failure/topology mismatch | Mild | Corner MCU loss and VehicleState loss map naturally; supervisor-only loss on one corner is documentable; no failover fiction. |

---

## 6. Model pressure

- **If you could change one WS concept** to simplify this system, what would it be? Explicitly support multi-alias **representation partitioning** on one canonical Wire (capacity split, not only migration); or default to CAN29 recommendation for cells above ~25 participant relations on one Wire.
- **Did WS expose a useful distinction** the conventional model obscures? Yes: **Question A** is the headline — nine Participants, one Wire, zero gateways, while hosting three distinct authority patterns (symmetric chassis peers, symmetric supervisors with different responsibilities, global read-only state). Origin/Node would have fought this topology. **Question B** localizes awkwardness to CAN11 relation-capacity (high-20s per Wire is where CAN29 should be actively considered), not to R6 identity or Wire shape.

---

## 7. Open questions

- Whether supervisor health exchange and coordinated reset permission can share one VCN per supervisor pair without Endpoint ambiguity — assumed yes (same unordered pair, different Endpoints).
- Supervisor consumption of chassis broadcast for correlated DTCs — likely observation of VCNs 10–13 without extra VCNs.
- Intra-corner Chassis↔Supervisor on CellCAN vs local inter-core Link on same MCU — archetype mandates CellCAN (four VCNs); moving off-bus reduces production count **29 → 25**; basic ceiling conclusion survives (DevPC brings 25-relation map to 29).
- Config B production deployment: is Alias 2 left installed (dormant DevPC bindings) or applied only in maintenance configuration export?
- QoS for torque-enable paths: Critical QoS bit within existing chassis pair VCNs vs separate Wire — QoS bits likely suffice.
- VehicleState loss: do chassis domains fall back to peer-only traction estimate, or enter faulted mode — application policy, not topology.

---

## 8. Spec findings (optional)

| ID | Finding |
|---|---|
| SF-R6-007 | Credible distributed chassis cell: **29 / 31** usable ordinary VCN slots in Config A; Config B **33 relations exceeds one alias by 2** — second WireAlias as representation partition on same Wire (not alias-resource exhaustion); first empirical CAN11 boundary ≈ high-20s participant relations per Wire → actively consider CAN29 (`archetype 09`, `overlay §8`, `§10`). |
| SF-R6-008 | Multi-alias same-Wire use should include **relation-capacity partitioning**, not only staged VCN-map migration (`overlay §8`). |

---

## 9. Diagrams (optional)

### Device-centric

```text
CellCAN (Classical CAN 11-bit) — one segment, no gateway
┌──────────────────────────────────────────────────────────────────┐
│  FL MCU       FR MCU       RL MCU       RR MCU      VehicleState │
│  ┌────────┐   ┌────────┐   ┌────────┐   ┌────────┐   ┌────────┐ │
│  │01 Chas │   │03 Chas │   │05 Chas │   │07 Chas │   │09 VS   │ │
│  │02 Sup  │   │04 Sup  │   │06 Sup  │   │08 Sup  │   │        │ │
│  └───┬────┘   └───┬────┘   └───┬────┘   └───┬────┘   └───┬────┘ │
│      └────────────┴─────────────┴─────────────┴────────────┘     │
│                         shared bus                                │
└──────────────────────────────────────────────────────────────────┘
              Config B optional: DevPC 0x0A (USB-CAN)
```

### Interaction structure (reference)

```text
Chassis mesh (4 peers)              Supervisor mesh (4 peers)
     01──03                              02──04
      |\ /|                               |\ /|
      | X |                               | X |
      |/ \|                               |/ \|
     05──07                              06──08

Corner local pairs: 01↔02, 03↔04, 05↔06, 07↔08
VehicleState: 09↔each Chassis; 09→Broadcast

Config B: 0x0A ↔ each Supervisor (Alias 2)
```

### Wire-centric (Config A production minimum)

```text
Wire W1 CellCAN (CAN11 Alias 1, Explicit VCN, 29 / 31 ordinary)
├── Chassis peer mesh {01,03,05,07}
│     6 pair VCNs (4–9); 4 broadcast VCNs (10–13)
├── Supervisor peer mesh {02,04,06,08}
│     6 pair VCNs (14–19); 4 broadcast VCNs (20–23)
├── Corner local handoff
│     4 pair VCNs (24–27)
└── VehicleState
      4 pair VCNs (28–31); 1 broadcast VCN (0)

No MainA/MainB. No gateway. No coordinator.
```

### Rejected alternatives

| Approach | Outcome | Issue |
|---|---|---|
| **Chosen:** W1 + Explicit 29-relation map (Config A) | Valid | 29 / 31 usable; 2 spare VCN slots |
| Config B: +Alias 2 for DevPC (4 relations) | Valid | Representation partition; 33 relations, overflow by 2 |
| Rejected: default VCN + fake Main | Incomplete | Peer meshes unreachable |
| Rejected: full 9×9 mesh | ≫33 VCNs | Not required by archetype |
| **CAN29** (optimization) | Likely better | Especially for Config B routine maintenance |

---

## Devices (reference)

| Device | Endpoint Domains | Link interfaces | Role summary |
|---|---|---|---|
| FL corner MCU | FL_Chassis, FL_Supervisor | CellCAN | W1: 0x01, 0x02 |
| FR corner MCU | FR_Chassis, FR_Supervisor | CellCAN | W1: 0x03, 0x04 |
| RL corner MCU | RL_Chassis, RL_Supervisor | CellCAN | W1: 0x05, 0x06 |
| RR corner MCU | RR_Chassis, RR_Supervisor | CellCAN | W1: 0x07, 0x08 |
| VehicleState ECU | VehicleState | CellCAN | W1: 0x09; global state only |
| Developer PC | DevPC | CellCAN (USB, Config B) | W1: 0x0A; maintenance only |

# WireSpaces Sketch Synthesis

**Status:** Structure only — synthesis content not yet written  
**Purpose:** Aggregate findings across architecture sketches; compare independent agent outputs; track spec gaps and documentation follow-ups surfaced by the sketch program.

**Related:** [`README.md`](README.md) (approach and per-sketch template), individual sketches `01`–`06`, independent variants in [`../sketches_b/`](../sketches_b/).

---

## How to use this document

| Section | Who updates it | When |
|---|---|---|
| [Sketch rollup](#sketch-rollup) | Human or lead agent | After each sketch reaches review-ready draft |
| [Friction heatmap](#friction-heatmap) | Lead agent | After 3+ sketches exist; refresh when new configs land |
| [Recurring patterns](#recurring-patterns) | Lead agent | When the same mapping or lesson appears in 2+ sketches |
| [Anti-patterns](#anti-patterns) | Lead agent | When a rejected decomposition appears in 2+ sketches |
| [Model pressure themes](#model-pressure-themes) | Lead agent | Phase 3 prep; cluster per-sketch model-pressure notes |
| [Agent comparison](#agent-comparison) | Reviewer | After Phase 2 independent sketches (e.g. `sketches_b`) |
| [Aggregate verdicts](#aggregate-verdicts) | Human | Phase 3 — not before sketches 01–06 and comparison pass |
| [Spec findings log](#spec-findings-log) | Any sketching agent | Whenever a sketch exposes ambiguity, gap, or stress in the normative docs |
| [Documentation follow-ups](#documentation-and-product-follow-ups) | Human or lead agent | When a sketch finding should change `CORE`, `DEPLOY`, `LINK`, tooling, or patterns |

Do **not** fill synthesis sections with architecture fan fiction. Record only what sketches and comparisons actually showed.

---

## Sketch rollup

One subsection per sketch file. Copy the **executive summary** from each sketch when present; expand only with cross-sketch links.

### `01_dev_board.md`

<!-- Executive summary: -->
<!-- Mapping choice: -->
<!-- Worst friction signal: -->
<!-- Main lesson: -->
<!-- Configs covered: A, B -->

### `02_peer_can.md`

<!-- Executive summary: -->
<!-- Mapping choice: -->
<!-- Worst friction signal: -->
<!-- Main lesson: -->

### `03_rs485_sensors.md`

<!-- Executive summary: -->
<!-- Mapping choice: -->
<!-- Worst friction signal: -->
<!-- Main lesson: -->
<!-- Configs covered: A, B -->

### `04_pi_robot.md`

<!-- Executive summary: -->
<!-- Mapping choice: -->
<!-- Worst friction signal: -->
<!-- Main lesson: -->
<!-- Configs covered: A, B, C -->

### `05_multicore_gateway.md`

<!-- Not yet drafted -->

### `06_redundant_gateway.md`

<!-- Not yet drafted -->

---

## Friction heatmap

Rates are **None / Mild / Significant** from each sketch's chosen happy-path mapping. Leave blank until rollup is written.

| Signal | 01-A | 01-B | 02 | 03-A | 03-B | 04-A | 04-B | 04-C | 05 | 06 |
|---|---|---|---|---|---|---|---|---|---|---|
| Artificial Origin | | | | | | | | | | |
| Artificial Wire | | | | | | | | | | |
| Wire proliferation | | | | | | | | | | |
| Forwarding tax | | | | | | | | | | |
| Identity awkwardness | | | | | | | | | | |
| Interaction awkwardness | | | | | | | | | | |
| Configuration burden | | | | | | | | | | |
| Role instability | | | | | | | | | | |
| Failure mismatch | | | | | | | | | | |

**Notes on reading the heatmap:**

- High **Interaction awkwardness** on `02` with low elsewhere suggests a class boundary, not a global flaw.
- **Failure mismatch** on `03-B` may be honest modeling cost, not a mapping error — distinguish in notes below.

### Heatmap commentary

<!-- Patterns across columns; which signals cluster; which sketches are outliers -->

---

## Recurring patterns

Positive mappings and distinctions that appear across multiple sketches. **Do not populate until evidence exists.**

### Authority-shaped Wires spanning Links

<!-- 01-B, 04: gateway forwards same Wire; second Wire = second Origin, not second cable -->

### Forward vs compose (gateway)

<!-- 01-B, 04: same-Wire forward preserves PDU; cross-Wire is application composition — SF-004, SF-015 -->

Do not label cross-Wire relay or Bench→Plant maintenance as "forwarding." See [`SF-004`](synthesis.md#spec-findings-log), [`SF-015`](synthesis.md#spec-findings-log).

### Snapshot + Link poll scheduler

<!-- 03: Service publication vs LLL initiation -->

### Semantic Origin ≠ Link poll initiator

<!-- 03-B: one Wire, multiple poll schedulers -->

### Observation on broadcast Links

<!-- 01-B, 02: configured consumption of others' NodeToOrigin; see stress-test note below -->

### Overlapping Wire membership

<!-- 01-B, 04: same device Origin on one Wire, Node on another -->

### Wiring persists when Origin offline

<!-- 04-C: no role reassignment -->

### Minimum machinery / resisted splits

<!-- 01-A: no forced maintenance Wire on one cable -->

---

## Anti-patterns

Rejected decompositions seen in more than one sketch. **Do not populate until evidence exists.**

### Physical-link-shaped Wires with gateway translation

<!-- USB Wire + CAN Wire + cross-Wire translate -->

### Forced maintenance / application plane split

<!-- When one link and one authority already suffice -->

### Duplicate publication onto overlapping Wires

<!-- When forward or observation suffices -->

### Nominated coordinator Origin without native authority

<!-- 02 one-Wire alternative -->

### Re-origination disguised as forwarding

<!-- Peer command relay through hub -->

---

## Model pressure themes

Clustered answers to "change one concept" and "useful distinction" across sketches. **Phase 3 input.**

| Theme | Sketches | Proposed direction | Doc target |
|---|---|---|---|
| | | | |

---

## Agent comparison

Phase 2: independent agents sketch the same system from the same neutral description. Record divergences here — not in individual sketch files.

### Comparison matrix

| Sketch | Primary (`sketches/`) | Variant (`sketches_b/` or other) | Same native model? | Wire count | Primary divergence | Convergence signal |
|---|---|---|---|---|---|---|
| `01_dev_board` | `01_dev_board.md` | `sketches_b/01_dev_board.md` | | | | |
| `02_peer_can` | `02_peer_can.md` | `sketches_b/02_peer_can.md` | | | | |

### `01_dev_board` — comparison notes

<!-- e.g. child count (2 vs 3), Wire naming, observation vs forward-only PC path -->

### `02_peer_can` — comparison notes

<!-- e.g. one-Wire+observation vs four authority-Wires; friction rating differences -->

### Underspecified guidance

<!-- Items where agents diverged because README/CORE does not prescribe a unique mapping -->

---

## Aggregate verdicts

**Phase 3 only.** System-class fit statements — not numeric scores.

| System class | Representative sketches | Fit summary | Caveats |
|---|---|---|---|
| Point-to-point bring-up | 01-A | | |
| USB/CAN gateway + bench authority | 01-B, 04 | | |
| Master-polled fieldbus | 03 | | |
| Peer-equal CAN (telemetry) | 02 | | |
| Peer-equal CAN (command) | 02 | | |
| Linux gateway + intermittent maintenance | 04-B | | |
| Coordinator absent / degraded | 04-C | | |
| Multicore + heterogeneous fieldbuses | 05 | | |
| Redundant gateway | 06 | | |

---

## Spec findings log

**For sketching agents:** record technical issues, spec ambiguities, and stress-test results here when they are relevant to a sketch but do not belong in the sketch's own open-questions section (or when they apply across sketches).

One row per finding. Link the sketch(s) that surfaced it. Do not resolve here — route to `REG` / normative docs when confirmed.

| ID | Date | Sketch(s) | Topic | Finding | Spec reference | Stress-test result | Suggested doc action |
|---|---|---|---|---|---|---|---|
| SF-001 | 2026-08-22 | 01-B, 02, 04 | **Observation on broadcast Links** | Configured consumption of another Node's `NodeToOrigin` traffic on a shared medium (`CORE §3.1`, `§12.6`) | `CORE §3.1`, `§12.6` | **Useful** for peer telemetry (`02`) and gateway multi-consumer paths (`04` tap). **Awkward** for peer-addressed commands without app-layer dest (`02`, Significant). Tooling for symmetric observer groups still open. | Document as pattern; observer-group Wiring in `DEPLOY` (DF-004); clarify vs duplicate TX |
| SF-002 | 2026-08-22 | 03-B | **Poll projection** | Per-Link poll schedule for Nodes on one semantic Wire when multiple Link schedulers exist | `CORE §1.7`, `§12` | **Positive** if compact: "Nodes 5–8 on Link B; poll Endpoint X at rate Y." Bridge polls Seg B while gateway remains semantic Origin. Schema unnamed. | Name and schema in `DEPLOY` / `LINK` (DF-003) |
| SF-003 | 2026-08-22 | 03-B, 05 | **Wire Origin vs Link initiator** | Semantic Origin must not be conflated with I2C/SPI/RS-485 bus master or poll controller | `CORE §1.7` | **Strong positive** (`03-B`, `05`): one Wire spans locally-polled Links and AMP partitions without per-segment Origins. Core1 poll scheduler ≠ PlantNet Origin. | Terminology guardrail in `CORE §1.7` (DF-002); avoid "Origin" for bus master in APIs |
| SF-004 | 2026-08-22 | 01-B, 04 | **Forward vs compose** | Gateway **forwarding** preserves Wire identity; cross-Wire traffic is **application composition** (consume → new PDU), not forward | `CORE §3.3`, `§12` | **Strong positive** (`04-A/B`): Bench maintenance pass-through vs teleop→RobotPlant compose. Early `01-B`/`04` drafts conflated them. | Pattern section in `CORE §12` or `DEPLOY` (DF-001); canonical gateway example |
| SF-005 | 2026-08-22 | 02-B, `sketches_b` 02 | **CAN alias ceiling** | Eight peer-authority Wires max on developed 11-bit profile (alias 0 + 7 named) | `LINK §2`, `CORE §5.1` | Observed in `sketches_b` four-Wire alternative; primary `02` stays one Wire | Profile doc + tooling validation (TF-005) |
| SF-006 | 2026-08-22 | 02 | **Per-source Snapshot storage** | Multi-producer latest-value state cannot use one coalescing Snapshot | `CORE §9` | Open — each peer publishes own Snapshot on observe path | `REG §6.12` / Service wiring guidance (AF-001) |
| SF-007 | 2026-08-22 | 01-B, 02 | **CAN NodeId 31 / provisioning drift** | Main docs conflict: `DEPLOY §1.6` recommends NodeId 31 for development equipment and `LINK §2.2` defines 1..31 as individual Nodes with 0 as broadcast, while `docs/notes/can_id_provisioning.md` reserves 31 as Broadcast. The note also uses a different CAN identifier field order from `LINK §2.2`. | `DEPLOY §1.6`; `LINK §2.2`; `docs/notes/can_id_provisioning.md` | Direct contradiction; `sketches_b/01` avoided 31 and assigned the PC NodeId 5 | Reconcile the provisioning design through `REG`, then update `CORE` / `LINK` / `DEPLOY` together before assigning conventional NodeIds |
| SF-008 | 2026-08-22 | `sketches_b` 02 | **Alias 0 with overlapping full-bus Wires** | When several authority Wires have identical full membership on one CAN bus, `CORE` says the physical bus is itself a Wire and alias 0 represents its native Wire, but does not say whether one authority Wire must be selected as native, alias 0 may remain unused, or tooling should represent a separate physical-Wire identity. | `CORE §3.5`, `§5.1`–`§5.3`; `LINK §2.2` | The four-authority-Wire mapping selected LeftDriveAuthorityWire for alias 0 arbitrarily and had to state that the choice carries no coordinator semantics | Clarify native-Wire selection and alias-0 use for several overlapping full-membership Wires; define Organizer/UI behavior |
| SF-009 | 2026-08-22 | 01-B | **Stable device identity definition** | `DEPLOY §1.7` presents a 128-bit UUID as stable device identity, while `docs/notes/can_id_provisioning.md` makes a 64-bit DeviceId canonical and says it avoids a separate 128-bit UUID; their relationship and migration path are unspecified. | `DEPLOY §1.7`; `docs/notes/can_id_provisioning.md` | Surfaced while separating stable device identity from Wire-local NodeId in commissioning examples | Decide whether DeviceId replaces UUID, derives from it, or is a separate commissioning identity; update terminology and tooling requirements |
| SF-010 | 2026-08-22 | 02 | **No `NodeToOrigin` broadcast** | Fan-out to all Nodes is `OriginToNode` with NodeId 0 from Origin, or configured **observation** of others' `NodeToOrigin` — not NodeId 0 on `NodeToOrigin` (invalid per `CORE §3.1`) | `CORE §3.1` | Common author misconception; `02` stress-test explicit | Anti-pattern callout in `CORE §3.1` or INTRO worked example |
| SF-011 | 2026-08-22 | 03 | **Communication age vs measurement age** | Poll/arrival timestamp bounds **delivery** latency; Service sample metadata bounds **measurement** staleness — must not conflate in capacity/freshness analysis | `CORE §9.3`, `§21.4`, `§1.7`; `DEPLOY §2.2` | **Positive** when distinguished (`03-A`); error when conflated | Clarify in `CORE §21.4` and static capacity checking (`DEPLOY §2.2`) |
| SF-012 | 2026-08-22 | 04-C | **Wiring persists when Origin offline** | Configured Wire and Origin role remain when coordinator device is absent; Nodes may emit valid `NodeToOrigin`; no automatic role reassignment | `CORE §3.2`, `§3.3` | **Positive** (`04-C`): honest degraded state, not Wire collapse | Degraded-operation note in `CORE §3` or `DEPLOY`; tooling flag (TF-004) |
| SF-013 | 2026-08-22 | 01-B, 04 | **Authority Wire lists downstream Nodes** | PC-origin Wire may include ECU Nodes reached only via gateway **same-Wire** forward — not one Wire per Physical Link | `CORE §3`, `§12` | **Strong positive** (`04` Bench spans Eth/WiFi+CAN); reverses physical-link decomposition error | Worked example in `INTRO`/`DEPLOY` (DF-005); sketch README guidance |
| SF-014 | 2026-08-22 | 02 | **Peer-addressed control class boundary** | Arbitrary peer A→B command meshes are a weak fit: no Node-to-Node; options are observe+app-address, per-commander Wires, or non-goal (`CORE §3.1`) | `CORE §3.1` | Telemetry OK; commands **Significant** awkwardness on one-Wire observe path | Document archetype limit; do not add peer primitive from one sketch (DL-004) |
| SF-015 | 2026-08-22 | 01-B, 04 | **Cross-Wire relay mislabeled as forward** | Bench→RobotPlant (or segment A→B) maintenance "forward" violates forwarding invariants — easy gateway authoring error | `CORE §3.3`, `§12` | Caught in sketch review; fixed in `01-B`, `04` | DF-001; gateway author checklist |
| SF-016 | 2026-08-22 | 04 | **Local observation tap** | Gateway full-rate log via configured tap on Wire ingress — not a second Wire, not a second reader on consumer Queue | `CORE §12.6`, `§9` | **Positive** (`04-A/B`); distinct from promiscuous mode (`DEPLOY §3.1`) | Gateway recipe in `DEPLOY` or `LIB` |
| SF-017 | 2026-08-22 | 03 | **Master-initiated profile ≠ PHY law** | Autonomous Node TX rejection is **Link profile** choice (`CORE §1.7`), not RS-485 impossibility — multi-master/token schemes exist on the wire | `CORE §1.7`, `LINK §3` | Wording correction during `03` review | One sentence in `LINK §3` scope |
| SF-018 | 2026-08-22 | 05 | **Per-domain Link Telemetry addressing** | Multicore device with N Endpoint Domains exposes N Domain-local Link Telemetry Services (`DEPLOY §3.3`); network face on a maintenance Wire needs distinct wire-addressable identity per Domain | `DEPLOY §3.3`, `CORE §13.2` | **Happy path:** Service Wire Node 9 = Core0 Domain, Node 10 = Core1 Domain. Spec does not yet say whether Wire NodeId is canonical or a service-instance sub-address is required | Document multicore gateway NodeId convention; validate in Organizer |
| SF-019 | 2026-08-22 | 05–06 | **Multi-Domain same-type Service on one device** | When several Endpoint Domains on one PCB (or paired gateways) each expose the same standard Service, remote maintainer needs distinct wire-addressable identity | `DEPLOY §3.3`, `CORE §1.5` | **Candidate resolution (provisional):** independent Wire NodeId per Endpoint Domain (9–10 per gateway in `05`; 9–12 across pair in `06`). Not settled normative | Define rule in `DEPLOY` / `REG`; validate in Organizer |
| SF-020 | 2026-08-22 | 06 | **Partner same-Wire forward** | Partner reaches remote fieldbuses via **same-Wire forward** on PartnerLink — partner is not a second Origin | `CORE §3`, `§12`, `§23.11` | **Strong positive** — `PlantNet \| OriginToNode(3)` preserved end-to-end through B | `DEPLOY` paired-gateway PlantNet template |
| SF-021 | 2026-08-22 | 06 | **Partner connectivity ≠ redundancy** | Partner link provides reachability to healthy partner's buses; exclusive 2+2 split provides fault isolation, not redundant fieldbus attachment or Origin failover | `CORE §23.11` | **Strong negative boundary** — A loss removes drives/sensors/Origin; B loss removes aux/cell; promoting B does not recover A's buses | Do not label partitioned pairs "redundant" without §23.11 composition |
| SF-022 | 2026-08-22 | 06 | **Exclusive bus ownership bounds failover** | Logical Wire reachability via partner forward ≠ physical bus redundancy when each bus has one exclusive gateway attachment | `CORE §23.11`; `README` sketch 06 | Whole loss of gateway removes its buses even if partner survives | Per-sink coverage matrix in tooling; Config D for true redundancy |
| SF-023 | 2026-08-22 | 06 | **Service Wire multipath on dual EthPlant** | Dual gateway EthPlant + PartnerLink creates a cycle; nominal Service must use **branch-owned ingress** (A Eth for A-local Nodes, B Eth for B-local) — PartnerLink not on baseline Service tables | `CORE §12.4`, `§23.11` | **Issue caught in review** — ambiguous "laptop → A or B" causes duplicate delivery if both paths installed | Branch map in `DEPLOY` Wiring; alternate Service via PartnerLink only under §23.11 failover |

### Finding template (copy for new rows)

```text
| SF-0NN | YYYY-MM-DD | sketch | Topic | Finding | Spec reference | Stress-test result | Suggested doc action |
```

---

## Documentation and product follow-ups

Tracked work items suggested by the sketch program. **Not synthesis conclusions** — promote to `REG`, `CORE`, `DEPLOY`, `LINK`, or tooling backlogs when accepted.

### Priority — promote to normative docs

| ID | Item | Source sketch(s) | Target doc / artifact | Status |
|---|---|---|---|---|
| DF-001 | **Forward vs compose** gateway vocabulary | 04, 01-B | `CORE §12` appendix or `DEPLOY` patterns | Open |
| DF-002 | **Wire Origin vs Link poll initiator** terminology | 03-B | `CORE §1.7`, `LINK` profiles | Open |
| DF-003 | **Poll projection** configuration shape | 03-B | `DEPLOY §2`, `LINK` | Open |
| DF-004 | **Observation / observer groups** on broadcast Links | 01-B, 02, `sketches_b` | `DEPLOY` Wiring; stress-test per SF-001 | Open |
| DF-005 | **Authority-shaped overlapping Wires** as intentional pattern | 01-B, 04 | `DEPLOY` or `INTRO` worked examples | Open |
| DF-006 | **Degraded operation / Origin offline** | 04-C | `CORE §3`, `DEPLOY` | Open |
| DF-007 | **Communication vs measurement age** (freshness) | 03 | `CORE §21.4`, `DEPLOY §2.2` | Open |
| DF-008 | **Local observation tap** on gateway ingress | 04, 06 | `DEPLOY` or `LIB` | Open |
| DF-009 | **Paired-gateway / partner forward** template | 06 | `DEPLOY` patterns | Open |
| DF-010 | **§23.11 redundancy composition** authoring | 06 | `CORE §23.11`, `DEPLOY` | Open |

### Priority — tooling and Organizer

| ID | Item | Source sketch(s) | Target | Status |
|---|---|---|---|---|
| TF-001 | "Clone Wire with different Origin" after discovery | 01-B | Organizer | Open |
| TF-002 | Symmetric observation group generation | 02 | Organizer / Manifest | Open |
| TF-003 | Present overlapping Wires as authority domains, not cable segments | 01-B, 04 | Host UI | Open |
| TF-004 | Flag "Origin device offline" without role reassignment | 04-C | Host tooling | Open |
| TF-005 | Validate CAN alias count vs peer-authority Wire count | `sketches_b` 02 | Organizer / `DEPLOY §2.3` | Open |
| TF-006 | Generate and display per-sink redundancy coverage by failure | `sketches_b` 06, SF-022 | Organizer / system-builder UI | Open |

### Priority — Service / Endpoint API

| ID | Item | Source sketch(s) | Target | Status |
|---|---|---|---|---|
| AF-001 | Per-source latest-value Snapshot / multi-Wire receive bindings | 02, `sketches_b` 02 | `LIB`, `REG §6.12` | Open |
| AF-002 | Multi-producer emergency-stop Service across authority Wires | `sketches_b` 02 | Service catalog / codegen | Open |
| AF-003 | Multi-domain instances of one standard Service on one Wire | `sketches_b` 05, SF-019 | `LIB`, Service registry / codegen | Open |
| AF-004 | Replicated-controller source selector and correlation wrapper | `sketches_b` 06, SF-021 | Service catalog / codegen / Transport guidance | Open |

### Deferred — Phase 3 or later

| ID | Item | Notes |
|---|---|---|
| DL-001 | Aggregate fit-by-system-class verdict | See [Aggregate verdicts](#aggregate-verdicts) |
| DL-002 | Full status-quo comparison essays | vs Modbus, raw CAN, ROS bridges, etc. |
| DL-003 | Direction-optional on point-to-point Links | Model pressure from 01-A; needs more evidence |
| DL-004 | Peer-addressed Wire profile | Only if multiple sketches demand it; `sketches_b` argues against for now |

---

## Open synthesis questions

Questions that require multiple sketches or agent comparison before answering. **Do not answer here prematurely.**

- Does observation remain the preferred peer-telemetry pattern when four-authority-Wire mapping is available (`sketches_b` 02)?
- Is **Mild** wire proliferation (four symmetric authority Wires) acceptable product cost for peer CAN, or is raw CAN the honest recommendation below N peers?
- Does poll projection generalize from RS-485 bridge (`03-B`) to multicore gateway (`05`) without new concepts?
- When should friction distinguish **model honesty** (good behavior under failure) from **low mapping cost**?

---

## Revision history

| Date | Change |
|---|---|
| 2026-08-22 | Extended SF-019–SF-021; added SF-022, TF-006, and AF-004 from independent redundant-gateway sketch |
| 2026-08-22 | Sketch 06 review: reframed as partitioned; SF-023 Service multipath; updated SF-019–022 |
| 2026-08-22 | Sketch 06: SF-020–SF-021; merged duplicate SF-019; DF-009–DF-010 |
| 2026-08-22 | Added SF-019 and AF-003 from independent multicore-gateway sketch |
| 2026-08-22 | Revised SF-018; added SF-019 (multi-Domain Service addressing) from sketch 05 review |
| 2026-08-22 | Added SF-018 (per-domain Link Telemetry) from sketch 05 |
| 2026-08-22 | Updated SF-001–SF-006 stress-test results from sketches 01–04; added SF-010–SF-017 |
| 2026-08-22 | Added SF-007–SF-009 from independent dev-board and peer-CAN review |
| | Created structure; seeded spec findings and doc follow-up tables from sketch review |

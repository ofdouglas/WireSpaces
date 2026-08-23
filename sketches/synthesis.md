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
| `07_amr` | `07_amr.md` | `sketches_b/07_amr.md` | Yes | 7 application Wires in both; variant adds one supporting Platform Wire | Platform reach; observer membership; Bumper route scope | Strong convergence on Participants, seven production scopes, gateway boundaries, and earned multi-Origin |
| `08_excavator` | `08_excavator.md` | `sketches_b/08_excavator.md` | Yes | 6 in both | Split versus spanning Hydraulic control scope | Strong convergence on all-multi-Origin topology, operation-level authorization, Platform scope, and degraded behavior |
| `09_bess` | `09_bess.md` | — | — | 12 | No independent variant yet | — |

### `01_dev_board` — comparison notes

<!-- e.g. child count (2 vs 3), Wire naming, observation vs forward-only PC path -->

### `02_peer_can` — comparison notes

<!-- e.g. one-Wire+observation vs four authority-Wires; friction rating differences -->

### `07_amr` — comparison notes

- Both agents derived the same seven production relationships: controller coordination, Safety, Power, Bumper/discrete sensing, Navigation sensing, Payload actuation, and Charge.
- `sketches_b/07` is the stronger consolidation base because its supporting Platform Wire preserves one canonical identity for firmware update, telemetry, logs, and time across Motion, Payload, and BMS forwarding. The primary's update paths otherwise cross application Wires without one preserved Wire identity.
- Normalize Bumper as Chassis-local unless the frozen machine explicitly gives Autonomy an observer path. Optional observation must not silently widen physical realization.
- Request-scoped replies from Motion/Payload on Safety remain `ParticipantToOrigin`; they do not add permitted Origins. Safety has one Origin, P3.
- Forwarding tax is **None** where Motion/BMS merely implement frozen native cross-link reach; generated route tables belong under configuration burden and carrier cost.

### `08_excavator` — comparison notes

- Both agents independently reached six Wires and found that all six legitimately need multiple Origins, including Platform and Safety I/O. This is evidence that multi-Origin is useful beyond peer networks.
- The primary split — high-level hydraulic requests on Chassis Control, local actuator/protection on Hydraulic Local Control — makes the frozen composition boundary clearest.
- The variant's spanning Hydraulic Wire is defensible only where unchanged leaf traffic or a genuinely common protective broadcast crosses the Hydraulic Gateway. Its duplicate `HydraulicFunction.highLevelRequest` authorization on both W2 and W4 must not survive consolidation.
- Normalize Configuration burden to **Mild** when compact authored service-family rules generate the large flattened safety-relevant matrix. Normalize global Identity awkwardness to **None**.

### `09_bess` — review notes

- Multi-Origin earns its cost on PlantControl and PlantPartner while preserving fixed membership and Participant identity through active-term failover.
- Static permitted-Origin capability and runtime accepted-Origin state are distinct. Organizer can audit the former; receiving Services enforce the latter.
- `PlatformTelemetry` and `FirmwareUpdate` have identical membership and realization. Their separation is mild Artificial-Wire pressure; a single Platform Wire with Endpoint-level update admission appears sufficient and should be tested before consolidation.
- Origin-count accounting must remain numeric: PlatformTelemetry needs 25 Origins even when authored as one all-members declaration.
- Normalize the standard friction scale to None/Mild/Significant. Runtime Plant term transfer is native Service policy, not Wire-role instability.

### Underspecified guidance

<!-- Items where agents diverged because README/CORE does not prescribe a unique mapping -->

### Remap experiment notes

- **05 multicore:** normalize the result as **Candidate 1 modestly simpler; Candidate 2 neutral**. Both remaps retain 2 Wires, require 0 multi-Origin Wires, add no authored authorization state, and leave forwarding, polling, observation, composition, and failure behavior unchanged.
- **05 open questions (do not score until brief clarifies):** (a) `SF-028` — one Domain = one `ParticipantId` may collapse multiple network identities (Cell Nodes 20/21) and push distinction into Endpoint policy; (b) whether forwarding Domains (P11) must be Wire members; (c) whether configured observers (P31) are members or explicit non-members.
- **02 peer CAN:** normalize the result as a clear **Candidate 2 simplification** on the primary interaction path. The Wire count remains one; Interaction awkwardness improves Significant→None and Failure mismatch Mild→None. Wire proliferation stays None→None (four-Wire alt was rejected escape hatch). Role instability stays None→None. Candidate 1 contributes only minor naming/tooling value here. Direct peer commands are primarily multi-Origin, not global-identity. Authored policy count: 1 compact symmetric-peer rule.
- **02 broadcast caveat:** all-member Wire broadcast must not imply every Endpoint broadcast is semantically required by every member — Endpoint/binding interest still needed (`SF-027`).
- **Candidate separability:** 05 shows global Endpoint-Domain identity can help when multi-Origin is useless; 02 shows multi-Origin can materially help where global identity is not central. Meta: 02 = 1/1 Wire needs multi-Origin; 05 = 0/2.
- **Provenance caveat:** the 05 remaps are useful convergence but not pristine blinded replications because their authors had prior exposure to the authority-shaped-Wire, multicore-identity, and forward-vs-compose conclusions. Treat 05 as good non-regression evidence, not an independent adoption vote. A third 05 remap is lower priority than fresh-agent checks of 01, 04, and 06.

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
| Distributed mobile robot authority | 07 | | |
| Concurrent normal/safety/protective authority | 08 | | |
| Redundant plant + safety/local autonomy | 09 | | |

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
| SF-024 | 2026-08-22 | [remap] 05 | **Multi-Origin does not collapse distinct authority planes** | Plant vs maintenance Wires remain separate when route scope, failure meaning, and Service bindings differ — collapsing would move boundary to authored Endpoint restrictions | `remap_brief` §4.4, §7.1 | **05 remap:** 2 Wires → 2 Wires; 0 multi-Origin Wires; plant (P10) vs maintenance (P30) | Do not count Wire removal without wire-accounting rows |
| SF-025 | 2026-08-22 | [remap] 05 | **Global identity resolves cross-Domain Service addressing** | Deployment-global `ParticipantId` gives each Endpoint Domain a stable network identity; multi-Origin did not independently contribute | `remap_brief` §8.5; SF-018, SF-019 | Distinct Core0/Core1 Participants resolve the cross-Domain Link Telemetry case. This does **not** resolve multiple independently addressed same-type Service instances inside one Domain | Adopt ParticipantId-per-Domain as provisional SF-019 resolution only for cross-Domain instances; keep same-Domain instance semantics open |
| SF-026 | 2026-08-22 | [remap] 02 | **Multi-Origin enables symmetric peer CAN** | Four peer ECUs on one CAN bus: all permitted Origins replaces nominated FL + observation + app-addressed commands | `remap_brief` §8.2; SF-014 | **02 remap:** 1 Wire before and after, 4 permitted Origins; Interaction awkwardness Significant→None and Failure mismatch Mild→None. Wire proliferation is **None→None** because the four-Wire design was only a rejected alternative. Direct peer addressing is a multi-Origin benefit; global identity contributes only minor naming/tooling value in this one-Wire case | Peer-cell Organizer profile; do not use single-Origin + observation as peer happy path if multi-Origin adopted |
| SF-027 | 2026-08-22 | [remap] 02 | **Observation bridge removable under multi-Origin** | Peer telemetry via observe-`NodeToOrigin` was single-Origin workaround; each peer `OriginToParticipant` broadcast replaces it on primary path | `remap_brief` §5.4; SF-001 | Observation still valid where broadcast is not the intended relation; not required for sketch-02 primary remap. Wire-level broadcast reach must not imply that every Endpoint is semantically required by every member; Endpoint/binding interest remains necessary | Document when observation remains necessary vs optional, and how Endpoint interest refines Wire broadcast reach |
| SF-028 | 2026-08-22 | [remap] 05 | **Multiple participant identities inside one Endpoint Domain** | Candidate 1 fixes one `ParticipantId` per Endpoint Domain, while source 05 used two Wire-local identities (Cell Nodes 20/21) in one Cell gateway Domain. Collapsing them to one Participant is safe only if the distinction is purely Endpoint-local | `remap_brief` §2.1, §8.5; SF-019 | Potential Candidate-1 expressive cost: separate broadcast membership, authority, routing, or failure identity may be lost or pushed into Endpoint policy unless the implementation splits one dispatch/ownership Domain into two | Decide whether one Domain may expose multiple participant identities; otherwise document the required Endpoint-level replacement and when a Domain must split |
| SF-029 | 2026-08-22 | 07 | **Canonical Platform reach across gateway chains** | Firmware update, telemetry, logs, and time that cross Motion, Payload, and BMS need one preserved Wire identity if the gateways are forwarding rather than composing | Accepted forwarding premise; `CORE §12` | AMR comparison: seven application Wires converge, but only the variant's supporting Platform Wire gives P1→P7/P8/P9 a complete same-Wire forwarding path | Document a supporting Platform-Wire pattern for bounded on-machine management; reject cross-application-Wire pseudo-forwarding |
| SF-030 | 2026-08-22 | 07–09 | **Spanning Wire partitions require per-path reachability** | A configured Wire can remain valid while gateway or segment loss partitions its members into connected components; one Wire-up/down flag is insufficient | `CORE §12`; multi-Origin degraded-state premise | AMR Motion loss partitions Safety/Power/Nav/Platform; excavator VCU loss partitions Powertrain/Platform; BESS rack isolation removes one branch of three spanning Wires | Organizer should compute and display per-Wire connected components and interaction-specific required-sink reach |
| SF-031 | 2026-08-22 | 08–09 | **Operation-level Origin authorization is load-bearing** | Per-Wire permitted-Origin sets are too coarse when VCU/Plant, Safety, BMS/capability, gateway protection, and leaves share a Wire but may originate disjoint operations | Accepted multi-Origin premise; Endpoint/Service authorization | Compact service-family/class declarations remain auditable; flattened per-Endpoint matrices are large but generated. Wire allow-sets restrict only a subset of cases | Name and specify Endpoint-level Origin admission classes; require generated-rule provenance |
| SF-032 | 2026-08-22 | 08 | **Composition boundary does not automatically require a Wire boundary** | A gateway may terminate one interaction and originate another on the same Wire, but only when route, broadcast, and failure scope remain genuinely common | Forward-vs-compose premise; `CORE §12` | Excavator agents diverged on split Hydraulic Local versus one spanning Hydraulic Wire. Split is clearer unless unchanged leaf traffic/common protection really crosses the Gateway | Add guidance for deciding same-Wire composition versus separate semantic scopes; prohibit duplicate authorization on both decompositions |
| SF-033 | 2026-08-22 | 09 | **Static permitted capability vs runtime accepted Origin** | Redundant Plant A/B are both statically permitted, while active term/epoch determines which is presently accepted. Organizer can audit capability but cannot evaluate the runtime predicate | Accepted multi-Origin premise; redundancy composition | BESS failover changes no Wire, membership, or Participant identity; stale old-term commands are rejected at receiving Services | Name runtime Origin-admission predicates and require tooling to disclose when authority is not statically decidable |
| SF-034 | 2026-08-22 | 07–09 | **No native live-Origin-population state** | “No currently live permitted Origin” is reconstructed from health plus authored Origin sets, not represented directly by Wire state | Multi-Origin degraded-state premise | Needed for Safety-unavailable, no active Plant, update-orchestrator unavailable, and multi-Origin gateway partitions | Add Organizer/runtime view of live permitted Origins per Wire and per authorized capability without implying automatic reassignment |
| SF-035 | 2026-08-22 | 09 | **Identical Platform Wire split is likely moved authority** | PlatformTelemetry and FirmwareUpdate have identical members, realization, and broadcast reach; separate failure wording alone may not justify two Wires when update authority is already Endpoint- and term-restricted | Minimum-machinery rule; Endpoint authorization premise | BESS W5/W6 split is the only acknowledged Artificial-Wire pressure; merging preserves route/failure topology and moves no new policy beyond the existing update class | Test one Platform Wire before adopting the split; report update availability as capability state rather than Wire state |
| SF-036 | 2026-08-22 | 09 | **Global identity sizing follows Endpoint Domains, not devices** | Large hierarchical systems consume one global identity per Endpoint Domain even when constrained Links use reusable carrier-local compact addresses | Accepted global-identity premise | Representative BESS uses 25 identities in one container while each local bus needs only 3–5 compact addresses | Carry Endpoint-Domain counts into sizing analysis; keep global semantic identity separate from carrier-local ceilings |

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
- Does one-`ParticipantId`-per-Endpoint-Domain preserve systems that need multiple participant-level broadcast, authority, routing, or failure identities inside one dispatch Domain (`SF-028`)?
- How does Endpoint/binding interest refine all-member Wire broadcast reach without recreating configured observation (`SF-027`)?
- Should a common on-machine Platform Wire be the default for canonical update/telemetry forwarding across gateway trees (`SF-029`), and when should platform capabilities split despite identical topology (`SF-035`)?
- What runtime data model exposes connected components, live permitted Origins, and capability-specific authority without pretending to perform failover (`SF-030`, `SF-033`, `SF-034`)?

---

## Revision history

| Date | Change |
|---|---|
| 2026-08-22 | Reviewed sketches 07–09; added AMR/excavator comparison notes and SF-029–SF-036 for platform reach, partitioned Wires, operation-level authority, runtime Origin admission, live-Origin state, and identity sizing |
| 2026-08-22 | Normalized remap 02/05 review: peer Wire proliferation None→None; direct addressing attributed to multi-Origin; broadcast-interest caveat; added SF-028 for multiple identities inside one Endpoint Domain |
| 2026-08-22 | [remap] 02: SF-026–SF-027; multi-Origin symmetric peer CAN on one Wire |
| 2026-08-22 | [remap] 05: SF-024–SF-025; global identity resolves SF-018/019; 2→2 Wires |
| 2026-08-22 | Extended SF-019–SF-021; added SF-022, TF-006, and AF-004 from independent redundant-gateway sketch |
| 2026-08-22 | Sketch 06 review: reframed as partitioned; SF-023 Service multipath; updated SF-019–022 |
| 2026-08-22 | Sketch 06: SF-020–SF-021; merged duplicate SF-019; DF-009–DF-010 |
| 2026-08-22 | Added SF-019 and AF-003 from independent multicore-gateway sketch |
| 2026-08-22 | Revised SF-018; added SF-019 (multi-Domain Service addressing) from sketch 05 review |
| 2026-08-22 | Added SF-018 (per-domain Link Telemetry) from sketch 05 |
| 2026-08-22 | Updated SF-001–SF-006 stress-test results from sketches 01–04; added SF-010–SF-017 |
| 2026-08-22 | Added SF-007–SF-009 from independent dev-board and peer-CAN review |
| | Created structure; seeded spec findings and doc follow-up tables from sketch review |

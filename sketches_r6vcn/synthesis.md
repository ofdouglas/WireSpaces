# Sketch Synthesis — R6 + Unified VCN Round

**Status:** Structure only — populate after sketches exist  
**Purpose:** Aggregate diagnostic findings; compare independent mappings; track spec gaps.

**Do not** fill sections with architecture fan fiction. Record only what sketches and reviews actually showed.

---

## How to use

| Section | Who | When |
|---|---|---|
| [Sketch rollup](#sketch-rollup) | Lead reviewer | After each sketch is review-ready |
| [Convergence](#convergence) | Lead reviewer | After 2+ independent mappings of same archetype |
| [Friction heatmap](#friction-heatmap) | Lead reviewer | After 3+ sketches |
| [Question A vs B summary](#question-a-vs-b-summary) | Lead reviewer | After majority of corpus complete |
| [Spec findings log](#spec-findings-log) | Any agent | When a sketch exposes normative gap |

---

## Sketch rollup

### `01_simple_can_machine_cursor.md` (archetype 01)

```text
**Minimum mapping:** 5 Participants, 1 Wire (PlantCAN), no forwarding, 1 committed CAN11 alias (default map).
**Question A (R6):** Natural — strong pass; controller/leaf graph maps directly.
**Question B (CAN11):** Default — pass for single-Main hierarchical case; MainB unused.
**Worst friction:** Configuration burden — Mild (explicit IDs vs flat CAN table).
**Main lesson:** R6 + default VCN does not make a simple conventional machine worse.
**Evidence limit:** Does not stress two-Main default allocation or Main↔MainB VCNs.
```

**Trial-process notes (review):**

- VCN count should be **4 / 32** for minimum mapping (four Main↔Node relations); VCN 0 broadcast is optional, not counted.
- Optional Guest + committed on same bus is **not valid** under overlay §13 without spec finding.
- Distinguish configured membership from runtime presence for optional laptop.

---

### Archetype 07 revision (2026-08-23)

`07_peer_can_dense.md` updated from 4 single-domain peers to **8 domains** (dual-core per corner). `07_peer_can_dense_cursor.md` used the prior 4-participant archetype — retain as historical evidence; re-map for corpus.

---

### `09_distributed_chassis_can_cell_cursor_B.md` (archetype 09)

```text
**Minimum mapping:** Config A — 9 Participants, 1 Wire (CellCAN), Explicit CAN11 (29 / 31 usable VCN slots); Config B — +DevPC, 33 relations, Alias 2 representation partition.
**Question A (R6):** Natural — strong pass; dual peer meshes + VehicleState, no coordinator.
**Question B (CAN11):** Custom — viable but near practical boundary; default map unusable.
**Worst friction:** VCN pressure — Significant (relation namespace binding, not table maintenance).
**Main lesson:** First sketch that genuinely pressures CAN11 capacity; R6 unchanged at 9P/1W/0 gateways.
**CAN11 boundary:** ~high-20s participant relations per Wire → actively consider CAN29.
**Caveat:** 4 of 29 VCNs are intra-corner on CAN by archetype choice (29→25 if local inter-core).
```

---

## Convergence

<!-- Archetype | Agents | Participants agree? | Wire count agree? | CAN11 default/custom agree? | Notes -->

---

## Friction heatmap

Minimum-mapping happy path. **None / Mild / Significant**.

| Signal | 01 | 02 | 03 | 04 | 05 | 06 | 07 | 08 | 09 |
|---|---|---|---|---|---|---|---|---|---|
| Artificial Wire | None | | | | | | | | None |
| Wire proliferation | None | | | | | | | | None |
| Artificial hierarchy | None | | | | | | | | None |
| VCN pressure | None | | | | | | | | Significant |
| WireAlias pressure | None | | | | | | | | Mild (Config B only) |
| Configuration burden | Mild | | | | | | | | Moderate |
| Failure/topology mismatch | None | | | | | | | | Mild |

### Heatmap commentary

Archetype **09** is the first **Significant VCN pressure** entry: the constraint is CAN11's finite relation namespace (29/31), not configuration table size (Moderate). WireAlias pressure is **Mild** in Config B only — representation partition at 2/8 aliases, not alias exhaustion.

---

## Question A vs B summary

| Archetype | R6 disposition | CAN11 disposition | CAN29 recommended? |
|---|---|---|---|
| 01 | Natural (strong pass) | Default / single-Main only | no |
| 02 | | | |
| 03 | | | |
| 04 | | | |
| 05 | | | |
| 06 | | | |
| 07 | | | |
| 08 | | | |
| 09 | Natural (strong pass) | Custom — near boundary (29/31) | yes (growth/headroom) |

---

## Recurring patterns

- **CAN11 default VCN** suits single-Main hierarchical buses (01, 05); **Explicit custom** required for peer meshes (07, 09); default has no node↔node slots (SF-R6-006).
- **High-20s participant relations on one Wire** is the first empirical CAN11 practical boundary; CAN29 preferable when growth or routine service tooling expected (09, SF-R6-007).
- **Multi-alias same-Wire** useful for representation partition when one VCN map is full, not only migration (09 Config B, SF-R6-008).

---

## Anti-patterns

<!-- Populate when rejected decomposition appears 2+ times -->

---

## Spec findings log

| ID | Date | Sketch(es) | Topic | Spec ref | Finding | Disposition |
|---|---|---|---|---|---|---|
| SF-R6-001 | 2026-08-23 | 01 | **VCN accounting — minimum only** | overlay §11 | Optional VCN 0 counted toward "used"; inflates VCN pressure | Brief + overlay updated |
| SF-R6-002 | 2026-08-23 | 01 | **Mixed Guest + committed on one physical CAN** | overlay §13 | No ingress classifier for committed-format and Guest-block IDs on same bus | Overlay §13 added; brief §7 |
| SF-R6-003 | 2026-08-23 | 01 | **Wire membership vs physical presence** | CORE §3.2; DEPLOY | "Not a member when disconnected" conflates membership with reachability | Brief §6.4; template |
| SF-R6-004 | 2026-08-23 | 01 | **Two-Main default not stressed** | overlay §5 | Archetype 01 uses MainA only; does not validate MainB allocation | Record evidence scope; stress in 03, 05, 06 |
| SF-R6-005 | 2026-08-23 | 02 | **Physical attachment vs Wire membership vs authority** | CORE §3.5; archetypes | "PC not on CAN" under-specifies mapping; forwarding onto heterogeneous-Link plant Wire is valid but authority rule excludes PC from plant scope | Archetype 02 updated |
| SF-R6-006 | 2026-08-23 | 07 | **Default VCN has no node↔node slots** | overlay §5 | Default map is Main/Node star only; dense peer mesh needs Explicit custom VCN or CAN29; fake MainA/MainB incomplete for ≥3 leaves | Open |
| SF-R6-007 | 2026-08-23 | 09 | **CAN11 relation-capacity boundary** | overlay §11; §10 | Config A 29/31 usable ordinary VCN slots; Config B 33 relations exceeds one alias by 2; high-20s relations per Wire → consider CAN29 | Open |
| SF-R6-008 | 2026-08-23 | 09 | **Multi-alias representation partition** | overlay §8 | Second WireAlias on same Wire for relation-capacity split, not only VCN-map migration; 2/8 aliases is Mild pressure not Significant | Open |
| SF-R6-009 | 2026-08-23 | 10 | **Guest VCN slots are deployment-global, not per-bus** | overlay §6 | One 2-node Guest bus with control + diagnostics consumed 4 of 7 ordinary Guest-3 slots for the whole deployment; a second Guest bus would have 3 left. Guest sizing guidance should be stated per deployment; widening is a deployment-wide migration | Open |
| SF-R6-010 | 2026-08-23 | 10 | **Guest has no per-frame QoS** | overlay §6; LINK §2.2 | Committed CAN11 QoS makes update bursts arbitrate below control within one alias; Guest binding supplies one fixed QoS, so the same protection is unavailable on a legacy bus where it is arguably needed more | Open |
| SF-R6-011 | 2026-08-23 | 11 | **Redundant paths cannot share a Wire; no named redundancy pattern** | CORE §3.3 | One Wire over dual-homed A/B buses is a cycle, so redundancy is necessarily N parallel Wires + Service-level selection. Likely the correct answer (keeps failure visible) but derivable only from the loop-free invariant; three structures in archetype 11 needed the same derivation | Open |
| SF-R6-012 | 2026-08-23 | 11 | **Duplicate delivery across redundant Wires undefined** | CORE §9.6; §12 | A Participant on two Wires carrying one Service relation receives every PDU twice; no stated position on whether single-slot Endpoint overwrite absorbs it, whether dedup is an application duty, or how a Service declares two Wires redundant | Open |
| SF-R6-013 | 2026-08-23 | 11 | **Destination-pruned egress is load-bearing, not optional** | CORE §12 | Base flood-and-filter makes a Wire spanning 100 Mbit/s Ethernet and a 100 kbit/s intermittent radio unusable; pruning is a viability requirement there, and its "optimization" framing pushed the sketch to more Wires than the topology needs | Open |
| SF-R6-009 | 2026-08-23 | 11 | **Physical ring path choice** | CORE §3.3; §12.4 | Loop-free static Wires do not automatically exploit alternate physical reachability; pair Wires, application relay, or reviewed failover configuration expose that tradeoff explicitly | Open — compare independent 11 mappings |
| SF-R6-010 | 2026-08-23 | 11 | **Redundant physical delivery crosses Wire boundary** | CORE §12.2; §12.4 | Parallel A/B actuator paths become separate canonical Wire interactions requiring Service/transport correlation; investigate, but do not yet invent, an optional redundant-Link profile below Wire | Open — compare independent 11 mappings |
| SF-R6-011 | 2026-08-23 | 11 | **Broad service-Wire branch tax** | CORE §12.1 | Acyclic service trees still flood directed PDUs into irrelevant branches unless destination-pruned location state is configured; narrow gateway/proxy Wires are the simpler baseline | Open |
| SF-R6-014 | 2026-08-23 | 12 | **Committed WireAlias namespace is unstated** | overlay §2–§3, §11 | Twenty identical physical CAN11 buses need one alias each. Per-Link scope permits Alias 0 reuse; deployment-global scope exhausts 0..7 at rack 9. The overlay defines Guest VCN as deployment-global but does not explicitly scope committed WireAlias. | Open — clarify as per physical CAN interface/classifier or state the global limit |
| SF-R6-015 | 2026-08-23 | 14 | **Geographic audience vs static Wire membership** | CORE §3; archetype 14 | Proximity audiences change during normal motion; broad flood-and-filter wastes airtime while dynamic narrow Wires reproduce geographic-group state absent from R6 | Open |
| SF-R6-016 | 2026-08-23 | 14 | **Opaque mobile mesh boundary** | CORE §1–§3; §12 | One opaque routed Link is viable only when route selection, duplicate suppression, sleeping-node delivery, and partition/merge remain explicit underlay/application duties; WS supplies identity and Services, not mesh topology | Open |
| SF-R6-017 | 2026-08-23 | 14 | **Per-hop forwarding churn exceeds static model** | CORE §12; DEPLOY §1.4 | Exposing Config B mesh hops implies roughly 120–600 forwarding changes/s plus distributed merge semantics; static/read-mostly reviewed configuration is not a natural control plane | Open |
| SF-R6-018 | 2026-08-23 | 12 | **Immutable multi-vendor identity collision** | CORE §4.6; §6; FUTURE §12 | Immutable colliding vendor identity universes cannot be merged by forwarding/splices; reassignment is unavailable and explicit translation is deferred | Open |
| SF-R6-019 | 2026-08-23 | 12 | **Replacement lifecycle vs unreachability** | CORE §3.2; DEPLOY §1.6–§1.7 | Role PID reuse with a new hardware UUID is plausible, but replacement/retirement/tombstone/history transitions have no specified lifecycle or source of truth | Open |
| SF-R6-020 | 2026-08-23 | 12 | **Multi-container ordinary identity ceiling** | CORE §2.4; §4.6 | Config C lists 562 Participants versus 254 ordinary IDs; separate container WireSpaces plus composition loses transparent module Service identity, while extended addressing/translation is unspecified | Open |
| SF-R6-021 | 2026-08-23 | 13 | **Link capabilities cannot express a schedule** | CORE §13.4; §17; DEPLOY §2.3 | `CORE §13.4` names size / timing / transport as the dimensions a placement can fail in and requires configuration-time failure; `CORE §17` gives size several fields, transport one, and timing only "polling cadence, for master-initiated Links". A 100 Mbit/s link with a 500 µs cycle, a full slot schedule and a 256 B/cycle acyclic region is indistinguishable from one with 12.5 MB/s free. Four machine-stopping hazards pass every `DEPLOY §2.3` check. Recommend capability fields for cycle time, slot allocation, reserved non-scheduled rate, jitter bound — descriptor change, not data plane | Open |
| SF-R6-022 | 2026-08-23 | 13 | **Total canonical elision is legal and leaves WS contributing nothing on the wire** | CORE §2.2; DEPLOY §2.5 | On a summation-frame link the slot offset supplies Wire, Src, Dest and Endpoint and the schedule supplies QoS and TransportType, so a conforming profile transmits **0 of 6 header bytes**. Nothing bounds elision or states a floor, so "WS supports this link class" and "WS is absent from this link" are the same configuration. Residual value is configuration coherence only, and only if one Wiring source also generates the slot map | Open — state a floor, or state that full elision marks the architecture boundary |
| SF-R6-023 | 2026-08-23 | 13 | **No aggregating / summation Link profile class** | LINK §1, §9; CORE §12.5 | Six gaps: aggregation triggered by a cycle boundary (not among §12.5's four triggers); static slot offsets rather than stream position; in-flight modification by a node that is neither source, destination nor forwarder; one frame carrying 49 canonical sources in two directions; total elision (SF-R6-022); cycle-synchronous acyclic region with master-arbitrated admission. Not an efficiency matter — individually framed, the exchange needs 192,000 fps against a 148,809 fps physical ceiling | Open |
| SF-R6-024 | 2026-08-23 | 13 | **"Time" is three concepts under one name; only one is a Service** | CORE §9.4; §21.4 | (1) wall-clock/epoch — a Service, spans every Link, ~1 ms here; (2) Link phase reference — not a Service, a Link-profile property, **not transitive across a gateway**, ±1 µs only where provided; (3) arrival time — a local monotonic tick with no cross-Participant meaning. Listing "Time" among standard Services implies the coherent-Services claim covers phase; it does not. Consequence: a `VibChain` sensor and a `MotionBus` drive align events no better than ~1 ms = 5 mm of web at 300 m/min | Open — name the three; state phase is out of scope for the Time Service |
| SF-R6-025 | 2026-08-23 | 13, 15 | **Group-atomic application has no representation and cannot be detected as unavailable** | CORE §3.2; §9.4; §13.4 | 48 setpoints must apply on one tick. Broadcast says nothing about when recipients act; `CORE §9.4` decouples acceptance from consumption; arrival time is local; `Control` has no spare bits. Leaving the mechanism to the Link profile is defensible layering — but a Service needing group atomicity can be placed on a Link that cannot provide it and nothing notices, because the `CORE §13.4` envelope has no coordination dimension. Distinct from SF-R6-021 (rate/deadline) — this is simultaneity | Open — two unrelated motion archetypes reached it independently |
| SF-R6-026 | 2026-08-23 | 13 | **Cyclic liveness must not be manufacturable by the LLL** | CORE §9.3–9.5; §18.5; §21.4 | On a scheduled link a slot is present in every frame whether or not its producer updated it. An LLL that accepts slot contents into a Snapshot each cycle advances the generation counter and refreshes arrival time, so a stopped producer becomes indistinguishable from a running one by exactly the two mechanisms `CORE §21.4` prescribes for detecting it. `CORE §18.5` states the principle for telemetry; nothing applies it to acceptance | Open — require evidence the carrying transfer occurred this period |
| SF-R6-027 | 2026-08-23 | 13 | **A shared fabric is not a Link Interface — no capacity isolation from QoS or Wire scope** | CORE §14.2; §17 | Switch queueing happens in a device that is not a Participant and has no capability descriptor, so WS QoS is a mark honored only by 802.1p/DSCP configuration outside the Wiring, and two Wires on one fabric share all of its capacity. The ≤50 ms registration budget (41.7 ms nominal) misses at 50–74 ms under the archetype's simultaneous-load case, and separating the control-plane Wire from the bulk Wire bought scope and zero bandwidth. Wire decomposition is a scope tool, never a capacity tool | Open |
| SF-R6-028 | 2026-08-23 | 13 | **Continuous streams and large objects exceed Endpoint/Transport semantics** | CORE §9.5; §20; LINK §6 | Snapshot replacement loses blocks; bounded Queues expose overflow but do not supply stream sequence, gap recovery, or multi-megabyte object retry/resume/integrity. A 64-block vibration Queue absorbs only 35.6 ms, while dumps/images are 18 MB/3 MB. | Open — specify a bounded bulk Transport or document an explicit beside-WS data-plane boundary |

**SF-R6-013 confirmation (archetype 13, independent of 11).** Destination-pruned egress reproduced four times in one mapping and escalated from *unusable* to *unsafe*: one 1500 B PDU flooded onto a 500 µs cyclic link is 120 µs = 24% of the cycle, and a 4 MB firmware image is 25.2 s = 50,400 consecutive cycle overruns, against a requirement that drives trip on a bounded number of missed cycles. It also forced two CAN-FD Wires instead of one (38.4% → ~77% segment load), forbade any Wire spanning `VibChain` and `MachineEth`, and drove the `Plant`/`Bulk` split. Pruning is additionally **necessary but not sufficient** here — it solves flooding, not admission, and no WS concept expresses a per-cycle admission rate.

> **Numbering note.** Some IDs in 009–020 appear twice in the rows above, from mapping agents appending concurrently. New findings should use the next free value after 028; remaining duplicates need a reconciliation pass by the synthesis owner.

---

## Documentation follow-ups

| ID | Topic | Sketch evidence | Target doc | Status |
|---|---|---|---|---|

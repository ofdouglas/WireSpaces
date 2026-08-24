# Experiment Brief — R6 Participants + Unified CAN11 VCN

**Status:** Phase 1 sketch trial instruction; not a specification and not an adopted change  
**Audience:** Mapping agents and independent review agents  
**Purpose:** Diagnostic evaluation of whether Revision 6 addressing and the unified CAN11 VCN direction are natural for real embedded systems — not confirmation that they work.

---

## 0. Read this first

This brief is the **entry point** for the trial. Read it completely before any other material.

### Authoritative for this experiment

| Material | Role |
|---|---|
| **This brief** | Experiment rules, output requirements, scoring separation |
| `docs/introduction.md` | Maturity ladder, progressive complexity, non-goals |
| `docs/core_architecture.md` | Participants, Logical Buses, canonical src/dest, forwarding |
| `docs/deployment.md` | Organizer, commissioning shape, validation intent |
| `docs/link_profiles.md` | **Except** CAN11 committed/Guest layout in §2.4–2.6 — replaced by overlay |
| `experiment_overlay_vcn.md` | Unified CAN11 VCN model for this trial |
| `README.md` (this folder) | Sketch approach, friction signals, templates |

### Historical — do not use for mapping

| Material | Why excluded |
|---|---|
| `sketches/` (prior round) | Origin/Node semantics; pre-R6 mappings; remap outputs |
| `sketches_b/` | Independent variants of prior round |
| `sketches/remap_brief_multi_origin_revised.md` | Superseded experiment (multi-Origin) |
| `docs/proposed/WireSpaces_CAN11_Unified_VCN_Proposal.md` | Source proposal; may bias toward expected outcomes |
| `synthesis.md` in prior `sketches/` | Prior-round findings and verdicts |
| Extended addressing / 16-bit header ideas | Out of scope for this round (see §8) |

If governed docs conflict with this brief or the overlay on CAN11, **this brief and the overlay win for the trial**.

---

## 1. Two independent questions

Score these **separately** in every sketch. Do not let CAN11 friction count as evidence against the canonical R6 model, or vice versa.

### Question A — R6 canonical model

> Is the **Participant + Logical Bus** model natural for this system's communication structure?

Covers: deployment-global `ParticipantId`, loop-free Logical Buses, canonical `SrcParticipantId` / `DestParticipantId`, forwarding vs composition, progressive configuration.

### Question B — Unified CAN11 VCN

> When CAN11 is used, does **unified VCN + WireAlias** remain usable without ugly configuration?

Covers: default vs custom VCN map, WireAlias count, Guest-global limits, immutability/migration cost. A valid answer is: *R6 excellent; CAN11 awkward — use CAN29.*

---

## 2. What this experiment is not

- **Not confirmatory.** Record awkwardness; do not argue the architecture is good.
- **Not a feature-design session.** Do not invent protocol extensions to fix friction (§7).
- **Not an optimization contest.** Report minimum mapping first, optimizations second (§5).
- **Not a capacity study.** Use ordinary 8-bit WireNumber and ParticipantId values unless the archetype naturally exceeds them (§8).

---

## 3. Agent roles

### 3.1 Mapping agent

**Receives:**

```text
architecture packet (read path in §0)
one neutral archetype from archetypes/
template_sketch_output.md
```

**Produces:** one completed sketch under `sketches/` (see §11 — **unique filename required; never overwrite**).

**Must not receive:** other agents' sketches, prior-round mappings, synthesis verdicts, or the source VCN proposal.

**Assignment rule:** exactly **one archetype per mapping agent**. For convergence tests, 2–3 independent agents may receive the **same** archetype without seeing each other's output.

### 3.2 Review agent

**Receives:**

```text
architecture packet
same neutral archetype
one completed sketch from a mapping agent
template_review.md
```

**Produces:** one review under `reviews/` (or appended review section if orchestrator prefers one file).

**Must not:** redesign the mapping unless it is **invalid** under R6 + overlay. Evaluate understandability, naturalness, configuration honesty, and rubric scores.

Review agents do not read mapping agents' working notes or other sketches.

---

## 4. Trial corpus

Nine archetypes in `archetypes/`. Deliberately spans fit, structured CAN11 stress, and boundary cases.

| # | File | Intent | Expected CAN profile mix |
|---|---|---|---|
| 01 | `01_simple_can_machine.md` | Small single-controller CAN11 machine | Committed CAN11 |
| 02 | `02_pi_robot.md` | Pi + ECUs + dev PC + leaves | Committed CAN11 + non-CAN |
| 03 | `03_dual_controller_can.md` | Two-controller / partitioned CAN machine | Committed CAN11 |
| 04 | `04_multi_can_gateway.md` | Dual-core gateway, 3× CAN11 — USB/Eth configs | Committed CAN11 ×3 |
| 05 | `05_heterogeneous_gateway.md` | CAN + UART + shared-memory + Linux | Mixed; CAN29 candidate on cell link |
| 06 | `06_overlapping_wire_bandwidth.md` | Overlapping Logical Buses to isolate bandwidth | Committed CAN11 |
| 07 | `07_peer_can_dense.md` | Peer CAN negative control (8 domains) | Committed CAN11 |
| 08 | `08_ethernet_peer_mesh.md` | Large flat Ethernet peer system — **boundary test** | CAN29 or non-CAN |
| 09 | `09_distributed_chassis_can_cell.md` | Distributed chassis cell — **CAN11 ceiling** (~29 VCN prod) | Committed CAN11; CAN29 at bring-up |

Across the set, sketches should use **Guest CAN**, **committed CAN11**, **CAN29**, and **non-CAN** where the archetype naturally calls for them. Do not force CAN11 where another profile is the honest answer.

---

## 5. Order of operations (mapping agents)

Create mappings in this order. **Stop at the simplest semantically correct R6 mapping** before optimizing.

```text
1. Participants       — Endpoint Domains and deployment-global ParticipantIds
2. Wires              — Logical Buses (loop-free propagation scope)
3. Interactions       — who talks to whom, broadcast vs directed
4. Forwarding         — gateway hops preserving canonical PDU identity
5. Link profile       — CAN11 vs CAN29 vs UART/Ethernet/etc. per physical link
6. CAN11 binding      — Guest vs committed; default vs custom VCN; WireAlias bindings
7. Narrower/overlapping Wires — only if bandwidth, failure scope, or locality justify
```

Section **Minimum mapping** in the output template is steps 1–5 (plus 6 when CAN11 is chosen). Section **Optional optimizations** is steps 6–7.

If step 7 adds Wires or VCN complexity, say explicitly what problem it solves and what the minimum mapping lacked.

---

## 6. Required output sections

Use `template_sketch_output.md`. Every sketch must include:

### 6.1 Disposition block (both questions)

| Area | Assessment |
|---|---|
| Participant identity | Natural / mildly awkward / poor |
| Wire decomposition | Natural / mildly awkward / poor |
| Forwarding | Simple / moderate / awkward |
| Non-CAN configuration | Low / moderate / high |
| CAN11 VCN fit | Default / custom / poor fit / N/A |
| Better with CAN29? | Yes / no / N/A |

Plus 2–4 sentences explaining the row, **separating Question A from Question B**.

### 6.2 Minimum mapping vs optimizations

Two subsections. Minimum mapping is the primary evidence.

### 6.3 CAN11 accounting (when CAN11 is used)

Per physical CAN11 bus, fill the numerical tables in overlay §11.

**Count only VCNs required by the minimum mapping:**

- Include each **ordinary** VCN that carries a minimum-mapping interaction (each unordered pair used for directed traffic counts as one VCN slot, not two directions).
- **Do not** count optional capabilities (e.g. VCN 0 broadcast) unless the minimum mapping actually uses them.
- List unused default capabilities separately: *available by default but unused* (e.g. VCN 0, MainB odd slots, spare Node positions).

Example: four Main↔Node directed relations → `Ordinary VCNs used: 4 / 32`, with VCN 0 noted as available but unused if broadcast summary is optional.

**MainA / MainB evidence scope:** Archetypes that use only MainA do not stress the two-Main default allocation. Record in disposition or open questions when MainB and Main↔MainB VCNs are unused — that is expected for controller/leaf machines, not a VCN design verdict.

### 6.4 Membership vs presence

Distinguish **configured Wire membership** from **runtime reachability**:

- A maintenance Participant may be **preconfigured** on a Wire and simply unreachable when absent — Wiring persists; delivery fails or queues drain.
- Alternatively, a **maintenance configuration** installs bindings only during service — count which approach you chose.

Do not write "not a Wire member when disconnected" unless you mean bindings are genuinely absent from the committed configuration.

### 6.5 Core friction signals (7)

Rate **None / Mild / Significant** on the happy-path **minimum mapping** only:

```text
Artificial Wire
Wire proliferation
Artificial hierarchy
VCN pressure
WireAlias pressure
Configuration burden
Failure/topology mismatch
```

Record Participant or interaction awkwardness in free-form notes when observed; do not force a rating for every possible concern.

### 6.6 Spec findings (optional)

If a sketch exposes a normative gap, add a row to `synthesis.md` § Spec findings — link by `SF-R6-NNN` ID. Do not resolve architecture in the sketch.

---

## 7. Prohibited behaviors

> **Do not repair perceived shortcomings by inventing new protocol features.** Record the friction and continue with the least-bad **valid** mapping under R6 + overlay.

Examples of prohibited invention:

- new addressing modes, group multicast primitives, or dynamic routing;
- runtime VCN map mutation not in the overlay;
- authorization metadata not in governed docs;
- "the Organizer could infer X" without counting X as configuration;
- **mixed Guest + committed CAN11 on one physical bus** unless the archetype explicitly requires legacy coexistence **and** you file a spec finding — ingress classification for simultaneous committed-format IDs and Guest-block IDs on the same bus is **not defined** in the trial overlay (see overlay §13).

> **Configuration burden is evidence.** You may note that entries are mechanically generatable, but **count them**. Do not hide burden behind an assumed future generator.

---

## 8. Out of scope for this round

- Extended WireNumber / ParticipantId widths or 16-bit header layouts
- Participant-Compressed / Compact-General CAN11 (deferred; separate comparison arm if reopened)
- Byte-exact PDUA bit packing freeze
- Implementation, sim, or Organizer tool design
- Formal safety cases or WireContracts

Use ordinary **8-bit** WireNumber and ParticipantId values. If an archetype would naturally exceed ~250 Participants or need wider identity, say so and prefer CAN29 or note as open question — do not design extended addressing.

---

## 9. Convergence protocol

For archetypes marked **convergence** in the archetype file (02, 05, 07 recommended):

1. Assign **2–3 mapping agents** the same archetype independently.
2. After all mappings exist, a **lead reviewer** (human or agent) compares:
   - Participant count and roles
   - Wire count and boundaries
   - CAN11: default vs custom VCN, WireAlias count
3. Record in `synthesis.md` § Convergence — agreement is evidence of natural abstractions; divergence warrants investigation.

---

## 10. Synthesis

`synthesis.md` in this folder has an empty Round 1 table. Mapping agents may append spec findings only. Lead reviewer fills rollup after sketches exist — **do not pre-design the taxonomy**.

---

## 11. Output files — naming and collision rules

### 11.1 Before writing

1. **List** `sketches_r6vcn/sketches/` (or `sketches/` relative to this folder).
2. **Choose a filename that does not exist.** If your default name is taken, change `<agent_id>` — do not overwrite another agent's work.
3. **Never** edit or replace an existing sketch file unless the orchestrator explicitly assigns you that file for revision.

`cursor`, `agent`, `claude`, etc. are **not** unique if multiple runs use the same tool — use the orchestrator-assigned id (e.g. `agent_a`, `mapper_2`, `20260823_jd`).

### 11.2 Patterns

```text
sketches/<archetype_id>_<short_name>_<agent_id>.md          mapping output
reviews/<archetype_id>_<short_name>_<mapper>_<reviewer>.md  review output
```

Examples:

```text
sketches/07_peer_can_dense_agent_a.md
sketches/07_peer_can_dense_agent_b.md      ← convergence: same archetype, different agents
sketches/04_multi_can_gateway_mapper_2.md
```

**Forbidden without explicit reassignment:**

```text
Overwriting sketches/07_peer_can_dense_cursor.md
Reusing a filename because "I am also Cursor"
```

### 11.3 If the target path already exists

Pick the first free variant:

```text
<archetype_id>_<short_name>_<agent_id>.md
<archetype_id>_<short_name>_<agent_id>_2.md
<archetype_id>_<short_name>_<agent_id>_<YYYYMMDD>.md
```

Record the **actual path used** in the sketch header (`**Output file:** …`).

### 11.4 Convergence tests

2–3 mapping agents receive the **same archetype** and the **same** neutral problem statement. Each writes a **different file** with a **different** `<agent_id>`. Comparison happens in synthesis after all files exist — not by sharing one filename.

---

## 12. Quick reference — R6 premises (do not relitigate)

- One `ParticipantId` per Endpoint Domain, same on every Wire
- A Wire is a loop-free Logical Bus; multiple Participants may source traffic
- Canonical PDU: Wire + `SrcParticipantId` + `DestParticipantId` + Endpoint + …
- `Origin`, `Node`, `NodeId`, canonical `Direction` are **retired**
- Forwarding preserves canonical PDU identity; composition uses the composer's ParticipantId
- CAN11 committed: `QoS[2] + WireAlias[3] + VCN[5] + Direction[1]` per overlay
- Guest: deployment-global VCN meanings; no WireAlias in frame
- Active WireAlias bindings are immutable; spare alias for VCN-map migration

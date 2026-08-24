# WireSpaces R6+VCN Architecture Sketches

**Status:** Working notes; sketches are hypothetical systems, not deployment specs  
**Purpose:** Diagnostic trials of Revision 6 (Participants, Logical Buses) and unified CAN11 VCN — whether representations are **simpler and clearer** than conventional designs, not whether WS can represent the system at all.

**Start here:** [`experiment_brief_r6_vcn.md`](experiment_brief_r6_vcn.md) — read it **before** any governed doc.

---

## Read path (mapping agents)

```text
1. experiment_brief_r6_vcn.md
2. docs/introduction.md
3. docs/core_architecture.md
4. docs/deployment.md
5. docs/link_profiles.md          — except §2.4–2.6 (replaced by overlay)
6. experiment_overlay_vcn.md
7. this README
8. one file from archetypes/
9. template_sketch_output.md
```

Do **not** read `sketches/` (prior round), remap briefs, or the source VCN proposal.

---

## Core invariant

> **Model the system as it naturally exists first. Apply WireSpaces with the minimum machinery necessary. Any machinery needed primarily to make the model fit is evidence — record it, do not hide it.**

Nearly any flexible model can represent a system. The useful question is whether the representation is **understandable and faithful** with **minimum** configuration.

Do not repair awkward mappings by inventing protocol features. Record friction and map with the least-bad valid R6 + overlay semantics.

---

## Two questions (always separate)

| Question | What you are testing |
|---|---|
| **A — R6 model** | Participant identity, Wire decomposition, forwarding, non-CAN config |
| **B — CAN11 VCN** | Default/custom VCN, WireAlias pressure, Guest limits, CAN29 escape |

A sketch may conclude: *R6 excellent; CAN11 poor — use CAN29.* That is a valid outcome.

---

## Order of operations

```text
1. Participants
2. Wires
3. Interactions
4. Forwarding
5. Link profile choice
6. CAN11 binding (if applicable)
7. Narrower/overlapping Wires (only if justified)
```

Document **minimum mapping** (steps 1–5, plus 6 when CAN11 is chosen) before **optional optimizations** (6–7 when they add complexity).

---

## Trial corpus

Neutral problem statements live in [`archetypes/`](archetypes/). They contain **no** prior WireSpaces mappings.

| ID | Archetype | Notes |
|---|---|---|
| 01 | Simple CAN machine | Strong fit candidate |
| 02 | Pi robot | Convergence test |
| 03 | Dual-controller CAN | Partitioned / paired controllers |
| 04 | Multi-CAN gateway | Dual-core; 3× CAN; USB vs Eth config |
| 05 | Heterogeneous gateway | CAN + UART + shared memory + Linux |
| 06 | Overlapping Wire bandwidth | Deliberate overlapping Logical Buses |
| 07 | Dense peer CAN (8 domains) | **Negative control** |
| 08 | Ethernet peer mesh | **Boundary test** |
| 09 | Distributed chassis CAN cell | **CAN11 ceiling** (~29 VCN) |
| 08 | Ethernet peer mesh | **Boundary test** |

Completed mappings go in [`sketches/`](sketches/). Reviews go in [`reviews/`](reviews/).

**Before saving a sketch:** list `sketches/`, pick a **new** filename, **never overwrite** an existing file. See [`experiment_brief_r6_vcn.md` §11](experiment_brief_r6_vcn.md#11-output-files--naming-and-collision-rules).

---

## Per-sketch output

Use [`template_sketch_output.md`](template_sketch_output.md). Required:

- **Disposition block** (Question A vs B)
- **Minimum mapping** then **optional optimizations**
- **CAN11 accounting tables** (when CAN11 is used)
- **7 core friction signals** on the minimum mapping happy path
- **Model pressure** and **open questions**

Reviewers use [`template_review.md`](template_review.md).

---

## Friction signals (7 core)

Rate **None / Mild / Significant** on the minimum-mapping happy path only.

| Signal | Question |
|---|---|
| **Artificial Wire** | Was a Wire created that does not correspond to a meaningful communication relationship? |
| **Wire proliferation** | Does one subsystem become many Wires mainly to satisfy direction or scope semantics? |
| **Artificial hierarchy** | Does default VCN MainA/MainB (or similar) force a coordinator the system does not have? |
| **VCN pressure** | Do ordinary relations exceed default map or force large custom tables? |
| **WireAlias pressure** | Are extra aliases needed mainly for CAN11 limits, not semantic Wires? |
| **Configuration burden** | Does WS need substantially more topology/VCN configuration than the conventional design? |
| **Failure/topology mismatch** | Does Logical Bus + binding abstraction obscure partition or degraded behavior? |

Record Participant or interaction awkwardness in free-form notes when observed.

**CAN11 evidence scope:** A sketch that uses only MainA does not validate the full two-Main default map — record that explicitly when MainB is unassigned.

---

## Membership vs presence

Configured Wire membership is static; physical presence is runtime. A maintenance Participant may remain configured on a Wire while absent, or bindings may exist only in a maintenance configuration — state which.

---

## Tables (comparable across sketches)

**Devices**

| Device | Endpoint Domains | Link interfaces | Role summary |
|---|---|---|---|

**Wires**

| Wire (name / #) | Participants | Physical realization | Notes |
|---|---|---|---|

**Interactions**

| Interaction | Src Participant | Dest Participant(s) | Wire | Broadcast? | Notes |
|---|---|---|---|---|---|

**Gateway forwarding** (when applicable)

| Ingress link | Wire | Egress link(s) | Local delivery? | Splice? | Notes |
|---|---|---|---|---|---|

**CAN11 bindings** — use accounting tables from overlay §11.

---

## Wires are not Physical Links

Prefer **authority- or scope-shaped Logical Buses** that span Links when the system shares one propagation domain. Gateway **forwarding** preserves Wire identity; **composition** authors new traffic under the composer's ParticipantId.

A second Wire usually reflects a **second communication scope** (plant vs maintenance, safety vs telemetry), not merely a second cable.

---

## Progressive complexity

Sketches assume a stable WS product baseline (Services, Organizer, reliable segment transport) per `INTRO §9`, but each archetype picks an appropriate **maturity level** (Level 0–3). Err toward **lower** maturity when the archetype allows.

Commissioning narrative may be omitted; count configuration entries anyway.

---

## Synthesis

Cross-sketch rollup: [`synthesis.md`](synthesis.md). Append spec findings as `SF-R6-NNN`. Do not pre-fill verdicts before sketches exist.

---

## Historical material

Prior-round sketches under `../sketches/` used Origin/Node and multi-Origin remap experiments. They are **not** part of this read path. Neutral archetypes were extracted from physical/system descriptions only.

---

## References

| Doc | Code |
|---|---|
| `docs/introduction.md` | INTRO |
| `docs/core_architecture.md` | CORE |
| `docs/deployment.md` | DEPLOY |
| `docs/link_profiles.md` | LINK |
| `experiment_overlay_vcn.md` | Trial CAN11 overlay |

Cross-references in sketch text: `CORE §3.1`, `DEPLOY §1.6`, `overlay §5`, etc.

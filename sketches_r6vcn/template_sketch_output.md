# Sketch Output Template — R6 + VCN Trial

**Before writing:** list `sketches/` and confirm your output path does **not** already exist. Do not overwrite another agent's file.

Copy this file to a **new** path:

```text
sketches/<archetype_id>_<short_name>_<agent_id>.md
```

If that path exists, change `<agent_id>` until unique.

```text
**Archetype:** archetypes/NN_....md
**Agent:** <orchestrator-assigned id — not shared across runs>
**Output file:** sketches/<same as filename>
**Date:**
```

---

## Executive summary

```text
**Minimum mapping:** one sentence — Participants, Wire count, primary Link profiles.
**Question A (R6):** Natural / mildly awkward / poor — one phrase.
**Question B (CAN11):** Default / custom / poor fit / N/A — one phrase.
**Worst friction (minimum path):** signal name + None/Mild/Significant.
**Main lesson:** one sentence.
```

---

## Disposition block

| Area | Assessment |
|---|---|
| Participant identity | Natural / mildly awkward / poor |
| Wire decomposition | Natural / mildly awkward / poor |
| Forwarding | Simple / moderate / awkward |
| Non-CAN configuration | Low / moderate / high |
| CAN11 VCN fit | Default / custom / poor fit / N/A |
| Better with CAN29? | Yes / no / N/A |

**Explanation (2–4 sentences):** Separate Question A (R6) from Question B (CAN11).

---

## 1. Native communication model

Describe the system **without WireSpaces terminology** (from archetype; refine if needed).

---

## 2. Obvious conventional implementation

> **Obvious conventional implementation:** …

> **What structure does a conventional design use?** (IDs, sockets, shared memory, etc.)

---

## 3. Minimum mapping (required first)

Steps 1–5 from the brief. This is the **primary evidence**.

### 3.1 Participants

| ParticipantId | Endpoint Domain | Device | Notes |
|---|---|---|---|

### 3.2 Wires

| Wire (#) | Participants | Physical links | Purpose |
|---|---|---|---|

### 3.3 Interactions (primary)

| Interaction | Src | Dest | Wire | Broadcast? | Notes |
|---|---|---|---|---|---|

### 3.4 Forwarding (if any)

| Ingress | Wire | Egress | Notes |
|---|---|---|---|

### 3.5 Link profiles

| Physical link | Profile | WS Wire(s) | Notes |
|---|---|---|---|

### 3.6 CAN11 bindings (if CAN11 used)

Reproduce accounting tables from `experiment_overlay_vcn.md` §11.

**Count ordinary VCNs used in minimum mapping only.** List optional default VCNs (e.g. VCN 0 broadcast, MainB slots) as *available but unused* unless the minimum path uses them.

Note whether MainB is assigned; if not, state that two-Main default allocation is **not exercised** by this sketch.

**Membership vs presence:** If a Participant may be absent (e.g. maintenance laptop), state whether it is preconfigured on the Wire or only present in a maintenance configuration — do not conflate with "not a member when disconnected."

---

## 4. Optional optimizations

Only after §3. Document what problem each optimization solves.

- Narrower/overlapping Wires
- Custom VCN maps
- Extra WireAliases
- Guest vs committed choice changes

**Do not** propose Guest + committed CAN11 on the **same physical bus** unless legacy coexistence is an archetype requirement — mixed ingress is not specified (overlay §13). File a spec finding if the archetype forces that question.

---

## 5. Friction signals (minimum mapping happy path)

| Signal | None / Mild / Significant | One-sentence justification |
|---|---|---|
| Artificial Wire | | |
| Wire proliferation | | |
| Artificial hierarchy | | |
| VCN pressure | | |
| WireAlias pressure | | |
| Configuration burden | | |
| Failure/topology mismatch | | |

---

## 6. Model pressure

- **If you could change one WS concept** to simplify this system, what would it be?
- **Did WS expose a useful distinction** the conventional model obscures?

---

## 7. Open questions

Do not resolve normative architecture here.

---

## 8. Spec findings (optional)

If applicable, add row to `synthesis.md` with ID `SF-R6-NNN`.

---

## 9. Diagrams (optional)

Mermaid or ASCII — device-centric or Wire-centric as appropriate.

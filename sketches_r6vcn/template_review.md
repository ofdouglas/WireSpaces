# Review Template — R6 + VCN Trial

Copy to `reviews/<archetype_id>_<short_name>_<mapper>_<reviewer>.md`.

**Rules:** Do not redesign unless the mapping is **invalid** under R6 + overlay. Evaluate clarity, honesty of minimum mapping, and rubric alignment.

```text
**Sketch reviewed:** sketches/...
**Archetype:** archetypes/...
**Reviewer:**
**Date:**
```

---

## Validity

| Check | Pass / Fail | Notes |
|---|---|---|
| One ParticipantId per Endpoint Domain | | |
| Wires are loop-free Logical Buses | | |
| No Origin/Node/Direction in canonical layer | | |
| Forwarding preserves canonical PDU identity | | |
| CAN11 matches overlay (if used) | | |
| Minimum mapping presented before optimizations | | |
| No invented protocol features | | |

If any **Fail**, describe the minimum fix only — do not full redesign.

---

## Disposition agreement

| Area | Mapper assessment | Reviewer agrees? | Reviewer assessment if different |
|---|---|---|---|
| Participant identity | | | |
| Wire decomposition | | | |
| Forwarding | | | |
| Non-CAN configuration | | | |
| CAN11 VCN fit | | | |
| Better with CAN29? | | | |

---

## Minimum mapping review

- Is the minimum mapping the **simplest semantically correct** R6 mapping, or did the mapper skip an obvious simpler structure?
- Is configuration **counted honestly** (including generatable-but-real entries)?
- Is Question A separated from Question B in the narrative?

---

## Friction review

For each **Significant** signal on the minimum path: is the justification fair, or should it be Mild/None?

For each **None** on a negative-control archetype (07, 08): is the mapper hiding awkwardness in optimizations?

---

## Convergence notes (if applicable)

If other independent mappings of this archetype exist (reviewer may receive them for convergence pass only):

| Aspect | This sketch | Other sketch(s) | Material difference? |
|---|---|---|---|
| Participant count | | | |
| Wire count | | | |
| CAN11 default vs custom | | | |

---

## Summary verdict (diagnostic, not pass/fail)

```text
R6 model:
CAN11 VCN:
Configuration honesty:
Understandability:
```

2–4 sentences.

---

## Spec findings (optional)

New `SF-R6-NNN` rows suggested?

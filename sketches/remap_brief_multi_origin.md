# Sketch Remap Brief — Global Endpoint-Domain Identity and Multi-Origin Wires

**Status:** Phase 2 experiment instruction; not a specification and not an adopted change
**Audience:** Sketch-generating agents. Read [`README.md`](README.md) first — everything in it still applies unless this brief overrides it explicitly.
**Source proposal:** `docs/proposed/WireSpaces Change Proposal — Global Participants, Multi-Origin Wires, and Revised Addressing.md` (revision 2)

> **Do not read the source proposal before doing your remap.** It is listed for provenance only. It contains a section titled "Expected Simplifications" listing nine mechanisms the change is hoped to remove. An agent who reads that list before mapping will find that list. Everything you need is in this brief. Read the proposal afterwards if you want, when writing your open questions.

---

# 1. The question this experiment asks

The peer-CAN sketch (`02`) already showed significant friction for peer-addressed commands (`SF-014`, `SF-001`). Multi-Origin Wires obviously improve that case. That is not in doubt and is not what we are testing.

The question is:

> **Do the other sketches get simpler, or merely different?**

A change that improves one archetype and leaves five others rearranged-but-equal is a bounded profile feature. A change that simplifies the hierarchical majority as well is foundational. Those two outcomes lead to different decisions, and only the sketches can tell them apart.

The second question, subordinate to the first:

> **When a Wire disappears, does the thing it was carrying disappear, or does it reappear somewhere else?**

Section 4.4 makes this concrete and it is the single most important instruction in this brief.

---

# 2. Premises — settled, do not relitigate

Treat these as given. If you think one is wrong, record it in your open questions and map against it anyway.

## 2.1 Deployment-global Endpoint-Domain identity

The addressable participant is an **Endpoint Domain**, not necessarily a device. Each one holds one `ParticipantId` that is the same on every Wire it belongs to. This replaces per-Wire NodeId allocation.

```text
Physical Gateway ABC
    Participant 10   Real-Time Control Domain
    Participant 11   Field I/O Domain
    Participant 12   Host/Diagnostics Domain
```

Tooling may group several Domains under one larger device identity. A Participant belonging to three Wires is the same Participant on all three.

## 2.2 Global semantic identity vs carrier-local compact address

**This premise exists to keep you out of a known dead end.** Assume the following two-level model:

```text
canonical / deployment
    ParticipantId       stable Endpoint-Domain identity

Link profile
    local compact address, where the carrier needs one

Organizer
    owns the deterministic projection between them
```

A constrained carrier such as 11-bit Classical CAN may therefore represent Participants using a compact bus-local address rather than the global identity directly. **Assume every bus has enough addresses for its members.** Do not count identifier bits, do not compute address ceilings, and do not design the projection. Those are candidate 3 (§3) and are deferred.

This is a real cost the proposal must eventually pay — it is mapping machinery, and it is honest to say so — but it is a *carrier compression* concern, not a topology concern, and it is invisible at sketch level.

## 2.3 Origin is a per-interaction role

Any member of a Wire may act as Origin, subject to a per-Wire permitted-Origin policy. A PDU names the actual `ParticipantId` of the Origin anchoring that interaction. There is no Origin index table and no permanent Origin device class.

```text
Participant 42 initiates to Participant 17:
    Origin = 42   Participant = 17   Direction = OriginToParticipant

Participant 17 responds within that interaction:
    Origin = 42   Participant = 17   Direction = ParticipantToOrigin

Participant 17 later initiates independently:
    Origin = 17   Participant = 42   Direction = OriginToParticipant
```

A Wire may permit one, several, or all of its members to originate. A conventional single-coordinator system is expressed by permitting exactly one — it becomes a common usage pattern rather than a protocol restriction.

## 2.4 Direction is unchanged and structural

`WIRE-5` still holds in full. Direction means exactly:

> which side of this Origin/Participant relation produced this PDU

It does **not** mean request/reply, client/server, or command/status. The three-line example in §2.3 is one interaction shape, not the meaning of the field.

In particular, `DISP-7` still holds: **reply authority is never inferred by reversing Direction.** A response is legal because a binding rule authorizes it. If your remap has a Service replying, say which binding mode makes that legal, exactly as you would today.

## 2.5 Forwarding and splicing are unchanged

```text
ordinary forwarding
    preserves canonical routing identity across Link Interfaces

splice
    the sanctioned operation that rewrites the Wire representation
    before egress (CORE §6, SPLICE-1)
```

Device-private Wires, `kLocalDomain`, and the splice rule all continue to work as specified. Forward-vs-compose (`SF-004`, `SF-015`) is unaffected: cross-Wire relay is still an authoring error.

---

# 3. Out of scope — hard fence

None of the following may appear in a remap. If your mapping seems to require one, that is a finding: record it in open questions and proceed without it.

| Excluded | Why |
|---|---|
| Header size, field widths, bit layout | Deferred candidate 3; the previously proposed 48-bit form is withdrawn |
| WireNumber range or namespace allocation | Being re-derived; the previously proposed 7-bit width was wrong |
| CAN identifier layout and arbitration order | Being re-derived; interacts with commissioning priority |
| Participant/Origin address ceilings on any carrier | See §2.2 — assume sufficient |
| Commissioning narrative | Already out of scope for sketches (`README` deferred items) |
| Encoding of the permitted-Origin policy | Deployment configuration; describe it in prose, do not invent a format |

You are mapping **topology and authority**, not a wire format.

---

# 4. Method

## 4.1 Remap, do not re-sketch

Start from the existing sketch file. Same system, same configurations, same maturity levels. You are producing a **delta**, not an independent take — that is what `sketches_b/` is for, and mixing the two makes the comparison unreadable.

Reuse verbatim, without re-deriving:

- §3 Native communication model
- §4 Obvious conventional implementation
- the device list

Everything downstream of those may change.

## 4.2 What to produce

For each configuration in your sketch:

1. **Participants table** — every Endpoint Domain and its `ParticipantId`. Use readable low numbers; the values carry no meaning at this stage.
2. **Wires table** — revised, with a new column. See §4.6.
3. **Interactions table** — revised, with Origin named per interaction rather than per Wire.
4. **Wire accounting** — see §4.4. Mandatory.
5. **Authority policy** — see §4.5. Mandatory.
6. **Friction re-rating** — see §4.7.
7. **Changed / unchanged summary** — what actually moved, in plain sentences.

Keep tables slim, per the `README`. Do not invent Endpoint IDs or storage semantics beyond what the original sketch already committed to.

## 4.3 Gateway forwarding table

Reproduce it if the original had one. State explicitly whether any forwarding entry changed. If the answer is "no," say so — an unchanged forwarding story across a topology change is itself a useful result.

## 4.4 Every removed Wire must be accounted for

This is the core discipline of the experiment.

A Wire in the current model can be carrying up to three distinct things at once:

```text
route scope         which Links this traffic traverses
broadcast scope     who a broadcast reaches (WIRE-4)
command authority   who is structurally permitted to originate
```

Under single-Origin those three arrive bundled. Multi-Origin unbundles them, and a Wire may be removable because one of the three no longer needs its own Wire — but the other two do not evaporate.

So for **every** Wire that exists in the original sketch and not in your remap, write one row:

| Removed Wire | Route scope now provided by | Broadcast scope now provided by | Command authority now provided by |
|---|---|---|---|

"Nothing — it was genuinely redundant" is a legitimate and valuable entry. "Absorbed into WholeMachine" is not an answer for the broadcast column unless broadcasting to the whole machine is actually acceptable for that traffic; if it is not, the Wire has not been removed, it has been renamed.

## 4.5 Write out the authorization policy in full

Under single-Origin, "who may command whom" is largely structural: the Wire's Origin is the only participant that can originate on it, so the topology *is* the policy. Multi-Origin makes authority explicit, which means it has to be written down somewhere.

For each configuration, produce the permitted-Origin policy your remap depends on:

| Wire | Members | Permitted Origins | Endpoint-level restrictions beyond that |
|---|---|---|---|

Then answer directly:

> Is this policy smaller, simpler, or easier to audit than the Wire structure it replaced?

A sketch that drops from four Wires to one while gaining a twelve-row Origin-and-Endpoint permission matrix has **not** been simplified. It has moved complexity from a structure the Router enforces into a table a human maintains. That may still be the right trade — permission tables are more expressive than topology — but it must be visible in the result, not hidden by counting Wires.

Note the related consequence for `DISP-5`: Endpoint identity provides naming, and authority comes from Wires, bindings, and typed local access. When Wires thin out, one of those three sources weakens. Say whether the remaining two carry the load in your system.

## 4.6 Count Origins per Wire

Add one column to the Wires table:

| Wire | Origins needed | ... |
|---|---|---|

Give the actual number of participants that must be able to originate on that Wire in your mapping — not the number permitted by a policy you chose for convenience.

This is cheap for you and answers a question you are not otherwise being asked to evaluate: whether multi-Origin belongs in every PDU or only on the Wires that need it. If most Wires across most sketches need exactly one Origin, that finding matters a great deal. Just report the number.

## 4.7 Re-rate the same nine friction signals

Use the same signals and the same None / Mild / Significant scale as `README §6`, so results drop straight into the friction heatmap in [`synthesis.md`](synthesis.md). Report as a delta: `Significant → Mild`, or `None → None`.

**Two of the nine signals are now tautological. Do not report improvements in them as evidence.**

| Signal | Status under this brief |
|---|---|
| **Artificial Origin** | Tautological. You are no longer required to nominate one, so this improves by construction. Rate it, but do not cite it as a benefit. |
| **Identity awkwardness** | Largely tautological for the "NodeId vs peer identity" half. Still meaningful if global Domain identity is awkward for a *different* reason — say which. |
| **Wire proliferation** | **Informative.** Primary signal. |
| **Configuration burden** | **Informative.** Primary signal — and must account for §4.5's policy table, not just topology. |
| **Interaction awkwardness** | **Informative.** Primary signal. |
| **Forwarding tax** | **Informative.** |
| **Failure mismatch** | **Informative.** See §5.1. |
| **Artificial Wire** | Informative. |
| **Role instability** | **Watch for regression.** Origin is now a per-interaction role rather than a configured one. Does that make roles less stable during normal operation, or is it simply more honest about roles that were already changing? |

---

# 5. Two specific checks

## 5.1 What does "the Origin is offline" mean now?

`SF-012` recorded a **positive** result: configured Wire and Origin role persist when the coordinator device is absent, Nodes may still emit valid `NodeToOrigin`, no automatic role reassignment, and tooling can flag a clear degraded state (`TF-004`). That clarity comes from there being exactly one Origin to be absent.

If your sketch has a degraded or coordinator-absent configuration — **`04-C` above all, also `06`** — state what the equivalent diagnostic is when any permitted member may originate. Is the degraded state still crisp, still describable to an operator, still flaggable by tooling? Rate Failure mismatch accordingly.

## 5.2 Does global identity improve the splice story?

`CORE §6` notes that when several devices each splice their own device-private Wire onto one shared external Wire, their internal NodeId assignments must be coordinated because they become Nodes on the same logical Wire — and recommends giving each device its own external Wire during bring-up to avoid the coordination.

Deployment-global Endpoint-Domain identity should make that coordination unnecessary, since identities are unique by construction. **Sketch `05` is where this shows up**; `01-B` and `06` may also touch it.

Confirm or refute. If it holds, does it change the recommended bring-up pattern?

---

# 6. Output

One file per sketch, at:

```text
sketches/remap/01_dev_board.md
sketches/remap/03_rs485_sensors.md
...
```

Originals in `sketches/` are **not** edited. The comparison depends on them staying put.

Executive summary block at the top of each remap, in the `README §0` style plus two lines:

```text
**Remap of:** sketches/NN_name.md
**Net effect:** Simpler / Different / Worse — one clause of justification.
**Wires:** N before -> M after; K of M need more than one Origin.
**Authority:** structural before -> N-row policy table after (or: unchanged).
**Worst friction:** signal + rating, and whether it moved.
**Main lesson:** one sentence.
```

Cross-sketch findings go in the [Spec findings log](synthesis.md#spec-findings-log) as new `SF-0NN` rows, tagged `[remap]` in the Topic column so they are separable from Phase 1 findings. Do not resolve architecture decisions in either place.

---

# 7. Null results are results

Report "no meaningful change" plainly when that is what you find. Multi-Origin has to earn each Wire it removes.

Three specific failure modes to resist:

**Removing a Wire because you can.** If two Wires had different Origins but also different memberships, different broadcast scopes, or different route reach, collapsing them because Origin is no longer the differentiator is a mapping error. `SF-013` found that authority-shaped Wires spanning several Links is a **strong positive** of the current model; do not discard that pattern reflexively.

**Counting Wires as the score.** See §4.5.

**Reporting tautological improvements.** See §4.7.

The `README` core invariant still governs: model the system as it naturally exists, apply the minimum machinery, and record any machinery needed primarily to make the model fit rather than hiding it. That applies to *this* proposal exactly as it applies to the current design.

---

# 8. Sketch order and per-sketch notes

Run in this order. The early ones are the ones that can falsify the change; the last one can only confirm it.

## 8.1 `03_rs485_sensors` — negative control, run first

The documented strong-fit hierarchical case. A single controller-origin system should be **completely unaffected** by making Origin a per-interaction role.

Test that. If 03 gets more complex — extra policy, extra configuration, less clear poll story — then the change taxes the majority to help the minority, and that is close to decisive on its own. Confirm that semantic Origin remains cleanly separate from Link poll scheduler (`SF-003`), which multi-Origin should not touch.

## 8.2 `01_dev_board` — predicted simplification

Configs A and B. The specific hypothesis: Main control and PC maintenance coexist as different Origins on one shared Wire without separate authority Wires.

Test whether Bench-style separation survives on its own merits as a route or broadcast boundary once it is no longer needed to distinguish authority. Both answers are interesting. Note that `01-B` currently exhibits `SF-013` (authority Wire listing downstream Nodes reached by same-Wire forward) as a strong positive — check it survives.

## 8.3 `04_pi_robot` — predicted simplification, harder case

Configs A, B, C. Same question as 01 but with an intermittent Link and a genuine degraded configuration.

`04-C` is the primary site for §5.1. Also re-check `SF-004` forward-vs-compose: teleop composition should be unchanged by this proposal, and if your remap makes it look like forwarding, that is an error.

## 8.4 `05_multicore_gateway` — tests candidate 1 directly

The strongest predicted win, and it comes from global identity rather than multi-Origin. `SF-018` and `SF-019` both found that per-Domain Service instances need distinct wire-addressable identity, with per-Domain NodeId as a provisional resolution.

Does global Endpoint-Domain identity resolve those by construction, or only rename the problem? Primary site for §5.2. Also report whether multi-Origin adds anything here *beyond* what global identity already gave you — separating the two contributions in this sketch is especially valuable.

## 8.5 `06_redundant_gateway` — authority representation only

**Scope restriction:** multi-Origin is *not* claimed to address the physical-coverage boundary found in `SF-021` and `SF-022`. Exclusive bus ownership still means losing a gateway loses its buses, whoever is permitted to originate. Do not credit the change with improving redundancy.

Evaluate one thing: does representing A and B as independent Origin-capable Participants on one Wire improve the model, or weaken failure and authority clarity? The `README` fixed choice — partner forwarding modeled in the forwarding table, not as two Origins on one Wire — is exactly what is being questioned here, so you may depart from it, but say so explicitly and compare. Keep `SF-023` in view: the Service-Wire multipath hazard is unchanged.

## 8.6 `02_peer_can` — positive control, run last and cheaply

Known to improve. Run it as an upper bound so the heatmap has the far end of the range. A short remap is sufficient; do not spend effort proving the easy case.

---

# 9. Pre-registered decision rule

Recorded before any remap runs, so results cannot be read to fit whichever answer feels better afterwards.

**Adopt candidate 2 (multi-Origin) as a canonical change if all three hold:**

1. At least **three** of `{01, 03, 04, 05, 06}` show reduced Wire count **or** reduced configuration burden, *with no equivalent authorization table appearing* under §4.5;
2. **no** sketch regresses any informative signal from None or Mild to Significant, and `03` shows no regression at all;
3. improvement is visible in the **informative** signals of §4.7, not only the tautological ones.

**Treat multi-Origin as a bounded profile feature rather than a canonical change if:**

- fewer than roughly a third of Wires across all remaps need more than one Origin (§4.6) — this materially strengthens the extension-based alternative, in which hierarchical Wires stay single-Origin and only some Wires carry an explicit Origin; **or**
- the Wire reductions are consistently offset by authorization tables of comparable size.

That outcome would align with the standing recommendation in `DL-004` and `SF-014` — document the archetype limit, do not add a peer primitive from one sketch.

**If the result is mixed,** do not decide. Identify which system property predicts benefit and run one more archetype chosen to test it.

Candidate 1 (global Endpoint-Domain identity) is adopted in principle already and is not subject to this rule. The remaps inform its cost, not its adoption — though a clear negative in `05` would reopen it.

---

# 10. Revision history

| Date | Change |
|---|---|
| 2026-08-22 | Created for the Phase 2 multi-Origin remap |

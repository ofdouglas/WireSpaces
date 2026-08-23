# Sketch Remap Brief — Global Endpoint-Domain Identity and Multi-Origin Wires

**Status:** Phase 2 experiment instruction; not a specification and not an adopted change  
**Audience:** Sketch-generating agents. Read [`README.md`](README.md) first — everything in it still applies unless this brief overrides it explicitly.  
**Source proposal:** `docs/proposed/WireSpaces Change Proposal — Global Participants, Multi-Origin Wires, and Revised Addressing.md` (revision 2)

> **Do not read either of these files before completing your remap:**
>
> - `docs/proposed/WireSpaces Change Proposal — Global Participants, Multi-Origin Wires, and Revised Addressing.md`
> - `docs/proposed/remap_evaluator_preregistration.md`
>
> They are listed for provenance/audit only and contain expected outcomes or decision criteria that would bias this experiment. Everything needed for the remap is in this brief. Read them afterwards if useful when writing open questions.

> **Assignment rule:** Each sketch-generating agent remaps **exactly one sketch**. Do not give one agent multiple remaps, prior remap results, or the evaluator preregistration. Cross-sketch comparison belongs to the evaluator after all remaps are complete.

---

# 1. The question this experiment asks

The experiment evaluates two proposed changes together at mapping level:

1. **Deployment-global Endpoint-Domain identity** (`ParticipantId`).
2. **Multi-Origin Wires**, where Origin is a per-interaction role rather than a permanent per-Wire participant.

The primary question is:

> **Do existing systems become simpler, or merely different?**

A Wire reduction is not automatically a simplification. If topology disappears but equivalent complexity reappears as authorization policy, route rules, broadcast restrictions, failure-state interpretation, or Service bindings, record that explicitly.

The second question is:

> **When a Wire disappears, what happens to every responsibility that Wire was carrying?**

Section 4.4 makes this concrete and is the central accounting rule of the experiment.

A third question is attribution:

> **Which observed change comes from global identity, which comes from multi-Origin, and which requires both?**

Do not credit one candidate with a benefit caused by the other.

---

# 2. Premises — settled for this experiment, do not relitigate

Treat these as given. If you think one is wrong, record it in open questions and map against it anyway.

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

Assume the following two-level model:

```text
canonical / deployment
    ParticipantId       stable Endpoint-Domain identity

Link profile
    local compact address, where the carrier needs one

Organizer
    owns the deterministic projection between them
```

A constrained carrier such as 11-bit Classical CAN may therefore represent Participants using a compact bus-local address rather than the global identity directly.

**Assume every bus has enough local addresses for its members.** Do not count identifier bits, compute address ceilings, or design the projection. Those belong to the deferred addressing/layout work.

This is still real machinery the proposal must eventually pay for. It may be mentioned as carrier-compression complexity, but it is not topology friction at sketch level.

## 2.3 Origin is a per-interaction role

Any member of a Wire may act as Origin, subject to a per-Wire permitted-Origin policy. A PDU names the actual `ParticipantId` of the Origin anchoring that interaction. There is no Origin index table and no permanent Origin device class.

```text
Participant 42 initiates to Participant 17:
    Origin = 42
    Participant = 17
    Direction = OriginToParticipant

Participant 17 responds within that interaction:
    Origin = 42
    Participant = 17
    Direction = ParticipantToOrigin

Participant 17 later initiates independently:
    Origin = 17
    Participant = 42
    Direction = OriginToParticipant
```

A Wire may permit one, several, or all of its members to originate. A conventional coordinator system may still permit exactly one participant to originate.

### Single-Origin policy accounting rule

If a remapped Wire still has exactly one legitimate Origin and that authorization is directly derivable from the same high-level Wiring declaration already required to describe the system, count it as **zero additional authored configuration**.

You must still show the policy row for analysis, but do not call the mere existence of a generated row a configuration regression.

If the user must author or review new authorization state that did not previously exist structurally, count that burden honestly.

## 2.4 Direction is unchanged and structural

`WIRE-5` still holds in full. Direction means exactly:

> which side of this Origin/Participant relation produced this PDU

It does **not** mean request/reply, client/server, or command/status. The three-line example in §2.3 is one interaction shape, not the meaning of the field.

`DISP-7` still holds: **reply authority is never inferred by reversing Direction.** A response is legal because a binding rule authorizes it. If your remap includes a Service reply, identify the binding mode that permits it exactly as you would in the current design.

## 2.5 Broadcast semantics under multi-Origin

Broadcast remains a **forward-direction interaction** anchored to the actual Origin of that PDU.

```text
Origin = Participant 42
Participant = broadcast
Direction = OriginToParticipant
```

A permitted Origin may broadcast when its Wire/Endpoint authorization allows that interaction. The broadcast reaches the Wire's configured members according to the existing `WIRE-4` broadcast scope.

There is still **no `ParticipantToOrigin` broadcast**. Reverse-direction traffic remains anchored to one actual Origin/Participant relation.

The canonical encoded value or representation used for the broadcast participant is deferred with the addressing/layout work. Do not reason about its bit pattern or numeric value in this experiment.

## 2.6 Configured observation is unchanged

Configured observation remains available with its existing semantics: a participant may be explicitly authorized to consume traffic for which it is not the addressed required sink.

Multi-Origin does **not** automatically turn observation into addressing, broadcast, forwarding, or redundancy.

If an original sketch used observation and the remap no longer needs it, account for what replaced it. A disappearance of observation is only a simplification if recipient scope, authorization, transmission count, and failure behavior remain correct.

## 2.7 Forwarding and splicing are unchanged

```text
ordinary forwarding
    preserves canonical routing identity across Link Interfaces

splice
    the sanctioned operation that rewrites the Wire representation
    before egress (CORE §6, SPLICE-1)
```

Device-private Wires, `kLocalDomain`, and the splice rule continue to work as specified.

Forward-vs-compose (`SF-004`, `SF-015`) is unchanged:

> **Cross-Wire relay is still composition/re-origination, not forwarding.**

If a remap makes an existing compose step look like forwarding merely because Wires were collapsed, examine whether the semantic operation actually became one interaction. If not, the collapse is wrong.

---

# 3. Out of scope — hard fence

None of the following may appear in a remap. If your mapping seems to require one, record that as a finding and proceed without designing it.

| Excluded | Why |
|---|---|
| Header size, field widths, bit layout | Deferred addressing/layout work |
| WireNumber range or namespace allocation | Being re-derived |
| CAN identifier layout and arbitration order | Being re-derived; interacts with commissioning priority |
| Participant/Origin address ceilings on any carrier | See §2.2 — assume sufficient |
| Commissioning narrative | Already out of scope for sketches |
| Encoding of the permitted-Origin policy | Deployment configuration; describe it in prose only |

You are mapping **topology, authority, route scope, broadcast scope, failure meaning, and configuration burden** — not a wire format.

---

# 4. Method

## 4.1 Remap, do not re-sketch

Start from the existing sketch file. Same system, same configurations, same maturity levels.

You are producing a **delta**, not an independent take.

Reuse verbatim, without re-deriving:

- Native communication model
- Obvious conventional implementation
- device list

Everything downstream of those may change.

Do not optimize for fewer Wires. Optimize for the most natural mapping under the new premises.

## 4.2 What to produce

For each configuration:

1. **Participants table** — every Endpoint Domain and its `ParticipantId`.
2. **Wires table** — revised, with `Origins needed`.
3. **Interactions table** — revised, naming Origin per interaction.
4. **Gateway forwarding table** — if the original had one.
5. **Wire accounting** — §4.4.
6. **Authority policy** — §4.5.
7. **Change attribution** — §4.6.
8. **Observation accounting** — §5.4 where the original sketch used configured observation.
9. **Friction re-rating** — §4.8.
10. **Changed / unchanged summary** — plain sentences describing what actually moved.

Keep tables slim. Do not invent Endpoint IDs or storage semantics beyond what the original sketch already committed to.

## 4.3 Gateway forwarding table

Reproduce it if the original had one.

State explicitly whether any forwarding entry changed.

If the answer is **no**, say so. An unchanged forwarding story across a topology or authority change is itself a useful result.

Do not collapse per-Domain forwarding into a device-global Router if the original architecture has separate Endpoint Domains.

## 4.4 Every removed Wire must be accounted for

This is the core discipline of the experiment.

A Wire in the current model can carry several distinct responsibilities at once:

```text
route scope
    which Links / forwarding projection this traffic traverses

broadcast scope
    who a Wire-scoped broadcast reaches

command authority
    who is structurally permitted to originate

failure / degraded-state distinction
    which relationship can be reported unavailable or degraded independently
```

Under single-Origin these may be bundled. Multi-Origin can unbundle command authority, but the other responsibilities do not disappear automatically.

For **every Wire that exists in the original sketch and not in your remap**, write one row:

| Removed Wire | Route scope now provided by | Broadcast scope now provided by | Command authority now provided by | Failure/degraded-state distinction now provided by |
|---|---|---|---|---|

Valid answers include:

- `Nothing — it was genuinely redundant`
- `Existing Wire X, unchanged`
- `Service binding Y`
- `Generated permitted-Origin policy`
- `No longer distinguishable — regression`

Invalid answer:

- `Absorbed into WholeMachine`

unless the row also explains why WholeMachine's route reach, broadcast membership, authorization, and degraded-state reporting are all genuinely correct for the removed Wire's traffic.

If a removed Wire's broadcast scope cannot be preserved without a new receiver filter or Service rule, record that new rule.

## 4.5 Write out the authorization policy in full

Under single-Origin, command authority is often structural: the Wire's configured Origin is the only participant that can originate.

Under multi-Origin, authority becomes explicit.

For each configuration, produce:

| Wire | Members | Permitted Origins | Endpoint-level restrictions beyond that | Authored or generated? |
|---|---|---|---|---|

Then answer directly:

> **Is this policy smaller, simpler, or easier to audit than the Wire structure it replaced?**

A sketch that drops from four Wires to one while gaining a twelve-row manually authored Origin/Endpoint permission matrix has **not** obviously simplified.

However, do not count flattened generated rows as equivalent to authored complexity automatically.

For every nontrivial policy, also answer:

> **Could this policy be generated from Service bindings and topology already required for the system, or is it genuinely new authorization state?**

Distinguish:

```text
author intent
    what the system designer must state and understand

generated projection
    flattened rows emitted by Organizer
```

Judge configuration burden primarily from the former, while still reporting the size/complexity of the latter when it affects auditability.

Related consequence for `DISP-5`:

> Endpoint identity provides naming, while authority comes from Wires, bindings, permitted-Origin policy, and typed local access.

If Wires become broader or fewer, say which remaining mechanism now carries the authority distinction that topology previously carried.

## 4.6 Attribute every meaningful change

For each meaningful simplification, regression, or structural change, tag its cause:

| Change | Attribution |
|---|---|
| ... | **Global identity** |
| ... | **Multi-Origin** |
| ... | **Both / inseparable** |
| ... | **Neither / incidental remap difference** |

Do not credit multi-Origin for a benefit caused only by deployment-global Participant identity.

Do not credit global identity for Wire reductions that arise only because multiple participants can now originate on one Wire.

If the two changes interact such that the benefit cannot reasonably be separated, mark **Both / inseparable** and explain why.

## 4.7 Count Origins per Wire

Add these columns to every Wires table:

| Wire | Origins needed | System role | ... |
|---|---:|---|---|

`System role` is a short classification such as **primary**, **supporting**, or **peripheral**. Use it to show whether multi-Origin is needed on the system's main interaction path or only on a secondary/debug/maintenance relationship.

Count every Participant that must legitimately originate on that Wire in **any supported runtime state represented by the sketch**, not merely nominal operation.

Examples:

- A permanently coordinator-driven sensor Wire: `1`
- A peer Wire where all four peers initiate: `4`
- A redundant-control Wire where B only originates after supported failover: `2`

Give the number the system actually requires, not a larger policy chosen for convenience.

Also report at the end of each sketch:

```text
Wires after remap: M
Wires requiring >1 Origin: K
Multi-Origin Wires by role: primary / supporting / peripheral
```

Do not infer an architectural decision from this count inside the remap.

## 4.8 Re-rate the same nine friction signals

Use the same signals and the same `None / Mild / Significant` scale as `README §6`.

Report each as a delta:

```text
Interaction awkwardness: Significant → Mild
Configuration burden: Mild → Mild
```

### Interpretation under this experiment

| Signal | Treatment |
|---|---|
| **Artificial Origin** | Tautological improvement risk. Rate it, but do not cite improvement as evidence for multi-Origin. |
| **Identity awkwardness** | Global identity removes some prior awkwardness by construction. Only count remaining/new awkwardness and attribute it correctly. |
| **Wire proliferation** | Primary informative signal. |
| **Configuration burden** | Primary informative signal. Must include authored authorization policy, not merely Wire count. |
| **Interaction awkwardness** | Primary informative signal. |
| **Forwarding tax** | Informative. |
| **Failure mismatch** | Informative. See §5.1. |
| **Artificial Wire** | Informative. |
| **Role instability** | Watch for regression. Origin is now per interaction; judge whether supported runtime behavior becomes harder to understand or audit. |

### Per-sketch trade rule

If you claim a sketch became **simpler**, it must have:

1. at least one improvement in an informative signal; and
2. no informative regression of equal or greater practical severity,

unless you explicitly describe the trade as favorable and explain why.

A reduction in Wire count by itself is not enough.

---

# 5. Specific checks

## 5.1 Coordinator absence and degraded-state clarity

Some original sketches contain a configured coordinator that may become unavailable at runtime.

Under the current model this can produce a crisp diagnostic:

```text
Wire exists
configured Origin = X
X is offline
```

Under multi-Origin, evaluate what the equivalent operational statement becomes.

Do **not** force the answer to remain Wire-wide. A valid remap might instead expose capability-specific authority state, for example:

```text
Participant 10 unavailable as plant-control authority
Participant 22 unavailable as maintenance authority
```

For every degraded/coordinator-absent configuration:

- state exactly what operators/tooling can report;
- state whether any role is reassigned;
- state whether valid traffic from remaining participants can continue;
- compare the clarity to the original mapping;
- rate `Failure mismatch` and `Role instability` accordingly.

The test is whether the degraded state remains **crisp, correct, and auditable**.

## 5.2 Global identity and splice coordination

`CORE §6` notes that when several devices splice device-private Wires onto one shared external Wire, internal NodeId assignments may need coordination because those participants become members of one external Wire.

Deployment-global Endpoint-Domain identity may remove that coordination because Participant identities are unique by construction.

Where the original sketch contains or discusses this pattern:

1. confirm whether global Participant identity removes the coordination requirement;
2. note where a carrier-local address projection is still required under §2.2, **without specifying or designing that projection**;
3. state whether the recommended bring-up pattern changes;
4. attribute the change to **Global identity**, not multi-Origin unless multi-Origin is also independently required.

## 5.3 Forward vs compose must remain stable

For every original interaction classified as application composition:

- verify whether it remains composition;
- if it becomes ordinary forwarding, explain what semantic boundary disappeared.

If no semantic boundary disappeared, treating it as forwarding is a remap error.

## 5.4 Observation accounting

Where the original sketch used configured observation, state for each such path whether the remap:

- keeps observation unchanged;
- removes it because direct addressing/broadcast now expresses the relationship;
- changes its purpose; or
- replaces it with another mechanism.

For every removed or changed observation path, compare:

```text
transmission count
recipient / broadcast scope
authorization
required sink vs optional observer status
failure / degraded-state meaning
```

If direct multi-Origin interaction replaces observation, say whether that is a genuine simplification or merely a different representation.

Observation still does **not** count as independent redundancy coverage unless the underlying delivery path is actually independent.

---

# 6. Output

One file per sketch:

```text
sketches/remap/01_dev_board.md
sketches/remap/03_rs485_sensors.md
...
```

Originals in `sketches/` are not edited.

Use this executive summary block:

```text
**Remap of:** sketches/NN_name.md
**Net effect:** Simpler / Different / Worse — one clause of justification.
**Wires:** N before -> M after; K of M need more than one Origin; multi-Origin roles: primary/supporting/peripheral.
**Authority:** structural before -> N authored policy rules after; generated projection: brief note.
**Attribution:** Global identity: ... ; Multi-Origin: ... ; Both: ...
**Worst friction:** signal + rating, and whether it moved.
**Main lesson:** one sentence.
```

If there are zero new authored policy rules because authorization is derivable from existing high-level Wiring/Service declarations, say so explicitly.

Cross-sketch findings go in the [Spec findings log](synthesis.md#spec-findings-log) as new `SF-0NN` rows tagged `[remap]`.

Do not resolve architecture decisions in either place.

---

# 7. Null results are results

Report **no meaningful change** plainly when that is what you find.

Multi-Origin has to earn each Wire it removes.

Resist these failure modes:

## 7.1 Removing a Wire because you can

If two Wires had different Origins but also different memberships, broadcast scopes, route reach, failure-state meaning, or Service-policy boundaries, collapsing them merely because Origin no longer differentiates them is a mapping error.

## 7.2 Counting Wires as the score

A topology reduction offset by equivalent authored authorization/configuration complexity is not automatically a simplification.

## 7.3 Counting generated rows as authored burden

A large flattened policy or route table may be mechanically generated from a compact declaration.

Report both, but do not confuse them.

## 7.4 Reporting tautological improvements

Artificial-Origin and some prior identity friction improve by construction under the premises. Report the deltas but do not use them as primary evidence.

## 7.5 Smuggling authority into Endpoint code

If collapsing Wires causes Services to acquire ad-hoc source checks, target filters, or authorization logic that was previously structural, that is moved complexity.

Record it under authorization/configuration and interaction awkwardness.

The `README` core invariant still governs:

> model the system as it naturally exists, apply the minimum machinery, and record machinery needed primarily to make the model fit.

Apply that equally to the current architecture and to this proposal.

---

# 8. Per-sketch neutral checks

Each agent is assigned **one** source sketch only. There is no experimental run order visible to remapping agents.

Use the subsection matching your assigned sketch and ignore the others.

## 8.1 `01_dev_board` — small gateway case

Remap Configs A and B.

Check:

- whether the existing control and maintenance Wires remain independently justified by route scope, broadcast scope, failure meaning, or Service policy;
- whether multi-Origin permits a natural collapse;
- whether same-Wire forwarding to downstream participants remains clear (`SF-013`);
- whether global identity changes participant configuration;
- whether any original observation path remains necessary.

Do not optimize for one Wire.

## 8.2 `02_peer_can` — symmetric peer case

Remap the peer-CAN system.

Keep the remap concise, but perform the same accounting:

- removed-Wire table;
- full permitted-Origin policy;
- origins-per-Wire count and system role;
- configuration burden including authored vs generated policy;
- observation accounting;
- interaction awkwardness.

Do not assume that fewer Wires means lower total complexity.

## 8.3 `03_rs485_sensors` — hierarchical polled case

Remap the existing master-initiated sensor-chain configurations.

Check:

- whether the Wire count changes naturally;
- whether single-controller authorization remains compact;
- whether semantic Origin remains separate from Link poll initiation (`SF-003`);
- whether global identity changes anything material;
- whether any new policy must be authored.

Do not assume the correct result is unchanged.

## 8.4 `04_pi_robot` — intermittent gateway and degraded-state case

Remap Configs A, B, and C.

Check:

- whether changing Ethernet to intermittent WiFi still remains a Link concern rather than a Wire change;
- whether existing plant and maintenance separation remains useful;
- `04-C` against §5.1;
- `SF-004` forward-vs-compose: teleop composition must remain semantically correct;
- whether any observation/logging path changes under the new model.

## 8.5 `05_multicore_gateway` — multicore identity case

Remap the existing multicore gateway.

Check:

- whether global Endpoint-Domain identity resolves `SF-018` / `SF-019` by construction or merely renames the issue;
- whether same-Origin subsystem Wires naturally remain separate or collapse;
- whether multi-Origin contributes anything beyond global identity;
- §5.2 where device-private/external Wire identity coordination appears;
- whether per-Domain routing remains explicit across shared-memory Links;
- whether observation branches used for logging/diagnostics change.

Separate Candidate 1 and Candidate 2 attribution carefully here.

## 8.6 `06_redundant_gateway` — redundant-controller authority case

Remap the existing redundant-controller topology without changing its physical coverage.

Check:

- whether separate controller-Origin Wires naturally remain separate;
- whether one multi-Origin Wire improves or weakens source-selection clarity;
- what happens to failure/authority diagnostics under §5.1;
- whether partner forwarding remains explicit;
- whether any existing multipath hazard remains unchanged;
- whether redundancy policy moves from Wire topology into authorization/Service state;
- whether observation paths remain awareness-only or become part of a different interaction.

Do not credit multi-Origin with physical redundancy it does not create.

---

# 9. End-of-remap summary required from each agent

Because each agent remaps exactly one sketch, report only that sketch's result:

```text
Sketch:
Wires before:
Wires after:
Wires requiring >1 Origin:
Multi-Origin Wires by role: primary / supporting / peripheral

Informative friction improved:
Informative friction regressed:
Informative friction unchanged:

Benefits attributable to Global identity:
Benefits attributable to Multi-Origin:
Benefits attributable to Both / inseparable:

New authored authorization state introduced:
Existing structural authority removed:
Observation paths removed / retained / changed:
```

Do not compare against other remaps and do not recommend adoption or rejection of either candidate.

Cross-sketch aggregation and architecture judgment belong to the evaluator after all blinded remaps are complete.

---

# 10. Revision history

| Date | Change |
|---|---|
| 2026-08-22 | Created for the Phase 2 multi-Origin remap |
| 2026-08-22 | Revised experiment hygiene: blinded predictions, Candidate 1/2 attribution, authored-vs-generated policy accounting, degraded-state accounting, and per-sketch trade rule |
| 2026-08-22 | Added explicit multi-Origin broadcast semantics, observation accounting, one-agent-per-sketch isolation, multi-Origin role significance, and evaluator-file placement rule |

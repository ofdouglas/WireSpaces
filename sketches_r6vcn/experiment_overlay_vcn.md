# Experiment Overlay — Unified CAN11 VCN

**Status:** Trial-only; supersedes `docs/link_profiles.md` §2.4–2.6 for the R6+VCN sketch experiment  
**Authority:** Governs CAN11 committed and Guest layout when read with `experiment_brief_r6_vcn.md`  
**Source:** Distilled from `docs/proposed/WireSpaces_CAN11_Unified_VCN_Proposal.md` — sketch agents must not read the proposal

---

## 1. What this overlay replaces

For this experiment, **ignore** these normative sections in `link_profiles.md`:

- §2.4 Guest VCN (three-bit Guest layout as currently written)
- §2.5 Native VCN (8-bit VCN without WireAlias)
- §2.6 Native Participant-Compressed

Retain from `LINK`:

- §2.1–2.3 (one Wire per binding, reconstruction pipeline, VCN relation semantics)
- Non-CAN profiles, PDUA general rules, commissioning control space references
- Ingress/egress validation *intent* (fail closed on ambiguous reconstruction)

**Participant-Compressed is not available** in this trial unless a future comparison arm explicitly reopens it.

---

## 2. Unified committed CAN11 identifier

```text
QoS                     2
WireAlias               3
VirtualCircuitNumber    5
Direction               1
-------------------------
                       11
```

Capacity per active WireAlias binding:

```text
4 QoS classes
8 WireAlias values (0..7)
32 VCN values per alias binding (0..31)
2 directions
```

`Direction`: `0` = AToB, `1` = BToA per the stored unordered pair for that VCN.

---

## 3. WireAliasBinding

Each committed CAN11 Link Binding references exactly one WS Wire via a **WireAliasBinding**:

```text
WireAliasBinding {
    WireAlias           0..7
    canonical WireNumber
    VCN mapping         Default | Explicit
    role positions      (when Default) MainA, MainB, Node0..Node13 PIDs
    explicit VCN table  (when Explicit)
}
```

Ingress:

```text
WireAlias -> {WireNumber, VcnMap}
VCN + Direction -> {SrcParticipantId, DestParticipantId}
=> canonical PDU identity (+ QoS from frame bits)
```

**VCN is scoped by WireAlias binding**, not by canonical WireNumber alone. Two aliases may bind the same WireNumber with different VCN maps (migration).

> **Every active WireAlias has exactly one canonical Wire binding. Multiple active WireAliases may bind the same canonical Wire.**

> **The meaning of an active WireAlias shall not be changed in place.** Migration uses a spare alias (§8).

Each CAN11 Link Binding still carries **exactly one** WS Wire at the binding level; multiple Wires on one physical bus use multiple bindings (separate Guest allocation and/or separate committed bindings with distinct classification).

---

## 4. VCN relation rules (unchanged semantics)

From `LINK` §2.3 — still apply:

- VCN names an **unordered participant relation** `{A, B}` or broadcast `{A, kBroadcast}`
- Self-pairs prohibited
- At most one ordinary VCN per unordered pair
- At most one broadcast-source VCN per participant
- Egress `(src, dest)` lookup must be unique within a binding
- Endpoint comes from PDUA, not from VCN

---

## 5. Standard default VCN mapping (5-bit)

When `VCN mapping = Default`:

```text
VCN 0       MainA -> Broadcast
VCN 1       MainB -> Broadcast
VCN 2       MainA <-> MainB
VCN 3       Reserved — Link control (profile-independent)

VCN 4       Node0 <-> MainA
VCN 5       Node0 <-> MainB
VCN 6       Node1 <-> MainA
VCN 7       Node1 <-> MainB
...
VCN 30      Node13 <-> MainA
VCN 31      Node13 <-> MainB
```

For `VCN >= 4`: `NodeIndex = (VCN - 4) / 2`; even VCN = Node↔MainA, odd VCN = Node↔MainB.

Supports: 2 Main positions, 14 Node positions, two broadcast channels, MainA↔MainB, every Node↔each Main — **without** a 32-entry custom edge table.

**MainA and MainB are CAN profile positions, not WS roles.** Assign any Participants that benefit from the default map (controller + Linux, primary + redundant controller, gateway + service tool, etc.). They do not reintroduce Origin/Node.

Role position → canonical `ParticipantId` is **deployment configuration**, not protocol-fixed.

---

## 6. Guest CAN11

Guest uses an **allocated contiguous 16-ID block** on an existing bus:

```text
Guest-3 (narrow):
    VCN[2:0] + Direction in low 4 bits; high bits = GuestBase

Guest-4 / Guest-5 / Guest-6:
    wider VCN field per allocated block size (trial sketches: state width used)
```

Guest frames carry **no WireAlias and no per-frame QoS**. The Link Binding supplies fixed canonical QoS and local Wire context.

> **Guest VCN meanings are global within the WireSpace deployment.**

Guest cannot select among multiple VCN mapping domains in-frame. This is an intentional capability limit.

Ordinary Guest capacity (Guest-3 style): VCN 0..6 ordinary, VCN 7 reserved for Link control → 7 ordinary relations, 14 directional IDs in the 16-ID block.

Larger Guest blocks may expose more VCN bits; existing VCN meanings remain stable when widening.

---

## 7. Reserved Link control

**VCN 3** in the default 5-bit map is reserved for Link control / commissioning in committed mode, consistent across QoS.

Guest reserved VCN is the all-ones value in the Guest width (e.g. VCN 7 in Guest-3).

Sketches need not design commissioning frames; treat reserved VCN as unavailable for application relations.

---

## 8. Migration via spare WireAlias

To change a VCN map without silent mismatch:

```text
1. allocate unused WireAlias
2. bind to same (or new) canonical WireNumber
3. install new VCN map under new alias
4. migrate participants
5. retire old alias after transition
```

Record spare-alias consumption as configuration cost when migration is part of the archetype story.

---

## 9. WireAlias 0 / kLocalBus

Bring-up may use:

```text
WireAlias 0 -> kLocalBus, Default VCN map
```

Named deployments may reassign aliases (e.g. Alias 0 → W17). Default is convenience, not a permanent restriction.

---

## 10. When to use CAN29 instead

CAN11 is not required to represent every topology. Prefer **CAN29** (or richer profile) when sketches honestly need:

- many central Participants not fitting MainA/MainB default
- large custom VCN tables on multiple aliases
- many Wires on one physical CAN without manageable binding count
- arbitrary dense peer graphs on CAN

Record **Better with CAN29?** in the disposition block.

---

## 11. CAN11 accounting (required in sketches)

For **each** physical CAN11 bus, reproduce these tables from the mapping.

### Counting rules

**`Ordinary VCNs used`** = count of distinct ordinary VCN slots that carry **minimum-mapping** traffic only.

| Count | Do not count |
|---|---|
| Each Main↔Node (or peer) relation used in §3 minimum interactions | VCN 0 broadcast if minimum mapping uses directed paths only |
| Custom explicit entries actually used | MainB odd slots when MainB unassigned |
| | VCN 3 (reserved control) |
| | Optional-optimization VCNs (list in §4 only) |

Also report:

```text
VCNs available by default but unused:   (e.g. VCN 0, VCN 2 MainA↔MainB, odd MainB slots)
MainB assigned?                         yes | no
```

When MainB is unassigned, state explicitly that the sketch does **not** exercise the two-Main default allocation — only the single-Main / Node portion.

### Committed CAN11

```text
Profile:          committed
WireAliases used:             N / 8
  default map:                  N
  custom map:                   N

Per alias:
  Alias:                        _
  Canonical Wire:               W__
  Mapping:                      Default | Explicit
  Ordinary VCNs used:           N / 32   (minimum mapping only)
  VCNs available, unused:       (list or "none")
  Default map sufficient?       yes | no
  Custom entries (if Explicit): N
  MainA PID / MainB PID:         __ / __
  Node positions used:          N / 14
```

### Guest CAN11

```text
Profile:          Guest
Guest width:      3 | 4 | 5  (VCN bits)
GuestBase:        0x___
Ordinary VCNs required:       N
Ordinary capacity:            (per width)
Default/global mapping sufficient?  yes | no
```

---

---

## 13. One profile classifier per physical CAN11 bus (trial limitation)

For this experiment, assume **each physical Classical CAN bus uses one ingress classifier**:

- either **committed** unified VCN for all WS frames on that bus, **or**
- **Guest** within one allocated block, **or**
- non-WS legacy IDs outside WS bindings.

**Simultaneous committed-format IDs and Guest-block IDs on the same physical bus** — with distinct frame layouts requiring per-ID classification — is **not specified** in the trial overlay.

Sketches may mention mixed legacy coexistence as a **future** or **open** question. Do not present Guest + committed bindings on one bus as a valid minimum or optional optimization unless filing `SF-R6-NNN` and marking it *requires unspecified mixed-profile ingress*.

Separate physical CAN segments each with their own profile are fine.

---

## 14. Explicit non-goals (CAN11 overlay)

- Runtime VCN map mutation
- Per-LLL independently authored VCN tables
- Transparent interpretation across unrelated WireSpaces deployments
- Restoring Participant-Compressed in this trial
- Mixed Guest + committed ingress on one physical bus (§13)

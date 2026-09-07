# WireSpaces Change Proposal — Global Endpoint-Domain Identity and Multi-Origin Wires

**Status:** Three separable candidates at three different stages. Candidate 1 adopted in principle; candidate 2 under sketch evaluation; candidate 3 withdrawn and deferred
**Scope:** Participant identity, Wire semantics, Origin role. Canonical header and Classical CAN encoding are explicitly **not** in scope of this revision
**Revision:** 2. Supersedes revision 1, which proposed all three as one bundled change including a 48-bit descriptor, a 7-bit WireNumber, and a CAN-11 identifier projection. Those specifics are withdrawn — see §7

Cross-references use the document code plus a section number, for example `CORE §3.1`. A bare `§x` means the current document.

---

# 1. Summary

Revision 1 proposed replacing the single-Origin Wire model, per-Wire NodeIds, WireAliases, and the 40-bit descriptor in one step. Review found the semantic direction sound and two of the encoding decisions unworkable as written. This revision splits the proposal into three candidates that can be decided independently, and in a defensible order: **semantics first, encoding last.**

| # | Candidate | Stage | Depends on |
|---|---|---|---|
| 1 | Deployment-global Endpoint-Domain identity | Adopted in principle | — |
| 2 | Origin as a per-interaction role | Under sketch evaluation | 1 |
| 3 | Canonical header and carrier encodings | Withdrawn, deferred | 1 and 2 both settled |

The conceptual core, unchanged from revision 1:

> **A Participant identifies who. A Wire identifies the communication scope and route. Origin identifies which Participant anchors the current interaction.**

What revision 1 got wrong was not that thesis. It was locking a bit layout because the thesis felt right, before either the WireNumber namespace or the constrained-carrier address problem had been worked through.

---

# 2. Candidate 1 — Deployment-Global Endpoint-Domain Identity

**Status: adopted in principle.** This is the strongest part of the proposal and it stands on evidence the sketch program produced independently.

## 2.1 ParticipantId

Each addressable Endpoint Domain receives one `ParticipantId` that is the same on every Wire in the deployment. If Participant 8 belongs to three Wires, it is Participant 8 on all three. This replaces per-Wire NodeId assignment.

Width is deliberately unspecified here; it belongs to candidate 3.

## 2.2 The addressable unit is the Endpoint Domain

The addressable Participant is an Endpoint Domain, not necessarily a physical device:

```text
Physical Gateway ABC
    Participant 10   Real-Time Control Domain
    Participant 11   Field I/O Domain
    Participant 12   Host/Diagnostics Domain
```

A larger stable device identity may group these three in tooling. This resolves "which instance of the Link Telemetry Service belongs to Core1?" by identity rather than by adding a Service-instance addressing mechanism.

## 2.3 Global semantic identity and carrier-local compact address

**This is the most important correction to revision 1, and it is a new concept rather than a repair.**

Revision 1 assumed the deployment-global identity would be encoded directly by every carrier. Combined with an 11-bit CAN profile representing Participants in five bits, that produced a cap of roughly 31 CAN-visible Endpoint Domains **for an entire deployment** rather than per bus — because global uniqueness forbids reusing the low range on a second CAN link. Three independent CAN buses of fifteen nodes each would already exceed it. The current per-Wire NodeId scheme has no such cap, since NodeIds are per-Wire and freely reused.

The resolution is to separate two things revision 1 conflated:

```text
canonical / deployment
    ParticipantId        stable Endpoint-Domain identity, deployment-global

Link profile
    ParticipantLocalId   compact carrier-local address, where the carrier needs one

Organizer
    owns the deterministic projection between them
```

CAN-11 then addresses 31 Participants **per bus** rather than 31 per system, without shrinking the canonical model.

**This is mapping machinery, and the proposal should not claim otherwise.** Revision 1 listed WireAlias removal among its simplifications; a carrier-local participant address re-admits a projection table. The honest claim is that the projection moves to a better layer:

| | Revision 1 removed | This revision adds |
|---|---|---|
| Concept | `WireAlias` — local alias for a *logical Wire* | `ParticipantLocalId` — local projection of a *globally identified Domain* |
| Layer | Semantic topology | Carrier compression |
| Justification | Existed because CAN-11 could not carry WireNumber | Exists because CAN-11 cannot carry a global identity |

Compact local addressing under a stable global identity is ordinary practice in constrained networks, and it is the correct layer for the compression. But the net reduction in configuration machinery is smaller than revision 1 claimed, and any adoption argument should be made on that basis.

Two consequences to carry into candidate 3:

- `PDU-2` forbids implicit aliasing and silent remapping. An Organizer-generated, statically checkable projection is not implicit aliasing, but the distinction must be stated normatively, not assumed.
- A gateway applying the projection changes the *representation* of Participant identity at a Link boundary. The architecture already has vocabulary for exactly this — a splice changes the Wire representation without changing identity (`CORE §6`). The participant projection should be described in the same terms rather than as a new kind of translation.

## 2.4 Stable identity vs deployment identity

`ParticipantId` is unique within one deployed Wiring namespace and is not a manufacturing identity. Commissioning maintains a mapping from a permanent device identity to the deployment `ParticipantId`.

`SF-009` records an unresolved conflict here: `DEPLOY §1.7` presents a 128-bit UUID as stable device identity while `docs/proposed/can_id_provisioning.md` makes a 64-bit `DeviceId` canonical. Candidate 1 does not resolve that, and now has a third layer beneath it. The full stack is:

```text
permanent device identity      (SF-009 — unresolved)
        ↓  commissioning
deployment ParticipantId       (candidate 1)
        ↓  Organizer projection
carrier-local address          (§2.3, candidate 3)
```

## 2.5 What this resolves

Five open items close or shrink, four of them from findings the sketch program produced without reference to this proposal:

| Item | Effect |
|---|---|
| `SF-018` | Per-Domain Link Telemetry addressing resolved by construction |
| `SF-019` | Multi-Domain same-type Service instances get distinct identity by construction; supersedes the provisional per-Domain NodeId convention |
| `SF-005` | CAN alias ceiling disappears with `WireAlias` |
| `SF-008` | "Which overlapping full-membership Wire owns alias 0" dissolves with `WireAlias` |
| `TF-005` | Organizer alias-count validation no longer needed |

There is a sixth benefit revision 1 did not claim. `CORE §6` notes that when several devices each splice a device-private Wire onto one shared external Wire, their internal NodeId assignments must be coordinated because they become Nodes on the same logical Wire — and recommends a separate external Wire per device during bring-up to avoid it. Global Endpoint-Domain identity makes those identities unique by construction and removes the coordination entirely. Sketch `05` is where this should be visible.

## 2.6 Costs

- The projection machinery of §2.3, which is real and must be specified.
- An identity ceiling that is consumed by Endpoint Domains rather than devices, so a large distributed system consumes it faster than a device count suggests. Width is a candidate 3 question, but the *consumption rate* is a candidate 1 property and should be measured against the sketch suite.
- `CFG-10` refers to detecting mismatched "routing, Endpoint, and alias configuration." That wording needs revisiting when aliases go.

---

# 3. Candidate 2 — Origin as a Per-Interaction Role

**Status: under sketch evaluation.** See `sketches/remap_brief_multi_origin.md`.

## 3.1 The change

A Wire may permit one, several, or all of its members to act as Origin. A PDU carries the actual `ParticipantId` of the Origin anchoring that interaction. There is no Origin index table:

```text
not this:                        this:
    Wire:                            OriginId = 41
        Origin[0] -> Participant 41
        Origin[1] -> Participant 7
```

`OriginId` and `ParticipantId` share one identity namespace. This replaces `WIRE-1`.

`WIRE-2` — Origin is a per-Wire role, not a device class — is not merely retained but strengthened: Origin becomes a per-*interaction* role.

## 3.2 Direction is unchanged

Revision 1 drifted into presenting Direction as request/reply structure. That was a mistake and is corrected here.

Direction means exactly what `WIRE-5` says it means:

> which side of the Origin/Participant relation produced this PDU

It is never the interaction pattern — not request/response, client/server, or command/status. Request/reply is one *shape* that can be expressed with Direction, alongside autonomous publication, event reporting, and polled state.

`DISP-7` continues to hold without qualification: **reply authority is never inferred by reversing Direction.** A response is legal because an Endpoint registration with an appropriate binding mode authorizes it. Nothing about naming the Origin explicitly changes that, and it is important that the wording of this proposal not let the drift reach an API.

## 3.3 Wire semantics

A Wire defines membership, broadcast scope, static routing and forwarding scope, and optionally which members may originate. It is not primarily an address namespace.

Wires remain valuable under multi-Origin because broadcast should generally not mean *every Participant in the deployment*:

```text
Wire Power        Battery, HV Controller, DC/DC, Charger
Wire HVAC         HVAC Controller, Compressor, Blower, Cabin Sensors
```

A Power broadcast stays on the Power Wire (`WIRE-4`, unchanged). The separation of concerns:

```text
ParticipantId       who
EndpointId          what
WireNumber          which routing / broadcast scope
OriginId + Direction  who anchors this interaction and which way it flows
```

The honest statement of what changes is that a Wire's three current jobs — route scope, broadcast scope, and command authority — arrive bundled under single-Origin and become unbundled. Whether that is a simplification depends on whether the authority job shrinks or merely relocates, which is what §4.4 and §4.5 of the remap brief exist to measure.

## 3.4 Origin permissions

A Wire may restrict which members may originate:

```text
RobotPlant:   members = {1, 2, 8, 9, 10}   allowed origins = {1, 2}
Service:      members = {8, 9, 10, 42}     allowed origins = {42}
```

Representation is deployment configuration and remains open (§9). It is not an Origin-index mapping.

Open question 4 of revision 1 — whether a per-Wire allow-set is required in the base model — is now the central design question of this candidate rather than a footnote, because the allow-set is where the displaced authority lands.

## 3.5 Peer interaction and hierarchical systems

A four-peer CAN system becomes one Wire with four Origin-capable members, using no artificial coordinator, no observation trick for commands, no authority Wire per producer, and no separate peer-addressing extension. This addresses the **Significant** friction recorded in `SF-014` and `SF-001`.

A conventional coordinator system is expressed by permitting exactly one Origin. The hierarchical model becomes a common usage pattern rather than a protocol restriction, and nothing requires other Participants to initiate.

## 3.6 What this candidate does not solve

**Redundancy.** Revision 1 implied multi-Origin meaningfully improved the redundant-gateway case. It does not. `SF-021` and `SF-022` found that the binding constraint is exclusive physical bus ownership: losing a gateway loses its buses regardless of who is permitted to originate. Multi-Origin makes multiple command authorities easier to *represent*; it provides no coverage. `SVC-1` — Origin failover and election are not base Wire features — is unaffected, and `CORE §23.11` composition remains where redundancy lives.

**Authorization.** Under single-Origin, "who may command on this Wire" is structural and checkable by construction. Multi-Origin converts it into explicit policy. `DISP-5` says authority comes from Wires, bindings, and typed local access; thinning Wires weakens one of the three. Being able to encode `OriginId = 42` must not imply Participant 42 may invoke every Endpoint.

This is the largest cost in the proposal. Revision 1 treated it as a documentation matter in a costs section and deferred the mechanism to an open question. For a system whose stated value includes static verifiability, converting a structural guarantee into a policy table is an architectural change in its own right and needs a designated owner — plausibly `DEPLOY` plus an Organizer check, but that has not been decided.

**Degraded-state clarity.** `SF-012` rated "Wiring persists when Origin offline" a positive: a crisp degraded state with no role reassignment, and a tooling flag in `TF-004`. That crispness comes from there being exactly one Origin to be absent. What replaces it under multi-Origin is untested — see §5.1 of the remap brief.

## 3.7 The alternative not yet excluded

Multi-Origin could be a **per-Wire property carried in a header extension** rather than a field in every descriptor. `PDU-6` already supports self-describing extensions, and `LINK-8` already makes profile family static per Link, so a peer-oriented CAN identifier layout could coexist with a hierarchical one.

Arguments for the fixed-field form, which is this proposal's preference:

- one canonical descriptor rather than two shapes;
- one CAN layout rather than two;
- no extension parsing on the peer path;
- the canonical model stays uniform, which is the point of the change.

Argument against: it pays an Origin field in every PDU forever, in a deployment population that appears to be predominantly hierarchical. In the single-coordinator case that field is a constant repeated in every PDU — something the current model gets free from the Wire.

The comparison should not be settled by preference. §4.6 of the remap brief collects the deciding datum cheaply: how many Wires, across all sketches, actually need more than one Origin. If the answer is "few," the extension form gets substantially stronger.

---

# 4. Candidate 3 — Encoding: Withdrawn and Deferred

**Status: withdrawn.** Revision 1's 48-bit descriptor, 7-bit WireNumber, and CAN-11 projection are not part of this revision. They should be re-derived after candidate 2 is decided, because a semantic model that is still moving cannot fix a bit layout.

What follows is not a design. It is the constraint list any future encoding proposal must satisfy, recorded so the same errors are not repeated.

## 4.1 The WireNumber namespace must be re-derived, not resized

Revision 1 proposed 7 bits on the reasoning that separate authorities no longer require separate Wires. That reasoning considered only network-visible Wires. `CORE §4.2`–`§4.3` allocate the current 10-bit space as:

```text
0..895        network-visible Wires
896..1022     device-private Wires (127 values)
1023          kLocalDomain
```

The two reserved regions consume exactly 128 values. A 7-bit WireNumber leaves **zero** network-visible Wires. `CORE §13.5` treats 127 device-private Wires as a considered sizing decision for multicore MCUs and FPGAs — the same multicore case this proposal wants to improve.

The right response is not to pick a bigger number. It is to redesign the namespace deliberately, since the meaning of Wire has changed:

```text
network-visible Wires    now route/broadcast scopes rather than authority scopes
device-private Wires     unchanged in meaning; count should be re-justified
kLocalDomain             sentinel, unchanged
```

Until that redesign is done, assume the current 10-bit allocation.

## 4.2 Carrier-local participant representation must be solved first

See §2.3. No canonical width choice is meaningful until it is settled whether constrained carriers encode the global identity directly or project it.

## 4.3 Commissioning arbitration behavior must be preserved

`docs/proposed/can_id_provisioning.md` orders the CAN identifier `QoS | NodeId | WireAlias | Direction` specifically so that within a QoS class, any ordinary NodeId wins arbitration over Broadcast regardless of the other fields — which places commissioning traffic below all ordinary Background traffic.

Revision 1's `QoS | OriginId | Direction | ParticipantId` destroys that property: a broadcast anchored to Origin 0 would outrank ordinary unicast anchored to Origin 1 in the same class. Any future layout should keep the participant field immediately below QoS, along the lines of:

```text
QoS
ParticipantLocalId
OriginLocalId
Direction
```

Two further requirements, neither addressed in revision 1:

- **A normative constant for the Origin field during commissioning.** An unconfigured node has no identity, and the anonymous binary search depends on every responder emitting a bit-identical `DLC = 0` frame. "Whatever value" is not acceptable; the constant must be specified.
- **Replacement for the commissioning subchannel space.** The provisioning note earmarks the `WireAlias` bits for commissioning message subtypes. Removing `WireAlias` removes that space; something must take it.

Note also that revision 1 implicitly resolved `SF-007` — the `NodeId 31` versus `NodeId 0` broadcast contradiction between `DEPLOY §1.6`, `LINK §2.2`, and the provisioning note — in favor of the provisioning note. That resolution may well be right, but it should be made explicitly through `REG` rather than as a side effect of a header proposal.

## 4.4 Byte alignment per `BITS`

Revision 1's `WireNumber(7) | OriginId(8) | Direction(1) | ParticipantId(8)` straddles two of three fields across byte boundaries, against the alignment preference `BITS §1` states and `BITS §3` reasoned through for the current RoutingWord. Swapping two fields fixes it at no cost:

```text
WireNumber      7
Direction       1
OriginId        8
ParticipantId   8
```

Both identity fields become whole bytes. This is recorded as a constraint, not as an endorsement of the 24-bit routing word or the 48-bit descriptor.

## 4.5 Descriptor size is a consequence, not a premise

Carrying two explicit identities plus Direction plus WireNumber does not fit the current 16-bit RoutingWord, so candidate 2 does force the descriptor to grow. That cost lands on shared-memory and byte-stream Links, since Classical CAN never carries the canonical descriptor. Sketch `05` is where per-PDU overhead on inter-core FIFOs would show up, and it should be assessed there rather than asserted here.

---

# 5. Forwarding, Splicing, and Identity Boundaries

Revision 1's proposed invariant — "forwarding preserves WireNumber, OriginId, ParticipantId, Direction, and Endpoint identity" — accidentally outlawed Wire splicing, which exists precisely to rewrite the Wire representation before egress (`SPLICE-1`, `CORE §6`) and is the only sanctioned route by which a device-private Wire reaches a Link. Corrected:

```text
ordinary forwarding
    preserves canonical routing identity across Link Interfaces

splice
    the sanctioned, explicit operation that rewrites the Wire
    representation before egress; the scope check runs on the
    post-splice representation

carrier projection (§2.3)
    the sanctioned, Organizer-generated operation that rewrites the
    participant representation at a Link boundary, without changing
    identity
```

Splicing and projection are the two identity-boundary operations. Both are explicit, both are statically generated, and neither is inference. `SPLICE-2` (at most one splice per local routing step) and `SPLICE-3` (an anonymous LocalBus is not spliceable) are unaffected.

`SPLICE-4` needs a successor. Its two clauses fare differently: "exactly one Origin across all segments" falls with `WIRE-1`, while "unique NodeIds across all segments" becomes true by construction under candidate 1 (§2.5).

---

# 6. Relationship to Existing Invariants

| Invariant | Effect |
|---|---|
| `WIRE-1` exactly one Origin per Wire | **Replaced** by candidate 2 |
| `WIRE-2` Origin is a role, not a device class | **Retained and strengthened** — now per-interaction |
| `WIRE-3` NodeId 0 broadcast semantics | **Successor needed**; entangled with `SF-007` and §4.3 |
| `WIRE-4` broadcast reaches one Wire's members | Retained |
| `WIRE-5` Direction is structural | **Retained in full** — see §3.2 |
| `SPLICE-1`–`SPLICE-3` | Retained; see §5 |
| `SPLICE-4` | **Successor needed** — see §5 |
| `DISP-5` naming vs authority | Retained, but one of its three authority sources weakens — see §3.6 |
| `DISP-7` no reply authority from Direction | **Retained in full** — see §3.2 |
| `PDU-2` no implicit aliasing | Retained; §2.3 must show the projection is explicit |
| `SVC-1` Origin failover is not a base Wire feature | Retained — see §3.6 |
| `CFG-10` compatibility check wording | Minor update needed when aliases go |
| `LINK-8` static profile family per Link | Retained; load-bearing for §3.7 |

Proposed successor invariants are deliberately **not** drafted in this revision. They should be written once candidate 2 is decided, and routed through `REG`.

---

# 7. What Revision 1 Got Wrong

Recorded so the reasoning is not repeated.

| Error | Cause |
|---|---|
| 7-bit WireNumber | Reinterpreted WireNumber as purely deployment-visible route identity and forgot the device-private range and `kLocalDomain` (§4.1) |
| CAN-11 deployment-wide participant cap | Assumed carriers encode the global identity directly; per-Wire NodeId reuse was silently lost (§2.3) |
| CAN identifier field order | Did not check the arbitration property the provisioning note deliberately designed for (§4.3) |
| Forwarding invariant outlawed splicing | Wrote "preserves WireNumber" without exempting the sanctioned rewrite (§5) |
| Direction presented as request/reply | Semantic drift against `WIRE-5` and `DISP-7` (§3.2) |
| §11 multi-hop example unrepresentable | Used Origin 42 across a CAN hop restricted to Origins 0–7 |
| Redundancy claim | Confused authority representation with physical coverage (§3.6) |
| Byte-straddling layout | Did not apply `BITS §1` (§4.4) |
| Three changes bundled | Prevented attributing any result to a single decision |

The common thread is that revision 1 chose an encoding to express a semantic model that had not been validated yet, and the encoding then had to be defended rather than derived.

---

# 8. Validation

The validation plan is now a separate document: `sketches/remap_brief_multi_origin.md`. It remaps sketches `01`–`06` under candidates 1 and 2 with candidate 3 fenced out, requires that every removed Wire be accounted for and every displaced authority written down, and pre-registers the adoption criteria.

The question it asks is deliberately not "does peer CAN improve" — it does — but whether the hierarchical majority gets **simpler or merely different**. `03_rs485_sensors` runs first as the negative control; `02_peer_can` runs last as the positive control.

Two standing recommendations argue for caution and are the reason a pre-registered rule exists: `DL-004` says add a peer-addressed profile only if multiple sketches demand it, and `SF-014` says do not add a peer primitive from one sketch. The counter-argument is that multi-Origin is not only a peer fix — it also bears on PC-plus-device authority on one bus, redundant controllers, the need for overlapping authority Wires, and the meaning of Origin itself. The remaps exist to decide which reading is correct.

---

# 9. Open Questions

Carried forward, re-scoped to the candidate they belong to.

**Candidate 1**

1. How does deployment `ParticipantId` relate to the permanent device identity, given the unresolved `SF-009` conflict between a 128-bit UUID and a 64-bit `DeviceId`?
2. Is the carrier-local projection a per-Link table, a per-Wire table, or a per-Link-profile function?
3. How is the projection made auditable, and which `CFG-12`-style coupled acceptance checks does Organizer run over it?
4. What consumption rate do Endpoint Domains actually show across the sketch suite?

**Candidate 2**

5. Is a per-Wire permitted-Origin allow-set required in the base deployment model, or is it optional policy? (§3.4 — now the central question of this candidate.)
6. Which layer validates that a particular Origin may invoke a particular Endpoint, and does Organizer check it statically?
7. May any permitted Origin broadcast on a Wire, or may Endpoint definitions restrict it further?
8. What replaces the crisp "the Origin is offline" degraded state and its `TF-004` tooling flag?
9. What guidance determines when to create a smaller Wire such as `Power` instead of using a larger `WholeMachine`, once authority is no longer the differentiator?
10. Fixed field or header extension (§3.7)?

**Candidate 3, all blocked on 1 and 2**

11. WireNumber namespace redesign: how many network-visible, how many device-private, and is `kLocalDomain` still a sentinel in that space?
12. Canonical broadcast representation.
13. CAN-11 identifier layout, preserving the §4.3 arbitration property.
14. Normative anonymous Origin constant and commissioning subchannel space.
15. Explicit resolution of `SF-007`, routed through `REG` rather than as a side effect.
16. Which canonical fields a future CAN-29 profile represents directly.

---

# 10. Positioning

Kept short deliberately. The full comparison against Cyphal and similar systems is a separate argument that does not need to ride along with an addressing change, and revision 1's version of it functioned as advocacy inside a document that should be making a technical case.

The relevant point for *this* proposal is narrow: WireSpaces should not depend on being awkward at peer networking in order to remain distinct. Its reason to exist is the embedded system model — explicit static routed communication scopes, heterogeneous forwarding as a primary goal rather than multi-transport support, shared-memory Domains under the same Link model, polled Links as first-class with semantic producer separated from transfer initiator, bounded Endpoint delivery as an architectural contract rather than an implementation concern, and useful operation on 11-bit Classical CAN.

Candidates 1 and 2 make the addressing layer less unusual. That is the intent, and it is only a loss if the differentiators above are not real. Testing whether they are real is what the sketch program is for.

---

# 11. Revision History

| Revision | Date | Change |
|---|---|---|
| 2 | 2026-08-22 | Split into three candidates; candidate 3 withdrawn; added carrier-local participant address (§2.3); corrected Direction framing, splice invariant, redundancy claim; added §7 error record |
| 1 | 2026-08-22 | Original bundled proposal: global Participants, multi-Origin Wires, 48-bit descriptor, 7-bit WireNumber, CAN-11 projection, WireAlias removal |

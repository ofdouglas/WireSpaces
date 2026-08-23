# Remap — Sketch 05 Multicore AMP industrial gateway

```text
**Remap of:** sketches/05_multicore_gateway.md
**Evaluator-normalized net effect:** Candidate 1 modest simplification; Candidate 2 neutral (no meaningful benefit; no regression).
**Wires:** 2 before → 2 after; 0 of 2 need more than one Origin.
**Authority:** structural (one Origin per Wire) before → 0 new authored policy rules; generated projection only.
**Attribution:** Global identity: P10/P11 stable, SF-018/019 cross-Domain case; Multi-Origin: neutral; Both: none.
**Worst friction (informative):** Identity awkwardness Mild → None (Candidate 1 only).
**Main lesson:** Null result for multi-Origin on hierarchical multicore; global identity and Candidate 1/2 are empirically separable here.
**Provenance:** Useful convergence, not pristine blinded replication — prior sketch exposure to authority-shaped Wires and forward-vs-compose.
```

**Experiment premises applied:** deployment-global `ParticipantId` (§2.1); Origin as per-interaction role with permitted-Origin policy (§2.3); carrier-local compact address projection assumed sufficient (§2.2) — not designed in this remap.

**Configs remapped:** Baseline only (single configuration in original).

---

## Unchanged from original (verbatim reuse)

### Native communication model

```text
One gateway MCU on a machine cell:

  Core0 (supervisor):
    - 100 Hz plant sequencer: reads sensors, commands drives and aux devices,
      exchanges setpoints with the downstream cell controller.
    - Ethernet to plant switch: SCADA (read-only) and service laptop (maintenance) as **separate hosts/sessions**.

  Core1 (fieldbus engine):
    - Owns PlantCAN, AuxCAN, SensorRS485 Link drivers.
    - Polls RS-485 sensors on a fixed schedule; Nodes do not initiate bus traffic.
    - Forwards canonical plant traffic between shared memory and the three local fieldbuses only.
    - Does NOT own CellLink — that Link Interface is on Core0.

  Downstream cell gateway (separate device):
    - Two Nodes on CellLink Ethernet — material handling handshake.

  Field devices:
    - DriveLeft / DriveRight (PlantCAN): ~500 Hz torque command / encoder feedback.
    - ValveCtrl / HydPump (AuxCAN): ~50 Hz commands, eventful status.
    - Temp ×2, pressure, flow (SensorRS485): Snapshots updated locally; bus master polls ~10 Hz.

  Plant SCADA (HMI / historian):
    - Configured observer on PlantNet (Node 22); receives canonical NodeToOrigin traffic
      the gateway forwards to EthPlant — read-only; no maintenance authority.

  Service laptop (bench / field service):
    - Configured Origin on Service Wire (Wiring static; participant may be offline — `SF-012`).
    - Reads health/logs; firmware update to any Node when online.

  Timing expectations (ASIL-A-ish notes, not a safety case):
    - Drive torque command freshness budget: ≤ 5 ms plant loop including Core0→Core1 hop.
    - RS-485 sensor delivery latency bounded by poll cadence (~100 ms), not plant loop.
    - Cross-core shared-memory hop: bounded queue + one context switch — budget ~0.1–0.5 ms.
    - Distinguish communication age (arrival) from measurement age (sample timestamp on sensors).
```

### Obvious conventional implementation

> **Obvious conventional implementation:** Core0 runs the plant application and uses **vendor-specific IPC** (shared-memory rings, `rpmsg`, or a custom mailbox API) to command Core1's CAN/RS-485 stacks. Core1 exposes a fixed struct-per-bus API ("write DriveLeft torque", "read sensor block"). Ethernet to SCADA uses another protocol stack (Modbus/TCP, OPC UA, or a proprietary plant protocol). Maintenance tools use a separate TCP port or UDS-on-Ethernet. Downstream cell is another socket or gateway-specific tunnel. Routing, Node addressing, and protocol translation live in **implicit gateway firmware tables** — one per bus, plus IPC framing.

> **What additional conceptual objects does WS introduce?** Named Wires with explicit Origin/Node roles spanning heterogeneous Links; **canonical PDUs** forwarded identically across shared memory, CAN, RS-485, and Ethernet; **poll projection** as Link configuration rather than a second Origin; per-Wire maintenance vs plant authority (`Service` vs `PlantNet`). The conventional design also needs routing — WS makes it **Wire-scoped and table-driven** instead of embedding it in per-bus APIs and IPC structs. Inter-core traffic is not a special case: it is one more Link Interface pair on the same PlantNet Wire.

### Devices

| Device | Endpoint Domains | Link interfaces | Role summary |
|---|---|---|---|
| Gateway MCU | Core0 supervisor; Core1 fieldbus | Core0: EthPlant, CellLink, SmCore1; Core1: SmCore0, PlantCAN, AuxCAN, SensorRS485 | PlantNet coordinator on Core0; per-Domain Router projections; RS-485 poll on Core1 |
| DriveLeft / DriveRight | 1 each | PlantCAN | Motion Participants P1–P2 |
| ValveCtrl / HydPump | 1 each | AuxCAN | Aux Participants P3–P4 |
| RS-485 sensors ×4 | 1 each | SensorRS485 | Polled Participants P5–P8 |
| Cell gateway | 1 | CellLink | Participant **P20** (one Domain; CellFeeder + CellConveyor Endpoints) |
| Plant SCADA | 1 | EthPlant | Configured observer on PlantNet — **membership TBD** (see below) |
| Service laptop | 1 | EthPlant | Service Wire maintenance authority P30 (Wiring static; may be offline) |

---

## Remapped WireSpaces mapping

### Participants

Deployment-global `ParticipantId` — same value on every Wire a Domain joins.

| ParticipantId | Endpoint Domain | Physical device | Notes |
|---|---|---|---|
| **P1** | DriveLeft | Field ECU | PlantCAN |
| **P2** | DriveRight | Field ECU | PlantCAN |
| **P3** | ValveCtrl | Field ECU | AuxCAN |
| **P4** | HydPump | Field ECU | AuxCAN |
| **P5** | Temp transmitter 1 | Sensor | SensorRS485 |
| **P6** | Temp transmitter 2 | Sensor | SensorRS485 |
| **P7** | Pressure transmitter | Sensor | SensorRS485 |
| **P8** | Flow transmitter | Sensor | SensorRS485 |
| **P10** | Core0 supervisor | Gateway MCU | Plant sequencer; EthPlant, CellLink, SmCore1 |
| **P11** | Core1 fieldbus | Gateway MCU | Link scheduler / forwarder only — **Wire membership TBD** (see below) |
| **P20** | Cell gateway | Downstream device | **One Domain** — CellFeeder and CellConveyor are distinct Endpoints, not separate Participants (`SF-028`) |
| **P30** | Service laptop | External | Service Wire maintenance authority |
| **P31** | Plant SCADA | External | PlantNet configured observer — **membership TBD** (see below) |

**SF-018 / SF-019 (cross-Domain case):** Global identity **resolves by construction** for P10 vs P11 on Service Wire — no wire-local NodeIds 9/10. Multi-Origin did not contribute.

**SF-028 (same-Domain case):** Original sketch used Wire-local Nodes 20 and 21 for two cell functions in **one** Cell gateway Domain. Under §2.1 (one Domain = one `ParticipantId`), remap uses **P20 only**; CellFeeder vs CellConveyor distinction moves to **Endpoints**. If those identities previously carried separate broadcast membership, routing, or failure semantics, that expressive capability is lost or must be expressed in Endpoint/binding policy — open architecture question.

Carrier-local compact addresses on each bus are projected by Organizer (§2.2) — not designed here.

### Membership ambiguities (remap brief not fully pinned — do not score as proposal evidence)

| Question | This remap's choice | Alternate valid choice |
|---|---|---|
| Is **P11** (forwarding Domain) a PlantNet/Service **member**? | **Yes** — listed in Wire tables (matches original Node 9/10 gateway presence) | **No** — P11 only forwards; not a semantic Wire participant |
| Is **P31** (SCADA observer) a PlantNet **member**? | **Yes** — listed for observe-set routing | **No** — explicit non-member observer only (`CORE §12.6`) |

Neither choice changes the headline null result for multi-Origin.

### Wires

| Wire | Origins needed | System role | Members (ParticipantIds) | Physical realization | Notes |
|---|---:|---|---|---|---|
| **PlantNet** | **1** | **primary** | P1–P8, P10, P11?, P20, P31? | SmCore1↔SmCore0, PlantCAN, AuxCAN, SensorRS485, CellLink, EthPlant (observe) | P10 only permitted Origin |
| **Service** | **1** | **supporting** | P1–P8, P10, P11?, P20, P30 | EthPlant + Sm + fieldbuses via per-Domain forward | P30 only permitted Origin |

```text
Wires after remap: 2
Wires requiring >1 Origin: 0
Multi-Origin Wires by role: none (primary=1 Origin, supporting=1 Origin)
```

**Why two Wires remain:** Plant control (P10) and maintenance (P30) are distinct authority planes with different failure meaning, Service binding scope, and cross-Wire compose rules (`SF-004`). Collapsing to one multi-Origin Wire would move that boundary into authored Endpoint restrictions without reducing route or observation complexity.

### Interactions (revised — Origin per interaction)

Direction uses `OriginToParticipant` / `ParticipantToOrigin` (structural per `WIRE-5`). **Origin** column names the Participant anchoring each interaction.

| Interaction | Origin | Producer | Participant (sink) | Wire | Direction | Notes |
|---|---|---|---|---|---|---|
| Drive torque command | **P10** | P10 plant sequencer | P1, P2 | PlantNet | OriginToParticipant | P10 → SmCore1 → Sm → PlantCAN; ≤5 ms budget |
| Drive encoder feedback | **P10** | P1, P2 | P10 sequencer | PlantNet | ParticipantToOrigin | Anchored to plant interaction Origin P10 |
| Aux valve command | **P10** | P10 sequencer | P3 | PlantNet | OriginToParticipant | Sm hop; AuxCAN on P11 |
| Sensor reading | **P10** | P5–P8 | P10 sequencer | PlantNet | ParticipantToOrigin | P11 poll projection; P10 remains interaction Origin (`SF-003`) |
| Cell handshake | **P10** | P10 sequencer | P20 (CellFeeder Ep) | PlantNet | OriginToParticipant | Direct CellLink on P10 Domain |
| Cell telemetry | **P10** | P20 (CellFeeder/Conveyor Eps) | P10 sequencer | PlantNet | ParticipantToOrigin | Same Participant P20; Endpoint distinguishes role |
| SCADA telemetry view | **P10** | P1–P8, P20 | P31 (observe) | PlantNet | ParticipantToOrigin (observe) | P31 membership ambiguous — see above |
| Parameter read (service) | **P30** | P30 laptop | P3 | Service | OriginToParticipant | EthPlant → Sm → AuxCAN; same-Wire forward (`SF-013`) |
| FW update segment | **P30** | P30 laptop | P1 | Service | OriginToParticipant | Reliable segment; forward across Sm + CAN |
| Link Telemetry (P10 Domain) | **P30** | P10 | P30 | Service | ParticipantToOrigin | Telemetry publish toward maintenance session Origin |
| Link Telemetry (P11 Domain) | **P30** | P11 | P30 | Service | ParticipantToOrigin | Same — P11 Domain network face |
| Plant event log tap | — | PlantNet ingress | P10 logger Queue | PlantNet | observe tap | Local tap — not a Wire interaction (`SF-016`) |

**Forward vs compose unchanged:** Cross-fieldbus sequencing on P10 remains application composition on PlantNet. Service maintenance does not forward to PlantNet (`SF-004`, `SF-015`).

**Reply authority (`DISP-7`):** Encoder feedback and Link Telemetry replies are authorized by existing Service/Endpoint bindings — not by reversing Direction alone.

### Gateway forwarding — per Endpoint Domain

**Forwarding entry changes:** **None structurally.** Same per-Domain Router projections, same Link Interface hops, same SmCore1↔SmCore0 path. Only the routed identity field changes from wire-local NodeId to global `ParticipantId` (and carrier-local projection at LLL egress).

#### P10 Domain (Core0) Router — PlantNet

| Ingress | Egress / delivery | Notes |
|---|---|---|
| P10 plant Services (local) | `CellLink` for P20 | Unchanged |
| P10 plant Services (local) | `SmCore1` for P1–P8 | Unchanged |
| `SmCore1` | local delivery to P10 plant Services | Unchanged |
| `CellLink` | local delivery | Unchanged |
| `SmCore1` / `CellLink` | `EthPlant` | Observe set toward P31 |

#### P10 Domain — Service

| Ingress | Egress / delivery | Notes |
|---|---|---|
| `EthPlant` | local delivery (P10 maintenance Endpoints) | Unchanged |
| `EthPlant` | `SmCore1` for P1–P8 | Unchanged |
| `EthPlant` | `CellLink` for P20 | Unchanged |
| `SmCore1` / `CellLink` | `EthPlant` | Unchanged |

#### P11 Domain (Core1) Router — PlantNet / Service

| Ingress | Egress / delivery | Notes |
|---|---|---|
| `SmCore0` | PlantCAN / AuxCAN / SensorRS485 per ParticipantId | Unchanged — **no CellLink** |
| fieldbus | `SmCore0` | Unchanged |

P11 Domain does not terminate EthPlant or CellLink — unchanged from original.

### Poll projection — unchanged semantics

```text
PlantNet on SensorRS485 (P11 LLL):
  Participants P5..P8
  poll Snapshot TX Endpoints at 10 Hz
  emit ParticipantToOrigin toward P10 via SmCore0

Semantic PDU:
  Wire: PlantNet
  Origin: P10
  Participant: P7
  Direction: ParticipantToOrigin
```

P11 initiates the physical transfer; P10 remains interaction Origin (`SF-003`).

### Shared-memory / splice (§5.2)

Original optional device-private Wire + splice for debug (`CORE §4.2`, `DEPLOY §3.2`) remains available.

| Topic | Remap effect | Attribution |
|---|---|---|
| Cross-device splice NodeId coordination | **Removed** if multiple devices splice onto one external Wire — ParticipantIds unique by construction | **Global identity** |
| Carrier-local address on each bus | **Still required** — Organizer projection per Link profile | **Global identity** (not elimination) |
| Recommended bring-up pattern | Unchanged for plant path; splice coordination step may simplify | **Global identity** |

Multi-Origin does not affect splice accounting.

---

## Wire accounting (§4.4)

**Wires removed:** 0

| Removed Wire | Route scope | Broadcast scope | Command authority | Failure distinction |
|---|---|---|---|---|
| — | — | — | — | — |

No Wire was removed. PlantNet and Service retain independent justification:

| Wire | Route scope | Broadcast scope | Command authority | Failure distinction |
|---|---|---|---|---|
| PlantNet | Plant traffic across Sm + 4 fieldbuses + EthPlant observe | Plant members P1–P8, P10, P20 (+ P11?, P31? TBD) | P10 plant coordinator | P10 unavailable → plant control degraded |
| Service | Maintenance across EthPlant + Sm + fieldbuses | Maintenance session members | P30 maintenance authority | P30 offline → maintenance unavailable; plant may continue (`SF-012`) |

---

## Authority policy (§4.5)

| Wire | Members | Permitted Origins | Endpoint-level restrictions | Authored or generated? |
|---|---|---|---|---|
| PlantNet | P1–P8, P10, P11?, P20, P31? | **P10 only** | P31: observation only; P11: forward/schedule only; P20: two Endpoints under one Participant | **Generated** from "plant coordinator = P10" |
| Service | P1–P8, P10, P11?, P20, P30 | **P30 only** | P10/P11: maintenance + telemetry Endpoints | **Generated** from "maintenance authority = P30" |

> **Is this policy smaller, simpler, or easier to audit than the Wire structure it replaced?**

**Equivalent.** Two single-Origin policies map 1:1 to the original two single-Origin Wires. No new authorization state — the high-level plant vs maintenance declarations already required.

> **Could this policy be generated from Service bindings and topology already required?**

**Yes.** Zero new authored policy rules. Flattened permitted-Origin rows are Organizer output, not designer-authored matrices.

**Authority distinction formerly carried by topology:** Plant vs maintenance separation remains on **separate Wires** — not moved into Service ad-hoc checks.

---

## Change attribution (§4.6)

| Change | Attribution |
|---|---|
| P10/P11 stable across PlantNet and Service; no NodeIds 9/10 | **Global identity** |
| Field Participants P1–P8, P20 same identity on both Wires | **Global identity** |
| SF-018 cross-Domain telemetry (P10 vs P11) resolved | **Global identity** |
| P20 collapse (Nodes 20/21 → one Participant, two Endpoints) | **Global identity** — possible expressive cost (`SF-028`) |
| Two Wires retained (no collapse) | **Neither** — same authority boundary as original |
| Origin named per interaction in PDU headers | **Multi-Origin** (terminology only; single permitted Origin per Wire) |
| Permitted-Origin policy rows (generated) | **Multi-Origin** (machinery) — but equivalent to structural single-Origin |
| Per-Domain forwarding unchanged | **Neither** |
| Poll projection / P11 not Origin | **Neither** — same as `SF-003` |
| Cross-Wire compose vs forward rules | **Neither** |
| Splice NodeId coordination simplified | **Global identity** |

**Multi-Origin did not independently justify any Wire removal or policy simplification** on this sketch.

---

## Observation accounting (§5.4)

| Original path | Remap | Transmission count | Recipient scope | Authorization | Failure meaning |
|---|---|---|---|---|---|
| SCADA P31 observes PlantNet traffic on EthPlant | **Retained** — configured observation; P31 not permitted Origin | Unchanged (no duplicate TX) | Same observe set | Explicit observe config on P31 | P31 offline ≠ plant failure |
| Plant event log tap on PlantNet ingress (P10) | **Retained** — local tap | Unchanged | P10 logger only | Tap config | Unchanged |
| Link Telemetry via Service to P30 | **Retained** — direct Service interaction | Unchanged | P30 as maintenance reader | Service binding | P30 offline → telemetry path unavailable |

No observation path removed or replaced by multi-Origin addressing. Direct multi-Origin interaction did not substitute for observation on this sketch.

---

## Friction re-rating (§4.8)

| Signal | Before | After | Notes |
|---|---|---|---|
| **Artificial Origin** | None | None | No change (tautological under multi-Origin premises — not cited as evidence) |
| **Artificial Wire** | None | None | Two Wires still justified |
| **Wire proliferation** | None | None | 2 → 2 |
| **Forwarding tax** | Mild / None | Mild / None | Unchanged |
| **Identity awkwardness** | Mild | **None** | P10/P11 global; SF-019 workaround gone |
| **Interaction awkwardness** | None | None | Unchanged |
| **Configuration burden** | Mild | Mild | Same forwarding + poll projections; identity config simpler |
| **Role instability** | None | None | Single permitted Origin per Wire; per-interaction Origin field does not add runtime role confusion here |
| **Failure mismatch** | None | None | Degraded states remain crisp — see below |

**Trade rule:** One informative improvement (identity awkwardness, Candidate 1 only); no informative regression → **Candidate 1 modest; Candidate 2 neutral.**

### Attempted one-Wire collapse (documented — not chosen)

Tempting collapse: `Machine` Wire with P10 and P30 both permitted Origins. Wire accounting shows complexity returns as: (a) plant vs maintenance Endpoint restrictions, (b) separate failure semantics for P10 vs P30 offline, (c) cross-Wire compose rules still needed for any plant↔maintenance bridge. **No net simplification** — confirms two-Wire retention.

### Coordinator absence / degraded state (§5.1)

Original (`SF-012`): Service laptop Origin offline — Wiring persists; plant continues.

Remap equivalent:

```text
PlantNet Wire exists
permitted plant Origin = P10
P10 available → plant control active

Service Wire exists
permitted maintenance Origin = P30
P30 offline → maintenance authority unavailable; plant unaffected
```

**Clearer than original?** **Equivalent** — failure distinction was already Wire-scoped; multi-Origin makes the per-authority statement explicit without losing crispness. No role reassignment. P1–P8 may continue `ParticipantToOrigin` toward P10 if plant loop runs.

---

## Changed / unchanged summary

**Changed:**
- Wire-local NodeIds → deployment-global `ParticipantId`
- Direction labels → `OriginToParticipant` / `ParticipantToOrigin`
- PDU names actual interaction Origin on every plant/service transaction
- P10/P11 addressable on Service Wire without wire-local 9/10 hack
- Permitted-Origin policy rows generated (equivalent to prior structural authority)

**Unchanged:**
- Two Wires (PlantNet + Service)
- Single plant coordinator (P10) and single maintenance authority (P30)
- Per-Domain Router projections and SmCore hop
- Poll projection on P11; P11 not interaction Origin
- Forward vs compose boundaries
- Observation paths (SCADA observe, log tap)
- Four heterogeneous fieldbuses on one PlantNet
- ASIL-A-ish timing notes

---

## Open questions

- Does global `ParticipantId` require splitting Domains when two network-visible identities (original Nodes 20/21) had distinct membership or failure semantics (`SF-028`)?
- Are forwarding Domains (P11) Wire members, or transport-only?
- Are configured observers (P31) Wire members, or explicitly non-members?
- If plant and maintenance were collapsed onto one multi-Origin Wire, what Endpoint restrictions would be **new authored state**?

---

## Spec findings [remap]

| ID | Topic |
|---|---|
| SF-019 | Cross-Domain case resolved (P10/P11); same-Domain multi-instance semantics **open** (`SF-028`) |
| [SF-024](synthesis.md#spec-findings-log) | [remap] Multi-Origin does not collapse plant/maintenance Wires |
| [SF-025](synthesis.md#spec-findings-log) | [remap] Candidate 1/2 separable on multicore gateway |
| [SF-028](synthesis.md#spec-findings-log) | [remap] Multiple network identities inside one Endpoint Domain |

---

## End-of-remap summary (§9)

```text
Sketch: 05_multicore_gateway
Wires before: 2
Wires after: 2
Wires requiring >1 Origin: 0
Multi-Origin Wires by role: none

Informative friction improved: Identity awkwardness (Mild → None)
Informative friction regressed: none
Informative friction unchanged: Wire proliferation, Configuration burden,
  Interaction awkwardness, Forwarding tax, Failure mismatch, Artificial Wire

Benefits attributable to Global identity:
  Stable P10/P11; SF-018 cross-Domain case; splice coordination may simplify;
  P20/21 collapse to one Participant (with SF-028 caveat)
Benefits attributable to Multi-Origin: none (neutral)
Benefits attributable to Both / inseparable: none

New authored authorization state introduced: 0
Existing structural authority removed: 0 (re-expressed as generated policy)
Observation paths removed / retained / changed: all retained unchanged
```

---

## References

| Doc | Sections used |
|---|---|
| `remap_brief_multi_origin_revised.md` | Full remap method |
| `sketches/05_multicore_gateway.md` | Source sketch (unchanged) |
| `CORE` (current) | §4.2 splice, §6, §12, §13 — forwarding/splice unchanged per §2.7 |

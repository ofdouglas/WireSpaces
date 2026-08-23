# Remap 05 — Multicore AMP industrial gateway under global identity and multi-Origin Wires

**Remap of:** `sketches/05_multicore_gateway.md`  
**Net effect:** Different — global Endpoint-Domain identity resolves the per-Domain telemetry addressing question, but the two authority- and policy-shaped Wires remain natural, so multi-Origin does not reduce topology.  
**Wires:** 2 before -> 2 after; 0 of 2 need more than one Origin; multi-Origin roles: none.  
**Authority:** structural before -> 0 new authored policy rules after; generated projection: 2 rows, one per retained Wire.  
**Attribution:** Global identity: stable cross-Wire Domain identity and removal of per-Wire NodeId allocation; Multi-Origin: no material benefit; Both: none.  
**Worst friction:** Configuration burden — Mild, unchanged.  
**Main lesson:** In this gateway, the plant and maintenance Wires remain separate because they carry different authority, broadcast, observation, and degraded-state meanings; only the identity model materially improves.

**Status:** Phase 2 experimental remap; not a specification or adopted architecture change.

---

## 1. Configuration

### Baseline — Dual-core gateway, four fieldbuses

The physical system, native behavior, maturity level, and conventional implementation are unchanged.

**Maturity level:** **Level 2–3**. Static Wiring, participant identities, WireNumbers, forwarding tables, poll projections, and per-Link capacity checks are authored or Organizer-generated and exported. No commissioning narrative.

The remap applies only these candidate premises:

- each Endpoint Domain has one deployment-global `ParticipantId`;
- Wire membership no longer allocates a per-Wire NodeId;
- Origin is named per interaction and constrained by a permitted-Origin policy;
- configured observers are identified separately and are excluded from the Wire's `Members` column.

---

## 2. Native communication model — reused verbatim

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

The natural model still has one plant authority, one maintenance authority, one read-only SCADA observer, and one fieldbus execution engine. The candidate architecture does not change those roles.

---

## 3. Obvious conventional implementation — reused verbatim

> **Obvious conventional implementation:** Core0 runs the plant application and uses **vendor-specific IPC** (shared-memory rings, `rpmsg`, or a custom mailbox API) to command Core1's CAN/RS-485 stacks. Core1 exposes a fixed struct-per-bus API ("write DriveLeft torque", "read sensor block"). Ethernet to SCADA uses another protocol stack (Modbus/TCP, OPC UA, or a proprietary plant protocol). Maintenance tools use a separate TCP port or UDS-on-Ethernet. Downstream cell is another socket or gateway-specific tunnel. Routing, Node addressing, and protocol translation live in **implicit gateway firmware tables** — one per bus, plus IPC framing.

> **What additional conceptual objects does WS introduce?** Named Wires with explicit Origin/Node roles spanning heterogeneous Links; **canonical PDUs** forwarded identically across shared memory, CAN, RS-485, and Ethernet; **poll projection** as Link configuration rather than a second Origin; per-Wire maintenance vs plant authority (`Service` vs `PlantNet`). The conventional design also needs routing — WS makes it **Wire-scoped and table-driven** instead of embedding it in per-bus APIs and IPC structs. Inter-core traffic is not a special case: it is one more Link Interface pair on the same PlantNet Wire.

In the remap, the references above to Node addressing describe the original baseline. Their candidate equivalent is global `ParticipantId` plus Organizer-generated carrier-local projections.

---

## 4. Baseline device list — reused verbatim

The old role labels are intentionally retained in this verbatim source table. The candidate participant mapping follows it.

| Device | Endpoint Domains | Link interfaces | Role summary |
|---|---|---|---|
| Gateway MCU | Core0 supervisor; Core1 fieldbus | Core0: EthPlant, CellLink, SmCore1; Core1: SmCore0, PlantCAN, AuxCAN, SensorRS485 | PlantNet Origin on Core0; per-Domain Router projections; RS-485 poll on Core1 |
| DriveLeft / DriveRight | 1 each | PlantCAN | Motion Nodes 1–2 |
| ValveCtrl / HydPump | 1 each | AuxCAN | Aux Nodes 3–4 |
| RS-485 sensors ×4 | 1 each | SensorRS485 | Polled Nodes 5–8 |
| Cell gateway | 1 | CellLink | Nodes 20–21 |
| Plant SCADA | 1 | EthPlant | PlantNet Node 22 — configured observer |
| Service laptop | 1 | EthPlant | Service Wire Origin (Wiring static; participant may be offline) |

---

## 5. Participants

The numeric values are illustrative stable deployment identities. Existing one-to-one NodeId values are retained where convenient, but they are no longer scoped to a Wire.

| ParticipantId | Endpoint Domain | Device | Relevant role |
|---:|---|---|---|
| 1 | DriveLeft Domain | DriveLeft | PlantCAN actuator and telemetry producer |
| 2 | DriveRight Domain | DriveRight | PlantCAN actuator and telemetry producer |
| 3 | ValveCtrl Domain | ValveCtrl | AuxCAN actuator and status producer |
| 4 | HydPump Domain | HydPump | AuxCAN actuator and status producer |
| 5 | Temp A Domain | Temp sensor A | Polled SensorRS485 producer |
| 6 | Temp B Domain | Temp sensor B | Polled SensorRS485 producer |
| 7 | Pressure Domain | Pressure sensor | Polled SensorRS485 producer |
| 8 | Flow Domain | Flow sensor | Polled SensorRS485 producer |
| 9 | Core0 supervisor Domain | Gateway MCU | Plant authority; maintenance and Link Telemetry Endpoints |
| 10 | Core1 fieldbus Domain | Gateway MCU | Link executor; Domain-local Link Telemetry Endpoints |
| 20 | Cell control Domain | Downstream cell gateway | Existing CellFeeder and CellConveyor Endpoint set |
| 22 | SCADA Domain | Plant SCADA | Configured PlantNet observer; not a Wire member for observation |
| 30 | Service-tool Domain | Service laptop | Maintenance authority |

The original CellLink Nodes 20 and 21 were hosted by one Endpoint Domain. Under global Endpoint-Domain identity they become one Participant, `20`; the existing CellFeeder and CellConveyor Endpoints retain their semantic distinction within that Domain. No new Endpoint IDs are invented here.

This is a clean reduction only if Nodes 20 and 21 represented functions rather than independently meaningful network participants. If they carried distinct broadcast membership, authority, routing, or failure identity, Candidate 1 has removed an expressive capability and moved that distinction into Endpoint semantics. Section 17 promotes this as a remap finding rather than assuming the collapse is free.

Core0 and Core1 remain separate Participants even though they share one physical MCU. This resolves the original `SF-018` / `SF-019` question for this topology: each Domain-local Link Telemetry instance is addressed through the stable identity of its owning Endpoint Domain. It does not resolve multiple independently addressable same-type Service instances inside one Endpoint Domain.

---

## 6. Wires after remap

| Wire | Origins needed | System role | Members | Configured observers | Physical realization | Why retained |
|---|---:|---|---|---|---|---|
| PlantNet #10 | 1 | Primary | 1–9, 20 | SCADA 22; Core0 local logger tap | SmCore1↔SmCore0, PlantCAN, AuxCAN, SensorRS485, CellLink, EthPlant observation egress | Plant control, plant-scoped broadcast, SCADA observation, and plant-authority failure state |
| Service #101 | 1 | Supporting | 1–10, 20, 30 | None | EthPlant + Sm + fieldbuses through per-Domain forwarding | Maintenance authority, gateway Domain maintenance, service broadcast scope, and laptop-offline state |

`PlantNet` permits only Core0 Participant 9 to act as Origin. `Service` permits only service-tool Participant 30. Core1 Participant 10 remains a Link scheduler and Router owner, not a PlantNet Origin.

Core1 is intentionally not a PlantNet member: forwarding and Link scheduling do not themselves require semantic membership. SCADA 22 is intentionally excluded because the experiment defines configured observers separately from `Members`.

### Count

```text
Wires after remap: 2
Wires requiring >1 Origin: 0
Multi-Origin Wires by role: none
```

Multi-Origin does not earn a Wire reduction in this sketch.

---

## 7. Interactions after remap

| Interaction | Producer | Consumer(s) | Wire | Actual Origin | Participant | Direction | Binding / notes |
|---|---|---|---|---:|---:|---|---|
| Drive torque command | Core0 / 9 | DriveLeft or DriveRight | PlantNet | 9 | 1 or 2 | OriginToParticipant | Static plant-control transmit binding; ≤5 ms path budget |
| Drive encoder feedback | Drive / 1 or 2 | Core0 / 9 | PlantNet | 9 | 1 or 2 | ParticipantToOrigin | Authorized telemetry binding; autonomous production does not make the drive an Origin |
| Aux valve command | Core0 / 9 | ValveCtrl / 3 | PlantNet | 9 | 3 | OriginToParticipant | Static plant-control binding |
| Sensor reading | Sensor / 5–8 | Core0 / 9 | PlantNet | 9 | 5–8 | ParticipantToOrigin | Core1 poll projection; poll initiation remains a Link role |
| Cell handshake | Core0 / 9 | Cell Domain / 20 | PlantNet | 9 | 20 | OriginToParticipant | Existing CellFeeder Endpoint selects the function inside Participant 20 |
| SCADA telemetry view | Field Participants 1–8, 20 | SCADA observer / 22 | PlantNet | 9 | Producing field Participant | ParticipantToOrigin, observed | Existing configured observation path; SCADA is not the required sink or a member |
| Parameter read | Service laptop / 30 | ValveCtrl / 3 | Service | 30 | 3 | OriginToParticipant | Static request binding |
| Parameter reply | ValveCtrl / 3 | Service laptop / 30 | Service | 30 | 3 | ParticipantToOrigin | Request-scoped reply binding; Direction reversal alone grants no authority |
| Firmware update segment | Service laptop / 30 | DriveLeft / 1 | Service | 30 | 1 | OriginToParticipant | Reliable segment transport |
| Link Telemetry — Core0 | Core0 / 9 | Service laptop / 30 | Service | 30 | 9 | ParticipantToOrigin | Authorized telemetry binding; reports Core0 Link faces |
| Link Telemetry — Core1 | Core1 / 10 | Service laptop / 30 | Service | 30 | 10 | ParticipantToOrigin | Authorized telemetry binding; reports Core1 Link faces |
| Plant event log tap | Preserved from tapped PDU | Core0 logger Queue | PlantNet | 9 | Preserved from tapped PDU | Preserved, observed locally | PlantNet permits only Origin 9; local observation is not a new interaction or Origin |

Direction remains structural. Participant-side autonomous telemetry is legal through its transmit binding even though the named Origin anchoring the relation is Core0 on PlantNet or the service laptop on Service.

---

## 8. Gateway forwarding — unchanged projection

No forwarding entry changes physically or semantically. Only the canonical destination terminology changes from per-Wire NodeId to deployment-global `ParticipantId`; each constrained Link may use an Organizer-generated compact local projection.

Each Endpoint Domain still owns its own Router. Shared memory remains an explicit Link hop.

### Core0 Router — PlantNet

| Ingress | Egress / delivery | Notes |
|---|---|---|
| Core0 plant Services (local) | `CellLink` for Participant 20 | Direct — CellLink IF belongs to Core0 |
| Core0 plant Services (local) | `SmCore1` for Participants 1–8 | Toward Core1 fieldbuses |
| `SmCore1` | local delivery to Core0 plant Services | Field telemetry and replies |
| `CellLink` | local delivery | Cell segment |
| `CellLink` / `SmCore1` | `EthPlant` | Existing configured observation set toward SCADA 22 |

### Core0 Router — Service

| Ingress | Egress / delivery | Notes |
|---|---|---|
| `EthPlant` | local delivery to Participant 9 maintenance Endpoints | Service laptop to Core0 |
| `EthPlant` | `SmCore1` for Participants 1–8 and 10 | Toward Core1 or its fieldbuses |
| `EthPlant` | `CellLink` for Participant 20 | Toward cell gateway |
| `SmCore1` / `CellLink` | `EthPlant` | Participant-to-Origin traffic toward service laptop 30 |

### Core1 Router — PlantNet

| Ingress | Egress / delivery | Notes |
|---|---|---|
| `SmCore0` | `PlantCAN` / `AuxCAN` / `SensorRS485` per ParticipantId | Fieldbus fan-out only; no CellLink |
| fieldbus | `SmCore0` | Participant-to-Origin traffic toward Core0 |

### Core1 Router — Service

| Ingress | Egress / delivery | Notes |
|---|---|---|
| `SmCore0` | local delivery to Participant 10 or appropriate fieldbus for Participants 1–8 | Maintenance and Link Telemetry routing |
| local Participant 10 / fieldbus | `SmCore0` | Traffic toward service laptop Origin |

Cross-fieldbus sequencing remains application composition on Core0. It does not become forwarding, and no semantic boundary disappears.

---

## 9. Wire accounting

### Removed Wires

No original Wire is removed. Therefore §4.4 has no removed-Wire responsibility rows.

### Why the tempting collapse was rejected

A single `Machine` Wire could permit Participants 9 and 30 to originate, but the removed topology would reappear as policy:

| Responsibility | PlantNet + Service today | If collapsed into one Machine Wire |
|---|---|---|
| Route scope | Plant traffic reaches EthPlant only through the SCADA observation projection; Service traffic enters normally from EthPlant | Requires Origin- and Endpoint-sensitive route restrictions on the same Wire |
| Broadcast scope | Plant broadcast excludes service-only gateway/laptop membership; Service broadcast may include maintenance Domains | Requires per-Origin or per-Endpoint broadcast restrictions and receiver filtering |
| Command authority | Structurally separated by two Wires | Moves into a two-Origin Endpoint authorization matrix |
| Failure distinction | Plant authority and maintenance authority can be reported independently by Wire | Must be reconstructed as capability-specific authority state |
| Observation | SCADA observes selected PlantNet traffic only | Requires policy preventing maintenance traffic from entering the same observation set |

The combined Wire would reduce the count from two to one while preserving or increasing authored intent. It is not the natural remap.

---

## 10. Authority policy

One table row is one policy rule for counting purposes.

| Wire | Members | Permitted Origins | Endpoint-level restrictions beyond that | Authored or generated? |
|---|---|---|---|---|
| PlantNet #10 | 1–9, 20 | 9 | Participant-side plant telemetry/replies require declared transmit bindings; SCADA 22 observation is separately authorized | Generated from the same PlantNet Wiring and Service bindings already required; **0 new authored rules** |
| Service #101 | 1–10, 20, 30 | 30 | Maintenance requests and updates originate through service bindings; Participant-side replies and Link Telemetry require declared bindings | Generated from the same Service Wiring and Service bindings already required; **0 new authored rules** |

The flattened policy is neither smaller nor more complex than the retained Wire structure: it is a direct projection of two single-Origin declarations. It introduces no genuinely new authorization state.

Global identity removes repeated per-Wire NodeId assignment from author intent. Classical CAN and other constrained carriers still require local compact-address projections, but Organizer owns those generated mappings.

---

## 11. Observation accounting

| Observation path | Remap result | Transmission count | Recipient scope and authorization | Required/optional and failure meaning |
|---|---|---|---|---|
| Field telemetry to SCADA 22 | Retained unchanged | One field publication plus the same configured EthPlant forwarding copy | Only the configured PlantNet observation set is delivered to globally identified observer 22; observer excluded from `Members` | Optional relative to required delivery at Origin 9; loss removes SCADA visibility but not plant delivery or redundancy coverage |
| PlantNet ingress to Core0 logger Queue | Retained unchanged | Local tap; no second network transmission | Explicit local observation binding | Logging loss is independent of required plant delivery |

Global identity names the observing Endpoint Domain consistently but does not convert observation into membership, addressing, forwarding authority, or redundancy.

---

## 12. Coordinator absence and degraded-state clarity

The two-Wire mapping preserves the original crisp states:

```text
Service Wire exists
permitted Origin 30 is offline
maintenance authority unavailable
PlantNet traffic continues
no role is reassigned
```

```text
PlantNet exists
permitted Origin 9 is unavailable
plant-control authority unavailable
Service maintenance may continue where the surviving paths and Endpoints permit
no role is reassigned
```

A Core1 failure still removes or degrades the PlantCAN, AuxCAN, and SensorRS485 branches while leaving Core0 and CellLink behavior separately diagnosable. Global identity improves attribution to Domain 10 but does not change physical coverage.

Failure clarity and role stability are therefore unchanged.

---

## 13. Global identity and splice coordination

The baseline carries network-visible PlantNet and Service directly across the inter-core Link and does not splice them. Its recommended bring-up pattern is unchanged.

If a future Core1 device-private diagnostic Wire is spliced onto Service, deployment-global Participant identity removes semantic NodeId coordination between the private and external Wire: Core1 remains Participant 10 on both sides. A constrained carrier may still assign a local compact address, generated by Organizer. This benefit is attributable to **Global identity**, not multi-Origin.

---

## 14. Change attribution

| Change | Attribution |
|---|---|
| Core0 and Core1 have stable identities 9 and 10 across all Wires | **Global identity** |
| Per-Domain Link Telemetry is addressed by its owning Endpoint Domain, resolving the cross-Domain topology exercised by `SF-018` / `SF-019` | **Global identity**; multiple same-type instances inside one Domain remain separate |
| Field participants no longer require repeated per-Wire NodeId allocation | **Global identity** |
| One Cell gateway Domain replaces two Wire-local participant aliases; existing Endpoints retain function selection | **Global identity** — simplification if they were only functions, potential expressive loss if they required independent participant semantics |
| Carrier-local compact addresses become Organizer projections rather than semantic identities | **Global identity** |
| PlantNet and Service remain separate | **Neither / unchanged natural mapping** |
| Permitted-Origin rows become explicit generated policy | **Multi-Origin premise**, but no authored burden or topology benefit |
| Per-Domain routing, shared-memory hops, poll projection, and forward-vs-compose boundaries remain unchanged | **Neither / unchanged** |
| No Wire requires more than one Origin | **Multi-Origin produces a null result** |

No meaningful change requires both candidates inseparably.

---

## 15. Friction re-rating

| Signal | Before -> after | Reason |
|---|---|---|
| **Artificial Origin** | None -> None | Core0 and the service laptop remain the natural authorities; improvement would be tautological in any case |
| **Artificial Wire** | None -> None | PlantNet and Service still represent distinct communication and failure relationships |
| **Wire proliferation** | None -> None | Two real authority/policy scopes remain two Wires |
| **Forwarding tax** | Mild / None -> Mild / None | Per-Domain routes and explicit shared-memory hops are unchanged |
| **Identity awkwardness** | Mild -> None | Stable Endpoint-Domain identity directly distinguishes Core0 and Core1 telemetry instances |
| **Interaction awkwardness** | None -> None | Commands, telemetry, poll projection, observation, and request-scoped replies retain their natural forms |
| **Configuration burden** | Mild -> Mild | Global identity removes per-Wire identity allocation, but per-Domain forwarding and poll projections still dominate |
| **Role instability** | None -> None | Each Wire still has one fixed legitimate Origin; no runtime reassignment |
| **Failure mismatch** | None -> None | The same physical and Domain failure boundaries remain visible |

The identity improvement is real but follows directly from the global-identity premise. No primary informative signal improves because of multi-Origin, so the overall result is **Different**, not a claimed simplification.

---

## 16. Changed / unchanged summary

**Changed:**

- Wire-local NodeIds become deployment-global Endpoint-Domain `ParticipantId`s.
- Core0 and Core1 service identity is settled by Domain ownership rather than Wire-local aliases.
- The single Cell gateway Domain has one Participant identity; its existing Endpoints distinguish CellFeeder and CellConveyor functions, subject to the expressive-capability finding below.
- The permitted-Origin policy is displayed explicitly as two generated rows.
- Carrier-local compact addressing is recognized as generated projection machinery.

**Unchanged:**

- Two Wires: PlantNet and Service.
- One legitimate Origin on each Wire.
- SCADA and local logger observation paths.
- Every physical forwarding path and per-Domain Router boundary.
- Shared memory as a real Link hop.
- Core1's RS-485 poll role without semantic Origin authority.
- Cross-fieldbus composition on Core0.
- Timing, failure coverage, and coordinator-offline behavior.

---

## 17. Candidate-1 finding and open questions

**Finding candidate — multiple network identities within one Endpoint Domain:** Candidate 1 imposes one `ParticipantId` per Endpoint Domain. The source used two Wire-local network identities, Nodes 20 and 21, inside one Cell gateway Domain. The remap can represent them as Endpoints under Participant 20, but that preserves behavior only when their distinction is Endpoint-local. The candidate needs an explicit answer for systems that require separate participant-level broadcast membership, authority, routing, or failure identity without splitting one dispatch/ownership Domain into two.

- When one Endpoint Domain exposes independently addressed CellFeeder and CellConveyor functions, is Endpoint identity alone sufficient for every required broadcast and authorization distinction, or should the device expose two Endpoint Domains?
- How is a globally identified but non-member observer such as SCADA 22 represented in generated observation-policy and audit views?
- Does the Organizer show carrier-local aliases alongside `ParticipantId` in per-Link route diagnostics without encouraging users to treat the aliases as semantic identity?

These are remap-local questions; this file does not resolve architecture adoption.

---

## 18. End-of-remap summary

```text
Sketch: 05_multicore_gateway
Wires before: 2
Wires after: 2
Wires requiring >1 Origin: 0
Multi-Origin Wires by role: none

Informative friction improved: none attributable to multi-Origin
Informative friction regressed: none
Informative friction unchanged: Wire proliferation, configuration burden, interaction awkwardness, forwarding tax, failure mismatch, artificial Wire, role instability

Benefits attributable to Global identity:
  stable per-Domain identity; cross-Domain SF-018/SF-019 topology resolved; no per-Wire NodeId allocation
  caveat: one Domain can no longer expose two independent participant identities
Benefits attributable to Multi-Origin:
  none in this sketch; both retained Wires require exactly one Origin
Benefits attributable to Both / inseparable:
  none

New authored authorization state introduced: 0 rules
Existing structural authority removed: none; two single-Origin Wire declarations remain
Observation paths removed / retained / changed:
  removed 0; retained SCADA PlantNet observation and Core0 local logger tap; changed 0
```


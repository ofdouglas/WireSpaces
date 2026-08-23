# WireSpaces Sketch 09 — Containerized Battery Energy Storage System (BESS)

**Status:** Phase-1 sketch against the frozen Sketch 09 brief, revision 2 after review. Derived under the accepted multi-Origin / global-ParticipantId premises. Carrier bit projection, CAN-ID allocation, and commissioning are out of scope by assignment.

**Source:** `WireSpaces_Sketch_09_BESS_Agent_Brief.md` (frozen). No sibling sketches, evaluator notes, or expected outcomes were consulted.

**Revision 2 changes:** applied the refined Wire-splitting rule (§0.2); merged `PlatformTelemetry` + `FirmwareUpdate`, and merged `PlantControl` + `ContainerCapability`, reducing 12 Wires to 10; removed invented peer-broadcast consumers in favour of addressed delivery; adopted the `permitted Origin` / `accepted Origin` terminology; revised the central allow-set finding, which the merges substantially weakened; re-rated Artificial Wire, Wire proliferation, and Role instability.

---

# 0. Assumptions and method

## 0.1 Leaf multiplicity — assumed, not given

The brief freezes the link segments and says every leaf is a WS-capable Participant with its own Endpoint Domain, but never states counts. Concrete representative counts assumed:

| Segment | Assumed leaf count |
|---|---|
| Rack CAN 1 | 4 rack managers |
| Rack CAN 2 | 4 rack managers |
| Rack CAN 3 | 4 rack managers |
| Thermal RS-485 | 2 HVAC/coolant controllers |
| Cabinet / safety I/O CAN | 2 cabinet I/O controllers (fire/smoke + isolation; contactor/cabinet discrete) |
| PCS power-stage CAN-FD | 4 inverter/power-module controllers |

Total: **25 Endpoint Domains** for one container. Nothing in the mapping depends on the exact numbers; they matter only to the identity-consumption and forwarding-tax observations in §10 and §11.

## 0.2 The Wire-splitting rule used

Stated up front because every Wire decision below is an application of it, and the brief's anti-cheat constraint requires the reasoning to be visible rather than the result.

> **Split into separate Wires when communication reach, physical or degraded topology, or meaningful membership and broadcast scope differ — that is, when there is an actual logical-network boundary.**
>
> **Differing application or Service availability is not by itself a reason to split.** Almost every Service has a distinct availability story, so allowing that as a criterion lets any Service justify its own Wire.

The corollary matters as much: where only authority or Service availability differs and the reach and failure topology are shared, the distinction is expressed at the Endpoint, not by adding a Wire. Every place this rule was applied in each direction is recorded, and the cost of each choice is counted in §10.

Revision 1 of this sketch used a weaker rule that admitted "operator-visible failure meaning" as a splitting criterion. That produced two Wire pairs that were topologically identical and differed only in which Service went quiet. Both are merged here, and §3.3 records what was lost and gained.

## 0.3 Two clarifications the mapping depends on

**Originating versus replying.**

> A Wire's permitted-Origin set governs who may **anchor** an interaction. A request-scoped reply travels `ParticipantToOrigin` under the *initiator's* Origin identity, so answering does not require the responder to be a permitted Origin.

Without this, every Wire carrying a request/response Service would need all-member Origins and the permitted-Origin set would collapse into a formality.

**Permitted versus accepted Origin.** Used consistently from here on:

```text
permitted Origin
    statically authorized to originate this operation in some supported state

accepted Origin
    presently accepted by this receiving Service under current runtime state
```

The first is configuration and is Organizer-checkable. The second is Service state and is not. Section 5.4 is about the consequences.

---

# 1. Native communication summary

Deliberately in the machine's own vocabulary, with no WireSpaces terms.

**Container-level control network (Control Ethernet LAN).** Five controllers share one switched industrial LAN: Plant Control A, Plant Control B, the Safety Controller, the PCS Controller, and the Rack Gateway. All container-level command, coordination, capability reporting, and platform traffic crosses this segment.

**Redundant plant control.** Exactly one Plant Controller holds the normal plant-control role at a time, identified by a monotonically advancing term. Plant A is preferred on clean start. Either controller can reach every production sink over the LAN without help from the other. The direct Plant A ↔ Plant B PartnerLink carries peer health and failover-role state only; it is never a command path. A gap with neither controller active is permitted; both simultaneously accepted for normal control is not.

**Independent safety authority.** The Safety Controller is always active and independent of plant role. It commands emergency shutdown, isolation and contactor opening, safety derate, and energization inhibit against the PCS, the Rack Gateway, the rack managers, and the cabinet I/O. It does not perform normal power dispatch.

**Power conversion.** The PCS Controller accepts a container-level power objective, refuses or clamps infeasible objectives, and turns what it accepts into module-level commands on its own private CAN-FD segment. It publishes power capability, actual power, and inverter fault/thermal constraint upward to the active Plant Controller and to Safety.

**Rack subsystem.** The Rack Gateway supervises three electrically and fault-isolated rack CAN buses, a thermal RS-485 segment, and a cabinet/safety I/O CAN segment. It aggregates rack state into container-level availability and state-of-charge/health figures, derives per-rack actions from container objectives, and retains a narrow local protective authority — isolate, inhibit, open contactors for a locally detected rack or cabinet fault — that survives the loss of both Plant Controllers. It is also the container's time-synchronization source.

**Leaves.** Rack managers own battery state, contactor sequencing, balancing, local limits, and rack faults, and report availability, power-limit, thermal-limit, and fault state upward to the Rack Gateway. Power modules run under PCS control with local electrical and thermal protections and report their state to the PCS. HVAC/coolant controllers report thermal capability and can raise a local thermal alarm to the Rack Gateway. Cabinet I/O controllers drive contactors and isolation and report smoke/fire and cabinet discrete state to the Rack Gateway.

**Precedence.** Safety action dominates normal plant objectives. Rack Gateway local protective action dominates a plant objective within the affected rack or cabinet scope. Rack and PCS capability limits constrain plant requests without conferring general control authority. Commands from a superseded plant term are stale even if the sender is still reachable. The receiving service or actuator owns source and term selection; last sender never wins.

**Firmware and time.** The Plant Controller holding the current term is the only system-wide firmware-update orchestrator, and only in an explicit maintenance/update state. Update is not initiated during a role transition, and is unavailable while no controller is active. The Rack Gateway is the time source; its loss does not transfer time-source ownership, and surviving controllers fall back to local monotonic time with degraded timestamp quality.

---

# 2. Participants

One Endpoint Domain per named ECU, one deployment-global `ParticipantId` each, stable across every Wire joined. Ranges are grouped for readability only; global uniqueness forbids reusing a low range on a second bus.

| PID | Endpoint Domain | Device class | Attaches to |
|---:|---|---|---|
| 1 | Plant Control A | Redundant plant controller | Control Ethernet, PartnerLink |
| 2 | Plant Control B | Redundant plant controller | Control Ethernet, PartnerLink |
| 3 | Safety Controller | Always-active safety authority | Control Ethernet |
| 4 | Rack Gateway | Embedded gateway + local controller + time source | Control Ethernet, Rack CAN 1/2/3, Thermal RS-485, Cabinet I/O CAN |
| 5 | PCS Controller | Power-conversion controller + platform gateway | Control Ethernet, PCS power-stage CAN-FD |
| 11–14 | Rack Manager R1–R4 | Complex battery-rack leaves | Rack CAN 1 |
| 21–24 | Rack Manager R5–R8 | Complex battery-rack leaves | Rack CAN 2 |
| 31–34 | Rack Manager R9–R12 | Complex battery-rack leaves | Rack CAN 3 |
| 41–44 | Power Module M1–M4 | Inverter/power-module leaves | PCS power-stage CAN-FD |
| 51–52 | HVAC/Coolant H1–H2 | Thermal leaves | Thermal RS-485 |
| 61–62 | Cabinet I/O C1–C2 | Discrete/safety I/O leaves | Cabinet I/O CAN |

Two observations, both properties of candidate 1 rather than of this machine:

- **No per-Wire renumbering exists.** The Rack Gateway is Participant 4 on all nine Wires it joins. Rack Manager R5 is Participant 21 whether seen on its own rack bus, on the forwarded safety Wire, or on the forwarded platform Wire.
- **Identity is consumed per Endpoint Domain, not per device.** 25 identities for one container. A site of eight containers plus site-level equipment consumes roughly 200. This is the consumption datum the brief's premises invite; it is not awkward here, but it is a sizing input that a device count understates.

---

# 3. Link Interfaces and Wires

## 3.1 Link Interfaces per participant

| Participant | Interfaces |
|---|---|
| Plant A (1), Plant B (2) | 2 each: Control Ethernet, PartnerLink |
| Safety (3) | 1: Control Ethernet |
| Rack Gateway (4) | 6: Control Ethernet, Rack CAN 1, Rack CAN 2, Rack CAN 3, Thermal RS-485, Cabinet I/O CAN |
| PCS (5) | 2: Control Ethernet, power-stage CAN-FD |
| All leaves (11–62) | 1 each |

Rack Gateway interfaces are referenced below as `IF0`..`IF5` in that order. PCS interfaces are `IF0` (Ethernet) and `IF1` (CAN-FD).

## 3.2 Wires

Ten Wires over eight frozen segments.

| # | Wire | Members | Physical realization | Permitted Origins | Origin count | Role |
|---|---|---|---|---|---:|---|
| W1 | `ContainerCoordination` | 1, 2, 3, 4, 5 | Control Ethernet | {1, 2, 4, 5} | 4 permitted; among 1 and 2 only one accepted at a time | Primary |
| W2 | `SafetyAuthority` | 1, 2, 3, 4, 5, 11–14, 21–24, 31–34, 61, 62 | Control Ethernet + Rack CAN 1/2/3 + Cabinet I/O CAN (forwarded by 4) | {3} | 1 | Primary |
| W3 | `PlantPartner` | 1, 2 | PartnerLink | {1, 2} | 2, both concurrently | Supporting |
| W4 | `Platform` | all 25 | Control Ethernet + all 6 downstream segments (forwarded by 4 and 5) | all members | 1 authored statement, 25 members | Supporting |
| W5 | `RackBus1` | 4, 11–14 | Rack CAN 1 | all members | 5 | Primary |
| W6 | `RackBus2` | 4, 21–24 | Rack CAN 2 | all members | 5 | Primary |
| W7 | `RackBus3` | 4, 31–34 | Rack CAN 3 | all members | 5 | Primary |
| W8 | `Thermal` | 4, 51, 52 | Thermal RS-485 | all members | 3 | Supporting |
| W9 | `CabinetIO` | 4, 61, 62 | Cabinet I/O CAN | all members | 3 | Primary |
| W10 | `PowerStage` | 5, 41–44 | PCS power-stage CAN-FD | all members | 5 | Primary |

Wires per segment: Control Ethernet 3 (W1, W2, W4); PartnerLink 1 (W3); each rack CAN 3 (W2, W4, and its own); Thermal RS-485 2 (W4, W8); Cabinet I/O CAN 3 (W2, W4, W9); power-stage CAN-FD 2 (W4, W10).

Every Wire in the set corresponds to either **one physical segment** or **one distinct multi-segment reach**. There is no Wire that shares both its membership and its realization with another.

## 3.3 Why each Wire exists, and the two merges

**W1 `ContainerCoordination` — merged from `PlantControl` and `ContainerCapability`.** Revision 1 split these because plant dispatch and capability publication have different permitted Origins, and because a silent dispatch Wire means something different to an operator than a silent capability Wire. Tested against the four criteria in §0.2, that split does not survive:

| Criterion | `PlantControl` | `ContainerCapability` | Differs? |
|---|---|---|---|
| Communication reach | Control Ethernet, container controllers | Control Ethernet, container controllers | No |
| Physical / degraded topology | Dies with the LAN | Dies with the LAN | No |
| Membership | 1, 2, 4, 5 | 1, 2, 3, 4, 5 | By one participant |
| Broadcast scope | Those four | Those five | By the same one |

The only structural difference was whether Safety is a member, and Safety is a legitimate member of the merged Wire: §4.5 makes it a frozen required recipient of PCS capability, and receiving plant role announcements is useful to it and harmless. Everything else that separated the two was Service availability, which §0.2 now excludes. Merged, with permitted Origins {1, 2, 4, 5} — Safety is a **member that never originates ordinary container coordination**, which the allow-set states directly.

What the merge costs: the Config C statement no longer comes from a Wire going silent. It now comes from *which permitted Origins are currently live within one Wire*, which turns out to be both more precise and more demanding of tooling (§9.3, §11.4).

**W4 `Platform` — merged from `PlatformTelemetry` and `FirmwareUpdate`.** Revision 1's weakest split, and it fails the rule outright: identical membership, identical realization across all seven segments, identical broadcast scope, identical physical failure behaviour. The sole justification was that update authority disappears with both Plant Controllers while telemetry does not — a Service-capability distinction. Merged, with all members permitted to originate, and firmware-update authority expressed entirely at the Endpoint (`FirmwareUpdateControl`, §5.2). Config C then reads naturally: *the Platform Wire is operational, telemetry continues, and no currently accepted Origin exists for firmware-update control.*

**W2 `SafetyAuthority` spanning onto the rack and cabinet buses.** This one is a genuine reach difference — five segments, nineteen members, a set no other Wire has — so it survives the rule easily. The design question was forwarding versus composition, and forwarding wins on authority grounds rather than availability grounds: the Rack Gateway is the sole physical path either way, but §4.6 places source and term selection in the receiving actuator, and a rack manager can only apply *Safety dominates plant objective* if it can see that the isolate request was originated by Safety. Composition would erase the distinction the precedence rule needs and relocate the decision into the gateway.

**W3 `PlantPartner`.** Its own segment, its own failure meaning that is physical rather than Service-level (PartnerLink loss is explicitly not a role transition), and genuinely two concurrent Origins. This is the one Wire in the mapping that would have needed either two Wires or an artificial coordinator under a single-Origin model.

**One Wire per rack CAN rather than one `RackFabric`.** Each bus is its own physical failure domain — config D names a single isolated rack CAN — its own broadcast scope, and its own reach. All three criteria agree. A merged `RackFabric` would turn config D from a Wire-unrealized condition into a partial-membership condition, which is precisely the reduction-by-Wire-count the brief warns against.

**All-member Origins on W5–W10, deliberately.** The supervisor originates commands downward and each leaf originates its own state, fault, and limit traffic upward, so any per-Wire allow-set would name every member. Splitting each into a command Wire and a status Wire would double Wire count on the constrained carriers to make the allow-set expressive, while reach, broadcast scope, and failure topology stay identical. Authority here is a per-Endpoint property — *a rack manager may publish, and may not command a peer* — which is where §4.5 already puts it.

---

# 4. Interactions

Origin named for each. `→` means required sink; broadcast is used only where the brief freezes more than one recipient. Reply traffic is `ParticipantToOrigin` under the named Origin's identity per §0.3, not a separate interaction.

## 4.1 Application interactions

| # | Interaction | Origin | Wire | Required sink(s) | Delivery | Notes |
|---:|---|---|---|---|---|---|
| A1 | Charge/discharge power objective, mode, ramp request | Accepted Plant (1 or 2) | W1 | PCS (5) | Addressed | Term-stamped. PCS may refuse or clamp. Reliable/confirmed transport |
| A2 | Container rack enable/disable objective, availability request, startup/shutdown sequence, balancing coordination | Accepted Plant (1 or 2) | W1 | Rack Gateway (4) | Addressed | Terminates at gateway; composed onward (§6.2) |
| A3 | Plant active-role / term announcement | 1 or 2 | W1 | 4, 5 | Broadcast to W1 members | Two frozen required sinks, both needing term state for §4.6 rejection. Safety and the standby peer receive it as members, not as required sinks |
| A4 | Peer health, state synchronization, failover-role state | 1 or 2 | W3 | the peer | Addressed | Two concurrent Origins. Never a command path |
| A5 | Emergency shutdown / isolation / open contactors / safety derate / inhibit energization | Safety (3) | W2 | 5, 4, addressed rack managers, addressed cabinet I/O | Broadcast to W2 members for container-wide action; addressed for scoped action | Forwarded through gateway with canonical identity preserved. Plants 1 and 2 receive safety state as members; precedence itself is resolved at each actuator |
| A6 | PCS power capability, actual power, inverter fault/thermal constraint | PCS (5) | W1 | Accepted Plant **and** Safety (3) | Broadcast to W1 members | Two frozen required sinks of differing role, and the required Plant sink is term-dependent. Standby Plant and Rack Gateway receive it without being required sinks |
| A7 | Container rack availability, power limit, SoC/SoH aggregate, rack degraded state | Rack Gateway (4) | W1 | Accepted Plant | Broadcast to W1 members | Frozen sink is "interested plant consumers". Broadcast rather than addressed because the required sink is term-dependent and may momentarily be nobody; Safety and PCS are non-required recipients |
| A8 | Rack availability, power limit, thermal limit, fault state | Rack manager (11–14, 21–24, 31–34) | W5/W6/W7 | Rack Gateway (4) | **Addressed** | Terminates at gateway. No peer consumption is claimed |
| A9 | Per-rack enable/disable, contactor and startup sequencing derived from A2 | Rack Gateway (4) | W5/W6/W7 | addressed rack managers | **Addressed** | Composition; distinct Endpoint from A10 |
| A10 | Rack-local protective isolate / inhibit / open contactors | Rack Gateway (4) | W5/W6/W7, W9 | affected rack managers or cabinet I/O | Addressed | Narrow local authority; persists with no Plant Controller |
| A11 | Thermal control command | Rack Gateway (4) | W8 | HVAC (51, 52) | Addressed | Master-initiated carrier; see §4.3 |
| A12 | Thermal capability, fault state, local thermal alarm | HVAC (51 or 52) | W8 | Rack Gateway (4) | Addressed | HVAC is the Origin even though the gateway's LLL initiates the transfer |
| A13 | Cabinet discrete state, smoke/fire detection, isolation feedback | Cabinet I/O (61, 62) | W9 | Rack Gateway (4) | Addressed | Reaches Safety only via A7 composition — see §11.7 |
| A14 | Module-level power-stage command | PCS (5) | W10 | power modules (41–44) | **Addressed** | Composition of A1, A5, A6, A15. Whether synchronized multi-module action needs a broadcast form is not frozen — see §11.8 |
| A15 | Module fault, thermal and electrical protection state | Power module (41–44) | W10 | PCS (5) | **Addressed** | Terminates at PCS; composed into A6 |
| A16 | Black-start / coordinated startup sequencing intent | Accepted Plant (1 or 2) | W1 | 4, 5 | Addressed | Composed at both gateways |

## 4.2 Platform interactions

Representative paths only, per the brief's scope rule.

| # | Interaction | Origin | Wire | Sink | Delivery | Notes |
|---:|---|---|---|---|---|---|
| P1 | Identity, hardware identity, software-version inventory | any participant | W4 | accepted Plant | Addressed, on request | Forwarded canonically through 4 and 5 for leaves |
| P2 | Health / heartbeat, major internal component status — LAN participants | 1, 2, 3, 4, 5 | W4 | accepted Plant | Addressed | Also the liveness source for the §9 degraded statements |
| P3 | Health / heartbeat, link and interface counters — leaves | each leaf | its own local Wire (W5–W10) | local collector: 4 for rack/thermal/cabinet, 5 for modules | Addressed | **Not** carried container-wide. Composed into a subsystem-health summary (§6.2) |
| P4 | Structured event and persistent fault record | any participant | W4 | accepted Plant | Addressed | Forwarded with leaf identity preserved, so fault history attributes to the originating leaf |
| P5 | Text log output | any participant | W4 | accepted Plant | Addressed, rate-limited | On-demand for leaves |
| P6 | Reset reason / watchdog history | any participant | W4 | accepted Plant | Addressed, on request | |
| P7 | Firmware-update control | Accepted Plant (1 or 2) | W4 | addressed target, any of the other 24 | Addressed | `FirmwareUpdateControl` Endpoint admits only 1 or 2, only at current term, only in maintenance/update state |
| P8 | Firmware-update status | — | W4 | — | `ParticipantToOrigin` under the initiating Plant's Origin | Not an independent interaction |
| P9 | Time synchronization | Rack Gateway (4) | W4 | all participants | Broadcast, forwarded onto all 6 downstream segments | The only genuinely container-wide broadcast in the mapping |
| P10 | Timestamp-quality / degraded-sync status | any participant | field within P2/P3 | local collector, then accepted Plant | — | Degrades without reassignment when 4 is absent |

**The heartbeat/event split in P3 versus P4 is a deliberate design decision.** Carrying 20 leaf heartbeats as container-wide traffic would push periodic platform load across every constrained carrier for the benefit of one collector. Periodic leaf liveness stays on the leaf's own local Wire and is composed into a subsystem summary; event and fault records — which must attribute to the originating participant and must not be summarized away — are forwarded on W4 with canonical identity intact. This is the concrete answer to the brief's broadcast-scope question: the container-spanning platform Wire is almost entirely addressed, and only time synchronization broadcasts on it.

## 4.3 Master-initiated Thermal RS-485

The gateway's link layer polls the HVAC controllers; they cannot push when they choose. Per A12 the HVAC controller remains the semantic Origin of its own alarm — initiating a transfer is not authoring a message. The consequence is that the *freshness* of A12 is bounded by the polling cadence rather than by the alarm's own timing, which is a deployment property of W8 and must be validated against any thermal-alarm latency requirement. Multi-Origin changes nothing here; the rule holds unmodified.

---

# 5. Authorization policy

## 5.1 Authored intent, layer 1 — permitted Origin per Wire

Ten statements.

```text
W1  ContainerCoordination  origins {1, 2, 4, 5}     -- excludes Safety
W2  SafetyAuthority        origins {3}              -- excludes 18 of 19 members
W3  PlantPartner           origins {1, 2}           -- all members
W4  Platform               origins = all members
W5  RackBus1               origins = all members
W6  RackBus2               origins = all members
W7  RackBus3               origins = all members
W8  Thermal                origins = all members
W9  CabinetIO              origins = all members
W10 PowerStage             origins = all members
```

**The allow-set restricts on 2 of 10 Wires and names every member on 8.** Under revision 1's finer topology it restricted on 4 of 12. Coarsening the Wires did not move authority into the allow-set; it moved authority *out* of it, into §5.2. That reversal is the sketch's most decision-relevant finding and is developed in §11.1.

## 5.2 Authored intent, layer 2 — Endpoint authorization classes

Nine classes. Each Endpoint in the deployment is assigned exactly one.

| Class | Permitted Origin | Runtime acceptance predicate | Applied to |
|---|---|---|---|
| `NormalPlantControl` | 1 or 2 | Origin holds the current active term | PCS power objective, mode, ramp; Rack Gateway rack objective, startup sequence, balancing coordination |
| `SafetyAction` | 3 only | none — always accepted | PCS safety inhibit; Rack Gateway safety isolate; rack manager safety isolate ×12; cabinet I/O safety isolate ×2; Plant safety-state notice ×2 |
| `LocalProtective` | 4 only | none | Rack manager protective isolate ×12; cabinet I/O protective open ×2 |
| `SubsystemCommand` | the participant's designated local supervisor — 5 for modules, 4 for HVAC and per-rack actions | none | Module power-stage command ×4; HVAC thermal command ×2; rack manager enable/sequence ×12 |
| `CapabilityPublication` | any member of the Endpoint's Wire | none; publication only, grants no actuation | PCS capability; container rack availability; rack availability and limits; module protection state; thermal capability; cabinet discrete state |
| `PeerCoordination` | 1 or 2 | none | Plant peer-health and failover-role Endpoints on 1 and 2 |
| `FirmwareUpdateControl` | 1 or 2 | Origin holds the current active term **and** container is in maintenance/update state | one per participant (25) |
| `PlatformQuery` | 1 or 2 | none; read-only | identity/version, log retrieval, reset reason, fault-history read on every participant |
| `PlatformPublication` | the owning participant itself | none | own heartbeat, link telemetry, event push, log push; time-sync publication is 4 only |

**Authored total: 10 Wire statements + 9 class definitions + one class assignment per Endpoint.** An auditor reads 19 rules to know the whole authority model, then checks class assignment per Endpoint, which is mechanical and generator-checkable.

## 5.3 Organizer-generated projection

Generated from §5.1 and §5.2, never authored:

- flattened `(target participant, Namespace, EndpointId, permitted-Origin set)` tuples — roughly 200 Endpoints across 25 participants;
- Wire membership tables per participant and per interface;
- forwarding tables for participants 4 and 5 (§6);
- egress representation and carrier-local participant address projection per constrained segment (out of scope by assignment);
- coupled acceptance checks: no Endpoint admits a permitted Origin that is not also a permitted Origin of a Wire the Endpoint's participant actually joins, and no `LocalProtective` Endpoint admits participant 4 across a segment 4 does not attach to.

The generated table is large and the authored intent is small, which is the good case the brief's checklist allows for. The audit does not require maintaining a flattened permission matrix by hand.

## 5.4 The part the projection cannot express — and it is load-bearing

Two of the nine classes carry a **runtime acceptance predicate**:

```text
NormalPlantControl      Origin holds the current active term
FirmwareUpdateControl   Origin holds the current active term
                        AND container is in maintenance/update state
```

The Organizer can verify that Participants 1 and 2 are *permitted* Origins toward these Endpoints. It cannot verify which one is *accepted*, because that depends on term state owned by the native failover protocol. Authority is therefore auditable in two layers with different guarantees:

```text
permitted Origin     configuration; Organizer-checkable; complete
accepted Origin      Service state at the receiving actuator; not statically checkable
```

This machine makes the split unavoidable: mutually exclusive redundant controllers require *some* runtime predicate to gate command acceptance no matter how Wires are drawn. The mapping's contribution is to keep the layers named and separate rather than blending them into one policy table.

The important negative result is that **accepted Origin should not become a Wire concept.** Nothing about it is topological — it is a Service's own state about which of several permitted Origins it will currently obey, and the brief's precedence rules already place it at the receiving actuator. What deployment tooling plausibly *should* do is be able to **declare that such a predicate exists** on an Endpoint, even though it cannot evaluate it, so that generated documentation says *this Endpoint's authority is not statically decidable* instead of presenting a permission table that is only half the truth. Carried to §11.3.

---

# 6. Gateways: forwarding versus composition

Two embedded gateways: Rack Gateway (4) and PCS Controller (5). The general rule is stated first because it is what makes the gateway story auditable:

> **The two container-spanning Wires (W2 `SafetyAuthority`, W4 `Platform`) are forwarded through both gateways with canonical identity preserved. Every other Wire terminates at the gateway, and anything that crosses between them is composition.**

## 6.1 Rack Gateway (4) — forwarding table

| Wire | Ingress | Egress | Direction / note |
|---|---|---|---|
| W2 | IF0 (Ethernet) | IF1, IF2, IF3, IF5 | `OriginToParticipant` from Safety; addressed egress selected by target participant, all four for container-wide broadcast |
| W2 | IF1, IF2, IF3, IF5 | IF0 | `ParticipantToOrigin` replies under Safety's Origin only |
| W4 | IF0 | IF1, IF2, IF3, IF4, IF5 | addressed queries and update control; all five for the P9 time-sync broadcast |
| W4 | IF1, IF2, IF3, IF4, IF5 | IF0 | leaf event push, log, query and update responses — leaf identity preserved |
| W1 | IF0 | — | **terminates locally**; never forwarded downstream |
| W5, W6, W7, W8, W9 | IF1..IF5 | — | **local only**; never forwarded to IF0, and never between downstream interfaces |

The last row carries the fault-isolation requirement: **no rack CAN is ever forwarded to another rack CAN.** Rack CAN 1/2/3 are electrically and fault isolated by the machine, and the forwarding table preserves that. No Wire appears on two Rack Gateway interfaces that are joined anywhere else, so the configured topology contains no loop.

## 6.2 Rack Gateway (4) — composition

Each item is an application interaction the gateway originates or terminates in its own right, not a forwarded one.

| Composition | Inputs | Output | Origin of output |
|---|---|---|---|
| Per-rack action derivation | A2 container rack objective on W1 | A9 per-rack enable/disable and contactor sequencing on W5/W6/W7 | 4 |
| Container rack-state aggregation | A8 rack availability and limits; A12 thermal on W8; A13 cabinet state on W9 | A7 container availability, power limit, SoC/SoH, degraded state on W1 | 4 |
| Local protective initiation | locally detected rack or cabinet fault | A10 isolate/inhibit/open on W5/W6/W7/W9 | 4 |
| Safety translation into non-W2 scope | A5 safety action received at 4's own W2 Endpoint | thermal and HVAC-scoped consequences on W8, which carries no W2 realization | 4 |
| Subsystem platform summary | P3 leaf heartbeats and link counters on W5–W9 | subsystem-health element of P2 on W4 | 4 |
| Time-source publication | local time reference | P9 broadcast on W4 | 4 |

**Term validation happens here, not at the leaf.** Because A2 terminates at the gateway and A9 is a new interaction the gateway originates, the current-term check for the whole rack subsystem is performed once, at Participant 4. The twelve rack managers do not track plant terms at all; they check that a rack-scoped command came from Participant 4 on their own bus. That is a single point of term-checking correctness, and it means a rack manager cannot itself distinguish "gateway relaying an accepted plant objective" from "gateway acting on its own protective authority" unless the Endpoints differ. They do: A9 and A10 are separate Endpoints in separate authorization classes (`SubsystemCommand` and `LocalProtective`). Without that separation the leaf's precedence rule would be unstatable.

## 6.3 PCS Controller (5) — forwarding table

| Wire | Ingress | Egress | Note |
|---|---|---|---|
| W4 | IF0 ↔ IF1 | both directions | module identity, events, logs, query responses, update control and status; P9 broadcast downstream |
| W1, W2 | IF0 | — | **terminate locally** |
| W10 | IF1 | — | **local only** |

W2 does **not** extend onto the power-stage CAN-FD. That follows the brief's authority table, which lists Safety's targets as the PCS and not the power modules. It produces a visible asymmetry with the Rack Gateway, where W2 *is* forwarded to the leaves — and the asymmetry is the machine's, not the mapping's: Safety is granted direct authority over rack managers and cabinet I/O, and only indirect authority over power modules through the PCS.

## 6.4 PCS Controller (5) — composition

| Composition | Inputs | Output | Origin of output |
|---|---|---|---|
| Power-stage control derivation | A1 accepted power objective on W1; A5 safety inhibit on W2; rack limits from A7 on W1; A15 module protection state on W10 | A14 module power-stage commands on W10 | 5 |
| Capability publication | A15 module state; local electrical and thermal model | A6 PCS capability, actual power, inverter constraint on W1 | 5 |
| Objective refusal / clamping | A1 objective versus computed feasibility | clamped objective and refusal reason, reported within A6 | 5 |
| Module platform summary | P3 module heartbeats and link counters on W10 | subsystem-health element of P2 on W4 | 5 |

The safety inhibit reaching modules only by composition means PCS availability sits inside that safety path. The brief's degraded matrix accepts this: losing the power-stage CAN-FD loses module command reach, and losing the PCS loses power conversion entirely, so no independent path to the modules exists to be preserved.

---

# 7. Broadcast and observation accounting

## 7.1 Broadcasts and who receives them

Five broadcasts. Every one has more than one recipient frozen by the machine brief; no broadcast in this mapping exists to serve an inferred peer consumer.

| Broadcast | Wire | Recipients | Frozen justification |
|---|---|---|---|
| A5 container-wide safety shutdown / isolation | W2 | 19 members | §4.5 names PCS, Rack Gateway, rack managers, and cabinet I/O as Safety's targets; §4.11 makes safety state relevant to both Plant Controllers |
| A3 plant role / term announcement | W1 | 5 members | §4.6 requires Participants 4 and 5 to reject superseded terms, so both need term state. Safety and the standby peer receive it as members |
| A6 PCS capability / actual power / inverter constraint | W1 | 5 members | §4.5 names the active Plant **and** Safety as recipients — two required sinks of different role, one of them term-dependent |
| A7 container rack availability / limits / SoC / SoH | W1 | 5 members | §4.5 names "interested plant consumers"; the required sink is term-dependent and may momentarily be none, which addressing cannot express |
| P9 time synchronization | W4 | all 25 | §4.9 makes Participant 4 the container time source for all participants |

**Everything else is addressed.** Revision 1 justified broadcasting rack-manager state and power-module protection state by peer coordination — group contactor sequencing and modules coordinating around a failed module. Neither is frozen by the brief, which describes both leaf classes as reporting upward to their supervisor. Both are addressed in revision 2 (A8, A15), as are per-rack commands (A9) and module commands (A14).

This does not weaken W5–W7 or W10, which remain multi-Origin for the reason that actually holds: **the supervisor originates commands downward and each leaf originates its own state and fault traffic upward.** The useful corollary is that **multi-Origin does not imply broadcast-heavy traffic** — nine of ten Wires in this mapping carry predominantly addressed traffic.

## 7.2 Configured observers

Optional, not addressed, no delivery guarantee, and — per the brief and `CORE §23.11` — **no observer is counted as delivery coverage or as a redundancy member.** With W1 merged, the frozen recipients of A3, A6, and A7 receive them as ordinary members, so only *addressed* traffic needs observation configuration.

| Observer | Observes | Purpose | Removable without functional loss? |
|---|---|---|---|
| Safety (3) | W1: A1 power objective and A2 rack objective, both addressed elsewhere | Awareness of what normal control is currently requesting | Yes. Safety's own authority never depends on it |
| Rack Gateway (4) | W1: A1, addressed to PCS | Coordinated startup and black-start sequencing awareness | Yes. A16 addresses the gateway explicitly for anything required |
| PCS (5) | W1: A2, addressed to Rack Gateway | Same, in the other direction | Yes, same reason |
| Standby Plant (1 or 2) | W1: A1 and A2 from the accepted peer | Warm tracking of current dispatch state to shorten a future transition | Yes. A4 on W3 is the authoritative peer channel |

---

# 8. Sink and source selection

Every receiving function that more than one participant can legitimately originate toward, with the selection rule and where it is enforced. In every case the enforcing participant is the receiving service or actuator, per the frozen rule that last-sender-wins is never the policy.

| Receiving function | Permitted Origins | Acceptance rule | Enforced at |
|---|---|---|---|
| PCS power objective / mode / ramp | 1, 2 | Accept only the current active term. A superseded term is rejected and counted even though the sender remains reachable and permitted | 5 |
| PCS energization inhibit versus power objective | 3 versus 1/2 | Safety dominates unconditionally. Separate Endpoints, separate classes, documented precedence | 5 |
| Rack Gateway rack objective / startup sequence | 1, 2 | Current active term, as above | 4 |
| Rack Gateway safety isolate versus plant objective | 3 versus 1/2 | Safety dominates | 4 |
| Rack manager isolate / inhibit / contactor state | 3 via forwarded W2; 4 via `LocalProtective`; 4 via `SubsystemCommand` derived from a plant objective | Fixed precedence: Safety > gateway local protective > plant-derived objective. Three distinct Endpoints, so the rule is expressible | each rack manager |
| Cabinet I/O contactor and isolation | 3 via forwarded W2; 4 via `LocalProtective` | Safety > gateway local protective | each cabinet I/O controller |
| Power module command | 5 only | Single source; no selection required. PCS's internal arbitration across objective, safety inhibit, rack limits, and module protection is composition (§6.4) | 5 |
| HVAC thermal command | 4 only | Single source | each HVAC controller |
| Firmware-update target on any participant | 1, 2 | Current active term **and** container in maintenance/update state. Not initiated during a transition | each target |

**This precedence structure is not moved complexity from multi-Origin.** The machine specifies three independent authority mechanisms — mutually exclusive plant role, always-active safety, narrow rack-local protection — so twenty actuators each carrying a small precedence table is a property of the machine. A single-Origin model would have needed separate Safety and Plant Wires anyway, and the precedence would still have lived in the actuator. What multi-Origin changes is only the first row: choosing between Plant A and Plant B toward the same Endpoint is a new selection problem, and its answer is the term predicate, which the machine already requires to exist.

---

# 9. Failure and degraded-state mapping

## 9.1 The four frozen configurations

**Config A — Plant A active, all eight segments available.** All ten Wires realized. Participant 1 is the accepted Origin for `NormalPlantControl` at term *N*. Operator statement: *Plant A active, term N; Safety active; all three rack segments, thermal, cabinet, and power stage reachable.*

**Config B — failover to Plant B.** The failover protocol advances the term on W3, and both controllers announce role state on W1 (A3). Participant 2 then originates A1 and A2 on W1 directly, because W1's membership already contains both controllers and its realization is the LAN. **Nothing in the WireSpaces configuration changes:**

```text
Participant identities      unchanged
Wire membership             unchanged
permitted Origin sets       unchanged
routes and forwarding       unchanged
Endpoint bindings           unchanged
accepted Origin             changed  <- the only thing that moves
```

Stale term-*N* commands from Participant 1 are rejected at Participants 4 and 5, whether or not Participant 1 is still reachable and still a permitted Origin. PartnerLink carried the coordination and never the command. Operator statement: *Plant B active, term N+1; Plant A no longer the accepted normal-control authority.*

This is where multi-Origin earns its cost in this machine. A single-Origin W1 would have required either two Wires with disjoint Origins and a duplicated sink-side story, or a runtime Origin reassignment on a Wire — the latter being exactly the role instability the premises warn about.

**Config C — both Plant Controllers unavailable.** By Wire:

```text
W1  ContainerCoordination  realized and operational; Origins 4 and 5 live,
                           Origins 1 and 2 absent
W3  PlantPartner           both members absent -> silent
W2  SafetyAuthority        fully operational; Origin 3 unaffected
W4  Platform               fully operational; no accepted Origin exists for
                           FirmwareUpdateControl; event and log push have no
                           container collector and are retained locally
W5..W10                    fully operational
```

Safety action, rack-local protective isolation, thermal control, module protection, and all local platform telemetry continue. No normal charge/discharge dispatch, no coordinated startup, no system-wide firmware update. **No local subsystem becomes a plant controller**, and no reassignment occurs, because permitted-Origin sets are authored configuration and not runtime state. Operator statement: *no Plant Controller currently active; normal dispatch and system-wide firmware update unavailable; Safety authority active; rack-local protection active.*

Note what changed from revision 1: this statement now comes from **Origin liveness within a live Wire** rather than from two Wires falling silent. It is more precise — it distinguishes "the coordination network is fine and nobody is currently in charge of dispatch" from "the coordination network is gone" — and it depends on tooling that can report per-Origin liveness. See §9.4.

**Config D — one rack CAN isolated, taking Rack CAN 2 as the instance.** W6 unrealized. W2 and W4 lose their Rack-CAN-2 realization only, so Safety cannot reach Participants 21–24 and the accepted Plant cannot query or update them. W5 and W7 untouched; W8, W9, W10 untouched; W1 and W3 untouched. Participant 4 reports the segment condition and republishes A7 with rack group 2 excluded from availability. Operator statement: *Rack CAN 2 segment isolated; rack group 2 unreachable and excluded from availability; rack groups 1 and 3 and all other container functions unaffected.* The statement is per-segment rather than container-wide precisely because each rack bus is its own Wire.

## 9.2 The frozen failure matrix

| Failure | Wires affected | Continues | Authority lost | Reassignment | Operator statement |
|---|---|---|---|---|---|
| Plant A fails while active | W1 loses one live Origin; W3 half-silent | W2, W4, W5–W10 fully; Safety and rack-local protection unaffected | Participant 1's current-term normal authority | Term transfers to Participant 2 by native protocol. No Wire, permitted-Origin, or identity change | *Plant A unavailable as accepted normal-control authority; failover in progress* |
| Plant B fails while standby | W3 half-silent; W1 loses one non-accepted Origin | Everything else, including Participant 1's normal control | Future failover capability only | None | *Standby Plant Controller unavailable; failover capability lost; Plant A active, term N* |
| Both Plant Controllers unavailable | W1 loses two of four Origins; W3 silent | W1 with Origins 4 and 5; W2, W4, W5–W10 | Normal dispatch, coordinated startup, system-wide update orchestration | None | *No Plant Controller currently active* (Config C) |
| PartnerLink unavailable | W3 unrealized | W1, W2, W4, W5–W10 all fully | Peer coordination channel and some failover confidence | **None**, and not automatically | *PartnerLink unavailable; both controllers reachable on container control network; Plant A active, term N* |
| Safety Controller unavailable | W2 realized on all five segments, zero live Origins | Everything else; non-safety state exchange continues | Independent shutdown, isolation, inhibit authority | None | *Safety authority unavailable* — distinct from *safety network unavailable* |
| Rack Gateway unavailable | W5–W9 unreachable; W2's rack and cabinet realization gone; W4's leaf reach gone; W1 loses one Origin and one sink | W1 Plant↔PCS coordination, W10 power stage, W3 | Rack coordination, all downstream rack/thermal/cabinet reach, gateway local protection, and the container time source | **None.** Time-source ownership does not transfer | *Rack Gateway unavailable; rack, thermal, and cabinet subsystems unreachable; container time source lost, all participants on local monotonic time with degraded timestamp quality* |
| Rack CAN 2 isolated | W6 unrealized; W2 and W4 lose that segment | W5, W7, and everything else | Rack group 2 reach and capability | None | Config D statement |
| PCS power-stage CAN-FD fails | W10 unrealized; W4 loses module reach | W1, W2, W3, W4 on the LAN; all rack, thermal, cabinet traffic | Module-level command and status reach | None | *PCS power-stage segment failed; container cannot perform normal power conversion; PCS Controller itself reachable* — distinct from *PCS Controller unavailable* |

## 9.3 The four statements the brief requires to stay distinct

| Required statement | What makes it distinguishable |
|---|---|
| *Plant A unavailable as accepted normal-control authority* | W1 realized with permitted Origins {1, 2, 4, 5}; term state names Participant 2, or names none. A statement about accepted Origin, not about the Wire |
| *No Plant Controller currently active* | W1 realized and carrying traffic from Origins 4 and 5; Origins 1 and 2 not live. A statement about Origin liveness within a healthy Wire |
| *Safety authority unavailable* | W2 realized on every one of its five segments, its sole permitted Origin not live. Distinct from any segment loss |
| *Rack CAN 2 isolated* | One Wire unrealized plus a named-segment realization loss on two container-spanning Wires. Never a container-wide condition |

None reduces to a generic Wire-down condition. The separating mechanism is worth naming, and the merges sharpened it:

```text
statements about Origin population      W1 has no live Plant Origin
                                        W2 has no live Origin at all
statements about physical realization   W6 unrealized
                                        W2/W4 lost one segment of five/seven
```

Under revision 1 the first kind was partly expressible as *a whole Wire went quiet*. Under the coarser topology it is not — the Wire stays busy while a specific authority disappears from it. That is the correct and more precise model, and it relocates the requirement onto tooling.

## 9.4 The gap, now more central than in revision 1

**WireSpaces has no native notion of per-permitted-Origin liveness.** A Wire whose only permitted Origin has stopped publishing is, at the Wire level, simply quiet; a Wire that has lost two of four permitted Origins is not quiet at all. Both of the crispest degraded statements in this machine — *Safety authority unavailable* and *no Plant Controller currently active* — are statements of exactly that form.

The mapping recovers them from platform health (P2 heartbeat) plus authored knowledge of each Wire's permitted-Origin set. That works, and it is how the brief's requirements are met. But it means the operator-facing degraded model is assembled by tooling from two sources rather than read from one, and the coarser 10-Wire topology increases the reliance rather than reducing it. Carried to §11.4.

---

# 10. Friction ratings

Scale: None / Minor / Moderate / Significant.

| Signal | Rating | Justification |
|---|---|---|
| **Artificial Origin** | **None** | Every Origin in the mapping is a native initiator named by §4.5. No coordinator, arbiter, or proxy was invented. W3 `PlantPartner` is the specific case where an artificial Origin would otherwise have been needed and is not |
| **Artificial Wire** | **None** | Revision 1's single artificial Wire is gone. Each of the ten Wires corresponds to either one physical segment or one distinct multi-segment reach; no two share both membership and realization |
| **Wire proliferation** | **None** | 10 Wires for 8 segments, 25 participants, and 11 required application services. Six are one-per-segment, two are distinct multi-segment reaches, one is the PartnerLink, one is the container coordination scope. No Wire exists to name a permitted Origin, and no Service has its own Wire |
| **Forwarding tax** | **Moderate** | Two container-spanning Wires forwarded through both gateways; the Rack Gateway's table is 2 Wires × 5 downstream interfaces in each direction, and every constrained bus carries 2 forwarded Wires plus its own. Reduced from revision 1 by the `Platform` merge. Rated Moderate rather than Minor because it is the dominant per-carrier cost — though most of it is machine-inherent (one gateway, five downstream segments) rather than WS-induced |
| **Identity awkwardness** | **Minor** | Global `ParticipantId` removed per-Wire renumbering entirely and made the multi-segment safety Wire's identities unique by construction. Nothing is awkward. Rated Minor only for the consumption datum: 25 identities per container, scaling with Endpoint Domains rather than devices |
| **Interaction awkwardness** | **Minor** | Two rough spots. Cabinet fire and smoke detection reaches Safety only through gateway composition, because §4.5 authorizes no cabinet-to-Safety initiation (§11.7). And Safety is forwarded to rack leaves but composed at the PCS toward modules — an asymmetry the machine specifies, but one that reads as inconsistent until the authority table is consulted |
| **Configuration burden** | **Moderate** | Authored intent is compact: 10 Wire statements, 9 Endpoint classes, one class per Endpoint. The generated projection is ~200 Endpoint tuples plus forwarding and membership tables, machine-checked. Moderate rather than Minor because the audit is irreducibly two-layer — the acceptance predicates in two of nine classes are outside anything the Organizer can evaluate (§5.4) |
| **Role instability** | **Minor** | The active Plant-control role transfers at runtime, so this machine has genuine role instability and not merely per-interaction Origin variation. But the instability is contained entirely in Service-level term state: Wire membership, permitted-Origin sets, Participant identity, routes, and Endpoint bindings all remain static across failover. That containment is one of the strongest positive results in the sketch, and Minor rather than None records that the underlying native role does move |
| **Failure mismatch** | **Minor** | All four frozen configurations and all eight matrix rows map to distinguishable statements, and the four the brief demands stay distinct do stay distinct. Rated Minor for one structural gap: *this Wire has no live permitted Origin of class X* is not a WireSpaces-level condition, and the coarser topology makes the mapping depend on it more, not less (§9.4) |

**No claim of simplification is made from Wire-count reduction.** The reduction from 12 to 10 came from applying a stricter splitting rule, and it did not remove any native distinction — it relocated two of them into Endpoint authorization and one into per-Origin liveness reporting, and §11.1 and §11.4 count both. The place where structure was genuinely eliminated rather than relocated remains narrow and specific: **the multi-Origin `ContainerCoordination` Wire, which removes the second dispatch Wire or the runtime Origin reassignment that redundant controllers would otherwise force, and `PlantPartner`, which removes an artificial coordinator.**

---

# 11. Model pressure and open questions

## 11.1 Coarsening the Wires moved authority *out* of the allow-set, not into it

This is the finding revision 2 changes most, and the change strengthens the result.

| | Wires | Allow-set restricts on |
|---|---:|---:|
| Revision 1, finer topology | 12 | 4 |
| Revision 2, stricter splitting rule | 10 | 2 |

Removing two Wires did not redistribute their authority into the remaining permitted-Origin sets. It pushed that authority entirely into Endpoint authorization classes: firmware-update authority left the Wire layer completely when `FirmwareUpdate` merged into `Platform`, and plant-versus-capability authority left it when `PlantControl` merged into `ContainerCoordination`. Eight of ten Wires now name every member as a permitted Origin.

The conclusion is uncomfortable for the per-Wire allow-set as a base-model feature: **the coarser the Wires, the less the allow-set does.** In this machine it survives in exactly two places — Safety's exclusive origination on `SafetyAuthority`, and Safety's exclusion from originating ordinary coordination. Both are genuinely useful, and both are one-line statements. But two useful statements out of ten is a weak case for requiring the mechanism in the base deployment model, and a good case for it being optional policy that a deployment uses where it happens to fit.

The counter-consideration is that the remaining two are the *safety* ones, which is not a coincidence: the allow-set does its best work where a single participant holds an exclusive, always-valid authority that no runtime predicate qualifies. That is a narrow but important shape, and it may be the honest scope of the feature.

## 11.2 Endpoint-level Origin admission is where authority actually lives

Eight of nine authorization classes are Endpoint-scoped, and the three-way precedence at each rack manager is unstatable without distinct Endpoints per authority source. The premises assume this mechanism exists ("Wire and Endpoint/Service policy") but the architecture has no first-class name for it. It needs one, along with a decision about whether it is authored per Endpoint or per class — the class form is what made the audit compact here, and a per-Endpoint form would have produced roughly 200 authored rules instead of 19.

## 11.3 `permitted Origin` and `accepted Origin` should both be named; only the first is a configuration concept

The distinction (§5.4) is what kept the authority model auditable. Its properties, as this machine exercises them:

- `permitted Origin` is static, topological in the sense that it is checkable against Wire membership, and completely Organizer-verifiable;
- `accepted Origin` is Service state at the receiving actuator, is not topological in any respect, and is not statically evaluable in principle rather than merely in practice.

The recommendation from this sketch is that **`accepted Origin` should not become a Wire concept.** What deployment tooling plausibly should be able to do is *declare that an acceptance predicate exists* on an Endpoint without evaluating it, so a generated authority report distinguishes "Participant 1 or 2 may command this" from "Participant 1 or 2 may command this, subject to a runtime predicate this tool cannot check." Whether that declaration is a `REG`-level concept, a Manifest field, or Service documentation is open.

## 11.4 Per-permitted-Origin liveness has become the main tooling requirement

Section 9.4. Under the coarser topology, two of the four degraded statements the brief demands are statements about *which permitted Origins of a live Wire are currently live*. WireSpaces has no such notion; the mapping assembles it from platform heartbeat plus authored Wire configuration.

This connects directly to the coarse-Wire question. **The coarser the Wires, the more the degraded model depends on per-Origin liveness rather than on Wire realization state.** A finer topology lets an operator read authority loss off Wire silence; a coarser one requires tooling to correlate. If coarse Wires are the recommended direction, per-Origin liveness reporting stops being a nice-to-have.

## 11.5 Composition concentrates term validation; forwarding would distribute it

Choosing composition for all Plant-to-rack application traffic put the current-term check at one participant (§6.2). Forwarding would have put it at twelve rack managers, each tracking plant term state. The first is a single point of correctness; the second is twelve implementations of the same check on constrained leaves. The mapping chose the first, and the brief does not settle which is wanted. The general question: **should acceptance predicates be evaluated at the composition boundary or at the actuator?** Section 4.6 says the receiving actuator owns source and term selection, which reads as an argument for the second, but applying it literally would push plant-term awareness onto every leaf in the container.

## 11.6 Container-spanning Wires should default to addressed, not broadcast

Only time synchronization legitimately wants all 25 participants (§7.1). Every other platform interaction is unicast to a collector, and leaf heartbeats were deliberately kept off the container-spanning Wire entirely. A Wire whose membership spans seven segments is a poor broadcast domain and a fine addressing domain. Revision 2 reinforces this from the other direction: removing the invented peer broadcasts left nine of ten Wires carrying predominantly addressed traffic, which shows that multi-Origin does not imply broadcast-heavy traffic. Whether "large Wires default to addressed" should be a stated deployment guideline, or something tooling warns about, is open.

## 11.7 Machine-description gap: cabinet safety detection has no authorized path to Safety

Section 4.5 gives cabinet I/O controllers no initiator row, so smoke and fire detection reaches the Safety Controller only through Rack Gateway composition into A7. That puts gateway availability inside a safety-relevant *detection* path, while §4.5 deliberately keeps Safety's *command* path direct. Two candidate resolutions, neither adopted because the machine description does not settle it: permit Participants 61 and 62 as additional Origins on W2 for detection-only Endpoints, which dilutes W2's single-authority property and its clean degraded statement; or accept the composed path and document the gateway as being in the fire-detection path. Flagged, not resolved.

## 11.8 Two delivery questions the brief does not freeze

Recorded rather than answered, after revision 2 removed the inferred peer consumers:

- Whether synchronized multi-module action (simultaneous inhibit across power modules) requires a broadcast form of A14, or whether addressed commands with adequate timing suffice. The brief describes module commands without specifying either.
- Whether group contactor sequencing across racks on one bus requires peer visibility of A8, or whether the Rack Gateway sequences them individually. The brief describes rack managers reporting upward only, so individual sequencing is assumed.

Both would restore a broadcast if the machine turns out to require them; neither affects the Wire structure.

## 11.9 Split-brain is bounded by sink-side rejection, and WireSpaces contributes nothing to it

With PartnerLink lost, nothing in WireSpaces prevents both Plant Controllers from claiming conflicting terms — no election, no failover, no arbitration, per `SVC-1`. Both remain permitted Origins on W1 by authored configuration. What bounds the damage is entirely the sink-side rule: Participants 4 and 5 accept the highest observed term and reject the rest. This is correct as an architectural stance, but it means the safety of the redundancy scheme rests on an acceptance predicate that no configuration audit can verify. Machine-owned, worth stating rather than assuming.

## 11.10 Data for the deferred encoding candidate

Recorded, not designed, since carrier projection is out of scope.

- Each rack CAN carries **3 distinct Wires** (W2, W4, and its own) with 5 participants; the cabinet I/O CAN the same; thermal and power-stage carry 2 each. Any constrained-carrier Wire representation must accommodate at least three concurrent Wires per bus in this machine — down from four before the `Platform` merge, which is a small argument that coarser Wires ease the constrained-carrier problem.
- Carrier-local participant projection is needed per rack bus for 5 participants each, and the three buses may reuse the same compact range because they are separate carriers, while `ParticipantId` 11–14, 21–24, 31–34 stay globally distinct. This machine exercises the proposal's §2.3 correction three times over.
- Per-PDU overhead of carrying an explicit Origin identity is not assessable here — no shared-memory or inter-core segment exists in this machine.

## 11.11 What this machine does not exercise

Stated so the sketch is not read as broader evidence than it is: no shared-memory or inter-core Link, no device with multiple Endpoint Domains, no device-private Wire, no splice, no non-WS leaf requiring gateway adaptation, and no multi-container or site-level scope. The identity-consumption and descriptor-overhead questions in the change proposal are only partly informed by this sketch.

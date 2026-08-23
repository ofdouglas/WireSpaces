# Sketch 06 — Dual Redundant Multicore Gateways

**Mapping:** Two three-Domain gateways each originate a separate plant-control Wire; every field device is a Node on both, while each gateway exclusively owns two fieldbuses and forwards its partner's Wire to those buses through a shared partner/service Ethernet switch.
**Worst friction:** Configuration burden — Mild.
**Main lesson:** Two controllers and two logical command Wires do not by themselves provide redundant delivery; source selection, correlation, per-sink coverage, partner health, and the exclusive-I/O failure boundary remain explicit composition and tooling responsibilities.
**Configs:** A — two mirrored gateways with a 2+2 fieldbus split and one service PC.

**Intent:** Rework sketch 05 into a dual-controller system and test whether WireSpaces describes controller redundancy, asymmetric physical ownership, partner forwarding, maintenance access, observation, and degraded operation without hiding them inside one multipath Wire.

This remains an architecture sketch, not a safety case. The redundancy policy is intentionally bounded but not fully designed; the base Router never performs Origin election, duplicate suppression, or failover.

---

# Configuration A — Mirrored control, exclusive 2+2 I/O

## 1. What changed and maturity

Relative to sketch 05:

- Gateway A and Gateway B each contain Control, Field I/O, and Host/Diagnostics Endpoint Domains.
- Gateway A alone is physically attached to DriveBus and SafetyIOBus.
- Gateway B alone is physically attached to SensorBus and VisionBus.
- Both Host Domains attach to one small partner/service Ethernet switch; the service PC attaches to the same switch.
- Each Control Domain has its own plant-command authority and therefore its own Wire.
- The gateways forward their partner's plant Wire to the fieldbuses they physically own.
- Partner health, command-source selection, observation, and coverage are explicit rather than inferred from duplicated controllers.

**Maturity level: Level 3.** The system requires static generated Wiring, synchronized projections for two gateways, explicit configuration fingerprints, resource and timing claims, and reviewed redundancy composition. It does not define a formal WireContract or safety profile.

## 2. Native communication model

Without WireSpaces terminology:

- Gateway A and Gateway B run equivalent machine-control applications.
- At any instant, product policy selects which controller's commands a field device may act on.
- The standby controller keeps enough current state to take over within a declared bound, but it does not become active merely because one heartbeat was late.
- Gateway A owns the physical interfaces for drives and safety I/O.
- Gateway B owns the physical interfaces for sensors and vision.
- To reach a bus it does not own, a controller sends through the partner Ethernet connection and the owning gateway's surviving Host/I/O path.
- Each field device publishes its state once. Both controllers receive it: the owner-side controller is the required sink and the partner receives a configured observation copy.
- The service PC discovers two physical gateways, their six Domains, four fieldbuses, two controller health streams, and the actual failure coverage.
- Failure of one Control Domain may be covered if that gateway's Host/I/O Domains and the shared switch still forward partner traffic.
- Failure of an entire gateway removes physical access to its two exclusive fieldbuses. The partner has no alternate transceiver.
- Failure of the small Ethernet switch removes partner forwarding and PC access; it does not remove each gateway's local control of its own two buses.

The system therefore has controller redundancy under some faults, but not redundant fieldbus attachment, not redundant maintenance access, and not a redundant partner network.

## 3. Obvious conventional implementation

> **Obvious conventional implementation:** two multicore controllers exchange heartbeat/state over Ethernet, run an active/standby or source-selection protocol, proxy commands to each other's locally owned fieldbuses, and expose both devices through a service switch; field ECUs implement command-source arbitration and freshness checks.

That conventional system already needs:

- two command-source identities;
- partner liveness and mode exchange;
- command epoch/sequence/freshness rules;
- deterministic source selection at each actuator;
- duplicate and stale-command handling;
- proxy routing to the non-owning gateway;
- a per-fault coverage matrix;
- service discovery of two devices and several software partitions;
- synchronized configuration across both gateway images and all field devices.

WireSpaces makes the two command authorities, partner forwarding, observation branches, and maintenance scope explicit. It does not replace the redundancy policy.

## 4. WireSpaces mapping

### 4.1 Physical and Domain topology

```text
                         Service / partner Ethernet switch
                       /                 |                  \
              Service PC          Gateway A Host       Gateway B Host
                                      /      \              /      \
                                SHM A-HC   SHM A-HI    SHM B-HC   SHM B-HI
                                   |          |           |          |
                             Gateway A     Gateway A   Gateway B    Gateway B
                              Control         I/O       Control        I/O
                                   \          /           \          /
                                    SHM A-CI               SHM B-CI
                                          |                 |
                              exclusive: DriveBus      exclusive: SensorBus
                              exclusive: SafetyIOBus   exclusive: VisionBus
```

There is no physical connection from Gateway B to DriveBus or SafetyIOBus, and none from Gateway A to SensorBus or VisionBus. “Partner forwarding” means that a complete PDU crosses the Ethernet switch to the gateway that owns the required Link Interface.

Each gateway retains the three-Domain structure of sketch 05:

- **Control Domain:** machine Services and one plant-Wire Origin;
- **Field I/O Domain:** two local field LLLs and same-Wire forwarding;
- **Host/Diagnostics Domain:** partner Ethernet, persistent log, service access, and forwarding between partner and internal Links.

### 4.2 Devices and physical ownership

| Device / Domain | Endpoint Domains | Link interfaces | Role summary |
|---|---|---|---|
| Gateway A — Control | One AMP Domain | SHM A-Control/I/O; SHM A-Host/Control | Origin of PlantControlA and GatewaySupervisionA |
| Gateway A — I/O | One AMP Domain | SHM A-Control/I/O; SHM A-Host/I/O; DriveBus; SafetyIOBus | Exclusive owner of two field Links |
| Gateway A — Host | One AMP Domain | partner/service Ethernet; SHM A-Host/Control; SHM A-Host/I/O | Partner gateway, logger, service participant |
| Gateway B — Control | One AMP Domain | SHM B-Control/I/O; SHM B-Host/Control | Origin of PlantControlB and GatewaySupervisionB |
| Gateway B — I/O | One AMP Domain | SHM B-Control/I/O; SHM B-Host/I/O; SensorBus; VisionBus | Exclusive owner of two field Links |
| Gateway B — Host | One AMP Domain | partner/service Ethernet; SHM B-Host/Control; SHM B-Host/I/O | Partner gateway, logger, service participant |
| Service PC | One host-tool Domain | partner/service Ethernet | Origin of ServiceWire; no control-failover authority |
| Drive and safety ECUs | One Domain each | Gateway-A-owned fieldbus | Nodes on both plant-control Wires and ServiceWire |
| Sensor and vision devices | One Domain each | Gateway-B-owned fieldbus | Nodes on both plant-control Wires and ServiceWire |

Stable device identity must group three Domains under Gateway A and three under Gateway B while preserving unambiguous Service instances (SF-019).

### 4.3 Plant-control Wires

| Wire (name / #) | Origin | Nodes | Physical realization | Notes |
|---|---|---|---|---|
| PlantControlA / assigned | Gateway A Control | All field devices | A internal Links + A-owned buses; A Host → switch → B Host/I/O + B-owned buses | Gateway B Control receives configured observation |
| PlantControlB / assigned | Gateway B Control | All field devices | B internal Links + B-owned buses; B Host → switch → A Host/I/O + A-owned buses | Gateway A Control receives configured observation |

Two Wires are mandatory because the two Control Domains are independent command producers. Modeling them as two Origins on one Wire would violate the base invariant.

The four fieldbuses do not automatically become four operational Wires here. Each controller authority spans all field devices. Subsystem-scoped Wires remain a valid alternative if broadcast membership or Service policy needs narrower recipient domains; the one-versus-four question from sketch 05 is intentionally not resolved by physical bus count.

Each physical fieldbus carries both PlantControlA and PlantControlB:

- commands from A use PlantControlA;
- commands from B use PlantControlB;
- devices accept both Wires structurally, while a bounded Service-level selector decides which source is currently actionable;
- A-owned devices publish ordinary telemetry once on PlantControlA;
- B-owned devices publish ordinary telemetry once on PlantControlB;
- the non-owning Control Domain receives those publications through a configured observation branch.

This telemetry convention avoids requiring one device transmit Endpoint to fan out onto two Wires. It is an ownership convention, not a claim that the owner-side controller is always healthy.

### 4.4 Maintenance, partner status, and internal Wires

| Wire (name / #) | Origin | Nodes | Physical realization | Notes |
|---|---|---|---|---|
| ServiceWire / assigned | Service PC | Both gateways and all field devices | Switch branches to both Hosts; each Host/I/O path reaches only its two local buses | One PC authority; no cross-partner forwarding needed |
| PartnerStatusA / assigned | Gateway A Control | Gateway B Control | A Host/Control + switch + B Host/Control | A heartbeat, mode, epoch, and health claims |
| PartnerStatusB / assigned | Gateway B Control | Gateway A Control | Mirrored path | B heartbeat, mode, epoch, and health claims |
| GatewaySupervisionA / device-private | Gateway A Control | Gateway A I/O and Host Domains | A Control/I/O + A Host/Control SHM Links | Internal restart/status authority; never external |
| GatewaySupervisionB / device-private | Gateway B Control | Gateway B I/O and Host Domains | B Control/I/O + B Host/Control SHM Links | Mirrored internal relationship |

PartnerStatusA and PartnerStatusB are separate because each gateway authors its own health and mode claims. They share one Ethernet switch, so two logical Wires do not provide physically independent status delivery.

The service PC sees partner status through configured observation or explicit diagnostic Endpoints; observation grants no control authority. The PC never decides which controller is active.

### 4.5 Per-Wire acyclic projections

PlantControlA forms this tree:

```text
                         Gateway A Control (Origin)
                                  |
                           A Control/I/O
                                  |
                             A I/O Domain
                       /          |           \
                  DriveBus   SafetyIOBus   A Host/I/O
                                               |
                                             A Host
                                               |
                                             switch
                                               |
                                             B Host
                                           /        \
                                    B Host/Control  B Host/I/O
                                         |             |
                                    B Control tap     B I/O
                                                   /        \
                                              SensorBus   VisionBus
```

PlantControlB is the mirrored tree. A PDU arriving from the partner is never forwarded back onto the partner Link. The two trees may use the same Physical Links in opposite logical directions without creating a forwarding cycle because routing is keyed by Wire and ingress.

ServiceWire is a separate tree rooted at the PC:

```text
                             Service PC (Origin)
                                      |
                                    switch
                                  /        \
                              A Host      B Host
                              /   \        /   \
                        A Control A I/O  B Control B I/O
                                  / \              / \
                              A buses          B buses
```

Each gateway routes ServiceWire only to its locally owned fieldbuses. It does not send ServiceWire across the partner switch a second time.

### 4.6 Forwarding projection — Gateway A

Gateway B has the exact mirrored projection.

| Ingress link / source | Wire | Egress link(s) | Local delivery? | Notes |
|---|---|---|---|---|
| A Control local TX | PlantControlA | A Control/I/O | N/A — locally produced | A I/O branches to local buses and toward the partner |
| A Control/I/O at A I/O | PlantControlA | selected A-owned fieldbus; A Host/I/O → switch toward B | Only explicit I/O endpoints | Same Wire reaches A buses, B-owned buses, and B observation through one acyclic tree |
| A-owned fieldbus | PlantControlA | A Control/I/O toward A Control; A Host/I/O → switch for B observation | Only explicit I/O endpoints | Required-sink and observation egresses are independent |
| Switch at A Host | PlantControlB | A Host/Control for A Control observation; A Host/I/O for A-owned target buses | Only explicit Host endpoints | Partner command/status enters once |
| A Control local TX | PlantControlB | None | Not permitted as Origin traffic | A is a Node/observer on B's Wire, not a second Origin |
| A-owned fieldbus | PlantControlB | A Host/I/O → switch toward B Origin; optionally A Control observation | Only explicit I/O endpoints | Replies/telemetry on B Wire return toward B |
| Switch at A Host | ServiceWire | A Host/Control and/or A Host/I/O | Yes when A Host is target | Destination map prunes local branches |
| A-owned fieldbus or A Control | ServiceWire | A Host → switch | No unless explicit observer binding | `NodeToOrigin` response returns to PC |
| A Control local TX | PartnerStatusA | A Host/Control → switch | N/A — locally produced | A status toward B |
| Switch at A Host | PartnerStatusB | A Host/Control | Optional Host log | B status terminates at A Control |

At A I/O, PlantControlA telemetry has independent required-sink and partner-observation egresses. Failure of either copy does not roll back the other, and a dead observation path must not backpressure Control delivery.

### 4.7 Redundancy composition

The Router sees ordinary Wires only. The redundancy component flattens to receive bindings on PlantControlA and PlantControlB plus bounded local state (`CORE §19.2`, §23.11).

| Responsibility | Explicit owner in this sketch | Required behavior |
|---|---|---|
| Message-instance correlation | Command Service / generated redundancy wrapper | Controller epoch + sequence identify one logical command instance |
| Duplicate suppression | Field-device command selector | At most one accepted action for one correlated command |
| Stale-copy rejection | Field-device command selector | Reject old epoch, old sequence, or expired freshness window |
| Active/standby selection | Product policy in field-device selector, informed by partner status | Never inferred by Router reachability alone |
| Per-Wire health | Each gateway's Link Telemetry plus PartnerStatusA/B | PlantControlA and PlantControlB tracked independently |
| Per-sink coverage | Deployment coverage matrix | State exactly which faults leave each field device commandable/observable |

An observation branch is not redundant delivery:

- A device telemetry PDU is one message on one owner Wire.
- The partner receives a forwarded copy of that same path.
- Loss of the owning fieldbus or owning gateway removes both views.
- The observation copy provides awareness, not an independent delivery member.

Similarly, accepting commands on two Wires is controller-source redundancy, not path redundancy. Both commands reach a field device through the same exclusive fieldbus and owning I/O Domain.

### 4.8 Coverage matrix

| Sink / capability | Normal A path | Normal B path | Covered fault | Not covered |
|---|---|---|---|---|
| A-owned field devices receive commands | A Control → A I/O → A bus | B Control → B I/O/Host → switch → A Host/I/O → A bus after source selection | A Control failure while the B source path and A Host/I/O survive | Whole Gateway A or A fieldbus failure; B I/O/Host/switch failure removes the standby path |
| B-owned field devices receive commands | A Control → A I/O/Host → switch → B Host/I/O → B bus when A is selected | B Control → B I/O → B bus | B Control failure while the A source path and B Host/I/O survive | Whole Gateway B or B fieldbus failure; A I/O/Host/switch failure removes the standby path |
| Both Controls receive A-bus telemetry | A bus → A Control; independent A I/O → A Host → switch copy to B | Same single publication | A Control failure while A I/O/Host and switch survive | A bus or whole Gateway A failure |
| Both Controls receive B-bus telemetry | B bus → B Control; independent B I/O → B Host → switch copy to A | Same single publication | B Control failure while B I/O/Host and switch survive | B bus or whole Gateway B failure |
| Service PC reaches all devices | Switch → both gateway branches | No independent path | One Control Domain failure | Switch failure; owner gateway failure removes its two buses |
| Partner status | PartnerStatusA and PartnerStatusB | Same switch, separate producers | One status producer failure is observable from the other | Switch failure removes both status paths |

The matrix is part of system truth. Neither duplicate cabling in a diagram nor membership on two Wires may be counted as coverage without a row demonstrating independent delivery.

### 4.9 Timing and failover intent

Illustrative bounds, not a safety contract:

- field commands carry end-to-end sequence, source epoch, and freshness metadata;
- a field device never applies a command after its freshness limit merely because a reliable transport delivered it;
- partner status is a Snapshot with an arrival time and restart generation;
- command-source switching has an explicit maximum detection and activation interval;
- switching does not rewrite either Wire's Origin or NodeIds;
- queued commands from the former source are rejected after an epoch/source change;
- partner forwarding queues have bounded capacity and expose high-water/congestion telemetry;
- the shared switch's latency and failure are included in non-owner command budgets;
- firmware update remains Background reliable-segment traffic and is paused during control-path pressure.

No universal failover timeout is invented here. The product must derive it from control-loop and actuator-safe-state requirements.

### 4.10 Failure behavior

- **A Control Domain fails:** B may become selected command source. B reaches A-owned buses only while A Host, A I/O, their SHM Links, and the switch survive.
- **Whole Gateway A fails:** DriveBus and SafetyIOBus are inaccessible. Gateway B cannot route around missing transceivers.
- **One A field LLL fails:** only that field path fails if A's shared resources permit isolation.
- **Partner/service switch fails:** gateways lose cross-control, partner status, and service PC access. Each may continue only the local behavior permitted without partner state; split-brain prevention is product policy.
- **One Host Domain fails:** that gateway loses partner forwarding and service access, but its Control/I/O direct path may continue local operation.
- **One I/O Domain fails:** both fieldbuses owned by that gateway fail together; in the chosen tree it also breaks that controller's plant-Wire branch toward its partner, which the coverage model must report.
- **One Plant Wire Origin is offline:** the Wire and role remain configured; Nodes may still publish and configured observers may still consume.
- **Configuration mismatch:** a gateway reports its own fingerprint and fails closed or enters an explicitly configured degraded mode; it never adapts aliases or source authority from observed partner traffic.

## 5. System-builder service view

The PC is attached to the switch as a maintainer of the assembled system, not as a hidden redundancy coordinator.

It should display:

```text
Physical devices:
    Gateway A, Gateway B

Endpoint Domains:
    A Control, A I/O, A Host
    B Control, B I/O, B Host

Exclusive field ownership:
    A -> DriveBus, SafetyIOBus
    B -> SensorBus, VisionBus

Controller authorities:
    PlantControlA, PlantControlB

Partner health:
    PartnerStatusA, PartnerStatusB

Coverage:
    per sink, per failure, with unsupported cases explicit

Common dependencies:
    partner/service switch
    per-gateway shared clocks/RAM/DMA/restart units
```

The UI must not:

- collapse six Domain telemetry Services into one healthy/failed icon;
- show an observed PDU as a redundant copy;
- label a fieldbus “dual attached” because both plant Wires reach it;
- imply that PC reachability equals control coverage;
- infer active authority from whichever Wire produced traffic most recently.

## 6. Rejected alternatives

| Alternative | Benefit | Why it is not the primary mapping |
|---|---|---|
| One PlantWire with Gateway A and B both as Origins | Looks like one redundant plant network | Violates exactly-one-Origin and makes source authority ambiguous |
| Change one PlantWire's Origin during failover | Fewer Wires | Base WS has no Origin election; role mutation invalidates static authority and in-flight assumptions |
| Re-originate partner commands at the owning gateway | Hides partner routing | Changes producer lineage and turns a transparent gateway into a command authority |
| Forward both plant Wires around a cyclic Ethernet/SHM graph | Appears to add path redundancy | Base routing is acyclic and has no duplicate suppression or hop count |
| Treat Host observation as redundant telemetry delivery | Easy coverage claim | The copy shares the source fieldbus and owning gateway path |
| Publish every field snapshot on both plant Wires | Both Origins receive directly | Requires duplicate transmit bindings, correlation, and suppression; one owner-Wire publication plus observation is sufficient here |
| One ServiceWire per gateway | Mirrors two switch ports | Same PC authority and device set; creates physical-link-shaped maintenance Wires |
| Let service PC select the active controller | Convenient during testing | Grants a development tool production control authority and creates a switch-dependent control plane |
| Physically attach both gateways to all four buses | Genuine path redundancy | Violates the required 2+2 exclusive split and changes the fault-containment experiment |

## 7. Friction signals

| Signal | Rating | Reason |
|---|---|---|
| Artificial Origin | None | A, B, and the service PC are real independent authorities for their respective Wires |
| Artificial Wire | None | Dual plant Wires, partner status, maintenance, and internal supervision correspond to distinct relationships |
| Wire proliferation | Mild | Redundant controller authority necessarily duplicates the plant Wire and adds explicit status Wires, but the structure is symmetric and generated |
| Forwarding tax | None | Non-owner bus traffic must cross the owning gateway in any implementation with exclusive 2+2 attachment |
| Identity awkwardness | Mild | Two physical devices contain six Domains and repeated standard Services, retaining the SF-019 instance-addressing pressure |
| Interaction awkwardness | Mild | Field Services need dual command-source bindings and bounded selection state; this is inherent redundancy work but not supplied by the base Wire |
| Configuration burden | Mild | Mirrored route trees are regular, but coverage, fingerprints, exclusive ownership, and partial-domain failure dependencies require system-level review |
| Role instability | None | Active source changes in Service policy while both Wire Origin roles remain fixed |
| Failure mismatch | None | The coverage matrix states that exclusive field attachment defeats whole-gateway path redundancy |

The sketch is complex because the system is complex. The chosen mapping remains auditable because each Wire is acyclic and every redundancy claim points to explicit composition and coverage state.

## 8. Model pressure

- **If one WireSpaces concept could change:** no Router or Wire change is justified. Tooling needs a redundancy-composition template that expands into two ordinary Wires, dual receive bindings, partner-status bindings, bounded selector state, and a coverage matrix. The flattened data plane remains unchanged.
- **Useful distinction exposed by WS:** controller-source redundancy, route/path redundancy, observation, and physical fieldbus ownership are four different claims. The explicit Wire model prevents a system builder from counting one as another.

## 9. Open questions

- What standard Service/Transport contract carries source epoch, command sequence, freshness, and source-selection state for replicated controllers?
- Is active-source selection configured independently at every field device, generated as one common policy, or delegated to a dedicated protected component?
- How does tooling prove that the mirrored per-Wire route trees are acyclic while still permitting opposite directions over the same partner Link?
- What minimum coverage schema distinguishes Control-Domain failure, Host/I/O partial survival, whole-gateway loss, fieldbus loss, and switch loss?
- How are configuration fingerprints scoped so both gateways agree on aliases, field ownership, source policy, and partner routes without coupling unrelated internal settings?
- Should partner status use two one-Origin Wires as shown, or one deliberately asymmetric Wire with explicit bidirectional Service semantics?
- How are six per-Domain telemetry instances addressed and grouped under two stable physical device identities (SF-019)?
- Which observation branches are permitted to drop independently under congestion, and how is loss shown without degrading control delivery?
- Does the service switch need a separate production partner Link in any deployment claiming whole-gateway failover?

## 10. Spec findings

- **SF-001 / SF-016:** configured observation is useful for partner awareness but does not constitute redundancy coverage.
- **SF-012:** a failed controller does not cause Origin role reassignment.
- **SF-019:** six Domain-local Service instances reinforce the need for an explicit network-visible instance rule.
- **SF-021:** two controller-Origin Wires require an explicit receiver-side source-selection composition; the Router cannot infer active authority.
- **SF-022:** exclusive fieldbus ownership bounds redundancy coverage even when both controller Wires reach every Node.


# Sketch 12 — Modular Grid Battery Storage, Field-Serviced and Multi-Vendor

**Archetype:** `archetypes/12_modular_bess_field_serviced.md`  
**Agent:** `gpt56sol_20260823`  
**Output file:** `sketches/12_modular_bess_field_serviced_gpt56sol_20260823.md`  
**Date:** 2026-08-23

---

## Executive summary

**Minimum mapping:** Config B conditionally maps 187 Endpoint Domains onto 21 named Wires: 20 rack Wires over committed CAN11 plus one container Wire over CAN-FD and Ethernet.  
**Question A (R6):** **Poor for the stated product** — the bus/Wire model is natural, but the required single identity universe cannot be constructed from repeated and colliding vendor-assigned ParticipantIds without renumbering or an identity translator.  
**Question B (CAN11):** **Default** — each nine-participant rack uses one default-map alias and 9 of 31 usable ordinary VCNs; CAN11 is not the limiting factor.  
**Worst friction (minimum path):** **Configuration burden — Significant**; even the conditional one-container map has at least 649 rack/container-specific topology facts before Endpoint and traffic declarations.  
**Main lesson:** The rack data plane is a strong repeated CAN11 fit, but current R6 does not supply the identity-assignment, translation, and lifecycle machinery needed to turn generic multi-vendor inventory into that data plane.

---

## Disposition block

| Area | Assessment |
|---|---|
| Participant identity | Poor |
| Wire decomposition | Natural |
| Forwarding | Moderate |
| Non-CAN configuration | High |
| CAN11 VCN fit | Default |
| Better with CAN29? | No for Config B rack buses; yes if a single global Config C is forced |

**Question A (R6):** Participants, rack-shaped Wires, forwarding, and composition are understandable once unique deployment-global identities are assumed, but that assumption is false for the supplied hardware and the ordinary ParticipantId range fails at plant scale. **Question B (CAN11):** the default VCN map fits every rack without a custom table; CAN29 would not cure vendor identity collisions, although a richer profile would be needed if Config C adopted wider canonical identities. The two results are independent.

---

## Falsification criteria

I will judge R6 identity **DEFECTIVE for this domain** if:

- a conforming full-container deployment requires Vendor A or Vendor B to change a factory ParticipantId that the archetype says is immutable;
- two independently assigned containers cannot be joined while preserving canonical Participant and Service identity, and the only missing mechanism is the identity-translating gateway already deferred by `CORE §4.6`;
- a replacement device cannot take over a stable functional role while tooling can still distinguish old and new physical devices and correlate history;
- the ordinary address range cannot represent the normal three-container product, with no currently specified profile transition that preserves the rack CAN11 deployment;
- repeated rack instances require configuration that grows linearly and lacks a safe offline commissioning procedure for generic inventory.

I will judge R6 identity **SOUND for this domain** if:

- factory identity remains intact while an explicitly specified commissioning mechanism assigns unique deployment-role ParticipantIds;
- the model defines a complete, checkable boundary for joining independent identity universes;
- role identity, physical UUID, replacement history, and version capability remain distinguishable through all Config D events; and
- Config C fits a supported canonical/profile combination without hidden per-rack renumbering.

The criteria are evaluated in §6.9 after the mapping and arithmetic.

---

## 1. Native communication model

One container has twenty independent battery racks. Each rack contains one rack controller and eight module monitors on its own 500-kbit/s Classical CAN bus. All rack controllers also connect to a shared container CAN-FD bus. The container controller coordinates racks, HVAC, auxiliary I/O, and PCS operation; container Ethernet connects it to the gateway and PCS inverter devices. Legacy fire/gas and revenue-meter equipment remains on Modbus RTU and is represented only by the controller or gateway that consumes it.

The repeated rack structure is fixed, but its physical instances are not: racks and monitors arrive from generic stock, are replaced in the field, and may be added years later. Vendor A, Vendor B, the integrator, and the plant operator assign identities independently. A three-container plant therefore combines systems that were commissioned at different times without a common identity plan.

---

## 2. Obvious conventional implementation

> **Obvious conventional implementation:** Keep each rack CAN database local and identical, address racks by physical slot on the container bus, use vendor-specific PCS identities unchanged, and expose container-level register/object models to the plant EMS.

> **What structure does a conventional design use?** Rack-local CAN IDs repeat safely because each RackCAN is a separate bus. The container controller maintains a slot-to-rack serial-number inventory, translates vendor object models, aggregates module state, and records replacement history by hardware serial number. The plant network addresses containers, not every internal module by one global 8-bit identity. Firmware tools traverse explicit rack/slot/module paths.

This conventional hierarchy makes identity contextual (`container/rack/module`) rather than deployment-global. It pays for gateway/proxy logic but avoids a 565-node flat short-ID universe.

---

## 3. Minimum mapping

### 3.0 Mapping status and boundary

There is **no valid full mapping of the stated Config B hardware into one R6 identity universe** if the vendor-assigned values in archetype §6 are canonical ParticipantIds and cannot be reassigned. `CORE §4.6` forbids treating plain forwarding or a splice as identity translation. The tables below are therefore the **minimum representational mapping conditional on unique deployment-role ParticipantIds**. They show that Wires, forwarding, and CAN11 work and isolate identity commissioning as the failed precondition.

The least-bad deployable scope under current R6 is:

1. one WireSpace per independently commissioned container;
2. container-level composition at `ContainerGateway` onto a separate plant WireSpace; and
3. no claim that plant EMS can transparently address every module Participant.

That scope satisfies dispatch, aggregate state, and container fault reporting, but it does **not** satisfy the required fleet-wide direct Identity/Health inventory or transparent selected-participant update. Those capabilities need explicit proxy Services or the unspecified identity translator; they cannot be described as forwarding.

### 3.1 Participants

#### Production counts

| Class | Config A: 4 racks | Config B: 20 racks | Config C: 3 containers |
|---|---:|---:|---:|
| Module monitors | 32 | 160 | 480 |
| Rack controllers | 4 | 20 | 60 |
| Container controller | 1 | 1 | 3 |
| Container gateway | 1 | 1 | 3 |
| PCS controller | 1 | 1 | 3 |
| PCS inverters | 2 | 2 | 6 |
| HVAC controller | 1 | 1 | 3 |
| Aux I/O | 1 | 1 | 3 |
| Plant EMS | 0 | 0 | 1 |
| **Explicitly named total** | **43** | **187** | **562** |
| Archetype scale target | ~43 | ~187 | ~565 |

The explicit archetype inventory yields 562, not 565. The three-node difference is treated as unknown plant inventory; no unnamed Participants are invented.

With 255 ordinary IDs (`0x00..0xFE`):

| Configuration | Explicit Participants | Ordinary range consumed |
|---|---:|---:|
| A | 43 | 16.9% |
| B | 187 | 73.3% |
| C | 562 | 220.4% |
| C, stated target | ~565 | ~221.6% |

One container with the seven fixed non-rack Participants can hold at most 27 nine-Participant racks: `floor((255 - 7) / 9) = 27`, producing 250 Participants. A 28-rack variant would require 259 and fail before reserving any commissioning or growth headroom.

#### Conditional Config B allocation

This is a role allocation that an Organizer could install only if all implementations accept deployment reassignment.

| ParticipantId(s) | Endpoint Domain | Device | Notes |
|---|---|---|---|
| `0x00` | container-control domain | `ContainerController` | Also composes FireGasPanel data |
| `0x01` | container-gateway domain | `ContainerGateway` | Container/plant composition boundary |
| `0x02` | PCS control domain | `PcsController` | Conditional canonical role, not Vendor B factory value |
| `0x03..0x04` | inverter domains | `PcsInverterA/B` | Two independent Endpoint Domains |
| `0x05` | HVAC domain | `HvacController` | |
| `0x06` | auxiliary domain | `AuxIO` | |
| `0x07..0x1A` | rack-control domains | `RackController_01..20` | One global role ID per rack |
| `0x1B..0xBA` | module-monitor domains | `ModuleMonitor_01_1..20_8` | 160 global role IDs |

Every rack controller uses the same ParticipantId on its rack Wire and `W_Container`; this is required by `CORE §3.1`. Hardware UUID remains separate from the role ParticipantId (`DEPLOY §1.6–1.7`).

### 3.2 Wires

| Wire (#) | Participants | Physical links | Purpose |
|---|---|---|---|
| `W_Rack_01` (`W01`) | RC01, MM01_1..8, ContainerController, ContainerGateway/service tooling | RackCAN_01; ContainerCAN; ContainerEth | Rack-local operation plus direct standard-Service reachability |
| `W_Rack_02..20` (`W02..W20`) | Corresponding RC and eight monitors, plus the same service endpoints | Corresponding RackCAN; shared ContainerCAN; ContainerEth | Same connected, loop-free shape repeated 19 times |
| `W_Container` (`W21`) | 20 RCs, ContainerController, Gateway, PCS controller/inverters, HVAC, AuxIO | ContainerCAN and ContainerEth | Container coordination, rack summaries, trip, PCS/HVAC/Aux interaction |
| `W_Plant` (separate plant universe) | Plant EMS and three container gateways | PlantEth | Container-level dispatch/state/faults through composition |

Each rack is a distinct propagation and failure domain, so twenty rack Wires are not artificial proliferation. A single Wire across all RackCAN buses would flood module traffic through nineteen unrelated 500-kbit/s buses and obscure rack isolation.

WireNumber consumption:

| Configuration | Conditional single-universe Wire instances |
|---|---:|
| A | 4 rack + 1 container = **5** |
| B | 20 rack + 1 container = **21** |
| C | 60 rack + 3 container + 1 plant = **64** |

Under the deployable separate-container fallback, each container consumes 21 values in its own universe and the plant consumes one; there are still 64 configured Wire instances, but the short numbers are not globally additive.

### 3.3 Interactions

| Interaction | Src | Dest | Wire | Broadcast? | Notes |
|---|---|---|---|---|---|
| Cell summary, SOC, balancing, fault/isolation | each module monitor | its rack controller | corresponding `W_Rack_rr` | No | Eight ordinary MainA↔Node relations |
| Balancing/mode command | rack controller | all eight monitors | corresponding `W_Rack_rr` | Yes | Uses default VCN 0 |
| Identity/Version/Health and diagnostics | module monitor | rack controller or service endpoint | corresponding `W_Rack_rr` | No | Same pair VCN is reused across Endpoints |
| Firmware update | service source via gateways/RC | selected module | corresponding `W_Rack_rr` | No | Background QoS; reliable transfer is above CAN LLL |
| Rack V/I/SOC/SOH, capability, limits | each rack controller | ContainerController | `W_Container` | No | 5–20 Hz state; no module stream leaves rack |
| Rack allocation and contactor command/state | ContainerController | each rack controller | `W_Container` | No | Directed normal control |
| Fire/gas open-all command | ContainerController | all rack controllers | `W_Container` | Yes | New PDU composed from Modbus input |
| Rack fault/trip | rack controller | ContainerController and Gateway | `W_Container` | Broadcast or separate directed PDUs | Source remains rack controller |
| Thermal state/setpoint | HVAC | ContainerController | `W_Container` | No | Bidirectional pair |
| PCS command/state | ContainerController | PCS controller | `W_Container` | No | Bidirectional pair |
| Aux/door/e-stop | AuxIO | ContainerController | `W_Container` | No | Event |
| Plant dispatch / container state | EMS / Gateway | Gateway / EMS | `W_Plant` | No | Gateway composes between identity universes |
| Plant-wide trip | container gateway | EMS and peers | `W_Plant` | Yes | Container-origin details are payload, not preserved source identity |

### 3.4 Forwarding

| Ingress | Wire | Egress | Notes |
|---|---|---|---|
| RackCAN_rr | `W_Rack_rr` | ContainerCAN | RackController forwards complete canonical PDUs |
| ContainerCAN | `W_Rack_rr` | RackCAN_rr | Reverse service/update path; one `Wire -> LinkBitmask` object per rack controller |
| ContainerCAN | `W_Rack_01..20`, `W_Container` | ContainerEth | ContainerController forwards each Wire; preserves canonical identity |
| ContainerEth | same container Wires | ContainerCAN | Reverse service/control path |
| ContainerEth | container Wires | PlantEth | **Not forwarding** in the deployable Config C fallback; Gateway composes onto `W_Plant` |

Config B conditional forwarding state is 41 `Wire -> LinkBitmask` objects:

- 20 objects, one at each RackController for its rack Wire;
- 20 rack-Wire objects at ContainerController; and
- 1 `W_Container` object at ContainerController.

Branches are not counted as separate objects. There are no splices. `FireGasPanel` and `RevenueMeter` are composition inputs; their data is authored on WS by `ContainerController` and `ContainerGateway`, respectively (`CORE §12.7`, §19.2).

### 3.5 Link profiles and physical-link counts

| Physical link | Profile | WS Wire(s) | Notes |
|---|---|---|---|
| each RackCAN_rr | committed unified CAN11, default VCN | one `W_Rack_rr` | One classifier and one alias on that physical bus |
| ContainerCAN | CAN-FD | `W_Rack_01..20`, `W_Container` | Richer profile is not yet frozen; must represent 21 Wires |
| ContainerEth | WS over Ethernet candidate | same 21 container Wires | Loop-free routing occurs only at ContainerController |
| PlantEth | Ethernet | `W_Plant` | Separate plant WireSpace in the deployable fallback |
| FireBus | non-WS Modbus RTU | none | ContainerController composes alarm |
| MeterBus | non-WS Modbus RTU | none | ContainerGateway composes reading |
| CtrlSHM | shared-memory profile, only if controller is split | device-private/internal Wire | No split is required in this minimum mapping |

Physical Links, including non-WS links:

| Type | Config A | Config B | Config C |
|---|---:|---:|---:|
| Rack CAN11 | 4 | 20 | 60 |
| Container CAN-FD | 1 | 1 | 3 |
| Container Ethernet | 1 | 1 | 3 |
| Plant Ethernet/fiber | 0 | 0 | 1 shared network |
| FireBus Modbus RTU | 1 | 1 | 3 |
| MeterBus Modbus RTU | 0 | 0 | 3 |
| **Total** | **7** | **23** | **73** |

Container-level rates are at most tens of hertz per source. Even allowing roughly 1,000 small PDUs/s, CAN-FD and 100-Mbit/s Ethernet are not the primary constraint; identity, commissioning, and rack CAN update scheduling are.

### 3.6 CAN11 bindings and accounting

#### Per physical RackCAN bus (same for all 20)

```text
Profile:          committed
WireAliases used:             1 / 8
  default map:                 1
  custom map:                  0

Per alias:
  Alias:                       0
  Canonical Wire:              W_Rack_rr
  Mapping:                     Default
  Ordinary VCNs used:          9 / 32 (9 / 31 usable ordinary)
  VCNs available, unused:      1, 2, 5, 7, 9, 11, 13, 15, 17, 19..31
  Reserved, unavailable:       3
  Default map sufficient?      yes
  Custom entries:              0
  MainA PID / MainB PID:       RackController_rr / unassigned
  Node positions used:         8 / 14
```

Used entries:

| VCN | Relation | Minimum use |
|---:|---|---|
| 0 | MainA → Broadcast | balancing/mode command |
| 4, 6, 8, 10, 12, 14, 16, 18 | Node0..7 ↔ MainA | all directed rack traffic |

MainB is unassigned. This sketch exercises only the one-Main/eight-Node part of the default allocation and provides no evidence about two-Main behavior. Default MainB broadcast, MainA↔MainB, all odd Node↔MainB slots, and Node8..13 slots are available but unused.

QoS allocation:

| QoS | Rack traffic |
|---:|---|
| 0 Critical | protection trip / severe module fault |
| 1 High | contactor-related state and urgent fault |
| 2 Normal | summaries, SOC, Health |
| 3 Background | firmware segments and bulk diagnostics |

#### Aggregate

| Scale | Physical rack buses | Active alias-binding instances | Default maps | Custom maps |
|---|---:|---:|---:|---:|
| Config A | 4 | 4 | 4 | 0 |
| Config B | 20 | 20 | 20 | 0 |
| Config C | 60 | 60 | 60 | 0 |

If `WireAlias` is scoped per physical CAN Link/ingress classifier, Alias 0 is independently reusable on every RackCAN and each bus consumes `1 / 8`. This follows the reconstruction shape `WireAlias -> {WireNumber, VcnMap}` in a local Link Binding, but the overlay never states the namespace explicitly.

If instead aliases are deployment-global, Config B requires 20 distinct active values and fails when rack 9 is added; Config C is further impossible. This is recorded as `SF-R6-014`. The recommended clarification is that committed `WireAlias` values are scoped to one physical CAN interface/classifier, while Guest VCN meanings remain explicitly deployment-global.

#### Membership versus presence

All production role Participants are configured members of their Wires. A silent monitor remains an unreachable member. Replacement hardware does not become a second member: after an explicit service operation it takes over the same role ParticipantId and membership, while the stable device UUID association changes. A retired rack requires a new committed configuration that removes its memberships; it is not merely “disconnected.”

---

## 4. Optional optimizations

### 4.1 Destination-pruned backbone forwarding

ContainerController may prune a directed `W_Rack_rr` PDU from ContainerEth when the destination is rack-local. This saves shared-backbone traffic without changing the rack Wire or canonical identity. It does not justify changing the minimum Wire decomposition.

### 4.2 Summarized plant boundary

Keeping each container as a separate WireSpace and composing only container state, capability, and faults onto `W_Plant` avoids the Config C ParticipantId ceiling and independent-universe collisions. This is a scope reduction, not a transparent optimization: plant-wide direct Standard Services and updates no longer work without explicit proxy Endpoints.

### 4.3 CAN29 / wider canonical identity

If the product requires one Config C WireSpace, wider canonical Participant identity is forced because 562 exceeds 255. VCN relation count on each rack remains nine, so a wider VCN map could theoretically reconstruct wider canonical IDs without consuming more 11-bit identifier bits. However, canonical descriptors, forwarding tables, mapping storage, configuration compatibility, and exact CAN11 profile version would all change; no such trial profile exists. CAN29 is the honest carrier candidate rather than silently extending the overlay.

No custom VCN map, extra alias, Guest profile, or overlapping rack Wire is useful in the minimum Config B mapping.

---

## 5. Friction signals (minimum mapping happy path)

Ratings apply to the conditional, uniquely commissioned Config B happy path. Identity failure is assessed separately rather than hidden in these seven signals.

| Signal | None / Mild / Significant | One-sentence justification |
|---|---|---|
| Artificial Wire | None | Every rack Wire and the container Wire corresponds to a real propagation/failure scope. |
| Wire proliferation | None | Twenty rack Wires are twenty physically and operationally independent racks, not a direction workaround. |
| Artificial hierarchy | None | RackController is the real rack authority and naturally occupies MainA; MainB is unused. |
| VCN pressure | None | Each rack uses 9 of 31 usable ordinary default-map slots. |
| WireAlias pressure | Mild | It is 1/8 per bus under the natural Link-local reading, but the unstated namespace makes the alternative fatal. |
| Configuration burden | Significant | At least 649 instance-specific topology facts must be correct for one ordinary container. |
| Failure/topology mismatch | None | Separate rack Wires preserve monitor, rack, container-link, and gateway failure scopes. |

---

## 6. Model pressure

### 6.1 Multi-vendor identity cases

| Case | Classification | Result and cost |
|---|---|---|
| Vendor A RC `0x10` collides with Vendor B PCS `0x10` | **(d) not resolvable under current R6** for one transparent universe | Commissioning reassignment would be (a), but both vendors refuse. A translator would be (b), but `CORE §4.6` defers it. A composition boundary changes source identity and Service addressing. |
| Twenty racks repeat the same factory IDs | **(d)** for one transparent container; **(c)** only with loss of transparency | Twenty rack-local WireSpaces can reuse the values, with each RC composing/proxying into the container. Then module Services are not canonical container Participants and plant inventory/update requires new proxy semantics. |
| Three independently commissioned containers join | **(c)** for aggregate operation; **(d)** for transparent fleet Services | Keep three container WireSpaces and a plant WireSpace. Gateways compose dispatch/state/faults. Direct EMS-to-module Identity/Health/update cannot cross as forwarding. |
| Four later racks add Endpoints/version skew | **(a)** if unique IDs can be assigned | Organizer inventory must record per-device Endpoint capabilities and reject unsupported bindings (`DEPLOY §2.3`). Additional Endpoints need not renumber old racks, but Service-version interoperability remains a deployment/schema concern. |

A splice is never an answer: it changes only Wire scope and preserves colliding ParticipantIds (`CORE §6.1`, §6.6).

### 6.2 Repeated-structure configuration inventory

Genuinely reusable:

- firmware and Service definitions;
- the nine-node rack topology template;
- default VCN shape and QoS policy;
- generated artifact schema and validation rules.

Per rack instance:

- 9 role-to-UUID/ParticipantId associations;
- 1 canonical WireNumber;
- 9 Wire membership entries;
- 9 default-map role-position PID values (MainA plus Node0..7);
- 1 Alias-to-Wire binding; and
- one forwarding object at the rack controller.

The first five categories are `29` authored facts per rack, or `580` across Config B. Add one container Wire plus 27 container-Wire memberships (`28`) and 41 forwarding objects: **649 topology facts minimum**, before Endpoint capability, traffic-bound, compatibility, and firmware-version declarations. Generated output may replicate bindings into nine device images per rack; generation reduces typing, not conceptual size or correctness burden.

Growth is linear:

- 4 → 20 racks: rack-specific facts grow 5×;
- 1 → 3 containers: Participants, rack Wires, and rack bindings grow 3×;
- no data-plane table is inherently worse than linear, but collision checking and field-service reconciliation become operationally harder because separately authored identity plans must be compared.

### 6.3 Offline commissioning mechanics

A two-person crew would need an offline Organizer and authoritative deployment Manifest on the service laptop or removable media. Before entering the field it must contain:

- the container identity universe and reserved ParticipantId/WireNumber pools;
- expected physical rack slots and the default rack template;
- fixed Vendor B identities or approved replacement role IDs;
- acceptable Endpoint/profile/software versions; and
- generated forwarding and binding plans.

At installation it must, one rack at a time:

1. select each physical device below ordinary traffic;
2. read UUID, vendor identity, version, and capabilities;
3. associate UUID with rack/module slot;
4. assign or verify the role PID and WireNumber;
5. stage the alias/default-position map and forwarding state;
6. reject duplicate canonical IDs, duplicate slot claims, wrong revisions, and unrepresentable Endpoints;
7. atomically commit and retain an export/backup.

`DEPLOY §1.2` defines Unconfigured/Selected/Staged/Committed phases, but the CAN11 selection/control payload and activation mechanism are not specified. The Organizer also cannot infer physical rack slot from an identical generic part unless installation supplies a port/indicator/scan procedure. A wrong association can produce a syntactically valid deployment whose telemetry and commands refer to the wrong rack. Thus R6 states what must be unique but does not yet provide the field procedure that makes it true.

### 6.4 Field-service deltas

| Event | Configuration delta | Who / tool | Identity and history consequence |
|---|---|---|---|
| Spare module installed | Replace one role→UUID association; load the existing PID, Wire membership, Node position, and profile artifact | Technician with offline Organizer | Same role ParticipantId, new physical UUID. Identity reports both; logs correlate by role PID plus UUID/time. R6 has no explicit “replaced” state beyond tooling history. |
| Four racks added (12→16) | Allocate 36 PIDs, 4 WireNumbers, 36 memberships, 36 default positions, 4 aliases, and 8 forwarding objects | Commissioning crew / Organizer | Existing twelve need not change if pools were reserved. New Endpoint capabilities are added to the Manifest; unsupported interactions are rejected. |
| Rack retired | Commit removal of 9 memberships and associated forwarding/binding state; tombstone 9 PIDs and one WireNumber | Maintainer / Organizer | Do not reuse while historical logs can refer to them. R6 specifies no reuse/retention policy. |
| Whole rack replaced | Update 9 UUID associations and reinstall the existing rack template | Technician / Organizer | Preserve nine role PIDs and WireNumber; record nine physical replacements. Vendor refusal to accept role IDs remains the blocker. |
| Container gateway replaced | Restore role PID, all Wire memberships, forwarding objects, profile state, and composition configuration from authoritative backup | Technician / Organizer | The runtime cannot safely infer this state. Without a backup, the container must be recommissioned and cross-boundary history reconciled. |

Static membership is useful for temporary absence. Permanent replacement is representable as role continuity plus UUID change (`DEPLOY §1.6–1.7`), but replacement event semantics, tombstoning, and retention are tooling policy rather than a defined R6 lifecycle.

### 6.5 Rack CAN bandwidth

Assumptions for one rack at nominal rates:

- 11-bit Classical CAN, 500 kbit/s;
- conservative 130 wire bits per 8-byte frame including overhead/stuffing allowance;
- cell summary: 5 Hz/module, 3 CAN frames/PDU;
- SOC/balancing state: 1 Hz/module, 1 frame/PDU;
- fault/isolation: 1 Hz/module, 2 frames/PDU;
- Health: 0.2 Hz/module, 1 frame/PDU;
- rack broadcast command: 1 Hz, 1 frame.

```text
cell summary       8 * 5 * 3       = 120.0 frames/s
SOC/balancing      8 * 1 * 1       =   8.0 frames/s
fault/isolation    8 * 1 * 2       =  16.0 frames/s
Health             8 * 0.2 * 1     =   1.6 frames/s
broadcast command  1 * 1           =   1.0 frames/s
                                      ----------------
nominal total                         146.6 frames/s

load = 146.6 * 130 / 500,000 = 3.81%
```

Even doubling the estimate leaves substantial headroom. This is an assumption-based sizing result because final CAN11 PDUA packing is provisional; a deployment must replace it with measured encoded frame counts.

### 6.6 Firmware campaign arithmetic

Assume an effective 5.5 firmware bytes per CAN frame after PDUA/Transport overhead and reserve 80% of each RackCAN for live traffic, retransmission margin, and idle headroom. Background update receives at most 20%.

```text
image bytes per rack = 8 * 256 KiB
                     = 2,097,152 bytes

update payload rate  = 500,000 bit/s * 20% * 5.5 byte/frame
                       / 130 bit/frame
                     = 4,231 byte/s

ideal per-rack time  = 2,097,152 / 4,231
                     = 496 s = 8.3 min
```

Allowing 15% for retries, command/ack exchanges, and flash pauses gives roughly **9.5 minutes per rack**. Because the twenty RackCAN buses are independent, a fully parallel campaign is about 9.5 minutes plus source/distribution overhead; a conservative one-rack-at-a-time campaign is about **190 minutes (3.2 hours)**. Intermediate batching scales between those values.

QoS helps by placing update at Background and protection/fault traffic at Critical/High. It does not create bandwidth or bound Service/flash scheduling. A high-priority CAN frame can still wait for the currently transmitting low-priority frame, approximately `130 / 500,000 = 260 µs`, which is acceptable here if the implementation never serializes a whole update PDU ahead of Critical traffic.

### 6.7 Fire/gas path

Required path:

```text
FireGasPanel --Modbus--> ContainerController
             --composed WS broadcast on W_Container--> 20 RackControllers
             --local contactor actuation
```

Budget assumption:

| Component | Worst-case allocation |
|---|---:|
| Priority Modbus poll interval | 20 ms |
| Modbus transfer/validation | 5 ms |
| composition and Endpoint enqueue | 1 ms |
| one CAN-FD broadcast | 0.3 ms |
| rack-controller dispatch/output scheduling | 5 ms |
| **Estimated total** | **31.3 ms** |

If broadcast is unavailable in the eventual CAN-FD profile, twenty directed frames at 0.3 ms each produce about 37 ms total. Both fit 100 ms with margin under these assumptions. The rack firmware campaign is on twenty different CAN11 buses and cannot directly occupy ContainerCAN, but CPU/queue scheduling must preserve Critical service. If the certified Modbus path cannot guarantee the assumed 20-ms priority poll, WireSpaces cannot recover the budget; composition relocates the legacy boundary but does not improve the poll.

### 6.8 Standard Services and version skew

Identity, Version, Health, fault/diagnostic status, and diagnostic query are ubiquitous across all 187 Config B Participants. Firmware update is present on 184 integrator/Vendor-A-managed Participants (160 monitors, 20 rack controllers, container controller, gateway, HVAC, and AuxIO); the three PCS Participants use Vendor B tooling. Capability-specific Services include energy state, contactor control, thermal control, PCS control, link status, logs, and configuration.

The Service definitions are shared, but there are still 187 Endpoint registrations and bounded storage/resource declarations per ubiquitous Service. Later rack revisions may expose three additional Endpoints without changing Participant identity. Configuration must use discovered capability/version facts and must not bind old racks to absent Endpoints. R6 configuration compatibility can detect a mismatch, but lifecycle policy for mixed Service versions is not supplied by the Wire model.

### 6.9 Result against falsification criteria

The mapping falsifies R6 suitability for the complete stated domain:

- immutable vendor ParticipantIds collide and repeat; current R6 offers neither transparent translation nor a way to make those values contextual;
- Config C exceeds the ordinary range by more than 2×;
- separate container WireSpaces preserve aggregate operation but fail required transparent fleet Services;
- role PID plus UUID can describe replacement, but replacement history and PID retirement are not defined lifecycle operations;
- the required offline CAN11 commissioning/activation mechanism is incomplete; and
- at least 649 one-container topology facts remain instance-specific.

The evidence does **not** falsify the R6 Wire model or unified CAN11 VCN inside a coordinated rack/container deployment. Those parts fit naturally. It falsifies the assumption that deployment-global static identity can be established for this multi-vendor, field-serviced product using currently specified mechanisms.

### 6.10 Concept change and useful distinction

**If one WS concept could change:** define an explicit boundary for hierarchical identity universes, including a specified translating/proxy gateway model and the effect on Standard Services. This need not be transparent forwarding, but the current choice between “one global universe” and “composition that loses participant identity” is too coarse for modular equipment.

**Useful distinction exposed by WS:** forwarding versus composition makes the plant boundary honest. A gateway that aggregates or translates vendor/container semantics is a new author; preserving the original source while transforming the meaning would be false lineage.

---

## 7. Open questions

1. Is committed `WireAlias` scoped per physical CAN interface/classifier or per deployment? Both consequences are quantified in §3.6.
2. Are Vendor A/B factory values canonical ParticipantIds or separate stable device identities? The archetype collision case treats them as the former; if they are UUIDs, Organizer role assignment removes that particular conflict.
3. What exact CAN11 control procedure selects one generic spare, stages its PID/default-map binding, and atomically activates it without mixed maps?
4. What is the canonical lifecycle record for role replacement, retirement, PID tombstoning, and history retention?
5. Does Config C require wider canonical identity, or is the intended product boundary one WireSpace per container?
6. What Standard Service semantics should a composition/proxy expose when a plant tool asks for a participant inside another identity universe?
7. Which three plant Participants account for the difference between the explicit Config C total of 562 and the stated approximate target of 565?
8. What CAN-FD profile represents 21 Wires on `ContainerCAN`, and what compatibility/version state must be installed?

---

## 8. Spec findings

| ID | Finding | Consequence |
|---|---|---|
| `SF-R6-014` | The overlay does not explicitly define the namespace of committed `WireAlias`. | Per-Link scope makes 20/60 repeated rack buses straightforward; deployment-global scope makes Config B fail at rack 9. |

---

## 9. Diagram

```text
RackCAN_01 (CAN11)                          RackCAN_20 (CAN11)
  MM01_1..8                                    MM20_1..8
       \                                         /
     RC01 ===== W_Rack_01       W_Rack_20 ===== RC20
       \                                             /
        +------------- ContainerCAN (CAN-FD) -------+
                              |
                     ContainerController
                       | compose FireBus
                       |
                 ContainerEth (21 Wires)
                       |
                ContainerGateway
                       |
          [composition / identity boundary]
                       |
                    W_Plant
                       |
                    Plant EMS
```

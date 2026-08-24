# Sketch — Archetype 12: Modular Grid Battery Storage, Field-Serviced and Multi-Vendor

```text
**Archetype:** archetypes/12_modular_bess_field_serviced.md
**Agent:** cursor_B
**Output file:** sketches/12_modular_bess_field_serviced_cursor_B.md
**Date:** 2026-08-23
**Primary configuration:** Config B — 20-rack container
```

---

## Executive summary

```text
**Minimum mapping:** Config B — 187 Participants, 22 Wires (20 repeated rack Wires + container control + container services), 20 committed CAN11 default-map bindings, CAN-FD and Ethernet; Config C requires separate container WireSpaces plus plant-level composition.
**Question A (R6):** Poor for the supplied multi-vendor/lifecycle domain; structurally natural only after a coordinated identity plan exists.
**Question B (CAN11):** Default — excellent per-rack fit, but WireAlias namespace ambiguity is deployment-blocking at rack 9 under a global reading.
**Worst friction (minimum path):** Configuration burden — Significant.
**Main lesson:** Repeated rack communication maps cleanly and scales linearly, but current R6 cannot reconcile immutable colliding vendor identities, distinguish replacement lifecycle, or transparently join three independently commissioned containers; Config C also exceeds the 254-ID universe.
```

---

## Disposition block

| Area | Assessment |
|---|---|
| Participant identity | poor |
| Wire decomposition | Natural |
| Forwarding | Moderate |
| Non-CAN configuration | high |
| CAN11 VCN fit | Default |
| Better with CAN29? | no |

**Explanation:** Question A splits sharply: once unique deployment identities are assumed, twenty identical nine-node rack scopes and two container scopes are natural, with linear configuration growth. But that assumption is unavailable here: Vendor A repeats immutable factory identities, Vendor B collides with them, field replacements come from generic inventory, and Config C needs 562 listed Participants versus 254 ordinary IDs. Plain forwarding and splices preserve identity and cannot repair this; translating gateways are deferred.

Question B is positive per physical rack: RackController=MainA, ContainerController=MainB, and eight ModuleMonitor Nodes use 18/31 ordinary VCN slots in one default binding. Twenty identical buses are valid only if WireAlias values are scoped to each physical CAN classifier/Link; if aliases are deployment-global, the ninth rack cannot be represented. CAN29 is not an honest remedy because the installed module monitors have exactly one Classical CAN11 Link.

---

## 0. Falsification criteria (stated before mapping)

I will judge R6 identity **defective for this domain** if:

1. Immutable Vendor A and Vendor B canonical identities collide and current R6 offers neither commissioning-time projection nor a specified translator that preserves participant-visible Services.
2. Twenty factory-identical racks cannot receive unique deployment roles without changing vendor-owned configuration or introducing composition proxies that erase original canonical identity.
3. A field replacement cannot be represented distinctly from temporary unreachability while retaining role history and recording the new hardware UUID.
4. The ordinary identity universe cannot represent Config C, and the available alternatives either lose transparent Service reachability or require an unspecified extended CAN11 projection.
5. A two-person field crew must author or verify more than **20 unique records per rack** without a specified source of truth, rollback, and collision validation.

I will judge R6 identity **sound for this domain** if:

1. Factory identity remains immutable while a specified deployment mechanism assigns collision-free canonical role IDs without vendor renumbering.
2. Replacement explicitly preserves or changes role identity while recording hardware continuity.
3. Independently commissioned containers can be joined without silent collision, application proxying, or an unspecified translator.
4. Config A→B→C scaling remains linear and inside a defined address/profile envelope.

The mapping fails criteria 1–4 and exceeds criterion 5. The failure is primarily identity/commissioning architecture, not rack communication semantics.

---

## 1. Native communication model

A grid battery container contains 20 interchangeable racks. Each rack has one RackController and eight ModuleMonitors on an isolated 500 kbit/s Classical CAN bus. The rack structure and firmware are byte-identical across projects.

At container level, a ContainerController coordinates rack power, contactors, HVAC, auxiliary I/O, and a Vendor B PCS over CAN-FD and Ethernet. A ContainerGateway provides the plant boundary. A certified FireGasPanel and sealed RevenueMeter remain Modbus devices; the ContainerController and Gateway respectively compose their useful state into the container application.

The integrator does not control all identities:

- Vendor A ships racks from generic inventory with factory identities and will not renumber them.
- Vendor B independently preconfigures PCS identities and will not renumber them.
- The integrator owns only container controller/gateway/HVAC/aux identities.
- Plant EMS identity is assigned later by another party.

Field service routinely replaces individual modules, whole racks, and gateways from generic stock. Rack count may grow years later, with version and Endpoint skew. Three containers are independently commissioned and later connected to one plant.

The fast path is a FireGasPanel alarm that must open all rack contactors within 100 ms. Everything else is low-rate; module update campaigns can last minutes or hours.

---

## 2. Obvious conventional implementation

> **Obvious conventional implementation:** Vendor-local CAN IDs repeated independently on each isolated rack bus; one container asset/slot database maps rack serials to physical slots; CAN-FD signals between container devices; Ethernet/Modbus gateways; plant EMS talks to one container facade per container rather than directly addressing every module.

> **What structure does a conventional design use?** Factory serial numbers, rack/slot addresses, per-vendor namespaces, an integrator asset registry, gateway translation/proxy tables, CAN databases, field-service replacement records, and versioned device capability manifests.

WireSpaces makes rack relations and standard Services uniform, but its one coordinated canonical identity universe removes the namespace boundaries that conventional integration relies on. Current R6 identifies the collision problem but supplies no translator or lifecycle model to replace those boundaries.

---

## 3. Minimum mapping — Config B first

This is the simplest semantic mapping **assuming** the integrator can assign unique deployment ParticipantIds. Section 4 shows why that assumption cannot be satisfied under the supplied vendor constraints.

### 3.1 Participants and address accounting

| Class | Config A | Config B | Config C (3 containers) |
|---|---:|---:|---:|
| ModuleMonitor | 32 | 160 | 480 |
| RackController | 4 | 20 | 60 |
| ContainerController | 1 | 1 | 3 |
| ContainerGateway | 1 | 1 | 3 |
| PcsController | 1 | 1 | 3 |
| PcsInverter | 2 | 2 | 6 |
| HvacController | 1 | 1 | 3 |
| AuxIO | 1 | 1 | 3 |
| Plant EMS | 0 | 0 | 1 |
| **Total from listed classes** | **43** | **187** | **562** |
| Archetype approximate total | ~43 | ~187 | ~565 |
| Ordinary 254-ID range consumed | **16.9%** | **73.6%** | **221.3%** |

The listed classes total **562**, not 565; the archetype's “~565” is treated as approximate. Either count exceeds 254 by more than 300.

Illustrative Config B allocation:

```text
0x01  ContainerController
0x02  ContainerGateway
0x03  PcsController
0x04  PcsInverterA
0x05  PcsInverterB
0x06  HvacController
0x07  AuxIO

Per rack rr = 1..20:
    one RackController PID
    eight ModuleMonitor PIDs
    allocated from remaining 0x08..0xBB
```

This allocation is a **role plan**, not a solution to immutable vendor identities.

### 3.2 Wires

| Wire(s) | Count A / B / C | Participants and Links | Purpose |
|---|---:|---|---|
| W_Rack_rr | 4 / 20 / 60 instantiated | ContainerController, RackController_rr, 8 ModuleMonitors; ContainerCAN + RackCAN_rr | Rack telemetry/control plus direct diagnostic/update relation |
| W_ContainerControl | 1 / 1 / 3 | ContainerController, 20 RackControllers, PCS, HVAC, AuxIO; ContainerCAN | Aggregate rack allocation, fire trip, PCS/thermal/aux |
| W_ContainerServices | 1 / 1 / 3 | ContainerController, Gateway, PCS controller/inverters; ContainerEth | Container service, PCS details, gateway handoff |
| W_Plant | 0 / 0 / 1 | Plant EMS + three ContainerGateway facades; PlantEth | Plant dispatch/capability/fault at container granularity |

**Canonical WireNumbers instantiated:** Config A **6**, Config B **22**, Config C **67** (66 container-local + one plant). If containers remain separate WireSpaces, each container needs only 22 local WireNumbers and the plant WireSpace needs one.

Each rack Wire is a distinct connected propagation domain and consumes its own WireNumber even though all share one template. Reusing one WireNumber across disconnected racks would not form one connected Logical Bus.

W_Rack_rr spans ContainerCAN and RackCAN_rr through RackController_rr so ContainerController can directly query/update ModuleMonitor Participants while preserving canonical identity. W_ContainerControl remains separate so one fire-trip broadcast reaches all RackControllers without sending on 20 rack Wires.

### 3.3 Interactions

| Interaction | Src | Dest | Wire | Broadcast? | Notes |
|---|---|---|---|---|---|
| Module summary/SOC/fault | each ModuleMonitor | RackController | W_Rack_rr | No | 0.5–10 Hz |
| Module balancing/mode | RackController | modules | W_Rack_rr | Yes/direct | 1 Hz/event |
| Module Identity/Health/update | ContainerController ↔ module | W_Rack_rr | No | MainB↔Node relation |
| Rack state/capability | RackController | ContainerController | W_ContainerControl | No | 2–20 Hz |
| Rack allocation/contactor | ContainerController ↔ RackController | W_ContainerControl | No | |
| Fire trip | ContainerController | all RackControllers | W_ContainerControl | Yes | Composed from Modbus; ≤100 ms |
| HVAC | ContainerController ↔ HvacController | W_ContainerControl | No | |
| PCS command/state | ContainerController ↔ PcsController | W_ContainerControl/W_ContainerServices | No | Vendor boundary issue in §4 |
| Aux/e-stop state | AuxIO | ContainerController | W_ContainerControl | No | |
| PCS inverter detail | PcsController/inverters ↔ ContainerController | W_ContainerServices | No | |
| Container dispatch/capability (C) | EMS ↔ ContainerGateway facade | W_Plant | No | Composition at container boundary |
| Fleet inventory/update (C) | EMS ↔ ContainerGateway | W_Plant | No | Gateway returns proxied container inventory |

The plant EMS does **not** transparently address every ModuleMonitor in the recommended Config C boundary; direct cross-container Service identity would require the missing translator or wider coordinated universe.

### 3.4 Forwarding and composition

| Element | Wire | Member Links | Objects |
|---|---|---|---:|
| Each RackController | its W_Rack_rr | ContainerCAN + RackCAN_rr | 1 Wire→LinkBitmask |
| ContainerController | local delivery on W_Rack_* and W_ContainerControl | ContainerCAN | no cross-Link forwarding for W_ContainerControl |
| ContainerGateway | W_ContainerServices | ContainerEth | local participant/facade |
| ContainerGateway (Config C) | W_Plant | PlantEth | composition boundary, not transparent forwarding |

**Gateway forwarding objects:** Config A **4**, Config B **20**, Config C **60** rack forwarding objects. Count is one object per RackController per Wire, not one per branch.

**Composition points:**

1. FireGasPanel Modbus → ContainerController-authored fire-trip PDU.
2. RevenueMeter Modbus → ContainerGateway-authored meter state (Config C).
3. Container-local state/Services → ContainerGateway-authored plant facade.
4. If vendor identity cannot be reassigned, vendor subsystem → integrator facade composition; this loses transparent canonical vendor Participant identity and is evidence of the gap.

No splice resolves identity collisions: splices preserve source/destination identity and do not merge independently assigned universes.

### 3.5 Link profiles and physical inventory

| Link | Count A / B / C | Profile | Wires |
|---|---:|---|---|
| RackCAN | 4 / 20 / 60 | committed Classical CAN11 | one W_Rack each |
| ContainerCAN | 1 / 1 / 3 | CAN-FD, direct normal-range addressing | W_Rack_* + W_ContainerControl |
| ContainerEth | 1 / 1 / 3 | Ethernet WS | W_ContainerServices |
| PlantEth | 0 / 0 / 1 | Ethernet WS | W_Plant |
| FireBus | 1 / 1 / 3 | Modbus RTU, non-WS | composition only |
| MeterBus | 0 / 0 / 3 | Modbus RTU, non-WS | composition only |
| **Physical Links** | **7** | **23** | **73** |

No CtrlSHM split is needed in the minimum mapping.

### 3.6 CAN11 accounting — each RackCAN

Role assignment, repeated on each physical rack bus:

```text
MainA = RackController_rr
MainB = ContainerController
Node0..Node7 = ModuleMonitor_rr_1..8
```

```text
Profile:          committed
WireAliases used per physical RackCAN:  1 / 8
  default map:                            1
  custom map:                             0

Per alias:
  Alias:                                  1 (illustrative)
  Canonical Wire:                         W_Rack_rr
  Mapping:                                Default
  Ordinary VCNs used:                    18 / 31
  VCNs available, unused:                VCN 1; VCN 20–31
  Default map sufficient?                 yes
  Custom entries:                         0
  MainA PID / MainB PID:                  RackController_rr / ContainerController
  Node positions used:                    8 / 14
```

Used VCNs:

- VCN 0: RackController broadcast balancing/mode command.
- VCN 2: RackController↔ContainerController.
- VCNs 4–19: each of 8 Nodes has MainA and MainB relation.
- VCN 3 reserved; MainB broadcast VCN 1 unused.

Two-Main allocation is genuinely exercised and natural: MainA is local rack authority; MainB is container diagnostics/update.

#### WireAlias namespace ambiguity

| Interpretation | Config B consequence |
|---|---|
| **Per physical CAN Link/classifier** | Each RackCAN independently reuses Alias 1; 20 active bindings are valid because ingress starts with a physical interface. |
| **Deployment-global alias namespace** | Only 8 alias values exist; rack 9 cannot receive a distinct Wire binding. Config B fails despite 20 separate physical buses. |

The overlay says VCN is scoped by a WireAlias **binding** and every active alias binds one Wire, but does not explicitly state whether alias value uniqueness crosses physical interfaces. Guest VCN meanings are explicitly deployment-global; committed alias scope lacks equally explicit text. The sketch cannot resolve this normatively. Recommended clarification: WireAlias values are local to one physical CAN classifier/interface; otherwise repeated isolated CAN buses are artificially capped at eight.

**Aggregate Config B:** 20 default bindings, each 18/31 used. This is **not** 360 VCNs in one map; it is one repeated 18-entry relation shape instantiated 20 times.

### 3.7 Rack CAN bandwidth

Nominal assumptions per rack:

```text
8 modules
cell summary:        5 Hz, average 2 CAN frames       = 80 frames/s
SOC/balancing:       1 Hz, 1 frame                    =  8
fault/isolation:     1 Hz, 1 frame                    =  8
Health:            0.2 Hz, 1 frame                    =  1.6
mode/balance cmd:    1 Hz broadcast                   =  1
------------------------------------------------------------
nominal total                                          ~99 frames/s

one 8-byte Classical CAN frame incl. overhead/stuff:  ~135 bits
wire rate: 99 × 135 = 13.4 kbit/s = 2.7% of 500 kbit/s
```

Fault bursts and queries add margin but ordinary telemetry is not the constraint.

QoS:

- Critical: module/rack faults and isolation.
- High: control/mode.
- Normal: telemetry/Health.
- Background: firmware update.

### 3.8 Firmware update campaign arithmetic

```text
image per module:                 256 KiB
8 modules per rack:            2,097,152 bytes = 16.78 Mbit payload
Classical CAN useful payload:      64 bits per ~135 wire bits ≈ 47.4%
effective payload at full bus:    ~237 kbit/s
```

At **25% bus allocation** for background update:

```text
effective update payload:       ~59.3 kbit/s
per-rack 8-module time:          16.78 Mbit / 59.3 kbit/s ≈ 283 s ≈ 4.7 min
20 racks parallel:               ≈4.7 min rack-bus phase
20 racks sequential:             ≈94 min
```

At a more conservative **10% allocation**:

```text
per rack:                        ≈11.8 min
20 sequential:                  ≈3.9 hours
```

Parallel rack updates require distributing/staging the image at 20 RackControllers and accepting simultaneous maintenance. QoS helps critical frames win arbitration, but it does **not** enforce a bandwidth cap or prevent update starvation; the update sender needs explicit rate limiting. Each update frame is bounded, so Critical/High traffic can preempt between frames.

### 3.9 Fire/gas timing

Assumptions:

```text
FireGasPanel Modbus poll interval:       20 ms
response/serial completion worst case:    5 ms
ContainerController composition:          1 ms
one ContainerCAN-FD broadcast:          <0.5 ms
RackController processing/contactor cmd:  5 ms
margin:                                  10 ms
------------------------------------------------
estimated end-to-end:                  <41.5 ms
```

One W_ContainerControl broadcast is preferable to 20 directed commands. Even 20 short CAN-FD commands should fit in a few milliseconds, but broadcast avoids that multiplication. An update campaign cannot block the path if:

1. update traffic is Background QoS and rate-limited;
2. FireBus polling is scheduled independently at ≤20 ms;
3. ContainerCAN Critical fire traffic preempts update frames.

The composition hop is small; Modbus polling dominates. WS relocates the legacy boundary cleanly but does not make the FireGasPanel faster.

### 3.10 Standard Services

| Service | Scope / cost |
|---|---|
| Identity / Version / Health / fault / RPC | All 187 Config B Participants; 160 repeated module instances |
| Update | 160 modules + 20 racks + integrator devices; PCS via vendor tooling |
| Time | Container/racks for ~10 ms fault correlation |
| Link status | Gateway, ContainerController, 20 RackControllers |
| Logs | Small module fault logs; richer rack/container logs |
| Configuration | Rack limits, allocation, thermal setpoints |
| Telemetry | Summarized module→rack→container→plant |

Service schemas and firmware are shared; Endpoint bindings, ParticipantIds, membership, and lifecycle records are per instance.

---

## 4. Mandatory multi-vendor identity analysis

R6 requires one unique deployment ParticipantId per Endpoint Domain in a coordinated identity universe. Plain forwarding preserves canonical identity; splices change Wire scope but preserve source/destination and do not resolve collisions.

| Case | Current R6 result | Cost / missing mechanism |
|---|---|---|
| Vendor A RackController `0x10` collides with Vendor B PCS `0x10` | **Not resolvable by plain forwarding or splice.** Commissioning reassignment would work only if devices accept a deployment PID override; premise says vendors will not renumber. | Explicit identity translator is deferred to `FUTURE §12`; composition proxies can isolate universes but re-author traffic and lose transparent source identity. |
| Twenty racks repeat the same factory IDs | Isolated RackCANs can each operate in separate vendor-local universes, but transparent forwarding into one container universe creates collisions. | Assign 180 unique deployment roles at commissioning (commercial premise rejects modifying vendor identity), or compose one rack facade per rack; current R6 lacks transparent projection across universes. |
| Three independently commissioned containers join | Keep each container as its own WireSpace and expose one ContainerGateway facade to W_Plant. | Plant EMS sees container-level Services/proxied inventory, not direct canonical module identities. Transparent cross-container query requires missing translation or coordinated renumbering. |
| Later rack has three new Endpoints/version | Not an identity collision. Same role pattern with a capability/version-skew manifest. | Organizer/service tooling must inspect implementation and endpoint capability before binding; unsupported endpoints remain absent. R6 provides identity but little lifecycle/capability-negotiation guidance. |

### Extended addressing alternative

Config C needs at least 562 ordinary identities. A wider canonical ParticipantId could fit one coordinated plant, but it is **not currently specified** by this trial:

- Ethernet/CAN-FD canonical headers and forwarding tables grow.
- Every PID-bearing manifest, Router, Endpoint acceptance table, and tool changes.
- CAN11 frame IDs need not grow because VCN reconstructs canonical IDs, but each default role-position table must store wider PIDs and the reconstruction profile must define that projection.
- Current overlay assumes ordinary 8-bit values and supplies no extended CAN11 binding format/version.

Thus extended addressing is a design option, not a valid current-R6 mapping.

### Scope verdict for Config C

Use **three container-local WireSpaces** plus a small plant WireSpace:

```text
Plant EMS
   ↕ W_Plant
ContainerGateway facade ×3
   ↕ composition
container-local Services / inventory
```

This is valid current composition and avoids identity collision, but sacrifices transparent plant-to-module canonical Service access. If direct access is a product requirement, Config C is not resolvable under current R6.

---

## 5. Commissioning and lifecycle

### 5.1 What an offline field Organizer must know

Before activation, an authoritative deployment manifest must contain:

1. physical rack slot and cable/interface association;
2. scanned hardware UUID/serial for every rack and module;
3. vendor/firmware/capability revision;
4. one unique canonical role PID per Endpoint Domain;
5. one WireNumber per rack;
6. MainA/MainB/Node0–7 role positions;
7. alias binding and forwarding object;
8. expected Endpoint/Service capability set;
9. container-level Wire memberships.

Per rack, unique configured records are approximately:

```text
9 Participant role assignments
1 WireNumber
10 default-map role positions (MainA, MainB, 8 Nodes)
1 WireAlias binding
1 forwarding object
--------------------------------
22 records per rack
20 racks → ~440 rack-instance records
```

The template is shared, but UUIDs, PIDs, WireNumbers, slot associations, and installed revisions are not. The crew needs an offline laptop/tool, barcode/UUID scan, physical-slot plan, duplicate/capability validation, staged activation, rollback, and signed-off export/backup. If the wrong rack slot is selected, state and contactor commands can be associated with the wrong physical rack; activation must fail validation rather than infer topology.

Under the premise that vendor canonical IDs cannot be overridden, commissioning halts: the tool can detect collision but cannot legally repair it.

### 5.2 Field-service deltas

| Event | Identity/configuration change | Who / tool |
|---|---|---|
| One ModuleMonitor replaced | Preserve functional role PID and VCN Node position; replace hardware UUID/serial/version association; revalidate Endpoints. Identity Service reports same deployment role plus new hardware identity. | Technician, offline Organizer replacement workflow |
| Four racks added (12→16) | Allocate 36 PIDs, 4 WireNumbers, 4 aliases/bindings, 4 forwarding objects, 40 role positions; existing 12 racks unchanged. | Integrator manifest authority + field crew |
| Rack retired | Deactivate its Wire/bindings; tombstone 9 PIDs and WireNumber for historical correlation; no immediate reuse policy is defined by R6. | Integrator lifecycle tool |
| Whole rack replaced | Reuse 9 role PIDs and WireNumber; replace 9 UUID associations and validate role positions/revision. | Field crew with known slot manifest |
| ContainerGateway replaced | New hardware UUID assumes same Gateway role PID; restore forwarding/composition configuration from authoritative backup. | Field crew; without backup R6 cannot infer/recover it |

R6 does not explicitly distinguish **replaced** from **unreachable**. Treating PID as a deployment role makes role reuse coherent, while stable hardware UUID preserves physical history — but the lifecycle transition, tombstone policy, and authoritative history are deployment-tooling gaps, not currently specified behavior.

---

## 6. Scaling and configuration accounting

| Measure | Config A (4 racks) | Config B (20 racks) | Config C (3×20) |
|---|---:|---:|---:|
| WS Participants | 43 | 187 | 562 listed |
| Ordinary PID capacity | 16.9% | 73.6% | 221.3% — impossible |
| Rack CAN11 buses | 4 | 20 | 60 |
| Canonical Wires | 6 | 22 | 67 instantiated |
| Rack forwarding objects | 4 | 20 | 60 |
| Rack instance records (~22/rack) | 88 | 440 | 1,320 |
| Default-map CAN bindings | 4 | 20 | 60 |

Growth from 4→20 racks and 1→3 containers is linear in hardware count. What is worse than linear is **organizational reconciliation**: independently owned identity universes cannot be merged at any configuration size without translation or renumbering.

Genuinely shared:

- firmware/schema per hardware revision;
- rack Wire template;
- default VCN map shape;
- Service definitions;
- validation rules.

Per instance:

- ParticipantIds and hardware UUID associations;
- WireNumber;
- alias binding;
- Main/Node role assignment;
- membership and forwarding object;
- physical slot and lifecycle history.

---

## 7. Optional alternatives

### 7.1 One Wire for all RackCAN buses

Invalid: the twenty rack buses are disconnected propagation domains. Reusing one WireNumber would produce a disconnected “Wire,” contrary to Logical Bus semantics.

### 7.2 Composition-only rack facades

Keep each vendor rack as its own WireSpace; RackController or integrator gateway publishes one rack aggregate Participant upstream. **Benefit:** contains repeated factory IDs. **Cost:** ModuleMonitor canonical identities and direct standard Services do not cross the boundary; update/diagnostics become vendor proxy APIs. Valid but weaker integration.

### 7.3 CAN29 for racks

Not physically available. ModuleMonitors have one Classical CAN11 interface; changing carrier is redesign, not mapping.

### 7.4 Reuse retired ParticipantIds immediately

Rejected: historical logs become ambiguous. R6 has no retention/reuse policy; tombstone until project-defined retention expires.

---

## 8. Friction signals and falsification result

| Signal | None / Mild / Significant | One-sentence justification |
|---|---|---|
| Artificial Wire | None | Twenty rack Wires correspond to twenty real isolated propagation domains. |
| Wire proliferation | Mild | 20 repeated Wires are physically required and template-identical, but consume global WireNumbers and records linearly. |
| Artificial hierarchy | None | MainA/MainB positions match rack control and container service roles. |
| VCN pressure | None | Each rack uses 18/31 default slots. |
| WireAlias pressure | Significant (ambiguity) | Per-Link scope is easy; deployment-global scope makes rack 9 impossible. |
| Configuration burden | Significant | ~440 unique rack records plus multi-vendor ownership and field lifecycle exceed a casual generated-manifest story. |
| Failure/topology mismatch | Mild | Rack/container failures map cleanly; replaced-vs-unreachable lifecycle does not. |

### Falsification outcome

| Criterion | Result |
|---|---|
| Immutable vendor collisions resolved | **Fail** |
| Repeated factory IDs projected transparently | **Fail** |
| Replacement lifecycle specified | **Fail** |
| Three containers join transparently | **Fail** |
| Ordinary address range fits Config C | **Fail** |
| ≤20 unique records/rack | **Fail** (~22) |
| Rack communication structure natural | Pass |
| CAN11 default map natural | Pass |

**Final verdict:** R6's communication model is clean inside a coordinated rack/container deployment, but R6 identity/commissioning is **not sufficient for this commercial multi-vendor field-service domain**. The honest current boundary is one container WireSpace with composition at the plant boundary — and even that requires vendors to accept deployment role assignment or to remain behind composition facades.

---

## 9. Model pressure

- **If you could change one WS concept** to simplify this system, what would it be? Specify the missing boundary between immutable hardware/vendor identity and deployment role identity, including collision-free projection, replacement lifecycle, and explicit identity translation between independently commissioned WireSpaces. This is a deployment/identity architecture requirement, not dynamic routing.
- **Did WS expose a useful distinction** the conventional model obscures? Yes: canonical role PID vs hardware UUID is a useful way to preserve functional role through replacement, and composition makes legacy/vendor boundaries visible. The trial also exposes exactly where transparent identity stops.

---

## 10. Open questions

- Is committed WireAlias uniqueness per physical classifier or deployment-global? This must be normative.
- Can a constrained CAN11 LLL project immutable vendor-local identities into canonical deployment PIDs without modifying device configuration? Current documents do not supply that cross-universe mechanism.
- What authoritative artifact records replacement, PID tombstones, hardware UUID history, and rollback?
- How are Endpoint capability/version differences validated when later rack revisions add Endpoints?
- Does the product require plant EMS direct access to module Services, or is container-facade inventory sufficient?
- If extended ParticipantIds are adopted, what exact CAN11 binding/profile version stores wider role-position PIDs?

---

## 11. Spec findings

| ID | Finding |
|---|---|
| SF-R6-018 | Immutable colliding multi-vendor identity universes cannot be merged by forwarding or splices; commissioning reassignment is commercially unavailable and the explicit translator is deferred. |
| SF-R6-019 | Field replacement needs a specified role-PID ↔ hardware-UUID lifecycle transition; static membership currently distinguishes unreachable, not replaced/retired/tombstoned. |
| SF-R6-020 | Config C has 562 listed Participants, exceeding 254 ordinary IDs; separate container WireSpaces plus composition works only at container-service granularity, while transparent access requires extended addressing or translation. |
| SF-R6-014 | Confirmed independently: twenty repeated RackCAN buses require clarification that committed WireAlias values are local to a physical classifier; a deployment-global reading fails at rack 9. |

---

## 12. Diagrams

### Config B mapping

```text
ContainerGateway 0x02 ── W_ContainerServices / Ethernet
          |
ContainerController 0x01
          |
          +── W_ContainerControl / ContainerCAN
          |      ├── RackController ×20
          |      ├── PCS / HVAC / Aux
          |      └── one Critical fire-trip broadcast
          |
          +── W_Rack_01 ... W_Rack_20 (each also on ContainerCAN)
                    |
              RackController_rr  [MainA, forwarding]
                    |
              RackCAN_rr / CAN11 Alias 1
                    |
              8 ModuleMonitors   [Node0..7]

ContainerController = MainB on every rack default map.
```

### Identity boundary

```text
Vendor A universe       Vendor B universe       Integrator universe
 repeated 0x10...         PCS 0x10...            role PID plan
       |                       |                      |
       +------ collision ------+----------------------+
                               |
                  current R6 has no translator
                               |
              either vendor renumbering (refused)
              or composition facades (identity changes)
              or not resolvable transparently
```

### Config C recommended scope

```text
Container WireSpace 1 ── Gateway facade 1 ──┐
Container WireSpace 2 ── Gateway facade 2 ──┼── W_Plant ── Plant EMS
Container WireSpace 3 ── Gateway facade 3 ──┘

Plant sees container-level authored Services.
Plain forwarding does not cross identity universes.
```


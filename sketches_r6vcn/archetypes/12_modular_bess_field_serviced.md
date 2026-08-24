# Archetype 12 — Modular Grid Battery Storage, Field-Serviced and Multi-Vendor

**ID:** 12
**Convergence test:** **Yes** — assign 2–3 independent mapping agents
**Suggested Link mix:** Classical CAN11 (rack-internal, repeated ~20×) + CAN-FD (container control) + Ethernet (container/plant) + Modbus RTU serial (legacy subsystems) + shared memory / IPC
**Stress:** A system whose participant set is **not known at design time**, changes during field service, repeats identical substructures dozens of times, spans identity universes the integrator does not control, and grows past the ordinary canonical address range
**Class:** **Adversarial archetype** — see §0

**Scale target:** approximately **43 / 187 / 565 production WS Participants** for the 4-rack, 20-rack, and 3-container configurations. The 20-rack figure is deliberately close to the ordinary ParticipantId ceiling; the 3-container figure deliberately exceeds it.

**Primary question:** Does deployment-global static Participant identity survive a system where **nobody knows the participant set at design time, hardware is replaced from generic inventory during field service, identical substructures repeat twenty times, and two subsystems arrive pre-numbered by vendors who will not renumber for you?**

---

## 0. Adversarial intent — read this first

Archetypes 01–11 all handed the mapping agent a complete, stable, single-vendor participant list drawn up by one engineering team that controlled the entire deployment. Every one of them passed. A review of the campaign identified this as an unnamed validity threat: the corpus may be measuring the model's comfort zone rather than its limits.

**This archetype is written to fail.** It targets the single most load-bearing R6 decision — *deployment-global static Participant identity within one coordinated identity universe* — and constructs a system that a large fraction of the real embedded market actually looks like, in which that decision is under genuine strain.

Mapping agents must **not** try to make this archetype pass. A mapping that reports "WireSpaces fits naturally here" will be treated as evidence of agent bias unless it engages every question in §11 with arithmetic. A mapping that concludes "the model needs a commissioning/identity architecture it does not currently have," or "WireSpaces should not extend above the container boundary," is a **successful** trial outcome.

Do not invent protocol features to rescue the archetype. Report the gap.

---

## 1. Falsification requirement (mandatory deliverable)

**Before** producing the mapping, the agent must write a short section titled **"Falsification criteria"** stating what specific, observable outcomes in *this* archetype would constitute evidence that WireSpaces R6 is the wrong tool, or that a named R6 mechanism is defective.

Suggested form:

```text
I will judge R6 identity DEFECTIVE for this domain if:
    - <criterion>
    - <criterion>

I will judge R6 identity SOUND for this domain if:
    - <criterion>
```

Then map the system, then report honestly against the criteria the agent itself wrote.

A mapping that omits this section, or that quietly rewrites its criteria after the fact, is invalid.

---

## 2. Prohibited escape hatches

The following moves have appeared in earlier sketches and are **not permitted** here. Each is a way of making a hard problem disappear rather than answering it.

| Prohibited | Why |
|---|---|
| "Codegen can generate the configuration" | Generated configuration still has conceptual size and still must be correct across field service. Count it. |
| "Bandwidth is ample" | Only after arithmetic. State the numbers. |
| "The application decides" — for identity, commissioning, or address assignment | Identity assignment is a *deployment* concern the model must have a position on. Punting it to the application is the finding, not the answer. |
| "Out of scope" for spare-part replacement, rack addition, or container joining | These are the archetype. |
| Assuming a benevolent, omniscient Organizer that already knows every device | State exactly what the Organizer must know, when it must know it, who operates it, and what happens when it is wrong. |
| Renumbering a vendor's pre-assigned identities | Vendors A and B will not do this. See §6. |
| Silently using extended canonical addressing | Permitted **only** where the agent shows the ordinary range is exhausted, and only with an explicit accounting of what that costs on the rack-internal CAN11 buses. |
| Declaring a hard failure without arithmetic | "This does not fit" also requires numbers. |

---

## 3. Native system

A grid-scale battery energy storage system (BESS), built by a **system integrator** who owns neither the battery racks nor the power conversion equipment.

### Physical structure

```text
                              Plant EMS  (Config C only)
                                   |
                          plant Ethernet / fiber
                    +--------------+--------------+
                    |              |              |
              Container 1     Container 2     Container 3     (Config C)
                    |
        +-----------+-----------------------------------+
        |                                               |
  ContainerGateway ------ ContainerEth ---- ContainerController
                               |                    |
                               |            ContainerCAN (CAN-FD)
                               |               |    |     |     |
                               |             Hvac  Aux  Pcs   (20x RackController)
                               |                          |
                          PcsController                   |
                          PcsInverterA/B                  |
                                                          |
                                              +-----------+-----------+
                                              |  RackCAN_01 (CAN11)   |
                                              |  RackController_01    |
                                              |  ModuleMonitor_01_1   |
                                              |  ...                  |
                                              |  ModuleMonitor_01_8   |
                                              +-----------------------+

                                              ... RackCAN_02 .. RackCAN_20
                                                  identical structure

  Legacy serial (Modbus RTU, never WireSpaces):
        FireGasPanel      ---- RS-485 ---- ContainerController
        RevenueMeter      ---- RS-485 ---- ContainerGateway   (Config C)
```

Each **rack** is a self-contained assembly: one rack controller and eight module monitors on a rack-internal Classical CAN bus. Twenty racks per container. **The rack structure is byte-identical across all racks, all containers, and all projects this integrator ships.**

### Configurations

**Config A — minimum container (4 racks).** Smallest shippable product. ~43 production Participants.

**Config B — full container (20 racks).** The normal product. ~187 production Participants. Map this first.

**Config C — three-container plant.** Three containers, **each independently commissioned at a different time by a different crew**, later joined under one plant EMS. ~565 production Participants. Containers 1 and 2 were commissioned eighteen months apart and neither crew coordinated identity assignment with the other.

**Config D — field service events** (applied to Config B, one at a time):

1. **Module monitor replaced.** `ModuleMonitor_07_3` fails. A field technician installs a spare pulled from a truck. The spare is a generic part from Vendor A's inventory carrying factory-default configuration and a different hardware serial number / UUID.
2. **Rack added.** The customer expands from 12 racks to 16 racks two years after commissioning. The four new racks are a **later hardware revision** with three additional Endpoints and a newer WireSpaces implementation version.
3. **Rack retired.** A rack is removed permanently after a cell failure. Its identities become unused.
4. **Whole rack replaced under warranty.** Nine participants at once, all from generic inventory.
5. **Container gateway replaced.** The forwarding element itself is swapped.

---

## 4. Device capabilities

| Participant class | Count (Config B) | Responsibilities |
|---|---|---|
| `ModuleMonitor_rr_m` | 160 | Cell voltage/temperature summary, module SOC estimate, balancing state, module fault, isolation reading, Identity/Version/Health, field-updatable firmware |
| `RackController_rr` | 20 | Rack string voltage/current, rack SOC/SOH, contactor command and state, rack protection limits, rack fault aggregation, owns the rack-internal CAN, field-updatable |
| `ContainerController` | 1 | Container-level power allocation across racks, thermal coordination, protection interlock, legacy Modbus master for `FireGasPanel`, container fault aggregation |
| `ContainerGateway` | 1 | Container ↔ plant boundary; owns container Ethernet and (Config C) plant Ethernet; forwarding; Link status |
| `PcsController` | 1 | Power conversion command/state, grid-side measurements, ride-through state, **Vendor B identity** |
| `PcsInverterA` / `PcsInverterB` | 2 | Per-inverter power/thermal/fault state, **Vendor B identity** |
| `HvacController` | 1 | Thermal management, setpoints, condensation/humidity state |
| `AuxIO` | 1 | Door/access sensors, emergency-stop position, aux power state, lighting |

**Non-WireSpaces devices** (present, will never speak WS, reachable only through composition):

| Device | Link | Notes |
|---|---|---|
| `FireGasPanel` | Modbus RTU / RS-485 | Third-party certified panel. Cannot be modified. Its alarm output is the fastest-priority signal in the system. |
| `RevenueMeter` | Modbus RTU / RS-485 | Utility-owned, sealed, read-only. |
| Cell-level monitor ICs | proprietary daisy chain inside each module | Below the modelling floor. Do **not** model as Participants. |

### Identity ownership — the crux

| Subsystem | Who assigns its identities today | Will they renumber for the integrator? |
|---|---|---|
| Battery racks (rack controller + module monitors) | **Vendor A** factory tooling, at manufacture, from generic stock | **No.** Racks are inventory shipped to hundreds of projects. |
| PCS (controller + inverters) | **Vendor B** factory configuration | **No.** Same platform ships worldwide. |
| Container controller, gateway, HVAC, aux I/O | The integrator | Yes |
| Plant EMS, plant-level devices | The end customer's SCADA vendor | Unknown at design time |

This is not hypothetical friction. It is the normal commercial structure of the industry.

---

## 5. Existing physical links

| Link | Type | Attachments | Config |
|---|---|---|---|
| `RackCAN_01` … `RackCAN_20` | **Classical CAN, 11-bit, 500 kbit/s** | 1 rack controller + 8 module monitors each | A (×4), B/C (×20) |
| `ContainerCAN` | CAN-FD, 1 Mbit/s | `ContainerController`, all rack controllers, `PcsController`, `HvacController`, `AuxIO` | A, B, C |
| `ContainerEth` | 100 Mbit/s Ethernet | `ContainerController`, `ContainerGateway`, `PcsController`, `PcsInverterA/B` | A, B, C |
| `PlantEth` | Gigabit Ethernet / fiber | `ContainerGateway` of each container, plant EMS | C |
| `FireBus` | Modbus RTU / RS-485 | `ContainerController`, `FireGasPanel` | A, B, C |
| `MeterBus` | Modbus RTU / RS-485 | `ContainerGateway`, `RevenueMeter` | C |
| `CtrlSHM` | Shared memory / IPC | `ContainerController` internal domains, if the mapper splits it | A, B, C |

Do not add physical Links to make the mapping easier. In particular, **the module monitors have exactly one Link: the rack-internal Classical CAN bus.** There is no service port, no Ethernet, and no second path to them.

Note the deliberate repetition: **twenty structurally identical Classical CAN11 buses in one deployment.** Archetypes 01–10 each had at most a few CAN11 buses with distinct structures.

---

## 6. Multi-vendor identity — mandatory analysis

Vendor A's racks arrive with identities already assigned. Vendor B's PCS arrives with identities already assigned. Neither vendor coordinates with the other, or with the integrator, or with the twenty other integrators buying the same parts.

The agent must state, with reference to `CORE §4.6` and the splice rules in `CORE §6`, what happens in each case:

1. **Collision.** Vendor A numbered its rack controller `0x10`. Vendor B numbered its PCS controller `0x10`. Both are now in one container.
2. **Repetition.** All twenty racks arrived from Vendor A's inventory with the *same* factory identities, because they are the same part number.
3. **Container joining (Config C).** Containers 1, 2, and 3 were each independently commissioned as a complete WireSpace. `CORE §4.6` states that a plain forwarding gateway is explicitly **not** an identity translator and that joining independently assigned WireSpaces with one "can create silent collisions in WireNumbers, ParticipantIds, and Endpoint identities."
4. **Endpoint/Namespace skew.** The four racks added in Config D.2 are a later hardware revision with three Endpoints the original twelve do not have, and a newer WS implementation.

For each case the agent must say which of the following it is, and what it costs:

```text
(a) resolved by commissioning-time reassignment    -> who does it, with what tool,
                                                      how long does it take, what
                                                      happens if it is done wrong
(b) resolved by an explicit translating gateway    -> what does that gateway do;
                                                      CORE defers this to FUTURE 12,
                                                      so state what is missing
(c) resolved by treating each container as its
    own WireSpace                                  -> then what is the plant EMS
                                                      talking to, and what happens
                                                      to Services across the boundary
(d) not resolvable under current R6                -> say so plainly
```

**Do not treat this as an authorization, trust, or security question.** It is purely a question of identity assignment and configuration ownership. Security and authentication are out of scope for this archetype (§13).

---

## 7. Standard Service expectations

| Service / capability | Expected scope |
|---|---|
| Identity / Version | Every WS Participant — including all 160 module monitors |
| Health | Every WS Participant |
| Fault / diagnostic status | Every WS Participant |
| Firmware / software update | Module monitors, rack controllers, container controller, gateway, HVAC, aux I/O; PCS by Vendor B's own tooling |
| Time synchronization | Container-level and above; racks for fault timestamp correlation (~10 ms adequate) |
| Link status | Gateway, container controller, rack controllers |
| Logging / event history | Rack controllers and above; module monitors keep a small fault log |
| Configuration | Rack protection limits, container allocation policy, thermal setpoints |
| Telemetry | Selected state at every level, aggressively summarized upward |
| Diagnostic query / RPC | Every WS Participant |
| Energy / capability state | Modules → racks → container → plant |

Note the Service *count* is unremarkable. The stress is **multiplicity**: 160 near-identical participants each exposing the full Service set, reached only through a 500 kbit/s Classical CAN bus, and each one replaceable from generic inventory.

Do not create one Wire per Service.

---

## 8. Required interactions

Count application relationships, not forwarding hops.

### Rack-internal (repeated identically ×20)

| Interaction | From | To | Rate / trigger |
|---|---|---|---|
| Module cell summary (min/max/avg V, T) | each `ModuleMonitor` | `RackController` | 2–10 Hz |
| Module SOC / balancing state | each `ModuleMonitor` | `RackController` | 0.5–2 Hz |
| Module fault / isolation reading | each `ModuleMonitor` | `RackController` | event + 1 Hz |
| Balancing / mode command | `RackController` | modules (broadcast or directed) | event / 1 Hz |
| Identity / Version / Health | each `ModuleMonitor` | `RackController`, service tooling | startup / query / 0.2 Hz |
| Firmware update | update source | each `ModuleMonitor` | maintenance only |

### Container-level

| Interaction | From | To | Rate / trigger |
|---|---|---|---|
| Rack string V / I / SOC / SOH | each `RackController` | `ContainerController` | 5–20 Hz |
| Rack power capability / limits | each `RackController` | `ContainerController` | 2–10 Hz |
| Rack power allocation | `ContainerController` | each `RackController` | 2–10 Hz |
| Contactor command / state | `ContainerController` ↔ each `RackController` | | event + 2 Hz |
| Rack fault / protection trip | each `RackController` | `ContainerController`, gateway | event |
| **Fire/gas alarm → open all rack contactors** | `ContainerController` (composed from `FireGasPanel`) | all 20 rack controllers | **event, ≤100 ms end-to-end** |
| Thermal state / setpoint | `HvacController` ↔ `ContainerController` | | 0.2–1 Hz |
| PCS power command / state | `ContainerController` ↔ `PcsController` | | 1–10 Hz, tighter during grid frequency response |
| Aux / door / e-stop position | `AuxIO` | `ContainerController` | event |

### Plant-level (Config C)

| Interaction | From | To | Rate / trigger |
|---|---|---|---|
| Plant power dispatch | plant EMS | each container | 1–10 Hz |
| Container capability / state | each container | plant EMS | 1–5 Hz |
| Plant-wide fault / trip | any container | plant EMS, peers | event |
| Fleet Identity / Health inventory | plant EMS | any participant | on demand |
| Update campaign orchestration | plant EMS | selected participants | maintenance |

**Explicitly not required:** every module monitor talking to the plant EMS continuously; every rack subscribing to every other rack; cell-level data leaving the rack; the plant EMS participating in rack protection.

---

## 9. Failure, service, and lifecycle assumptions

### Ordinary faults

- One module monitor silent: rack continues in a degraded, reduced-capability state and reports it.
- One rack controller silent: that rack is isolated; the other nineteen continue.
- `ContainerCAN` lost: rack controllers hold last commanded state and fall back to an autonomous safe state; container coordination is lost.
- `ContainerGateway` lost: container operates autonomously; plant dispatch stops.
- Plant link lost (Config C): each container continues under local policy.
- `FireGasPanel` alarm: highest priority path in the system, must not be delayed by anything, including an in-progress update campaign.

### Service and lifecycle — the hard part

The mapper must state, for each of these, exactly what configuration changes and who makes them:

```text
1. spare module monitor installed
       different hardware UUID, same functional role
       -> is this "the same Participant" or a new one?
       -> what does the Identity Service now report?
       -> does fault/log history remain correlatable?
       -> what does the model say about a REPLACED participant, as
          distinct from an UNREACHABLE one? (CORE membership is static)

2. four racks added two years later
       -> must the existing sixteen be re-commissioned?
       -> new Endpoints on new hardware revision: version skew handling
       -> are new WireNumbers needed, and from whose pool?

3. one rack retired
       -> are its ParticipantIds reused? by whom? after how long?
       -> what happens to historical logs referencing them?

4. whole rack replaced
       -> nine identities at once, all from generic inventory

5. container gateway replaced
       -> the forwarding element itself; is its configuration
          recoverable from anywhere but a backup?
```

Static membership has been a repeated strength in this corpus: an unreachable participant remains a member and no renumbering occurs. **This archetype asks whether that strength survives when the unreachable participant is not coming back and a different physical device takes its place.**

---

## 10. Bandwidth, timing, and CAN11 accounting

### Rack-internal Classical CAN, 500 kbit/s

Nine participants per bus. Structure: one authority plus eight leaves — which should be an excellent fit for the default two-Main VCN map.

The agent must report, per rack bus:

```text
relations used / 31 usable ordinary VCN values
MainA / MainB assignment (or explicit map, with justification)
QoS use
frames/s at nominal rates, as a percentage of 500 kbit/s
```

And then the part that matters:

```text
The SAME structure repeats 20 times in one container, and 60 times
in a Config C plant.

The committed CAN11 identifier provides 8 WireAlias values (0..7).
The overlay states: "Every active WireAlias has exactly one canonical
Wire binding."

Each rack-internal Wire is a distinct canonical Wire.

The agent must determine and state explicitly:
    Is the WireAlias namespace per physical CAN Link, or per WireSpace
    deployment?

    If per-Link:  20 racks are fine; say so and note the ambiguity.
    If per-deployment: the container fails at NINE racks. Report it.

The overlay's Guest section already states that Guest VCN meanings are
GLOBAL within the WireSpace deployment. Reason about whether the same
scoping applies to committed WireAlias, and what twenty identical buses
imply either way.
```

This is the sharpest single question in the archetype. Do not resolve it by assumption. State the ambiguity, state both consequences, and recommend.

### WireNumber pressure

Twenty structurally identical rack-internal Wires are twenty distinct propagation domains and therefore cannot share a WireNumber (a Wire realization must be connected). Report:

```text
WireNumbers required: Config A / Config B / Config C
Whether repeated identical substructure consuming deployment-global
WireNumbers linearly is acceptable, and at what count it stops being so
```

### Firmware update campaign

Compute it, do not wave at it:

```text
160 module monitors
~256 kB image each
rack-internal bus: 500 kbit/s Classical CAN, shared with live protection traffic

Per-rack time for 8 modules =
Whole-container campaign time =
What fraction of the bus can safely be given to update traffic while
protection traffic still meets its rates?
Does QoS help? Does it help enough?
```

### Fire/gas path

```text
FireGasPanel (Modbus RTU) -> ContainerController -> 20 rack controllers
Budget: 100 ms end-to-end, including Modbus poll latency

Report: poll interval assumption, composition hop cost, ContainerCAN
transmission of 20 contactor commands (or one broadcast), and whether
an in-flight update campaign can delay it.
```

### Ordinary rates

Container-level traffic is slow (0.2–20 Hz) and bandwidth is genuinely not the constraint there. Say so once and move on. The constraints are **identity, commissioning, repetition, and the rack-internal CAN**.

---

## 11. Expected diagnostic outcomes

### Question A — identity when N is unknown at design time

The integrator sells 4-, 8-, 12-, 16-, and 20-rack containers from one design. Does deployment-global static ParticipantId allocation work when the count is chosen per order, and later changed by the customer?

### Question B — replaced vs unreachable

`CORE` treats membership as static and absence as unreachability. A field-replaced module monitor is a *different physical device in the same role*. What does the model owe here, and does it deliver it? Consider Identity/Version reporting, fault history correlation, and whether the ParticipantId is a role or a device.

### Question C — the ordinary address ceiling

Config B consumes ~187 of 254 ordinary ParticipantIds for **one container**. Config C needs ~565.

State plainly:

```text
- What is the largest system expressible with ordinary addressing?
- Is one large container really 74% of the address space?
- For Config C: extended addressing, or three WireSpaces plus translation,
  or something else?
- If extended addressing: what does it cost on the rack-internal
  Classical CAN11 buses, where participant identity is already compressed?
```

Earlier archetypes were instructed not to use extended addressing. This one is instructed to find out where the boundary actually is.

### Question D — repeated identical substructure

Twenty identical racks. Report what is genuinely shared (firmware image, Wire *structure*, Service definitions, VCN map shape) versus what must be per-instance (ParticipantIds, WireNumbers, alias bindings, membership entries). Is the per-instance cost linear, and is it small enough that a field crew can execute it correctly?

### Question E — multi-vendor identity universes

Answer §6 completely. This is the question most likely to expose a genuine architectural hole, since `CORE §4.6` states the problem and `FUTURE §12` defers the solution.

### Question F — commissioning mechanics

What must exist, concretely, for a two-person crew to bring up a 20-rack container in a shipping container in a field with no network? Who holds the identity plan? What tool writes it? What happens when a rack is installed in the wrong physical slot? The answer "the Organizer handles it" is not acceptable without saying what the Organizer must know and when.

### Question G — brownfield legacy boundary

`FireGasPanel` and `RevenueMeter` will never speak WireSpaces. Is composition at `ContainerController` / `ContainerGateway` clean, and does the safety-relevant timing survive the extra hop? Does the WS Service model make the legacy boundary better or merely relocate it?

### Question H — fleet lifecycle and version skew

Hardware revisions with additional Endpoints; WS implementation versions differing across racks installed years apart; update campaigns across 160 identical nodes. Does the model have anything to say, and if not, is that a gap or correctly out of scope?

### Question I — the fast path in a slow system

Everything here is slow except the fire/gas trip. Does a 100 ms safety path coexist with a multi-hour update campaign under flood-and-filter plus 2-bit QoS? Show the arithmetic.

### Question J — where should WireSpaces stop?

A legitimate and possibly correct outcome is a **scope statement**: WS is right inside the rack and the container, and wrong above it. If the agent believes that, say it clearly and draw the line, rather than stretching the model to the plant EMS because it is theoretically possible.

---

## 12. Valid experiment outcomes

Genuinely valid results include the following. Several are unflattering; that is the point.

```text
"R6 identity is sound inside a container but has no commissioning or
 identity-assignment architecture, which is where the real cost lives.
 The model needs a deployment story, not a protocol change."

"Static membership does not distinguish a replaced device from an absent
 one. For field-serviced systems that distinction is essential and the
 model currently has no position on it."

"The 8-bit ParticipantId is a hard product boundary at roughly one large
 container. Multi-container plants must either use extended addressing --
 with consequences on CAN11 -- or be separate WireSpaces, and the
 translating gateway that would join them does not exist yet."

"Repeated identical substructures consume deployment-global WireNumbers
 and alias bindings linearly with hardware count. This is a scaling defect
 for modular products, independent of participant count."

"The WireAlias namespace scoping is ambiguous in the overlay. Under one
 reading, a container fails at nine racks."

"Multi-vendor pre-assigned identity is not resolvable under current R6
 without either renumbering (commercially impossible) or a translating
 gateway (specified nowhere)."

"WireSpaces should stop at the container boundary. Above it, the plant
 network is a different problem with different assumptions."

"The rack-internal Classical CAN bus is a good CAN11 positive control
 twenty times over, and that repetition is the most encouraging result
 here -- but it is also the thing that stresses alias scoping."

"Field service is where this architecture will actually be judged, and
 the sketch corpus has never looked at it."
```

Outcomes that would be **suspicious** and require unusually strong evidence:

```text
"Everything fits naturally."
"Configuration burden is mild."
"Identity worked without any commissioning discussion."
```

---

## 13. What this archetype does not include

Out of scope for WireSpaces here:

- security, authentication, authorization, and access control of any kind;
- utility/grid interconnection protocols and compliance (IEEE 1547-class);
- cell chemistry, cell balancing algorithms, SOC/SOH estimation math;
- cell-monitor IC daisy chains inside a module;
- PCS internal power-electronics control loops;
- fire suppression certification and safety-case argumentation;
- thermal/HVAC control algorithms;
- revenue metering accuracy and settlement;
- cloud/fleet analytics infrastructure;
- Modbus protocol design;
- physical installation, cabling, and enclosure engineering.

Also excluded unless separately assigned:

- dynamic routing;
- ad-hoc wireless;
- security or trust boundaries between vendors (identity assignment only);
- modelling cell monitors as Participants;
- inventing a commissioning protocol — **describe what is missing instead**.

Note what is deliberately **in** scope, against earlier archetypes: participant sets unknown at design time, field replacement, hardware revisions, identity universes the integrator does not own, and extended canonical addressing where the agent shows it is forced.

---

## 14. Configuration / accounting requirements

```text
Production Participants:
    Config A / B / C counts, by class
    percentage of ordinary ParticipantId range consumed, per config

Physical Links:
    count by type, including the 20 repeated rack buses

Canonical Wires:
    count, purpose, member Links, participant scope
    how many are structurally identical repeats
    WireNumbers consumed per config

CAN11 accounting, per rack bus AND in aggregate:
    relations / 31 usable ordinary VCNs
    default vs explicit map
    WireAlias bindings required in the deployment
    explicit statement of alias namespace scoping and its consequence

Gateway / forwarding state:
    Wire -> LinkBitmask objects (one per gateway per Wire; do NOT
    inflate by counting each branch separately)
    splices, composition points

Commissioning inventory:
    what a field crew must configure per rack
    what is shared across racks
    total per-container configuration actions

Field-service deltas:
    configuration change for each Config D event
    who performs it and with what

Standard Services:
    ubiquitous vs capability-specific
    cost of 160 near-identical participants exposing the full set

Scaling:
    4 -> 20 racks
    1 -> 3 containers
    identify anything that is worse than linear
```

Generated configuration is allowed. Its size and conceptual burden still count.

---

## 15. Comparison to archetypes 10 and 11

| | **10 — Automotive Zone** | **11 — Redundant eVTOL** | **12 — Modular BESS** |
|---|---|---|---|
| Production Participants | ~20 | ~26 / 30 / 38 | ~43 / 187 / 565 |
| Participant set known at design time | Fully | Fully | **No** |
| Single vendor | Yes | Yes | **No — three identity owners** |
| Field replacement from generic stock | No | No | **Routine** |
| Identical repeated substructures | Moderate | 4/8/16 leaves | **20 nine-node racks** |
| CAN11 buses | 1 committed + 1 Guest | None | **20 identical committed** |
| Ordinary address range | Comfortable | Comfortable | **74% for one container; exceeded at plant scale** |
| Redundancy | Moderate | Extreme | Minimal |
| Timing pressure | Moderate | High | Low, except one safety path |
| Main stress | Heterogeneous integration | Redundancy + loops | **Identity, commissioning, repetition, lifecycle** |
| Written to | Explore | Explore | **Break the model** |

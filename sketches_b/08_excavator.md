# WireSpaces Sketch 08 — Battery-Electric Compact Excavator

**Status:** Phase-1 independent mapping  
**Basis:** Frozen Sketch 08 agent brief only  
**Cardinality assumption:** Two drive inverter/controller ECUs and two valve controllers, as directed for this assignment  
**Scope:** Semantic mapping only. Bit layouts, carrier-local address projection, commissioning, exhaustive Endpoint catalogs, and formal safety analysis are excluded.

**Mapping:** Six multi-Origin Wires: Operator, Chassis Coordination, Powertrain Control, Hydraulic Control, Safety I/O, and Machine Platform.  
**Worst friction:** Mild — forwarding and operation-specific authorization generate nontrivial tables, but compact authored intent remains possible.  
**Main lesson:** A hierarchical machine can naturally require multi-Origin on every Wire; broad Wire membership remains compatible with narrow operation-specific authority, but Wire-scoped broadcast can become the limiting abstraction.  
**Configs:** A, B, C

---

# 1. Native communication summary

This section intentionally uses native machine terminology rather than WireSpaces terminology.

The excavator has a hierarchical physical network with overlapping production authority:

- the HMI supplies operator intent only to the VCU;
- the VCU coordinates normal machine motion, braking, traction, operating mode, and high-level hydraulic functions;
- the Safety ECU independently supervises the machine and may issue narrowly bounded inhibit, controlled-stop, and limited emergency-retreat operations;
- the BMS constrains traction and regenerative power without selecting machine trajectory;
- the Brake ECU reports brake availability and applies its own local protection;
- the Hydraulic Zone Gateway accepts high-level hydraulic requests, composes them into valve and pump control, and retains local protective authority;
- smart drive and hydraulic controllers enforce local limits and report availability and faults;
- the Safety I/O controller supplies an independent safety path, including a non-network hardwired drive inhibit;
- RS-485 hydraulic sensors and LIN cabin/simple-I/O nodes remain non-WS peripherals owned by gateway application services.

The native topology contains three kinds of cross-interface behavior:

1. **Unchanged network transfer.** The VCU, Hydraulic Gateway, and Safety ECU pass selected canonical interactions between WS-capable segments without changing their semantic identity.
2. **Local termination and composition.** HMI intent terminates at the VCU; high-level hydraulic requests terminate at the Hydraulic Gateway. New downstream commands are separately authored interactions.
3. **Native-protocol adaptation.** The Hydraulic Gateway polls or controls non-WS RS-485 and LIN peripherals and exposes gateway-owned machine-level services.

Authority is concurrent, not transferred at runtime. Safety inhibition may coexist with normal VCU intent, BMS constraints may coexist with torque requests, and gateway-local protection may coexist with high-level hydraulic requests. Deterministic selection occurs at the receiving function; packet arrival order has no authority meaning.

No failure promotes another controller into the VCU role. In particular, loss of the VCU removes normal machine coordination, cross-chassis/powertrain WS transfer, update orchestration, and limited emergency-retreat command reach. It does not remove independent hardwired drive inhibit, local hydraulic protection, or powertrain-local constraints.

---

# 2. Participants

Each listed WS-capable ECU has exactly one Endpoint Domain and therefore one deployment-global `ParticipantId`.

| ParticipantId | Endpoint Domain | Character and responsibility |
|---:|---|---|
| `1` | Vehicle Control ECU (VCU) | Normal machine coordinator; update orchestrator; time source; VCU-side embedded gateway |
| `2` | Operator HMI | Operator-intent source and production display |
| `3` | Safety ECU | Independent safety monitor and narrow safety-command authority; safety-side embedded gateway |
| `4` | Brake ECU | Brake controller, availability source, and local brake protection |
| `5` | Hydraulic Zone Gateway | Hydraulic request terminator, composer, local protector, protocol adapter, and embedded gateway |
| `6` | BMS | Energy, isolation, thermal, charge/discharge, and regen constraint authority |
| `7` | Drive Inverter A | Torque/regen execution, local constraint enforcement, state/fault source |
| `8` | Drive Inverter B | Torque/regen execution, local constraint enforcement, state/fault source |
| `9` | Valve Controller A | Local hydraulic actuation and local availability/fault source |
| `10` | Valve Controller B | Local hydraulic actuation and local availability/fault source |
| `11` | Pump Controller | Local pump actuation and local availability/fault source |
| `12` | Safety I/O Controller | Dedicated emergency-stop/steering safety I/O and hardwired inhibit control |

RS-485 sensors and LIN nodes receive no `ParticipantId`: they have no WS Endpoint Domain. Their machine-visible identities belong to services authored by Participant 5.

---

# 3. Physical Link Interfaces and Wires

## 3.1 Logical Wires

The mapping uses six Wires. Four carry primary application interactions, one is a peripheral safety-control scope, and one is a supporting platform scope.

| Wire | Members | Physical realization | System role | Origins needed | Permitted Origins |
|---|---|---|---|---:|---|
| `W1 Operator` | P1 VCU, P2 HMI | Operator Ethernet | Primary: operator intent, machine presentation, HMI-local platform services | **2** | P1, P2 |
| `W2 Chassis Coordination` | P1 VCU, P3 Safety, P4 Brake, P5 Hydraulic Gateway | Chassis CAN-FD | Primary: cross-domain machine mode, braking, safety constraints, high-level hydraulic coordination | **4** | P1, P3, P4, P5 |
| `W3 Powertrain Control` | P1 VCU, P3 Safety, P6 BMS, P7 Drive A, P8 Drive B | Virtual Wire over Chassis CAN-FD and Powertrain CAN-FD, forwarded by P1 | Primary: traction/regen, safety powertrain operations, BMS limits, drive state | **5** | P1, P3, P6, P7, P8 |
| `W4 Hydraulic Control` | P1 VCU, P3 Safety, P5 Hydraulic Gateway, P9 Valve A, P10 Valve B, P11 Pump | Virtual Wire over Chassis CAN-FD and Hydraulic CAN-FD, forwarded by P5 where identity is unchanged | Primary: high-level hydraulic requests, local actuator control, hydraulic constraints, subsystem state/protection | **6** | P1, P3, P5, P9, P10, P11 |
| `W5 Safety I/O` | P3 Safety, P12 Safety I/O | Safety I/O CAN-FD | Peripheral but safety-critical: dedicated safety sensing, actuation, and supervision | **2** | P3, P12 |
| `W6 Machine Platform` | P1 VCU, P3 Safety, P4 Brake, P5 Hydraulic Gateway, P6 BMS, P7 Drive A, P8 Drive B, P9 Valve A, P10 Valve B, P11 Pump, P12 Safety I/O | Virtual Wire over Chassis, Powertrain, Hydraulic, and Safety I/O CAN-FD; forwarded unchanged by P1, P5, and P3 | Supporting: inventory, health, update, runtime/link telemetry, events/logs/fault history, and time quality | **11** | P1, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12 |

**All 6 of 6 Wires require multiple Origins.** This includes a strongly hierarchical control machine, its peripheral Safety I/O scope, and its supporting platform scope. Multi-Origin is therefore not confined to peer networks in this mapping.

`W2` is intentionally broader than a separate Wire per chassis function. Its four members share one physical and failure scope, and all four consume cross-domain mode, availability, safety, brake, or hydraulic coordination. Its authority policy remains a short service-family list rather than an all-to-all command matrix.

`W4` also requires an explicit comparison against splitting it into:

```text
Hydraulic Coordination:  P1, P3, P5
Hydraulic Actuation:     P5, P9, P10, P11
```

The application-composition boundary at P5 is real: P1's high-level request terminates there, and P5 authors new valve/pump commands. That fact alone does not require a Wire boundary, because a Wire describes communication and broadcast scope rather than an application processing pipeline. One `W4` remains preferable here for a stronger reason: P5's protective state has a meaningful common audience across both sides. P1 and P3 consume subsystem degradation while P9–P11 consume the corresponding neutralize, inhibit, relief, or shutdown state; all are members of one hydraulic failure scope. The same Wire also permits selected leaf state to retain canonical identity when it genuinely must cross P5.

This decision is conditional, not thematic. If hydraulic protection/state were always transformed into separate upstream summaries and downstream commands, with no unchanged interaction or common broadcast audience across P5, the two-Wire decomposition would be clearer. “All participants are hydraulic” would not justify combining them.

`W6` is intentionally broad because platform identity, health, update, telemetry, logs/events, and time quality are common production obligations. Its broad membership does not imply broad command authority: update and time authority remain exclusively with P1, while each other member may author only its own bounded platform state and request-scoped replies.

## 3.2 Physical segments and local Link Interfaces

| Frozen segment | Local WS Link Interfaces | Wires carried | Routing/application boundary |
|---|---|---|---|
| Operator Ethernet | P1, P2 | `W1` | Operator intent and production display terminate at P1/P2. No raw operator command is forwarded onto CAN. |
| Chassis CAN-FD | P1, P3, P4, P5 | `W2`, chassis portions of `W3`, `W4`, and `W6` | Shared cross-domain segment. P1 forwards selected `W3/W6`; P5 forwards selected `W4/W6`; P3 forwards selected `W6`. |
| Powertrain CAN-FD | P1, P6, P7, P8 | powertrain portions of `W3`, `W6` | P1 is the only WS path to Chassis CAN-FD. |
| Hydraulic CAN-FD | P5, P9, P10, P11 | hydraulic portions of `W4`, `W6` | P5 forwards unchanged traffic where configured and separately composes high-level requests into new actuator interactions. |
| Hydraulic sensor RS-485 | P5 plus non-WS sensors | No canonical WS Wire beyond P5 | P5 terminates the native sensor protocol and authors gateway-owned sensing services. |
| Cabin/simple-I/O LIN | P5 plus non-WS simple I/O | No canonical WS Wire beyond P5 | P5 terminates the native protocol and authors gateway-owned cabin/simple-I/O services. |
| Safety I/O CAN-FD | P3, P12 | `W5`, safety-I/O portion of `W6` | P3 forwards selected platform interactions on `W6`; safety application exchanges use `W5`. |

The hardwired drive-enable/inhibit path from the Safety I/O controller to the drive inverters is outside WS and is not represented as a Wire.

---

# 4. Representative interactions

Direction remains structural in every row: the named Origin authors that interaction. A reply or status publication has its own explicit authorization; it is not authorized by reversing Direction.

| Interaction | Actual Origin | Required sink(s) | Wire and scope | Transport/freshness need | Forwarding or composition |
|---|---|---|---|---|---|
| Operator joystick, mode, enable, auxiliary intent | P2 HMI | P1 VCU | `W1`, addressed | Bounded latest-state intent with sequence/freshness; mode transitions may require acknowledged operation | Terminates at P1 |
| Machine state, availability, warnings, alerts | P1 VCU | P2 HMI | `W1`, addressed or HMI-only publication | Periodic latest-state plus reliable discrete alerts | New VCU-authored presentation, not relayed raw subsystem traffic |
| Normal traction torque/regen request | P1 VCU | P7, P8 drives | `W3`, addressed fanout to each required drive sink | Bounded cyclic command with sequence, validity window, and timeout | P1 local transmit onto Powertrain CAN-FD |
| Normal braking/deceleration request | P1 VCU | P4 Brake | `W2`, addressed | Bounded cyclic/transactional control with validity window | Direct on Chassis CAN-FD |
| High-level boom/arm/bucket/auxiliary request | P1 VCU | P5 Hydraulic Gateway | `W4`, addressed | Bounded latest-state function request with mode and requested limits | Local delivery at P5; not forwarded to leaves |
| Safety torque/hydraulic inhibit or controlled stop | P3 Safety | P1, P4, P5 as applicable; P7/P8 for powertrain inhibit | `W2` for chassis targets; separately authored, addressed `W3` fanout to required drives | Safety state with explicit validity and deterministic precedence; delivery cannot rely on last-packet order | Each `W3` PDU is forwarded unchanged Chassis→Powertrain by P1 while P1 path is healthy |
| Limited emergency-retreat command | P3 Safety | P7/P8 drive controller function(s) | `W3`, narrowly addressed and conditionally authorized | Explicit operation distinct from normal torque; bounded validity and positive state checks | Forwarded unchanged by P1; unavailable when P1 is down |
| BMS charge/discharge, regen, thermal, isolation limits | P6 BMS | P1 VCU and P7/P8 drives | `W3`, addressed fanout to the explicitly required enforcing sinks | Latest valid constraint with version/freshness and fail-safe expiry semantics | Powertrain-local delivery; P1 receives directly |
| Drive state, thermal limit, availability, fault | P7 or P8, individually | P1 VCU; P3 Safety where safety consumption is configured | `W3`, source-specific publication | Periodic latest-state plus reliable fault event | P3-bound traffic is forwarded unchanged Powertrain→Chassis by P1 |
| Brake availability/degraded capability and fault | P4 Brake | P1 VCU and P3 Safety | `W2` | Latest-state availability plus reliable discrete fault event | Direct |
| Local valve/pump command | P5 Hydraulic Gateway | P9/P10/P11 as applicable | `W4`, addressed or hydraulic-leaf broadcast for an identical protective state | Bounded cyclic actuator command with freshness and timeout | New interaction composed by P5 after high-level request termination |
| Smart hydraulic leaf limit/fault/availability | P9, P10, or P11 | P5 Hydraulic Gateway | `W4`, addressed publication | Latest-state constraint plus reliable event | Direct on Hydraulic CAN-FD |
| Gateway-local neutralize/inhibit/pressure relief/shutdown | P5 Hydraulic Gateway | Affected P9/P10/P11; P1/P3 as status consumers | `W4` | Protective state dominates normal hydraulic request; reliable event accompanies latest-state protection | New P5-authored protective interaction, not forwarded input |
| Hydraulic subsystem state/protective event | P5 Hydraulic Gateway | P1 VCU, P3 Safety; affected leaves for common inhibit state | `W4` | Latest-state summary plus reliable event | P5 originates; fanout crosses both P5 interfaces without semantic transformation |
| Hydraulic sensor state | P5 Hydraulic Gateway | P5 local hydraulic composition; P1/P3 where machine-level state is required | `W4` for network-visible projection | Sample timestamp/quality and freshness required | Native RS-485 terminates at P5; P5 authors the WS service |
| Cabin/simple-I/O state or command | P5 Hydraulic Gateway | P5-owned application functions and relevant chassis consumers | `W2` when machine-visible | Bounded state/event semantics | Native LIN terminates at P5; P5 authors the WS service |
| Safety input/status | P12 Safety I/O | P3 Safety | `W5` | Bounded safety state with freshness; discrete event as needed | Direct |
| Safety I/O actuation/supervision request | P3 Safety | P12 Safety I/O | `W5` | Explicit safety operation with bounded validity | Direct; hardwired drive inhibit remains separate |
| Firmware update control/chunk | P1 VCU | One selected WS target | `W1` for P2; `W6` for P3–P12 | Reliable, ordered, integrity-protected, resumable/segmented transfer in explicit maintenance state | Forwarded unchanged through P5 or P3 for downstream targets; P1 directly reaches powertrain |
| Firmware update status | Selected target | P1 VCU | Same Wire as update target | Explicit request/reply or reliable status; source identity preserved | Forwarded unchanged where required |
| Machine time synchronization | P1 VCU | P2 on `W1`; all `W6` members on `W6` | Scoped broadcast on each Wire | Periodic synchronization plus timestamp-quality state | P1 originates; gateways forward `W6` unchanged |
| Identity/version inventory request and reply | P1 VCU / selected target | Selected target / P1 | `W1` or `W6` | Bounded request/reply | Forwarded unchanged where target is downstream |
| Health and heartbeat | Each substantial ECU for its own state | P1 normally; immediate subsystem owner where operationally required | `W1`, `W2`–`W5` for control-relevant health, or `W6` for platform health | Periodic latest-state with freshness | Platform traffic may forward; control-relevant availability remains on the corresponding application Wire |
| Link telemetry, OS/runtime telemetry, logs, events, fault history | ECU owning the data | P1 normally; P3 or P5 for their downstream segment where required | `W1` or `W6` | Snapshot telemetry; bounded logs/events; persistent history queried reliably | Forwarded unchanged on `W6`; never treated as command authority |

---

# 5. Permitted-Origin and Endpoint authorization policy

## 5.1 Authored policy

Authorization is expressed as `(Wire, Endpoint/service operation, permitted Origin, permitted sink)` rules. Membership alone never grants operation authority. The receiver rejects a structurally valid PDU whose Origin is not permitted for that operation.

The following sets are shorthand only; each expands to the listed Participants in generated configuration:

- `DRIVES = {P7, P8}`
- `HYD_LEAVES = {P9, P10, P11}`
- `CHASSIS_CONTROLLERS = {P1, P3, P4, P5}`
- `PLATFORM_TARGETS = {P3, P4, P5, P6, P7, P8, P9, P10, P11, P12}`
- `W6_MEMBERS = {P1, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12}`
- `W6_NON_VCU = {P3, P4, P5, P6, P7, P8, P9, P10, P11, P12}`

| Wire | Endpoint/service operation | Permitted Origin(s) | Permitted sink(s) | Authored limit |
|---|---|---|---|---|
| `W1` | `OperatorIntent.latest` | P2 | P1 | Operator intent only; no leaf command operation exists for P2 |
| `W1` | `MachinePresentation.latest/event` | P1 | P2 | Status/presentation only |
| `W1` | `HmiPlatform.inventory/health/log/updateStatus` | P2 | P1 | P2 may report only its own platform state |
| `W1` | `HmiPlatform.inventoryRequest/updateControl/timeSync` | P1 | P2 | Update requires explicit maintenance state |
| `W2` | `MachineMode.latest` | P1 | P3, P4, P5 | Normal coordination state, not safety authority |
| `W2` | `BrakeControl.normalRequest` | P1 | P4 | Normal braking only |
| `W2` | `HydraulicFunction.highLevelRequest` | P1 | P5 | No direct valve/pump operation is exposed to P1 |
| `W2` | `SafetyControl.inhibit/controlledStop` | P3 | P1, P4, P5 as configured per operation | Narrow safety operations only |
| `W2` | `SafetyState.latest/event` | P3 | P1, P4, P5 | P3's own safety state |
| `W2` | `BrakeCapability.latest/event` | P4 | P1, P3 | Brake state/constraint only |
| `W2` | `HydraulicSubsystem.latest/event` | P5 | P1, P3 | Hydraulic state/protection only |
| `W2` | `CabinIo.machineVisibleState/event` | P5 | P1 and P3 only where the signal is relevant | P5 authors adapted machine-level state; LIN nodes have no authority |
| `W3` | `DriveControl.normalTorqueRegen` | P1 | P7, P8 | Normal traction/regen and mode control |
| `W3` | `DriveSafety.inhibit/controlledStop` | P3 | P7, P8 | Narrow override; distinct from normal torque |
| `W3` | `DriveSafety.limitedEmergencyRetreat` | P3 | P7, P8 as configured | Separate operation, enabled only in the native allowed state; not arbitrary torque authority |
| `W3` | `PowerLimit.latest/isolation` | P6 | P1, P7, P8 | Constraint only; no trajectory or torque request |
| `W3` | `DriveState.latest/event` | P7 | P1, P3; P6 only for explicitly needed energy/thermal coordination | P7 reports only its own state |
| `W3` | `DriveState.latest/event` | P8 | P1, P3; P6 only for explicitly needed energy/thermal coordination | P8 reports only its own state |
| `W4` | `HydraulicFunction.highLevelRequest` | P1 | P5 | Terminates at P5 |
| `W4` | `HydraulicSafety.inhibit/controlledStop` | P3 | P5 | P3 does not acquire normal valve/pump authority |
| `W4` | `HydraulicActuator.normalCommand` | P5 | P9, P10, P11 as applicable | P5 only; derived from accepted high-level request |
| `W4` | `HydraulicActuator.protectiveCommand` | P5 | P9, P10, P11 as applicable | Neutralize/inhibit/relief/shutdown only |
| `W4` | `HydraulicLeafState.latest/event` | P9 | P5 | P9's own state/limit/fault only |
| `W4` | `HydraulicLeafState.latest/event` | P10 | P5 | P10's own state/limit/fault only |
| `W4` | `HydraulicLeafState.latest/event` | P11 | P5 | P11's own state/limit/fault only |
| `W4` | `HydraulicSubsystem.latest/event` | P5 | P1, P3 and affected HYD_LEAVES | P5's composed subsystem state/protection |
| `W4` | `HydraulicSensing.latest/quality/event` | P5 | P1, P3 where required; P5 local consumer | Gateway-owned WS service; native sensors are not Origins |
| `W5` | `SafetyIo.inputState/event` | P12 | P3 | P12's dedicated I/O state only |
| `W5` | `SafetyIo.output/supervision` | P3 | P12 | Explicit safety operations only |
| `W6` | `Platform.inventoryResponse/health/heartbeat/osTelemetry/linkTelemetry/log/event/faultHistory/updateStatus` | Each `W6_NON_VCU` member, for its own instance only | P1; additionally P3 for P12 and P5 for P9–P11 where local supervision requires it | No participant may impersonate another platform instance |
| `W6` | `Platform.health/heartbeat/timeQuality/event` for the VCU instance | P1 | P3; other chassis controllers only where their native validity policy consumes it | Exposes P1 availability without granting recipients VCU authority |
| `W6` | `Platform.inventoryRequest` | P1; P3 for P12; P5 for P9–P11 | The selected target in the requester's supervision scope | Request authority is scoped; reply authority is separately constrained above |
| `W6` | `Platform.updateControl/updateData` | P1 only | One selected `PLATFORM_TARGETS` member | Explicit maintenance/update state required; no automatic substitute |
| `W6` | `Platform.timeSync` | P1 only | All W6 members | P1 is the sole native machine time source |

Local protection inside P4, P6, P7, P8, P9, P10, P11, or P12 is not created by a network PDU and therefore is not represented by granting those Participants broader network command authority.

## 5.2 Receiver-side separation of authority

The command receivers expose source-constrained operations rather than one generic command slot:

- drive controllers distinguish P1 normal torque, P3 safety inhibit/retreat, and P6 power constraints;
- the Brake ECU distinguishes P1 normal deceleration from P3 controlled stop and from Brake-local protection;
- the Hydraulic Gateway distinguishes P1 high-level function requests from P3 safety inhibition and from its own local protective state;
- hydraulic leaves accept network actuator commands only from P5;
- the Safety I/O controller accepts safety operations only from P3;
- all update targets accept update control only from P1 and only in the explicit update state.

This keeps the source/operation decision at the receiving service boundary. It is auditable configuration, not an unrelated application callback checking a hidden sender field.

## 5.3 Organizer-generated projection

The Organizer may generate the following from the authored rules:

- per-Link membership and carrier-local address projections;
- ingress source filters and permitted-Origin checks;
- Endpoint dispatch entries and source-constrained bindings;
- target-specific unicast or broadcast egress;
- gateway forwarding entries for `W3`, `W4`, and `W6`;
- per-target firmware-update routes;
- platform rules expanded once per participant;
- compatibility fingerprints for the resulting projections.

The generated tables will be larger than the authored list because each set and multi-interface path expands per participant and per Link. That is acceptable only while tooling can show, for every generated entry, the specific authored rule that produced it. A generated forwarding or filter entry must not invent authority, infer a reply permission, or broaden an authored sink set.

---

# 6. Embedded gateway forwarding and composition

## 6.1 Ordinary forwarding tables

Only complete, unchanged canonical interactions use these forwarding entries. Ingress never grants authorization by itself.

### P1 — VCU

| Ingress | Wire | Eligible unchanged egress | Purpose and limits |
|---|---|---|---|
| Chassis CAN-FD | `W3` | Powertrain CAN-FD | P3 safety operations addressed to drives; other configured chassis-origin powertrain interactions |
| Powertrain CAN-FD | `W3` | Chassis CAN-FD | Drive/BMS state addressed to P3 or other configured chassis-side sinks |
| Chassis CAN-FD | `W6` | Powertrain CAN-FD | Platform requests/update/time toward P6–P8 |
| Powertrain CAN-FD | `W6` | Chassis CAN-FD | P6–P8 platform replies, health, telemetry, logs/events |

P1 does **not** forward `W1` operator intent onto any CAN Wire. It consumes that intent and, if valid, authors separate `W2`, `W3`, and `W4` interactions.

### P5 — Hydraulic Zone Gateway

| Ingress | Wire | Eligible unchanged egress | Purpose and limits |
|---|---|---|---|
| Chassis CAN-FD | `W4` | Hydraulic CAN-FD | Only interactions whose target is a WS-capable hydraulic participant and whose canonical identity/operation is permitted unchanged |
| Hydraulic CAN-FD | `W4` | Chassis CAN-FD | Only leaf or hydraulic interactions explicitly configured for a chassis-side sink |
| Chassis CAN-FD | `W6` | Hydraulic CAN-FD | Platform inventory/update/time requests toward P9–P11 |
| Hydraulic CAN-FD | `W6` | Chassis CAN-FD | P9–P11 platform replies, health, telemetry, logs/events |

The normal P1 high-level hydraulic request is addressed to P5 and terminates locally; it does not match a forward entry. P5's resulting valve/pump commands are new authored interactions.

### P3 — Safety ECU

| Ingress | Wire | Eligible unchanged egress | Purpose and limits |
|---|---|---|---|
| Chassis CAN-FD | `W6` | Safety I/O CAN-FD | P1 platform inventory/update/time requests toward P12 |
| Safety I/O CAN-FD | `W6` | Chassis CAN-FD | P12 platform replies, health, telemetry, logs/events |

P3 safety application commands to P12 use `W5` and originate locally. They are not forwarded chassis commands.

## 6.2 Application composition and adaptation

| Device | Input or condition | Local termination/composition | Newly originated output |
|---|---|---|---|
| P1 VCU | P2 operator intent on `W1` | Validate operator state, machine mode, limits, and coordination policy | P1 normal brake, traction, and high-level hydraulic interactions on `W2`/`W3`/`W4` |
| P5 Hydraulic Gateway | P1 high-level hydraulic function request | Resolve requested function into coordinated local actuation within accepted limits | P5 valve/pump commands on `W4` |
| P5 Hydraulic Gateway | Local pressure/temperature/actuator fault | Evaluate local hydraulic protection | P5 neutralize/inhibit/relief/shutdown and degradation state on `W4` |
| P5 Hydraulic Gateway | Native RS-485 sensor transactions | Poll, validate, timestamp, and interpret native sensor data | P5-owned hydraulic sensing state/events |
| P5 Hydraulic Gateway | Native LIN transactions | Poll/control and interpret simple cabin I/O | P5-owned cabin/simple-I/O state or commands |
| P3 Safety ECU | Safety monitoring and P12 safety input | Evaluate explicit safety policy | P3 inhibit, controlled-stop, or limited-retreat interactions on `W2`/`W3`, plus P3 safety I/O operations on `W5` |

No row in this table is ordinary forwarding. New output has a new author, operation, and application decision.

---

# 7. Broadcast and observation accounting

## 7.1 Representative broadcasts

| Broadcast | Origin | Wire | Receivers in broadcast scope | Accounting |
|---|---|---|---|---|
| Machine mode/availability | P1 | `W2` | P3 Safety, P4 Brake, P5 Hydraulic Gateway | All three are semantically interested chassis controllers |
| Chassis safety inhibit/controlled-stop state | P3 | `W2` | P1 VCU, P4 Brake, P5 Hydraulic Gateway | All must constrain affected normal functions; operation-specific authorization still applies |
| Hydraulic protective state | P5 | `W4` | P1 VCU, P3 Safety, P9 Valve A, P10 Valve B, P11 Pump | All are relevant to subsystem inhibition/degradation; function-specific commands remain addressed |
| Machine time synchronization | P1 | `W6` | P3–P12 | Common platform obligation; P2 receives a separate P1 broadcast/publication on `W1` |

`W3` deliberately uses no Wire-wide broadcast for normal torque, Safety powertrain commands, or BMS limits:

- P3 Safety uses addressed fanout to P7/P8. P6 BMS is not made a required Safety-inhibit sink.
- P6 BMS uses addressed fanout to P1/P7/P8. P3 Safety is not made a required BMS-limit sink.
- P1 normal torque uses addressed fanout to P7/P8.

The extra `W3` members are neither silently promoted to required sinks nor reclassified as observers. This exposes the important distinction:

```text
Wire membership
    != required sink
    != observer
    != broadcast audience
```

Firmware update data, operator intent, normal braking, high-level hydraulic requests, and leaf-specific actuator commands are likewise not broad broadcasts.

## 7.2 Optional configured observation

No required command, safety action, forwarding decision, failover behavior, or redundancy claim depends on observation.

Optional observation may be configured for bounded on-machine diagnostics, for example:

- P1 may observe selected P4↔P3 brake fault traffic on `W2`;
- P5 may observe selected hydraulic-leaf event traffic already addressed within `W4` for its local fault journal;
- P3 may observe drive availability publications on `W3` when that observation is useful but not required for command delivery.

If any such consumer becomes necessary for production behavior, it must be promoted from “observer” to an explicit required sink in the interaction and authorization tables. Observation does not permit a reply, does not create command authority, and does not count as redundant delivery.

---

# 8. Sink and source-selection policy

The network delivers concurrently valid authored intent. The receiving service/actuator boundary performs deterministic selection.

| Receiving function | Legitimate sources | Required selection policy |
|---|---|---|
| Drive torque/regen execution at P7/P8 | P1 normal request; P3 inhibit/controlled stop/limited retreat; P6 power/regen/isolation limits; drive-local protection | Safety inhibit/stop dominates contradictory P1 motion. P6 limits bound any P1 or permitted retreat request. Limited retreat is a separate P3 operation and never implies general torque authority. Local inverter protection remains effective. No last-packet-wins behavior. |
| Brake execution at P4 | P1 normal deceleration; P3 safety controlled stop/inhibit; Brake-local protection | P3 safety operation and Brake-local protection dominate contradictory normal P1 intent. Each source uses an explicitly distinct authorized operation. |
| High-level hydraulic acceptance at P5 | P1 normal high-level function request; P3 hydraulic inhibit/stop; P5 local protection | P3 inhibit and P5 local protection dominate P1 productive motion. P5 may continue protection without P1 but may not invent productive jobs. |
| Valve/pump execution at P9–P11 | P5 network command; leaf-local protection | Only P5 has network command authority. Leaf-local protection may clamp or reject the command and reports the resulting availability/constraint. |
| Machine coordination at P1 | P2 operator intent; P3 safety state; P4 brake constraints; P5 hydraulic availability; P6 power constraints; P7/P8 drive availability | P2 supplies intent, not actuator authority. P1 composes normal commands only while all required validity and machine-policy conditions hold. Safety and subsystem constraints bound or prohibit those commands. |
| Firmware-update target | P1 update control; target-local boot/update safety checks | Only P1 may orchestrate. The target may reject based on maintenance state, integrity, compatibility, power, or local safety, but no other Participant becomes orchestrator. |

Sequence numbers, validity windows, freshness, and operation state determine whether a source value remains valid. They do not alter source precedence.

---

# 9. Failure and degraded-state mapping

## 9.1 Required configurations

### Configuration A — Normal operation

- All six Wires have their complete configured membership and paths.
- P1 coordinates normal motion using valid P2 operator intent.
- P3 safety authority, P6 power constraints, P5 hydraulic protection, and smart-leaf constraints remain concurrently active.
- P1 forwards unchanged `W3/W6` traffic between chassis and powertrain.
- P5 forwards unchanged configured `W4/W6` traffic and separately composes hydraulic commands.
- P3 forwards selected `W6` platform traffic to P12.
- P1 orchestrates updates in explicit maintenance state and supplies machine time.

There is no runtime Origin election. Each interaction is accepted from its statically permitted Origin.

### Configuration B — VCU unavailable

- `W1` loses P1, so P2 has no operator-intent processor and no machine presentation source.
- `W2` remains physically connected among P3 Safety, P4 Brake, and P5 Hydraulic Gateway. Their explicitly authorized direct chassis interactions continue.
- `W3` is partitioned because P1's Chassis↔Powertrain forwarding is gone:
  - P3 cannot reach P7/P8 with network safety inhibit, controlled stop, or limited emergency-retreat commands;
  - P6 and P7/P8 remain mutually connected on Powertrain CAN-FD and retain BMS/drive-local constraints;
  - the independent hardwired drive inhibit remains available and can enforce disable/zero torque.
- `W4` remains connected through P5 between surviving chassis participants and hydraulic leaves, but no new normal high-level hydraulic request exists because P1 is absent. P5 local protective authority continues and cannot expand into productive machine control.
- `W5` remains fully available between P3 and P12.
- `W6` is partitioned at P1 between the surviving chassis-side tree and the isolated powertrain segment. Local platform interactions may continue within each surviving connected component.
- System firmware-update orchestration is unavailable; no replacement is elected.
- Machine time synchronization is unavailable; survivors retain local monotonic time and report degraded timestamp quality.

This is a crisp degraded state: “VCU coordination and the chassis/powertrain WS bridge are absent,” not “another Origin became VCU.”

### Configuration C — Hydraulic-side fault with VCU online

- P5 detects the local sensor/actuator condition and originates the applicable protective operation on `W4`.
- The P5 protective operation dominates contradictory P1 hydraulic motion intent for the affected function.
- P5 publishes affected-function degradation to P1/P3 and relevant leaves.
- Unaffected hydraulic functions continue only where native function-level policy allows.
- `W1`, `W2`, `W3`, `W5`, and non-affected `W6` platform reach remain available.
- P5 does not gain authority over unrelated machine functions.

## 9.2 Additional frozen failure cases

| Failure/absence | WS topology and traffic that continue | Authority/reach lost | Reassignment |
|---|---|---|---|
| HMI unavailable | P1 and all CAN-side Wires remain connected; safety and local protections continue | New operator intent and HMI display/alerts | None. P1 produces no productive motion without valid operator intent. |
| Safety ECU unavailable | P1, P4, P5 remain on `W2`; normal non-safety state may still exchange subject to native safety policy; `W3` still connects P1/P6/P7/P8 | P3 independent safety authority, `W5` supervision, P3 `W6` forwarding to P12, and hardwired safety-inhibit control | None |
| Hydraulic Gateway unavailable | `W1`, `W2` among P1/P3/P4, `W3`, and powertrain/platform paths through P1 continue | `W4` bridge and composition, hydraulic-leaf/platform reach, RS-485/LIN adaptation, P5 local protection | None |
| One hydraulic CAN leaf fails | P5 and other hydraulic leaves remain on `W4`; unrelated Wires continue | Affected actuator function and that leaf's platform state | None |
| Hydraulic RS-485 segment fails | All WS Links remain; P5 reports sensing quality/degradation | Sensor-derived hydraulic availability; affected hydraulic functions may be limited | None |
| Powertrain CAN-FD fails | `W1`, chassis-side `W2/W4/W5`, hydraulic/safety branches, and hardwired inhibit continue | Normal traction/regen and networked powertrain reach; powertrain portion of `W3/W6` unavailable | None |

“Local platform telemetry continues” means surviving members can still exchange explicitly authorized local health, inventory, link telemetry, and event traffic inside a connected component. It does not imply that the absent P1 collector or update orchestrator has been replaced.

---

# 10. Friction ratings

Scale: **None**, **Mild**, **Significant**.

| Signal | Rating | Reason |
|---|---|---|
| Artificial Origin | **None** | P1, P2, P3, P4, P5, P6, and smart leaves originate only interactions native to their stated responsibilities. No coordinator is invented to satisfy WS. |
| Artificial Wire | **Mild** | `W1`–`W5` follow real route/broadcast/failure scopes. `W6` is a logical supporting scope, but it represents a real common platform-service obligation rather than application traffic disguised as maintenance. |
| Wire proliferation | **Mild** | Six Wires cover seven physical segments and distinct semantic scopes. Splitting per command source would add artificial Wires; collapsing all application traffic would create a large permission and broadcast matrix. |
| Forwarding tax | **Mild** | Three embedded devices have forwarding obligations, but each follows the frozen physical hierarchy. The cost is most visible in `W6`; application composition remains separately visible and tooling can generate the bounded forwarding entries. |
| Identity awkwardness | **None** | One global ParticipantId per ECU cleanly identifies repeated drive/valve instances across every Wire, while non-WS peripherals correctly remain gateway-owned rather than receiving fake Participants. |
| Interaction awkwardness | **Mild** | Multi-Origin directly represents VCU commands, Safety overrides, BMS constraints, gateway protection, and leaf status on their natural scopes. Addressed fanout is still needed where broad Wire membership exceeds an interaction's true sink set. |
| Configuration burden | **Mild** | Source-constrained actuator operations and three gateway projections require generated tables. The authored policy is compact and grouped by service family, provided generated entries remain traceable to it. |
| Role instability | **None** | Origin varies normally by interaction, but no authority is elected, transferred, or reassigned in any required configuration. |
| Failure mismatch | **Mild** | Logical Wire partitioning follows loss of the native gateway or segment, and no false failover/redundancy claim is introduced. The only subtle point is reporting connected components of broad `W6` clearly after a gateway loss. |

The principal model cost is not Wire count; it is making overlapping authority auditable at drive, brake, and hydraulic receiving functions. That cost exists in the native machine and is exposed rather than hidden by this mapping.

---

# 11. Model pressure and open questions

## 11.1 Model pressure

1. **Authorization is coupled to operation, not merely Wire membership.** P1 normal torque, P3 safety operations, and P6 constraints all reach a drive controller, but they are not interchangeable commands. A per-Wire permitted-Origin set alone is insufficient; the effective rule is per service operation and sink.
2. **A broad platform Wire needs explainable generation.** `W6` avoids repeating an almost identical maintenance topology per subsystem, but its generated forwarding/filter table may be large. Audit tooling must preserve a path from each generated entry back to a compact authored rule.
3. **Failure reporting needs connected-component vocabulary.** When P1 fails, `W3` and `W6` retain the same configured identity but are physically partitioned. Tooling must report which members remain mutually reachable without implying route repair or role reassignment.
4. **Composition and forwarding coexist in the same gateway.** P5 may forward platform or selected canonical hydraulic traffic while terminating a high-level request and creating new valve/pump traffic. Counters and topology views must not merge those paths.
5. **Protection is not a second normal controller.** Multi-Origin makes it easy to encode P3 and P5 as Origins; the policy model must still make their narrow operation sets more prominent than their bare ability to originate.
6. **Wire-wide broadcast is narrower than Wire membership.** `W3` is a natural multi-Origin relationship, yet its Safety and BMS interactions require addressed fanout because the brief does not make every member a required consumer. Multi-Origin, operation-specific authority, and deliberately limited broadcast work together here.

## 11.2 Open questions left unresolved by the machine brief

- Which “interested thermal consumers,” beyond P1 and the drive controllers, must consume BMS thermal limits?
- Which selected hydraulic-leaf application publications, if any, must pass unchanged to P1 or P3 rather than being represented by P5-composed subsystem state?
- Which machine-visible LIN signals are required by P1 or P3, and which remain local to P5?
- Which safety operations are broadcasts versus addressed operations for a subset of P1/P4/P5 or P7/P8?
- What exact acknowledgement, retry, freshness, and integrity requirements apply to each safety and control service?
- Which logs/events must remain locally queryable after P1 is unavailable, and what bounded persistence each ECU provides?
- Does update policy permit P1 to update several targets concurrently, or exactly one at a time?
- Which operation-specific conditions authorize limited emergency retreat while the P1 forwarding path remains healthy?

These questions affect Endpoint schemas, sink sets, and deployment policy. They do not require a different Participant or Wire decomposition at Phase 1.

---

# 12. Checklist result

- Frozen topology preserved: **yes**
- One Endpoint Domain per named ECU: **yes**
- Deployment-global ParticipantIds assigned: **yes**
- Every physical segment and Wire shown: **yes**
- Actual Origin and required sinks shown for representative interactions: **yes**
- Full auditable permitted-Origin/service policy shown: **yes**
- Authored intent separated from generated projection: **yes**
- Forwarding separated from composition/adaptation: **yes**
- Broadcast and optional observation accounted for: **yes**
- Multiple-source selection shown at each relevant receiving function: **yes**
- Configurations A, B, C and frozen failures mapped without role promotion: **yes**
- Representative platform services mapped: **yes**
- Nine friction signals rated: **yes**
- Bit layouts, CAN-ID allocation, commissioning, and exhaustive catalogs avoided: **yes**

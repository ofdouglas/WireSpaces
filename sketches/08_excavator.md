# Sketch 08 — Battery-Electric Compact Excavator

**Basis:** Fresh Phase-1 mapping from the frozen excavator brief, using deployment-global `ParticipantId` and per-interaction Origin. Encoding, CAN identifier layout, commissioning, and comparison with the former single-Origin model are out of scope.

**Mapping:** Five production-control Wires preserve the machine's natural authority and failure scopes; one supporting machine-platform Wire provides inventory, health, time, logs, and firmware-update reach across the WS-capable topology.

**Worst friction:** **Configuration burden — Mild.** The authored policy is compact and auditable; the larger Origin/Endpoint/target and gateway projection is Organizer-generated rather than manually maintained.

**Main lesson:** Multi-Origin fits overlapping VCU, Safety, BMS, Brake, Hydraulic Gateway, and smart-leaf authority without inventing controller-specific Wires, but it does not remove source-selection policy or the VCU's physical role as the only Chassis↔Powertrain WS path.

**Configurations:** A — normal operation; B — VCU unavailable; C — hydraulic-side fault.

---

# 1. Native communication summary

This section intentionally avoids WireSpaces terminology.

The excavator has a hierarchical physical network but overlapping production authority:

- The operator HMI sends joystick, mode, enable, and auxiliary-function intent only to the VCU. The VCU validates and converts that intent into machine-level traction, brake, and hydraulic requests.
- The VCU coordinates normal motion. It sends torque and regenerative-braking requests to two drive controllers, braking requests to the Brake ECU, and high-level boom/arm/bucket/auxiliary requests to the Hydraulic Zone Gateway.
- The Safety ECU independently monitors the machine and may demand torque inhibit, hydraulic inhibit, or controlled stop. It may request a narrowly bounded emergency-retreat operation while the VCU communication path is healthy. It cannot originate ordinary excavation trajectories.
- The BMS publishes power, regenerative-energy, isolation, and thermal limits. The VCU and drive controllers must respect the most restrictive valid limits.
- The Brake ECU reports brake capability and faults and applies local brake protections.
- The Hydraulic Zone Gateway accepts high-level hydraulic requests, then coordinates two valve controllers and one pump controller. It polls non-WS hydraulic sensors over RS-485 and controls simple cabin I/O over LIN.
- The Hydraulic Gateway may independently neutralize or inhibit its subsystem when local pressure, temperature, sensing, or actuator conditions demand protection. This authority remains protective, not productive.
- Smart hydraulic leaves report local faults, availability, and current/thermal constraints to the Gateway.
- The Safety ECU supervises a dedicated Safety I/O controller over a separate CAN-FD segment. That controller also drives an independent hardwired inhibit to the drive inverters.

Command precedence is part of the application:

```text
hardwired drive inhibit
    dominates networked torque permission

valid Safety inhibit / controlled stop
    dominates contradictory normal VCU motion intent

Hydraulic Gateway local protection
    dominates accepted VCU hydraulic motion requests

BMS limits
    constrain VCU and drive-controller requests

actuator-local protection
    remains independently enforceable
```

No receiver uses packet arrival order or "last packet wins" to select authority.

The VCU is also the normal firmware-update orchestrator and machine time source. No other controller assumes either role when the VCU is absent.

---

# 2. Participants

The assumed concrete leaf count is two drive controllers, two valve controllers, and one pump controller. Every named ECU has exactly one Endpoint Domain.

| ParticipantId | Device / Endpoint Domain | Production responsibility |
|---:|---|---|
| **1** | Vehicle Control ECU (`VCU`) | Normal machine coordination, HMI-intent termination, motion composition, platform orchestration, time source |
| **2** | Operator HMI (`HMI`) | Operator intent and production presentation |
| **3** | Safety ECU (`Safety`) | Independent monitoring, bounded override/stop/retreat authority, Safety I/O supervision |
| **4** | Brake ECU (`Brake`) | Braking execution, availability, and local protection |
| **5** | Hydraulic Zone Gateway (`HydGateway`) | Hydraulic request termination/composition, local protection, gatewaying, RS-485/LIN adaptation |
| **6** | Battery Management System (`BMS`) | Power, regen, isolation, and thermal constraints |
| **7** | Left drive controller (`DriveL`) | Left traction/regen execution and drive-local protection |
| **8** | Right drive controller (`DriveR`) | Right traction/regen execution and drive-local protection |
| **9** | Valve controller A (`ValveA`) | Local hydraulic actuator execution and constraints |
| **10** | Valve controller B (`ValveB`) | Local hydraulic actuator execution and constraints |
| **11** | Pump controller (`Pump`) | Pump execution, availability, and constraints |
| **12** | Safety I/O controller (`SafetyIO`) | Dedicated E-stop/steering safety I/O and hardwired drive inhibit |

RS-485 sensors and LIN nodes are not WS Participants. Their machine-level identity and semantics terminate at services owned by Participant 5.

---

# 3. Physical Link Interfaces

| Physical segment | Participant Link Interfaces | WS use |
|---|---|---|
| **Operator Ethernet** | HMI P2; VCU P1 | Operator interaction and machine-platform traffic |
| **Chassis CAN-FD** | VCU P1; Safety P3; Brake P4; HydGateway P5 | Chassis control, the chassis side of Powertrain Coordination, and machine-platform traffic |
| **Powertrain CAN-FD** | VCU P1; BMS P6; DriveL P7; DriveR P8 | Powertrain Coordination and machine-platform traffic |
| **Hydraulic CAN-FD** | HydGateway P5; ValveA P9; ValveB P10; Pump P11 | Hydraulic Local Control and machine-platform traffic |
| **Hydraulic sensor RS-485** | HydGateway plus non-WS sensors | Native polling/adaptation only; no WS Wire reaches the sensors |
| **Cabin/simple-I/O LIN** | HydGateway plus non-WS I/O | Native adaptation only; no WS Wire reaches the LIN nodes |
| **Safety I/O CAN-FD** | Safety P3; SafetyIO P12 | Safety Local Control and machine-platform traffic |

The WS-capable topology is an acyclic tree rooted physically at the VCU:

```text
HMI -- Operator Ethernet -- VCU
                              |
                        Chassis CAN-FD
                  Safety --+-- Brake -- HydGateway
                    |                       |
              Safety I/O CAN-FD       Hydraulic CAN-FD
                    |                  /      |       \
                SafetyIO          ValveA   ValveB    Pump

VCU -- Powertrain CAN-FD -- BMS -- DriveL -- DriveR
```

The drawing shows attachment, not semantic relay. Devices on a bus are peers on that physical segment.

---

# 4. Wires

## 4.1 Wire summary

| Wire | Members | Physical realization | System role | Origins needed | Why it remains distinct |
|---|---|---|---|---:|---|
| **Operator Interaction** | P1 VCU, P2 HMI | Operator Ethernet | **primary** | **2** | Operator-intent and presentation scope ends at VCU |
| **Chassis Control** | P1 VCU, P3 Safety, P4 Brake, P5 HydGateway | Chassis CAN-FD | **primary** | **4** | Shared cross-domain command, constraint, state, and stop scope |
| **Powertrain Coordination** | P1 VCU, P3 Safety, P6 BMS, P7 DriveL, P8 DriveR | Chassis CAN-FD + Powertrain CAN-FD through VCU | **primary** | **5** | Torque, regen, power-limit, drive-state, and Safety reach; partitions if VCU fails |
| **Hydraulic Local Control** | P5 HydGateway, P9 ValveA, P10 ValveB, P11 Pump | Hydraulic CAN-FD | **primary** | **4** | Local actuator command/protection and smart-leaf constraint scope |
| **Safety Local Control** | P3 Safety, P12 SafetyIO | Safety I/O CAN-FD | **primary** | **2** | Independent safety I/O and hardwired-inhibit supervision |
| **Machine Platform** | P1–P12 | All five WS-capable physical segments through VCU, Safety, and HydGateway | **supporting** | **12** | Common inventory, health, time, logs/events, link telemetry, and update reach |

```text
Wires after mapping:                 6
Wires requiring more than 1 Origin: 6
Multi-Origin roles:                 primary=5, supporting=1, peripheral=0
```

## 4.2 Why this is not one Whole-Machine Wire

One broad Wire would preserve physical reach but would erase useful boundaries:

- operator-intent broadcasts must not reach ECUs beyond the VCU;
- local hydraulic protection must not imply command authority over Brake or drive controllers;
- Safety I/O traffic has an independent physical and failure scope;
- Powertrain Coordination partitions specifically at the VCU in Config B;
- Hydraulic Local Control survives VCU absence and is locally enforceable;
- broad machine broadcasts would require recipient filters to reconstruct scopes already present in the machine.

The six-Wire mapping therefore uses topology for route and broadcast scope, then Endpoint policy for the remaining authority distinctions inside each scope.

## 4.3 Why Machine Platform is broad

Machine Platform is the one intentionally broad supporting Wire. Its route scope is genuinely machine-wide:

- the VCU must reach every WS-capable update target;
- the VCU supplies machine time;
- each substantial ECU publishes health, inventory, update status, runtime/link telemetry, events, and fault history;
- downstream update and platform interactions are canonically forwarded through the appropriate embedded gateway.

Its broadcasts are correspondingly narrow at the Endpoint level: machine-time and explicitly defined platform announcements may be machine-wide; firmware payloads, logs, health, and update status are addressed, not broadcast.

---

# 5. Representative interactions

`O→P` means `OriginToParticipant`; `P→O` means `ParticipantToOrigin`. A response uses `P→O` only when it belongs to the initiating interaction and a request-scoped binding authorizes it. Autonomous status is a new interaction originated by its publisher.

| Interaction | Actual Origin | Required sink(s) | Wire / Direction | Optional observers / broadcast scope | Transport and path classification |
|---|---|---|---|---|---|
| Operator joystick/mode intent | HMI P2 | VCU P1 | Operator Interaction / O→P | none | Freshness-protected datagram; **terminates at VCU** |
| Machine state and production alert | VCU P1 | HMI P2 | Operator Interaction / O→P | none | Snapshot/event; not a relay for control |
| Normal torque/regen request | VCU P1 | DriveL P7, DriveR P8 | Powertrain Coordination / O→P | BMS may observe requested power where configured | Freshness-protected command; local Powertrain segment |
| Drive request acceptance/result | VCU P1 | initiating relation: drives P7/P8 reply | Powertrain Coordination / P→O | none | Request-scoped reply |
| Autonomous drive state/fault | DriveL P7 or DriveR P8 | VCU P1 | Powertrain Coordination / O→P | BMS and Safety may observe selected state | Snapshot/event; drive is actual Origin |
| Power/regen/thermal limit | BMS P6 | VCU P1 and drives P7/P8 | Powertrain Coordination / O→P | Safety may observe isolation state | Bounded broadcast to all Wire members or addressed fan-out; most restrictive valid limit wins |
| Safety torque inhibit / controlled stop | Safety P3 | drives P7/P8 | Powertrain Coordination / O→P | VCU and BMS receive/observe safety state | **Ordinary forwarding** Chassis→Powertrain through VCU; dominates normal request |
| Limited emergency retreat | Safety P3 | drives P7/P8 | Powertrain Coordination / O→P | VCU is required policy participant while healthy | Narrow operation; forwarded through VCU; unavailable when VCU is absent |
| Brake request | VCU P1 | Brake P4 | Chassis Control / O→P | Safety may observe | Freshness-protected command |
| Safety brake stop/inhibit | Safety P3 | Brake P4 | Chassis Control / O→P | VCU receives safety state | Narrow override; no arbitrary normal braking job |
| Brake capability/fault | Brake P4 | VCU P1 and Safety P3 | Chassis Control / O→P | HydGateway may observe machine-stop relevance | Snapshot/event |
| High-level hydraulic function request | VCU P1 | HydGateway P5 | Chassis Control / O→P | Safety may observe mode/limit fields | **Local termination at HydGateway**, not forwarding |
| Hydraulic inhibit / controlled stop | Safety P3 | HydGateway P5 | Chassis Control / O→P | VCU receives safety state | Safety override |
| Hydraulic subsystem state | HydGateway P5 | VCU P1 | Chassis Control / O→P | Safety observes availability/protection state | Snapshot/event |
| Local valve/pump command | HydGateway P5 | ValveA P9, ValveB P10, Pump P11 as required | Hydraulic Local Control / O→P | non-target leaves are not consumers | **Composition** from accepted high-level request |
| Gateway-local protective neutralize/inhibit | HydGateway P5 | affected hydraulic leaves | Hydraulic Local Control / O→P | other leaves receive only a scoped broadcast if their coordinated safe state requires it | New locally originated protective interaction |
| Smart-leaf fault/constraint | ValveA P9, ValveB P10, or Pump P11 | HydGateway P5 | Hydraulic Local Control / O→P | no automatic peer consumption | Snapshot/event; may trigger Gateway protection |
| Adapted hydraulic sensor state | HydGateway P5 | VCU P1; Safety P3 where safety-relevant | Chassis Control / O→P | Brake not a consumer by default | Gateway-owned Service after native RS-485 polling; **adaptation**, not WS forwarding |
| Adapted cabin/simple-I/O state | HydGateway P5 | VCU P1 | Chassis Control / O→P | Safety only for explicitly safety-relevant state | Gateway-owned Service after LIN adaptation |
| Safety-I/O command | Safety P3 | SafetyIO P12 | Safety Local Control / O→P | none | Explicit safety interaction |
| E-stop/steering safety state | SafetyIO P12 | Safety P3 | Safety Local Control / O→P | none | Independent status/event; hardwired inhibit is outside WS |
| Machine time synchronization | VCU P1 | all P2–P12 | Machine Platform / O→P | broadcast scope is all Machine Platform members | Datagram with quality metadata; no reassignment if VCU fails |
| Firmware-update control/segment | VCU P1 | one selected WS ECU | Machine Platform / O→P | HMI may display state but is not update Origin | Reliable segmented transfer; forwarded canonically to downstream WS targets |
| Firmware-update result | VCU P1 | selected target replies in VCU-anchored interaction | Machine Platform / P→O | HMI receives separate presentation state | Request-scoped reply |
| HMI update request/presentation | HMI P2 | VCU P1 | Operator Interaction / O→P | none | HMI requests; VCU independently originates update authority |
| Representative ECU health | each non-VCU ECU P2–P12 | VCU P1 normally; HydGateway P5 also supervises P9–P11 and Safety P3 supervises P12 | Machine Platform / O→P | HMI sees a VCU-composed presentation, not raw mandatory fan-out | Snapshot; local supervising sinks remain useful if VCU is absent |
| VCU health/platform state | VCU P1 | HMI P2 | Machine Platform / O→P | none | Snapshot; HMI presents availability but does not become supervisor |
| Link telemetry | each non-VCU ECU P2–P12 | VCU P1 | Machine Platform / O→P | Safety may observe declared safety-link fields; VCU keeps its own telemetry locally | Snapshot, one service per Endpoint Domain |
| Structured fault/event | any non-VCU ECU P2–P12 | VCU P1 | Machine Platform / O→P | VCU's own faults remain local and are presented selectively to HMI | Event Queue; persistent history remains local |
| Text log | any non-VCU ECU P2–P12 | VCU P1 | Machine Platform / O→P | VCU keeps its own logs locally and sends selected presentation to HMI | Bounded Queue/background QoS |

---

# 6. Permitted-Origin and Endpoint authorization policy

## 6.1 Authored intent

The following table is the authority the machine designer must state and audit. "May originate on Wire" never means "may invoke every Endpoint on Wire."

| Wire | Participant allowed to originate | Authorized Endpoint/service operations and targets | Explicit prohibitions |
|---|---|---|---|
| Operator Interaction | **HMI P2** | OperatorIntent, update request, and presentation queries → VCU P1 | No direct drive, brake, hydraulic-leaf, BMS, or Safety command |
| Operator Interaction | **VCU P1** | MachineState, ModeState, Availability, ProductionAlert, update presentation → HMI P2 | HMI is not a machine-control relay |
| Chassis Control | **VCU P1** | BrakeCoordination → P4; HydraulicFunctionRequest → P5; machine mode/coordination → P3/P4/P5 as declared | No Safety-only command; no direct local valve/pump command |
| Chassis Control | **Safety P3** | SafetyInhibit, ControlledStop, safety state → P1/P4/P5 as applicable | No arbitrary trajectory, normal excavation job, or unconstrained productive motion |
| Chassis Control | **Brake P4** | BrakeCapability, BrakeFault, degraded constraint → P1/P3 | No commands to hydraulic or powertrain participants |
| Chassis Control | **HydGateway P5** | HydraulicState, protective event, adapted sensor/I/O state → P1/P3 as declared | No unrelated brake/traction command; no general VCU substitution |
| Powertrain Coordination | **VCU P1** | TorqueRequest, RegenRequest, machine mode → P7/P8 | Subject to BMS, Safety, and local limits |
| Powertrain Coordination | **Safety P3** | TorqueInhibit, ControlledStop, bounded EmergencyRetreat → P7/P8 | Emergency retreat is a distinct bounded operation; no arbitrary normal torque |
| Powertrain Coordination | **BMS P6** | PowerLimit, RegenLimit, ThermalDerate, IsolationRequest → P1/P7/P8 | No trajectory or wheel-speed selection |
| Powertrain Coordination | **DriveL P7, DriveR P8** | DriveState, request result, local fault/limit → P1; selected state observable by P3/P6 | No command to peer drive or unrelated subsystem |
| Hydraulic Local Control | **HydGateway P5** | ValveCommand/PumpCommand and LocalProtectiveNeutralize/Inhibit → P9/P10/P11 | Productive commands require accepted high-level authority; absence of VCU grants no new productive authority |
| Hydraulic Local Control | **ValveA P9, ValveB P10, Pump P11** | Local state, availability, thermal/current/fault constraint → P5 | No arbitrary peer-leaf command and no direct machine-level command |
| Safety Local Control | **Safety P3** | SafetyIO command/test under valid machine safety state → P12 | No unrelated general I/O use |
| Safety Local Control | **SafetyIO P12** | E-stop/steering state, output result, local fault → P3 | No normal traction or hydraulic command |
| Machine Platform | **VCU P1** | FirmwareUpdate and TimeSync → all WS targets; own health/platform presentation → HMI P2 | Update only in explicit maintenance/update state |
| Machine Platform | **HMI P2, Safety P3, Brake P4, HydGateway P5, BMS P6, drives P7/P8, hydraulic leaves P9–P11, SafetyIO P12** | Own identity/version, health, update status/reply, runtime/link telemetry, event/fault/log, time-quality report → VCU P1; hydraulic leaves may also report to supervising HydGateway P5 and SafetyIO to supervising Safety P3 | No firmware-update orchestration and no time-source takeover |

## 6.2 Organizer-generated projection

The Organizer expands the authored intent into bounded local artifacts:

- per-Wire membership and permitted-Origin sets;
- per-Origin Endpoint and target allowlists;
- transmit bindings constrained to the listed Wire, target set, operation, and Transport;
- request-scoped reply bindings that cannot be retargeted;
- configured observer sets;
- forwarding entries at VCU, HydGateway, and Safety;
- Link-profile ParticipantId projections;
- compatibility fingerprints over every projection.

The generated projection is materially larger than the 16 authored rows above. In particular, the Machine Platform row expands across 12 Participants and several service families, and Safety operations expand into distinct target/operation combinations. This is acceptable only if:

1. the high-level declarations remain the sole authored source;
2. generated rows retain provenance to the declaration that produced them;
3. tooling can answer "why may P3 invoke this Endpoint on P8?" without inspecting firmware;
4. coupled checks reject a valid Origin with an unauthorized Endpoint, target, operation, state, or Transport.

The designer does **not** hand-maintain a flattened all-to-all permission matrix.

---

# 7. Sink/source-selection policy

Several receiving functions legitimately see multiple Origins. Selection is explicit application policy, not Direction, arrival order, QoS, or last-writer behavior.

| Receiving function | Legitimate sources | Deterministic selection |
|---|---|---|
| Drive torque/regen actuator | VCU normal request; Safety inhibit/stop/limited retreat; BMS limits; hardwired SafetyIO inhibit; drive-local protection | Hardwired inhibit and valid Safety stop dominate productive torque; BMS limits bound any remaining request; limited retreat is accepted only under its explicit state/operation; local protection remains enforceable |
| Brake actuator | VCU normal braking request; Safety stop/inhibit; Brake-local protection | Valid Safety stop dominates contradictory normal request; local protection may further constrain |
| HydGateway high-level function | VCU productive request; Safety hydraulic inhibit/stop; Gateway-local protection state | Safety inhibit and active local protection dominate; absence/invalidity of VCU request never authorizes spontaneous productive motion |
| Valve/pump leaves | HydGateway normal local command; HydGateway protective neutralize/inhibit; leaf-local protection | Protective operation dominates normal command; leaf-local limit may reject or constrain both |
| VCU machine availability | Safety state; BMS limit/isolation; Brake capability; HydGateway availability; drive states | VCU composes capability from typed constraints; no generic source-priority number replaces service semantics |
| HMI presentation | VCU machine state/alerts | HMI displays VCU-composed state; it does not merge raw controller commands into authority |

Every command type carries enough freshness/validity state for the receiving Service to reject stale authority. A higher-precedence stale command does not remain valid merely because its source class is stronger.

---

# 8. Broadcast and observation accounting

## 8.1 Representative broadcasts

| Broadcast | Origin | Wire scope / recipients | Endpoint restriction |
|---|---|---|---|
| Machine mode transition | VCU P1 | Chassis Control: P3 Safety, P4 Brake, P5 HydGateway | Only the MachineMode Endpoint; does not reach HMI, drives, or hydraulic leaves as raw traffic |
| Chassis controlled-stop state | Safety P3 | Chassis Control: P1 VCU, P4 Brake, P5 HydGateway | Safety operation only; target-specific actuation may still use addressed commands |
| Power/regen limit | BMS P6 | Powertrain Coordination: P1 VCU, P3 Safety, P7/P8 drives | BMS limit Endpoint; membership is semantically interested |
| Hydraulic coordinated neutralize | HydGateway P5 | Hydraulic Local Control: P9/P10/P11 | Protective Endpoint only; narrower addressed neutralization is preferred when one function is affected |
| Machine time | VCU P1 | Machine Platform: all P2–P12 | TimeSync only; loss produces degraded quality, not source election |

Firmware images, text logs, health snapshots, and update results are not machine-wide broadcasts.

## 8.2 Configured observations

| Produced interaction | Required sink | Optional configured observers | Meaning |
|---|---|---|---|
| Drive state | VCU | BMS for energy/thermal coordination; Safety for declared safety state | Observation does not grant drive command authority |
| VCU brake request | Brake | Safety | Awareness/audit only; Safety retains its own command Endpoint |
| VCU hydraulic request | HydGateway | Safety | Awareness; Safety may separately originate inhibit |
| Brake capability | VCU and Safety | HydGateway only if machine-stop behavior needs it | Observer is not required delivery coverage |
| Hydraulic subsystem state | VCU | Safety | Safety observation does not turn HydGateway into a Safety relay |
| Platform health/logs | VCU | HMI receives selected VCU-composed presentation | Raw observation is not required for HMI production operation |

An observer never counts as redundant delivery or as authority. Shared-bus electrical visibility does not create observation configuration.

---

# 9. Embedded gateway behavior

## 9.1 VCU forwarding and composition

| Ingress | Wire | Egress | Local delivery | Classification |
|---|---|---|---|---|
| Chassis CAN-FD | Powertrain Coordination | Powertrain CAN-FD | where VCU is required sink/observer | **Ordinary forwarding** of selected Safety interactions |
| Powertrain CAN-FD | Powertrain Coordination | Chassis CAN-FD | where Safety is configured observer/sink | **Ordinary forwarding** of selected BMS/drive state |
| Operator Ethernet / Chassis / Powertrain | Machine Platform | other configured VCU interfaces | as addressed | **Ordinary forwarding**; branch pruning follows target membership |
| Operator Ethernet | Operator Interaction | none | VCU | **Local termination** of HMI intent |
| VCU local Service | Chassis Control | Chassis CAN-FD | local producer | New VCU-originated machine interaction |
| VCU local Service | Powertrain Coordination | Powertrain CAN-FD | local producer | New VCU-originated machine interaction |

Explicit composition at VCU:

```text
HMI operator intent
    -> terminate and validate at VCU
    -> VCU machine-control state
    -> new VCU-originated Brake, Powertrain, and Hydraulic interactions
```

Raw joystick or mode PDUs are never forwarded to leaves. Firmware-update presentation requested by HMI similarly terminates at VCU; VCU originates the canonical update operation only after maintenance-state policy accepts it.

## 9.2 Hydraulic Zone Gateway forwarding, termination, and composition

| Ingress | Wire | Egress | Local delivery | Classification |
|---|---|---|---|---|
| Chassis CAN-FD | Machine Platform | Hydraulic CAN-FD | as addressed | **Ordinary forwarding** to hydraulic WS targets |
| Hydraulic CAN-FD | Machine Platform | Chassis CAN-FD | as addressed | **Ordinary forwarding** of target replies/platform state |
| Chassis CAN-FD | Chassis Control | none | HydGateway high-level Service | **Local termination** |
| HydGateway local Service | Hydraulic Local Control | Hydraulic CAN-FD | local producer | **Composition/re-origination** |
| RS-485 / LIN | native protocols | no canonical forwarding | HydGateway adapter Services | **Adaptation/application boundary** |

Explicit composition:

```text
accepted VCU HydraulicFunctionRequest
    + Safety state
    + adapted sensor state
    + leaf availability
    -> hydraulic coordination
    -> new HydGateway-originated valve/pump commands
```

A local pressure, temperature, or actuator fault may independently originate HydGateway protective traffic without any upstream request.

## 9.3 Safety ECU forwarding and application traffic

| Ingress | Wire | Egress | Local delivery | Classification |
|---|---|---|---|---|
| Chassis CAN-FD | Machine Platform | Safety I/O CAN-FD | as addressed | **Ordinary forwarding** to SafetyIO platform Endpoints |
| Safety I/O CAN-FD | Machine Platform | Chassis CAN-FD | as addressed | **Ordinary forwarding** of platform replies/state |
| Safety local Service | Safety Local Control | Safety I/O CAN-FD | local producer | Explicit Safety application interaction |

Safety commands to SafetyIO are not disguised as forwarding. Safety creates them under its own Endpoint authority.

## 9.4 Forwarding invariants

- Ordinary forwarding preserves Wire, Origin, Participant, Direction, Endpoint, Transport, QoS, extensions, and payload semantics.
- Participant carrier-local representation may change at each Link boundary without changing canonical identity.
- No gateway infers a route or authority from observed traffic.
- The Machine Platform realization is acyclic: VCU branches to Ethernet, Chassis, and Powertrain; Safety branches Chassis to Safety I/O; HydGateway branches Chassis to Hydraulic.
- VCU failure removes Chassis↔Powertrain and Operator↔machine forwarding. HydGateway and Safety can still forward between their surviving local branches where their own power and links remain healthy.

---

# 10. Configuration A — Normal operation

All seven physical links and major controllers are available.

## 10.1 Available behavior

- HMI intent terminates at VCU and is composed into normal traction, brake, and hydraulic requests.
- Safety, BMS, Brake, HydGateway, drives, and smart hydraulic leaves concurrently originate their bounded interactions.
- Safety traffic reaches drives over Powertrain Coordination through ordinary VCU forwarding.
- Hydraulic high-level requests terminate at HydGateway and are composed into local control.
- HydGateway local protection and Safety overrides can coexist with contradictory normal requests; receiver source-selection resolves them deterministically.
- Machine Platform reaches every WS Participant for update, time, health, telemetry, logs, and events.

## 10.2 Audit statement

```text
Normal productive authority:
    VCU, after accepted HMI intent and machine policy

Independent constraints/overrides:
    Safety, BMS, Brake-local, HydGateway-local, leaf-local

No generic controller role:
    each Origin is authorized per Wire + Endpoint + target + operation
```

---

# 11. Configuration B — VCU unavailable

The VCU is fully unavailable, including all of its forwarding and composition.

## 11.1 Wire realization after failure

| Wire | Surviving realization | What continues | What is unavailable |
|---|---|---|---|
| Operator Interaction | HMI remains alone on Operator Ethernet | HMI local UI only | Operator-intent termination and machine presentation |
| Chassis Control | Safety, Brake, HydGateway remain peers on Chassis CAN-FD | Safety↔Brake, Safety↔HydGateway, Brake/HydGateway state as authorized | VCU normal coordination |
| Powertrain Coordination | Partitioned: Safety on Chassis side; BMS and drives on Powertrain side | BMS↔drive limits/state and drive-local protection | Safety WS reach to drives, VCU torque, Chassis↔Powertrain forwarding, emergency-retreat torque |
| Hydraulic Local Control | HydGateway and hydraulic leaves remain connected | Local protective neutralize/inhibit and local status | New productive jobs requiring VCU high-level request |
| Safety Local Control | Safety and SafetyIO remain connected | Safety I/O supervision and hardwired drive inhibit | Nothing intrinsic to this Wire |
| Machine Platform | Partitioned at missing VCU; surviving HydGateway and Safety branches remain locally forwardable | Local segment health/status and downstream platform exchanges with surviving local sinks | Machine-wide update orchestration, time source, cross-VCU platform reach |

## 11.2 Crisp degraded-state reporting

Operators and diagnostic state can report:

```text
VCU / Participant 1 unavailable
normal machine coordination unavailable
Operator Interaction has no VCU sink
Powertrain Coordination partitioned at VCU
Safety network path to drive controllers unavailable
hardwired drive inhibit remains available
limited emergency-retreat torque operation unavailable
machine time source unavailable; timestamp quality degraded
firmware-update orchestrator unavailable
Hydraulic local protection available; productive hydraulic jobs unavailable
```

No Participant is promoted. Safety does not become VCU; HydGateway does not become machine coordinator; BMS does not select trajectory; HMI does not bypass VCU.

---

# 12. Configuration C — Hydraulic-side fault

A sensor or actuator condition requires HydGateway-local protection while the VCU remains online.

## 12.1 Interaction sequence

```text
RS-485 sensor state or smart-leaf fault
    -> HydGateway local protective decision
    -> HydGateway originates addressed or scoped neutralize/inhibit
       on Hydraulic Local Control
    -> affected valve/pump leaves enforce protective state
    -> HydGateway originates HydraulicDegraded state/event
       on Chassis Control to VCU and Safety
    -> VCU stops issuing the affected high-level function and
       may continue unrelated traction/brake/hydraulic functions where allowed
```

The protective PDU is not a forwarded sensor frame and not a reply to the last VCU command. It is a new HydGateway-originated interaction under narrow authority.

## 12.2 Available and unavailable behavior

| Scope | Continues | Degraded/lost |
|---|---|---|
| Operator / VCU | Operator intent, state display, unrelated coordination | Affected hydraulic function reported unavailable |
| Chassis | Safety, Brake, VCU, HydGateway interactions | No general Chassis failure |
| Powertrain | Normal operation subject to machine policy | None caused solely by hydraulic fault |
| Hydraulic Local | Unaffected leaves where safe; all protective traffic | Affected actuator/function |
| Platform | Health, fault history, link telemetry, time, update reach | Fault/event indicates hydraulic degradation |

There is no role reassignment and no Wire reconfiguration.

---

# 13. Additional frozen failure/degraded-state mapping

| Failure / absence | Wire-level effect | Traffic/behavior that continues | Authority lost / degraded | Reassignment |
|---|---|---|---|---|
| **HMI unavailable** | Operator Interaction lacks P2 | All other Wires and local protections remain connected | New operator intent and display/alerts | None; VCU follows native no-valid-intent policy |
| **Safety unavailable** | P3 absent from Chassis, Powertrain, Safety Local, Platform; SafetyIO branch loses its gateway | Non-safety state may still flow; local Brake/BMS/drive/HydGateway protections remain | Independent Safety commands, SafetyIO supervision, hardwired safety-inhibit control | None; normal operation may be prohibited/degraded |
| **HydGateway unavailable** | P5 absent; Hydraulic Local and downstream Platform branch lose upstream gateway | Chassis Safety/Brake and Powertrain interactions not requiring P5 continue | Hydraulic composition, local hydraulic protection, hydraulic-leaf platform reach, RS-485/LIN adaptation | None |
| **One hydraulic CAN leaf fails** | One P9/P10/P11 missing from Hydraulic Local and Platform | Other leaves and Gateway remain connected, subject to function safety | Affected actuator function | None |
| **Hydraulic RS-485 fails** | No WS Wire topology change; HydGateway adapter loses native source | Other machine Links; Gateway/leaf communication | Sensor-derived hydraulic availability; Gateway may protectively limit subsystem | None |
| **Powertrain CAN-FD fails** | Powertrain Coordination and Machine Platform powertrain branch unavailable | Chassis, Hydraulic, Safety Local, Operator; independent hardwired inhibit | Normal traction/regen and networked BMS/drive reach | None |

---

# 14. Friction ratings

| Signal | Rating | Reason |
|---|---|---|
| **Artificial Origin** | **None** | Every Origin corresponds to a real initiator: HMI, VCU, Safety, BMS, Brake, gateways, or smart leaves. |
| **Artificial Wire** | **Mild** | Machine Platform is broad and exists mainly to share cross-machine obligations, but its update/time/health route scope is real. |
| **Wire proliferation** | **Mild** | Six Wires over seven physical segments are auditable; each production Wire preserves a distinct scope or failure meaning. |
| **Forwarding tax** | **Mild** | VCU, HydGateway, and Safety forwarding is real native gateway work; WS adds explicit tables but not artificial relays. |
| **Identity awkwardness** | **None** | One ParticipantId per ECU remains stable across every Wire and gateway hop. |
| **Interaction awkwardness** | **Mild** | Forwarding, termination, composition, and local protection must be distinguished carefully, especially at VCU and HydGateway. |
| **Configuration burden** | **Mild** | Six overlapping memberships, narrow Safety operations, target allowlists, observers, forwarding, and source selection produce a large safety-relevant projection, but the authored intent remains 16 compact rows and Organizer generates the flattened tables. |
| **Role instability** | **None** | Runtime Origin varies normally by interaction, but no election, failover, or role reassignment occurs in any frozen state. |
| **Failure mismatch** | **Mild** | Degraded state is capability- and partition-specific rather than a single Origin-offline flag; it is crisp only if tooling reports lost forwarding and authority explicitly. |

The Mild configuration rating still reflects real audit and tooling work. A Whole-Machine Wire would move the same complexity into a larger Endpoint policy and broadcast-filter matrix while weakening failure-scope visibility.

---

# 15. Model pressure

## 15.1 What the mapping asks from WireSpaces

1. **Auditable coupled authorization.** A per-Wire permitted-Origin set is insufficient. Safety P3 is a legitimate Origin on Powertrain Coordination but only for inhibit, stop, and bounded retreat operations toward declared targets.
2. **Typed source-selection policy.** Receivers need generated evidence that VCU, Safety, BMS, and local protection inputs are combined by the correct Service rule, never by generic packet priority.
3. **Capability-specific degraded state.** "Participant 1 unavailable" must expand to lost composition, lost forwarding branches, lost update/time authority, and unavailable retreat reach while showing surviving Chassis and local protection traffic.
4. **Forwarding provenance.** Tooling must show whether a PDU was forwarded unchanged, terminated, or caused a new composed interaction.
5. **Broad platform-Wire discipline.** Endpoint targeting and branch pruning must prevent logs and telemetry from becoming machine-wide broadcast load merely because update/time scope is broad.

## 15.2 Useful distinctions exposed by WS

- The VCU is simultaneously an Endpoint participant, application composer, platform authority, and physical gateway; those roles no longer collapse into "main controller."
- Safety's network authority and the independent hardwired inhibit are visibly different capabilities with different failure reach.
- HydGateway protection is explicitly new local authority, while high-level-request handling is composition and platform passage is forwarding.
- Participant identity survives every Wire and physical segment, while route/broadcast scopes remain narrower than the deployment.

---

# 16. Open questions

1. What deployment schema expresses operation-level authority such as "Safety may originate EmergencyRetreat but not ordinary TorqueRequest" without hiding policy in Service code?
2. Should source-selection rules be part of generated Endpoint contracts, a separate machine policy artifact, or both?
3. Can Machine Platform use target-aware static branch pruning while preserving the simple bounded Router model, or should platform obligations be split into subsystem Wires?
4. Which hydraulic conditions require a full Hydraulic Local broadcast versus addressed neutralization of one leaf?
5. When VCU is unavailable, which surviving Platform interactions have useful local sinks, and should unused publications be suppressed by static state policy or merely rejected at missing routes?
6. Where is the "VCU communication path healthy" precondition for limited emergency retreat enforced—Safety Origin eligibility, VCU forwarding admission, or drive-Endpoint state—while preserving Safety as the canonical Origin?
7. How should tooling present a partitioned multi-Origin Wire so that "BMS and drives still communicate, Safety cannot reach them" is obvious without implying a new Wire identity?
8. How are hardwired inhibit state and networked Safety state correlated in diagnostics without treating the hardwired path as a WS participant?

---

# 17. Completion summary

```text
Participants:                         12
WS-capable physical segments:          5
Non-WS adapted physical segments:      2
Wires:                                 6
Wires requiring >1 Origin:             6
Embedded gateways:                     3 (VCU, HydGateway, Safety)

Primary Wires:
    Operator Interaction
    Chassis Control
    Powertrain Coordination
    Hydraulic Local Control
    Safety Local Control

Supporting Wire:
    Machine Platform

New authored authorization state:
    16 compact policy rows plus explicit source-selection rules

Generated projection:
    materially larger; membership, Origin/Endpoint/target allowlists,
    bindings, observers, forwarding, carrier projection, fingerprints

Frozen role reassignment:
    none

Key failure result:
    VCU absence partitions Powertrain and Platform reach but leaves
    Chassis-side interactions, Safety Local, hardwired inhibit, BMS/drive
    local protection, and HydGateway local protection available.
```

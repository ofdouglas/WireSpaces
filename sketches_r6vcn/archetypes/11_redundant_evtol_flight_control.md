# Archetype 11 — Redundant eVTOL Flight-Control + Distributed Propulsion

**ID:** 11  
**Convergence test:** **Yes** — assign 2–3 independent mapping agents  
**Suggested Link mix:** Shared memory / IPC + redundant RS-485 + Automotive/Embedded Ethernet + redundant CAN-FD / 29-bit CAN profile + local serial sensor links + wireless telemetry Link  
**Stress:** Human-capable experimental eVTOL evolved from UAV architecture — triply redundant flight control, redundant actuator networks, repeated propulsion nodes, local sensor pairs, remote test/control authority, mixed criticality, high standard-Service density, and degraded operation under multiple independent failures

**Scale target:** approximately **26 / 30 / 38 production WS Participants** for 4 / 8 / 16 propulsion-unit variants, plus Ground Station and optional DebugPC. The assignment should remain auditable despite repeated hardware and redundant Links.

**Primary question:** Can WireSpaces cleanly model a triply redundant flight-control system with duplicated actuator networks, multicore flight computers, repeated propulsion/battery nodes, and a remote ground-control path **without introducing hidden master semantics, embedding authority into networking, or making redundancy itself a protocol feature?**

---

## Configurations

### Config A — Flight / production minimum

Production aircraft hardware with:

- three flight controllers;
- dedicated IMU + GNSS sensing for each flight controller;
- redundant forward actuator buses;
- redundant aft actuator buses;
- quadrant-distributed propulsion;
- one elevon/control-surface unit per quadrant;
- one battery/BMS unit per quadrant;
- compact avionics display/SoC;
- one onboard Telemetry Unit;
- Ground Station connected when mission/test operations require it.

The aircraft must not require the Ground Station, DebugPC, compact avionics display, or cloud/network infrastructure to sustain basic flight-control operation.

### Config B — Redundant telemetry option

Same as Config A, plus a second **Telemetry Unit** and independent ground radio/data path.

This variant is for exploring whether duplicated remote links fit naturally without affecting flight-control semantics.

Do not assume both telemetry units are required in the baseline mapping.

### Config C — Development / flight test

Same as Config A or B, plus optional **DebugPC / development workstation** connected to the onboard avionics Ethernet/service network.

The DebugPC may:

- inspect Identity / Version / Health;
- inspect fault and reset history;
- stream engineering telemetry;
- query Link status;
- update software/configuration where permitted;
- inject explicit test-only commands through designated test Services.

The DebugPC is not a production flight-control participant and must not become necessary for normal operation.

---

## Propulsion-count variants

The architecture is fixed while repeated propulsion hardware scales.

### Variant P4

```text
FL: 1 propulsion controller
FR: 1 propulsion controller
RL: 1 propulsion controller
RR: 1 propulsion controller

Total propulsion controllers: 4
```

### Variant P8

```text
FL: 2 propulsion controllers
FR: 2 propulsion controllers
RL: 2 propulsion controllers
RR: 2 propulsion controllers

Total propulsion controllers: 8
```

### Variant P16

```text
FL: 4 propulsion controllers
FR: 4 propulsion controllers
RL: 4 propulsion controllers
RR: 4 propulsion controllers

Total propulsion controllers: 16
```

Run the semantic mapping first against **P8** unless the experiment operator assigns another variant.

Then perform a scaling check against P4 and P16 without redesigning the logical architecture merely because the repeated-node count changes.

---

## Physical topology

```text
                                      Ground Station
                                    /                \
                          Primary Radio            Backup Radio
                              |                     (Config B)
                       TelemetryUnit_A            TelemetryUnit_B
                              \                       /
                               +---- AvionicsEth ----+
                              /        |              \
                     CompactAvionics  DebugPC       FC platform domains
                                      (Config C)       |   |   |

                        +-------------------------------------------+
                        |           Flight-control computers        |
                        |                                           |
                        |   FC-A         FC-B         FC-C           |
                        |  +-----+      +-----+      +-----+         |
                        |  |CtrlA|      |CtrlB|      |CtrlC|         |
                        |  +--+--+      +--+--+      +--+--+         |
                        |     | FC_A_SHM    | FC_B_SHM  | FC_C_SHM   |
                        |  +--+--+      +--+--+      +--+--+         |
                        |  |PlatA|======|PlatB|======|PlatC|         |
                        |  +--+--+      +--+--+      +--+--+         |
                        |      \________ RS-485 ________/            |
                        |            triangle / ring                 |
                        +-------------------------------------------+
                            |  |  |       |  |  |       |  |  |
                            |  |  +-------+  |  +-------+  |  |
                            |  +-------------+-------------+  |
                            +---------------------------------+
                                 actuator-network access

          Forward actuator group                         Aft actuator group
      +--------------------------+                  +--------------------------+
      |      FwdCAN_A / _B       |                  |      AftCAN_A / _B       |
      |                          |                  |                          |
      | FL propulsion unit(s)    |                  | RL propulsion unit(s)    |
      | FR propulsion unit(s)    |                  | RR propulsion unit(s)    |
      | BMS_FL / BMS_FR          |                  | BMS_RL / BMS_RR          |
      | Elevon_FL / Elevon_FR    |                  | Elevon_RL / Elevon_RR    |
      +--------------------------+                  +--------------------------+

Dedicated sensors:

    IMU_A  ---- local serial ---- FC-A
    GNSS_A ---- local serial ---- FC-A

    IMU_B  ---- local serial ---- FC-B
    GNSS_B ---- local serial ---- FC-B

    IMU_C  ---- local serial ---- FC-C
    GNSS_C ---- local serial ---- FC-C
```

The drawing is schematic. It is not a required Wire decomposition.

---

## Flight-controller multicore model

Each flight controller is physically a four-core safety MCU / SoC arranged as **two independent lockstep pairs**.

For WireSpaces modelling:

```text
4 physical cores
    ->
2 usable execution domains
    ->
2 WS Participants
```

Lockstep replicas are **not** separate Participants.

Each flight controller therefore contains:

| Participant | Primary responsibilities |
|---|---|
| `FC_A_Control` / `FC_B_Control` / `FC_C_Control` | State estimation, guidance/control-law execution, flight mode, actuator command generation, local control-state publication |
| `FC_A_Platform` / `FC_B_Platform` / `FC_C_Platform` | External Link ownership, communications, Health/fault/platform management, configuration, update support, logging/telemetry staging |

Each pair communicates over a device-local shared-memory / IPC Link:

```text
FC_A_Control  <->  FC_A_Platform   via FC_A_SHM
FC_B_Control  <->  FC_B_Platform   via FC_B_SHM
FC_C_Control  <->  FC_C_Platform   via FC_C_SHM
```

The mapper may choose which Services are terminated by Control vs Platform, but must preserve the functional/fault-containment distinction.

Do not collapse all six flight-controller Participants into three solely to simplify the network sketch.

---

## Other core/domain modelling rules

### Propulsion controllers

Each propulsion controller is assumed to use a dual-core lockstep MCU.

For modelling:

```text
2 physical lockstep cores
    ->
1 usable core/domain
    ->
1 WS Participant
```

Do not expose the lockstep replica as a second Participant.

### Elevon / control-surface units

Each elevon controller is similarly modelled as:

```text
2 physical lockstep cores
    ->
1 usable core/domain
    ->
1 WS Participant
```

### Battery/BMS controllers

Model each quadrant BMS as **one WS Participant**.

A real pack controller may contain additional safety-monitoring silicon, but internal BMS partitioning is outside the purpose of this assignment unless the experiment operator explicitly extends it.

### Sensors, telemetry, avionics

Model each IMU, GNSS receiver, Telemetry Unit, CompactAvionics unit, Ground Station, and DebugPC as **one Participant/domain** for this exercise.

Do not invent extra Endpoint Domains merely because the underlying processor may have additional cores.

---

## Device capabilities

### Flight-control Participants

| Participant class | Role |
|---|---|
| `FC_*_Control` | Local state estimate, guidance/control state, flight mode, actuator command intent, control-loop Health |
| `FC_*_Platform` | External network access, peer communication transport, Health/fault/platform Services, software/configuration Services, telemetry/log staging |

There is **no canonical primary flight controller**.

The three flight-control computers are peers. Application-level voting, mode selection, degraded operation, or command acceptance must not be encoded as Origin/Main/network authority semantics.

### IMUs

Three dedicated IMUs:

```text
IMU_A -> FC-A
IMU_B -> FC-B
IMU_C -> FC-C
```

Each provides:

- angular rate;
- acceleration;
- sensor status;
- temperature where available;
- timestamp/synchronization metadata where supported;
- Identity / Version / Health / fault status.

### GNSS receivers

Three dedicated GNSS receivers:

```text
GNSS_A -> FC-A
GNSS_B -> FC-B
GNSS_C -> FC-C
```

Each provides:

- position/velocity/time;
- solution status / quality;
- receiver Health;
- Identity / Version;
- fault/status information.

The assignment does not require raw RF/baseband data transport.

### Propulsion controllers

Each propulsion Participant provides/consumes:

- thrust / torque / speed command as appropriate;
- enable/inhibit state;
- measured speed/current/voltage;
- temperature;
- local fault state;
- actuator availability/capability;
- Identity / Version / Health;
- firmware/update capability where realistic;
- engineering telemetry.

Do not require every raw motor-control sample to become a WS message.

### Elevon controllers

Each quadrant has one elevon/control-surface Participant:

```text
Elevon_FL
Elevon_FR
Elevon_RL
Elevon_RR
```

Each provides/consumes:

- commanded surface position / rate;
- measured position;
- servo status;
- power/current state where useful;
- faults;
- Identity / Version / Health;
- update/service support where realistic.

### Battery/BMS controllers

Each quadrant has one BMS Participant:

```text
BMS_FL
BMS_FR
BMS_RL
BMS_RR
```

Each exposes:

- pack voltage/current;
- state of charge / state of health;
- temperature summary;
- contactor state;
- power-availability limits;
- local faults;
- isolation/safety status where available;
- Identity / Version / Health;
- software/update support where realistic.

### Telemetry Unit(s)

`TelemetryUnit_A`, plus optional `TelemetryUnit_B`, provide the onboard end of the ground data link.

Responsibilities include:

- ground-to-air WS transport/gateway function;
- telemetry downlink;
- remote diagnostic access;
- test-mode command ingress;
- mission / ODD configuration ingress;
- software/configuration transfer;
- Link status;
- local Health / Version.

The telemetry unit has **no inherent flight-control authority** merely because it forwards Ground Station traffic.

### Ground Station

The Ground Station is a genuine WS Participant for the trial.

It may:

- receive high-rate/selected telemetry;
- query Identity / Version / Health;
- retrieve faults/logs;
- issue test/mission control inputs;
- issue explicit operator overrides where the application mode permits;
- provide ODD/geofence/mission configuration;
- initiate software/update workflows;
- request engineering data.

Ground Station authority is **Service/application-state dependent**.

Do not represent “Ground Station may override during flight test” as a network-layer role.

### CompactAvionics

One lightweight integrated display + SoC.

Expected responsibilities:

- pilot display;
- warnings/annunciation;
- aircraft state summary;
- flight-mode display;
- propulsion/battery summary;
- basic operator inputs;
- Health / Version / fault display.

It is not part of the inner control loop.

Loss of CompactAvionics must not by itself prevent the flight controllers from continuing control.

### DebugPC

Optional development participant.

Capabilities:

- broad diagnostics;
- engineering telemetry;
- test/configuration access;
- software update;
- explicit test-only Service calls.

No production role is assumed.

---

## Existing physical links

| Link | Type | Attachments | Config |
|---|---|---|---|
| `FC_A_SHM` | Shared memory / IPC | `FC_A_Control`, `FC_A_Platform` | A, B, C |
| `FC_B_SHM` | Shared memory / IPC | `FC_B_Control`, `FC_B_Platform` | A, B, C |
| `FC_C_SHM` | Shared memory / IPC | `FC_C_Control`, `FC_C_Platform` | A, B, C |
| `FC_AB_RS485` | Point-to-point / multidrop-capable RS-485 | `FC_A_Platform`, `FC_B_Platform` | A, B, C |
| `FC_BC_RS485` | Point-to-point / multidrop-capable RS-485 | `FC_B_Platform`, `FC_C_Platform` | A, B, C |
| `FC_CA_RS485` | Point-to-point / multidrop-capable RS-485 | `FC_C_Platform`, `FC_A_Platform` | A, B, C |
| `AvionicsEth` | Embedded / automotive Ethernet | all FC Platform domains, CompactAvionics, TelemetryUnit_A, optional TelemetryUnit_B, optional DebugPC | A, B, C |
| `FwdCAN_A` | CAN-FD, 29-bit CAN profile | all FC Platform domains + forward propulsion/BMS/elevon devices | A, B, C |
| `FwdCAN_B` | CAN-FD, 29-bit CAN profile | same forward actuator set | A, B, C |
| `AftCAN_A` | CAN-FD, 29-bit CAN profile | all FC Platform domains + aft propulsion/BMS/elevon devices | A, B, C |
| `AftCAN_B` | CAN-FD, 29-bit CAN profile | same aft actuator set | A, B, C |
| `IMU_A_Link` | Local serial / UART or RS-422-class | `IMU_A`, FC-A | A, B, C |
| `IMU_B_Link` | Local serial / UART or RS-422-class | `IMU_B`, FC-B | A, B, C |
| `IMU_C_Link` | Local serial / UART or RS-422-class | `IMU_C`, FC-C | A, B, C |
| `GNSS_A_Link` | Local serial / UART or RS-422-class | `GNSS_A`, FC-A | A, B, C |
| `GNSS_B_Link` | Local serial / UART or RS-422-class | `GNSS_B`, FC-B | A, B, C |
| `GNSS_C_Link` | Local serial / UART or RS-422-class | `GNSS_C`, FC-C | A, B, C |
| `GroundRadio_A` | Wireless telemetry/data Link | `TelemetryUnit_A`, Ground Station | A, B, C |
| `GroundRadio_B` | Independent wireless telemetry/data Link | `TelemetryUnit_B`, Ground Station | B, optional C |

Do not add physical Links merely to make the WS mapping easier.

The radio waveform/protocol itself is out of scope. Treat it as a bounded packet or framed-data Link capable of carrying the trial's WS traffic.

---

## Participant-count reference

Ignoring Ground Station and DebugPC:

```text
Flight-control domains:       6
IMUs:                         3
GNSS receivers:               3
BMS units:                    4
Elevon units:                 4
CompactAvionics:              1
TelemetryUnit_A:              1
Propulsion controllers:     4 / 8 / 16
------------------------------------------------
Production total:          26 / 30 / 38
```

Config B adds `TelemetryUnit_B`.

Ground Station is external but participating when connected.

Config C adds DebugPC.

All variants should fit the ordinary preferred low-range Participant allocation.

Do not use extended canonical addressing merely because the larger address space exists.

---

## Standard Service expectations

This archetype deliberately has high Service reuse.

Unless a device is too simple for a particular capability, expect:

| Service / capability | Expected scope |
|---|---|
| Identity / Version | Every production WS Participant |
| Health | Every production WS Participant |
| Fault / diagnostic status | Every intelligent ECU/sensor/actuator |
| Firmware / software update | All realistically field-updatable devices |
| Time synchronization | Flight controllers, sensors, actuator groups, telemetry where useful |
| Link status | FC Platform domains, Telemetry Units, CompactAvionics where applicable |
| Logging / event history | Flight controllers, avionics, telemetry, BMS/actuator devices as capability permits |
| Configuration | Flight controllers, avionics, telemetry, selected actuator/BMS nodes |
| Telemetry | Selected state from most subsystems; rate varies substantially |
| Diagnostic RPC/query | Most smart devices |
| Flight mode/state | Flight-control and avionics participants |
| Energy state | BMS + interested control/avionics consumers |
| Actuator state | Propulsion/elevon nodes + flight-control consumers |

Do not create one Wire per Service.

Do not force all telemetry onto every Link.

---

## Required interactions

Count required **application relationships**, not transparent forwarding hops.

### Flight-controller peer exchange

The three flight-control computers are symmetric peers.

Required interactions include:

| Interaction | From | To | Rate / trigger | Notes |
|---|---|---|---|---|
| Peer flight-control state | each `FC_*_Control` | other flight-control computers | ~50–200 Hz | Estimate/mode/control state needed for redundancy logic |
| Peer Health / fault state | each flight controller | peers | ~10–50 Hz + event | Includes degraded capability |
| Voting / agreement data | flight controllers | peers | control-cycle or event | Application semantics only |
| Mode transition coordination | flight controllers | peers | Event | No network-level leader assumption |
| Time/reference coordination | flight controllers | peers | periodic | Exact algorithm out of scope |

Agents may choose whether peer traffic is authored by Control or Platform domains, but must explain the domain boundary and SHM crossing.

The RS-485 topology must not silently create a canonical “master” flight controller.

### Local sensor flows

For each controller `X`:

```text
IMU_X  -> FC_X
GNSS_X -> FC_X
```

Required data:

| Interaction | Rate / trigger | Notes |
|---|---|---|
| IMU state | ~100–1000 Hz | High-rate local sensor data |
| IMU Health/fault | ~1–10 Hz + event | |
| GNSS state | ~5–20 Hz | Position/velocity/time |
| GNSS Health/solution status | ~1–10 Hz + event | |
| Sensor Identity/Version | startup / query | |

Do not require IMU raw samples to traverse the whole aircraft unless a diagnostic/telemetry use case explicitly asks for them.

### Actuator command flows

The flight-control system must command both forward and aft actuator groups.

Required conceptual interactions:

```text
flight-control command intent
    ->
propulsion controllers
elevon controllers
```

The assignment deliberately does **not** specify which one of the three controllers is “active master.”

Mappers must represent whatever redundant command-consumption pattern they believe is valid under the supplied WS architecture, without inventing new authority semantics.

For each propulsion/elevon unit:

| Interaction | Rate / trigger | Notes |
|---|---|---|
| Command/setpoint | ~100–500 Hz | Exact motor-control inner loop remains local |
| Actuator state | ~50–200 Hz | Position/speed/thrust/current as appropriate |
| Health/fault | ~10–50 Hz + event | |
| Availability/capability | ~10–50 Hz + event | Useful after degraded failures |

The redundant A/B CAN Links exist so loss of one physical bus need not remove actuator connectivity.

### Energy / BMS flows

Each `BMS_*` provides:

| Interaction | Consumers | Rate / trigger |
|---|---|---|
| Available power / limit | flight-control + relevant propulsion consumers | ~10–50 Hz |
| Pack state | flight-control + CompactAvionics | ~5–20 Hz |
| Fault / contactor / isolation status | flight-control + avionics/telemetry | event + periodic |
| Identity/Version/Health | diagnostics | startup / query |

Do not require every propulsion controller to subscribe to every BMS if a simpler quadrant/locality mapping is valid.

### Compact avionics

| Interaction | From | To | Rate / trigger | Notes |
|---|---|---|---|---|
| Aircraft state summary | flight-control system | CompactAvionics | ~10–50 Hz | |
| Mode / warning / fault summary | flight-control + BMS/actuator aggregates | CompactAvionics | event + periodic | |
| Pilot/operator inputs | CompactAvionics | flight-control Service consumers | event / ~10–50 Hz | Not a network authority primitive |
| Identity/Version/Health inventory | diagnostics | CompactAvionics | query | |

Loss of CompactAvionics must not break inner-loop control.

### Ground telemetry

| Interaction | From | To | Rate / trigger | Notes |
|---|---|---|---|---|
| Flight-state telemetry | selected aircraft participants | Ground Station | ~10–100 Hz | Rate reduced/selected for radio |
| Engineering telemetry | selected participants | Ground Station | Configurable | Flight-test use |
| Health/fault summary | aircraft | Ground Station | ~1–10 Hz + event | |
| Logs/events | aircraft | Ground Station | on demand / streamed | |
| Link status | Telemetry Unit(s) | Ground Station / aircraft | periodic | |

Do not forward all raw internal traffic over the radio.

### Ground command / test interactions

The Ground Station is not merely a logger.

Required test/mission capabilities include:

| Interaction | From | To | Trigger | Notes |
|---|---|---|---|---|
| Mission / ODD configuration | Ground Station | flight-control configuration Service | preflight / test | Includes geofence/test-envelope style parameters |
| Test-mode control input | Ground Station | flight-control Service consumers | test dependent | Application-controlled acceptance |
| Operator override | Ground Station | flight-control Service consumers | explicit event | Only when flight/test mode permits |
| Fault/test command | Ground Station | designated test Services | explicit | Do not invent unrestricted memory/register access |
| Identity/Version query | Ground Station | aircraft participants | on demand | |
| Update/configuration transfer | Ground Station | selected participants | service operation | May be relayed via Telemetry Unit |

The mapper must keep **network reachability** separate from **authorization/flight-mode acceptance**.

### Software/update path

At minimum:

```text
Ground Station
    ->
Telemetry Unit
    ->
target flight-control / avionics / actuator / BMS participant
```

The assignment does not require simultaneous update of many critical devices.

The aircraft is not assumed to continue normal flight-control operation while arbitrary safety-critical controllers are being updated.

### Debug / development — Config C only

| Interaction | From | To | Trigger | Notes |
|---|---|---|---|---|
| Engineering telemetry | selected participants | DebugPC | stream/on demand | Prefer rich local Ethernet path |
| Health/fault inspection | DebugPC | aircraft participants | on demand | |
| Link diagnostics | DebugPC | FC Platforms / telemetry / avionics | on demand | |
| Software/configuration | DebugPC | selected participants | explicit | |
| Test-only Service calls | DebugPC | designated test endpoints | explicit | No production role |

**Explicitly not required:**

- Ground Station becoming a canonical controller/Origin;
- one FC designated by WireSpaces as permanent bus master;
- all-to-all communication among all Participants;
- every raw IMU sample on AvionicsEth or GroundRadio;
- every actuator receiving every BMS publication;
- flight-control inner loops running across WS;
- a single Wire spanning every redundant physical Link;
- one Wire per redundancy path;
- separate Participants for lockstep replica cores.

---

## Redundant actuator-network structure

Forward and aft groups are each attached to two independent CAN-FD buses.

### Forward

```text
FwdCAN_A
FwdCAN_B

Participants/devices:
    all FC Platform domains
    all FL propulsion controllers
    all FR propulsion controllers
    BMS_FL
    BMS_FR
    Elevon_FL
    Elevon_FR
```

### Aft

```text
AftCAN_A
AftCAN_B

Participants/devices:
    all FC Platform domains
    all RL propulsion controllers
    all RR propulsion controllers
    BMS_RL
    BMS_RR
    Elevon_RL
    Elevon_RR
```

Actuator devices are dual-homed to the A/B buses for their physical group.

This does **not** require the mapper to make one canonical Wire per physical CAN bus.

The experiment should determine whether one logical propagation domain over redundant Links, multiple Wires, or another valid R6 mapping is clearest under the supplied rules.

Document the choice and its failure implications.

---

## RS-485 flight-controller ring / triangle

The three flight-controller Platform domains are connected by three RS-485 Links:

```text
FC-A <-> FC-B
FC-B <-> FC-C
FC-C <-> FC-A
```

This is physically looped.

The canonical Wire model still requires ordinary Wire forwarding topology to obey the experiment's loop-free Logical Bus rules.

Mappers must therefore explicitly decide how peer Wires are realized across these Links rather than blindly flooding one Wire around the physical triangle.

Do not invent TTL, spanning-tree, or dynamic routing to solve the loop.

This is an intentional architecture stress point.

---

## Interaction structure (reference)

The application graph is dense in a few places but not globally all-to-all.

```text
Flight-control redundancy:
    FC-A <-> FC-B <-> FC-C
    plus A <-> C

Local sensing:
    IMU_A/GNSS_A -> FC-A
    IMU_B/GNSS_B -> FC-B
    IMU_C/GNSS_C -> FC-C

Forward actuation:
    flight-control consumers/producers <-> FL/FR propulsion + elevons
    BMS_FL/BMS_FR -> interested control/propulsion consumers

Aft actuation:
    flight-control consumers/producers <-> RL/RR propulsion + elevons
    BMS_RL/BMS_RR -> interested control/propulsion consumers

Avionics:
    summarized state / warnings / pilot input

Telemetry:
    selected aircraft state -> Telemetry Unit -> Ground Station
    Ground Station -> designated test/configuration Services
```

Standard-Service relations add broad **queryability and observability**, but should not be counted as a requirement for every participant to maintain bespoke point-to-point control relationships with every other participant.

---

## Failure and redundancy assumptions

### Flight computers

- Loss of one complete flight controller: remaining two flight controllers continue in a defined degraded mode.
- Loss of one `FC_*_Control` domain: sibling Platform domain may remain diagnosable, but that flight computer no longer supplies valid control computation.
- Loss of one `FC_*_Platform` domain: local Control may continue executing but loses some/all external communications; document consequences.
- Lockstep disagreement within a physical pair is handled locally by the device's safety architecture; do not model lockstep replicas as WS peers.

### Peer RS-485 network

- Loss of one RS-485 leg must not necessarily isolate any flight controller because the other two legs remain.
- Loss of two appropriate legs may partition a flight controller.
- A physical loop exists, but ordinary WS forwarding must remain loop-free per Wire.

### Sensors

- Loss of one GNSS receiver: associated flight controller degrades its navigation input; other GNSS sources remain elsewhere in the system.
- Loss of one IMU: associated flight controller degrades or becomes unavailable according to application policy; other flight computers remain.
- Sensor faults must be observable through Health/fault Services.

### Actuator CAN

- Loss of `FwdCAN_A`: forward actuators remain reachable via `FwdCAN_B`.
- Loss of `FwdCAN_B`: same via A.
- Equivalent assumptions for aft A/B.
- Simultaneous loss of both buses in one group removes network control/visibility for that actuator group.
- Loss of one actuator node must not take down either CAN bus.

### Propulsion

- Loss of one propulsion controller/drive: reduced thrust/capability.
- Loss of all propulsion units in one quadrant is a severe degraded condition but does not imply total network failure.
- P4/P8/P16 should use the same basic logical model.

### Battery/BMS

- Loss or isolation of one quadrant pack/BMS reduces available power/capability.
- Other quadrants remain observable/controllable where physically possible.
- Flight-control participants must be able to determine changed power capability.

### Telemetry / ground

- Loss of TelemetryUnit_A in Config A: ground link lost; onboard flight control continues.
- In Config B, loss of one telemetry path leaves the other available.
- Ground Station disconnect/loss: onboard flight control continues according to mission/test policy.
- Remote control/override authority must fail according to application policy, not because WS re-elects a controller.

### Avionics

- Loss of CompactAvionics: pilot display/inputs are degraded or unavailable, but flight-controller computation and actuator networking continue.
- No separate redundant display is assumed for this slice.

### Debug

- DebugPC absent/unplugged: no production impact.

Agents should identify whether their Wire decomposition makes these failure boundaries clear or obscures them.

---

## Bandwidth and timing constraints

These are approximate challenge inputs, not a certified control-network budget.

### Flight-controller SHM

- Control/platform exchange may include **100 Hz–1 kHz** local state.
- Shared memory bandwidth is ample.
- Latency/jitter should be low and bounded.
- Avoid pushing purely local intermediate control data onto external Wires.

### RS-485 peer links

Assume:

```text
1–5 Mbit/s class
short airframe wiring
framed datagram-oriented WS profile
```

Peer state can be **50–200 Hz** with fault/event bursts.

The flight-control architecture should not require arbitrary bulk telemetry to share this path.

### CAN-FD actuator buses

Assume:

```text
CAN-FD
29-bit identifiers
2–5 Mbit/s data phase class
```

Typical loads include:

- 100–500 Hz command traffic;
- 50–200 Hz state feedback;
- fault/event bursts;
- Health/version/diagnostic traffic;
- firmware/update traffic only during explicit service operations.

The P16 variant is intended to make repeated-node scaling and bus load nontrivial.

### Local sensor Links

IMU:

```text
~100–1000 Hz state
small bounded frames
```

GNSS:

```text
~5–20 Hz navigation state
lower-rate status/configuration
```

### Avionics Ethernet

Assume at least **100 Mbit/s-class** embedded Ethernet.

Normal WS traffic is small relative to capacity.

Engineering telemetry and software transfer may be much larger.

Fixed full canonical headers are not a meaningful bandwidth concern here.

### Ground radio

Treat the primary radio as **much scarcer and less reliable than Ethernet**.

Assume approximately:

```text
100 kbit/s – several Mbit/s usable payload
variable latency
occasional loss
possible temporary outage
```

Telemetry selection/rate control matters.

Do not assume every aircraft Service should continuously publish across GroundRadio.

---

## Configuration / accounting requirements

At minimum, each mapper must report:

```text
Production Participants:
    P4 / P8 / P16 counts

Physical Links:
    count by type

Canonical Wires:
    count
    purpose
    member Links
    major participant scope

Device-private Wires:
    FC SHM and sensor locality decisions

Flight-control peer Wires:
    how the RS-485 physical loop is represented
    how loop-free forwarding is maintained

Actuator Wires:
    how A/B redundancy is represented
    forward vs aft scope
    whether redundant Links share or duplicate canonical Wires

Avionics / telemetry:
    which Wires reach Ground Station
    which traffic is intentionally excluded from radio

Gateway / forwarding state:
    approximate masks/table entries

Standard Services:
    ubiquitous vs capability-specific

Scaling:
    config delta from P4 -> P8 -> P16
```

Do not hide large configuration growth behind “codegen can generate it.”

Generated configuration is allowed; its size and conceptual burden still count.

---

## Expected diagnostic outcomes

### Question A — R6 symmetric-controller fit

Do global Participant IDs + explicit src/dest + Logical Buses naturally model three equal flight controllers?

A healthy mapping should not require:

- canonical Origin;
- MainA/MainB semantics;
- network-level controller authority;
- address changes during controller failure.

### Question B — multicore continuity

Do:

```text
FC_Control <-> FC_Platform over SHM
```

and:

```text
FC_A <-> FC_B over RS-485
FC -> actuator over CAN-FD
FC -> avionics over Ethernet
```

feel like one coherent Service architecture?

This is a direct test of intra-device + inter-device continuity.

### Question C — physical-loop pressure

Can the RS-485 triangle/ring be represented cleanly while keeping each canonical Wire's forwarding topology loop-free?

Valid outcomes may include:

```text
"Three pair/locality Wires are clearer than one flooded peer Wire."

"One logical peer Wire is possible only with an explicitly chosen loop-free
 forwarding tree; the spare physical leg remains available for another Wire /
 failover configuration."

"The physical ring strongly pressures static Wire topology and needs a better
 deployment pattern."
```

Do not invent dynamic routing to make the test pass.

### Question D — redundant CAN representation

Do redundant A/B CAN buses map naturally to Logical Bus semantics?

The trial should make clear whether:

- one Wire spanning redundant Links;
- separate Wires;
- endpoint/service-level redundancy;

is simplest under the supplied rules.

Do not force all redundant physical paths into one canonical abstraction if that obscures failure behavior.

### Question E — repeated-node scaling

Does P4 -> P8 -> P16 mostly add repeated Participants/config entries, or does it force qualitative architecture changes?

A good result is systematic linear growth.

A bad result is combinatorial mapping/configuration growth.

### Question F — remote authority

Can Ground Station control/test/ODD interactions be represented as ordinary Participant + Service behavior, with acceptance determined by application state?

The network should carry the traffic without deciding whether Ground Station is presently authorized to influence flight behavior.

### Question G — wireless scarcity

Can the architecture expose the same standard Services over a constrained/intermittent GroundRadio without assuming continuous high-rate publication from every node?

This should pressure Service usage/rate choices more than canonical addressing.

### Question H — Service ecosystem density

Do Identity, Version, Health, faults, update, telemetry, time, logs, and Link status remain coherent across:

```text
tiny actuator MCU
sensor
BMS
flight-control multicore SoC
telemetry unit
display SoC
Ground Station PC
```

This is one of the assignment's most important project-level questions.

### Question I — failure auditability

Can an engineer inspect the mapping and understand:

```text
one FC lost
one RS-485 leg lost
one actuator CAN lost
one quadrant propulsion group lost
one BMS lost
ground link lost
display lost
```

without tracing an opaque web of Wires?

---

## Valid experiment outcomes

Examples:

```text
"R6 is a strong fit: the three FCs remain peers, authority stays in Services,
 and lockstep hardware does not leak into canonical identity."

"SHM + RS-485 + CAN-FD + Ethernet use the same Service model cleanly;
 transport/link differences remain mostly below Service implementations."

"The RS-485 triangle is the main topology pressure point; loop-free Logical
 Buses require an explicit static decomposition that is understandable but not
 entirely free."

"Forward/aft redundant CAN groups map naturally and degraded physical-link
 behavior remains visible."

"P16 scales mostly by repeated Participant/config entries, with no new protocol
 mechanism."

"Ground Station control fits naturally as Service-level conditional authority;
 no special network role is needed."

"Ground radio makes ubiquitous periodic Health/telemetry too chatty; standard
 Services need capability/rate profiles rather than fixed assumptions."

"Configuration is technically valid but redundancy causes Wire proliferation;
 the Logical Bus model may need a clearer redundancy deployment pattern."

"Flight-control redundancy exposes hidden assumptions in forwarding or
 endpoint binding that were not visible in smaller sketches."
```

---

## What this archetype does not include

The following equipment may exist physically but is **outside WireSpaces scope for this assignment**:

- ADS-B transponder / receiver;
- conventional aviation VHF communications;
- lightweight emergency/backup aviation radio equipment;
- tablet / iPad-style basic navigation display using its own GPS/navigation ecosystem;
- certification-specific avionics buses not explicitly listed;
- regulatory/certification architecture;
- flight-control algorithms or stability/control-law design;
- motor inverter inner control loops;
- detailed battery cell-monitor topology;
- RF waveform/modem design;
- cloud infrastructure.

Also excluded unless separately assigned:

- dynamic participant discovery/readdressing;
- dynamic routing;
- arbitrary ad-hoc wireless mesh;
- VCN/CAN11 compression analysis;
- extended canonical Participant/Wire address ranges;
- lockstep cores as independent Participants;
- a requirement that every sensor/actuator have multiple internal WS domains.

---

## Comparison to archetype 10

| | **10 — Automotive Zone + Core** | **11 — Redundant eVTOL** |
|---|---|---|
| Production Participants | ~20 | ~26 / 30 / 38 |
| Multicore modelling | Central/zone/TCU domains | 3 flight controllers × 2 usable domains |
| Shared memory | Several SoC islands | One SHM island per flight controller |
| Controller structure | Central + zone hierarchy | Three symmetric redundant controllers |
| Ethernet | Main backbone | Avionics/service backbone |
| CAN | FD + committed/Guest CAN11 | Four redundant CAN-FD actuator buses |
| RS-485 | No | Flight-controller triangle/ring |
| Wireless | No | Ground telemetry/control link |
| Repeated leaves | Moderate | 4 / 8 / 16 propulsion controllers |
| Remote operator | Service/debug PC | Ground Station with test/mission control authority |
| Main topology stress | Heterogeneous service-rich vehicle slice | Redundancy + physical loop + repeated actuation |
| Main Service stress | Fleet/platform service density | Critical control + remote test + constrained radio |
| Primary role | Larger-system integration test | Advanced redundancy / multicore / wireless challenge |

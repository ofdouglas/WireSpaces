# Sketch — Archetype 11: Redundant eVTOL Flight Control

```text
**Archetype:** archetypes/11_redundant_evtol_flight_control.md
**Agent:** cursor_A
**Output file:** sketches/11_redundant_evtol_flight_control_cursor_A.md
**Date:** 2026-08-23
```

---

## Executive summary

```text
**Minimum mapping:** P8 uses 30 onboard production Participants, Ground Station, 12 canonical Wires in Config A, pairwise RS-485 peer Wires, four separate redundant-path actuator Wires on CAN-FD/29-bit, and no CAN11.
**Question A (R6):** Natural — symmetric controllers, multicore domains, repeated leaves, and remote authority remain ordinary Participants and Services.
**Question B (CAN11):** N/A — actuator networks use CAN-FD / 29-bit profiles.
**Worst friction (minimum path):** Configuration burden — Significant.
**Main lesson:** Identity, authority, multicore continuity, and repeated-node scaling fit naturally; genuine pressure appears where loop-free Wires expose path choice and turn redundant physical delivery into multiple canonical interactions.
```

---

## Disposition block

| Area | Assessment |
|---|---|
| Participant identity | Natural |
| Wire decomposition | Mildly awkward |
| Forwarding | Moderate |
| Non-CAN configuration | High |
| CAN11 VCN fit | N/A |
| Better with CAN29? | N/A — actuator buses already use a 29-bit profile |

**Explanation:** Question A: one deployment-global ParticipantId per actual Endpoint Domain fits the six flight-controller domains, dedicated sensors, repeated actuator/BMS nodes, avionics, telemetry, and Ground Station without assigning a primary controller. Application voting, command acceptance, and Ground Station influence remain Service/application behavior; the network carries addressed traffic but does not decide authority.

The main R6 pressure is topology rather than identity: the RS-485 triangle cannot be one flooded Wire over all three legs, and the parallel actuator buses cannot both realize one ordinary loop-free Wire without duplicate/cyclic realization. Three pairwise FC-peer Wires and four actuator-path Wires are explicit and auditable, but they deliberately expose path choice: alternate physical reachability is not used automatically, and redundant delivery becomes multiple canonical PDUs that Services must correlate. Question B is not exercised because the actuator buses are CAN-FD with 29-bit identifiers; no VCN or WireAlias compression is used.

---

## 1. Native communication model

### Configurations

- **Config A — flight / production:** three flight computers, dedicated sensor pairs, duplicated forward/aft actuator buses, P8 propulsion, four elevons, four BMS units, CompactAvionics, TelemetryUnit_A, and Ground Station when connected.
- **Config B — redundant telemetry:** Config A plus TelemetryUnit_B and an independent radio path.
- **Config C — development / flight test:** Config A or B plus DebugPC on AvionicsEth.

The aircraft continues basic flight control without Ground Station, DebugPC, CompactAvionics, either telemetry unit, or external infrastructure.

### Natural relationships

Each flight computer has a Control domain and a Platform domain joined by shared memory. Control computes estimates, modes, and command intent. Platform owns external links, stages peer state, publishes external actuator commands, and terminates platform/diagnostic Services. Dedicated IMU and GNSS devices feed only their associated flight computer.

The three flight computers are equal peers. Their Platform domains exchange staged control state, Health/fault state, voting data, mode transitions, and time/reference data over three direct RS-485 legs:

```text
FC_A_Platform <-> FC_B_Platform
FC_B_Platform <-> FC_C_Platform
FC_C_Platform <-> FC_A_Platform
```

No controller is the network master. Each Platform publishes external actuator commands derived from its sibling Control domain's intent. Actuator Services decide how redundant controller inputs and duplicate A/B network paths are accepted; that policy is not encoded in addressing.

Forward and aft actuator groups each have two independent CAN-FD buses. Loss of one bus leaves the corresponding group reachable on the other. Propulsion count changes from P4 to P8 to P16 without changing the architecture.

CompactAvionics exchanges summarized state, warnings, and pilot inputs with FC Platforms over AvionicsEth. Telemetry units select and stage data for scarce radio links. Ground Station commands, overrides, test calls, and configuration are accepted or rejected according to application mode, not a network role.

---

## 2. Obvious conventional implementation

> **Obvious conventional implementation:** device-local IPC/shared memory; three bespoke peer links or a statically managed RS-485 ring; duplicated CAN-FD command/status databases for forward and aft groups; Ethernet topics/RPC for avionics; telemetry gateways that aggregate, rate-limit, and proxy radio operations; application-level voting and redundant-command acceptance.

> **What structure does a conventional design use?** Per-link message dictionaries, controller-pair protocols, duplicated CAN databases, explicit gateway/proxy logic, device-role IDs, update routing, and hand-maintained failure-mode configuration.

WireSpaces adds global ParticipantIds, named loop-free Logical Buses, canonical source/destination identity, explicit forwarding tables for the few multi-Link Wires, and a common Service model. It does not eliminate the system's real redundancy configuration; it makes pair scopes, redundant paths, and composition boundaries inspectable.

---

## 3. Minimum mapping

The primary mapping uses **P8**, as assigned by the archetype.

### 3.1 Participants

| PID | Endpoint Domain / Participant | Device | Notes |
|---|---|---|---|
| 0x01 | FC_A_Control | FC-A lockstep pair 1 | Control computation |
| 0x02 | FC_A_Platform | FC-A lockstep pair 2 | External links and platform Services |
| 0x03 | FC_B_Control | FC-B lockstep pair 1 | Control computation |
| 0x04 | FC_B_Platform | FC-B lockstep pair 2 | External links and platform Services |
| 0x05 | FC_C_Control | FC-C lockstep pair 1 | Control computation |
| 0x06 | FC_C_Platform | FC-C lockstep pair 2 | External links and platform Services |
| 0x07–0x09 | IMU_A / IMU_B / IMU_C | Dedicated IMUs | One Participant each |
| 0x0A–0x0C | GNSS_A / GNSS_B / GNSS_C | Dedicated GNSS | One Participant each |
| 0x0D–0x10 | Prop_FL1/2, Prop_FR1/2 | Forward propulsion | P8: two per forward quadrant |
| 0x11–0x14 | Prop_RL1/2, Prop_RR1/2 | Aft propulsion | P8: two per aft quadrant |
| 0x15–0x18 | Elevon_FL / FR / RL / RR | Control surfaces | One per quadrant |
| 0x19–0x1C | BMS_FL / FR / RL / RR | Battery controllers | One per quadrant |
| 0x1D | CompactAvionics | Display/SoC | Not inner-loop critical |
| 0x1E | TelemetryUnit_A | Primary radio gateway | Config A/B/C |
| 0x1F | GroundStation | Ground computer | Configured member; may be unreachable |
| 0x20 | TelemetryUnit_B | Backup radio gateway | Config B, optional C |
| 0x21 | DebugPC | Development workstation | Config C only |

**Production onboard counts:** P4 = 26, P8 = 30, P16 = 38. Ground Station is an external Participant when configured. Config B adds one onboard Participant; Config C adds DebugPC.

Lockstep replicas are not separate Participants. Participant identity is stable across all Wires; no identity changes when an FC, actuator bus, radio, or Ground Station is lost.

### 3.2 Wires

#### Config A — 12 canonical Wires

| Wire | Participants / major scope | Physical realization | Purpose |
|---|---|---|---|
| W01 FC_A_Local | FC_A_Control, FC_A_Platform, IMU_A, GNSS_A | FC_A_SHM + IMU_A_Link + GNSS_A_Link | Local sensing, control intent, local Health |
| W02 FC_B_Local | FC_B_Control, FC_B_Platform, IMU_B, GNSS_B | FC_B_SHM + IMU_B_Link + GNSS_B_Link | Same for FC-B |
| W03 FC_C_Local | FC_C_Control, FC_C_Platform, IMU_C, GNSS_C | FC_C_SHM + IMU_C_Link + GNSS_C_Link | Same for FC-C |
| W04 FC_Peer_AB | FC_A_Platform, FC_B_Platform | FC_AB_RS485 | Direct A↔B peer exchange |
| W05 FC_Peer_BC | FC_B_Platform, FC_C_Platform | FC_BC_RS485 | Direct B↔C peer exchange |
| W06 FC_Peer_CA | FC_C_Platform, FC_A_Platform | FC_CA_RS485 | Direct C↔A peer exchange |
| W07 FwdAct_A | all FC Platforms; forward propulsion, BMS, elevons | FwdCAN_A | Forward actuator path A |
| W08 FwdAct_B | same canonical Participants as W07 | FwdCAN_B | Forward actuator path B |
| W09 AftAct_A | all FC Platforms; aft propulsion, BMS, elevons | AftCAN_A | Aft actuator path A |
| W10 AftAct_B | same canonical Participants as W09 | AftCAN_B | Aft actuator path B |
| W11 Avionics | all FC Platforms, CompactAvionics, TelemetryUnit_A; DebugPC in C; TU_B in B | AvionicsEth | Aircraft summaries, pilot input, engineering/service staging |
| W12 Ground_A | GroundStation, TU_A, all FC Platforms, CompactAvionics | GroundRadio_A + AvionicsEth | Selected telemetry and Platform-mediated ground service operations |

**Config B:** add **W13 Ground_B**, with the same narrow service scope as W12 but realized by GroundRadio_B + AvionicsEth through TelemetryUnit_B. W12 and W13 are distinct canonical Wires; the application chooses one path or correlates duplicate operations.

**Config C:** no new Wire is required. DebugPC is a configured W11 member. Broad device operations are mediated by FC Platform Services; DebugPC is not added to flight-control or actuator command Wires.

#### Why these boundaries

- W01–W03 are real per-computer locality/fault-containment scopes; raw sensor traffic does not traverse the aircraft.
- W04–W06 map the physical peer triangle into three loop-free pair scopes. No ordinary Wire floods around the physical loop.
- W07–W10 make redundant actuator paths explicit. Commands and state are authored on both path Wires when redundancy policy requires; one PDU does not silently exist on two Wires.
- W11 is the onboard avionics/service scope.
- W12/W13 terminate at real gateway/application boundaries: FC Platform Services stage telemetry and proxy/compose operations toward Control, sensor, actuator, BMS, and elevon targets. They do not include SHM or actuator CAN branches. This avoids baseline flood-and-filter traffic on irrelevant internal branches and keeps radio selection explicit.

### 3.3 Interactions

| Interaction | Src | Dest | Wire(s) | Broadcast? | Notes |
|---|---|---|---|---|---|
| Local IMU state/Health | IMU_X | FC_X_Control | W01/W02/W03 | No | Platform forwards sensor PDU to SHM; source preserved |
| Local GNSS state/Health | GNSS_X | FC_X_Control | W01/W02/W03 | No | Local only |
| Control intent / local state | FC_X_Control | FC_X_Platform | W01/W02/W03 | No | 100 Hz–1 kHz SHM path |
| Peer FC state/voting/mode | FC_X_Platform | each other FC Platform | corresponding two of W04–W06 | No | Platform authors staged peer state; pairwise duplicate publication |
| Propulsion/elevon command | each FC Platform | selected actuator | W07+W08 or W09+W10 | No | Separate canonical PDU on each redundant path; Service resolves controller/path inputs |
| Actuator state/Health | actuator | FC Platforms | corresponding A and B actuator Wires | Directed or broadcast | Duplicate path publication only where required |
| BMS power capability | quadrant BMS | FC Platforms and relevant propulsion | corresponding forward/aft A+B Wires | Directed/broadcast | Locality by group/quadrant |
| Aircraft summary/warnings | FC Platforms | CompactAvionics | W11 | No | Not inner-loop data |
| Pilot/operator input | CompactAvionics | FC Platform Service consumers | W11 | No | Platform composes local Control-domain intent on W01–W03 |
| Selected ground telemetry | FC Platform / TU / CompactAvionics staging Service | GroundStation | W12; W13 in B | No | Rate-selected composition; no raw all-aircraft flood |
| Ground mission/test/override | GroundStation | designated FC Platform gateway Service | W12 or W13 | No | Platform composes intent onto W01–W03; final acceptance is application-mode dependent |
| Identity/Health/query | GroundStation | FC Platform inventory/proxy Service | W12 or W13 | No | Direct only for W12/W13 members; Platform mediates other targets |
| Update/config transfer | GroundStation | FC Platform update gateway | W12 or W13 | No | Reliable remote leg; Platform terminates and authors target-side operation |
| Engineering telemetry / debug | FC Platforms / TU / CompactAvionics | DebugPC | W11 | No | Config C |
| Debug test/config call | DebugPC | designated Platform Service | W11 | No | Platform composes onward only where intended |

**Composition boundaries:** Control→Platform command intent becomes a Platform-authored external actuator or peer PDU. CompactAvionics input becomes a Platform-authored local Control-domain PDU. Aggregated/rate-selected radio telemetry is newly authored by its staging Participant. Ground operations terminate at Platform gateway Services and become target-side Service operations on local/actuator Wires. These are semantic transformations, not forwarding; Ground Station source identity is not falsely preserved across a proxy boundary.

**Redundancy is not forwarding:** publishing an actuator command on W07 and W08 creates two canonical PDUs with different Wire identities. Endpoint/Service logic correlates them; WireSpaces does not elect the accepted controller or path.

### 3.4 Forwarding

| Router / ingress | Wire | Egress | Notes |
|---|---|---|---|
| FC_X_Platform sensor serial | W01/W02/W03 | local SHM | Preserves IMU/GNSS canonical source |
| FC_X_Platform SHM | local Wire | sensor Link only for directed sensor service requests | Local tree; no aircraft-wide propagation |
| TelemetryUnit_A radio | W12 | AvionicsEth | Same canonical Ground_A PDU |
| TelemetryUnit_A Ethernet | W12 | GroundRadio_A | Rate/service policy is at authored traffic; forwarding preserves identity |
| TelemetryUnit_B radio/Ethernet | W13 | Ethernet/radio | Config B |

W04–W10 and W11 coincide with one Physical Link each and require no inter-Link forwarding.

Approximate forwarding state in Config A:

```text
3 local FC trees:        3 Wires, 3 interfaces each
1 remote service path:   GroundRadio_A <-> TU_A <-> AvionicsEth
pair/actuator/avionics:  direct Link bindings, no gateway hop
```

This is bounded and systematic. Config B duplicates only the radio↔Ethernet forwarding path under W13; target-side operations remain Platform composition in both configurations.

### 3.5 Link profiles and physical-link accounting

| Physical Link class | Config A count | Profile / Wires |
|---|---:|---|
| Shared memory / IPC | 3 | W01–W03 |
| RS-485 | 3 | W04–W06, framed datagram |
| Embedded Ethernet | 1 | W11, W12; W13 in B |
| CAN-FD / 29-bit | 4 | W07–W10 |
| Local sensor serial | 6 | W01–W03 |
| Ground radio | 1 | W12 |
| **Total Config A** | **18** | |
| Config B delta | +1 radio | W13; total 19 |
| Config C delta | no new Physical Link | DebugPC attaches to AvionicsEth |

All Link profiles reconstruct full canonical Wire, source, and destination before generic routing. Fixed canonical headers are acceptable on Ethernet/radio; constrained CAN11 compression is not part of this mapping.

### 3.6 CAN11 bindings

**N/A.** No Classical CAN11 physical bus is present. All actuator buses are CAN-FD with 29-bit identifiers.

```text
CAN11 WireAliases used:       0
CAN11 VCN relations:          0
Question B (unified VCN):     not exercised
```

---

## 4. Optional optimizations and rejected mappings

### 4.1 One FC peer Wire over the RS-485 triangle

A single W_Peer could use two RS-485 legs as a loop-free spanning tree and leave the third leg outside that Wire.

**Benefit:** one publication reaches both peers and reduces Wire count from three to one.

**Rejected for minimum:** one selected-leg failure can partition the Wire until a reviewed alternate static configuration is installed; using all three legs creates a forbidden Wire loop. The three pair Wires use every physical leg concurrently and make each leg's failure explicit. They do require each FC Platform to publish peer state on two Wires.

### 4.2 Multiple loop-free spanning-tree peer Wires

Two or three peer Wires could each use a different two-leg tree, duplicating peer traffic for path redundancy.

**Rejected:** more forwarding and duplicate traffic than direct pair Wires, with harder failure auditing. It is valid if an implementation specifically needs all three Participants reachable on each canonical publication after one path loss.

### 4.3 One actuator Wire spanning A and B buses

One W_Fwd and one W_Aft could try to include both redundant CAN buses.

**Rejected:** all FC Platforms and actuator nodes are attached to both parallel buses, creating a cyclic/duplicate realization for one ordinary Wire and obscuring which physical path survived. Selecting only one active Link plus a failover configuration is valid but does not provide concurrent path availability. Separate A/B Wires expose failures honestly.

### 4.4 One Wire per controller or per Service

Per-controller actuator Wires, per-Service Wires, or one Wire per propulsion unit would multiply configuration combinatorially.

**Rejected:** controller authority belongs to Services, and standard Services reuse the existing scope Wires. Repeated propulsion nodes add memberships, not new Wires.

### 4.5 Broad aircraft Wire reaching the radio

A broad direct-target Ground Wire could span radio, Ethernet, three SHM branches, and selected actuator CAN buses so Ground Station retains canonical end-to-end source/destination identity.

**Rejected for the minimum:** a loop-free tree is possible, but baseline flood-and-filter forwarding (`CORE §12.1`) still sends directed traffic into irrelevant member branches. A Ground→FC_B_Control PDU can consume forward/aft CAN capacity, and an actuator update can enter unrelated SHM/CAN branches.

Two valid alternatives remain:

1. configure destination-pruned forwarding with a participant-location map, count that map as meaningful deployment state, and retain end-to-end target identity; or
2. use the chosen narrow W12/W13 and terminate remote operations at Platform gateway Services.

The minimum chooses #2 because application mediation and radio rate selection already exist in the archetype.

---

## 5. Friction signals

| Signal | None / Mild / Significant | Justification |
|---|---|---|
| Artificial Wire | None | Every Wire corresponds to locality, a direct FC pair, a redundant physical actuator path, onboard avionics, or a remote service path. |
| Wire proliferation | Mild | Twelve Config-A Wires is substantial, but seven are demanded by three direct peer legs plus four explicit actuator redundancy paths; per-Service/per-node Wires are avoided. |
| Artificial hierarchy | None | FC-A/B/C remain peers; Ground Station and telemetry units gain no networking authority; FC Platform is a real domain boundary. |
| VCN pressure | N/A | No CAN11. |
| WireAlias pressure | N/A | No CAN11. |
| Configuration burden | Significant | The explicit state is large but mostly inherent: 30 onboard Participants, 18 physical Links, 12 Wires, repeated dual-path memberships, and proxy bindings must be audited. WS-specific additions are pair/path Wire selection and Service correlation. |
| Failure/topology mismatch | Mild | Pair/path Wires make failures clear, but surviving alternate physical reachability is not automatically exploited and redundant delivery is not one canonical interaction. |

Additional pressure:

- **Participant/interaction awkwardness:** None. Six FC domains and stable global IDs match the hardware fault boundaries.
- **Role stability:** None. No address or networking-role reassignment occurs after FC, link, or radio loss.
- **Remote authority:** None at the network layer. Ground Station reachability is separate from application acceptance.

---

## 6. Model pressure

- **If you could change one WS concept:** keep an optional lower-level redundant/cyclic Link profile as an open architecture question. This sketch is concrete evidence for investigating whether one canonical PDU should ever be delivered over several physical paths with duplicate suppression below Wire. Do not invent that mechanism in this mapping; first compare independent Archetype 11 mappings.
- **Useful distinction exposed by WS:** the mapping separates four concepts conventional diagrams often conflate:

```text
Participant identity
    != controller authority
    != canonical Wire scope
    != redundant Physical Link
```

It also makes forwarding vs composition concrete. Sensor PDUs retain sensor source identity through Platform forwarding to SHM; Control intent becomes a Platform-authored external command because that is a real semantic boundary.

### Standard Service placement

| Service class | Placement / scope |
|---|---|
| Identity / Version / Health | Every capable Participant, over its existing local/actuator/avionics scope |
| FC peer Health/state | W04–W06 |
| Sensor state/Health | W01–W03 only |
| Actuator/BMS state, fault, capability | W07–W10, limited by forward/aft group |
| Aircraft inventory/summary | FC Platforms on W11 |
| Ground query/update/test | W12/W13 terminate at Platform gateway Services; Platforms proxy/compose target operations |
| Link status | FC Platforms, telemetry units, CompactAvionics on W11/W12/W13 |
| Logs/engineering telemetry | staged and rate-selected; raw inner-loop data excluded from radio |

No Wire is created per Service.

---

## 7. Failure audit and scaling

### Failure behavior

| Failure | Mapping consequence |
|---|---|
| One FC complete loss | Its local Wire and two pair endpoints disappear; remaining FC pair retains its direct peer Wire and both actuator paths |
| One FC Control loss | Sibling Platform remains diagnosable but no valid local control computation |
| One FC Platform loss | Sibling Control loses external command/peer paths; other two FCs remain attached |
| One RS-485 leg loss | One pair Wire fails; no FC is isolated from both other peer legs, but the affected pair loses direct exchange unless application-mediated relay or alternate reviewed config is used |
| One actuator CAN A/B loss | Corresponding W07/W08/W09/W10 unavailable; sibling path Wire remains |
| Both buses in one group lost | Forward or aft actuator scope unreachable |
| One propulsion node/quadrant lost | Membership/node failure only; Wire topology remains |
| GroundRadio_A or TU_A loss | W12 remote path unavailable; onboard W01–W11 unaffected |
| Config B single radio/TU loss | Other Ground Wire remains; no network election |
| Ground Station disconnect | Configured membership remains; physical reachability lost; flight continues |
| CompactAvionics or DebugPC loss | W11 remains; inner control and actuator Wires unaffected |

### P4 → P8 → P16 scaling

| Variant | Onboard production Participants | Propulsion per forward/aft group | Canonical Wires | Membership/config delta |
|---|---:|---:|---:|---|
| P4 | 26 | 2 forward / 2 aft | 12 (Config A) | Baseline |
| P8 | 30 | 4 forward / 4 aft | 12 | +4 Participants; each joins two redundant group Wires |
| P16 | 38 | 8 forward / 8 aft | 12 | +8 vs P8; +16 actuator-Wire memberships |

Config B adds one Participant, one radio Link, and W13 at every propulsion scale. Config C adds DebugPC membership on W11 only. Scaling is linear in repeated nodes and dual-path memberships; it does not require wider canonical identity or a new protocol mechanism.

---

## 8. Open questions

- Does the implementation want pairwise FC state authored independently on two peer Wires, or a single peer Wire with a reviewed active-tree/failover configuration?
- Should actuator state be published on both A/B Wires continuously, or command on both with status returned on one preferred path? This affects bus load but not topology validity.
- Would destination-pruned forwarding plus an auditable participant-location map justify a broad direct-target Ground Wire in a later configuration?
- Is loss of end-to-end GroundStation source identity across the Platform proxy acceptable for every query/update/test Service, or do selected Services warrant direct reachability?
- What endpoint-level correlation metadata is used for duplicate controller commands and duplicate A/B path messages? This is Service/transport design, not a new routing role.
- Can Organizer tooling render per-Wire failure matrices clearly enough that the 12/13-Wire configuration remains reviewable?

---

## 9. Spec findings

| ID | Finding |
|---|---|
| SF-R6-009 | Loop-free static Wires expose path choice on a physical ring: alternate physical reachability remains unused unless represented by additional Wires, application relay, destination/path configuration, or a reviewed failover configuration. |
| SF-R6-010 | Parallel physical actuator redundancy becomes multiple canonical Wire interactions under the current model; Services/transports must correlate duplicate A/B operations. This is explicit and deterministic, but pressures whether an optional redundant-Link profile belongs below Wire. |
| SF-R6-011 | A loop-free broad service tree still incurs flood-and-filter branch tax. Direct Ground-to-target reachability requires destination-pruned location state or narrower gateway/proxy Wires; acyclicity alone does not provide traffic locality. |

These findings do **not** justify dynamic routing or a new protocol feature in this sketch. They should remain open through independent Archetype 11 mappings.

---

## 10. Diagrams

### Canonical scope overview

```text
Local FC scopes:
  W01: IMU_A -- Platform_A --SHM-- Control_A -- GNSS_A
  W02: IMU_B -- Platform_B --SHM-- Control_B -- GNSS_B
  W03: IMU_C -- Platform_C --SHM-- Control_C -- GNSS_C

FC peer triangle (three loop-free pair Wires):
  Platform_A ===== W04 / RS485_AB ===== Platform_B
  Platform_B ===== W05 / RS485_BC ===== Platform_C
  Platform_C ===== W06 / RS485_CA ===== Platform_A

Actuator redundancy (distinct canonical Wires):
  W07 FwdAct_A   ||   W08 FwdAct_B
  W09 AftAct_A   ||   W10 AftAct_B

Onboard service:
  W11 AvionicsEth

Remote:
  Ground --Radio_A-- TU_A --Ethernet-- FC Platform gateways  (W12)
  Ground --Radio_B-- TU_B --Ethernet-- FC Platform gateways  (W13, Config B)

  Platform gateway Services compose/proxy toward W01–W10.
```

### Platform composition boundary

```text
Control_X -- W0X / SHM --> Platform_X
  local estimate/command intent

Platform_X -- W04..W06 --> peer Platforms
Platform_X -- W07..W10 --> actuator/BMS/elevon nodes

These external PDUs are authored by Platform_X.
Platform is not transparently relabeling a Control-domain PDU.
```

### Redundant actuator behavior

```text
FC Platform command
    +-- PDU on W07 FwdAct_A --> forward target
    `-- PDU on W08 FwdAct_B --> forward target

Loss of FwdCAN_A removes W07.
W08 remains; no ParticipantId change and no controller election.
```

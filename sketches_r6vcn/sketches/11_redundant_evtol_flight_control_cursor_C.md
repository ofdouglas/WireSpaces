# Sketch — Archetype 11: Redundant eVTOL Flight-Control + Distributed Propulsion

```text
**Archetype:** archetypes/11_redundant_evtol_flight_control.md
**Agent:** cursor_C
**Output file:** sketches/11_redundant_evtol_flight_control_cursor_C.md
**Variant mapped:** P8 (Config A baseline; B and C as deltas)
**Date:** 2026-08-23
```

---

## Executive summary

```text
**Minimum mapping:** 30 production Participants (P8), 18 Wires (3 device-private + 15 network-visible, 16 distinct WireNumbers), 28 Wire-forwarder scopes, 5 splices; no CAN11 anywhere.
**Question A (R6):** Natural — three flight controllers stay peers; no Origin, no Main, no address change on failure.
**Question B (CAN11):** N/A — archetype has no Classical CAN11.
**Worst friction (minimum path):** Wire proliferation — Significant (13 of 18 Wires track physical paths).
**Main lesson:** The loop-free Logical Bus invariant makes redundancy inexpressible as one Wire: every redundant path (3 RS-485 legs, 4 A/B CAN-FD buses, 2 radios) becomes its own Wire with selection pushed to Services. That keeps authority out of the network — exactly what a triplex FCS wants — but R6 has no named redundancy pattern, so every mapper re-derives it.
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
| Better with CAN29? | N/A (already CAN-FD / 29-bit by requirement) |

**Explanation (Question A — R6).** The canonical model handles the hard part of this archetype cleanly and without help. Three flight controllers are six Participants (two usable domains each; lockstep replicas are invisible to WS, as required). There is no Origin, no Main position, no canonical primary, and **no ParticipantId changes when a controller fails** — a failed FC simply stops sourcing traffic, and its peers observe that through Health Services rather than through a network role change. Command intent from `FC_A_Control` arrives at an actuator with `Src = FC_A_Control` preserved across a shared-memory hop and a CAN-FD bus, which is what makes application-level voting possible at all: the receiver can tell *which* control domain authored each command. Authority, mode selection, and voting stay entirely in Services.

**Explanation (Wire decomposition — the honest caveat).** No individual Wire is artificial; every one names a real propagation domain. But the loop-free invariant (`CORE §3.3`) forbids merging redundant paths — a single Wire spanning `FwdCAN_A` and `FwdCAN_B` is a cycle, because the actuators are dual-homed to both — so 13 of the 18 Wires exist because the airframe has 13 physically distinct paths (6 dedicated sensor links, 3 RS-485 legs, 4 A/B CAN buses). The result is correct and auditable, and it is close to a 1:1 naming of the physical topology, which means the logical layer adds identity and Service reuse here rather than structural compression. See SF-R6-011.

**Question B — CAN11.** Not applicable. This archetype specifies CAN-FD with 29-bit identifiers on all four actuator buses and explicitly excludes VCN/CAN11 compression analysis. Direct canonical addressing is sufficient everywhere; no relation table, alias, or VCN map appears in this mapping.

**Archetype Questions A–I are answered individually in §10.**

---

## 1. Native communication model

A human-capable experimental eVTOL evolved from a UAV architecture, with triple-redundant flight control and quadrant-distributed propulsion.

**Three flight-control computers** (FC-A/B/C) are symmetric peers. Each is a four-core safety MCU arranged as two lockstep pairs, giving **two usable execution domains**: a `Control` domain (state estimation, control laws, flight mode, actuator command intent) and a `Platform` domain (external Link ownership, communications, Health/fault/platform Services, configuration, update, telemetry staging). The two communicate over device-local shared memory. There is no primary controller — voting, mode selection, and degraded operation are application concerns.

**Dedicated sensing:** each FC has its own IMU (100–1000 Hz) and GNSS receiver (5–20 Hz) on private serial links. Sensor data is not shared aircraft-wide; each controller estimates from its own sensors and cross-checks via peer exchange.

**Peer exchange** runs over three RS-485 legs forming a physical triangle (A–B, B–C, C–A) at 1–5 Mbit/s, carrying peer control state at 50–200 Hz plus Health, voting data, and mode coordination.

**Actuation** is split forward/aft, each group on two independent CAN-FD buses (`FwdCAN_A`/`_B`, `AftCAN_A`/`_B`) at 2–5 Mbit/s. All three FC Platform domains attach to all four buses. Forward carries FL/FR propulsion, `BMS_FL`/`BMS_FR`, `Elevon_FL`/`Elevon_FR`; aft carries the RL/RR equivalents. Actuator devices are **dual-homed** to their group's A and B buses, so losing one bus does not lose the actuators.

**Avionics and ground:** an embedded Ethernet backbone connects the three FC Platforms, a CompactAvionics display SoC (not in the inner loop), and `TelemetryUnit_A`. The telemetry unit reaches the Ground Station over a scarce, intermittent radio (100 kbit/s – few Mbit/s, variable latency, occasional outage). The Ground Station is a genuine participant that issues mission/ODD configuration, test-mode inputs, and operator overrides — but only when the aircraft's application mode permits. Config B adds a second telemetry unit and radio; Config C adds a DebugPC on the Ethernet.

**Repeated hardware scales** without architectural change: P4/P8/P16 vary only the propulsion-controller count per quadrant (1/2/4).

**Degraded operation is a first-class requirement:** one FC lost → two continue; one RS-485 leg lost → no FC isolated; one actuator bus lost → actuators still reachable; ground link lost → flight continues; display lost → inner loop unaffected.

---

## 2. Obvious conventional implementation

> **Obvious conventional implementation:** Hand-defined cross-channel data link (CCDL) frame formats on point-to-point RS-485 between flight computers; a fixed CAN-FD message ID matrix per actuator bus, duplicated for the A and B buses with an A/B selection or voting layer in each node; a private UART protocol per IMU/GNSS type; ARINC-style or ad hoc UDP/TCP services on the avionics Ethernet; a bespoke telemetry framing/downsampling layer for the radio with its own packet catalogue; a separate maintenance/loader protocol for software update; RPMsg or a static mailbox for `Control ↔ Platform`.

> **What structure does a conventional design use?** Six or seven unrelated message catalogues, one per transport, with the flight computers acting as translators between all of them. "Health" means a CCDL status word to a peer, a CAN status frame to an actuator, a telemetry packet field to the ground, and a maintenance-protocol response to a loader — four implementations of one concept. Redundancy is handled by duplicating the message catalogue per path and adding selection logic wherever paths converge.

WireSpaces replaces the catalogues with one identity model and keeps redundancy where it belongs. Two consequences are the substance of this sketch. **First**, `Src`/`Dest` survive transport changes, so a peer receives "FC_A's *Control* domain said this" rather than "something arrived on the A–B link" — which is what a voter actually needs. **Second**, redundancy stays a Service concern: WS gives you N parallel Wires and refuses to hide them, so the selection logic is explicit and inspectable rather than buried in a bus driver. Whether that refusal is a feature or a gap is the interesting question (§6, SF-R6-011).

---

## 3. Minimum mapping (required first)

### 3.1 Participants

ParticipantIds are allocated in **functional blocks with a reserved propulsion range**, so P4 → P8 → P16 never renumbers anything:

| ParticipantId | Endpoint Domain | Device | Notes |
|---|---|---|---|
| 0x01 | `FC_A_Control` | FC-A (2 of 4 cores, lockstep pair) | Estimation, control laws, mode, command intent |
| 0x02 | `FC_A_Platform` | FC-A (2 of 4 cores, lockstep pair) | External Links, comms, Health/platform, update, staging |
| 0x03 | `FC_B_Control` | FC-B | Peer, identical role |
| 0x04 | `FC_B_Platform` | FC-B | |
| 0x05 | `FC_C_Control` | FC-C | Peer, identical role |
| 0x06 | `FC_C_Platform` | FC-C | |
| 0x07 | `IMU_A` | IMU on FC-A serial | 100–1000 Hz state |
| 0x08 | `IMU_B` | IMU on FC-B serial | |
| 0x09 | `IMU_C` | IMU on FC-C serial | |
| 0x0A | `GNSS_A` | GNSS on FC-A serial | 5–20 Hz PVT |
| 0x0B | `GNSS_B` | GNSS on FC-B serial | |
| 0x0C | `GNSS_C` | GNSS on FC-C serial | |
| 0x0D | `CompactAvionics` | Display + SoC | Not in inner loop |
| 0x0E | `TelemetryUnit_A` | Onboard radio end | No inherent flight authority |
| 0x0F | `TelemetryUnit_B` | Second radio end (**Config B**) | |
| **0x10–0x1F** | **propulsion block (16 reserved)** | | P8 uses 0x10–0x17 |
| 0x10 / 0x11 | `Prop_FL_1` / `Prop_FL_2` | FL propulsion controllers | Dual-core lockstep → 1 Participant each |
| 0x12 / 0x13 | `Prop_FR_1` / `Prop_FR_2` | FR propulsion controllers | |
| 0x14 / 0x15 | `Prop_RL_1` / `Prop_RL_2` | RL propulsion controllers | |
| 0x16 / 0x17 | `Prop_RR_1` / `Prop_RR_2` | RR propulsion controllers | |
| 0x18–0x1F | *(reserved)* | P16 propulsion growth | Unused at P8 |
| 0x20–0x23 | `Elevon_FL` / `_FR` / `_RL` / `_RR` | Control-surface units | Lockstep → 1 Participant each |
| 0x24–0x27 | `BMS_FL` / `_FR` / `_RL` / `_RR` | Quadrant pack controllers | One Participant each |
| 0x28 | `GroundStation` | Ground PC | External but participating; authority is Service-level |
| 0x29 | `DebugPC` | Workstation (**Config C**) | No production role |

**Production totals: 26 (P4) / 30 (P8) / 38 (P16)** — matching the archetype reference. Config B adds `TelemetryUnit_B`; Config C adds `DebugPC`; `GroundStation` participates when connected. Highest value used at P16 is 0x29, comfortably inside the ordinary 8-bit range with no extended addressing (`CORE §3.1`).

**Lockstep is invisible to WS**, as required: a four-core FC yields two Participants, a dual-core lockstep propulsion or elevon controller yields one. Nothing in the mapping reveals the replica.

**Which domain owns what — derived, not chosen.** The archetype's own failure model settles the sensor-link question. It requires that losing `FC_*_Platform` leaves local `Control` still executing; if Platform owned the IMU and GNSS links, losing Platform would blind Control and that requirement would be unsatisfiable. So **`Control` owns the dedicated sensor links** (they are part of its sensing chain, dedicated 1:1, and in the inner-loop latency path), while **`Platform` owns every shared/external Link** (RS-485, CAN-FD, Ethernet). Platform remains a member of the sensor Wires so sensor Identity/Version/Health/fault/update are reachable without a proxy.

### 3.2 Wires

| Wire (#) | Scope | Member Links | Addressed Participants | Purpose |
|---|---|---|---|---|
| W1–W3 `FC_x_Local` | Device-private ×3 | `FC_x_SHM` | `FC_x_Control`, `FC_x_Platform` | Purely local control/platform state, 100 Hz–1 kHz; local health handshake |
| W4 `IMU_A_Sense` | Locality | `IMU_A_Link`, `FC_A_SHM` | 0x07, 0x01, 0x02 | IMU state + sensor Services |
| W5 `GNSS_A_Sense` | Locality | `GNSS_A_Link`, `FC_A_SHM` | 0x0A, 0x01, 0x02 | GNSS state + sensor Services |
| W6/W7 | Locality | `IMU_B_Link`/`GNSS_B_Link` + `FC_B_SHM` | 0x08/0x0B, 0x03, 0x04 | FC-B equivalents |
| W8/W9 | Locality | `IMU_C_Link`/`GNSS_C_Link` + `FC_C_SHM` | 0x09/0x0C, 0x05, 0x06 | FC-C equivalents |
| W10 `PeerAB` | Cross-channel | `FC_A_SHM`, `FC_AB_RS485`, `FC_B_SHM` | 0x01, 0x02, 0x03, 0x04 | Peer control state, Health, voting, mode, time |
| W11 `PeerBC` | Cross-channel | `FC_B_SHM`, `FC_BC_RS485`, `FC_C_SHM` | 0x03, 0x04, 0x05, 0x06 | Same |
| W12 `PeerCA` | Cross-channel | `FC_C_SHM`, `FC_CA_RS485`, `FC_A_SHM` | 0x05, 0x06, 0x01, 0x02 | Same |
| W13 `FwdActA` | Actuation path A | 3× `FC_x_SHM`, `FwdCAN_A` | 0x01–0x06, 0x10–0x13, 0x20, 0x21, 0x24, 0x25 (14) | Forward command/state/health/energy |
| W14 `FwdActB` | Actuation path B | 3× `FC_x_SHM`, `FwdCAN_B` | same 14 | Independent redundant path |
| W15 `AftActA` | Actuation path A | 3× `FC_x_SHM`, `AftCAN_A` | 0x01–0x06, 0x14–0x17, 0x22, 0x23, 0x26, 0x27 (14) | Aft command/state/health/energy |
| W16 `AftActB` | Actuation path B | 3× `FC_x_SHM`, `AftCAN_B` | same 14 | Independent redundant path |
| W17 `Avionics` | Aircraft state | `AvionicsEth`, 3× `FC_x_SHM` | 0x01–0x06, 0x0D, 0x0E (8) | Summaries, mode/warnings, pilot + operator inputs |
| W18 `Service` | Service/ground | `AvionicsEth`, `GroundRadio_A` | 0x02, 0x04, 0x06, 0x0D, 0x0E, 0x28 (6) | Identity/Health/fault/logs/config/update/telemetry; Ground Station |
| — `ServiceB` | **Config B** | `AvionicsEth`, `GroundRadio_B` | 0x02, 0x04, 0x06, 0x0D, 0x0F, 0x28 | Independent ground path |
| — `Debug` | **Config C** | `AvionicsEth` | 0x29, 0x02, 0x04, 0x06, 0x0D, 0x0E | Bulk engineering telemetry, test/config, update |

**18 configured Wires; 16 distinct WireNumbers** — W1/W2/W3 are device-private and device-scoped (`CORE §4.2`), so all three flight computers can use **the same** device-private WireNumber. That is not a trick: it means the three FCs run byte-identical configuration for their internal Wire, and only their ParticipantIds and locality Wire numbers differ.

Four decisions do the real work, and three of them are forced rather than chosen.

**(a) The RS-485 triangle dissolves into three pair Wires — no leg is wasted and no FC forwards for another.** The physical loop never becomes a WS loop because **no Wire spans two legs**. Each `Peer*` Wire is one RS-485 leg plus the shared-memory hop at each end, so its realization is a path (`Control_A — SHM_A — Platform_A — RS485 — Platform_B — SHM_B — Control_B`) and is trivially loop-free. Each FC is a member of exactly two peer Wires and publishes its state on both. The rejected alternative — one peer Wire over a spanning tree of two legs — fails the archetype's own requirement that "loss of one RS-485 leg must not necessarily isolate any flight controller," because losing the one leg that attaches a controller to the tree isolates it completely; it also makes the middle controller a forwarding dependency for the other two, which is precisely the hidden-master smell the archetype warns against. The pair-Wire mapping costs duplicate publication (each FC sends its state twice, trivial at 50–200 Hz on a 1–5 Mbit/s link) and buys **zero peer forwarding state and perfect symmetry**. It also happens to match how real cross-channel data links are actually built: point-to-point, not a shared bus.

**(b) Redundant actuator buses cannot share a Wire — this is forced, not preferred.** The actuator devices are dual-homed to their group's A and B buses, and all three FC Platforms attach to both. A single Wire spanning `FwdCAN_A` and `FwdCAN_B` therefore contains two parallel paths between the same members, which is a cycle and **invalid** under `CORE §3.3`. So forward and aft each need two Wires, one per physical bus, and A/B selection or voting becomes a Service-level behavior. The failure semantics that fall out are exactly right: losing `FwdCAN_A` takes down W13 and leaves W14 untouched, and that is visible in the Wire table rather than hidden in a driver.

**(c) Peer and actuator Wires reach into shared memory so that canonical source survives.** Command intent and control state originate in `Control`, not `Platform`. If `Platform` re-authored them (composition), the receiver would see `Src = FC_A_Platform` and could no longer distinguish "FC-A's control estimate" from "FC-A's platform opinion" — which destroys the fault-containment distinction a voter depends on. Including `FC_x_SHM` as a member Link and `Control` as a member makes `Platform` a transparent forwarder instead, preserving `Src = FC_A_Control` all the way to the actuator. This is the single most valuable thing the canonical model does in this archetype.

**(d) Low-rate aircraft state is separated from bulk service traffic on the Ethernet.** `W17 Avionics` spans shared memory (so pilot and operator inputs reach `Control` with canonical source) but carries only 10–50 Hz summaries and events. `W18 Service` stays on Ethernet and the radio, carrying Identity/Health/fault/logs/configuration/update/telemetry. Merging them would flood software-update transfers and engineering telemetry onto three shared-memory links that also carry 1 kHz control traffic — semantically valid, operationally wrong. Under base flood-and-filter this separation is the correct minimum; with destination-pruned egress the two could merge (§4.3).

**Rejected outright:** one Wire per Service (13 Service classes, explicitly discouraged); one Wire spanning every redundant Link (invalid — cycles); one Wire per redundancy path *plus* per Service (combinatorial); a single aircraft-wide Wire (would put 1 kHz IMU data on the radio); separate Participants for lockstep replicas.

**Deliberate asymmetry.** `FC_A_SHM` carries nine Wires (local, 2 sensor, 2 peer, 4 actuator, avionics); each RS-485 leg and each CAN-FD bus carries exactly one. Shared memory absorbs overlapping scopes cheaply; segregated safety Links stay one-Wire-per-Link. Same pattern archetype 10 found, more pronounced.

### 3.3 Interactions (primary)

**Flight-controller peer exchange (W10/W11/W12)** — each relation appears on both of a controller's two peer Wires.

| Interaction | Src | Dest | Rate | Notes |
|---|---|---|---|---|
| Peer flight-control state | each `FC_x_Control` | kBroadcast on each peer Wire | 50–200 Hz | Estimate, mode, control state; `Src` identifies the authoring Control domain |
| Peer Health / degraded capability | each `FC_x_Control`, `FC_x_Platform` | kBroadcast | 10–50 Hz + event | Both domains report separately |
| Voting / agreement data | `FC_x_Control` | kBroadcast | control cycle / event | Application semantics only; WS carries opaque payload |
| Mode transition coordination | `FC_x_Control` | kBroadcast | event | No network leader |
| Time / reference coordination | `FC_x_Control` | kBroadcast | periodic | Distributed agreement; algorithm out of scope |

**Local sensing (W4–W9)**

| Interaction | Src | Dest | Rate | Notes |
|---|---|---|---|---|
| IMU state | `IMU_x` | `FC_x_Control` | 100–1000 Hz | Directed; never leaves the FC island |
| IMU Health / fault | `IMU_x` | kBroadcast on its Wire | 1–10 Hz + event | Both Control and Platform consume |
| GNSS state | `GNSS_x` | `FC_x_Control` | 5–20 Hz | PVT + solution status |
| GNSS Health / solution status | `GNSS_x` | kBroadcast | 1–10 Hz + event | |
| Sensor Identity/Version, fault query, update | `FC_x_Platform` ↔ `IMU_x` / `GNSS_x` | — | startup / query | Direct Service access, no proxy |

**Actuation (W13–W16)** — every relation appears on both the A and B Wire of its group.

| Interaction | Src | Dest | Rate | Notes |
|---|---|---|---|---|
| Group command intent | each `FC_x_Control` | kBroadcast on the group Wire | 100–500 Hz | **One aggregated frame per controller per cycle** carrying all setpoints for that group (see scaling, §3.7) |
| Actuator state | each propulsion / elevon node | kBroadcast | 50–200 Hz | Position/speed/thrust/current; all three Controls consume for voting |
| Actuator Health / fault | each node | kBroadcast | 10–50 Hz + event | |
| Availability / capability | each node | kBroadcast | 10–50 Hz + event | Matters after degraded failures |
| Available power / limit | `BMS_q` | kBroadcast on its group Wire | 10–50 Hz | Quadrant-local scope: forward BMS on forward Wires only |
| Pack state, contactor / isolation, faults | `BMS_q` | kBroadcast | 5–20 Hz + event | Summarized to avionics by Platform composition |
| Identity/Version, fault query, update | `FC_x_Platform` ↔ each node | — | query / service op | Same Services as on every other Link |

Three controllers each publishing command intent is the minimum-mapping choice: it invents no authority, and actuator-side selection or voting is an application behavior. Reducing to a single application-designated publisher is a **Service-mode** change requiring no WS reconfiguration — which is the point.

**Avionics (W17)**

| Interaction | Src | Dest | Rate | Notes |
|---|---|---|---|---|
| Aircraft state summary | `FC_x_Platform` | `CompactAvionics` | 10–50 Hz | **Composed** aggregate, not forwarded raw actuator traffic |
| Mode / warning / fault summary | `FC_x_Platform` | `CompactAvionics`, `TelemetryUnit_A` | event + periodic | Aggregates BMS/actuator state under the composer's PID |
| Pilot / operator inputs | `CompactAvionics` | `FC_x_Control` | event / 10–50 Hz | Directed, canonical source preserved; not a network authority primitive |
| Avionics Identity / Version / Health | `CompactAvionics` | queried by Platforms | query | |

**Service and ground (W18)**

| Interaction | Src | Dest | Rate | Notes |
|---|---|---|---|---|
| Flight-state telemetry | `FC_x_Platform` (staged) | `GroundStation` | 10–100 Hz, rate-selected | Selected and rate-limited for the radio |
| Engineering telemetry | `FC_x_Platform` (staged) | `GroundStation` | configurable | Prefer `Debug` Wire when DebugPC is present |
| Health / fault summary | `FC_x_Platform` | `GroundStation` | 1–10 Hz + event | **Composed summary**, not 30 forwarded publications (§10, Question G) |
| Logs / events | Platforms, avionics, TU | `GroundStation` | on demand / streamed | |
| Link status | `FC_x_Platform`, `TelemetryUnit_A`, `CompactAvionics` | `GroundStation`, peers | periodic | |
| Mission / ODD configuration | `GroundStation` | flight-control configuration Service | preflight / test | Acceptance is a Service/mode decision |
| Test-mode control input, operator override | `GroundStation` | flight-control Service consumers | explicit event | Reaches `Control` via the W18→W17 splice |
| Fault / test command | `GroundStation` | designated test Services | explicit | Named Endpoints only |
| Identity / Version query | `GroundStation` | aircraft participants | on demand | |
| Update / configuration transfer | `GroundStation` | selected participants | service op | Relayed by TU forwarding; actuator targets via splice |

**Config B** adds the same Ground Station relations on `ServiceB` over the independent radio; path selection is a Service behavior. **Config C** adds DebugPC relations on `Debug` (bulk engineering telemetry, software/configuration, test-only Service calls to Platform test Endpoints) plus DebugPC membership on `W17` for test inputs that must reach `Control` — the same path pilot inputs already use.

**Relation count (P8, Config A): ~54 application relation classes** across 18 Wires, most of them Service-standard rather than bespoke.

### 3.4 Forwarding and splices

| Forwarder | Wire | Branches | Local delivery? | Notes |
|---|---|---|---|---|
| `FC_x_Control` | its 2 sensor Wires | sensor link ↔ `FC_x_SHM` | Yes (member) | 2 branches each; owns the UARTs |
| `FC_x_Platform` | its 2 peer Wires | `FC_x_SHM` ↔ RS-485 leg | Yes (member) | Transparent; `Src = FC_x_Control` preserved |
| `FC_x_Platform` | W13, W14, W15, W16 | `FC_x_SHM` ↔ CAN-FD bus | Yes (member) | Four actuator Wires per Platform |
| `FC_x_Platform` | W17 | `FC_x_SHM` ↔ `AvionicsEth` | Yes (member) | Low-rate summaries and inputs |
| `TelemetryUnit_A` | W18 | `AvionicsEth` ↔ `GroundRadio_A` | Yes (member) | Transparent; **no authority conferred** |
| `TelemetryUnit_B` | `ServiceB` | `AvionicsEth` ↔ `GroundRadio_B` | Yes | Config B |

**28 Wire-forwarder scopes** (6 sensor + 6 peer + 12 actuator + 3 avionics + 1 radio), realized as roughly **56 Wire→Link mask elements** (`CORE §11.4`) since every scope is two-branch. Config B adds one scope; Config C adds none.

**Splices (5).** Ground-originated traffic must cross from the service scope into control and actuation scopes, and extending `W18` into those scopes is impossible — adding `AvionicsEth` to an actuator Wire would put the three Platforms on two paths at once, forming a cycle. A splice is the mechanism designed for exactly this: an explicit configured Wire-scope projection that preserves canonical source, destination, and Endpoint (`DEPLOY §1.8`).

| Splice | At | Purpose | Spare |
|---|---|---|---|
| W18 → W17 | `FC_A_Platform` | Ground/operator inputs and configuration reaching `Control` domains | B, C configured inactive |
| W18 → W13 `FwdActA` | `FC_A_Platform` | Ground-originated service/update access to forward actuator nodes | B, C inactive |
| W18 → W15 `AftActA` | `FC_A_Platform` | Aft equivalent | B, C inactive |
| W18 → W14 `FwdActB` | `FC_B_Platform` | Forward path B | A, C inactive |
| W18 → W16 `AftActB` | `FC_B_Platform` | Aft path B | A, C inactive |

Exactly one splice per target Wire is **active** so ground traffic is not delivered two or three times; the other Platforms hold the same splice configured but inactive, giving an explicit reconfiguration path rather than automatic failover (no dynamic routing, per the archetype). Note carefully what these splices are and are not: they carry **service and configuration** traffic only. Flight-control commands never traverse a splice — every `Control` domain reaches every actuator directly on the actuator Wires. So the designated splice host is not a control-path dependency and confers no flight authority.

### 3.5 Link profiles

| Physical link | Profile | WS Wire(s) | Notes |
|---|---|---|---|
| `FC_A/B/C_SHM` (×3) | Shared memory / FIFO (`LINK §8`) | 9 Wires each | Simplest profile class; flow control a strong candidate |
| `IMU_x_Link` (×3) | Byte-stream / framed serial (`LINK §3`) | 1 each | Framing and CRC unresolved in `LINK` |
| `GNSS_x_Link` (×3) | Byte-stream / framed serial | 1 each | |
| `FC_AB/BC/CA_RS485` (×3) | Byte-stream / RS-485, framed datagram | 1 each | Point-to-point use; no multidrop arbitration needed |
| `AvionicsEth` | WS over embedded Ethernet (`LINK §6`) | W17, W18 (+`ServiceB`, `Debug`) | Aggregation candidate; approach undeveloped |
| `FwdCAN_A/B`, `AftCAN_A/B` (×4) | CAN-FD, 29-bit | 1 each | **Approach undecided in `LINK §1`** — see §7 |
| `GroundRadio_A` (`_B` in Config B) | Bounded packet / framed data Link | W18 (`ServiceB`) | Waveform out of scope; scarce and lossy |

**18 physical Links, 6 profile classes, no CAN11 and no VCN configuration anywhere.** Direct canonical addressing on CAN-FD/29-bit means the four actuator buses need no relation tables, no aliases, and no maps — the entire representation cost of this aircraft's actuator networking is "carry 8-bit `Src` and `Dest` in a 29-bit identifier."

### 3.6 CAN11 accounting

**N/A.** No Classical CAN11 bus exists in this archetype, and CAN11/VCN compression analysis is explicitly out of scope for it. No committed alias, no Guest block, no VCN map, no `MainA`/`MainB` assignment appears in this mapping — which is itself the relevant evidence: **the two-Main default map never had to be considered, because nothing here forces relation compression.**

**Membership vs presence.** All 30 production Participants are **preconfigured** on their Wires and remain members when unreachable — a failed `Prop_FL_2` or a powered-down `GNSS_B` keeps its membership and simply stops sourcing; delivery to it fails or queues drain (`brief §6.4`). This matters more here than in earlier sketches because *degraded operation is the normal case*: the mapping deliberately never removes membership to represent a failure, so a controller's view of "who is present" comes from Health Services and delivery status, never from a configuration change. `GroundStation` is also preconfigured and simply unreachable when the radio is down. The two genuinely configuration-scoped participants are `TelemetryUnit_B` (present only in Config B) and `DebugPC` (present only in Config C).

### 3.7 Scaling check — P4 / P8 / P16

The logical architecture is **identical** across all three variants. Nothing is redesigned; repeated hardware joins existing Wires.

| Quantity | P4 | P8 | P16 | Growth |
|---|---|---|---|---|
| Production Participants | 26 | 30 | 38 | +1 per controller (linear) |
| Propulsion Participants | 4 | 8 | 16 | linear |
| Wires | 18 | 18 | 18 | **constant** |
| WireNumbers | 16 | 16 | 16 | **constant** |
| Physical Links | 18 | 18 | 18 | **constant** |
| Wire-forwarder scopes | 28 | 28 | 28 | **constant** |
| Splices | 5 | 5 | 5 | **constant** |
| Wire memberships | 2 per prop | 2 per prop | 2 per prop | linear |
| ParticipantId renumbering | none | none | none | reserved 0x10–0x1F block |
| CAN relation entries | 0 | 0 | 0 | 29-bit direct addressing |

**Config delta per added propulsion controller:** one ParticipantId from the reserved block, membership in its group's A and B Wires, and its Endpoint/Service set. That is it — no new Wire, no new Link, no new forwarding scope, no new splice, no addressing change. Growth is **systematic and linear**, which is the archetype's stated good result.

**The real P16 constraint is bandwidth, not architecture**, and the mapping shape matters here. Command intent is published as **one aggregated broadcast frame per controller per cycle** carrying all setpoints for that group, not one frame per actuator — CAN-FD's 64-byte payload holds eight propulsion setpoints comfortably. So command traffic is ~3 × 500 Hz ≈ 1500 frames/s **regardless of P4/P8/P16**. Only state feedback scales with node count: at P16 the forward group is 12 nodes × 50–200 Hz ≈ 600–2400 frames/s. Total stays in the low thousands of frames per second against a CAN-FD 2–5 Mbit/s bus, with fault-burst and service headroom. Had commands been modelled per-actuator instead of per-group, P16 would have tripled command load for no benefit — a modelling choice, not a protocol limit, but worth recording as the one place where the mapping had to think about scale.

### 3.8 Configuration inventory (auditable)

```text
Production Participants:           26 (P4) / 30 (P8) / 38 (P16)
  flight-control domains           6      (3 devices x 2 usable domains)
  IMU / GNSS                       3 / 3
  propulsion                       4 / 8 / 16   (reserved block 0x10-0x1F)
  elevon / BMS                     4 / 4
  CompactAvionics / TelemetryUnit  1 / 1        (+1 TU in Config B)
  external                         GroundStation (+DebugPC in Config C)
  lockstep replicas exposed        0

Physical Links:                    18
  shared memory / IPC              3
  local serial (IMU/GNSS)          6
  RS-485 peer legs                 3
  embedded Ethernet                1
  CAN-FD 29-bit                    4
  wireless ground radio            1            (+1 in Config B)

Canonical Wires:                   18 configured / 16 distinct WireNumbers
  device-private                   3 configured, 1 WireNumber (reused per FC)
  sensor locality                  6            one per sensor Link + its SHM
  flight-control peer              3            one per RS-485 leg
  actuator                         4            one per CAN-FD bus (A/B forward, A/B aft)
  avionics / service               2
  Config B / C additions           +1 / +1

Device-private Wires:
  FC SHM                           yes - purely local control/platform state only
  sensor locality                  NOT device-private (crosses a device boundary
                                   to the IMU/GNSS Participant)

Flight-control peer Wires:
  physical loop representation     three pair Wires, one per leg; NO Wire spans
                                   two legs, so the triangle is never a WS cycle
  loop-free maintained by          construction (each Wire realization is a path)
  legs unused                      none - all three carry traffic
  peer forwarding state            zero (no FC forwards for another FC)
  cost                             duplicate publication (each FC publishes state
                                   on both of its peer Wires)

Actuator Wires:
  A/B redundancy representation    separate Wire per physical bus - FORCED, since
                                   one Wire over dual-homed A+B buses is a cycle
  forward vs aft scope             separate; quadrant BMS scoped to its own group
  redundant Links share a Wire?    no - impossible under CORE 3.3
  selection / voting               Service level, application-defined

Avionics / telemetry:
  Wires reaching Ground Station    W18 Service (+ServiceB in Config B) only
  excluded from radio by design    raw IMU/GNSS samples, peer CCDL traffic,
                                   raw actuator command/state, local FC state,
                                   per-participant periodic Health (composed
                                   into one summary instead)

Gateway / forwarding state:
  Wire-forwarder scopes            28   (+1 Config B)
  Wire->Link mask elements         ~56
  splices                          5    (1 active per target Wire; spares inactive)
  composition points               ~9   (3 Platforms x {state summary, warning
                                   summary, ground health/telemetry summary})

Standard Services:
  ubiquitous (all production)      Identity/Version, Health, Fault/diagnostic,
                                   diagnostic RPC
  broad but capability-gated       firmware update, logs/events, configuration
  capability-specific              Link status (5), Telemetry (selected + staged),
                                   time sync (FC/sensors/actuators/TU),
                                   flight mode (FC + avionics),
                                   energy state (BMS + consumers),
                                   actuator state (actuators + FC)
  extra Wires required             0

Scaling P4 -> P8 -> P16:
  new Wires / Links / splices      0 / 0 / 0
  new forwarding scopes            0
  per-controller delta             1 PID + 2 Wire memberships + Endpoint set
  renumbering                      none
```

Roughly **125 counted configuration objects** at P8, excluding per-Service Endpoint definitions. That is heavier than archetype 10's ~100 for 1.5× the Participants and 1.8× the Links, and the excess is concentrated in exactly one place: **redundancy**. Twelve of the 28 forwarding scopes exist because there are four actuator buses instead of two, and six exist because there are three peer legs instead of one shared bus.

---

## 4. Optional optimizations

### 4.1 One peer Wire over a spanning tree

Use two RS-485 legs as a single peer Wire and hold the third as a spare. **Problem it would claim to solve:** three peer Wires become one, and each FC publishes state once instead of twice. **Rejected:** it violates the archetype's failure requirement (losing the leg that attaches a controller to the tree isolates that controller, whereas the pair mapping leaves it reachable on its other leg), it makes the middle controller forward for the outer two, and it wastes a physical leg. The saving — three Wire numbers and some duplicate publication on a link with ample headroom — is not worth a forwarding dependency in a triplex flight-control path.

### 4.2 Merge forward and aft actuator scopes

Two actuator Wires (`ActA`, `ActB`) each spanning both the forward and aft bus of its letter. **Problem it would solve:** 4 actuator Wires → 2, and 12 forwarding scopes → 6. **Rejected:** the forward and aft buses are physically separate segments, so a single Wire spanning both is legal only as a tree through the Platforms — but the Platforms are on all four buses, so any such Wire is a cycle. Even if it were legal, it would flood aft state onto the forward bus and double both buses' load for no benefit. Forward/aft separation is also a real failure boundary worth keeping visible.

### 4.3 Merge `Avionics` and `Service` under destination-pruned egress

One Wire spanning `AvionicsEth`, `GroundRadio_A`, and the three SHM links, with participant-location egress pruning at the Platforms and the telemetry unit. **Problem it would solve:** one fewer Wire, one fewer splice (ground inputs would reach `Control` directly with no W18→W17 projection). **Deferred, not rejected:** it is strictly better *if* pruning is available — and pruning is required for the radio anyway (§10, Question G). Under base flood-and-filter it would push software-update transfers and engineering telemetry onto shared-memory links carrying 1 kHz control traffic, so the minimum mapping keeps them separate. This is the cleanest single simplification available to this sketch, and it hinges entirely on whether pruning is treated as a normal deployment capability (SF-R6-013).

### 4.4 Consolidate the six sensor Wires into three

One `Sense_x` Wire per FC covering both its IMU and GNSS links. **Problem it would solve:** six Wires → three. **Rejected under base semantics:** the two sensor links would be branches of one Wire, so flood-and-filter would forward 100–1000 Hz IMU state onto the GNSS serial link. With destination-pruned egress the consolidation is safe and preferable; without it, six Wires is the correct minimum. Six also happens to make sensor failure boundaries individually visible.

### 4.5 Platform-mediated update instead of service splices

Drop the four actuator splices; ground-originated update and diagnostics target the Platform's update Service, which then acts on the actuator under its own ParticipantId. **Problem it would solve:** four splices and their active/spare designation disappear. **Rejected for the minimum mapping:** the actuator would see `Src = FC_A_Platform` rather than `Src = GroundStation`, losing the requester identity at the point of action — undesirable for flight-test traceability, and it re-creates the conventional "loader proxy" the canonical model is supposed to make unnecessary. Worth revisiting if splice configuration proves harder to audit than a proxy Service.

### 4.6 Single designated command publisher

Have one application-selected `Control` publish command intent while the other two stand by. **Problem it would solve:** actuator bus command load drops from 3× to 1×. **Not needed at P16** given aggregated group commands (§3.7), and available at any time as a **Service-mode** decision with **zero WS reconfiguration** — no Wire, membership, or addressing change. Recorded because that property is the strongest single piece of evidence for Question A: the network genuinely does not know which controller is active.

### 4.7 Peer exchange over Ethernet as a fallback

All three Platforms are already on `AvionicsEth`, so peer state could also traverse `W17`. **Rejected for the minimum mapping:** it would mix flight-critical cross-channel data with avionics and display traffic on a single-switch network, and the RS-485 legs exist precisely to avoid that. Available as an application-level degraded path if two legs are lost, at the cost of adding peer relations to `W17`.

---

## 5. Friction signals (minimum mapping happy path)

| Signal | None / Mild / Significant | One-sentence justification |
|---|---|---|
| Artificial Wire | None | Every one of the 18 Wires is a real propagation domain — a shared-memory island, a sensor link, an RS-485 leg, a CAN-FD bus, the Ethernet, or the radio path; none was created to satisfy direction or Service semantics. |
| Wire proliferation | **Significant** | Eighteen Wires for 30 Participants, of which 13 exist because the loop-free invariant forbids merging physically redundant or segregated paths (6 sensor links, 3 peer legs, 4 A/B buses) — high in *count* while remaining zero in *artificiality*, which is the distinction reviewers should weigh (SF-R6-011). |
| Artificial hierarchy | None | No Origin, no `MainA`/`MainB`, no canonical primary; the three controllers are structurally indistinguishable in the configuration, and the one designation the mapping does make (active splice host) carries service traffic only and no control authority. |
| VCN pressure | N/A | No CAN11; CAN-FD 29-bit needs no relation table. |
| WireAlias pressure | N/A | No CAN11. |
| Configuration burden | **Significant** | ~125 counted objects, with 28 forwarding scopes and 5 splices — mechanically generatable and layered for audit, but redundancy multiplies the forwarding inventory roughly 2.4× over archetype 10 for 1.5× the Participants. |
| Failure/topology mismatch | None | Every documented failure is a Wire-realization partition or a silent member, readable from the Wire table without tracing forwarding (§10, Question I). |

**Free-form notes.** Two things were genuinely awkward. First, **redundancy has no name in the model**: I re-derived "N parallel Wires plus Service-level selection" from the loop-free invariant, and nothing in the governed docs told me that is the intended pattern — a second mapper could plausibly try one Wire over redundant links, get a validation error, and conclude WS cannot express redundancy. Second, **duplicate delivery is left entirely undefined**: an actuator that is a member of both its group's A and B Wires receives every command twice, and the model says nothing about whether Endpoint storage semantics are expected to absorb that or whether the application must deduplicate (SF-R6-012). Both are documentation gaps rather than protocol defects — the mapping works — but both cost real thinking time that a named pattern would have saved.

Worth recording as a positive: nothing about the multicore structure was awkward at all. `Control`/`Platform` as two Participants on a shared-memory Wire behaved exactly like two ECUs on a cable, and the fact that `Platform` forwards for `Control` transparently is what made canonical source survive to the actuator.

---

## 6. Model pressure

- **If you could change one WS concept** to simplify this system, what would it be? Not a protocol change — a **named redundancy deployment pattern**. The mapping needed three independent derivations of the same insight (peer legs, A/B actuator buses, dual radios all become parallel Wires with Service-level selection, because the loop-free invariant admits nothing else). That is arguably the correct architectural answer, and I would not change the invariant: collapsing redundant paths into one Wire would hide exactly the failure behavior a flight-control engineer must see. But it deserves to be written down as a pattern with its consequences — Wire count tracks path count, duplicate delivery is normal, selection is a Service responsibility — rather than rediscovered per sketch.
- **Did WS expose a useful distinction** the conventional model obscures? Yes, and one of them is load-bearing for safety. **(a) Canonical source across transports:** a voter receives "FC-A's *Control* domain authored this," preserved across a shared-memory hop and a CAN-FD bus, where the conventional CCDL/CAN design gives you "a frame arrived on link A-B" and reconstructs provenance by convention. **(b) Forwarding vs composition, sharply:** peer state and actuator commands are *forwarded* (identity preserved, because provenance is safety-relevant), while avionics summaries and the ground health digest are *composed* (new identity, because they are new information authored by the Platform) — the conventional design blurs both into "the FC sends a message." **(c) Reachability vs authority:** the Ground Station is an ordinary Participant with ordinary relations, and nothing in the network decides whether it may currently influence flight; the telemetry unit forwards its traffic while holding no authority whatsoever. That separation is stated once in the model instead of re-implemented in every uplink handler.

**Synthesis note.** The corpus now has a clean spread on the redundancy axis. Archetypes 01/05/10 are hierarchical and CAN11-friendly; 07/09 are dense peer graphs that pressure CAN11 relation capacity; 08 and 11 are the two where CAN11 never enters the question at all. What 11 adds that nothing else does is **redundancy as a first-class topology property** — and the finding is that R6's canonical model absorbs it without strain while the *Wire* abstraction pays for it in count. Notably, 11 also produces zero pressure on the addressing model: 38 Participants at P16 sit inside the ordinary 8-bit range with a reserved growth block, and the four CAN-FD buses need no compression machinery.

---

## 7. Open questions

- **CAN-FD profile is undefined.** `LINK §1` lists CAN FD/XL as "approach undecided." This mapping assumes direct canonical addressing of 8-bit `Src`/`Dest` in a 29-bit identifier with no relation table, and it assumes the 64-byte payload is available for aggregated group commands (which is what makes P16 scale in payload rather than frame count). Both assumptions are central to §3.7 and should be revisited when the profile exists.
- **RS-485 profile for point-to-point datagram use.** `LINK §3` leaves byte-stream framing and CRC unresolved. Each peer leg is used point-to-point, so no multidrop arbitration or polling cadence is needed — a simpler case than the RS-485 sensor bus in archetype 05, and possibly worth noting as the easier profile target.
- **Wireless Link profile.** The radio is treated as a bounded packet Link. What a Link profile should say about an intermittent, high-latency, lossy carrier — particularly whether Link status and queue-drain semantics differ from a wired Link — is unaddressed and matters for how "unreachable Ground Station" is reported.
- **Duplicate delivery across redundant Wires.** Filed as SF-R6-012. The specific unresolved question is whether single-slot Endpoint storage overwrite semantics are the intended absorber, or whether A/B deduplication belongs in every application.
- **Splice granularity.** `DEPLOY §1.8` describes a splice as a Wire-scope projection but does not say whether it can be scoped per Endpoint or per destination. This mapping needs five splices and designates one active host per target Wire to avoid duplicate delivery; finer granularity would let ground service traffic reach actuators without an active/spare designation at all.
- **Nine Wires on one shared-memory Link.** `FC_A_SHM` carries nine overlapping Wires. Legal (`WIRE-9`) and cheap, but whether that is idiomatic or a sign the SHM boundary is being asked to do too much is worth a convergence mapper's second opinion.
- **Time reference with no master.** Peer time coordination is distributed agreement, but publishing time onto each actuator group still requires the application to designate a publisher per cycle. That designation is correctly a Service concern; whether the standard time Service should define it, or leave it open, is unresolved and is the one place where "no network master" pushes a decision upward.

---

## 8. Spec findings

| ID | Finding |
|---|---|
| SF-R6-011 | **Redundant physical paths cannot share a canonical Wire, and R6 has no named pattern for that.** One Wire spanning `FwdCAN_A` + `FwdCAN_B` is a cycle under `CORE §3.3` because the actuators are dual-homed, so redundancy is necessarily expressed as N parallel Wires with selection at the Service level. This is very likely the *right* answer — it keeps failure behavior visible — but it is currently derivable only from the loop-free invariant. Three separate structures in this archetype (peer legs, A/B actuator buses, dual radios) required the same derivation. Recommend documenting "redundant path ⇒ one Wire per path + Service-level selection" as a named deployment pattern with its consequences, including Wire-count growth and expected duplicate delivery. |
| SF-R6-012 | **No stated position on duplicate delivery from redundant Wires.** A Participant that is a member of two Wires carrying the same Service relation (every actuator here) receives every command twice and publishes every state twice. Nothing in `CORE` says whether single-slot Endpoint storage overwrite is the intended absorber, whether duplicate suppression is an application responsibility, or how a Service should express "these two Wires are redundant paths for one relation." Each mapper will invent an answer. |
| SF-R6-013 | **Destination-pruned egress is load-bearing, not an optimization, when one Wire spans links differing by orders of magnitude in capacity.** `CORE §12` describes base behavior as flood-and-filter and destination pruning as "an optimization, not a change to Wire semantics." A Wire spanning 100 Mbit/s Ethernet and a 100 kbit/s intermittent radio (`W18`) is unusable under base semantics — as is any consolidation of the sensor Wires (§4.4) or the merge in §4.3. Pruning here is a viability requirement, and its optional framing pushed this sketch toward more Wires than the topology needs. Recommend stating when pruning is expected rather than optional. |

**Evidence note (not a finding).** This archetype confirms that CAN11 relation compression is a CAN11 artifact and not a property of the canonical model — the same result archetype 08 produced on Ethernet, now reproduced on CAN-FD with a far denser and more critical interaction graph. Fourteen-member actuator Wires with dual-homed redundancy needed no relation table at all.

---

## 9. Diagrams

### Wire-centric (P8, Config A)

```text
device-private (never forwarded, one shared device-private WireNumber)
  W1/W2/W3  FC_x_Local      [FC_x_SHM]           Ctrl_x, Plat_x
                                                 purely local state 100 Hz - 1 kHz

sensor locality (6 Wires - one per sensor Link)
  W4  IMU_A_Sense   [IMU_A_Link  + FC_A_SHM]     0x07 IMU_A,  Ctrl_A, Plat_A
  W5  GNSS_A_Sense  [GNSS_A_Link + FC_A_SHM]     0x0A GNSS_A, Ctrl_A, Plat_A
  W6/W7  ... FC-B          W8/W9  ... FC-C

flight-control peer / CCDL (3 Wires - one per RS-485 leg, NO Wire spans two legs)
  W10 PeerAB  [FC_A_SHM + FC_AB_RS485 + FC_B_SHM]   Ctrl_A Plat_A Ctrl_B Plat_B
  W11 PeerBC  [FC_B_SHM + FC_BC_RS485 + FC_C_SHM]   Ctrl_B Plat_B Ctrl_C Plat_C
  W12 PeerCA  [FC_C_SHM + FC_CA_RS485 + FC_A_SHM]   Ctrl_C Plat_C Ctrl_A Plat_A
        each FC publishes its state on BOTH of its peer Wires
        zero peer forwarding; physical triangle is never a WS cycle

actuation (4 Wires - one per CAN-FD bus; merging A+B would be a cycle)
  W13 FwdActA [3x FC_x_SHM + FwdCAN_A]   6 FC domains + Prop_FL/FR(4) + Elevon_FL/FR + BMS_FL/FR
  W14 FwdActB [3x FC_x_SHM + FwdCAN_B]   same 14 members, independent path
  W15 AftActA [3x FC_x_SHM + AftCAN_A]   6 FC domains + Prop_RL/RR(4) + Elevon_RL/RR + BMS_RL/RR
  W16 AftActB [3x FC_x_SHM + AftCAN_B]   same 14 members, independent path
        command intent: Src = FC_x_Control preserved through Platform + CAN-FD

avionics / service
  W17 Avionics [AvionicsEth + 3x FC_x_SHM]  6 FC domains + CompactAvionics + TU_A
                                            summaries (composed) + pilot/operator input
  W18 Service  [AvionicsEth + GroundRadio_A] 3 Platforms + CompactAvionics + TU_A + GroundStation
                                            Identity/Health/fault/logs/config/update/telemetry

Config B: +ServiceB [AvionicsEth + GroundRadio_B] with TU_B - a second Wire, not a
          second path on W18 (that would be a cycle)
Config C: +Debug [AvionicsEth] with DebugPC, plus DebugPC on W17 for test inputs
```

### Why redundancy cannot collapse into one Wire

```text
INVALID - one Wire over both forward buses:

        Plat_A ----+---- FwdCAN_A ----+---- Prop_FL_1
                   |                  |
                   +---- FwdCAN_B ----+
                   two parallel paths between the same members  =>  cycle
                   rejected by CORE 3.3 / loop validation

VALID - one Wire per bus, selection in Services:

        W13 FwdActA:  Plat_A --- FwdCAN_A --- Prop_FL_1     (tree)
        W14 FwdActB:  Plat_A --- FwdCAN_B --- Prop_FL_1     (tree)

        same pattern applies to:
            3 RS-485 peer legs      -> 3 pair Wires
            2 ground radios         -> 2 service Wires
```

### Failure boundaries read off the Wire table

```text
one FC lost (say FC-B)
    W2 gone; W10 PeerAB and W11 PeerBC lose their B end; W13-W17 lose the
    FC_B_SHM branch and Plat_B stops forwarding
    -> A and C still peer directly on W12 PeerCA; both still command all four
       actuator Wires; no address change, no re-election                  [2 FCs continue]

one FC_x_Control lost
    Ctrl_x silent on its sensor, peer, actuator and avionics Wires
    -> Plat_x still reachable and diagnosable on W18; that channel supplies
       no control computation                                             [as specified]

one FC_x_Platform lost
    Ctrl_x keeps its sensor Wires (Control owns those Links) and keeps running
    -> loses peer, actuator, avionics reach; a blind but live channel      [as specified]

one RS-485 leg lost (say A-B)
    W10 PeerAB down only
    -> A still peers with C on W12, B still peers with C on W11
    -> NO controller isolated; application sees a missing direct peer      [requirement met]

one actuator CAN lost (say FwdCAN_A)
    W13 down; W14 FwdActB carries the same 14 members
    -> forward actuators fully reachable; aft untouched                    [requirement met]

both buses of one group lost
    W13 + W14 down -> forward actuator group unreachable; aft unaffected

one quadrant propulsion group lost
    those 2 (P8) Participants silent on W13/W14; Wires and all other
    members unaffected; capability loss reported via availability Service

one BMS lost (say BMS_RL)
    silent on W15/W16 only; other quadrants still report power limits

ground link lost
    W18 partitions at TU_A -> GroundStation unreachable, onboard unaffected
    Config B: ServiceB still available over the independent radio

display lost
    CompactAvionics silent on W17/W18; inner loop untouched               [requirement met]

DebugPC absent
    Debug Wire not in the production configuration at all
```

### Identity layering (what replaced what)

```text
conventional                          WireSpaces
------------                          ----------
CCDL frame format per leg        -->  W10/W11/W12 + Src = FC_x_Control
CAN-FD ID matrix, duplicated A/B -->  W13..W16, direct 29-bit Src/Dest, no matrix
private IMU/GNSS UART protocol   -->  W4..W9 + standard sensor Services
avionics UDP/TCP services        -->  W17 (state) + W18 (service)
telemetry packet catalogue       -->  composed summaries on W18, rate-selected
maintenance/loader protocol      -->  ordinary update Service + 4 splices
RPMsg / mailbox                  -->  device-private Wire
A/B selection in the bus driver  -->  explicit: two Wires, Service-level choice
```

---

## 10. Archetype questions A–I

**A — R6 symmetric-controller fit.** Strong pass, and the archetype's four prohibitions are all satisfied structurally rather than by convention. No canonical Origin appears. No `MainA`/`MainB` semantics appear — impossible here, since nothing forces relation compression. No network-level controller authority exists: the three FCs have structurally identical configurations, each a member of two peer Wires and all four actuator Wires, and an inspector cannot tell from the configuration which one is "in charge" because none is. And **no address changes on failure** — a failed controller stops sourcing traffic and its peers learn that from Health Services and delivery status, never from a configuration change. The clinching evidence is §4.6: moving between "all three publish command intent" and "one application-designated publisher" is a Service-mode decision with *zero* WS reconfiguration. The network genuinely does not know which controller is active.

**B — multicore continuity.** Strong pass, and the most pleasant part of the mapping. `FC_Control ↔ FC_Platform` over shared memory is an ordinary Wire over an ordinary Link (`LINK §8`), indistinguishable in the Service model from two ECUs on a cable. The important consequence is directional: because `Platform` *forwards* rather than *re-authors*, a command leaving `Control`, crossing shared memory, and arriving at an actuator over CAN-FD still carries `Src = FC_A_Control`. Concretely, one Health Service implementation serves `IMU_A` over local serial, `FC_B_Control` over RS-485, `Prop_FL_1` over CAN-FD, `CompactAvionics` over Ethernet, and `GroundStation` over the radio — five transports, one Service. Transport differences stayed entirely below the Service, which is exactly the claim being tested.

**C — physical-loop pressure.** Resolved cleanly, and the archetype's first listed outcome is the right one: **three pair/locality Wires are clearer than one flooded peer Wire.** The key move is that the triangle is not "broken" — all three legs carry traffic — because *no Wire spans two legs*, so no Wire realization contains the cycle. Each peer Wire is a path and is loop-free by construction, with **zero** peer forwarding state. The rejected spanning-tree alternative fails the archetype's own non-isolation requirement and introduces a forwarding dependency between flight-critical channels. The cost is honest and small: each controller publishes its state twice, on both of its legs. Worth noting that this is also what real cross-channel data links look like in practice — point-to-point, not a shared bus — so the loop-free invariant pushed the mapping *toward* conventional safety practice rather than away from it.

**D — redundant CAN representation.** **Separate Wires, and the choice is forced rather than preferred.** Because the actuators are dual-homed and all three Platforms attach to both buses, one Wire spanning `FwdCAN_A` and `FwdCAN_B` contains parallel paths between the same members and is rejected as a cycle (`CORE §3.3`). So the answer among the archetype's three candidates is "separate Wires" for the topology plus "endpoint/service-level redundancy" for the behavior — they are not alternatives but two halves of the same answer. This is the *good* outcome for failure visibility: losing `FwdCAN_A` takes down exactly one Wire and the redundancy is legible in the Wire table. It is the *costly* outcome for configuration: four actuator Wires and twelve forwarding scopes where the physical system has two actuator groups. And it generalizes — the dual radios in Config B hit the identical constraint. Filed as SF-R6-011 with the recommendation that this become a named pattern.

**E — repeated-node scaling.** Systematically linear, which is the archetype's stated good result. Across P4 → P8 → P16: Wires, WireNumbers, Links, forwarding scopes, and splices are all **constant**; only Participants and Wire memberships grow, at exactly one ParticipantId plus two memberships per added controller. The reserved `0x10–0x1F` propulsion block means no renumbering at any variant, and 29-bit direct addressing means zero relation-table growth. The one place scale required thought is the *modelling* of commands: publishing one aggregated group command per controller per cycle (CAN-FD's 64-byte payload holds eight setpoints) keeps command frame rate flat at ~1500 frames/s across all variants, so only state feedback scales with node count. Per-actuator command frames would have tripled P16 command load — a modelling choice, not a protocol limit, but the difference between linear and unpleasant.

**F — remote authority.** Clean pass with no special mechanism. `GroundStation` is ParticipantId 0x28, an ordinary member of `W18`, addressing named configuration and test Endpoints with `Src = GroundStation` preserved end to end (including through the W18→W17 splice into the `Control` domains). The network carries the traffic and **decides nothing**: whether a mission/ODD update, test-mode input, or operator override is currently permitted is resolved by the receiving Service against the aircraft's flight/test mode, at the Endpoint. `TelemetryUnit_A` forwards Ground Station traffic transparently and holds no authority whatsoever — it is a forwarder on one Wire, and nothing in its configuration distinguishes ground-originated PDUs from any other. Reachability and authorization are separated once, in the model, instead of being re-implemented in every uplink handler.

**G — wireless scarcity.** The radio is the sharpest constraint in the archetype, and it pressures **Service usage and rate choices** rather than addressing — exactly as predicted. Two findings. First, ubiquitous periodic Services do not survive contact with a 100 kbit/s intermittent link: 30 Participants each publishing Health at 1 Hz is 30 messages/s of pure overhead before any telemetry, so the mapping has each `FC_x_Platform` **compose** an aircraft health/fault summary for the ground while individual Health remains directly queryable on demand. Composition is doing real work here, and the distinction from forwarding is what makes it legitimate — the summary is new information authored by the Platform, not a relabelled copy. Second, and less comfortable: `W18` spans Ethernet and the radio, so under base flood-and-filter every onboard service PDU would be pushed at the radio. Destination-pruned egress at `TelemetryUnit_A` is therefore not an optimization but a **precondition for the Wire to be viable at all** (SF-R6-013), alongside per-Endpoint rate and selection configuration. Raw IMU/GNSS samples, peer CCDL traffic, raw actuator command/state, and local FC state are excluded from the radio by Wire construction rather than by filtering — they are on Wires the radio is not a member of, which is the more robust exclusion.

**H — Service ecosystem density.** Strong pass: **13 Service classes across up to 38 Participants required zero additional Wires**, because Service identity is the Endpoint while the Wire only bounds propagation. The span the archetype asks about is genuinely covered — a tiny lockstep actuator MCU, an IMU on a UART, a quadrant BMS, a four-core flight-control SoC, a telemetry unit, a display SoC, and a Ground Station PC all expose Identity/Version, Health, and fault status through the same canonical form, differing only in capability. Capability gating is the honest part: firmware update, logs, and configuration are broad but not universal; Link status belongs to the five Link-owning domains; telemetry is selected and staged rather than universal. Nothing required per-Link Service variants, and no device class needed bespoke integration. The one real tension is Question G's: uniform Service *availability* is achievable, uniform Service *publication rates* are not, and standard Services would benefit from capability/rate profiles rather than fixed periodic assumptions.

**I — failure auditability.** Pass. Every failure the archetype lists is either a **partition of one Wire's realization** or a **silent member**, both readable directly from the Wires table without tracing forwarding logic (see the diagram in §9). The two most demanding cases resolve without ambiguity: losing one RS-485 leg takes down exactly one peer Wire and isolates nobody, because each controller has a second leg; and losing `FC_x_Platform` leaves `Ctrl_x` holding its sensor Wires — because `Control` owns the sensor Links — producing precisely the "live but blind channel" the archetype specifies. What makes this work is that the Wire decomposition tracks physical paths, which is the same property that makes Wire count high. That trade is the central result of this sketch: **the decomposition that costs the most configuration is the one that makes redundancy legible**, and for a flight-control system that is the right side of the trade.

---

## Devices (reference)

| Device | Endpoint Domains | Link interfaces | Role summary |
|---|---|---|---|
| FC-A / FC-B / FC-C | `FC_x_Control`, `FC_x_Platform` (2 usable of 4 cores) | `FC_x_SHM`, `IMU_x_Link`, `GNSS_x_Link` (Control); 2 RS-485 legs, 4 CAN-FD, `AvionicsEth` (Platform) | 0x01–0x06; symmetric peers; Platform forwards 7 Wires |
| `IMU_A/B/C` | one each | one local serial | 0x07–0x09; 100–1000 Hz state, dedicated 1:1 |
| `GNSS_A/B/C` | one each | one local serial | 0x0A–0x0C; 5–20 Hz PVT, dedicated 1:1 |
| `CompactAvionics` | one | `AvionicsEth` | 0x0D; display and operator input; not in inner loop |
| `TelemetryUnit_A` (`_B`) | one each | `AvionicsEth`, `GroundRadio_x` | 0x0E (0x0F); transparent forwarder, **no authority** |
| Propulsion controllers ×4/8/16 | one each (dual-core lockstep) | its group's CAN-FD A and B | 0x10–0x1F block; dual-homed leaves |
| `Elevon_FL/FR/RL/RR` | one each (lockstep) | its group's CAN-FD A and B | 0x20–0x23; dual-homed |
| `BMS_FL/FR/RL/RR` | one each | its group's CAN-FD A and B | 0x24–0x27; quadrant-scoped energy state |
| Ground Station | one | `GroundRadio_A` (`_B`) | 0x28; ordinary Participant; authority is Service-level |
| DebugPC | one | `AvionicsEth` | 0x29; Config C only; no production role |

# Sketch — Archetype 10: Automotive Zone + Central Compute Slice

```text
**Archetype:** archetypes/10_automotive_zone_core_slice.md
**Agent:** cursor_C
**Output file:** sketches/10_automotive_zone_core_slice_cursor_C.md
**Date:** 2026-08-23
```

---

## Executive summary

```text
**Minimum mapping:** 20 production Participants, 8 Wires (3 device-private + 5 network-visible), 3 forwarding domains (~23 mask entries), BodyCAN11 = 1 committed alias with the DEFAULT VCN map (11 / 32, MainA+MainB both assigned), LegacyAuxCAN = Guest-3 (4 / 7), PowerFD = direct 29-bit addressing.
**Question A (R6):** Natural — scales to ~20 Participants without one-Wire-per-interaction or one giant Wire.
**Question B (CAN11):** Default — positive control passes; first sketch in the corpus to exercise MainB.
**Worst friction (minimum path):** Configuration burden — Moderate (~100 counted entries, auditable).
**Main lesson:** Wires shaped by locality/authority (not by cable or by Service) keep ~9 standard Services and ~48 relations on 8 Wires; the two-Main default fits because the service tool reaches ECUs through DiagOTA instead of becoming a third Main.
```

---

## Disposition block

| Area | Assessment |
|---|---|
| Participant identity | Natural |
| Wire decomposition | Natural |
| Forwarding | Moderate |
| Non-CAN configuration | Moderate |
| CAN11 VCN fit | Default |
| Better with CAN29? | No (BodyCAN11); PowerFD already CAN-FD/29-bit |

**Explanation (Question A — R6):** Twenty Endpoint Domains across three multi-domain devices and three fieldbuses map to twenty deployment-global ParticipantIds with no nominated Origin. The decomposition that works is **locality + authority scope**, not cable count: three device-private Wires absorb the 100 Hz–1 kHz internal traffic, one Wire carries vehicle↔zone control, one carries telematics, and one carries each fieldbus scope spanning shared memory → backbone → bus. Transparent forwarding at `CoreNet` / `ZoneNet` / `TcuNet` preserves canonical `Src`/`Dest`, so `ZoneControl` addresses a body ECU directly even though `ZoneNet` owns the transceiver (`CORE §12`). Standard Services add **zero** Wires — they add Endpoints (§10, Question E).

**Explanation (Question B — CAN11):** `BodyCAN11` is a genuine positive control. The unified default map fits unmodified with `MainA = ZoneControl` (local control authority) and `MainB = DiagOTA` (diagnostic authority) — exactly the "gateway + service tool" pairing overlay §5 anticipates, and **not** an invented hierarchy: both authorities exist in the archetype. Eleven of 32 ordinary VCNs, five of 14 Node positions, one alias of eight, no custom entries. `LegacyAuxCAN` needs only **Guest-3** (4 of 7 ordinary VCNs) for its two Guest nodes. `PowerFD` needs no relation table at all. **This is the first sketch in the corpus that assigns MainB** and therefore closes the evidence gap recorded as SF-R6-004.

**Archetype Questions C–G are answered in §10.**

---

## 1. Native communication model

A vehicle slice with three intelligent devices and three fieldbuses.

**Central compute SoC** hosts four domains on a shared-memory/IPC link (`CoreSHM`): `CoreNet` (owns backbone Ethernet, forwards, reports Link status), `VehicleControl` (vehicle mode/intent, zone command/state, system time source for the slice), `DiagOTA` (diagnostic aggregation, software inventory, update orchestration, service-tool entry point), and `Telemetry` (collects selected high-rate engineering data; no control authority).

**Telematics ECU** hosts `TcuNet` (network-facing) and `CloudServices` (cloud upload, update package handoff, remote diagnostic session support; no actuator authority) over `TcuSHM`.

**Zone controller SoC** hosts `ZoneNet` (owns zone Ethernet + three CAN controllers), `ZoneControl` (local body/zone control; must keep working when the backbone is gone), and `ZonePlatform` (zone-local health, fault history, watchdog/reset records, logging) over `ZoneSHM`.

**Fieldbuses behind the zone controller:**

- `PowerFD` — CAN-FD/29-bit, four native WS smart ECUs (`SmartPDU`, `ThermalPump`, `SensorHub`, `ActuatorHub`) at 20–100 Hz.
- `BodyCAN11` — committed Classical CAN11, five native WS body ECUs (`DoorGateway`, `WiperECU`, `AccessECU`, `LightingGateway`, `HVACGateway`) at 50–150 frames/s.
- `LegacyAuxCAN` — pre-existing mixed-use CAN11 carrying unrelated legacy traffic, with an allocated Guest block for two WS-capable nodes (`AuxSenseGW`, `WasherECU`) at <20 frames/s.

**LIN peripherals** (`DoorLIN`, `LightingLIN`, `HVACLIN`) are conventional peripheral devices polled by their CAN gateway. There is no WS-LIN profile in this trial, so the WS-visible interaction terminates at the gateway.

**Config B** adds a DevPC on the backbone for inventory, health/fault inspection, Link diagnostics, engineering telemetry, and service workflows. It is not a runtime coordinator and has no direct actuation authority.

**Degraded operation is a requirement, not an afterthought:** backbone loss must leave `ZoneControl` running its fieldbuses; `ZoneControl` loss must leave field ECUs powered and diagnosable; telematics loss must change nothing locally.

---

## 2. Obvious conventional implementation

> **Obvious conventional implementation:** SOME/IP or DDS over the Ethernet backbone between central compute, telematics, and zone controller; a signal-oriented CAN matrix (DBC/ARXML) per CAN bus; UDS/DoIP diagnostics with a routing table in the zone controller and central gateway; shared-memory mailboxes or RPMsg queues between SoC domains; LIN schedule tables inside each gateway ECU; a separate diagnostic address space (UDS target addresses) parallel to the control signal space.

> **What structure does a conventional design use?** Three unrelated naming systems stacked on each other: service IDs and instance IDs on Ethernet, CAN IDs plus signal offsets on the fieldbuses, and UDS logical addresses plus routing tables for diagnostics. The gateway holds hand-written translation between them, and "the same service" (health, version, update) is implemented differently per transport.

WireSpaces replaces the three naming systems with one: `Wire + SrcParticipantId + DestParticipantId + Endpoint`. The interesting consequence for this archetype is that **the diagnostic path stops being a parallel architecture** — `DiagOTA` is an ordinary Participant with ordinary relations on the same Wires as control traffic, rather than a routing layer with its own address space. That is the main structural claim this sketch tests.

---

## 3. Minimum mapping (required first)

### 3.1 Participants

| ParticipantId | Endpoint Domain | Device | Notes |
|---|---|---|---|
| 0x01 | `CoreNet` | Central compute SoC | Backbone owner; forwarder; Link status |
| 0x02 | `VehicleControl` | Central compute SoC | Vehicle mode/intent; slice time source |
| 0x03 | `DiagOTA` | Central compute SoC | Diagnostics, inventory, update orchestration |
| 0x04 | `Telemetry` | Central compute SoC | Engineering collection; no control authority |
| 0x05 | `TcuNet` | Telematics ECU | Network-facing; forwarder; Link status |
| 0x06 | `CloudServices` | Telematics ECU | Cloud upload/package handoff; no actuation |
| 0x07 | `ZoneNet` | Zone controller SoC | Ethernet + 3 CAN controllers; forwarder |
| 0x08 | `ZoneControl` | Zone controller SoC | Local body/zone control; degraded-mode owner |
| 0x09 | `ZonePlatform` | Zone controller SoC | Zone health, fault history, reset records, logs |
| 0x0A | `SmartPDU` | `PowerFD` ECU | Switched power, current sense, load faults |
| 0x0B | `ThermalPump` | `PowerFD` ECU | Local thermal loop |
| 0x0C | `SensorHub` | `PowerFD` ECU | Aggregated higher-rate sensors |
| 0x0D | `ActuatorHub` | `PowerFD` ECU | Bounded auxiliary actuators |
| 0x0E | `DoorGateway` | `BodyCAN11` ECU | Body/door control; gateways `DoorLIN` |
| 0x0F | `WiperECU` | `BodyCAN11` ECU | Wiper/washer coordination |
| 0x10 | `AccessECU` | `BodyCAN11` ECU | Lock/access |
| 0x11 | `LightingGateway` | `BodyCAN11` ECU | Lighting; gateways `LightingLIN` |
| 0x12 | `HVACGateway` | `BodyCAN11` ECU | HVAC flaps; gateways `HVACLIN` |
| 0x13 | `AuxSenseGW` | `LegacyAuxCAN` Guest node | Aux sensor/status aggregation |
| 0x14 | `WasherECU` | `LegacyAuxCAN` Guest node | Aux pump/washer control |
| 0x15 | `DevPC` | Service tool (**Config B only**) | Not a coordinator; no direct actuation |

**20 production Participants** (0x01–0x14), +1 in Config B. Comfortably inside the 8-bit allocation (`CORE §3.1`).

**LIN peripherals are deliberately not Participants.** Ten LIN leaves (3 door, 3 lighting, 4 HVAC) remain conventional peripherals. Their gateway exposes the useful behavior as **Endpoints** on its own ParticipantId — for example `DoorGateway` offers window/mirror/latch command and aggregate-state Endpoints. Per-peripheral addressing where genuinely needed is Endpoint-level or payload-level detail, not a ParticipantId, and there is no WS-LIN profile to justify otherwise (`archetype §LIN gateway behavior`). This keeps the deployment identity space at 20 instead of 30.

### 3.2 Wires

| Wire (#) | Scope | Member Links | Addressed Participants | Purpose |
|---|---|---|---|---|
| W1 `CoreLocal` | Device-private | `CoreSHM` | 0x01–0x04 | Core-internal state 100 Hz–1 kHz; core domain Services |
| W2 `ZoneLocal` | Device-private | `ZoneSHM` | 0x07–0x09 | Zone-internal state; platform health/fault/reset records |
| W3 `TcuLocal` | Device-private | `TcuSHM` | 0x05, 0x06 | Telematics-internal handoff |
| W4 `VehicleCtrl` | Network-visible | `CoreSHM` + `BackboneEth` + `ZoneSHM` | 0x02, 0x03, 0x04, 0x07, 0x08, 0x09 | Vehicle↔zone intent/state, degraded notification, time, zone summary telemetry, zone platform diagnostics |
| W5 `Telematics` | Network-visible | `CoreSHM` + `BackboneEth` + `TcuSHM` | 0x03, 0x04, 0x05, 0x06 | Cloud status, remote diag session, upload, update package handoff |
| W6 `ZonePower` | Network-visible | `CoreSHM` + `BackboneEth` + `ZoneSHM` + `PowerFD` | 0x03, 0x04, 0x08, 0x0A–0x0D | `PowerFD` control/state, selected telemetry, diagnostics/update |
| W7 `ZoneBody` | Network-visible | `CoreSHM` + `BackboneEth` + `ZoneSHM` + `BodyCAN11` | 0x03, 0x08, 0x0E–0x12 | Body control/state + body diagnostics/update |
| W8 `ZoneAux` | Network-visible | `CoreSHM` + `BackboneEth` + `ZoneSHM` + `LegacyAuxCAN` (Guest block) | 0x03, 0x08, 0x13, 0x14 | Aux state/command + Guest-node diagnostics |
| W9 `ServiceAccess` | Network-visible, **Config B only** | `BackboneEth` + `CoreSHM` | 0x15, 0x03, 0x04, 0x01, 0x05, 0x07 | Service-tool access to diagnostics, telemetry, Link status |

**Why this shape, and not another.**

Three decisions do the real work:

**(a) Device-private Wires absorb high-rate internal traffic.** `CoreSHM` and `ZoneSHM` carry 100 Hz–1 kHz internal publications. Those belong on device-private Wires (`CORE §4.2`) — real Logical Buses with ordinary semantics that cross an internal Link but are never forwarded outward. Without them, internal state would either leak onto external Wires or need application-level suppression. Note that device-private WireNumbers are **device-scoped**, so W1/W2/W3 need not consume three deployment-global Wire values.

**(b) Fieldbus Wires span shared memory → backbone → bus.** This is what lets `DiagOTA` hold ordinary canonical relations with a body ECU (and be `MainB`), and lets `Telemetry` receive `SensorHub` data, without a parallel diagnostic address space, a splice, or an application proxy. `ZoneNet` and `CoreNet` forward transparently and are **not** addressed Participants on W6/W7/W8 — their own Link status Service lives on W2/W4/W1. That matters concretely: `ZoneNet` does **not** consume a Node position on `BodyCAN11`.

**(c) Each Wire spanning a scarce Link carries only relations that need that Link.** Base gateway behavior is **flood-and-filter within the Wire's realization** (`CORE §12`: a gateway may forward a directed PDU to every configured branch; destination-based egress pruning is an explicit optimization, not the baseline). So `ZoneControl ↔ DiagOTA` chat is assigned to **W4**, never W7 — otherwise it would be replicated onto a 500 kbit/s CAN segment. Every W7 relation has a `BodyCAN11` endpoint; every W6 relation has a `PowerFD` endpoint; every W8 relation has a Guest-node endpoint. The residual cost is that zone-local body control also floods onto the backbone toward `CoreSHM` — trivial at 100BASE-T1 and 50–150 frames/s, and addressable later (§4.2).

**Rejected:** one Wire per Service (9 Services × scopes → unauditable, and explicitly discouraged); one vehicle-wide Wire (would put 1 kHz internal traffic and body control on every Link, violating `INTRO §3` / `WIRE-10`); one Wire per physical Link (would force `DiagOTA` into a proxy architecture and waste `MainB`).

**Deliberate asymmetry worth recording:** `CoreSHM` carries six Wires and `BackboneEth` five, while **each CAN bus carries exactly one**. Overlapping Wires are cheap on shared memory and Ethernet and expensive on CAN (one binding per Wire, overlay §3). The decomposition leans into that rather than fighting it.

### 3.3 Interactions (primary)

**Vehicle ↔ zone control (W4)**

| Interaction | Src | Dest | Broadcast? | Rate | Notes |
|---|---|---|---|---|---|
| Vehicle/zone intent | 0x02 | 0x08 | No | 20–50 Hz + event | Mode, body intent, power-state intent |
| Zone state | 0x08 | 0x02 | No | 10–20 Hz | Summarized body/zone status |
| Time synchronization | 0x02 | kBroadcast | Yes | 1–10 Hz | Slice time source (`CORE §3.2`) |
| Degraded-mode notification | 0x02 ↔ 0x08 | — | No | Event | Either direction; no authority invention |
| Zone summary telemetry | 0x08 | 0x04 | No | 5–20 Hz | |
| Zone platform fault/status | 0x09 ↔ 0x03 | — | No | Event + query | Reset history, runtime state, logs |
| Zone Link status | 0x07 | 0x03 | No | Query + event | Zone Ethernet/CAN health |
| Zone domain Identity/Health | 0x07–0x09 | 0x03 | No | Startup/query | Standard Services |

**Telematics (W5)**

| Interaction | Src | Dest | Rate | Notes |
|---|---|---|---|---|
| Cloud connection/status | 0x06 | 0x03, 0x04 | Low + event | |
| Remote diagnostic session request | 0x06 | 0x03 | On demand | `DiagOTA` stays vehicle-side diagnostic path |
| Update package handoff | 0x06 ↔ 0x03 | On demand | Large transfer; cloud protocol out of scope |
| Selected telemetry upload | 0x04 | 0x06 | Batched/periodic | Subset only |
| Telematics Link status / Identity / Health | 0x05, 0x06 | 0x03 | Query | |

**`PowerFD` zone control (W6)**

| Interaction | Src | Dest | Rate | Notes |
|---|---|---|---|---|
| Power command | 0x08 | 0x0A | 20 Hz + event | Switched loads / power state |
| PDU status/current/fault | 0x0A | 0x08 | 20–50 Hz | |
| Thermal command/state | 0x08 ↔ 0x0B | 10–50 Hz | Survives backbone loss |
| Sensor state | 0x0C | 0x08 | 50–100 Hz | Selected subset |
| Actuator command/state | 0x08 ↔ 0x0D | 20–50 Hz | |
| Selected telemetry | 0x0A, 0x0B, 0x0C | 0x04 | 20–100 Hz | FD payload capacity |
| Zone mode + relayed time | 0x08 | kBroadcast | 1–10 Hz | Composition, see below |
| Fault/DTC, Identity, Health, update | 0x03 ↔ 0x0A–0x0D | Query/event | Four ordinary relations |

**`BodyCAN11` zone control (W7)**

| Interaction | Src | Dest | Rate | Notes |
|---|---|---|---|---|
| Door/body command + state | 0x08 ↔ 0x0E | 10–20 Hz + event | Gateway performs `DoorLIN` transactions internally |
| Wiper command + state | 0x08 ↔ 0x0F | 10–20 Hz + event | |
| Access command + state | 0x08 ↔ 0x10 | Event + 5–10 Hz | |
| Lighting command + state | 0x08 ↔ 0x11 | 10–20 Hz + event | Gateways `LightingLIN` |
| HVAC command + state | 0x08 ↔ 0x12 | 5–20 Hz | Gateways `HVACLIN` |
| Zone mode + relayed time | 0x08 → kBroadcast | 1–10 Hz | One broadcast instead of five directed relations |
| Diagnostics: Identity/Version, Health, Fault/DTC, update, LIN-subordinate status | 0x03 ↔ 0x0E–0x12 | Query/event | Five ordinary relations; QoS Background |

**Guest `LegacyAuxCAN` (W8)**

| Interaction | Src | Dest | Rate | Notes |
|---|---|---|---|---|
| Auxiliary state | 0x13 | 0x08 | 2–10 Hz | Coexists with unrelated legacy CAN |
| Auxiliary command/status | 0x08 ↔ 0x14 | Event + low | |
| Diagnostic/version access | 0x03 ↔ 0x13, 0x03 ↔ 0x14 | On demand | Production network path |

**Config B service access (W9)**

| Interaction | Src | Dest | Notes |
|---|---|---|---|
| Vehicle inventory | 0x15 ↔ 0x03 | | Through production diagnostic path |
| Health/fault inspection | 0x15 ↔ 0x03 | | Platform domains via `DiagOTA` |
| Link diagnostics | 0x15 ↔ 0x01, 0x05, 0x07 | | Three network domains directly |
| Engineering telemetry | 0x15 ↔ 0x04 | | On demand / streaming |
| Targeted update/service | 0x15 → 0x03 → target | | **No direct DevPC↔ECU relation** |

**Device-private (W1/W2/W3)**

`VehicleControl` internal state publications (100 Hz–1 kHz) and core-domain Services on W1; `ZoneControl ↔ ZonePlatform` health/fault/watchdog records and zone internal state on W2; `TcuNet ↔ CloudServices` handoff on W3. `DiagOTA ↔ CoreNet`, `DiagOTA ↔ VehicleControl`, and `DiagOTA ↔ Telemetry` are same-device relations and stay on W1 rather than consuming W4 scope.

**Relation count (production): ~48 application relations across 8 Wires.** Config B adds 5 and changes no existing Wire.

**Time distribution is composition, and that is worth flagging.** `VehicleControl` is the slice time source, but it is not a Main position on `BodyCAN11` and the default VCN map offers only two broadcast slots (`MainA`/`MainB`). Rather than force `VehicleControl` into a Main slot or enumerate five directed time relations, `ZoneControl` consumes time on W4 and re-authors zone mode + time as a `MainA` broadcast on W6 and W7 under **its own** ParticipantId. That is composition, not transparent forwarding (`CORE §12`, `§5.1`), it matches how per-segment time masters actually work, and it costs **two composition points** — counted, not hidden.

### 3.4 Forwarding

| Gateway domain | Ingress | Wire | Egress | Local delivery? | Splice? | Notes |
|---|---|---|---|---|---|---|
| `CoreNet` 0x01 | `CoreSHM` ↔ `BackboneEth` | W4 | other side | No (not a member) | No | Transparent; preserves `Src`/`Dest` |
| `CoreNet` | `CoreSHM` ↔ `BackboneEth` | W5, W6, W7, W8 | other side | No | No | Same |
| `ZoneNet` 0x07 | `BackboneEth` ↔ `ZoneSHM` | W4 | other side | **Yes** (member for Link status) | No | |
| `ZoneNet` | `BackboneEth` / `ZoneSHM` ↔ `PowerFD` | W6 | remaining branches | No | No | 3-branch star at `ZoneNet` |
| `ZoneNet` | `BackboneEth` / `ZoneSHM` ↔ `BodyCAN11` | W7 | remaining branches | No | No | Committed CAN11 binding, Alias 1 |
| `ZoneNet` | `BackboneEth` / `ZoneSHM` ↔ `LegacyAuxCAN` | W8 | remaining branches | No | No | Guest block binding |
| `TcuNet` 0x05 | `BackboneEth` ↔ `TcuSHM` | W5 | other side | **Yes** (Link status) | No | |
| `CoreNet` | `CoreSHM` ↔ `BackboneEth` | W9 | other side | No | No | Config B only |

**No splices in the minimum mapping.** Composition occurs at two points only (`ZoneControl` zone-mode/time broadcast onto W6 and W7).

Every Wire realization is a tree: `CoreSHM —(CoreNet)— BackboneEth —(ZoneNet)— {ZoneSHM, PowerFD | BodyCAN11 | LegacyAuxCAN}` and `… —(TcuNet)— TcuSHM`. Loop-free ✓ (`CORE §3.3`).

**Application-gateway boundaries (not WS forwarding):** `DoorGateway`, `LightingGateway`, and `HVACGateway` terminate WS and perform native LIN transactions internally. This is composition at the application level — the gateway authors LIN activity from its own logic, and reports aggregate state under its own ParticipantId.

### 3.5 Link profiles

| Physical link | Profile | WS Wire(s) | Notes |
|---|---|---|---|
| `CoreSHM` | Shared memory / FIFO (`LINK §8`) | W1, W4, W5, W6, W7, W8 (+W9) | Simplest profile class; six overlapping Wires |
| `ZoneSHM` | Shared memory / FIFO | W2, W4, W6, W7, W8 | Five overlapping Wires |
| `TcuSHM` | Shared memory / FIFO | W3, W5 | |
| `BackboneEth` | WS over Automotive Ethernet (`LINK §6`) | W4, W5, W6, W7, W8 (+W9) | Aggregation candidate; profile provisional |
| `PowerFD` | CAN-FD, 29-bit | W6 | **Profile approach undecided in `LINK §1`** — see §7 |
| `BodyCAN11` | Committed CAN11, **default** VCN map | W7 | Alias 1; positive control |
| `LegacyAuxCAN` | Guest CAN11, Guest-3 block | W8 | Coexists with non-WS legacy IDs |
| `DoorLIN`, `LightingLIN`, `HVACLIN` | **Not WS Links** | — | Conventional LIN behind gateway ECUs |

Overlay §13 is satisfied: `BodyCAN11` is committed-only, `LegacyAuxCAN` is Guest-only, and they are separate physical segments. No bus mixes classifiers.

### 3.6 CAN11 bindings

#### `BodyCAN11` — committed CAN11

```text
Profile:          committed
WireAliases used:             1 / 8
  default map:                  1
  custom map:                   0

Per alias:
  Alias:                        1
  Canonical Wire:               W7 ZoneBody
  Mapping:                      Default
  Ordinary VCNs used:           11 / 32   (minimum mapping only)
  VCNs available, unused:       VCN 1 (MainB broadcast), VCN 2 (MainA<->MainB),
                                  VCN 14-31 (Node5..Node13 slots, 18 slots)
  Default map sufficient?       yes
  Custom entries (if Explicit): 0
  MainA PID / MainB PID:        0x08 ZoneControl / 0x03 DiagOTA
  Node positions used:          5 / 14
  Second alias needed for ordinary production?   no
```

| VCN | Default-map relation | Assigned Participants | Use |
|---|---|---|---|
| 0 | MainA → Broadcast | `ZoneControl` → kBroadcast | **Used** — zone mode + relayed time |
| 1 | MainB → Broadcast | `DiagOTA` → kBroadcast | Available, unused (no diagnostic broadcast needed) |
| 2 | MainA ↔ MainB | `ZoneControl` ↔ `DiagOTA` | Available, **deliberately unused** — that relation lives on W4 so it never crosses CAN |
| 3 | Reserved | — | Link control |
| 4 | Node0 ↔ MainA | `DoorGateway` ↔ `ZoneControl` | **Used** — door command/state |
| 5 | Node0 ↔ MainB | `DoorGateway` ↔ `DiagOTA` | **Used** — diagnostics/update |
| 6 / 7 | Node1 ↔ MainA / MainB | `WiperECU` ↔ `ZoneControl` / `DiagOTA` | **Used** |
| 8 / 9 | Node2 ↔ MainA / MainB | `AccessECU` ↔ `ZoneControl` / `DiagOTA` | **Used** |
| 10 / 11 | Node3 ↔ MainA / MainB | `LightingGateway` ↔ `ZoneControl` / `DiagOTA` | **Used** |
| 12 / 13 | Node4 ↔ MainA / MainB | `HVACGateway` ↔ `ZoneControl` / `DiagOTA` | **Used** |
| 14–31 | Node5–13 ↔ Mains | — | Available, unused — 4.5 spare Node positions |

**MainB is assigned.** This sketch exercises the two-Main default allocation, `Main↔Node` on both Mains, and one Main broadcast. It does not exercise `MainB → Broadcast` or `MainA ↔ MainB`.

**QoS does useful work here.** Body control uses QoS Critical/High and diagnostics/update uses QoS Background within the same alias. Because canonical QoS occupies the most arbitration-significant identifier bits (`LINK §2.2`), an update burst on VCN 5/7/9/11/13 arbitrates *below* ongoing control traffic without a second Wire, a second alias, or a bandwidth reservation scheme. That is the mechanism that answers the archetype's "diagnostics/update may create temporary additional load" concern.

#### `LegacyAuxCAN` — Guest CAN11

```text
Profile:          Guest
Guest width:      3  (VCN[2:0] + Direction in low 4 bits)
GuestBase:        0x7A0   (aligned 16-ID block 0x7A0..0x7AF)
Canonical Wire:   W8 ZoneAux
Ordinary VCNs required:       4
Ordinary capacity:            7   (VCN 0..6; VCN 7 reserved for Link control)
Default/global mapping sufficient?  yes
Fixed canonical QoS:          Normal (binding-supplied; no per-frame QoS in Guest)
```

| Guest VCN | Relation | Use |
|---|---|---|
| 0 | `ZoneControl` ↔ `AuxSenseGW` | Auxiliary state |
| 1 | `ZoneControl` ↔ `WasherECU` | Auxiliary command/status |
| 2 | `DiagOTA` ↔ `AuxSenseGW` | Identity/version/health/fault |
| 3 | `DiagOTA` ↔ `WasherECU` | Identity/version/health/fault/update |
| 4–6 | — | Available |
| 7 | Reserved | Link control |

**Guest-3 is genuinely sufficient**, which is the useful adoption answer: a 16-identifier allocation on somebody else's bus buys two fully-serviced WS nodes including diagnostics and update. Two limitations are real and counted: Guest carries **no per-frame QoS**, so an update burst on VCN 3 cannot be de-prioritized below aux control the way it can on `BodyCAN11`; and Guest VCN meanings are **deployment-global** (overlay §6), so these four slots are consumed deployment-wide — a second Guest bus in this vehicle would have only three ordinary slots left. See SF-R6-009.

#### `PowerFD` — CAN-FD / 29-bit

Direct canonical addressing is sufficient: 8-bit `Src` + 8-bit `Dest` + a Wire/QoS selector fit comfortably in 29 bits, so the twelve W6 relations need **no relation table, no aliases, and no VCN enumeration**. This is the same result archetype 08 produced on Ethernet: relation compression is a CAN11 artifact, not a canonical-model property.

**Membership vs presence.** All 20 production Participants are **preconfigured** on their Wires and stay members when unreachable — a powered-down `WasherECU` or a failed `DoorGateway` leaves its Guest VCN / Node0 slot configured and silent; delivery fails or queues drain (`brief §6.4`). The DevPC is different by choice: `W9 ServiceAccess` exists **only in the Config B configuration**, so in production the service tool is genuinely absent from the committed configuration rather than present-but-unreachable. Config A → Config B is purely additive: one Wire, six memberships, two `CoreNet` mask entries, and **zero** changes to any CAN binding.

### 3.7 Configuration inventory (auditable)

```text
Production WS Participants:        20            (+1 DevPC in Config B)
Endpoint Domains per device:       Core 4, Zone 3, Tcu 2, field ECUs 1 each
LIN peripherals as Participants:   0 of 10

Physical Links:                    10 total
  shared memory / IPC              3   (CoreSHM, ZoneSHM, TcuSHM)
  Automotive Ethernet              1   (BackboneEth)
  CAN-FD / 29-bit                  1   (PowerFD)
  Classical CAN11 committed        1   (BodyCAN11)
  Classical CAN11 Guest            1   (LegacyAuxCAN)
  LIN (not WS Links)               3

Canonical Wires:                   8   (+1 Config B)
  device-private                   3   (W1, W2, W3 — device-scoped WireNumbers)
  network-visible                  5   (W4..W8)  +1 (W9, Config B)
Wires per Link:                    CoreSHM 6, BackboneEth 5, ZoneSHM 5, TcuSHM 2,
                                   PowerFD 1, BodyCAN11 1, LegacyAuxCAN 1

Application relations:             ~48  (+5 Config B)
Composition points:                2    (ZoneControl mode/time onto W6, W7)
Splices:                           0

Gateway / forwarding state:        ~23 Wire-to-Link mask entries  (+2 Config B)
  CoreNet                          10   (5 Wires x 2 branches)
  ZoneNet                          11   (W4 x2, W6/W7/W8 x3)
  TcuNet                           2

BodyCAN11:
  WireAliases used                 1 / 8
  default-map aliases              1
  custom-map aliases               0
  VCNs used                        11 / 32
  second alias for production?     no
  MainA / MainB                    assigned / assigned

LegacyAuxCAN:
  Guest width                      3
  Guest VCNs used / available      4 / 7   (deployment-global)
  default Guest mapping sufficient yes

CAN-FD:
  direct normal-range addressing sufficient?   yes (no relation table)

Standard Services:
  ubiquitous (all 20)              Identity/Version, Health, Fault/DTC,
                                   firmware update, diagnostic RPC
  limited by design                Link status (6), Telemetry producers (4),
                                   Logs/events (12), Time consumers (selected)
  extra Wires required             0
```

Roughly **100 counted configuration objects** for a 20-Participant, 10-Link vehicle slice with 9 Service classes. Tooling can generate essentially all of it, but it is counted here regardless.

---

## 4. Optional optimizations

### 4.1 Second `BodyCAN11` alias for service separation

Bind Alias 2 to W7 with a diagnostic-only VCN map so service traffic is separable at the Link layer. **Problem it would solve:** Link-level isolation of diagnostics from control. **Rejected:** QoS bits already order update traffic below control within one alias (§3.6), 11/32 VCNs leaves ample room, and a second alias would double ingress classification state for no semantic gain. Overlay §8's spare alias remains reserved for VCN-map migration.

### 4.2 Locality splice for zone-local fieldbus traffic

Make W6/W7/W8 zone-local (`ZoneSHM` + bus) and splice them at `ZoneNet` into a backbone-scope diagnostic/telemetry Wire (`DEPLOY §1.8`). **Problem it would solve:** under base flood-and-filter, zone-local body control currently replicates onto `BackboneEth` and `CoreSHM`; a splice would confine it to the zone. **Rejected for the minimum mapping:** the backbone is 100BASE-T1-class and body control is 50–150 frames/s, so bandwidth does not justify it (`brief §5` step 7). It becomes the right answer with several zones, a slower backbone, or if central-side receive load matters. Note this is the honest alternative to the simpler and cheaper option below.

### 4.3 Destination-pruned egress masks

Install participant-location egress pruning at `CoreNet`/`ZoneNet` so directed PDUs are not flooded to branches that cannot reach the destination. **Problem it would solve:** same as §4.2, at much lower configuration cost. `CORE §12` names this explicitly as an optimization that does not change Wire semantics. **Preferred over §4.2** if backbone replication ever matters — it needs no new Wires and no splices.

### 4.4 Direct DevPC relations to field ECUs

Give the DevPC direct VCN relations on `BodyCAN11` / `LegacyAuxCAN`. **Problem it would solve:** one less hop for service tooling. **Rejected, and this is load-bearing:** the DevPC would become a *third* Main on `BodyCAN11`, which the two-Main default cannot express — forcing an Explicit custom map plus five more VCNs, and consuming two more of the seven deployment-global Guest slots. Routing the service tool through `DiagOTA`'s production diagnostic path keeps Config B additive and keeps the default map viable. **The two-Main default is sufficient here precisely because the service tool is not a Main.**

### 4.5 Wider Guest allocation on `LegacyAuxCAN`

Move to Guest-4 for headroom. **Rejected:** 4/7 slots used, <20 frames/s, and a wider block asks the bus owner for 32 identifiers instead of 16. Revisit only if a second Guest bus appears (the global-numbering constraint in §3.6), which is a deployment question rather than a bus-capacity one.

### 4.6 Promote LIN peripherals to Participants

**Rejected:** no WS-LIN profile exists in this trial, ten more ParticipantIds would buy nothing the gateway Endpoints do not already express, and it would force per-peripheral relations across `BodyCAN11`. Gateway-level representation is both cheaper and more faithful.

### 4.7 Separate telemetry Wire

A dedicated telemetry Wire from `PowerFD` producers to `Telemetry`. **Rejected:** W6 already connects those Participants; a separate Wire would be a Service-shaped Wire, which is precisely what the archetype warns against. Telemetry rate control belongs to QoS and producer configuration, not Wire count.

---

## 5. Friction signals (minimum mapping happy path)

| Signal | None / Mild / Significant | One-sentence justification |
|---|---|---|
| Artificial Wire | None | Each of the eight Wires names a real locality or authority scope (device-internal, vehicle control, telematics, one per fieldbus); none exists to satisfy direction or Service semantics. |
| Wire proliferation | Mild | Eight Wires for 20 Participants and 7 WS Links is fewer than one per Link, but the three device-private Wires are a judgement call — a leaner mapping could fold core-internal traffic onto W4 at the cost of exporting 1 kHz publications. |
| Artificial hierarchy | None | `MainA = ZoneControl` and `MainB = DiagOTA` correspond to control authority and diagnostic authority that genuinely exist in the archetype; no coordinator was invented, and no Origin role appears. |
| VCN pressure | None | 11 / 32 on the default map with 4.5 spare Node positions; Guest 4 / 7; `PowerFD` needs no relation table. |
| WireAlias pressure | None | 1 / 8 aliases on `BodyCAN11`; the Guest bus is a separate segment; spare aliases remain free for migration. |
| Configuration burden | Moderate | ~100 counted objects (20 PIDs, 8 Wires, ~23 mask entries, 11 VCN role assignments, 4 Guest entries, ~10 Link bindings) — more explicit than any single conventional table, but replacing three parallel naming systems, and auditable in one page. |
| Failure/topology mismatch | None | Every documented degraded mode is a Wire-realization partition that can be read off the Wire table (§10, Question F). |

**Free-form notes.** Two things were awkward without being frictional. First, deciding *which* Wire carries `ZoneControl ↔ DiagOTA` is a real design decision with a bandwidth consequence, and nothing in the model forces the right answer — it depends on knowing that flood-and-filter is the baseline. Second, `ZoneNet` owning three CAN controllers while not being an addressed Participant on any of their Wires reads oddly at first, then turns out to be the cleanest part of the mapping: it is what keeps the transceiver owner out of the VCN map.

---

## 6. Model pressure

- **If you could change one WS concept** to simplify this system, what would it be? Nothing in the canonical model. The one place the mapping had to be clever is the interaction between broad Wires and flood-and-filter forwarding: correctness is easy, but *efficiency* depends on assigning each interaction to the narrowest Wire connecting the pair, and that reasoning is currently left entirely to the mapper. A validation rule of the form "warn when a Wire's relation set contains relations whose endpoints are all off a scarce member Link" would catch the mistake mechanically. That is tooling, not protocol.
- **Did WS expose a useful distinction** the conventional model obscures? Yes, three, and they are the substance of this sketch. **(a)** Endpoint vs Wire vs Dest: nine Service classes across 20 Participants added zero Wires, because the Service is the Endpoint and the Wire is only the propagation scope — the conventional design needs SOME/IP service IDs, CAN signal maps, *and* UDS addresses to say the same thing. **(b)** Transceiver ownership vs Participant identity: `ZoneNet` forwards `BodyCAN11` without being addressable on it, so `MainA` can be the actual control authority. **(c)** Diagnostics as ordinary relations rather than a parallel routing architecture — `DiagOTA` as `MainB` is the whole UDS routing layer collapsed into two VCN slots per node.

**Synthesis note:** The corpus progression is now legible. Hierarchical machine (01) → default map. Peer mesh (07) → default map unusable, Explicit required. Dense cell (09) → Explicit near the 29/31 ceiling. **Controller + diagnostic authority on a realistic body bus (10) → default map fits unmodified with both Mains assigned and 21 slots spare.** The default map is not a compromise for hierarchical buses; it is a good fit for the specific and common shape of *one control authority plus one service authority plus leaves*, which is what a body bus actually is.

---

## 7. Open questions

- **CAN-FD profile is undefined.** `LINK §1` lists CAN FD/XL as "approach undecided." This sketch assumes direct canonical addressing of 8-bit `Src`/`Dest` in a 29-bit identifier with no relation table. If the eventual FD profile compresses identity instead, W6's twelve relations would need re-examination — though 12 relations is far from any plausible ceiling.
- **Ethernet profile choice for the backbone.** `LINK §6` offers WS-over-UDP/IP or direct Layer 2 and develops neither. An automotive backbone plausibly wants Layer 2 with aggregation (`CORE §12.5`); the choice affects binding count and MTU, not the Wire decomposition.
- **Shared-memory Wire multiplicity.** `CoreSHM` carries six Wires. Overlapping Wires on one Link are explicitly legal (`WIRE-9`), but whether six on one shared-memory Link is idiomatic or a sign of over-decomposition is worth a second opinion from a convergence mapper.
- **Device-private WireNumber reuse.** W1/W2/W3 are device-scoped, so all three could use the same reserved value. Whether tooling should present them as one Wire or three is a `DEPLOY` presentation question that affects auditability more than semantics.
- **Guest update throughput.** Firmware update to `WasherECU` through a 16-identifier Guest block at <20 frames/s is architecturally fine and practically slow. Whether "field-updatable over Guest" should carry a documented expectation is an adoption question, not a protocol gap.
- **Time sync as a Service vs a protocol.** This sketch treats per-segment relay as composition. Whether the standard time Service should *define* relay behavior, or leave it to deployment, is unresolved and affects whether the two broadcast slots in the default map are ever a constraint.

---

## 8. Spec findings

| ID | Finding |
|---|---|
| SF-R6-009 | **Guest VCN slots are a deployment-global resource, not a per-bus one** (`overlay §6`). One 2-node Guest bus with control + diagnostics consumes 4 of the 7 ordinary Guest-3 slots for the entire WireSpace deployment; a second Guest bus in the same vehicle would have 3 left, regardless of its own capacity. Guest sizing guidance should be stated per deployment, and Guest widening should be described as a deployment-wide migration. |
| SF-R6-010 | **Guest has no per-frame QoS, so update traffic cannot be de-prioritized below control on the same Guest block** (`overlay §6`, `LINK §2.2`). On committed CAN11 the QoS field makes an update burst arbitrate below control within one alias; the Guest binding supplies one fixed canonical QoS, so the same protection is unavailable on a legacy bus — where it is arguably needed more. Worth an explicit note in the Guest profile rather than discovery during a service event. |

**Evidence closing an earlier finding:** SF-R6-004 recorded that no sketch had exercised the two-Main default allocation. Archetype 10 assigns `MainA = ZoneControl` and `MainB = DiagOTA` with five Node positions and both `Main↔Node` columns in use — the allocation is exercised and fits. `MainB → Broadcast` (VCN 1) and `MainA ↔ MainB` (VCN 2) remain unexercised, deliberately.

---

## 9. Diagrams

### Wire-centric (production, Config A)

```text
device-private (never forwarded)
  W1 CoreLocal   [CoreSHM]   0x01 CoreNet  0x02 VehicleControl  0x03 DiagOTA  0x04 Telemetry
  W2 ZoneLocal   [ZoneSHM]   0x07 ZoneNet  0x08 ZoneControl     0x09 ZonePlatform
  W3 TcuLocal    [TcuSHM]    0x05 TcuNet   0x06 CloudServices

network-visible
  W4 VehicleCtrl [CoreSHM + BackboneEth + ZoneSHM]
       0x02 VehicleControl <-> 0x08 ZoneControl        intent / state / degraded / time
       0x03 DiagOTA        <-> 0x08, 0x09, 0x07        zone diagnostics + Link status
       0x08 ZoneControl     -> 0x04 Telemetry          zone summary

  W5 Telematics  [CoreSHM + BackboneEth + TcuSHM]
       0x06 CloudServices  <-> 0x03 DiagOTA            session / package handoff
       0x04 Telemetry       -> 0x06 CloudServices      upload

  W6 ZonePower   [CoreSHM + BackboneEth + ZoneSHM + PowerFD]        CAN-FD 29-bit
       0x08 <-> 0x0A 0x0B 0x0C 0x0D    control/state
       0x03 <-> 0x0A 0x0B 0x0C 0x0D    diagnostics
       0x0A 0x0B 0x0C -> 0x04          telemetry
       0x08 -> kBroadcast              zone mode + time

  W7 ZoneBody    [CoreSHM + BackboneEth + ZoneSHM + BodyCAN11]      committed, DEFAULT map
       MainA 0x08 ZoneControl <-> Node0..4     VCN 4 6 8 10 12
       MainB 0x03 DiagOTA     <-> Node0..4     VCN 5 7 9 11 13
       MainA 0x08 -> kBroadcast                VCN 0
                                               11 / 32 used

  W8 ZoneAux     [CoreSHM + BackboneEth + ZoneSHM + LegacyAuxCAN]   Guest-3 @ 0x7A0
       0x08 <-> 0x13, 0x14      Guest VCN 0, 1
       0x03 <-> 0x13, 0x14      Guest VCN 2, 3
                                4 / 7 used

Config B adds W9 ServiceAccess [BackboneEth + CoreSHM]: 0x15 DevPC <-> 0x03, 0x04, 0x01, 0x05, 0x07
                                                        zero CAN reconfiguration
```

### Wires per Link (asymmetry is deliberate)

```text
CoreSHM      ######  6 Wires     shared memory: overlapping Wires are cheap
BackboneEth  #####   5 Wires     Ethernet: cheap
ZoneSHM      #####   5 Wires
TcuSHM       ##      2 Wires
PowerFD      #       1 Wire      CAN: one binding per Wire, so exactly one
BodyCAN11    #       1 Wire
LegacyAuxCAN #       1 Wire      Guest block only; legacy IDs untouched
```

### Failure boundaries read off the Wire table

```text
BackboneEth lost
    W4, W5, W6, W7, W8 partition at CoreNet/ZoneNet/TcuNet
    -> ZoneSHM + PowerFD + BodyCAN11 + LegacyAuxCAN portions intact
    -> ZoneControl keeps FULL local control; DiagOTA/Telemetry/cloud unreachable   [required behavior]

ZoneNet lost
    W4/W6/W7/W8 zone branches dead; W2 ZoneLocal intact
    -> ZoneControl + ZonePlatform alive internally, no external reach              [documented limitation]

ZoneControl lost (MainA silent)
    W7 MainB path unaffected -> DiagOTA still reaches all 5 body ECUs
    -> ECUs powered and diagnosable, coordinated control lost                      [exactly as specified]

DiagOTA lost (MainB silent)
    VCN 5/7/9/11/13 idle; control on VCN 0/4/6/8/10/12 unaffected

Telemetry lost          W6 telemetry relations idle; control unaffected
Telematics lost         W3 gone, W5 TcuSHM branch gone; no production impact
BodyCAN11 lost          W7 CAN branch only; DoorLIN/LightingLIN/HVACLIN unreachable
                        PowerFD and LegacyAuxCAN unaffected
DoorGateway lost        Node0 silent; DoorLIN peripherals unreachable; rest of body bus fine
one LIN leaf lost       only that function; gateway still reports aggregate state
DevPC absent            W9 not in the committed production configuration at all
```

### Identity layering (what replaced what)

```text
conventional                        WireSpaces
------------                        ----------
SOME/IP service + instance ID  -->  Endpoint (Namespace + EndpointId)
CAN ID + signal offset         -->  Wire + Src/Dest + Endpoint  (VCN is Link-local only)
UDS logical address + routing  -->  DiagOTA as an ordinary Participant (MainB)
RPMsg / mailbox channel        -->  device-private Wire
LIN schedule table             -->  stays inside the gateway ECU
```

---

## 10. Archetype questions A–G

**A — R6 scale / Logical Bus model.** Holds. Twenty Participants, ten Links, three gateways, and nine Service classes resolve to **8 Wires and ~48 relations**, with neither of the failure modes the archetype warns about: no Wire-per-interaction (48 relations on 8 Wires) and no giant machine-wide Wire (the widest, W4, has six members and excludes every field ECU). What makes it work is choosing Wires by **locality and authority** rather than by cable, Service, or direction.

**B — intra-device + inter-device continuity.** Yes, and this is the strongest result. `CoreSHM`/`ZoneSHM`/`TcuSHM` are ordinary Physical Links with ordinary Wires (`LINK §8`, `CORE §13.1`). Moving a Service boundary from same-SoC to Ethernet-ECU to CAN-ECU changes the **Link profile and Wire membership** — deployment configuration — not the Service. Concretely: `DiagOTA` queries Health from `Telemetry` (same SoC, W1), `ZonePlatform` (Ethernet + SHM, W4), `SensorHub` (CAN-FD, W6), `DoorGateway` (committed CAN11, W7), and `WasherECU` (Guest CAN11, W8) using the same canonical `Src`/`Dest`/Endpoint form each time. Five transports, one Service implementation, zero application-level reinvention. The only thing that varies is what the Link profile must reconstruct.

**C — committed CAN11 VCN (positive control).** **"Two-Main default is natural; one alias covers production body + diagnostics."** Unmodified default map, `MainA = ZoneControl`, `MainB = DiagOTA`, five Nodes, 11/32 VCNs, 1/8 aliases, no custom entries, no second alias for production. Three details matter more than the counts. (i) The Main positions map to authorities that *already exist* — this is not a coordinator invented to satisfy the profile, which is exactly the failure mode archetype 07 exposed. (ii) The default map survives because the service tool goes through `DiagOTA` rather than becoming a third Main (§4.4); a design that gave the DevPC direct ECU relations would have forced a custom map. (iii) QoS within one alias, not extra Wires or aliases, is what keeps update bursts from disturbing control. Consequence recorded honestly: with only two broadcast slots, time distribution becomes a per-segment composition relay (§3.3) — sensible, but a constraint rather than a free choice.

**D — Guest CAN.** The **smallest** Guest allocation is sufficient and useful. Guest-3 (16 identifiers, 7 ordinary VCNs) supports both Guest nodes with full control *and* diagnostic/update relations using 4 slots, without touching legacy identifiers and with the block placed high so WS arbitrates below typical low-ID legacy control frames. That is a good adoption story: a bus owner gives up 16 IDs and gets two fully-serviced WS nodes. Two limits are real and are filed as SF-R6-009 (Guest slots are deployment-global, so this bus consumed 4 of 7 vehicle-wide) and SF-R6-010 (no per-frame QoS, so update cannot be de-prioritized below aux control).

**E — Service density.** Repetitive in the useful way. Nine Service classes across up to 20 Participants required **zero additional Wires** and zero per-Link Service variants, because Service identity is the **Endpoint** while the Wire only bounds propagation. Service count scales Endpoints (per-device, local) and leaves the topology untouched — the property that fails in the conventional design, where the same nine capabilities need SOME/IP definitions, CAN signal maps, and UDS services separately. The one place Service density *did* touch the network layer is `BodyCAN11`, where diagnostics needs the second Main position — and that is precisely what `MainB` is for.

**F — failure/degraded behavior.** Easy to reason about, because Wire boundaries were drawn on locality and therefore coincide with failure boundaries. Each documented degraded mode is a **partition of a Wire realization**, readable directly from the Wire table (see the diagram in §9). The two most demanding cases fall out for free: backbone loss leaves the `ZoneSHM`+fieldbus portions of W6/W7/W8 fully functional so `ZoneControl` keeps complete local authority; and `ZoneControl` loss leaves the `MainB` diagnostic path independent, so ECUs stay diagnosable exactly as the archetype requires. A per-cable Wire decomposition would have obscured the first case; an all-in-one Wire would have obscured both.

**G — configuration auditability.** Passes, with the caveat that it is genuinely ~100 objects (§3.7). What makes it auditable is that the inventory is *layered* rather than flat: 20 Participants and 8 Wires describe the system, ~23 mask entries describe forwarding mechanically, and the per-bus representation detail is 11 default-map role assignments plus 4 Guest entries — two small tables, no hand-written matrices. A competent embedded engineer reading the Wires table plus the two CAN tables can state who talks to whom, over what, and what breaks when a given element fails. The honest comparison is not "100 entries vs zero" but "100 entries vs a DBC set, a SOME/IP service catalogue, a UDS routing table, and the gateway translation code between them."

---

## Devices (reference)

| Device | Endpoint Domains | Link interfaces | Role summary |
|---|---|---|---|
| Central compute SoC | `CoreNet`, `VehicleControl`, `DiagOTA`, `Telemetry` | `CoreSHM`, `BackboneEth` | 0x01–0x04; vehicle control, diagnostics, telemetry; forwards 5 Wires |
| Telematics ECU | `TcuNet`, `CloudServices` | `TcuSHM`, `BackboneEth` | 0x05–0x06; cloud path; no actuation authority |
| Zone controller SoC | `ZoneNet`, `ZoneControl`, `ZonePlatform` | `ZoneSHM`, `BackboneEth`, `PowerFD`, `BodyCAN11`, `LegacyAuxCAN` | 0x07–0x09; local authority (`MainA` on body bus); forwards 4 Wires |
| `SmartPDU`, `ThermalPump`, `SensorHub`, `ActuatorHub` | one each | `PowerFD` | 0x0A–0x0D; native WS smart ECUs, 29-bit addressing |
| `DoorGateway`, `WiperECU`, `AccessECU`, `LightingGateway`, `HVACGateway` | one each | `BodyCAN11` (+ LIN on three) | 0x0E–0x12; Node0–Node4; three also gateway LIN peripherals |
| `AuxSenseGW`, `WasherECU` | one each | `LegacyAuxCAN` | 0x13–0x14; Guest WS on a legacy bus |
| LIN peripherals (10) | none | `DoorLIN`, `LightingLIN`, `HVACLIN` | Conventional peripherals behind gateway Participants |
| DevPC | `DevPC` | `BackboneEth` | 0x15; Config B only; service access through `DiagOTA` |

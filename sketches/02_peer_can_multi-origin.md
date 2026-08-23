# Sketch 02 — Peer CAN Cell (Multi-Origin)

**Intent:** Exercise symmetric peer communication on a single Classical CAN bus under the **proposed** multi-Origin Wire model and deployment-global `ParticipantId`. Four moderately capable ECUs publish state, command one another, and broadcast cell-wide mode changes — with no designated bus coordinator. Stress whether one multi-Origin Wire is a natural fit for peer-equal traffic, how permitted-Origin policy compares to multiple single-Origin Wires, and whether global identity removes per-bus NodeId awkwardness.

**Protocol basis:** Phase 1 sketch written against premises in `RFP/remap_brief_multi_origin.md` (global `ParticipantId`, per-interaction Origin, `OriginToParticipant` / `ParticipantToOrigin` directions). **Not** mapped against current `CORE` single-Origin-per-Wire semantics.

**Archetype:** Four STM32-class ECUs on one committed Classical CAN segment inside a small machine cell. A line-level gateway may exist elsewhere in the factory, but this sketch models only the four peers and their shared bus.

---

## Executive summary

```text
**Mapping:** One multi-Origin Wire (PlantCellBus) carrying all peer traffic; global ParticipantId
    (P10–P13); permitted-Origin policy allows all four members; carrier-local address projection
    owned by Organizer (not designed here).
**Net effect (vs hypothetical single-Origin CORE):** Simpler — one Wire replaces a multi-Wire or
    coordinator-faking decomposition for the same peer semantics.
**Wires:** 1 (PlantCellBus); 1 requires >1 Origin; multi-Origin role: primary (all application
    traffic).
**Authority:** Hypothetical single-Origin would need ~4 command-authority Wires or an artificial
    coordinator Origin → 1 authored policy rule (all members may originate) after remap;
    generated projection: per-Service TX bindings from declarations.
**Attribution:** Global identity: stable ParticipantId across future gateway attachment;
    Multi-Origin: peer commands and broadcasts on one Wire; Both: interaction table clarity.
**Worst friction:** Configuration burden — Mild (single Wire, explicit policy row).
**Main lesson:** Peer-equal CAN is the case multi-Origin is built for; remaining cost is
    authorized policy and Service bindings, not Wire proliferation.
**Configs:** A (single configuration).
```

---

## Config A — Four-peer machine cell

### What changed

- Baseline for this sketch; no prior config in this file.
- Four ECUs on one Classical CAN bus; no gateway in scope.
- Each ECU is one Endpoint Domain with one global `ParticipantId`.
- All four may initiate commands, publish autonomously, and (where authorized) broadcast.

### Maturity level

**Level 1–2.** Nodes are commissioned with stable `ParticipantId` and bus-local address projection before field use. Organizer assigns Wire membership and permitted-Origin policy; export to static configuration is expected after bench validation (`DEPLOY §1.9`). Anonymous `kLocalBus` is not the steady-state model for this cell.

---

## Native communication model

```text
Four ECUs share one Classical CAN bus in a small machine cell:

  P10 — Drive A       publishes drive status (speed, torque, faults);
                      accepts motion setpoints from peers;
                      may command P12 (interlock outputs) directly.

  P11 — Drive B       same pattern as P10; may coordinate with P10 for paired moves.

  P12 — I/O rack      publishes digital input image and output feedback;
                      accepts output commands from any peer;
                      asserts safety interlocks locally.

  P13 — Sequencer     runs local sequence logic; publishes cell mode and step index;
                      commands P10, P11, P12 (moves, outputs, mode);
                      accepts manual overrides from an attached local HMI task on the same MCU;
                      may broadcast cell-wide ESTOP-clear / mode-change announcements.

Traffic patterns (all routine in normal operation):

  - Each ECU publishes its own status at a fixed rate (tens of ms).
  - Any peer may send a command PDU to one other peer (unicast).
  - Commands that expect a reply use request/response at the Service layer; the reply returns
    on the same Origin/Participant relation (Origin stays the initiator).
  - P13 may broadcast a mode or enablement change to all peers.
  - P10 and P11 occasionally command each other for synchronized motion (peer-to-peer, not
    routed through P13).
  - Firmware update is infrequent maintenance: one peer acts as updater, one as target; other
    peers may observe status but are not addressed.

There is no bus master, no CAN-level coordinator, and no ECU whose absence makes the others
unable to exchange peer traffic — though loss of P13 removes sequence authority, not CAN
reachability.
```

---

## Obvious conventional implementation

**Obvious conventional implementation:** Fixed CAN identifier matrix per message type and source/destination; each ECU filters on IDs it cares about; broadcast IDs for cell-wide announcements; application-level request/response correlation (sequence byte or transaction id in payload).

**What additional conceptual objects does WS introduce compared with this?**

Under the **proposed** model: one Wire, four global `ParticipantId`s, a permitted-Origin policy row, per-Service TX bindings, and Organizer-maintained bus-local address projection. Compared with raw CAN, WS adds explicit Wire membership and authorization layers; compared with a **hypothetical single-Origin CORE** mapping of the same system, WS multi-Origin removes several artificial Origins or extra Wires that would otherwise stand in for peer command authority.

---

## WireSpaces mapping

### Topology — wire-centric view

```text
PlantCellBus (Wire 42) — Classical CAN, committed profile
  Members:     P10 Drive A, P11 Drive B, P12 I/O rack, P13 Sequencer
  Origins:     all four (permitted-Origin policy)
  Realization: one Physical Link, four Link Interfaces

        ┌─────────┐
        │  P13    │  Sequencer
        │ Seq     │
        └────┬────┘
             │
    ┌────────┼────────┐
    │   Classical CAN  │
    │   PlantCellBus   │
    └─┬──────┬──────┬──┘
      │      │      │
  ┌───┴──┐ ┌─┴───┐ ┌┴────┐
  │ P10  │ │ P11 │ │ P12 │
  │DriveA│ │Drv B│ │ I/O │
  └──────┘ └─────┘ └─────┘
```

### Topology — device-centric view

```text
Each ECU: one Endpoint Domain, one Link Interface (CAN), ParticipantId as below.

  Drive A (P10)          Drive B (P11)
    Domain P10             Domain P11
    CAN IF ──┐             CAN IF ──┐
             │                      │
             └──── PlantCellBus ────┘
                      │
             ┌────────┴────────┐
             │                 │
  I/O rack (P12)        Sequencer (P13)
    Domain P12             Domain P13
    CAN IF                 CAN IF
```

No gateway forwarding table — all four Link Interfaces are on the same Wire and the same Physical Link.

### Participants

| Device | Endpoint Domain | ParticipantId | Link interfaces | Role summary |
|---|---|---:|---|---|
| Drive A MCU | DriveA | **10** | CAN ×1 | Motion actuator + status publisher |
| Drive B MCU | DriveB | **11** | CAN ×1 | Motion actuator + status publisher |
| I/O rack MCU | IoRack | **12** | CAN ×1 | Digital I/O + interlock outputs |
| Sequencer MCU | Sequencer | **13** | CAN ×1 | Cell sequence + broadcast authority |

Tooling may group all four under one cell device identity for maintenance history; routing uses `ParticipantId`.

### Wires — application

| Wire (name / #) | Origins needed | System role | Members | Physical realization | Notes |
|---|---:|---|---|---|---|
| **PlantCellBus** / 42 | **4** | **primary** | P10, P11, P12, P13 | One Classical CAN segment | Multi-Origin; all members permitted to originate |

### Permitted-Origin policy

| Wire | Members | Permitted Origins | Endpoint-level restrictions beyond that | Authored or generated? |
|---|---|---|---|---|
| PlantCellBus | P10, P11, P12, P13 | **All four members** | Each Service declares which peer targets and broadcasts its TX bindings allow; MotionCommand and FirmwareUpdate require explicit target Participant; SafetyInterlock command limited to P10/P11/P13 → P12 | **Generated** from cell topology declaration + Service manifests; user states "symmetric peer cell," not four manual Origin rows |

**Policy audit question:** Is one generated "all members may originate" rule simpler than four single-Origin Wires each existing only so one peer could command the others? **Yes** for this archetype — the policy matches the native model directly.

### Interactions

| Interaction | Origin | Participant (target) | Direction | Producer Service | Consumer Service | Notes |
|---|---:|---:|---|---|---|---|
| Drive A status publish | 10 | 10 | ParticipantToOrigin | DriveStatus (Snapshot TX) | — | Autonomous telemetry; all peers may observe if configured |
| Drive B status publish | 11 | 11 | ParticipantToOrigin | DriveStatus | — | Same |
| I/O image publish | 12 | 12 | ParticipantToOrigin | IoImage | — | Same |
| Sequencer mode publish | 13 | 13 | ParticipantToOrigin | CellMode | — | Same |
| Sequencer → Drive A move | 13 | 10 | OriginToParticipant | MotionCommand | MotionHandler | Request; reply below |
| Drive A move response | 13 | 10 | ParticipantToOrigin | MotionHandler | MotionCommand | Reply binding: request-scoped; Origin stays 13 |
| Drive A → I/O interlock | 10 | 12 | OriginToParticipant | InterlockCmd | InterlockHandler | Direct peer command |
| Drive A ↔ Drive B sync | 10 or 11 | 11 or 10 | OriginToParticipant | SyncMotion | SyncMotion | Either may initiate independently |
| Cell mode broadcast | 13 | *broadcast* | OriginToParticipant | CellMode | CellMode | Fan-out; broadcast scoped to Wire members |
| Maintenance FW update | *updater* | *target* | OriginToParticipant | FirmwareUpdate | FirmwareUpdate | Updater Participant authorized by maintenance binding; rare |

*Broadcast participant encoding deferred (addressing/layout work). Scope: all configured members of PlantCellBus (`WIRE-4` equivalent under proposal).*

### Services / Endpoints (topology-relevant)

| Service | Endpoint (NS/EID) | Typical binding | Storage | Notes |
|---|---|---|---|---|
| Health | NS0 / EID 2 | autonomous TX on PlantCellBus | Snapshot | All four |
| DriveStatus | user / snapshot | autonomous TX | Snapshot | P10, P11; latest-value semantics |
| IoImage | user / snapshot | autonomous TX | Snapshot | P12 |
| CellMode | user / snapshot | autonomous TX | Snapshot | P13; broadcast-capable |
| MotionCommand | user / command | TX: explicit target Participant; RX: request handler | Queue | P10, P11, P13; reliable segment for setpoints |
| InterlockCmd | user / command | TX: P10,P11,P13 → P12 only | Queue | Policy restricts destinations |
| FirmwareUpdate | NS0 common | TX: maintenance role | Queue | Reliable segment; updater/target bound |

**Learned-from-ingress** is **not** used on this shared CAN bus (`CORE §10.6` equivalent). Reply authority comes from request-scoped bindings, not from reversing Direction.

### Carrier-local address projection

The Organizer maintains a deterministic **bus-local compact address** for each `ParticipantId` on PlantCellBus. ECU firmware uses the projection table generated at commission time; semantic routing uses `ParticipantId` and per-interaction Origin. Projection format and CAN ID layout are **out of scope** (deferred addressing work). Assume four members fit comfortably.

---

## Hypothetical comparison — single-Origin CORE (not adopted)

*This section records what would likely be required under current `CORE` single-Origin-per-Wire semantics, for experiment context. It is not the mapping above.*

A faithful single-Origin mapping of the same native model typically forces one of:

```text
A) One coordinator Origin (e.g. P13) on one Wire — peer commands from P10/P11 become awkward
   (wrong structural authority or observation-only workarounds).

B) Multiple Wires differing only by which peer is Origin — Wire proliferation; same physical
   bus, several Wire identities; broadcast scope split or duplicated.

C) Application-layer ad-hoc source checks — authority moved into Service code.
```

### Wire accounting (hypothetical removal of extra Wires)

If a single-Origin sketch carried separate command Wires per initiating peer, each removed Wire would be accounted as follows:

| Removed Wire (hypothetical) | Route scope now | Broadcast scope now | Command authority now | Failure distinction now |
|---|---|---|---|---|
| CmdWire_P10 | PlantCellBus (same CAN) | PlantCellBus broadcast | Permitted-Origin + TX bindings | Participant-specific health, not Wire-specific |
| CmdWire_P11 | PlantCellBus | PlantCellBus broadcast | Permitted-Origin + TX bindings | Same |
| CmdWire_P13 | PlantCellBus | PlantCellBus broadcast | Permitted-Origin + TX bindings | Same |

Under multi-Origin, **route scope** and **broadcast scope** were always one CAN segment; only **command authority** was artificially split.

---

## Friction signals

Ratings for the **proposed multi-Origin mapping** (happy path).

| Signal | Rating | Justification |
|---|---|---|
| **Artificial Origin** | **None** | No peer pretends to be bus coordinator; Origin names the actual initiator per PDU. |
| **Artificial Wire** | **None** | One Wire matches one physical CAN segment and one membership set. |
| **Wire proliferation** | **None** | Four peers do not require four (or more) Wires for direction/authority. |
| **Forwarding tax** | **None** | No gateway; no forwarding. |
| **Identity awkwardness** | **Mild** | Global `ParticipantId` is stable and meaningful; bus-local projection is invisible machinery until commission/export — Organizer owns it. |
| **Interaction awkwardness** | **Mild** | Interactions must name Origin explicitly; request/reply keeps initiator's Origin on responses — familiar once tabulated, slightly more columns than raw CAN IDs. |
| **Configuration burden** | **Mild** | One Wire + generated permitted-Origin policy + Service TX bindings; less than multi-Wire single-Origin alternatives. |
| **Role instability** | **Mild** | Origin changes per interaction by design; operators must understand "who initiated this relation" rather than "who is the Wire Origin" — auditable via PDU fields. |
| **Failure mismatch** | **Mild** | P13 loss is "sequence authority unavailable," not "Wire Origin offline"; CAN peer traffic among P10/P11/P12 can continue — clearer than conflating roles, but diagnostics vocabulary shifts. |

**Hypothetical single-Origin CORE (same system):** Artificial Origin **Significant**, Wire proliferation **Significant**, Interaction awkwardness **Significant** — included for contrast only.

---

## Model pressure

**If you could change one WireSpaces concept** to make this system simpler, what would you change?

Possibly collapse permitted-Origin policy into a Wire **profile** for symmetric peer cells ("all members originate") so the policy row does not need to be stated separately from Wire kind — a documentation convenience, not a topology change.

**Did WS expose a useful distinction** that the conventional model tends to obscure?

Yes: separating **interaction initiator** (Origin on this PDU) from **Wire membership** and from **carrier-local address**. Raw CAN conflates destination encoding with identity; multi-Origin makes initiator, target, and broadcast scope explicit while keeping one bus.

---

## Degraded-state notes (§5.1-style)

| Condition | What operators/tooling report | Traffic that continues |
|---|---|---|
| P13 offline | P13 unavailable; cell sequence and mode broadcast origin lost | P10↔P11 sync, P10→P12 interlock, status publishes among remaining peers |
| P10 offline | Drive A unavailable | Other peers unchanged on CAN |
| CAN fault | Link fault on PlantCellBus | None on this segment |

Role reassignment is **not** automatic: no peer becomes "the Wire Origin" because there is none. Sequencer functions require P13 or manual intervention on another authorized peer.

---

## Observation accounting

| Path | Treatment |
|---|---|
| P11 consumes P10 DriveStatus without being addressed | Configured **observation** on PlantCellBus — unchanged semantics; not membership; multi-Origin does not convert observation to addressing |
| FW update status visible to non-target peers | Observation or explicit status Endpoint; not broadcast command authority |

Observation paths **retained**; none removed by multi-Origin collapse.

---

## Open questions

1. **Symmetric peer profile:** Should WireSpaces define a standard "peer cell" Wire kind (all members originate) to avoid repeating the permitted-Origin policy row in every sketch?
2. **Broadcast encoding:** How is `Participant = broadcast` represented on Classical CAN without colliding with unicast projection — deferred layout work.
3. **Intensive concurrent commands:** When P10 and P13 both command P11 in the same epoch, ordering is Service/Transport concern; Wire model does not serialize — is that obvious enough in tooling?
4. **Future line gateway:** If PlantCellBus later bridges to a line Wire via a fifth ECU, do P10–P13 `ParticipantId`s stay stable while bus-local addresses are re-projected on each segment?
5. **Policy exceptions:** InterlockCmd restricted to P10/P11/P13 → P12 is Endpoint-level policy; confirm tooling can express destination allowlists without per-peer Wires.

---

## End-of-sketch summary

```text
Sketch:              02_peer_can_multi-origin (Config A)
Wires:               1
Wires requiring >1 Origin:  1
Multi-Origin by role:       primary=1, supporting=0, peripheral=0

Informative friction improved (vs hypothetical single-Origin):
    Artificial Origin, Wire proliferation, Interaction awkwardness

Informative friction regressed:
    Role instability (Mild — Origin per interaction vs fixed Wire Origin)

Informative friction unchanged:
    Identity awkwardness remains Mild (projection machinery)

Benefits attributable to Global identity:
    Stable P10–P13 across future gateway attachment; no per-Wire NodeId reassignment

Benefits attributable to Multi-Origin:
    One Wire for all peer commands and broadcasts; no coordinator fiction

Benefits attributable to Both / inseparable:
    Interaction table reads naturally with Origin + Participant columns

New authored authorization state introduced:
    Zero beyond "symmetric peer cell" declaration; policy generated

Existing structural authority removed:
    Hypothetical per-peer command Wires or fake coordinator Origin

Observation paths removed / retained / changed:
    Retained unchanged
```

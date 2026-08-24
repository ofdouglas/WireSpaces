# Sketch — Archetype 13: Hard-Real-Time Cyclic Motion + High-Rate Streaming

```text
**Archetype:** archetypes/13_cyclic_motion_and_streams.md
**Agent:** cursor_A
**Output file:** sketches/13_cyclic_motion_and_streams_cursor_A.md
**Variant mapped:** Config C, A48 (48 axes, 500 us cycle); A/B/D and A24/A64 as deltas
**Date:** 2026-08-23
```

---

## Executive summary

```text
**Minimum mapping:** 77 production Participants (78 in Config D), 6 Wires, 4 gateway
forwarding objects, 0 splices, 5 Link profile classes — one of which LINK does not have.
The cyclic motion exchange is deliberately NOT a Wire.
**Question A (R6):** Natural for 29 of 34 relation classes; silent on the other 5.
**Question B (CAN11):** N/A — no Classical CAN11 anywhere in this machine.
**Worst friction (minimum path):** Failure/topology mismatch — Significant.
**Main lesson:** The canonical header is priced by payload size, not by link type. At 7 bytes
it costs 46% overhead and 192,000 PDUs/s and WireSpaces cannot carry the cyclic core; at
1,536 bytes it costs 0.39% and carries a 1.84 MB/s stream without noticing. The honest
boundary is the drive's communication slot, not the drive — and WS still owns 100% of the
Participants and 85% of the relationships on the other side of it.
```

---

## 0. Falsification criteria

Written before the mapping, per archetype §1. Discharged in §13.

```text
I will judge WS UNSUITABLE for the cyclic core if ANY of:

  F1  Realizing 96 canonical PDUs per 500 us cycle as individually framed
      transfers on MotionBus exceeds 100 Mbit/s, OR exceeds the link's
      physical maximum frame rate.
  F2  The 6-byte canonical header exceeds 10% of MotionBus capacity at any
      specified variant (A24 / A48 / A64).
  F3  Making the exchange fit requires eliding 5 or more of the 6 canonical
      header bytes — i.e. WS contributes no bytes to the wire.
  F4  Per-PDU handling of the cyclic exchange costs more than 5% of one
      controller core on a Participant that also runs the motion algorithm.
  F5  WS has no representation for group-atomic application of 48 setpoints,
      AND no way for tooling to detect a Link that cannot provide it.
  F6  The +/- 1 us phase requirement cannot be met by any WS Service and must
      be a Link/hardware property below WS.

I will judge WS SUITABLE for the cyclic core if ALL of:

  S1  A canonical Wire carries the exchange with header overhead below 10% of
      cyclic traffic and below 2% of link capacity; AND
  S2  the canonical model contributes at least one field the Link profile does
      not statically imply; AND
  S3  jitter and group atomicity are expressible in the model, or at least
      checkable against a Link capability.

I will judge WS UNSUITABLE for continuous / bulk streams if ANY of:

  F7  Per-PDU header overhead exceeds 5% at a block size the consumer can
      actually schedule.
  F8  A declared Endpoint storage class loses stream data with no counter.
  F9  An 18 MB object cannot be transferred without a mechanism the model
      does not define.

I will judge WS SUITABLE for streams if ALL of:

  S4  Header overhead below 1% at a defensible block size; AND
  S5  a declared Endpoint storage class matches stream semantics, with a
      countable loss path; AND
  S6  segmentation, sequencing, and gap detection either exist, or are
      explicitly the Service's responsibility with a stated place to put them.
```

Both verdicts are numeric. A negative verdict without arithmetic would be worth as little as a positive one.

---

## Disposition block

| Area | Assessment |
|---|---|
| Participant identity | Natural |
| Wire decomposition | Natural |
| Forwarding | Simple |
| Non-CAN configuration | Low |
| CAN11 VCN fit | N/A |
| Better with CAN29? | N/A |

**Explanation (Question A — R6).** For everything that is not the cyclic core, R6 is the most economical result in the corpus so far: **78 Participants on 6 Wires with 4 forwarding objects and no splices**, and the two structures that usually cost the most — the 48-drive population and the 12-sensor daisy chain — cost one Wire each. Participant identity is entirely unremarkable, which is what the archetype predicted: the stress is temporal, not nominal. The one place the model is genuinely silent is the place the machine exists for. A Wire is a propagation scope (`CORE §3`) and explicitly guarantees no deadlines, ordering, or freshness (`CORE §3.6`); a Link profile owns the schedule; and there is **no capability field by which the Link can tell the model what schedule it owns** (`CORE §17`). That is correct layering for the mechanism and a real gap in the declaration — every hazard in §10 is invisible to the checks in `DEPLOY §2.3`.

**Explanation (Wire decomposition — Natural, with one deliberate omission).** Every one of the six Wires names a real communication scope, and no Wire was created to satisfy direction, Service identity, or bandwidth accounting. The candidate seventh Wire — `W_MotionCyclic`, spanning `CtrlSHM` and `MotionBus` for the 2 kHz exchange — is **rejected in the minimum mapping on arithmetic** (§3.2, §10.1–10.2), not deferred to optimizations. Declaring it would have been the artificial Wire in this sketch: a Logical Bus whose Link profile elides all six canonical header bytes, carrying 194,000 PDUs/s across shared memory to produce a frame in which no canonical field survives.

**Explanation (Non-CAN configuration — Low, and that is the uncomfortable part).** WS's own configuration inventory here is roughly 130 counted objects for 78 Participants, the lowest ratio in the corpus. But the configuration this machine actually needs is a 96-slot cycle schedule with static bit offsets and a jitter budget, and **none of it is WS configuration**. Rating the burden "Low" is accurate and slightly misleading at the same time: WS is cheap here partly because it declined the expensive job.

**Question B — CAN11.** Not applicable. The only CAN on this machine is CAN-FD at 2 Mbit/s on `IOBus_A`/`IOBus_B`, and the archetype excludes CAN11/VCN analysis explicitly. No alias, no VCN map, no Guest block, no `MainA`/`MainB` assignment appears anywhere. The CAN-FD buses are nevertheless the **only** place in this machine where the 2-bit QoS field does measurable work (§10.5), which is worth recording against a corpus where CAN usually appears as an addressing problem.

**Archetype Questions A–J are answered individually in §14.**

---

## 1. Native communication model

A high-speed converting and packaging line. Web material is unwound, printed, laminated, cut, and packed continuously. Dozens of servo axes follow an **electronic line shaft** — a virtual master position that each axis tracks in a fixed phase relationship. Phase error becomes registration error becomes scrap, so the coordination requirement is not "fast messaging" but **common-instant application of a computed vector**.

**The controller** is one machine with four execution domains sharing memory: `MotionCore` (line shaft, cam profiles, setpoint vector, phase supervision, motion state machine), `LogicCore` (sequence logic, I/O scan, interlocks, recipes), `CommsCore` (owns `MotionBus` and `MachineEth`, forwarding, Link status), `HmiCore` (operator model, alarms, production data).

**The motion network** is a time-scheduled industrial Ethernet of the summation / passing-frame class: the master emits one frame per cycle, the frame traverses every node in sequence, each node reads and writes its own slice **in flight** at a statically configured bit offset, and there is no per-node frame, no per-node address field in the cyclic path, and no arbitration. A small statically sized acyclic region rides the same cycle. 48 drives, the line encoder, and the safety controller hang off it — and **each of the 48 drives has exactly one Link**, so every Service they expose must arrive through a bus whose bandwidth is allocated to a fixed schedule.

**Distributed I/O** runs on two CAN-FD segments at 2 Mbit/s with a 2 ms scan and a ≤2 ms fast-event path that must reach both `LogicCore` and `MotionCore`.

**Two subsystems were bolted on later** and produce data at rates the control network was never designed for. Twelve triaxial vibration sensors stream 25.6 kHz × 3 channels × 16 bits continuously to an analyzer over a 100 Mbit daisy chain — 1.84 MB/s aggregate — and release a 10 s rolling raw buffer (1.5 MB each, up to 18 MB) on trigger. Two cameras deliver 3 MB mono frames at 4 fps each to a vision controller over Gigabit Ethernet — 24 MB/s — and the vision controller must return a registration correction to `MotionCore` within 50 ms.

**Degraded behavior is specified rather than emergent:** a faulted drive stops its section without spoiling other axes' phase lock; a missed cycle trips drives to a safe state within a bounded number of cycles; a stalled condition-monitoring stream must have zero effect on motion; a saturated Gigabit network must not endanger the motion or I/O buses.

---

## 2. Obvious conventional implementation

> **Obvious conventional implementation:** EtherCAT (or SERCOS III / POWERLINK) for the motion bus, with vendor PDO slot mapping generated from an ESI/XML device description and CoE/SDO mailbox transfers for drive parameters, cam tables, and firmware; distributed clocks for the ±1 µs phase reference; FSoE or CIP Safety as a black-channel safety layer inside the cyclic frame; CANopen over CAN-FD for the I/O islands, with its own object dictionary and EMCY error frames; OPC UA or a vendor PLC protocol on the Gigabit network for HMI and machine data; GigE Vision / GenICam for the cameras; a proprietary vendor protocol for the vibration sensors and their analyzer; RT-task shared memory inside the controller; and a separate firmware-update tool per device class.

> **What structure does a conventional design use?** Seven unrelated identity spaces. A drive is `index:subindex` in a CoE object dictionary; an I/O island is a CANopen node ID plus another object dictionary; a camera is a GenICam feature name; a vibration sensor is a vendor tag; an HMI value is an OPC UA `NodeId`; an internal signal is a struct offset in shared memory. "Health" is a CoE status object, a CANopen EMCY code, a GenICam device-status feature, a vendor UDP status packet, an OPC UA node, and a syslog line — six implementations of one concept, integrated by a systems engineer with a spreadsheet.

What WireSpaces offers this machine is a single identity and Service model across all 78 Participants, and — the part that actually matters — **a name for `Drive_17`'s Identity Service that is the same name the motion slot map refers to.** What it does not offer is the motion protocol, and the substance of this sketch is that it should not try. Two consequences run through everything below. First, the 48 drives are reachable *only* through the scheduled link, so the Service plane and the motion plane must share one configuration or the drives are unmanageable; that is a genuine argument for WS and it is conditional on a toolchain integration that does not exist yet. Second, the conventional design's worst property — seven catalogues — is a *relationship* problem, and 29 of this machine's 34 relation classes are relationships. The 5 that are not carry 90% of the frames.

---

## 3. Minimum mapping (required first)

### 3.1 Participants

Allocated in functional blocks with a reserved drive range, so A24 → A48 → A64 never renumbers anything.

| ParticipantId | Endpoint Domain | Device | Notes |
|---|---|---|---|
| 0x01 | `MotionCore` | MachineController | Line shaft, cam/gear profiles, setpoint vector, phase supervision, motion state machine |
| 0x02 | `LogicCore` | MachineController | Sequence logic, I/O scan, interlocks, recipes; owns `IOBus_A`/`IOBus_B` |
| 0x03 | `CommsCore` | MachineController | Owns `MotionBus` and `MachineEth`; the machine's only multi-Link forwarder |
| 0x04 | `HmiCore` | MachineController | Operator model, alarms, production data aggregation |
| 0x05 | `LineEncoder` | Master encoder | `MotionBus`-only |
| 0x06 | `SafetyController` | Safety chain master | `MotionBus`-only; dual-channel, 8 ms watchdog |
| 0x10–0x15 | `IoIsland_1` … `_6` | Distributed I/O | 40 DI + 8 AI each; 1–3 on `IOBus_A`, 4–6 on `IOBus_B` |
| 0x20–0x2B | `VibSensor_01` … `_12` | Triaxial accelerometers | Config B+; `VibChain`-only |
| 0x30 | `CmAnalyzer` | CM analyzer | Config B+; on `VibChain` and `MachineEth` |
| 0x31 | `VisionController` | Print inspection | Config C+ |
| 0x32 / 0x33 | `Camera_A` / `Camera_B` | 2048×1536 mono, 4 fps | Config C+ |
| 0x34 | `HmiPanel` | Operator terminal | |
| 0x35 | `RemoteGateway` | Remote support | **Config D only** |
| **0x40–0x7F** | **drive block (64 reserved)** | | A48 uses 0x40–0x6F |
| 0x40–0x6F | `Drive_01` … `Drive_48` | Servo drives | `MotionBus`-only; one Link each |
| 0x70–0x7F | *(reserved)* | A64 growth | Unused at A48 |

**Production totals: 61 (Config A) / 74 (B) / 77 (C) / 78 (D)** — matching the archetype. Highest value used is `0x7F` at A64, leaving 127 ordinary identities spare in the current 8-bit allocation (`CORE §2.1`). No extended addressing is needed or considered.

Two identity decisions are worth stating because both were nearly wrong.

**The four controller domains are four Participants, not one.** They have separate dispatch and authority boundaries and separate Link ownership, which is the test in `CORE §1.5`. Collapsing them would make `CommsCore`'s forwarding invisible and would put the motion algorithm and the Ethernet stack in one concurrency scope.

**A drive's servo loops are not Participants.** Current, velocity, and position inner loops are local by requirement, and nothing about them is network-visible. One drive is one Endpoint Domain regardless of how many cores it contains — the same rule that kept lockstep replicas invisible in archetype 11.

### 3.2 Wires

Six Wires. The membership column names the **Link on which each Participant binds the Wire**, because two Participants have two interfaces and that distinction is load-bearing.

| Wire (#) | Member Links | Participants (bind interface) | Purpose |
|---|---|---|---|
| W1 `IoA` | `IOBus_A`, `CtrlSHM` | `IoIsland_1..3` (CAN-FD); `LogicCore` (both, forwarder); `MotionCore` (SHM) | 2 ms scan, output commands, fast events for section A |
| W2 `IoB` | `IOBus_B`, `CtrlSHM` | `IoIsland_4..6` (CAN-FD); `LogicCore` (both, forwarder); `MotionCore` (SHM) | Section B equivalent |
| W3 `Plant` | `CtrlSHM`, `MachineEth` | `MotionCore`, `LogicCore`, `HmiCore` (SHM); `CommsCore` (both, forwarder); `VisionController`, `CmAnalyzer`, `HmiPanel` (Eth) | Machine control plane: mode/state, registration correction, defect/reject, alarms, CM summaries and diagnosis, production data, Link status, wall-clock time |
| W4 `Bulk` | `MachineEth` | `Camera_A/B`, `VisionController`, `CmAnalyzer`, `HmiCore` (Eth), `HmiPanel`, `RemoteGateway` (D) | Image frames, defect image archive, vibration dump archive, logs, firmware for Ethernet devices |
| W5 `VibStream` | `VibChain` | `CmAnalyzer`, `VibSensor_01..12` | Continuous raw stream, envelope/RMS, triggered dumps, sensor Services (Config B+) |
| W6 `DriveService` | `MotionBus`, `CtrlSHM`, `MachineEth` | `Drive_01..48`, `LineEncoder`, `SafetyController` (MotionBus); `MotionCore`, `LogicCore`, `HmiCore` (SHM); `CommsCore` (all three, forwarder); `HmiPanel`, `RemoteGateway` (Eth) | Everything the MotionBus population exposes: Identity, Version, Health, fault detail, parameters, cam tables, logs, firmware, machine mode to drives |

Config A has five Wires (`W5` appears with condition monitoring). No configuration adds a Wire after B.

Five decisions do the work, and three of them are forced.

**(a) `CtrlSHM` and `MachineEth` form a physical loop, so exactly one domain may forward between them.** Both `CommsCore` and `HmiCore` sit on both links, so any Wire whose realization includes both links and both domains as forwarders contains a cycle and is invalid (`CORE §3.3`, §12.4). The resolution is not to remove a link but to bind per interface: **`CommsCore` is the sole `CtrlSHM`↔`MachineEth` forwarder, and `HmiCore` binds each Wire on exactly one of its two interfaces** — `CtrlSHM` for `W3 Plant`, `MachineEth` for `W4 Bulk`. `CORE §12.6` makes this legitimate rather than a trick: electrical visibility is not membership, so `HmiCore`'s unbound interface simply ignores that Wire's frames. This is the same physical-loop pressure archetype 11 found, in a much smaller topology, and it resolved without a splice.

**(b) The two CAN-FD segments are two Wires, because flood-and-filter makes one Wire unusable.** A single `MachineIO` Wire spanning `IOBus_A` and `IOBus_B` would have `LogicCore` forward section A's scan traffic onto section B and vice versa. Each segment already runs at **38.4% utilization** for its own 2 ms scan (§11, timing ledger); doubling it lands at 77% before fast events, Health, and diagnostics, which pushes the ≤2 ms fast-event budget past its deadline. So two Wires. This is a scope decision the topology justifies anyway — the segments are separate failure boundaries — but the arithmetic is what forces it, and it is the fourth independent instance in this sketch of destination-pruned egress being a viability requirement rather than an optimization (SF-R6-013).

**(c) `W6 DriveService` deliberately spans a 1 Gbit link and a 512 kB/s scheduled channel, and this is the sharpest structural choice in the mapping.** Archetype §10.6 poses it as a trap and it is one. The alternatives were composition at `CommsCore` (a proxy for 48 devices, destroying canonical source — the corpus's strongest repeated result) and a splice from a MachineEth-side Wire to a MotionBus-side Wire. I chose one spanning Wire with destination-pruned egress, because a splice would add a WireNumber and a projection between two scopes with *identical* membership semantics, which is configuration for no scope difference (`CORE §6.1` — a splice exists to change scope, and here there is no scope to change). The cost is stated plainly: **destination-pruned egress at `CommsCore` is mandatory for safety, not efficiency**, and `CommsCore` must additionally meter admitted traffic into the 256 B/cycle acyclic region. The full arithmetic is §10.6; the short version is that one unpruned 1500-byte frame occupies 120 µs of a 500 µs cycle and a 4 MB firmware image occupies 25.2 seconds of continuous link time.

**(d) No Wire spans `VibChain` and `MachineEth`.** `CmAnalyzer` has both interfaces but forwards nothing. A Wire spanning both would let 24 MB/s of image traffic onto a 100 Mbit chain carrying a 1.84 MB/s continuous stream — a 2× oversubscription of a link whose stream cannot be re-requested. The cost, stated: a triggered raw dump reaches `RemoteGateway` only as a `CmAnalyzer`-**composed** archive record, so `Src = VibSensor_07` is lost at the archive boundary and provenance survives only in the Service payload. That is exactly the "reconstructed by convention" outcome the corpus keeps flagging, and here I accepted it deliberately because the alternative endangers the stream.

**(e) `W_MotionCyclic` is rejected — the cyclic exchange is not a Wire.** This is a scope decision and it belongs in the minimum mapping rather than in optimizations. The candidate Wire would span `CtrlSHM` and `MotionBus` with `CommsCore` forwarding, carrying 48 setpoints, 48 feedbacks, the line reference, and the safety chain. Four numbers kill it:

```text
individually framed on MotionBus   129 Mbit/s offered on a 100 Mbit/s link, and
                                   192,000 frames/s against a 148,809 frames/s
                                   physical ceiling                      (10.1)

canonical header cost              46.2% of cyclic traffic; 9.2% of the whole
                                   link at A48, 24.6% at A64             (10.2)

fields the profile must elide      6 of 6 header bytes — 100%            (10.2)

per-PDU handling across CtrlSHM    194,000 PDUs/s = ~9.7% of one core at A48,
                                   ~25.8% at A64, on the core that also runs
                                   48 cam evaluations per cycle          (10.1)
```

The last one is the argument that generalizes past this machine. Preserving canonical source across the shared-memory hop costs 194,000 router lookups and bounded copies per second — **and buys nothing observable**, because the summation frame has no source field and the receiving drive reconstructs `Src = MotionCore` from its slot map no matter who assembled the frame. On a totally-eliding link, canonical identity is a configuration fact, not a carried one, and paying per-PDU to preserve it is paying for a property the wire cannot express.

**Rejected outright:** a device-private `CtrlCore` Wire for intra-controller traffic (its ~5 kB/s would flood a 1 Gbit link at 0.004% cost — it does not earn a Wire, so `W3 Plant` absorbs it); one Wire per Service; a Wire per camera; separate Wires for setpoint and feedback directions; making each daisy-chained `VibSensor` a WS forwarder (see §3.5).

### 3.3 Interactions (primary)

**Below the WS boundary — carried by the MotionBus schedule, not by a Wire.** Listed because the archetype requires them to be accounted for, not because they are WS interactions.

| Interaction | Src | Dest | Rate | Payload | Realization |
|---|---|---|---|---|---|
| Axis setpoint | `MotionCore` | each drive | 2 kHz | ~7 B | Static slot in the cyclic frame's setpoint image |
| Axis feedback | each drive | `MotionCore` | 2 kHz | ~7 B | Static slot, written in flight |
| Line reference | `LineEncoder` | `MotionCore` | 2 kHz | ~6 B | Static slot |
| Cycle tick / phase | reference source | all MotionBus nodes | 2 kHz | — | Distributed clocks, ±1 µs; hardware (§14, Question D) |
| Safety chain | `SafetyController` ↔ drives | | 125 Hz | ~16 B opaque | Reserved 96 B/cycle region; full 48-drive round in 8 ms |

**Machine logic and I/O (W1 `IoA`, W2 `IoB`)**

| Interaction | Src | Dest | Wire | Broadcast? | Notes |
|---|---|---|---|---|---|
| Digital + analog input scan | each `IoIsland` | `LogicCore` | its Io Wire | No | 2 ms cyclic; 21 B; QoS 1 |
| Output command | `LogicCore` | each `IoIsland` | its Io Wire | No | 2 ms cyclic; 5 B; QoS 1 |
| Fast input event | `IoIsland` | kBroadcast | its Io Wire | Yes | Registration mark, reject sensor; ≤2 ms; **QoS 0**; both `LogicCore` and `MotionCore` consume |
| I/O Health / fault | each `IoIsland` | kBroadcast | its Io Wire | Yes | 1 Hz + event |
| Identity / Version / firmware | `LogicCore` ↔ `IoIsland` | | its Io Wire | No | Service ops; QoS 2/3 |

Fast events are **forwarded** to `MotionCore` over `CtrlSHM`, not composed by `LogicCore`. Composition would substitute `Src = LogicCore` for `Src = IoIsland_2` on a registration-mark event that `MotionCore` uses to correct phase — losing exactly the provenance the correction depends on. Forwarding costs 3 × 21 B × 500 Hz ≈ 31.5 kB/s onto shared memory, which is ample.

**Machine control plane (W3 `Plant`)**

| Interaction | Src | Dest | Broadcast? | Notes |
|---|---|---|---|---|
| Machine state / mode | `LogicCore` | kBroadcast | Yes | 10–100 Hz + event; QoS 1 |
| Registration correction | `VisionController` | `MotionCore` | No | 4–20 Hz, ≤50 ms; **QoS 0** |
| Defect / reject signal | `VisionController` | `LogicCore` | No | Event, ≤20 ms; QoS 0 |
| Envelope / RMS summary | each `VibSensor` | kBroadcast | Yes | 10 Hz; small; forwarded by nothing — `CmAnalyzer` composes the machine-facing summary |
| CM diagnosis / trend | `CmAnalyzer` | kBroadcast | Yes | 0.1–1 Hz; composed |
| Alarm / fault summary | `HmiCore`, `LogicCore` | kBroadcast | Yes | Event; QoS 0/1 |
| Production data | `LogicCore`, `HmiCore` | `HmiPanel` | No | 1–10 Hz |
| Link status | `CommsCore`, `CmAnalyzer`, `VisionController` | kBroadcast | Yes | 1 Hz |
| Wall-clock time | `LogicCore` | kBroadcast | Yes | 1 Hz; **not** the phase reference (§14, Question D) |

The envelope/RMS relation is the one place where a Wire boundary forces a choice: the sensors are on `VibChain` and the consumers are on `MachineEth`/`CtrlSHM`. Because no Wire spans those links, `CmAnalyzer` composes the machine-facing summary under its own ParticipantId. That is correct — the analyzer is authoring an aggregate, not relabelling twelve publications — and it is cheap, unlike the dump-archive case in §3.2(d) where the same boundary costs real provenance.

**Bulk (W4 `Bulk`)**

| Interaction | Src | Dest | Notes |
|---|---|---|---|
| Image frame | each camera | `VisionController` | 4 fps each, ~3 MB; 2,161 PDUs/frame; **QoS 3** |
| Defect image archive | `VisionController` | `HmiCore`, `RemoteGateway` | 3 MB per record; QoS 3 |
| Vibration dump archive | `CmAnalyzer` | `HmiCore`, `RemoteGateway` | Composed; up to 18 MB; QoS 3 |
| Logs / event history | all Ethernet Participants | `HmiCore`, `RemoteGateway` | On demand; QoS 3 |
| Firmware (Ethernet devices) | `RemoteGateway`, `HmiCore` | cameras, `VisionController`, `CmAnalyzer` | ~MB; QoS 3 |

**Condition monitoring (W5 `VibStream`)**

| Interaction | Src | Dest | Notes |
|---|---|---|---|
| Raw vibration stream | each `VibSensor` | `CmAnalyzer` | 1,536 B blocks, 100/s per sensor, 1,200/s total; QoS 2 |
| Triggered raw dump | each `VibSensor` | `CmAnalyzer` | 1,000 chunks per sensor, 12,000 total; QoS 3 |
| Envelope / RMS | each `VibSensor` | `CmAnalyzer` | 10 Hz; QoS 2 |
| Sensor Identity / Health / firmware | `CmAnalyzer` ↔ each sensor | | QoS 2/3 |

**Drive and MotionBus Services (W6 `DriveService`)** — all of this shares one 256 B/cycle acyclic region.

| Interaction | Src | Dest | Notes |
|---|---|---|---|
| Machine mode to drives | `LogicCore` | kBroadcast | 10–100 Hz; **QoS 1**; the one production-critical relation on the acyclic channel |
| Drive fault detail | each drive | `LogicCore`, `HmiCore` | Event, hundreds of bytes; QoS 0 |
| Identity / Version | `CommsCore`, `HmiPanel`, `RemoteGateway` ↔ each drive | | Startup and on demand; QoS 2 |
| Health | each drive | kBroadcast | 1 Hz; QoS 2 |
| Parameter / cam table | `MotionCore`, `RemoteGateway` ↔ each drive | | Tens of kB; QoS 2 |
| Firmware update | `RemoteGateway`, `HmiCore` → each drive | | ~4 MB; **QoS 3**; maintenance window (§10.3) |
| Logs | each drive | on request | QoS 3 |
| Link status | `CommsCore` | kBroadcast | 1 Hz |

Note what is **not** here: no drive receives another drive's feedback, no image crosses `MotionBus`, and the analyzer subscribes to no motion data — all as the archetype requires, and all achieved by Wire membership rather than by filtering.

**Relation count (Config C, A48): 34 relation classes**, of which 5 are below the WS boundary and 29 are WS relations.

### 3.4 Forwarding and composition

| Forwarder | Wire | Branches | Local delivery? | Notes |
|---|---|---|---|---|
| `LogicCore` | W1 `IoA` | `IOBus_A` ↔ `CtrlSHM` | Yes | Fast events and scan reach `MotionCore` with source preserved |
| `LogicCore` | W2 `IoB` | `IOBus_B` ↔ `CtrlSHM` | Yes | |
| `CommsCore` | W3 `Plant` | `CtrlSHM` ↔ `MachineEth` | Yes | Small, low rate |
| `CommsCore` | W6 `DriveService` | `CtrlSHM` ↔ `MachineEth` ↔ `MotionBus` | Yes | **Destination-pruned egress mandatory**; admission control into the acyclic region |

**4 Wire→LinkBitmask objects, 0 splices.** For 78 Participants on 6 physical links this is the leanest forwarding state in the corpus, and it is a genuine R6 result: the drives, the sensors, and the cameras all needed exactly one Wire each and no identity translation.

Composition points (5): `CmAnalyzer` × 2 (machine-facing CM summary; dump archive record), `HmiCore` (production data aggregation), `CommsCore` (Link status rollup), `LogicCore` (alarm summary). Each authors new traffic under its own ParticipantId (`CORE §12.7`). Only one of the five costs identity that a reviewer should care about — the dump archive — and it is costed in §3.2(d).

### 3.5 Link profiles

| Physical link | Profile class | WS Wire(s) | Notes |
|---|---|---|---|
| `CtrlSHM` | Shared memory / FIFO (`LINK §8`) | W1, W2, W3, W6 | Simplest class; flow control a strong candidate |
| `IOBus_A` / `IOBus_B` | CAN-FD (`LINK §4`, **approach undecided**) | W1 / W2 | 2 Mbit/s, 29-bit ID, direct canonical addressing; QoS→arbitration per `LINK §2.2` |
| `MachineEth` | Switched Ethernet (`LINK §6`, **undeveloped**) | W3, W4, W6 | Aggregation candidate; jumbo frames optional (§10.4) |
| `VibChain` | Switched Ethernet, daisy-chain realization | W5 | Integrated 2-port switches are **below** the LLL (see below) |
| `MotionBus` | **Scheduled summation / passing-frame — does not exist in `LINK`** | W6 (acyclic region only) | See the elision table below and SF-R6-023 |

**5 profile classes, one of which `LINK` does not have.** Four of the five are named-but-undeveloped in `LINK §1`; the fifth is not named at all.

**`VibChain`'s daisy chain is one Physical Link, not twelve forwarders.** Each sensor's integrated 2-port switch is Link-layer machinery, exactly like the external switch on `MachineEth`, and `CORE §1.1` explicitly permits a bus-like Physical Link. Modelling it the other way would have produced 12 Wire→LinkBitmask objects for a chain with no routing decisions in it. The consequence is worth naming: the chain's accumulated per-hop latency — up to **1.52 ms** for the farthest sensor under store-and-forward, ~12 µs under cut-through — is below WS and invisible to it, because `CORE §17` capabilities carry "nominal/usable link rate" but no hop count and no per-hop latency.

#### What the MotionBus profile must do that `LINK` does not describe

Six things, all of them structural rather than parametric:

```text
1  Aggregation triggered by a CYCLE BOUNDARY.
   CORE 12.5 permits an Ethernet LLL to pack several small WS PDUs into one
   transfer, and lists its triggers: MTU nearly full, maximum latency expired,
   high-QoS traffic pending, queue idle. A cycle boundary is not among them,
   and it is the only trigger a scheduled link has.

2  STATIC SLOT ASSIGNMENT rather than stream position.
   CORE 12.5's picture is a stack of PDUs in a payload. A summation frame is a
   fixed-offset array: PDU k always occupies bits [b_k, b_k + n_k). Nothing in
   LINK describes an aggregation whose member positions are configured.

3  IN-FLIGHT MODIFICATION BY NON-TERMINAL NODES.
   A drive is not the source, not the destination, and not a forwarder of the
   frame; it reads and writes its own slice as the frame passes. CORE 12
   forwarding is reconstruct -> route -> re-encode, which a passing-frame node
   does not do. There is no WS concept for this node role.

4  MANY CANONICAL SOURCES IN ONE FRAME.
   One frame carries the setpoint image (source MotionCore) and the feedback
   image (48 distinct sources), i.e. 49 canonical sources and 49 canonical
   destinations in one transfer, in two directions. No profile in LINK has a
   frame with more than one canonical source.

5  TOTAL ELISION OF THE CANONICAL DESCRIPTOR.
   See the table below. CORE 2.2 permits eliding any field the binding implies;
   here every field is implied. Nothing bounds this. (SF-R6-022)

6  A CYCLE-SYNCHRONOUS ACYCLIC REGION WITH MASTER-ARBITRATED ADMISSION.
   The nearest existing concept is CORE 1.7's master-initiated Link, but that
   describes a master polling a slave, not a byte region inside a scheduled
   frame whose per-cycle size is a configuration constant.
```

#### Field-by-field elision on the MotionBus cyclic path

| Canonical field | Width | Transmitted? | Supplied by |
|---|---|---|---|
| `Control.QoS` | 2 b | **elided** | nothing — no priority exists inside a static schedule |
| `Control.Reserved` | 2 b | **elided** | always zero |
| `Control.HasHeaderExtensions` | 1 b | **elided** | always zero; extensions are unrepresentable in a slot |
| `Control.TransportType` | 3 b | **elided** | fixed: cyclic process image |
| `WireNumber` | 8 b | **elided** | Link Binding |
| `SrcParticipantId` | 8 b | **elided** | slot offset ⇒ node position |
| `DestParticipantId` | 8 b | **elided** | slot offset ⇒ image direction |
| `Endpoint` (Namespace + Id) | 16 b | **elided** | slot offset ⇒ one fixed Endpoint per slot |
| **Total** | **48 b / 6 B** | **0 of 6 bytes transmitted** | the static slot map |

This is legal under `CORE §2.2` and it means **WireSpaces contributes zero bytes to the cyclic path.** What it can still contribute is configuration coherence: if the slot map is generated from the same Wiring source as the Service plane (`DEPLOY §2.5`, "one authoritative Wiring source generates every participating projection"), then `Drive_17`'s slot and `Drive_17`'s Identity Service are one identity rather than two. That is a real benefit and it is entirely conditional on a toolchain that spans a non-WS protocol's own configuration tooling. Absent it, the slot map is a second identity universe bolted alongside the first (`CORE §4.6`), and the honest description is that MotionBus is below the WS boundary. Verdict and consequences in §12 and §14 (Question B).

The **acyclic region** is different and is genuine WS carriage: it transmits the full 6-byte canonical descriptor plus payload, because none of it is statically implied — the whole point of the region is that any of 50 devices may exchange any Service message with any of several hosts. That asymmetry is the boundary made visible: **on the same physical link, the scheduled part elides 100% of the header and the unscheduled part elides 0%.**

### 3.6 CAN11 accounting

**N/A.** No Classical CAN11 bus exists in this machine, and the archetype excludes CAN11/VCN analysis. No `WireAlias`, no VCN map, no Guest allocation, no `MainA`/`MainB` position appears in this mapping, and the two-Main default allocation is **not exercised**. The CAN-FD segments use direct canonical addressing in a 29-bit identifier with no relation table (`LINK §4`).

The relevant CAN evidence this sketch does produce is about QoS rather than addressing: on `IOBus_A`/`IOBus_B`, canonical QoS occupies the most arbitration-significant identifier bits (`LINK §2.2`), and moving the fast-event class from QoS 1 to QoS 0 changes the worst-case sensor→`MotionCore` latency from **1,796 µs to 456 µs** against a 2,000 µs budget (§11). That is the only place in the entire machine where the QoS field measurably changes an outcome.

**Membership vs presence.** All Participants in a configuration are **preconfigured** on their Wires and remain members when unreachable; a faulted `Drive_23` keeps its membership and its slot and simply stops updating, and a powered-down `HmiPanel` keeps its `W3`/`W4` bindings. Configuration-scoped Participants — genuinely absent bindings rather than unreachable members — are the 13 condition-monitoring Participants (Config B+), the 3 vision Participants (Config C+), and `RemoteGateway` (Config D only). Within Config D, `RemoteGateway` is a preconfigured member that is unreachable whenever the remote link is down; its bindings are not installed and removed per session.

### 3.7 Configuration progression A → D

| Step | Adds | New Links | New Wires | New forwarding | Rethink or extend? |
|---|---|---|---|---|---|
| A (motion only) | 61 Participants | — | 5 | 4 | baseline |
| A → B (+CM) | +12 `VibSensor`, +`CmAnalyzer` = 74 | +`VibChain` | +1 (`VibStream`) | 0 | **Extend** |
| B → C (+vision) | +2 cameras, +`VisionController` = 77 | 0 | 0 | 0 | **Extend** |
| C → D (+remote) | +`RemoteGateway` = 78 | 0 | 0 | 0 | **Extend**, but see below |

No step required rethinking the mapping. Two observations about *which* step matters, though, because "extend" undersells the last one.

**B and C are the steps the archetype expects to be hard, and they are not.** Adding 1.84 MB/s of continuous streaming and 24 MB/s of image bursts to a hard-real-time machine added one Wire and one Link and touched nothing else, because both subsystems live on their own media and reach the control plane only through small composed summaries. The Wire model handled the "bolted-on subsystem" pattern exactly as advertised.

**D is the step that makes an existing requirement non-optional.** Before Config D, the only `MachineEth` members of `W6 DriveService` are `HmiPanel` and `HmiCore`, and their traffic is small and human-paced; `CommsCore` could be sloppy about pruning and metering and get away with it. `RemoteGateway` introduces a Participant whose normal job is to push 4 MB firmware images at a 48-drive population reachable only through a 512 kB/s scheduled channel. From Config D onward, **destination-pruned egress and acyclic admission control at `CommsCore` are the difference between a working machine and a torn web.** Nothing in the configuration difference between C and D says so, and no check in `DEPLOY §2.3` would catch it.

### 3.8 Configuration inventory (auditable)

```text
Production Participants:              61 (A) / 74 (B) / 77 (C) / 78 (D)
  controller Endpoint Domains         4
  drives                              48        (reserved block 0x40-0x7F)
  MotionBus non-drive                 2         LineEncoder, SafetyController
  I/O islands                         6
  vibration sensors / analyzer        12 / 1    (Config B+)
  cameras / vision controller         2 / 1     (Config C+)
  HMI panel / remote gateway          1 / 1     (gateway Config D only)
  servo inner loops exposed           0
  A24 / A48 / A64 at Config C         53 / 77 / 93

Physical Links:                       6  (5 in Config A)
  shared memory / IPC                 1   CtrlSHM, ample
  scheduled summation Ethernet        1   MotionBus, 100 Mbit/s, 500 us cycle
  switched Gigabit Ethernet           1   MachineEth, 1 Gbit/s
  CAN-FD                              2   IOBus_A/B, 2 Mbit/s, 2 ms scan
  Ethernet daisy chain                1   VibChain, 100 Mbit/s, 13 attachments

Canonical Wires:                      6  (5 in Config A)
  touching MotionBus                  1   W6 DriveService, acyclic region only
  touching MachineEth                 3   W3, W4, W6
  touching CtrlSHM                    4   W1, W2, W3, W6
  single-Link Wires                   2   W4 Bulk, W5 VibStream
  device-private Wires                0   (rejected - did not earn its place)
  Wires carrying cyclic motion data   0   (rejected - see 3.2(e))

Link profiles:                        5 classes
  described in LINK                   4   (all "undecided" or "undeveloped")
  not described in LINK               1   scheduled summation / passing-frame
  fields elided on MotionBus cyclic   6 of 6 canonical header bytes
  fields elided on MotionBus acyclic  0 of 6

Gateway / forwarding state:
  Wire -> LinkBitmask objects         4   (LogicCore x2, CommsCore x2)
  splices                             0
  composition points                  5   (one of which costs provenance)
  destination-pruned egress required  W6 at CommsCore - MANDATORY, not optional
  admission control required          W6 at CommsCore into 256 B/cycle - no WS
                                      concept expresses this

Counted configuration objects:        ~130 at Config C excluding per-Service
                                      Endpoint definitions
Configuration NOT in WS:              a 96-slot cycle schedule with static bit
                                      offsets, a jitter budget, an acyclic
                                      region size, and a distributed-clock
                                      topology
```

---

## 4. Optional optimizations

### 4.1 Jumbo frames on `MachineEth`

Raise the image PDU from 1,456 B to 8,954 B of payload. **Problem it solves:** image PDU rate falls from 17,288/s to 2,816/s and header overhead from 0.41% to 0.067%. **Cost:** the non-preemptible transmit unit grows from 12 µs to 72 µs, which is still negligible against the 50 ms correction budget, and every device on the segment must support it. **Worth taking**, and the reason it is an optimization rather than the minimum is that `LINK §6` has not selected an Ethernet approach at all.

### 4.2 Aggregate the I/O scan into one PDU per island

Each island already sends one 21 B report per scan; nothing to aggregate. Rejected as already minimal. Noted only because the equivalent move is what makes MotionBus work, and it is instructive that a 2 ms bus needs no aggregation while a 500 µs bus is impossible without it.

### 4.3 Merge `W3 Plant` and `W4 Bulk`

One Ethernet-plus-shared-memory Wire. **Problem it would solve:** one fewer Wire, and `HmiCore` would bind one interface instead of choosing per Wire. **Rejected:** it would put 24 MB/s of image traffic into a Wire whose realization includes `CtrlSHM`, so `CommsCore` would forward image PDUs onto shared memory under base flood-and-filter. Even with destination-pruned egress the merge is not clearly better, because keeping the ≤50 ms correction and the 3 MB archive on separate Wires makes the QoS assignment auditable. **Note what the merge would *not* cost:** capacity isolation on `MachineEth`, because there was never any (§4.4).

### 4.4 Rely on Wire decomposition for capacity isolation on `MachineEth`

Not an optimization — a **temptation to refuse explicitly**. `W3` and `W4` are separate Wires on the same switched fabric, and separate Wires on one fabric get no independent capacity whatsoever. The switch is not a WS Participant, its queueing is not a Link Interface (`CORE §17`), and WS QoS marks a PDU without scheduling the fabric (`CORE §14.2`: "Local ordering is not a bus-wide guarantee"). Wire decomposition here buys *scope* — no image reaches `CtrlSHM` or `IOBus` — and nothing else. Filed as SF-R6-027.

### 4.5 A second acyclic region size for maintenance mode

Raise the acyclic region from 256 B to 1,024 B per cycle while the machine is stopped. **Problem it solves:** a 48-drive firmware campaign drops from ~7.1 min to ~1.8 min, and a full cam-table download from 3.4 s to 0.9 s. **Cost:** 12.3% of the cycle budget instead of 4.1% — affordable at A48 (committed rises from 33.8% to 42.0%) and much less comfortable at A64. **Deferred, not rejected:** switching between two schedules is a Link-profile capability, and WS has no way to express that a Link has more than one operating configuration, so the model cannot represent the optimization even though the hardware can.

### 4.6 Specify the acyclic region in bytes per second rather than bytes per cycle

At 2 kHz, 256 B/cycle is 512 kB/s and 4.1% of the budget. At 8 kHz the same 256 B/cycle is 2 MB/s and 16.4% of the budget — four times the Service bandwidth nobody asked for at four times the cost. The correct specification is a rate with a per-cycle cap, i.e. the region appears every Nth cycle. **This is the right engineering answer and WS cannot say it:** `CORE §21.2` lets a Service declare a maximum rate, but there is nowhere to declare a Link's *reserved* rate for a class of traffic, and no capability field to check the declaration against (SF-R6-021).

---

## 5. Friction signals (minimum mapping happy path)

| Signal | None / Mild / Significant | One-sentence justification |
|---|---|---|
| Artificial Wire | **None** | All six Wires name real propagation scopes; the one candidate that would have been artificial — a Logical Bus over a link that elides 100% of the canonical header — was rejected on arithmetic in the minimum mapping rather than declared and apologized for. |
| Wire proliferation | **None** | Six Wires for 78 Participants across 6 heterogeneous links, with the 48-drive population and the 12-sensor chain costing one Wire each; this is the leanest decomposition in the corpus. |
| Artificial hierarchy | **None** | No `MainA`/`MainB`, no Origin, no coordinator invented by the model; `CommsCore`'s central position is a physical fact about which domain owns which NIC, not a role the addressing scheme imposed. |
| VCN pressure | **N/A** | No Classical CAN11. |
| WireAlias pressure | **N/A** | No Classical CAN11. |
| Configuration burden | **Mild** | ~130 counted objects for 78 Participants is low, but two things the machine cannot run without — destination-pruned egress and an acyclic admission rate at `CommsCore` — have no home in the model and must be carried by hand outside the Wiring. |
| Failure/topology mismatch | **Significant** | `W6 DriveService` spans a 1 Gbit link and a 512 kB/s scheduled channel — a 244:1 capacity ratio behind a hard 500 µs deadline — and a configuration that is structurally valid under `CORE §19.1` and passes every check in `DEPLOY §2.3` can stop the machine and tear the web; the Logical Bus abstraction shows a member list and says nothing about the schedule the members live under. |

**Free-form notes.** Three things were awkward in ways the seven signals do not capture.

**The model has no vocabulary for "this relation needs a slot, not an identity."** I spent most of the mapping effort deciding which relations WS should own, and the model gave me no help: every relation looks the same to `CORE`, and the distinction that actually mattered — 5 relation classes need a guaranteed instant and 29 need a name — had to be derived from arithmetic each time. A conventional motion engineer makes this call by habit ("that's a PDO, that's an SDO"); a WS mapper has to rediscover it.

**Endpoint storage answered the missed-cycle question well, and then opened a new hazard.** `CORE §9.5`'s required Snapshot generation counter plus `CORE §9.3`'s mandatory Snapshot arrival time is exactly the right mechanism for distinguishing a missed cyclic update from an idle one, and `CORE §21.4` already prescribes the behavior. But on a scheduled link the slot is *present in every frame whether or not the master updated it*, so a naive LLL that accepts the slot each cycle advances the generation and refreshes the arrival time, and a dead master becomes indistinguishable from a live one. `CORE §18.5` states the principle for telemetry ("unavailable is not zero") and nothing applies it to acceptance. Filed as SF-R6-026.

**Nothing about the four-domain controller was awkward.** Four Participants on one shared-memory Wire behaved exactly like four boxes on a cable, `CommsCore` forwarding for `MotionCore` preserved source across three link types, and the `HmiCore` dual-interface loop resolved by binding per interface without a splice. That part of R6 is settled.

---

## 6. Model pressure

- **If you could change one WS concept** to simplify this system, what would it be? **Put schedule properties into Link capabilities.** `CORE §13.4` names three dimensions of the compatibility envelope — size, timing, transport — and states the required behavior sharply: "an unrepresentable placement fails at configuration or before TX, never by truncation or silent degradation." Size is fully checkable (`maximum PDU / MTU`, `maximum reassembled PDU size`). Transport is checkable (`supported TransportTypes`). **Timing has exactly one field, `polling cadence, for master-initiated Links`**, and nothing else — no cycle time, no per-node slot, no jitter bound, no reserved rate for a non-scheduled region. That single omission is the root of every hazard in this sketch: flood-and-filter onto a cyclic link, unmetered admission into the acyclic region, group atomicity placed on a Link that cannot provide it, and a ≤50 ms deadline crossing a fabric with no reservation. This is a **capability-descriptor change, not a data-plane change** — `CORE §17` exists for exactly this purpose and already says an LLL must declare its scheduling position "even when the answer is 'none'." Extending that declaration to schedules would make `DEPLOY §2.3` able to reject the configurations that break this machine.

- **Did WS expose a useful distinction** the conventional model obscures? Two, and one of them I did not expect. **(a) Which relations need identity and which need a slot.** The conventional design blurs this — everything is "a PDO" or "a message" and the split is a vendor tooling artifact. Forcing the question produced a clean number: 5 of 34 relation classes need a guaranteed instant and cannot be WS relations; 29 need a name and are. No conventional motion architecture states that split, and every one of them consequently re-implements the Service plane per vendor. **(b) Forwarding versus composition, priced.** The mapping has four forwarding objects and five composition points, and the distinction told me exactly where provenance survives and where it does not — the vibration dump archive loses `Src = VibSensor_07` at the `VibChain`/`MachineEth` boundary and nothing else does. In the conventional design that boundary exists too and nobody writes it down.

- **What WS did not expose, and should have.** The 244:1 capacity ratio inside `W6 DriveService` is the most dangerous fact about this configuration, and the Wire model represents it as a member list. Nothing in a Wire, a Link Binding, or a capability descriptor makes it visible.

---

## 7. Open questions

- **Is a fully-eliding Link inside WS or outside it?** `CORE §2.2` permits eliding any field the binding implies and sets no floor. A profile that transmits zero canonical bytes is conforming, and the only thing WS then contributes is that the slot map and the Service plane share one configuration source. Whether that is the elision principle working as designed or the boundary of the architecture is a normative question this sketch can pose but should not settle (SF-R6-022).
- **Does the standard Service set need an operational-state / mode member?** Archetype 13's coordinated phase-in is a group state machine across 49 Participants: command a target state to a set, each reports actual state plus a blocking reason, abort at any point. Archetype 11 needed flight mode and archetype 12 needed commissioning state — the same shape three times. This is a Service and flattens to ordinary Endpoints (`CORE §19.2`), so it is an ecosystem question rather than a protocol one, but three independent rediscoveries is a signal.
- **What should a Link capability say about a multi-hop realization?** `VibChain`'s twelve integrated switches add up to 1.52 ms of store-and-forward latency for the farthest sensor, and `CORE §17` has no field for hop count, per-hop latency, or cut-through versus store-and-forward. The same question applies to `MachineEth`'s switch and would apply to any daisy chain.
- **Does `CORE §12.5` aggregation cover a scheduled aggregation?** The mechanism is named and the triggers are enumerated, and a cycle boundary is not among them. Whether a scheduled summation profile is an instance of §12.5 or a separate profile class is a `LINK` question with real consequences for whether the cyclic path is WS at all.
- **How should a Service declare that it needs a guaranteed instant?** Group-atomic application has no representation and, more importantly, no way to be *checked* — a Service could be placed on a Link that cannot provide it and nothing would notice. Whether that belongs in a Service traffic declaration (`CORE §21.2`), a Link capability, or a WireContract (out of scope for this round) is open.
- **What is the intended relationship between WS QoS and a switched fabric?** On `MachineEth`, WS marks and the switch schedules. Whether the model should say that explicitly, or treat the fabric as part of the Link, or require a capability declaration of what the marking is honored by, is unaddressed and matters wherever a deadline crosses a switch.
- **Is a segmenting Transport the right home for streams?** `CORE §20` says bulk belongs to a segmenting Transport and that none is designed; `FUTURE §3.1` names the sequenced / E2E-protected datagram as the likely second Transport. This machine needs exactly that and nothing more exotic — see §14, Question F.

---

## 8. Spec findings

| ID | Finding |
|---|---|
| SF-R6-021 | **Link capabilities cannot express a schedule, so the "timing" half of the placement contract is uncheckable.** `CORE §13.4` names size, timing, and transport as the dimensions in which a Service placement can fail, and requires that an unrepresentable placement fail at configuration time. `CORE §17` gives size several fields and transport one, but gives timing only `polling cadence, for master-initiated Links`. Consequently a 100 Mbit/s link with a 500 µs cycle, a fully allocated slot schedule, and a 256 B/cycle acyclic region is indistinguishable, in the capability model, from a 100 Mbit/s link with 12.5 MB/s free. Every hazard in this sketch is invisible to `DEPLOY §2.3`: forwarding a 1500-byte PDU onto a cyclic link (120 µs = 24% of the cycle), admitting a 4 MB image into a 512 kB/s channel, placing a ≤2 ms relation behind an unbounded queue, or placing a group-atomic relation on a Link that cannot provide one. Recommend capability fields for cycle time, slot allocation, reserved non-scheduled rate, and jitter bound. This is a descriptor change, not a data-plane change. |
| SF-R6-022 | **Total canonical elision is legal and leaves WireSpaces contributing nothing on the wire.** `CORE §2.2` permits a profile to elide any canonical value the Link Binding supplies. On a summation-frame link, slot offset supplies `WireNumber`, `SrcParticipantId`, `DestParticipantId`, and `Endpoint`; the schedule supplies `QoS` and `TransportType`. A conforming profile therefore transmits **0 of 6 header bytes**, and the receiving Participant reconstructs canonical identity entirely from configuration. Nothing in `CORE` bounds elision or states what a profile must retain to still be a WS Link, so "WS supports this link class" and "WS is absent from this link" are the same configuration. Recommend either an explicit floor or an explicit statement that a fully-eliding Link marks the architecture's boundary — with the corollary that the remaining value is configuration coherence (`DEPLOY §2.5`) and only if the toolchain actually generates both projections. |
| SF-R6-023 | **`LINK` has no aggregating / summation profile class, and `CORE §12.5` does not stretch to cover one.** Six specific gaps, enumerated in §3.5: aggregation triggered by a cycle boundary (not among §12.5's four triggers); static slot offsets rather than stream position; in-flight modification by a node that is neither source, destination, nor forwarder; a single frame carrying 49 canonical sources in two directions; total elision (SF-R6-022); and a cycle-synchronous acyclic region with master-arbitrated admission, for which `CORE §1.7`'s polled-Link model is the nearest and not close. The arithmetic in §10.1–10.2 shows this is not an efficiency matter: without such a profile the exchange is not merely wasteful but physically impossible (192,000 frames/s against a 148,809 frames/s ceiling). |
| SF-R6-024 | **"Time" is at least three concepts sharing one name, and only one of them is a Service.** (1) *Wall-clock / epoch time* — a Service, deliverable over any Link, accuracy bounded by Link cadence and consumer scheduling, good to roughly a millisecond here. (2) *Link phase reference* — not a Service, a property of one Link profile, achievable at ±1 µs only where the Link provides distributed clocks, and **not transitive across a gateway**. (3) *Arrival time* (`CORE §9.4`) — a local monotonic tick with no cross-Participant meaning at all. The model conflates (1) and (2) by listing "Time" among standard Services, which quietly implies the coherent-Services-across-all-Link-types claim covers phase. It does not. The product consequence is concrete: `VibSensor_07` and `Drive_17` share wall-clock but cannot share phase, so a bearing event cannot be aligned to a print registration event better than ~1 ms, which at 300 m/min web speed is **5 mm of material**. Recommend naming the three and stating that phase is out of scope for the Time Service. |
| SF-R6-025 | **Group-atomic application has no representation and, worse, no way to be detected as unavailable.** Forty-eight setpoints computed from one line-shaft position must take effect on the same tick. `CORE` offers nothing: broadcast is bounded fan-out with no statement about when recipients act; `CORE §9.4` explicitly decouples acceptance from consumption ("a Service consumes what was accepted later, in a context it owns"); arrival time is a local tick; and the `Control` field has no room and no defined header extension. Leaving the mechanism to the Link profile is defensible layering. What is not defensible is that a Service requiring group atomicity can be placed on a Link that cannot provide it and **nothing in `CORE §13.4` or `DEPLOY §2.3` will notice** — the compatibility envelope has no coordination dimension. Related to SF-R6-021 but distinct: that one is about rate and deadline, this one is about simultaneity. Independently corroborated by SF-R6-018 from archetype 15, which reaches the same conclusion from the receive side (no cycle-indexed group completeness, no distributed timed application, no stale-member exclusion); two unrelated motion archetypes hitting this is evidence it is structural rather than archetype-specific. |
| SF-R6-026 | **Cyclic liveness must not be manufacturable by the LLL.** On a scheduled link a slot is present in every frame whether or not its producer updated it. An LLL that accepts the slot's contents into a Snapshot Endpoint each cycle advances the required generation counter (`CORE §9.5`) and refreshes the arrival time (`CORE §9.3`, §9.4), so a stopped producer is indistinguishable from a running one by exactly the two mechanisms `CORE §21.4` prescribes for detecting it. `CORE §18.5` states the principle for telemetry — "unavailable is not zero" — and nothing applies it to acceptance; `LINK §9`'s "idle behavior: how 'nothing to send' is expressed" is adjacent but is about transmitters. Recommend the rule that acceptance requires evidence that the carrying transfer occurred this period, stated wherever a profile may present periodic slot data. |
| SF-R6-027 | **A shared fabric is not a Link Interface, so neither QoS nor Wire decomposition provides capacity isolation on switched Ethernet.** `CORE §14.2` is candid locally ("Local ordering is not a bus-wide guarantee") and `CORE §17` describes a Link Interface's own queues, but a switched Gigabit network has queueing in a device that is not a WS Participant and has no capability descriptor. Two consequences bit this mapping. WS QoS on `MachineEth` is a *mark* honored only by whatever 802.1p/DSCP configuration exists outside the Wiring. And two Wires on one fabric share all of its capacity, so separating `W3 Plant` from `W4 Bulk` bought scope and zero bandwidth isolation — which is why the ≤50 ms registration budget is not met under the archetype's §8 load (§10.4). Recommend stating what a Link capability claims when the medium is a fabric, and that Wire decomposition is a scope tool and never a capacity tool. |

**Confirmation, not a new finding — SF-R6-013 (destination-pruned egress).** This archetype reproduces it four independent times and raises its severity from "unusable" to "unsafe." `W6 DriveService` spans `MachineEth` and `MotionBus`: under base flood-and-filter one 1500-byte PDU occupies **120 µs of a 500 µs cycle (24%)** and a single 4 MB image occupies **25.2 s of continuous link time, i.e. 50,400 consecutive cycle overruns**, against a requirement that drives trip to a safe state within a bounded number of missed cycles. The two CAN-FD segments needed separate Wires for the same reason (§3.2(b)), no Wire may span `VibChain` and `MachineEth` (§3.2(d)), and the `W3`/`W4` separation exists partly for it (§4.3). `CORE §12.1` calls pruning "an optimization, not a change to the generic forwarding key or Wire semantics." On this machine it is a safety-relevant precondition, and its optional framing pushed the mapping toward more Wires than the topology needs — the same effect archetype 11 reported.

---

## 9. Diagrams

### Wire-centric (Config C, A48)

```text
BELOW WS  ------------------------------------------------------------------
  MotionBus cyclic image, 500 us, 6 of 6 canonical header bytes elided
    setpoint image   MotionCore -> 48 drives      336 B/cycle, static offsets
    feedback image   48 drives -> MotionCore      336 B/cycle, written in flight
    line reference   LineEncoder -> MotionCore      6 B/cycle
    safety chain     SafetyController <-> drives   96 B/cycle, 8 ms round
    cycle tick / +/-1 us phase                    distributed clocks, hardware

WS  -------------------------------------------------------------------------
  W1 IoA        [IOBus_A + CtrlSHM]     IoIsland_1..3, LogicCore(fwd), MotionCore
  W2 IoB        [IOBus_B + CtrlSHM]     IoIsland_4..6, LogicCore(fwd), MotionCore
                  2 ms scan 38.4% loaded; fast events at QoS 0 -> 456 us worst case

  W3 Plant      [CtrlSHM + MachineEth]  MotionCore, LogicCore, HmiCore(SHM),
                                        CommsCore(fwd), VisionController,
                                        CmAnalyzer, HmiPanel(Eth)
                  mode, registration correction, reject, alarms, CM summary,
                  production data, Link status, wall clock. All small.

  W4 Bulk       [MachineEth]            Camera_A/B, VisionController, CmAnalyzer,
                                        HmiCore(Eth), HmiPanel, RemoteGateway(D)
                  24 MB/s images, 3 MB archives, dumps, logs, firmware

  W5 VibStream  [VibChain]              CmAnalyzer, VibSensor_01..12
                  1.84 MB/s continuous, 1,200 PDUs/s, 0.39% header

  W6 DriveService [MotionBus + CtrlSHM + MachineEth]
                                        48 drives, LineEncoder, SafetyController,
                                        4 controller domains, HmiPanel,
                                        RemoteGateway(D)
                  the ONLY Wire touching MotionBus; lives in the 256 B/cycle
                  acyclic region; PRUNING AND ADMISSION MANDATORY

BESIDE WS  -------------------------------------------------------------------
  raw vibration blocks and image payloads travel as WS PDUs, but sequencing,
  gap detection, reassembly, retry, and backpressure are Service-authored
  because no Transport defines them
```

### Why the cyclic exchange is not a Wire

```text
CANDIDATE - W_MotionCyclic [CtrlSHM + MotionBus], CommsCore forwarding

    MotionCore --CtrlSHM--> CommsCore --MotionBus--> Drive_01 .. Drive_48
       48 setpoint PDUs        router lookup           slot in summation frame
       + 48 feedback PDUs      + bounded copy
       + 1 line reference      per PDU
       = 97 PDUs per 500 us

    cost of the CtrlSHM hop     194,000 PDUs/s  = ~9.7% of one core  (A48)
                                516,000 PDUs/s  = ~25.8% of one core (A64)

    what it buys on the wire    nothing: the summation frame has no source
                                field, so Drive_17 reconstructs
                                Src = MotionCore from its slot map regardless
                                of who assembled the frame

    if individually framed      129 Mbit/s on a 100 Mbit/s link, and
                                192,000 fps against a 148,809 fps ceiling
                                -> not slow, IMPOSSIBLE

REJECTED. The cyclic vector is one object below WS. The 48-PDU framing is an
artifact of insisting that WS own the cyclic path.
```

### The physical loop, and how it resolved without a splice

```text
                MotionCore   LogicCore   CommsCore   HmiCore
                     |           |          |  \        |  \
                     +--- CtrlSHM ----------+   \       |   \
                                                 \      |    \
                                          MachineEth ---+-----+

  CommsCore and HmiCore are BOTH on CtrlSHM and MachineEth.
  Any Wire with both links and both forwarders contains a cycle. (CORE 3.3)

  RESOLUTION - bind per interface, not per device:
      CommsCore   sole CtrlSHM <-> MachineEth forwarder, every Wire
      HmiCore     W3 Plant on CtrlSHM;  W4 Bulk on MachineEth;  forwards nothing

  CORE 12.6 makes the unbound interface inert: electrical visibility is not
  membership. Cost: 0 splices, 0 extra Wires, 0 extra WireNumbers.
```

### Failure behavior read off the mapping

```text
one drive faults
    status word in its cyclic feedback slot -> detected in <= 1 cycle (0.5 ms),
    BELOW WS; fault detail (hundreds of bytes) follows on W6 within one acyclic
    rotation (~12.5 ms). Fast path below WS, diagnostic path in WS - correct.

missed MotionBus cycle
    drive's cyclic watchdog, below WS. If it were a WS Snapshot: generation
    unchanged + arrival age > N cycles (CORE 21.4) - the right mechanism, with
    the hazard in SF-R6-026.

one IoIsland silent
    W1 or W2 member stops sourcing; membership persists; LogicCore's freshness
    timeout trips its section. Motion continues where safe.

CmAnalyzer lost, or its stream backing up
    zero effect on motion, enforced by TWO independent things:
      (a) WS - no Wire spans VibChain and MotionBus, and CmAnalyzer is not a
          member of W6 at all;
      (b) the MotionBus schedule - cyclic slots are allocated regardless of
          acyclic demand.
    (a) is a genuine WS contribution. (b) is not WS.

MachineEth saturated: 24 MB/s images + 18 MB dump archive + 4 MB firmware
    MotionBus     protected. Schedule guarantees the cyclic slots; CommsCore
                  prunes and meters into the 256 B/cycle acyclic region.
    IOBus_A/B     protected. No Wire spans IOBus and MachineEth at all.
    correction    NOT PROTECTED. 41.7 ms nominal chain + 8-32 ms of switch
                  queueing = 50-74 ms against a 50 ms budget. WS has no lever;
                  the fix is switch QoS, outside the model. (SF-R6-027)

HmiPanel lost / RemoteGateway absent
    members of W3/W4 (and W6 for the gateway) stop sourcing. No production
    impact. RemoteGateway is preconfigured in Config D and merely unreachable;
    in Configs A-C its bindings are genuinely absent.
```

---

## 10. Mandatory arithmetic

### 10.1 Cyclic fit, per-PDU

```text
A48:  48 setpoints + 48 feedbacks = 96 PDUs per 500 us cycle = 192,000 PDUs/s
```

If each canonical PDU became an individual Ethernet frame on `MotionBus`:

| Quantity | Value |
|---|---|
| WS PDU size | 7 B payload + 6 B canonical header = **13 B** |
| Minimum Ethernet frame on the wire | 7 preamble + 1 SFD + 14 header + 46 payload + 4 FCS + 12 IFG = **84 B** |
| Offered load | 192,000 × 84 B = 16.13 MB/s = **129.0 Mbit/s** |
| `MotionBus` capacity | 100 Mbit/s = 12.5 MB/s |
| **Fits?** | **No — 129% of capacity** |
| Maximum frame rate of 100BASE-TX at minimum frame size | 100e6 / (84 × 8) = **148,809 frames/s** |
| Required frame rate | **192,000 frames/s** |
| **Physically possible?** | **No — 129% of the physical frame-rate ceiling, before payload is considered** |

The second row is the decisive one. Individual framing does not merely waste bandwidth; the link cannot emit that many frames at any payload size. Both plausible topologies fail: on a shared/passing domain every drive MAC must sustain 192,000 frames/s (one frame per 5.2 µs, against a 6.72 µs minimum frame time — again impossible); on a switched star each drive sees only its own ~4,000 frames/s, but the uplink to `MotionCore` must carry all 129 Mbit/s and exceeds a 100 Mbit port.

For contrast, the **application data alone** is 96 × 7 B = 672 B/cycle = 1.344 MB/s = **10.75% of the link**. The machine's actual information content is a tenth of the link; the framing is what makes it impossible.

**A second cost, independent of the wire.** If `W_MotionCyclic` existed, every cyclic PDU would also cross `CtrlSHM` individually — 97 PDUs per cycle (48 out, 48 back, 1 line reference) — each requiring a router lookup against read-mostly tables and a bounded copy into Endpoint storage (`CORE §11.1`, §16.1):

```text
A48   97 x 2,000  = 194,000 PDUs/s  x ~0.5 us  = 97 ms/s  = ~9.7% of one core
A64  129 x 4,000  = 516,000 PDUs/s  x ~0.5 us  = 258 ms/s = ~25.8% of one core
```

on the controller that also evaluates 48 (or 64) cam profiles per cycle. Falsification criterion **F4 met**.

### 10.2 Canonical header overhead

Payload 7 B, canonical header 6 B (`CORE §2.1`).

| | A24 (1 kHz, 48 PDU/cycle) | A48 (2 kHz, 96 PDU/cycle) | A64 (4 kHz, 128 PDU/cycle) |
|---|---|---|---|
| PDUs/s | 48,000 | 192,000 | 512,000 |
| Header bytes/s | 288 kB/s | **1.152 MB/s** | **3.072 MB/s** |
| Header bits/s | 2.30 Mbit/s | 9.22 Mbit/s | 24.58 Mbit/s |
| Payload bytes/s | 336 kB/s | 1.344 MB/s | 3.584 MB/s |
| Cyclic total | 624 kB/s | 2.496 MB/s | 6.656 MB/s |
| **Header as % of cyclic traffic** | **46.2%** | **46.2%** | **46.2%** |
| **Header as % of `MotionBus` capacity** | **2.30%** | **9.22%** | **24.58%** |

> **Does "fixed full canonical headers are not a meaningful bandwidth concern" hold on this link?** No. At A64 the canonical headers alone consume **a quarter of the entire 100 Mbit/s link** to carry addressing that is 100% statically known, before payload, framing, safety, sync, or acyclic traffic. At A48 they consume 9.2%. Falsification criterion **F2 met at A64**, missed at A48 by 0.8 points.

The claim is not wrong in general — it is wrong *below a payload size*, and the crossover is computable:

```text
header fraction = 6 / (6 + payload)

payload >= 114 B   ->  header below 5%
payload >= 594 B   ->  header below 1%

cyclic motion PDU        7 B  ->  46.2%   <- the machine's reason for existing
vibration block      1,536 B  ->   0.39%
image chunk (MTU)    1,456 B  ->   0.41%
image chunk (jumbo)  8,954 B  ->   0.067%
```

Across seven traffic classes on one machine, canonical header overhead spans **0.067% to 46.2%**, and the spread is entirely a function of payload size rather than link type. The doc claim is true for six of the seven and catastrophically false for the seventh.

**How much must a summation-frame profile elide to make it hold?** All of it. Eliding `WireNumber` alone (1 B) leaves 5 B and 41.7%. Eliding everything but `Endpoint` (2 B) leaves 22.2%. Only eliding all six bytes brings the cyclic image to 672 B/cycle of pure payload — 10.75% of link, 5.4% of the per-cycle budget for each direction — which is affordable. Falsification criterion **F3 met**: 6 of 6 bytes, 100% elision, and criterion **S2 fails** because there is no canonical field the profile does not statically imply.

### 10.3 Acyclic budget for drive Services

**Chosen acyclic region: 256 bytes per cycle.** Defensible because it is 4.1% of the per-cycle byte budget at A48 (§11) and is the same order as real motion-bus mailbox allocations. Sensitivity at 64 B/cycle is shown for comparison.

```text
256 B/cycle at 2 kHz  =  512,000 B/s  =  512 kB/s  =  4.10 Mbit/s
per-PDU overhead      =  6 B canonical + ~4 B mailbox framing = 10 B
mean acyclic PDU      =  ~120 B  ->  8.3% overhead
effective payload     =  ~469 kB/s
```

| Operation | At 100% of the acyclic region | At 25% share | At 64 B/cycle |
|---|---|---|---|
| Identity + Version + Health, all 48 drives (~4.5 kB, 144 exchanges) | **10 ms** bandwidth-limited / **36 ms** transaction-limited | 40 / 144 ms | 40 / 144 ms |
| 32 kB cam table, one drive | **70 ms** | 280 ms | 279 ms |
| 32 kB cam table, all 48 drives (1.57 MB) | **3.4 s** | 13.4 s | 13.4 s |
| 4 MB firmware image, one drive | **8.9 s** | 35.8 s | **35.8 s** |
| 48-drive firmware campaign (201 MB) | **7.1 min** | 28.5 min | **28.5 min** |

**Possible during production?** Split by operation, and the split is not the one bandwidth alone suggests.

- **Identity, Version, Health, fault detail, parameter reads, cam tables — yes.** Tens to hundreds of milliseconds, and the only production-critical relation sharing the channel (machine mode to drives, 100 Hz × ~30 B ≈ 3 kB/s) needs 0.6% of it.
- **Firmware — no**, and the binding reason is not bandwidth. 7.1 minutes at 100% allocation starves fault detail and mode traffic for 47 other drives, and a drive receiving an image is not running its axis anyway. Firmware is a maintenance-window operation, and the mapping should say so rather than pretending a rate limit makes it production-safe.

**What happens to the cyclic budget if the acyclic region is made large enough to be useful?** It already is useful at 256 B, which is the mildly surprising result — 2 kHz is fast enough that a small per-cycle region yields half a megabyte per second. The cost curve is linear and gentle at A48 and steep at A64:

```text
                A48 (500 us, 6,250 B budget)     A64 (250 us, 3,125 B budget)
 256 B/cycle    4.1% of budget, 512 kB/s          8.2% of budget, 1.02 MB/s
 512 B/cycle    8.2% of budget, 1.02 MB/s        16.4% of budget, 2.05 MB/s
1024 B/cycle   16.4% of budget, 2.05 MB/s        32.8% of budget, 4.10 MB/s
```

Note the perversity: a fixed byte-per-cycle region delivers *more* Service bandwidth at higher cycle rates while costing *more* of the budget, which is backwards from what the machine needs. The correct specification is a rate with a per-cycle cap, and §4.6 records that WS cannot express it.

### 10.4 Stream fit

#### Continuous vibration on `VibChain`

```text
per sensor   3 ch x 25,600 Hz x 2 B = 153.6 kB/s
aggregate    12 x 153.6 kB/s        = 1.84 MB/s   on a 12.5 MB/s chain (14.7%)
```

**Block size: 256 samples per channel-triple = 1,536 B payload, 10 ms per block.**

| Block size | Payload | PDUs/s (all 12) | Header overhead | Wire load |
|---|---|---|---|---|
| 1 sample | 6 B | **307,200** | **50.0%** | 3.69 MB/s (29.5%) — reject |
| 32 samples | 192 B | 9,600 | 3.03% | 2.13 MB/s |
| **256 samples** | **1,536 B** | **1,200** | **0.39%** | **1.90 MB/s (15.2%)** |
| 1024 samples | 6,144 B | 300 | 0.098% | 1.86 MB/s — 40 ms latency, needs jumbo frames |

**What does Endpoint storage do with a continuous stream?** It depends entirely on the declared class, and `CORE §9.5` gives the right answer without ambiguity.

- **Snapshot is wrong and the model says so.** "Snapshot is valid only when processing the newest complete value, without processing every intermediate value, preserves the intended application semantics." A vibration stream fails that test by definition — the spectrum is computed from every sample. A Snapshot would coalesce blocks silently, and the only evidence would be the required generation counter and the replacement count.
- **Queue is right.** History-preserving, statically bounded, non-blocking acceptance, `Full` as an ordinary outcome with `accepted` / `rejected_full` / `high_water_mark` diagnostics. Depth 32 per sensor = 32 × 1,542 B = **49.3 kB per sensor, 592 kB total, buying 320 ms of consumer-stall tolerance**.

**What the model provides, and what it does not:**

```text
PROVIDES   bounded storage with declared depth
           accept / reject with a countable rejection path
           high-water mark and replacement counters
           arrival timestamp at acceptance (CORE 9.4)
           an explicit statement that depth is a claim about the protocol

DOES NOT   sequence numbers - CORE 21.3 lists "freshness, sequence, correlation,
           duplicate, and restart semantics" as things a Service specification
           must state, i.e. the model names the hole and hands it to the Service
           gap detection at the receiver - a PDU lost on the wire is invisible
           to CmAnalyzer; only the sender's congestion counters and the
           receiver's rejected_full counter exist, and neither covers wire loss
           end-to-end backpressure - only optional hop-by-hop credit
           (CORE 15.3-15.5), which is a Link profile choice, not a stream tool
           reassembly of an object larger than one Endpoint slot
```

**Triggered dump — up to 18 MB.**

```text
per sensor   1.5 MB  =  1,000 chunks of 1,536 B
all twelve   18 MB   = 12,000 chunks
wire time    18 MB / (12.5 - 1.9) MB/s available = 1.7 s at full rate
                                                   3.4 s at 50% allocation
```

**Does the model have a fragmentation concept?** Checked, as instructed. `TransportType` is 3 bits with up to 8 values (`CORE §2.1`) and **the registry is not frozen and only Unreliable Datagram is defined** (`CORE §20`). LLL-level fragmentation exists per profile (e.g. CAN PDUA) but `CORE §20` explicitly forbids growing it into a transport: "firmware images, files, logs, and similar bulk data belong to a segmenting Transport, **not** to a constrained LLL." That segmenting Transport does not exist; `FUTURE §3.1` names the sequenced / E2E-protected datagram as the likely second one. So the field has room and the answer is empty. Falsification criterion **F9 met**.

The practical cost of the gap, quantified:

```text
depth-1 lock-step, Service-authored, ~2 ms round trip
    1,000 chunks x 2 ms = 2.0 s per sensor, 24 s for twelve serialized
    (CORE 9.5: "a depth-1 Endpoint asserts that the protocol is lock-step")

depth-64 window, Service-authored, 98.7 kB per sensor
    wire-limited: 1.7 s for all twelve
    but no gap detection and no retry - any loss is silent

difference    14x, and the fast version is the one with no error handling
```

**On loss of one fragment:** undefined at the model level. Under Service-authored chunking the receiver detects the gap only if the Service put a sequence number in its schema, and recovery is a re-request the Service must also author. `CORE §14.4`'s `QOS-9` ("a rejected submission must not advance protocol state") is the one piece of the puzzle the model does supply, and it is a transmit-side rule.

#### Images on `MachineEth`

```text
per frame    2048 x 1536 x 1 B = 3,145,728 B
aggregate    2 cameras x 4 fps x 3.0 MB = 24 MB/s   on 125 MB/s (19.2%)
```

| Chunking | Payload | PDUs/frame | PDUs/s | Header overhead | Wire load |
|---|---|---|---|---|---|
| MTU 1500 | 1,456 B | 2,161 | **17,288** | **0.41%** | 25.9 MB/s (20.7%) |
| Jumbo 9000 | 8,954 B | 352 | 2,816 | **0.067%** | 25.3 MB/s (20.3%) |

Header overhead is a non-issue. The problem is interference, and it is the one place in this machine where a stated requirement is **not met**.

```text
REGISTRATION CORRECTION LATENCY CHAIN (budget 50 ms)

  line-position trigger -> exposure and readout   ~5.0 ms   (assumed, out of scope)
  Camera -> VisionController, 3.24 MB at 1 Gbit    25.9 ms
  VisionController analysis                       ~10.0 ms  (assumed, out of scope)
  VC -> CommsCore -> MotionCore on W3 Plant         0.3 ms
  MotionCore applies at the next cycle             <=0.5 ms
  ------------------------------------------------------
  nominal total                                    41.7 ms   margin 8.3 ms
```

The image transfer alone is **52% of the latency budget**, so the chain has no room for queueing. Under the archetype's §8 scenario — 24 MB/s of images, an 18 MB dump archive, and a 4 MB firmware transfer converging — a typical access switch with 1–4 MB of shared buffer contributes **8–32 ms of queueing** at 125 MB/s before it starts dropping, and dropping hits the correction PDU too. Total: **50–74 ms against a 50 ms budget.**

**What keeps a 24 MB/s stream and an 18 MB burst from delaying the correction?** Two mechanisms, one of which works and one of which is outside WS.

- **Locally, at each Participant's egress: WS QoS works, conditionally.** The correction is QoS 0 and bulk is QoS 3, and the non-preemptible unit is one Ethernet frame — **12 µs at MTU, 72 µs jumbo** (`CORE §14.2`: "the in-progress unit is not preemptible"). The condition is that bulk is submitted as many PDUs rather than one enormous one; a Service that declared a 3 MB PDU on a Link whose MTU permitted it would make the non-preemptible unit 25.9 ms, and nothing in the model forbids that.
- **Across the fabric: nothing WS controls.** The switch is not a Participant, its queues are not a Link Interface, and WS QoS is a mark that an 802.1p or DSCP configuration outside the Wiring may or may not honor. Separating `W3 Plant` from `W4 Bulk` gives them zero independent capacity (SF-R6-027).

### 10.5 QoS expressiveness

**Enumerate the urgency classes this machine actually has**, correcting the archetype's starting list. Four of its twelve are removed as not-WS, and six are added or split.

| # | Class | Requirement | Correction to the archetype's list |
|---|---|---|---|
| 1 | Cyclic motion setpoint | 500 µs, hard | **Not a WS class** — a schedule slot |
| 2 | Cyclic motion feedback | 500 µs, hard | **Not a WS class** — a schedule slot |
| 3 | Cycle tick / phase | ±1 µs | **Not a WS class** — a hardware property |
| 4 | Functional safety chain | 8 ms watchdog | **Not a WS class** on MotionBus — a reserved slot |
| 5 | Line reference | 2 kHz | **Added** — omitted from the starting list; also a slot |
| 6 | Fast I/O event | ≤2 ms | |
| 7 | Drive fault / alarm event | ≤100 ms | **Added** — the starting list buried this in "diagnostics" |
| 8 | Registration correction | ≤50 ms | |
| 9 | Defect / reject signal | ≤20 ms | **Added** — tighter than the correction |
| 10 | Ordinary cyclic I/O scan | 2 ms | |
| 11 | Machine state / mode | 10–100 Hz | |
| 12 | Continuous vibration stream | 10 ms blocks | |
| 13 | Envelope / RMS, CM diagnosis | 0.1–10 Hz | **Split** from the stream — different size and cadence |
| 14 | Drive Service request/response | seconds | **Added** — scheduled by the motion master, not by QoS |
| 15 | Wall-clock time, Link status, telemetry | 1 Hz | **Split** from "diagnostics" |
| 16 | Logs / event history | on demand | |
| 17 | Image frame | 4 fps, 3 MB | |
| 18 | Triggered dump / defect archive | burst, MB | **Split** from images — different urgency, same class available |
| 19 | Firmware transfer | 4 MB | |

**Nineteen classes, of which 5 are below the WS boundary. Fourteen WS classes onto 4 QoS values:**

| QoS | Class | Members |
|---|---|---|
| **0** Critical | | fast I/O event (6), registration correction (8), defect/reject signal (9), drive fault/alarm event (7) |
| **1** High | | cyclic I/O scan and output command (10), machine state/mode (11) |
| **2** Normal | | continuous vibration stream (12), envelope/RMS and diagnosis (13), drive Service request/response (14), wall-clock/Link status/telemetry (15) |
| **3** Background | | logs (16), image frames (17), triggered dump and defect archive (18), firmware (19) |

**Which distinctions are lost, and whether the loss matters:**

1. **Fast I/O event (2 ms) and registration correction (50 ms) share Critical** — a 25× spread in the same class. Harmless here, but only because they never share a Link: the event lives on CAN-FD and the correction on Ethernet. The isolation comes from the topology, not from the QoS field.
2. **Four bulk classes share Background with genuinely different urgency.** A firmware campaign in a maintenance window should outrank a routine defect archive; a triggered dump during a bearing alarm should outrank ordinary image traffic. There is no expression for that, and `CORE §14.2`'s two named disciplines schedule *between* classes, not within one.
3. **The continuous stream (1.84 MB/s) and drive parameter access (100 B) share Normal**, semantically unrelated. Head-of-line cost is one 1,542 B block = 123 µs on `VibChain`. Tolerable, and only because the link is 15% loaded.
4. **QoS cannot express a deadline or a rate, and the docs say so.** `CORE §14.2`: "Neither discipline provides a deadline, a latency bound, bus-wide fairness, or a real-time guarantee." Three of this machine's four hard constraints — 500 µs cycle, ±1 µs phase, 2 ms scan — are deadlines.
5. **On `MachineEth` the effective scheduler is a switch**, and WS QoS is a mark (SF-R6-027).
6. **On `MotionBus` QoS is one of the elided fields** (§3.5). Inside a static schedule there is no priority to express; inside the acyclic region, arbitration belongs to the motion master and WS QoS is advisory at best.

> **Is QoS even the right mechanism for classes 1–3?** No, and the archetype's suspicion is exactly right. Those are **schedule** properties — a guaranteed instant and a guaranteed period — and priority is a mechanism for deciding *order among things already accepted*. `CORE §14.2` is explicit that strict priority cannot produce a deadline. Placing cyclic motion behind a Critical QoS class would be a category error that happens to produce plausible-looking configuration.

> **Where QoS does earn its keep — exactly one of six links.** On `IOBus_A`/`IOBus_B`, canonical QoS occupies the most arbitration-significant identifier bits (`LINK §2.2`), so the field maps onto a real medium arbitration. Promoting the fast-event class from QoS 1 to QoS 0 changes the worst-case sensor→`MotionCore` latency from **1,796 µs to 456 µs** against a 2,000 µs budget — margin from 10% to 77% (§11). Everywhere else QoS is unused (`MotionBus`), advisory (`MachineEth`), or unnecessary (`CtrlSHM` ample, `VibChain` 15% loaded).

### 10.6 The Wire-scope trap

`CommsCore` is on both `MotionBus` and `MachineEth`. The archetype is right that this is the sharpest structural question.

**Case 1 — a Wire spans `MotionBus` and `MachineEth`.**

Under base flood-and-filter (`CORE §12.1`), `CommsCore` "may forward a directed PDU to every configured branch of Wire 42 and allow non-destination participant Domains to reject it." Consequences on a 500 µs cyclic link:

```text
one 1500 B PDU forwarded onto MotionBus
    1500 B x 8 / 100 Mbit/s  =  120 us  =  24.0% of a 500 us cycle
    -> the cycle overruns, or the frame is deferred and the schedule slips

one 3 MB image frame
    3.15 MB / 12.5 MB/s      =  252 ms  =  504 consecutive cycles

one 4 MB firmware image
    4.19 MB / 12.5 MB/s      =  25.2 s  =  50,400 consecutive cycles
```

Against the archetype's requirement that a drive "detect and enter a safe state within a bounded number of cycles" on a missed cycle, forwarding a single ordinary Ethernet PDU onto `MotionBus` is a machine-stopping event and forwarding an image is a web-tearing one. **Flood-and-filter is not inefficient here; it is unsafe.** This is the second independent confirmation of SF-R6-013, and it raises the severity: archetype 11 found pruning to be a *viability* requirement on a scarce radio, and this archetype finds it to be a *safety* requirement on a scheduled link.

**Case 2 — no Wire spans both.** Then `CmAnalyzer`, `HmiCore`, `VisionController`, `RemoteGateway`, and engineering tooling reach a drive's Identity / Health / Fault / parameter / update Services only via composition or a splice.

| Route | Identity | Acyclic budget | Verdict |
|---|---|---|---|
| Composition at `CommsCore` | **lost** — `Src = CommsCore` for all 48 drives | still consumed | Rejected: `CommsCore` becomes a proxy for 48 devices, destroying the corpus's strongest repeated result |
| Splice `W_EthService` → `W_MotionService` | preserved | still consumed | Valid but wasteful: two Wires with identical membership semantics and no scope difference, so the splice projects between scopes that are the same scope (`CORE §6.1`) |
| One spanning Wire with destination-pruned egress | preserved | still consumed | **Chosen** |

**Choice, justification, and cost.**

I chose the spanning Wire (`W6 DriveService`) with mandatory destination-pruned egress. Justification: canonical source must survive to the drive — `RemoteGateway` initiating a firmware update on `Drive_17` has to arrive as `Src = RemoteGateway` for traceability, and a proxy would show `Src = CommsCore` for every Service operation on every drive, which is precisely the conventional loader-proxy pattern WS exists to remove. The splice preserves identity equally well but buys nothing the spanning Wire does not, because there is no scope difference to project.

The cost has three parts and the third is the interesting one:

1. **Destination-pruned egress is mandatory**, per Case 1. Its "optional" framing in `CORE §12.1` is wrong on this link.
2. **Pruning is necessary but not sufficient.** It solves flooding, not admission. A 4 MB firmware image *correctly* addressed to `Drive_17` is *correctly* pruned to `MotionBus` alone, and still must fit in a 512 kB/s acyclic region. `CommsCore` must therefore meter it — mechanically available through the LLL's bounded TX queue and `kCongestedLink` (`CORE §14.4`, §15.7), but it means Services on this Wire see congestion rejection as the **steady state** rather than as an exception, which inverts `CORE §15.2`'s guidance to "engineer normal traffic below capacity."
3. **The mismatch is unstatable.** `W6` spans a 125 MB/s link and a 512 kB/s channel — a **244:1 ratio** — behind a hard deadline, and nothing in the Wire, the Link Binding, or the capability descriptor records that. A tool applying every check in `DEPLOY §2.3` would pass this configuration (SF-R6-021).

**Does this archetype break "canonical source preserved across heterogeneous links"?** **No — and only because the cyclic core is below WS.** Every Service relation to a drive preserves `Src` end to end across Ethernet, shared memory, and the scheduled link's acyclic region, through one forwarder and zero splices. Had the cyclic exchange been required to cross the same boundary, the answer would have been different: the summation frame has no source field, so identity there is a configuration fact rather than a carried one, and the property would have become unobservable rather than preserved (§3.2(e)). The corpus result survives contact with this archetype, but it survives *on the WS side of a boundary this archetype forced me to draw*.

---

## 11. Ledgers

### Timing ledger — `MotionBus` per-cycle byte budget

**A48: 500 µs cycle at 100 Mbit/s = 6,250 bytes per cycle.** Assumptions stated inline; per-node forwarding delay of 0.6 µs and a 10% jitter guard are typical passing-frame figures and are converted to byte equivalents at 12.5 B/µs.

| Item | Bytes | % of budget |
|---|---:|---:|
| Ethernet framing (preamble, SFD, header, FCS, IFG) | 38 | 0.6% |
| Motion protocol header/trailer (assumed) | 12 | 0.2% |
| Sync / tick datagram (assumed) | 32 | 0.5% |
| Setpoint image, 48 × 7 B | 336 | 5.4% |
| Feedback image, 48 × 7 B | 336 | 5.4% |
| Line reference | 6 | 0.1% |
| Safety region (48 × 16 B per 8 ms = 96 B/cycle) | 96 | 1.5% |
| **Acyclic / mailbox region (chosen)** | **256** | **4.1%** |
| Node forwarding delay, 50 nodes × 0.6 µs = 30 µs | 375 eq | 6.0% |
| Jitter / guard band, 10% of cycle = 50 µs | 625 eq | 10.0% |
| **Committed** | **2,112** | **33.8%** |
| **Headroom** | **4,138** | **66.2%** |
| *If WS owned the cyclic path: +6 B × 96 canonical headers* | *+576* | *+9.2%* |

The header row is the one worth staring at. It **fits** — 43.0% committed instead of 33.8% — which is why §10.1's frame-rate ceiling and §10.1's per-PDU processing cost, not the header bytes, are what make the cyclic path impossible.

**Acyclic bytes/s and what they buy:** 512 kB/s gross, ~469 kB/s of Service payload after 8.3% per-PDU overhead. That buys full Identity/Version/Health enumeration of 48 drives in 10–36 ms, a cam table in 70 ms, and a firmware image in 8.9 s — with the machine-mode broadcast to drives (3 kB/s, 0.6%) as the only production-critical consumer.

**Registration-correction latency chain:** 41.7 ms nominal of a 50 ms budget (§10.4), **50–74 ms under the archetype's §8 simultaneous load — budget not met.**

**Fast-I/O latency chain (budget 2,000 µs), and the one place QoS changes the answer:**

| Stage | At QoS 1 | At QoS 0 |
|---|---:|---:|
| `IoIsland` detects and builds the PDU (assumed) | 100 µs | 100 µs |
| CAN-FD arbitration: in-progress frame + queued equal-priority | 1,008 µs | 168 µs |
| Frame time, 12 B payload at 2 Mbit/s | 88 µs | 88 µs |
| `LogicCore` ingress, route, `CtrlSHM` to `MotionCore` (assumed) | 100 µs | 100 µs |
| `MotionCore` consumes at its next scan | ≤500 µs | ≤500 µs |
| **Total** | **1,796 µs** | **456 µs** |
| **Margin** | **204 µs (10%)** | **1,544 µs (77%)** |

**`IOBus_A` utilization at 2 ms scan:** 3 islands × (168 µs input report + 88 µs output command) = 768 µs = **38.4%**; ~45% with Health and events. A single Wire spanning both segments would double it to ~77–90% and consume the margin above — which is why §3.2(b) uses two Wires.

**Safety watchdog margin:** the 8 ms watchdog with 48 drives at 16 B dual-channel requires 768 B per 8 ms window = **96 B/cycle**, which is what the budget above reserves. A naive 16 B/cycle slot would complete a 48-drive round in 48 cycles = **24 ms**, three times the watchdog — the arithmetic drove the allocation rather than confirming it.

### Stream ledger

| Stream | Link | Bytes/s | PDU size | PDUs/s | Header % | What absorbs a stalled consumer |
|---|---|---:|---|---:|---:|---|
| Raw vibration (12 sensors) | `VibChain` | 1.84 MB/s | 1,542 B | 1,200 | **0.39%** | `CmAnalyzer` Queue depth 32/sensor (49 kB each, 592 kB total, 320 ms), then `rejected_full` |
| Envelope / RMS | `VibChain` + `W3` | ~2 kB/s | ~40 B | 120 | 15.0% | Snapshot — coalescing is correct here |
| Triggered dump | `VibChain` | ≤10.6 MB/s burst | 1,542 B | ≤6,900 | **0.39%** | Queue depth 64 (98.7 kB/sensor); no gap detection |
| Image frames (MTU) | `MachineEth` | 24 MB/s | 1,462 B | 17,288 | **0.41%** | `VisionController` Queue depth 2 frames ≈ 6 MB, then reject + counter |
| Image frames (jumbo) | `MachineEth` | 24 MB/s | 8,960 B | 2,816 | **0.067%** | same |
| Defect archive | `MachineEth` | 3 MB/defect | 1,462 B | burst | 0.41% | `HmiCore` storage; Background QoS |
| Firmware to a drive | `MachineEth` → `MotionBus` | ≤512 kB/s | ~120 B | ≤4,000 | 8.3% | `CommsCore` TX queue → `kCongestedLink` as the steady state |
| *Cyclic, if WS owned it* | `MotionBus` | *2.50 MB/s* | *13 B* | *192,000* | ***46.2%*** | *n/a* |

The last row against the other seven is the whole archetype in one table.

### QoS assignment

See §10.5 for the full nineteen-class enumeration, the four-value mapping, and the six named losses. Summary: **QoS changes a measurable outcome on 1 of 6 links.**

### Scaling — A24 → A48 → A64

| Quantity | A24 (1 kHz) | A48 (2 kHz) | A64 (4 kHz) | Growth |
|---|---:|---:|---:|---|
| Production Participants (Config C) | 53 | 77 | 93 | linear; inside 8-bit with 127 spare |
| **Wires** | **6** | **6** | **6** | **constant** |
| **Wire→LinkBitmask objects** | **4** | **4** | **4** | **constant** |
| **Splices** | **0** | **0** | **0** | **constant** |
| Physical Links | 6 | 6 | 6 | constant |
| ParticipantId renumbering | none | none | none | reserved 0x40–0x7F block |
| Canonical header, % of `MotionBus` | 2.30% | 9.22% | **24.58%** | quadratic in (axes × rate) |
| Cyclic PDUs/s if WS owned it | 48,000 | 192,000 | 512,000 | |
| `CtrlSHM` per-PDU cost if WS owned it | ~2.4% of a core | ~9.7% | ~25.8% | |
| MotionBus cycle utilization (as mapped) | 22.4% | 33.8% | 66.5% | |

**Which constraint binds first?** Not bandwidth, and nothing WS owns. Solving the per-cycle budget for axis count on 100 Mbit/s:

```text
   n  <=  (11.25 T - 344) / (21.5 + T/500)        T = cycle time in us

   T = 500 us (2 kHz)   ->  n <= 234 axes
   T = 250 us (4 kHz)   ->  n <= 112 axes
   T = 125 us (8 kHz)   ->  n <=  48 axes
```

So A24, A48, and A64 all sit far inside the wire limit, and **the architecture stops working at roughly 110 axes at 4 kHz or 48 axes at 8 kHz.** In both cases the binding term is the same and it is a schedule property: accumulated per-node forwarding delay plus the jitter guard, which together are 807 B of 3,125 (25.8%) at A64 and 651 B of 1,562 (41.7%) at 48 axes / 8 kHz. Forwarding delay grows with node count and the guard shrinks with cycle time, so the two close from both sides.

**On the WS side nothing binds at any variant**, which is the clean part of the result: 93 Participants, 6 Wires, 4 forwarding objects, 0 renumbering. The mapping survives A24 → A64 without qualitative change because the part that scales was placed below the boundary.

---

## 12. Scope boundary (mandatory deliverable)

```text
+--------------------------------------------------------------------------+
| BELOW WIRESPACES                                                          |
|   MotionBus cycle schedule and slot map                                   |
|   cyclic setpoint exchange       48 x 7 B per 500 us                      |
|   cyclic feedback exchange       48 x 7 B per 500 us                      |
|   line reference                  1 x 6 B per 500 us                      |
|   cycle tick and +/-1 us phase reference (distributed clocks)             |
|   functional safety chain framing (opaque 16 B, 96 B/cycle region)        |
|   inner current / velocity / position servo loops                         |
|   VibChain per-hop switching; MachineEth switch queueing                  |
|                                                                            |
|   = 5 of 34 relation classes, ~192,000 of ~213,000 PDU-equivalents/s      |
|   = 15% of the relationships, 90% of the messages                         |
+--------------------------------------------------------------------------+
| WIRESPACES OWNS                                                           |
|   identity of all 78 Participants, including all 48 drives                |
|   drive Services over the acyclic region: Identity, Version, Health,      |
|     fault detail, parameters, cam tables, logs, firmware, Link status     |
|   machine logic and distributed I/O (IOBus_A/B, CtrlSHM)                  |
|   machine state / mode, alarms, interlock reporting                       |
|   condition-monitoring control plane and the vibration block stream       |
|   vision control plane, registration correction, defect/reject signalling |
|   HMI, production data, recipes, telemetry, diagnostics                   |
|   wall-clock time                                                          |
|                                                                            |
|   = 29 of 34 relation classes, 6 Wires, 4 forwarding objects, 0 splices   |
+--------------------------------------------------------------------------+
| BESIDE WIRESPACES                                                         |
|   raw vibration sample blocks and image payloads TRAVEL as WS PDUs with   |
|   negligible header cost (0.39% / 0.41%), but every stream property the   |
|   application needs is Service-authored because no Transport defines it:  |
|     sequencing, gap detection, reassembly of an 18 MB object,             |
|     retry, and end-to-end backpressure                                    |
|                                                                            |
|   These move INSIDE WS the day a sequenced / segmenting Transport exists  |
|   (CORE 20, FUTURE 3.1). Nothing else is needed.                          |
+--------------------------------------------------------------------------+
```

**The boundary is the drive's communication slot, not the drive.** A drive is a full WS Participant with a ParticipantId, an Endpoint set, and the standard Service catalogue; what is outside WS is one of its two data paths.

---

## 13. Verdict against the §0 falsification criteria

### Cyclic core — **UNSUITABLE**, by six of six criteria

| Criterion | Result | Evidence |
|---|---|---|
| **F1** individual framing exceeds capacity or frame rate | **MET** | 129.0 Mbit/s on a 100 Mbit/s link; 192,000 fps against a 148,809 fps physical ceiling (§10.1) |
| **F2** header exceeds 10% of link capacity at any variant | **MET at A64** (24.58%); missed at A48 (9.22%) and A24 (2.30%) | §10.2 |
| **F3** requires eliding ≥5 of 6 header bytes | **MET** | 6 of 6, 100% (§3.5, §10.2) |
| **F4** per-PDU handling exceeds 5% of a core | **MET** | 9.7% at A48, 25.8% at A64 across `CtrlSHM` (§10.1) |
| **F5** no representation for group atomicity, and no way to detect its absence | **MET** | §14 Question C; SF-R6-025 |
| **F6** ±1 µs unachievable by any WS Service | **MET** | §14 Question D; SF-R6-024 |
| **S1** header <10% of cyclic traffic and <2% of capacity | **FAILS** | 46.2% and 9.2–24.6% |
| **S2** at least one field the profile does not statically imply | **FAILS** | zero |
| **S3** jitter and group atomicity expressible or checkable | **FAILS** | neither; SF-R6-021, SF-R6-025 |

### Streams — **carriage SUITABLE, transport UNSUITABLE**

| Criterion | Result | Evidence |
|---|---|---|
| **F7** header overhead >5% at a schedulable block size | **NOT MET** | 0.39% at 1,536 B (10 ms blocks); 0.41% at 1,456 B image chunks (§10.4) |
| **F8** declared storage loses data with no counter | **PARTIALLY MET** | A Queue counts every local loss (`rejected_full`, high-water, replacement). **Wire loss is not countable at the receiver** — a PDU dropped in transit is invisible to `CmAnalyzer` unless the Service carries its own sequence number. Choosing Snapshot would lose silently, but `CORE §9.5` tells you not to, and that guidance is unambiguous. |
| **F9** 18 MB needs a mechanism the model does not define | **MET** | `TransportType` has room and no defined segmenting Transport; the Service must author sequencing, windowing, and retry (§10.4) |
| **S4** header <1% at a defensible block size | **MET** | 0.39% / 0.41% / 0.067% |
| **S5** a declared storage class matches, with a countable loss path | **MET** | Queue with declared depth; 592 kB buys 320 ms of stall tolerance |
| **S6** segmentation, sequencing, gap detection exist or have a stated home | **PARTIALLY MET** | `CORE §21.3` explicitly names sequence, duplicate, correlation, and restart semantics as Service-schema content, so there *is* a stated place. There is no Transport, so every Service authors its own and none interoperate. |

**Net:** Endpoint storage is **not** the wrong abstraction for streams — a bounded Queue with a declared depth is precisely right, and the model even tells you why a Snapshot would be wrong. What is missing is the **Transport**, not the storage model. That is a narrower and more actionable conclusion than "WS needs a stream concept," and `CORE §20` and `FUTURE §3.1` already name the thing that would close it.

---

## 14. Archetype questions A–J

**A — Cyclic determinism.** A Wire cannot carry a 2 kHz exchange with bounded jitter, and the reason is definitional rather than a shortcoming: `CORE §3.6` states that a Wire supplies no reliability, ordering, freshness, deadlines, flow control, or duplicate suppression, and each exists only where something explicitly provides it. The Link profile owning the schedule is **correct layering** and I would not change it. The missing piece is the declaration. `CORE §13.4` names timing as one of three dimensions in which a placement can fail and requires that an unrepresentable placement fail at configuration time; `CORE §17` gives size several capability fields and timing exactly one (`polling cadence, for master-initiated Links`). So a Link can *have* a 500 µs schedule and no Link can *say* so, and `DEPLOY §2.3` therefore cannot reject any of the four configurations that would stop this machine. Missing piece, at the capability layer, not the data plane (SF-R6-021).

**B — Canonical header on summation-frame links.** §10.1 and §10.2 are discharged above. Can `LINK` support this link class at all? Not today — it has no aggregating profile class, and `CORE §12.5`'s aggregation triggers (MTU, latency, high-QoS, idle) do not include a cycle boundary; five further gaps are enumerated in §3.5 and SF-R6-023. **And if the answer is "only by eliding every canonical field," is that the elision principle working as designed, or the boundary of WS?** It is the boundary, and the arithmetic says so more clearly than the principle does: eliding all six bytes is legal under `CORE §2.2`, so a conforming profile transmits zero canonical bytes, and the *only* remaining WS contribution is that `Drive_17`'s slot and `Drive_17`'s Identity Service are one identity rather than two. That is a real benefit and it is conditional on `DEPLOY §2.5`'s "one authoritative Wiring source generates every participating projection" holding across a non-WS protocol's own tooling — which nothing enforces. So: legal elision, genuine but conditional value, and the honest description is a boundary rather than a supported link class (SF-R6-022).

**C — Group atomicity.** WS offers nothing, and I checked each candidate the archetype names. *Broadcast plus a locally aligned tick* fails because `CORE §3.2` broadcast is bounded fan-out with no statement about when recipients act, and `CORE §9.4` explicitly decouples the two — "a Service consumes what was accepted later, in a context it owns" — so even a perfectly simultaneous broadcast produces no common application instant. *A control-metadata field* does not exist: `Control` is QoS[2] + Reserved[2] + HasHeaderExtensions[1] + TransportType[3] with no spare bits, and header extensions are self-describing in length but undefined in content (`CORE §2.3`), so using one would be protocol invention. *Arrival timestamp* (`CORE §9.4`) is a local tick with no cross-Participant meaning; it can tell a drive how old its setpoint is and cannot tell 48 drives to act together. So the Link profile's cyclic slot semantics own it entirely. **Correct layering for the mechanism; a gap in the declaration.** The specific harm is not that WS lacks a mechanism — it is that a Service requiring group atomicity can be placed on a Link that cannot provide one and *nothing notices*, because `CORE §13.4`'s compatibility envelope has size, timing, and transport but no coordination dimension (SF-R6-025).

**Coordinated phase-in** is the second half of §7 and lands differently. Forty-eight axes enabling, homing, and ramping into phase lock in a partly ordered sequence with abort is a distributed group state machine across 49 Participants. The *sequencing logic* is machine-specific and belongs in the application. The *shape* is not: a coordinator commands a target state to a set, each member reports actual state plus a blocking reason, and abort is available at any point. That is a Service, it flattens to ordinary Endpoints under `CORE §19.2`, and the standard set has no member for it. Archetype 11 needed flight mode and archetype 12 needed commissioning state; three independent rediscoveries of the same shape is an ecosystem signal rather than a protocol gap, and it is recorded in §7 rather than filed.

**D — Time synchronization at ±1 µs.** The WS Time Service is not a candidate, and the reason is structural rather than a matter of implementation quality. A Service message crosses the Endpoint storage boundary; arrival time is captured at acceptance (`CORE §9.4`), which is **after** the LLL, after reassembly, after routing, and possibly after a scheduling delay, and the model defines no transmit-side timestamp, no residence-time correction across a forwarder, and no hardware timestamp point. PTP-class accuracy comes from timestamping in the MAC/PHY and correcting residence time hop by hop; WS timestamps three layers above that. Quantitatively, one 1500-byte frame on 100 Mbit/s is 120 µs, so acceptance-time uncertainty is at minimum two orders of magnitude above 1 µs on the very link that needs it — and on shared memory the bound becomes the consumer's task period.

So phase synchronization must be a hardware / Link-profile property below WS: distributed per-node clocks with propagation-delay measurement, which is what real motion buses do and what reaches sub-100 ns. **What happens to the claim that standard Services span all Link types coherently?** It survives, but only after admitting that "time" is not one thing. **Is time actually two different things the model conflates?** Three:

```text
wall-clock / epoch      a Service. Spans every Link. ~1 ms here. Log timestamps,
                        trend records, defect archive metadata.
Link phase reference    NOT a Service. A property of one Link profile. Scoped to
                        that Link's Participants. NOT transitive across a
                        gateway. +/-1 us only where the Link provides it.
arrival time            CORE 9.4. A local monotonic tick. No cross-Participant
                        meaning at all. Correctly used by CORE 21.4's freshness
                        machinery, which is why the model is internally
                        consistent even though the vocabulary is not.
```

The product consequence makes the abstraction concrete: `VibSensor_07` is on `VibChain` and `Drive_17` is on `MotionBus`, so they share wall-clock and can never share phase. A bearing event therefore cannot be aligned to a print registration event better than ~1 ms, which at 300 m/min web speed is **5 mm of material** — enough to attribute a defect to the wrong impression. This is the archetype's most productive question and I agree with its framing (SF-R6-024).

**E — QoS expressiveness.** §10.5 is discharged above: nineteen classes corrected from the archetype's twelve, five of them below the WS boundary, fourteen mapped onto four values, six losses named. The two results worth repeating are that **three of this machine's four hard constraints are schedule properties that QoS is structurally the wrong mechanism for** — `CORE §14.2` says as much — and that **QoS changes a measurable outcome on exactly one of six links**, the CAN-FD I/O buses, where it moves the fast-event margin from 10% to 77%. That is a genuine and quantified positive result in a section otherwise full of negatives, and it is worth noting that it comes from `LINK §2.2`'s mapping of canonical QoS onto arbitration bits — i.e. from the Link profile doing its job, not from the QoS field being expressive.

**F — Streams versus Endpoint storage.** §10.4 and §13 are discharged. The conclusion disagrees mildly with the archetype's suggested framing and I want to be explicit that it is not bias: **Endpoint storage is the right abstraction and the Transport layer is the wrong (missing) one.** A bounded Queue with declared depth is exactly a block stream's semantics; `CORE §9.5` even supplies the test that rules out Snapshot, and the depth-as-protocol-claim rule tells you how to size it (592 kB for 320 ms of stall tolerance across twelve sensors). What is absent is everything above storage: no defined segmenting Transport for an 18 MB object, no sequencing, no receiver-side gap detection for wire loss, no end-to-end backpressure. The evidence that this is a Transport gap and not a storage gap is that the *same* Endpoint model handles 24 MB/s of images and 1.84 MB/s of vibration at 0.4% overhead without complaint, and fails only where an object exceeds one slot. `CORE §20` already states the direction ("bulk belongs to a segmenting Transport") and `FUTURE §3.1` already names the likely candidate. Should WS declare streams out of scope? No — declaring them out of scope would put a 24 MB/s flow beside the model on a link the model also carries control traffic on, which is the worst of both. The narrow answer is the right one.

**G — Devices behind a scheduled link.** The Service ecosystem story survives, with one condition and one cost. It survives because §10.6's spanning Wire preserves canonical source end to end: `RemoteGateway` → `MachineEth` → `CommsCore` → the acyclic region → `Drive_17`, one forwarder, zero splices, `Src = RemoteGateway` intact at the point of action. And §10.3 shows the acyclic budget is adequate for everything except firmware: Identity/Version/Health for all 48 drives in 10–36 ms, a cam table in 70 ms, and a 4 MB image in 8.9 s that nonetheless belongs in a maintenance window because it starves the other 47 drives' fault reporting for seven minutes.

The **cost** is that Service traffic and the control loop genuinely do compete, and the competition is arbitrated by a scheduler WS does not own. The **condition** is that destination-pruned egress and acyclic admission control at `CommsCore` are both mandatory, and neither is expressible in the Wiring. So the archetype's dichotomy — "either the gateway becomes a proxy (losing canonical source) or Service traffic competes with the control loop" — resolves as **the second horn, accepted deliberately**, because the competition is bounded by a byte budget the schedule enforces while the identity loss would have been permanent and unbounded.

**H — Interference and isolation.** No. The model provides no way to say "this link's capacity is reserved and shall not be consumed by that Wire." I checked each candidate: `CORE §15.2` static traffic engineering is a design-time projection, not a runtime reservation; `CORE §14` QoS is priority, not share; `CORE §15.7` bounds every storage element but bounds storage rather than bandwidth; `CORE §15.5` credit meters receiver storage in a profile-defined quantum, hop by hop, and is explicitly "not a global congestion algorithm."

**Is "draw narrower Wires and use QoS" adequate at 500 µs?** It splits three ways, and only one third is a WS success.

- **Across physically separate media it is adequate, and the credit belongs to Wire scope.** No Wire spans `IOBus` and `MachineEth`, and none spans `VibChain` and `MachineEth`, so a saturated Gigabit network cannot reach the I/O buses or the vibration chain at all. That is a real WS contribution and it is why `CmAnalyzer` backing up has provably zero effect on motion.
- **On a shared switched fabric it is not adequate and Wire scope contributes nothing.** `W3` and `W4` share every byte of `MachineEth`'s capacity; the switch is not a Participant; WS QoS is a mark. The ≤50 ms correction budget is missed under the archetype's own load scenario (SF-R6-027).
- **On the cyclic link neither mechanism applies.** No QoS value makes a frame shorter, and one unscheduled 1500-byte frame is 24% of a 500 µs cycle. The only adequate mechanism is admission by schedule — the acyclic region — which is a Link-profile concept WS cannot see, size, or check.

**I — The scope boundary.** Produced in §12. I do propose the split the archetype anticipates, with one refinement: raw streams are *beside* WS today only because no Transport defines their semantics, not because their carriage is a problem — the header cost is 0.39%, and they move inside the moment `FUTURE §3.1`'s sequenced datagram exists.

**With the cyclic core outside WS, is WireSpaces still worth adopting on this machine?** Yes — for a **different reason than archetype 08's**, and conditionally.

Archetype 08 answered yes because a flat Ethernet rack still had twenty device types needing one Service model; the argument was breadth. Here the argument is *necessity plus arithmetic*. Necessity: the 48 drives are reachable only through the scheduled link, so the Service plane and the motion plane must share one configuration or the drives are unmanageable — and WS is the only element in the architecture with a name for `Drive_17`'s Identity Service that is also the name the slot map uses. Arithmetic: **WS is excluded from 90% of the messages and included in 85% of the relationships**, and integration pain scales with relationships, not with frames. Seven vendor catalogues is the problem this machine actually has, and 29 of its 34 relation classes are inside the boundary.

The answer is weaker than 08's in one respect that should be said plainly: WS contributes nothing to this machine's performance, and a vendor motion stack already ships a drive Service plane over its own mailbox protocol, so WS displaces an incumbent rather than filling a void. And it is **conditional**: WS earns adoption if and only if the MotionBus Link profile exists (SF-R6-023) and the slot map is generated from the same Wiring source as everything else (`DEPLOY §2.5`). If the motion bus keeps its own tooling and identity space, WS becomes a second identity universe bolted alongside the first (`CORE §4.6`), the drives are reachable only through a proxy, canonical source is lost, and the argument collapses. So the honest verdict is: yes, and the condition is a toolchain integration rather than a protocol feature — which is the least glamorous and most actionable place for a boundary to sit.

**J — Scaling A24 → A48 → A64.** The mapping survives without qualitative change. Wires, forwarding objects, splices, Links, and ParticipantId allocation are all constant; only Participant count and slot-schedule numbers move, and 93 Participants at A64 sit inside the ordinary 8-bit range with 127 spare. **The mapping survives precisely because the part that scales badly was placed below the boundary** — had the cyclic exchange been a Wire, the canonical header would have gone from 2.30% to 24.58% of the link and the `CtrlSHM` per-PDU cost from 2.4% to 25.8% of a core across the same three variants, and A64 would have required a redesign.

**Which constraint binds first, and where does the architecture stop working?** Not bandwidth, not WS, and not the acyclic budget. Solving the per-cycle budget (§11) gives **234 axes at 2 kHz, 112 at 4 kHz, and 48 at 8 kHz** on 100 Mbit/s, and in every case the binding term is accumulated per-node forwarding delay plus the jitter guard — 807 B of 3,125 (25.8%) at A64, and 651 B of 1,562 (41.7%) at 48 axes / 8 kHz. Forwarding delay grows with node count while the guard's byte cost shrinks with cycle time, so the two close from opposite directions. Every one of those numbers is a MotionBus schedule property, which is a fitting last result: **the constraint that binds this machine is one the model cannot see, on the link the model does not carry.**

---

## Devices (reference)

| Device | Endpoint Domains | Link interfaces | Role summary |
|---|---|---|---|
| MachineController | `MotionCore`, `LogicCore`, `CommsCore`, `HmiCore` | `CtrlSHM` (all four); `IOBus_A`, `IOBus_B` (`LogicCore`); `MotionBus`, `MachineEth` (`CommsCore`); `MachineEth` (`HmiCore`) | 0x01–0x04; the machine's only multi-Link forwarder is `CommsCore` |
| `Drive_01` … `_48` | one each | **`MotionBus` only** | 0x40–0x6F; cyclic slots below WS, full Service set over the acyclic region |
| `LineEncoder` | one | `MotionBus` only | 0x05; 2 kHz master position, a cyclic slot |
| `SafetyController` | one | `MotionBus` only | 0x06; dual-channel chain in a reserved 96 B/cycle region; opaque to WS |
| `IoIsland_1` … `_6` | one each | one CAN-FD segment | 0x10–0x15; 40 DI + 8 AI, 2 ms scan, fast events at QoS 0 |
| `VibSensor_01` … `_12` | one each | `VibChain` | 0x20–0x2B; 153.6 kB/s each, 1,536 B blocks at 100/s |
| `CmAnalyzer` | one | `VibChain`, `MachineEth` | 0x30; **forwards nothing** — composes the machine-facing summary and the dump archive |
| `Camera_A` / `_B` | one each | `MachineEth` | 0x32 / 0x33; 3 MB frames at 4 fps, hardware-triggered from line position |
| `VisionController` | one | `MachineEth` | 0x31; registration correction at QoS 0, defect archive at QoS 3 |
| `HmiPanel` | one | `MachineEth` | 0x34; no production impact when lost |
| `RemoteGateway` | one | `MachineEth` | 0x35; **Config D only**; the Participant that makes pruning and admission control non-optional |

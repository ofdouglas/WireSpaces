# Sketch 13 — Hard-Real-Time Cyclic Motion + High-Rate Streaming

**Archetype:** `archetypes/13_cyclic_motion_and_streams.md`  
**Agent:** `gpt56sol_20260823`  
**Output file:** `sketches/13_cyclic_motion_and_streams_gpt56sol_20260823.md`  
**Date:** 2026-08-23

---

## Executive summary

**Minimum mapping:** Config C uses 77 WS Participants and four service/control Wires; cyclic motion, phase synchronization, safety framing, raw vibration, and image payloads remain below or beside WS.  
**Question A (R6):** **Natural for identity and Services; poor for cyclic and continuous-stream data planes** — narrow Wires preserve drive identity through a fixed MotionBus mailbox, but R6 supplies neither the scheduled process image nor bulk-stream semantics.  
**Question B (CAN11):** **N/A** — no Classical CAN11 Link exists.  
**Worst friction (minimum path):** **Configuration burden — Significant** because one deployment must consistently generate WS Wiring, a fixed MotionBus schedule/mailbox, CAN-FD timing, bulk-stream shaping, and cross-artifact identity/version checks.  
**Main lesson:** Per-axis Ethernet PDUs already exceed MotionBus capacity at A48, while static process-image offsets fit easily by eliding every per-axis canonical field; that is the boundary between WS service traffic and the native cyclic protocol.

---

## Disposition block

| Area | Assessment |
|---|---|
| Participant identity | Natural |
| Wire decomposition | Natural after explicit control/bulk boundaries |
| Forwarding | Moderate |
| Non-CAN configuration | High |
| CAN11 VCN fit | N/A |
| Better with CAN29? | N/A |

**Question A (R6):** Deployment-global Participants and narrow service Wires cleanly preserve drive, I/O, condition-monitoring, and vision identities. The canonical model is silent on fixed slots, ±1 µs phase, group-atomic apply, continuous-stream sequencing, and large-object transfer; those data planes therefore remain native. **Question B (CAN11):** no VCN evidence is produced.

---

## Falsification criteria

I will judge WS **UNSUITABLE for the cyclic core** if:

- A48 per-axis PDUs exceed the `6,250 B/cycle` MotionBus capacity or leave no defensible processing/timing margin;
- retaining the native summation frame requires statically eliding every canonical field for every axis slot;
- ±1 µs phase and simultaneous 48-axis apply are supplied entirely by native hardware/schedule rather than a named WS mechanism; or
- four QoS values are presented as substitutes for cycle slots, phase, or watchdog scheduling.

I will judge WS **SUITABLE for the cyclic core** only if:

- all 48/64 axis Participants remain canonical cyclic sources/destinations;
- A64 fits 250 µs with explicit worst-case serialization, processing, and phase arithmetic;
- a specified existing mechanism supplies cycle completeness and group-atomic application; and
- drive Services coexist without changing cyclic timing.

I will judge WS **UNSUITABLE for continuous streams** if:

- a Snapshot Endpoint silently overwrites vibration/image blocks;
- bounded Queues merely move an unbounded stall into overflow without a specified sequencing/backpressure/bulk-transfer mechanism;
- a lost fragment of an 18 MB dump or 3 MB image has no defined gap/recovery behavior; or
- registration correction can sit behind an unbounded bulk queue for more than 50 ms.

I will judge WS **USEFUL beside streams** if:

- canonical Services control acquisition, report Health, and publish diagnoses/events;
- raw payloads use a named bounded bulk mechanism beside WS; and
- physical queueing/shaping prevents those payloads from affecting MotionBus, I/O, or correction deadlines.

The criteria are evaluated in §6.9.

---

## 1. Native communication model

The native machine has three different communication shapes:

1. a fixed cyclic process image on a 100-Mbit/s passing-frame network, with distributed phase and static per-node offsets;
2. bounded commands, status, diagnostics, and configuration across shared memory, CAN-FD, scheduled mailboxes, and switched Ethernet; and
3. continuous or bursty bulk data from vibration sensors and cameras.

The first is a schedule. The second is a datagram/Service system. The third is a flow of blocks and multi-megabyte objects. Treating all three as interchangeable messages would erase the constraints that distinguish them.

---

## 2. Obvious conventional implementation

> **Obvious conventional implementation:** Keep the electronic line shaft, axis image, distributed phase, and safety watchdog in the native scheduled MotionBus; use fixed mailbox bytes for drive diagnostics; use CAN-FD schedules for I/O; use TCP/native bulk channels and switched-Ethernet queues for vibration and vision.

> **What structure does a conventional design use?** Every drive owns static command/feedback offsets in one passing frame. Controller and drives share a hardware phase reference. Service requests use a bounded acyclic slot. Vibration and image receivers own stream/object buffers, sequence numbers, and flow control. Small diagnoses and corrections are separate high-priority messages.

---

## 3. Minimum mapping

### 3.1 Scope boundary — mandatory deliverable

```text
BELOW WS
    MotionBus cyclic setpoint/feedback image
    static slot/offset schedule and passing-frame mechanics
    cycle tick and +/-1 us hardware phase synchronization
    group-atomic apply at the named tick
    opaque functional-safety framing and watchdog exchange

WS OWNS
    canonical Participant identity and Standard Services
    drive Service traffic in the fixed MotionBus mailbox
    machine logic/I/O messages and fast events
    CM/vision control planes, summaries, diagnoses, corrections, faults
    HMI, recipes, configuration, logs, telemetry, and update orchestration

BESIDE WS
    continuous raw vibration blocks
    triggered 18 MB vibration dumps
    3 MB image frames and defect-image archives
    native sequencing, retransmission, backpressure, and object storage
```

WS still earns a place because the excluded cyclic core is narrow but specialized, while 77 heterogeneous Participants still need one identity and Service vocabulary across shared memory, CAN-FD, scheduled mailboxes, daisy-chain Ethernet, and switched Ethernet.

### 3.2 Participants

| Class | Config A | Config B | Config C | Config D |
|---|---:|---:|---:|---:|
| MachineController domains | 4 | 4 | 4 | 4 |
| Drives | 48 | 48 | 48 | 48 |
| I/O islands | 6 | 6 | 6 | 6 |
| Line encoder | 1 | 1 | 1 | 1 |
| Safety controller | 1 | 1 | 1 | 1 |
| HMI panel | 1 | 1 | 1 | 1 |
| Vibration sensors | 0 | 12 | 12 | 12 |
| CM analyzer | 0 | 1 | 1 | 1 |
| Cameras | 0 | 0 | 2 | 2 |
| Vision controller | 0 | 0 | 1 | 1 |
| Remote gateway | 0 | 0 | 0 | 1 |
| **Total** | **61** | **74** | **77** | **78** |

The four controller domains are real dispatch/fault boundaries connected by `CtrlSHM`; they are not collapsed into one Participant. All counts fit ordinary 8-bit Participant identity.

### 3.3 Wires

| Wire (#) | Participants | Physical links | Purpose |
|---|---|---|---|
| `W_DriveService` (`W01`) | CommsCore, MotionCore, HmiCore, 48 drives, LineEncoder, SafetyController, HmiPanel, RemoteGateway when present | MotionBus **mailbox only**, CtrlSHM, MachineEth | Identity/Health/fault/parameters/logs/update while preserving drive PID |
| `W_MachineOps` (`W02`) | four controller domains, six I/O islands, HmiPanel | CtrlSHM, IOBus_A/B, MachineEth | I/O scan/events, machine state, recipes, and operator control |
| `W_CMService` (`W03`) | 12 vibration sensors, CmAnalyzer, HmiCore, HmiPanel, RemoteGateway | VibChain, MachineEth, CtrlSHM | CM Identity/Health/configuration, summaries, diagnosis, dump control |
| `W_VisionService` (`W04`) | Camera_A/B, VisionController, LogicCore, MotionCore, HmiCore, HmiPanel, RemoteGateway | MachineEth and CtrlSHM | Camera/vision Services, trigger/configuration, correction, defects, archive control |

Only `W_DriveService` touches MotionBus, and only through its fixed mailbox region. Raw images, vibration blocks, general machine telemetry, and unrelated broadcasts cannot enter that binding because they use different Wires or non-WS bulk channels.

Configuration growth is additive:

- Config A: `W01`, `W02`;
- Config B: add `W03` without changing motion or I/O Wiring;
- Config C: add `W04` without changing prior Wires;
- Config D: add RemoteGateway membership to the service Wires, with no production-path dependency.

No step requires rethinking the mapping.

### 3.4 Primary interactions

| Interaction | Src | Dest | Wire | Broadcast? | Notes |
|---|---|---|---|---|---|
| Drive Identity/Health/fault/parameter/log | drive or tool | tool or drive | `W_DriveService` | No | Consumes fixed mailbox bytes |
| Drive firmware/update control | HMI/RemoteGateway | selected drive | `W_DriveService` | No | Transfer only in maintenance |
| I/O scan/output | I/O island / LogicCore | LogicCore / island | `W_MachineOps` | No | CAN-FD cyclic schedule |
| Fast I/O event | I/O island | LogicCore, MotionCore | `W_MachineOps` | Directed copies or broadcast | ≤2 ms |
| Machine state/mode | LogicCore | MotionCore, HmiCore, drives | `W_MachineOps` plus composed drive update on `W01` | Optional | CommsCore/LogicCore authors only what they compose |
| RMS/envelope summary | vibration sensor | CmAnalyzer/HmiCore | `W_CMService` | No | 10 Hz |
| Diagnosis/trend | CmAnalyzer | HmiCore/RemoteGateway | `W_CMService` | No | Composed from raw stream; analyzer is source |
| Registration correction | VisionController | MotionCore | `W_VisionService` | No | ≤50 ms |
| Defect/reject | VisionController | LogicCore | `W_VisionService` | No | ≤20 ms |
| Bulk-stream control/status | sensor/camera/controller | analyzer/vision/HMI | corresponding service Wire | No | Payload itself is beside WS |

Cyclic axis setpoints, feedback, line reference, tick, and safety payload are not canonical interactions in this minimum mapping.

### 3.5 Forwarding and composition

| Router/domain | Wire | LinkBitmask / action | Count |
|---|---|---|---:|
| CommsCore | `W_DriveService` | MotionBus mailbox + CtrlSHM + MachineEth | 1 |
| CommsCore | `W_MachineOps` | CtrlSHM + MachineEth | 1 |
| CommsCore | `W_CMService` | CtrlSHM + MachineEth | 1 |
| CommsCore | `W_VisionService` | CtrlSHM + MachineEth | 1 |
| LogicCore | `W_MachineOps` | CtrlSHM + IOBus_A + IOBus_B | 1 |
| CmAnalyzer | `W_CMService` | VibChain + MachineEth | 1 |
| HmiCore / controller local domains | listed Wires | local delivery, not fake Links | 0 |

Total: **6 Wire→LinkBitmask objects**. Branches are not counted separately. There are no splices.

Composition points:

- `CmAnalyzer` turns raw vibration into diagnosis/trend PDUs using its own PID;
- `VisionController` turns images into correction/defect PDUs using its own PID;
- Logic/Comms domains may translate machine mode into a drive-facing Service command and then become the canonical source;
- cyclic process-image state summarized for HMI is authored by the summarizing controller domain.

Drive Service forwarding preserves drive identity across MotionBus mailbox and Ethernet. A 48-drive proxy is not required.

### 3.6 Link profiles

| Physical Link | Profile / boundary | WS Wires | Missing or required behavior |
|---|---|---|---|
| MotionBus cyclic region | native passing-frame protocol below WS | none | fixed offsets, schedule, distributed phase, atomic apply, missed-cycle handling |
| MotionBus mailbox region | **new profile class required**; master-initiated, fixed 64 B/cycle | `W_DriveService` | bounded mailbox framing/reassembly, static slot ownership, canonical reconstruction, no schedule overrun |
| MachineEth | WS direct Ethernet or WS/UDP candidate | all four service Wires | switched queue mapping, MTU, aggregation and admission remain profile/deployment work |
| IOBus_A/B | CAN-FD profile, not CAN11 | `W_MachineOps` | cyclic scan/event schedule and representable Endpoint rules are not frozen |
| VibChain service channel | Ethernet/mailbox profile | `W_CMService` | bounded service traffic beside native raw stream |
| CtrlSHM | shared-memory profile | `W01`, `W02`, `W04` as needed | bounded queues and domain serialization |
| Raw vibration/image channels | native bulk transport beside WS | none | block sequencing, credits/backpressure, object integrity, retry, storage |

`LINK` describes no summation/passing-frame or CAN-FD profile. This sketch records the required responsibilities without inventing their encoding.

Physical-Link inventory:

| Link type | Config A | Config B | Config C/D | Rate / cycle |
|---|---:|---:|---:|---|
| MotionBus scheduled Ethernet | 1 | 1 | 1 | 100 Mbit/s; 250–1000 µs |
| MachineEth switched Ethernet | 1 | 1 | 1 | 1 Gbit/s |
| CAN-FD I/O buses | 2 | 2 | 2 | 2 Mbit/s each |
| VibChain daisy-chain Ethernet | 0 | 1 | 1 | 100 Mbit/s |
| CtrlSHM | 1 | 1 | 1 | bounded local IPC |
| **Total Physical Links** | **5** | **6** | **6** | |

#### MotionBus field-by-field boundary

| Canonical field | Cyclic passing frame | Acyclic mailbox |
|---|---|---|
| Wire | Static process-image scope; therefore absent, but cyclic region is treated as non-WS | Elided by the one-Wire Link Binding (`W01`) |
| SrcParticipantId | Implied by fixed feedback offset | Reconstructed from selected node/mailbox slot or carried by bounded map |
| DestParticipantId | Implied by fixed command offset | Carried or reconstructed by configured request/response relation |
| Endpoint | Implied by process-image offset | Must be carried or mapped; Services cannot all be one Endpoint |
| QoS | Fixed schedule position, not priority | Fixed mailbox slot/admission; canonical QoS may be represented for egress queues |
| TransportType | Fixed native cyclic semantics | Must be carried/mapped for datagram vs reliable bounded transfer |
| Control/extensions | Native cycle/integrity metadata | Represented according to the eventual mailbox profile |

Calling each cyclic offset a canonical PDU would elide `Wire`, both ParticipantIds, Endpoint, QoS, TransportType, and most control—every field that makes it a PDU. WS would contribute only an after-the-fact label around the native process image. The honest boundary is that cyclic offsets are below WS. The mailbox is different because it reconstructs complete canonical Service PDUs.

### 3.7 Membership versus presence

Production devices remain configured members while temporarily unreachable. RemoteGateway is a Config D member, not a Config C member that happens to be absent. A failed drive remains a configured `W_DriveService` member; the native cycle separately marks its cyclic slot invalid.

---

## 4. Mandatory arithmetic

### 4.1 MotionBus per-cycle native ledger

At 100 Mbit/s:

```text
A24: 1000 us -> 12,500 wire bytes/cycle
A48:  500 us ->  6,250 wire bytes/cycle
A64:  250 us ->  3,125 wire bytes/cycle
```

The following is a transparent sizing assumption, not a claimed final profile:

| Item | A24 bytes/cycle | A48 bytes/cycle | A64 bytes/cycle |
|---|---:|---:|---:|
| Axis setpoints + feedback (`N × 14 B`) | 336 | 672 | 896 |
| Line reference | 6 | 6 | 6 |
| Cycle/tick/integrity metadata reserve | 32 | 32 | 32 |
| Safety slot reserve | 32 | 32 | 32 |
| Fixed acyclic mailbox | 64 | 64 | 64 |
| Passing-frame/carrier reserve | 64 | 64 | 64 |
| **Total** | **534** | **870** | **1,094** |
| **Serialization at 100 Mbit/s** | **42.72 µs** | **69.60 µs** | **87.52 µs** |
| **Capacity used** | **4.27%** | **13.92%** | **35.01%** |

The native aggregate fits all variants. The remaining cycle time covers propagation, in-flight node access, local control computation, phase guard, and implementation margin; exact qualification belongs to the selected native network.

### 4.2 Per-axis canonical PDU fit

Assume each 7-byte payload plus 6-byte canonical header is sent as one minimum Ethernet frame: 84 serialized bytes including preamble and inter-frame gap.

| Variant | Axis PDUs/cycle | PDU rate | Offered bytes/s | Offered bit rate | Fits? |
|---|---:|---:|---:|---:|---|
| A24, 1 kHz | 48 | 48,000/s | 4.032 MB/s | 32.256 Mbit/s | Raw serialization yes |
| A48, 2 kHz | 96 | 192,000/s | **16.128 MB/s** | **129.024 Mbit/s** | **No** |
| A64, 4 kHz | 128 | 512,000/s | **43.008 MB/s** | **344.064 Mbit/s** | **No** |

A48 offers `96 × 84 = 8,064 B` every 500 µs against `6,250 B`, before line reference, safety, tick, mailbox, or processing. Each drive would originate and consume at least 4,000 frames/s at A48; the master processes 192,000 cyclic frames/s, and passing hardware would forward the full stream.

At 2 kHz the raw minimum-frame ceiling is `floor(6,250 / 168) = 37 axes` before non-axis traffic. The qualitative break is therefore between A24 and A48. At 4 kHz the corresponding optimistic ceiling is 18 axes.

### 4.3 Canonical header overhead

| Variant | Headers/s | Header bytes/s | 7-byte payload bytes/s | Header / payload | Header / (header+payload) | Header / MotionBus capacity |
|---|---:|---:|---:|---:|---:|---:|
| A48 | 192,000 | **1.152 MB/s** | 1.344 MB/s | **85.7%** | **46.2%** | **9.22%** |
| A64 | 512,000 | **3.072 MB/s** | 3.584 MB/s | **85.7%** | **46.2%** | **24.58%** |

The statement that fixed canonical headers are not meaningful overhead is false for per-axis cyclic PDUs. Minimum Ethernet framing is even more binding.

A summation frame makes the overhead small only by replacing all per-axis canonical headers with static offsets. One six-byte aggregate header at 2 kHz would be 12 kB/s, only 0.096% of MotionBus capacity, but then axis source/destination/Endpoint live in the generated offset table rather than the canonical PDU stream. That is native aggregation, not 96 independently delivered PDUs.

### 4.4 Drive acyclic mailbox budget

Choose a fixed **64-byte mailbox region per A48 cycle**:

```text
raw mailbox capacity       = 64 B * 2,000 = 128,000 B/s
assumed usable Service data = 48 B * 2,000 =  96,000 B/s
serialization reservation   = 64 B * 8 / 100 Mbit/s = 5.12 us/cycle
share of MotionBus bytes     = 64 / 6,250 = 1.024%
```

Assumptions:

- Identity + Version + Health exchange: 160 usable bytes/drive including request/response metadata;
- firmware image: 4 MiB;
- cam table: 32 KiB;
- one campaign serializes drive transfers and ignores flash-programming pauses in the ideal number.

| Operation | Usable bytes | Ideal time at 96 kB/s | Operational conclusion |
|---|---:|---:|---|
| Query all 48 drives | 7,680 | 0.080 s | Practical during production at low cadence; allow ≥0.25 s for turns/retries |
| One 4 MiB drive image | 4,194,304 | 43.7 s | Maintenance only; roughly one minute with protocol/flash overhead |
| 48-drive campaign | 201,326,592 | 34.95 min | At least ~45–60 min operationally; stopped machine |
| One 32 KiB cam table | 32,768 | 0.341 s | Changeover/stopped motion; ~0.5 s practical |

The 64-byte slot is useful for identity, faults, parameters, and slow logs, but firmware is deliberately slow. Increasing it to 512 B/cycle gives 1.024 MB/s raw while consuming 40.96 µs and 8.19% of every A48 cycle. That permanently reduces timing margin even when no update is active. The mailbox is therefore fixed small for production; a maintenance configuration may install a larger validated schedule only while motion is stopped.

### 4.5 Continuous vibration stream

Exact raw rate:

```text
12 sensors * 3 channels * 25,600 samples/s * 2 B
    = 1,843,200 B/s
```

Hypothetical WS block mapping with 1,024-byte payloads:

```text
PDUs/s                    = 1,843,200 / 1,024 = 1,800
canonical header bytes/s  = 1,800 * 6 = 10,800
header / payload          = 0.586%
per-sensor PDU rate       = 150/s
```

With 38 Ethernet wire bytes per block, the stream uses about `1.8432 + 0.0108 + 0.0684 = 1.9224 MB/s`, roughly 15.4% of VibChain.

A Snapshot Endpoint is invalid: unread sample blocks would be intentionally overwritten. A bounded Queue with 64 blocks consumes 64 KiB and absorbs only `65,536 / 1,843,200 = 35.6 ms` of analyzer stall. On overflow, WS can reject/count delivery; it does not inherently sequence samples, identify gaps, or restore them. Link credits may backpressure a bounded producer, but the 10-second sensor rolling buffer and release policy remain native.

The selected mapping therefore keeps raw blocks beside WS. `W_CMService` controls acquisition and reports sequence/gap/Health summaries produced by the native stream implementation.

### 4.6 Triggered vibration dump

An 18 MB event is not one bounded PDU. The three-bit `TransportType` can name up to eight Transport kinds, but the governed documents do not define an arbitrary large-object/stream Transport that supplies block numbering, object integrity, retry, resume, and storage.

At a shaped 5 MB/s dump allocation, 18 MB takes approximately **3.6 s**, concurrent with the 1.84 MB/s live stream. A native bulk transfer divides the object into bounded blocks. Loss of one block is detected by its sequence/object manifest and retried or marks the object incomplete. Base unreliable WS datagrams would merely lose that block; LLL fragmentation guarantees only that a partial bounded PDU is not delivered, not recovery of an 18 MB object.

### 4.7 Camera stream and burst interference

At the archetype's decimal rate:

```text
2 cameras * 4 frames/s * 3 MB = 24 MB/s
```

Hypothetical 1,400-byte WS blocks over standard Ethernet MTU:

```text
PDUs/s                    = 24,000,000 / 1,400 = 17,143
canonical header bytes/s  = ~102,858
header / payload          = 0.429%
Ethernet overhead/s       = ~651,434 B
total wire bytes/s        = ~24.75 MB/s
```

Headers are affordable here; queueing and object semantics are the problem. A 64-block Queue holds 89.6 kB, only 3.7 ms at 24 MB/s. The camera or VisionController must own full-frame storage and native block flow control.

One 3 MB frame takes 24 ms to cross a 1-Gbit/s egress. Two simultaneous frames need 48 ms at the VisionController's single 1-Gbit/s ingress, leaving no credible time for analysis and a ≤50 ms correction. The native camera triggers must therefore be staggered or the analyzed region reduced; average bandwidth does not prove latency.

One defensible staggered correction ledger:

| Stage | Budget |
|---|---:|
| One image serialization to VisionController | 24.0 ms |
| Vision analysis | 20.0 ms |
| correction enqueue + switched network | 1.0 ms |
| CommsCore/CtrlSHM delivery to MotionCore | 1.0 ms |
| next A48 cycle boundary | 0.5 ms |
| **Total** | **46.5 ms** |
| **Margin** | **3.5 ms** |

Without separate queues, a simultaneous 18 MB dump plus one 3 MB archive ahead of the correction represents `21 MB / 125 MB/s = 168 ms` of serialization—already over the deadline. The underlay must use a bounded high-priority correction queue, strict-priority or scheduled switch service, and rate shapers for image/dump/update bulk. A 1,500-byte lower-priority frame causes at most about 12 µs non-preemptive blocking at 1 Gbit/s; WS QoS alone does not configure or prove this.

### 4.8 Fast I/O and safety timing

Conservative fast-I/O ledger:

| Stage | Budget |
|---|---:|
| Wait for reserved/high-priority CAN-FD opportunity | 0.50 ms |
| One conservative 80-byte-equivalent transfer at 2 Mbit/s | 0.32 ms |
| LogicCore validation/dispatch | 0.10 ms |
| CtrlSHM transfer to MotionCore | 0.10 ms |
| consumer scheduling / next control observation | 0.50 ms |
| **Total** | **1.52 ms** |
| **Margin to 2 ms** | **0.48 ms** |

This requires native CAN-FD arbitration/scheduling and static admission. It is not guaranteed by the Wire alone.

The safety interaction is stated as 125 Hz with an 8 ms watchdog. Its nominal period is exactly `1 / 125 = 8 ms`, so the stated watchdog margin is **0 ms** before clock tolerance, transmission, or processing. A safety schedule cannot be validated from those numbers. The selected native safety design must specify a faster refresh, a larger timeout, or another certified timing interpretation; this sketch does not choose one.

### 4.9 QoS expressiveness

The archetype header says eleven urgency classes, but §10.5 enumerates twelve. All twelve are accounted for:

| # | Interaction class | WS QoS / mechanism |
|---:|---|---|
| 1 | cyclic motion setpoint | Native schedule; QoS not the mechanism |
| 2 | cyclic motion feedback | Native schedule; QoS not the mechanism |
| 3 | cycle tick / sync | Native hardware phase; not a queued PDU |
| 4 | functional safety chain | Native reserved schedule/black channel; not ordinary QoS |
| 5 | fast I/O event ≤2 ms | QoS 0 Critical plus CAN-FD admission |
| 6 | ordinary cyclic I/O scan | QoS 1 High plus fixed scan schedule |
| 7 | registration correction ≤50 ms | QoS 0 Critical plus switched-queue bound |
| 8 | machine state / mode | QoS 1 High for event/state |
| 9 | continuous vibration stream | Beside WS; native stream queue, equivalent Normal class |
| 10 | image / bulk burst | Beside WS; shaped bulk queue, equivalent Background |
| 11 | diagnostics, logs, telemetry | QoS 2 Normal for interactive; QoS 3 for bulk logs |
| 12 | firmware transfer | QoS 3 Background; maintenance schedule |

Four values lose distinctions between 2-ms events and 50-ms corrections, between ordinary I/O and mode changes, and among several bulk types. This is acceptable only because deadlines and throughput are enforced by Link schedules, switch queues, and shapers. QoS is a coarse cross-Link hint, not the machine's urgency model.

### 4.10 Wire-scope trap

**Case 1 — one broad Wire spans MotionBus and MachineEth.**

The 24 MB/s image stream alone contributes `24 MB/s × 500 µs = 12,000 B/cycle` to a Link with only 6,250 B/cycle. Adding vibration yields 12,922 B/cycle before cyclic motion. Base flood-and-filter would make the cycle physically impossible. Destination pruning would save directed traffic but still would not supply schedule admission or protect a broadcast/bad binding.

**Case 2 — no cross-Link Wire and CommsCore proxies drives.**

This protects MotionBus but changes canonical source to CommsCore for every translated response. It creates a 48-device proxy and loses the strongest benefit of heterogeneous forwarding.

**Chosen case — one narrow cross-Link `W_DriveService`.**

Only drive/encoder/safety Service traffic uses this Wire. It maps to the fixed 64-byte MotionBus mailbox and preserves canonical source/destination through CommsCore. Bulk and machine Wires are distinct and cannot enter the binding. This retains canonical drive identity at the explicit cost quantified in §4.4.

Destination pruning remains useful on MachineEth, but it is not the safety argument. The structural guarantee is separate Wire membership plus a binding whose fixed mailbox capacity cannot exceed 64 B/cycle.

---

## 5. Optional optimizations

1. A maintenance-only MotionBus schedule may enlarge the mailbox after motion is stopped and the new schedule is validated. It is a configuration change, not dynamic bandwidth borrowing.
2. Destination-pruned egress on `W_DriveService` can avoid irrelevant Ethernet branches. It does not change the MotionBus slot.
3. CM diagnoses and vision events may use narrower Wires if measured switch congestion justifies them; raw streams remain outside WS.
4. Shared-memory direct local delivery can reduce controller-domain latency without collapsing the four Participants.

No optimization puts cyclic offsets or raw multi-megabyte objects back into ordinary WS datagrams.

---

## 6. Model pressure

### 6.1 Failure behavior

| Failure | Result and enforcing mechanism |
|---|---|
| One drive faults | Native image marks its slot invalid; MotionCore coordinates section stop. `W_DriveService` reports fault detail later. |
| Missed/overrun MotionBus cycle | Native cycle counter/watchdog distinguishes missing cycle from idle. A Snapshot Endpoint alone cannot: no cyclic WS PDU exists in the selected mapping. |
| I/O island silent | CAN-FD scan timeout identifies island; affected section stops while independent motion continues where valid. |
| Safety watchdog expiry | Native opaque safety path commands safe torque off; stated 125 Hz / 8 ms margin needs correction before qualification. |
| CmAnalyzer loss/backlog | VibChain/native stream queues and physical separation prevent any MotionBus effect; summaries become stale/unreachable. |
| Camera/Vision loss | Inspection degrades; machine continues under product policy. No cyclic dependency exists. |
| MachineEth bulk convergence | Sender shapers and switch queues cap image/dump/update rates; correction queue has strict bounded service. MotionBus and CAN-FD are separate Links. |
| HmiPanel loss | Controller domains and native cycles continue. |
| RemoteGateway absent | It is not a Config C member and no production path depends on it. |

### 6.2 Group atomicity and coordinated phase-in

R6 contributes no distributed group-atomic setpoint application. Canonical broadcast would distribute one payload but would not guarantee all drives received it, held the same cycle, or applied it on the same hardware tick. The native process-image offsets, cycle number, and distributed clock provide those semantics below WS.

Coordinated phase-in is naturally an application-level 49-Participant state machine:

- drive readiness/homing/enable state is queried through native cyclic state and `W_DriveService`;
- MotionCore advances explicit phases and aborts on fault;
- commands remain native cycle records;
- WS Services can expose state, configuration, and diagnostics but do not need a new Router concept.

A reusable motion-coordination Service might be valuable, but it would flatten to existing Endpoints and application state; no protocol feature is invented here.

### 6.3 Time synchronization

±1 µs phase is a hardware/Link-profile property below WS. A wall-clock Time Service may distribute UTC/monotonic correlation for logs, but ordinary Service PDUs and software timestamping cannot establish or prove sub-microsecond simultaneous drive phase.

These are two distinct concepts:

- **time-of-day / event correlation:** a WS Service can expose it across Link types;
- **cyclic phase / apply boundary:** native distributed clocks tied to one scheduled Link.

The standard Service story remains coherent only if it does not claim that one Time Service supplies both.

### 6.4 Friction signals — minimum mapping happy path

| Signal | None / Mild / Significant | Justification |
|---|---|---|
| Artificial Wire | None | Four Wires correspond to real service/control scopes and physical capacity boundaries. |
| Wire proliferation | Mild | Separate drive, machine, CM, and vision Wires are needed to prevent bulk from entering scheduled links. |
| Artificial hierarchy | None | Controller domains and analyzer/vision composition reflect actual ownership. |
| VCN pressure | None | No CAN11 is used. |
| WireAlias pressure | None | No CAN11 is used. |
| Configuration burden | Significant | WS Wiring, MotionBus schedule, CAN-FD scan, switch queues, and bulk shapers must agree. |
| Failure/topology mismatch | Mild | Cyclic slot and bulk-stream failures live below/beside Wires and require explicit telemetry. |

### 6.5 Scaling A24 → A48 → A64

The logical boundary and four service Wires do not change. The native aggregate grows linearly and occupies 4.3%, 13.9%, and 35.0% of per-cycle byte capacity under the stated ledger.

The per-PDU alternative breaks first:

- A24 at 1 kHz fits raw bandwidth but still lacks group apply and phase semantics;
- A48 exceeds raw link capacity at 37 axes before other traffic;
- A64 exceeds link capacity by 3.44× and reaches 512,000 PDUs/s.

Thus individual frame overhead binds before aggregate payload bandwidth. At A64, native traversal/processing and phase margin—not the 896-byte axis payload—become the next qualification concern.

### 6.6 If one concept could change

Two explicit boundaries would simplify this system more than a new universal primitive:

1. document a scheduled aggregate/passing-frame profile class as below-canonical or as a deliberately specialized projection, including when total static elision ceases to be meaningful WS; and
2. state that continuous streams/large objects require a separately specified Transport or run beside WS rather than pretending Queue/Snapshot storage is sufficient.

### 6.7 Useful distinctions

WS makes composition lineage explicit:

- drive Service forwarding can preserve the drive PID through the mailbox;
- CM diagnosis is authored by `CmAnalyzer`, not falsely attributed to sensors;
- registration correction is authored by `VisionController`, not cameras; and
- HMI motion summaries are authored by the controller domain that computes them.

It also forces the useful distinction between configured Wire scope and physical Link capacity: a narrow cross-Link drive Wire is safe where one broad “machine” Wire is not.

### 6.8 Specification pressure

- `CORE §2.2` permits elision, but total elision of every canonical field for cyclic offsets marks a practical boundary (`SF-R6-022`); the absent profile class is separately recorded in `SF-R6-023`.
- Link capabilities cannot express the fixed schedule/admission facts required to validate placement (`SF-R6-021`).
- Endpoint Queue/Snapshot storage is bounded and honest about overflow/replacement, but it does not define stream sequence, gap recovery, or multi-megabyte object transfer (`SF-R6-028`).
- wall-clock Time Service and hardware cyclic phase are different semantics (`SF-R6-024`).
- distributed group coherence is recorded for both motion archetypes as `SF-R6-025`, and cyclic-slot liveness needs the separate safeguard in `SF-R6-026`.
- shared switched-fabric queue isolation is not created by Wire scope or QoS (`SF-R6-027`).
- this sketch independently confirms `SF-R6-013`: broad flood-and-filter onto a scheduled constrained Link is not viable.

### 6.9 Result against falsification criteria

WS is **unsuitable for the cyclic core**:

- A48 individual PDUs require 129 Mbit/s on a 100-Mbit/s Link;
- A64 requires 344 Mbit/s;
- A48/A64 canonical headers alone consume 9.2%/24.6% of Link capacity;
- total static offset elision is required to recover native efficiency; and
- phase and group apply are entirely native schedule/clock properties.

WS is **unsuitable as the raw continuous-stream mechanism under current governed documents**:

- Snapshot replacement loses blocks;
- a 64-block vibration Queue absorbs only 35.6 ms;
- no selected Transport defines 18 MB/3 MB object segmentation, retry, resume, and integrity; and
- correction latency needs switch/shaper guarantees outside the Wire model.

WS remains **useful** for the service and control planes. The fixed drive mailbox preserves canonical identity, and the CM/vision/controller Services cover the integration surface without endangering motion.

---

## 7. Open questions

1. Is total static elision on a passing frame a legitimate Link profile or evidence that the cyclic region is below WS?
2. What exact mailbox profile reconstructs canonical Endpoint/Transport identity within 48 usable bytes/cycle?
3. Is a maintenance-only larger MotionBus schedule an accepted deployment transition, and how is compatibility/activation checked?
4. Which concrete bulk Transport supplies block sequence, object integrity, retry/resume, and backpressure for vibration/images?
5. Can the product guarantee staggered camera delivery or otherwise preserve the 50-ms correction budget?
6. Is the stated 125-Hz safety exchange compatible with an 8-ms watchdog, given its zero nominal timing margin?
7. What measured native A64 traversal and distributed-clock error remain after 64 in-flight node accesses?
8. Should documentation explicitly separate wall-clock Time Service from Link-level phase synchronization?
9. Does CAN-FD profile work provide a generated cyclic scan/event schedule, or is that also a native mechanism beside WS?

---

## 8. Spec findings

| ID | Finding | Consequence |
|---|---|---|
| `SF-R6-022` | Summation-frame efficiency requires total per-axis canonical-field elision. | The cyclic region is better treated below WS; only the bounded mailbox reconstructs ordinary canonical PDUs. |
| `SF-R6-028` | Endpoint storage and the current Transport placeholder do not define continuous-stream or large-object semantics. | Raw vibration/images need a separately specified bulk Transport or an explicit beside-WS boundary. |
| `SF-R6-024` | Wall-clock Time Service and hardware cyclic phase are distinct concepts. | ±1 µs apply synchronization must remain a Link/hardware property and must not be claimed by ordinary Time Service portability. |

---

## 9. Diagram

```text
                             WS SERVICE PLANE

 Drives/Encoder/Safety ==[64 B/cycle MotionBus mailbox]== CommsCore
          |                                                    |
          +---------------- W_DriveService ---------------------+---- MachineEth

 IoIslands == CAN-FD == LogicCore == CtrlSHM == MotionCore/HmiCore
          +---------------- W_MachineOps -----------------------+

 VibSensors == W_CMService == CmAnalyzer ===== MachineEth
     \_____ raw blocks / dumps beside WS _____/

 Cameras == W_VisionService == VisionController == controllers/HMI
    \______ image objects beside WS __________/

 BELOW WS ON MOTIONBUS:
     passing-frame cyclic image + fixed offsets + cycle tick
     distributed phase + group apply + opaque safety framing
```

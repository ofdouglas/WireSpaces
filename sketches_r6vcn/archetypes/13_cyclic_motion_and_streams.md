# Archetype 13 — Hard-Real-Time Cyclic Motion + High-Rate Streaming

**ID:** 13
**Convergence test:** **Yes** — assign 2–3 independent mapping agents
**Suggested Link mix:** Time-scheduled industrial Ethernet (summation / passing-frame class) + switched Gigabit Ethernet + CAN-FD distributed I/O + 100 Mbit daisy chain + shared memory / IPC
**Stress:** Sub-millisecond cyclic determinism, group-atomic setpoint application, ±1 µs phase synchronization, continuous multi-megabyte-per-second sensor streams, multi-megabyte image bursts, eleven distinct urgency classes, and a device population reachable **only** through a fully scheduled link
**Class:** **Adversarial archetype** — see §0

**Scale target:** approximately **77 production WS Participants** (78 with remote support). Participant count is deliberately *unremarkable*; the stress is entirely temporal and payload-shaped.

**Primary question:** Is WireSpaces the right layer for hard-real-time cyclic control and continuous high-rate streams — and if it is not, **exactly where is the boundary**, and does WS still earn its place on the other side of it?

---

## 0. Adversarial intent — read this first

Archetypes 01–11 were all *rate-comfortable*. Where bandwidth was tight (CAN11 relation compression, the eVTOL ground radio) the pressure was on **addressing** or **selection**, never on **time**. Every one of them was permitted to write some version of "bandwidth is ample" or "the exact algorithm is out of scope," and several did — including for time synchronization, which this archetype makes a hard product requirement.

The campaign has therefore never tested:

- whether a Wire can carry sub-millisecond cyclic traffic with bounded jitter;
- whether the fixed canonical header is affordable on small, very frequent payloads;
- whether a 2-bit QoS field can express a real machine's urgency classes;
- whether Endpoint storage is a usable model for a continuous stream;
- whether WS has anything to say about group-atomic application of a setpoint vector;
- what happens when a whole device class is reachable *only* over a link with no spare bandwidth.

**This archetype is written to fail.** A mapping that concludes "WireSpaces should not own the cyclic motion core; here is the honest boundary and here is what WS still contributes" is a **successful** trial outcome — arguably the most valuable one available. A mapping that concludes WS handles all of this naturally will be treated as evidence of agent bias unless every arithmetic requirement in §10 is discharged.

Do not invent protocol features to rescue the archetype. Report the gap.

---

## 1. Falsification requirement (mandatory deliverable)

**Before** producing the mapping, write a section titled **"Falsification criteria"** stating what observable outcomes in *this* archetype would show that WireSpaces is the wrong layer for cyclic control, for streaming, or for both.

Suggested form:

```text
I will judge WS UNSUITABLE for the cyclic core if:
    - <criterion, with a number>

I will judge WS SUITABLE for the cyclic core if:
    - <criterion, with a number>

Same for continuous streams.
```

Then map, then report against the agent's own criteria.

Additionally, this archetype requires a **scope-boundary deliverable**: a single explicit diagram or list stating what WireSpaces owns, what lives below it, and what lives beside it. "Everything is WS" and "nothing is WS" are both acceptable answers if argued.

---

## 2. Prohibited escape hatches

| Prohibited | Why |
|---|---|
| "Bandwidth is ample" | This archetype has a 500 µs cycle and a 12.5 MB/s link. Compute. |
| "Fixed canonical headers are not a meaningful bandwidth concern" | This is quoted from the docs and is exactly what is under test. Prove or refute it with arithmetic. |
| "Time sync algorithm is out of scope" | ±1 µs phase error is a product requirement here. State where sync lives and why. |
| "QoS handles it" | Enumerate the urgency classes first, then show them mapping onto four values. |
| "The application handles buffering / dedup / sequencing" | Permitted only after stating what the model provides and what it does not. |
| "Use a bigger link" | Link speeds are given. Do not upgrade the machine to make the mapping work. |
| Adding a physical Link that a drive does not have | Drives have one Link. That is the trap; do not disarm it. |
| Claiming a summation-frame Link profile exists | `LINK` has no such profile. Say what one would have to do. |
| Concluding "unsuitable" without arithmetic | A negative verdict also needs numbers. |

---

## 3. Native system

A high-speed converting and packaging line: continuous web material is unwound, printed, laminated, cut, and packed. Dozens of servo axes run on an **electronic line shaft** — a virtual master position that every axis follows in fixed phase relationship. Phase error becomes registration error becomes scrap.

Bolted onto this machine, as is now standard, are two subsystems that were not part of the original control architecture: **continuous vibration-based condition monitoring** and **camera print inspection**. Both produce data at rates the control network was never designed for, and both matter commercially.

### Physical topology

```text
                          MachineController (4 domains, CtrlSHM)
                    MotionCore   LogicCore   CommsCore   HmiCore
                        |            |           |   |        |
                        +-- CtrlSHM -+-----------+   |        |
                                     |               |        |
                        +------------+               |        |
                        |                            |        |
                   IOBus_A / IOBus_B          MotionBus       |
                   (CAN-FD 2 Mbit/s)     (100 Mbit/s scheduled)|
                        |                            |        |
              IoIsland_1..3 / _4..6      +-----------+------+  |
              240 DI, 48 AI              |     |            |  |
                                    Drive_01 .. Drive_48    |  |
                                    LineEncoder             |  |
                                    SafetyController        |  |
                                                            |  |
                                     MachineEth (Gigabit switched)
                                        |      |      |      |     |
                                  VisionCtrl  CmAnalyzer  HmiPanel  RemoteGateway
                                     |   |        |                  (Config D)
                              Camera_A Camera_B   |
                                                VibChain (100 Mbit daisy chain)
                                                  |
                                          VibSensor_01 .. _12
```

### Axis-count / rate variants

The logical architecture must not change across variants.

```text
Variant A24:   24 axes,  1 kHz cycle  (1000 us)
Variant A48:   48 axes,  2 kHz cycle  ( 500 us)     <-- map this first
Variant A64:   64 axes,  4 kHz cycle  ( 250 us)
```

### Configurations

**Config A — motion machine only.** Drives, controller, distributed I/O, safety chain, HMI panel. No condition monitoring, no vision.

**Config B — A + condition monitoring.** Adds 12 vibration sensors and the analyzer.

**Config C — B + print inspection.** Adds two cameras and the vision controller. **This is the normally delivered machine; map Config C, variant A48, first.**

**Config D — C + remote support gateway.** Occasional remote engineering access, including firmware transfer.

Report what each configuration step added and whether any step required rethinking the mapping rather than extending it.

---

## 4. Device capabilities

| Participant class | Count | Responsibilities |
|---|---|---|
| `Drive_01` … `Drive_48` | 48 | Servo control (current/velocity/position inner loops **local**), cyclic setpoint consumption, cyclic feedback publication, drive fault/status, temperature/current diagnostics, Identity/Version/Health, field-updatable firmware, parameter set |
| `MotionCore` | 1 | Line-shaft generation, cam/gear profiles per axis, setpoint vector computation, phase supervision, motion state machine |
| `LogicCore` | 1 | Machine sequence logic, distributed I/O scan, interlocks, recipe execution |
| `CommsCore` | 1 | Owns `MotionBus` and `MachineEth`; forwarding; Link status |
| `HmiCore` | 1 | Operator model, alarms, production data aggregation |
| `IoIsland_1` … `_6` | 6 | 40 digital points + 8 analog points each; 2 ms scan; event reporting |
| `LineEncoder` | 1 | Physical master encoder / external line reference |
| `SafetyController` | 1 | Dual-channel functional-safety chain, 8 ms watchdog, safe-torque-off coordination |
| `VibSensor_01` … `_12` | 12 | Triaxial accelerometer, **25.6 kHz per channel, 16-bit, continuous**; local envelope/RMS computation; 10 s rolling raw buffer released on trigger |
| `CmAnalyzer` | 1 | Continuous stream ingest, spectral analysis, bearing/gear diagnosis, trend storage |
| `Camera_A` / `Camera_B` | 2 | 2048×1536 mono, **4 frames/s each, ~3 MB per frame**, hardware-triggered from line position |
| `VisionController` | 1 | Print registration/defect analysis, defect image archive, reject signalling |
| `HmiPanel` | 1 | Operator terminal |
| `RemoteGateway` | 1 | Config D only; remote diagnostics and firmware transfer |

**Production total: 77** (78 in Config D).

### The structural trap — do not disarm it

> **Each of the 48 drives has exactly one Link: `MotionBus`.**

There is no service Ethernet port on a drive, no second bus, and no maintenance connector in scope. Every drive Service — Identity, Version, Health, fault detail, parameter access, logs, firmware update — must reach the drive over a link whose bandwidth is allocated to a fixed cycle.

`SafetyController` and `LineEncoder` are likewise `MotionBus`-only.

---

## 5. Existing physical links

| Link | Type | Rate | Attachments |
|---|---|---|---|
| `MotionBus` | **Time-scheduled industrial Ethernet, summation / passing-frame class** | 100 Mbit/s, fixed cycle 250–1000 µs | `CommsCore`, 48 drives, `LineEncoder`, `SafetyController` |
| `MachineEth` | Switched Ethernet | 1 Gbit/s | `CommsCore`, `HmiCore`, `VisionController`, `Camera_A/B`, `CmAnalyzer`, `HmiPanel`, `RemoteGateway` |
| `IOBus_A` | CAN-FD | 2 Mbit/s | `LogicCore`, `IoIsland_1..3` |
| `IOBus_B` | CAN-FD | 2 Mbit/s | `LogicCore`, `IoIsland_4..6` |
| `VibChain` | Ethernet daisy chain | 100 Mbit/s | `CmAnalyzer`, `VibSensor_01..12` |
| `CtrlSHM` | Shared memory / IPC | ample | `MotionCore`, `LogicCore`, `CommsCore`, `HmiCore` |

### What "summation / passing-frame class" means for this trial

`MotionBus` is not a packet-switched network with per-node datagrams. Model it as the class of link used by real cyclic motion buses:

```text
- The master emits one (or a few) frames per cycle.
- The frame traverses every node in sequence.
- Each node reads its own slice and writes its own slice IN FLIGHT,
  at a statically configured bit offset.
- There is no per-node frame, no per-node addressing field in the
  ordinary cyclic path, and no arbitration.
- A small statically sized acyclic/mailbox region may be carried in
  the same cycle for non-cyclic traffic.
- Cycle time, slot layout, and offsets are fixed at configuration time.
```

`LINK` currently has **no profile for this link class**. `CORE §2.2` does permit a Link profile to encode canonical fields in native metadata rather than transmitting the six-byte sequence literally, and to elide fields that are statically implied.

The agent must determine how far that elision principle stretches here, and say what is left. If a Link profile must statically elide `Wire`, `SrcParticipantId`, `DestParticipantId`, `Endpoint`, `QoS`, and `TransportType` — i.e. all of it — then state plainly what WireSpaces is contributing on that link, and whether that is a legitimate use of the elision principle or an admission that the link is below the WS boundary.

Do not assume the answer. Argue it.

---

## 6. Required interactions

### Cyclic motion — the core

| Interaction | From | To | Rate | Payload | Notes |
|---|---|---|---|---|---|
| Axis setpoint | `MotionCore` | each drive | **2 kHz** (A48) | ~7 B: 4 B position, 2 B torque feedforward, 1 B control word | **All 48 must be applied on the same tick** |
| Axis feedback | each drive | `MotionCore` | **2 kHz** | ~7 B: 4 B actual position, 2 B current, 1 B status | Sampled coherently |
| Line reference | `LineEncoder` | `MotionCore` | 2 kHz | ~6 B | Master position |
| Cycle tick / sync | reference source | all `MotionBus` nodes | 2 kHz | — | **±1 µs phase across all axes** |
| Safety chain | `SafetyController` ↔ drives | | 125 Hz | ~16 B opaque | Dual-channel protocol, 8 ms watchdog, WS carries it opaquely |

### Machine logic and I/O

| Interaction | From | To | Rate |
|---|---|---|---|
| Digital input scan | each `IoIsland` | `LogicCore` | 2 ms cyclic |
| Fast input event (registration mark, reject sensor) | `IoIsland` | `LogicCore`, `MotionCore` | event, **≤2 ms latency** |
| Output command | `LogicCore` | each `IoIsland` | 2 ms cyclic |
| Machine state / mode | `LogicCore` | `MotionCore`, `HmiCore`, drives | 10–100 Hz + event |

### Condition monitoring — continuous stream

| Interaction | From | To | Rate | Volume |
|---|---|---|---|---|
| Raw vibration stream | each `VibSensor` | `CmAnalyzer` | **3 channels × 25.6 kHz × 16-bit, continuous** | ~154 kB/s per sensor, **~1.84 MB/s total** |
| Envelope / RMS summary | each `VibSensor` | `CmAnalyzer`, `HmiCore` | 10 Hz | small |
| Triggered raw buffer release | each `VibSensor` | `CmAnalyzer` | on trigger | **10 s rolling buffer ≈ 1.5 MB per sensor, up to 18 MB per event** |
| Diagnosis / trend | `CmAnalyzer` | `HmiCore`, `RemoteGateway` | 0.1–1 Hz | small |

### Print inspection — burst

| Interaction | From | To | Rate | Volume |
|---|---|---|---|---|
| Image frame | each camera | `VisionController` | 4 frames/s each | **~3 MB per frame, ~24 MB/s aggregate** |
| Registration correction | `VisionController` | `MotionCore` | 4–20 Hz, **≤50 ms latency** | small |
| Defect / reject signal | `VisionController` | `LogicCore` | event, **≤20 ms** | small |
| Defect image archive | `VisionController` | `HmiCore`, `RemoteGateway` | on defect | 3 MB per record |

### Standard Services — and where they must go

| Interaction | Notes |
|---|---|
| Identity / Version | Every production Participant, **including all 48 drives** |
| Health | Every Participant |
| Fault / diagnostic detail | Every Participant; drive fault detail can be hundreds of bytes |
| Parameter / configuration access | Drives (tuning, limits, cam tables — cam tables can be tens of kB) |
| Firmware update | Drives (~4 MB image), I/O islands, sensors, cameras |
| Logs / event history | Controller domains, drives (small), vision, CM |
| Link status | `CommsCore`, `CmAnalyzer`, `VisionController` |
| Time | See Question D — this is not a routine Service here |
| Telemetry | Selected engineering data, heavy during commissioning |

**Explicitly not required:** the CM analyzer subscribing to cyclic motion data; every drive receiving every other drive's feedback; images crossing `MotionBus`; the HMI participating in the cyclic loop; inner servo loops running over WS.

---

## 7. Group atomicity and startup coordination

Two requirements the corpus has never encountered.

**Group-atomic setpoint application.** All 48 axis setpoints are computed from one line-shaft position and must take effect on the same control tick. If axis 17 applies a setpoint one cycle late, the web tears or the print misregisters. Report what, if anything, the canonical model offers: broadcast plus locally aligned tick? A control-metadata field? Nothing at all, leaving it entirely to the Link profile's cyclic slot semantics? If nothing, say whether that is correct layering or a gap.

**Coordinated phase-in.** On start, 48 axes must enable, home where required, and ramp into phase lock in a partly ordered sequence, with abort at any point. This is a distributed group state machine spanning 49 Participants. Report whether WS Services express this naturally, whether it belongs in the application, and whether the standard Service set is missing an obvious member.

Do not invent a new protocol mechanism for either. Report what is absent.

---

## 8. Failure and degraded assumptions

- One drive faults: coordinated stop of the affected section; other axes must not lose phase lock spuriously.
- `MotionBus` cycle overrun or missed cycle: drives must detect and enter a safe state within a bounded number of cycles. State how a missed *cyclic* PDU is distinguishable from an idle one under Endpoint storage semantics.
- One `IoIsland` silent: affected machine section stops; motion continues where safe.
- `SafetyController` watchdog expiry: safe torque off. This is a functional-safety data path, **not** a security or authorization question.
- `CmAnalyzer` lost or its stream backing up: **must have zero effect on motion.** State what enforces that.
- Camera or `VisionController` lost: inspection degrades, machine continues at reduced quality assurance.
- `MachineEth` saturated by an image burst, a triggered 18 MB vibration dump, and a firmware transfer at once: state what protects `MotionBus`, `IOBus`, and the registration-correction latency budget.
- `HmiPanel` lost: no production impact.
- `RemoteGateway` absent: no production impact.

---

## 9. Bandwidth and timing constraints

```text
MotionBus:    100 Mbit/s = 12.5 MB/s;  cycle 500 us (A48)
              per-cycle budget must include cyclic motion, safety,
              and any acyclic/mailbox traffic
MachineEth:   1 Gbit/s = 125 MB/s switched
IOBus_A/B:    2 Mbit/s CAN-FD each, 2 ms scan
VibChain:     100 Mbit/s = 12.5 MB/s
CtrlSHM:      ample; latency low and bounded

Phase requirement:      +/- 1 us across all axes
Registration latency:   camera -> MotionCore correction <= 50 ms
Fast I/O latency:       sensor -> LogicCore/MotionCore <= 2 ms
Safety watchdog:        8 ms
```

---

## 10. Mandatory arithmetic

Each of these must appear in the mapping with numbers. This section is the core deliverable.

### 10.1 Cyclic fit, per-PDU

```text
A48: 48 setpoints + 48 feedbacks = 96 PDUs per 500 us cycle
     = 192,000 PDUs/s

If each canonical PDU became an individual Ethernet frame on MotionBus,
what is the offered load, and does it fit in 12.5 MB/s?
What frame rate would the drive MACs have to sustain?
```

### 10.2 Canonical header overhead

```text
Payload per cyclic PDU: ~7 bytes
Canonical header candidate: 6 bytes (CORE 2.1)

Compute, for A48 and A64:
    header bytes/s
    header as a percentage of cyclic motion traffic
    header as a percentage of MotionBus capacity

Then state whether "fixed full canonical headers are not a meaningful
bandwidth concern" holds on this link, and how much of the header a
summation-frame Link profile would have to elide to make it hold.
```

### 10.3 Acyclic budget for drive Services

```text
48 drives are reachable ONLY over MotionBus.
Assume the Link profile can carry a statically sized acyclic region
per cycle. Choose a defensible size and state it.

Compute:
    acyclic bytes/s available at 2 kHz
    time to read Identity + Version + Health from all 48 drives
    time to transfer one 4 MB firmware image to one drive
    time for a 48-drive firmware campaign
    time to download a 32 kB cam table to one drive
    whether any of this is possible during production, or only in a
    maintenance window

Then state what happens to the cyclic budget if the acyclic region is
made large enough to be useful.
```

### 10.4 Stream fit

```text
Continuous vibration: 1.84 MB/s aggregate on VibChain (12.5 MB/s)
    -> fits, but at what PDU rate?
    -> choose a block size; state PDUs/s and per-PDU header overhead
    -> what does Endpoint storage do with a continuous stream?
       If storage is single-slot overwrite, what happens to samples
       between consumer reads?
    -> is there sequencing? gap detection? backpressure? Report what
       the model provides and what it does not.

Triggered dump: up to 18 MB burst
    -> fragmentation and reassembly: does the model have a concept?
       Check TransportType (3 bits / up to 8 values) and report.
    -> what happens on loss of one fragment?

Images: ~3 MB per frame, ~24 MB/s aggregate on MachineEth
    -> same questions
    -> plus: what keeps a 24 MB/s stream and an 18 MB burst from
       delaying the <=50 ms registration correction on the same link?
```

### 10.5 QoS expressiveness

```text
Enumerate every urgency class this machine actually has. A starting
list, which the agent should correct:

    1  cyclic motion setpoint
    2  cyclic motion feedback
    3  cycle tick / sync
    4  functional safety chain
    5  fast I/O event (<=2 ms)
    6  ordinary cyclic I/O scan
    7  registration correction (<=50 ms)
    8  machine state / mode
    9  continuous vibration stream
    10 image / bulk burst
    11 diagnostics, logs, telemetry
    12 firmware transfer

Now map them onto 4 QoS values (CORE 2.1).

State which distinctions are lost, whether the loss matters, and
whether QoS is even the right mechanism for classes 1-3 (which are
schedule properties, not priorities).
```

### 10.6 The Wire-scope trap

```text
CommsCore is on both MotionBus and MachineEth.

Case 1: a Wire spans MotionBus + MachineEth.
    Under base flood-and-filter (CORE 12), what reaches MotionBus?
    Compute the consequence for the 500 us cycle.

Case 2: no Wire spans both.
    Then how do CmAnalyzer, HmiCore, VisionController, RemoteGateway,
    and engineering tooling reach a drive's Identity / Health / Fault /
    parameter / update Services?

    - via composition at CommsCore?  -> then canonical source is lost
      and CommsCore becomes a proxy for 48 devices. Note that
      "canonical source preserved across heterogeneous links" has been
      the corpus's strongest repeated result. Say whether this
      archetype breaks it.
    - via a splice?                  -> canonical identity survives,
      but the traffic still lands in the cyclic acyclic budget (10.3).
    - via destination-pruned egress? -> state whether pruning is
      sufficient, and note that SF-R6-013 already flags pruning's
      "optional" framing as a problem.

Choose, justify, and state the cost. This is the sharpest structural
question in the archetype.
```

---

## 11. Expected diagnostic outcomes

### Question A — cyclic determinism

Can a Wire carry 2 kHz cyclic exchange with bounded jitter, or is the canonical model simply silent on schedule? Note that WS has Wires, Endpoints, and QoS, but no reservation, slot, budget, or deadline concept. Is that correct layering (the Link profile owns the schedule) or a missing piece?

### Question B — canonical header on summation-frame links

Discharge §10.1 and §10.2. Then answer: can `LINK` support this link class at all, and if the answer is "only by eliding every canonical field," is that the elision principle working as designed or the boundary of WS?

### Question C — group atomicity

Discharge §7. Does WS offer anything for applying 48 setpoints on a common tick?

### Question D — time synchronization at ±1 µs

The corpus has repeatedly deferred time sync. Here it is a hard requirement. Answer specifically:

```text
Is the WS time Service a candidate at +/- 1 us?
Or must synchronization be a hardware / Link-profile property below WS?
If below WS, what happens to the claim that standard Services span all
    Link types coherently?
Is "time" actually two different things -- a wall-clock Service and a
    Link-level phase reference -- that the model currently conflates?
```

This may be the most productive question in the archetype.

### Question E — QoS expressiveness

Discharge §10.5.

### Question F — streams versus Endpoint storage

Discharge §10.4. Is Endpoint storage the wrong abstraction for continuous data, and if so, does WS need a stream/bulk concept or should it declare streams out of scope and let them run beside WS?

### Question G — devices behind a scheduled link

Discharge §10.6 and §10.3. Forty-eight devices expose the full standard Service set and are reachable only through a bus with no spare bandwidth. Does the Service ecosystem story survive contact with this?

### Question H — interference and isolation

Does the model provide any way to say "this link's capacity is reserved and shall not be consumed by that Wire"? If the answer is only "draw narrower Wires and use QoS," is that adequate at 500 µs?

### Question I — the scope boundary

Produce the mandatory boundary deliverable. A plausible honest answer is:

```text
below WS:     cyclic motion exchange, cycle tick, phase sync,
              inner servo loops, safety chain framing
WS owns:      drive Services, machine logic and I/O, condition
              monitoring control plane, vision control plane, HMI,
              recipes, diagnostics, updates, telemetry
beside WS:    raw vibration streams and image payloads, carried by a
              bulk transport with WS handling only their control plane
```

If the agent proposes that split, it must then answer the follow-up: **with the cyclic core outside WS, is WireSpaces still worth adopting on this machine?** Archetype 08 asked the same question about a flat Ethernet rack and answered "yes, on Service-ecosystem grounds." Does that answer survive when the excluded part is the machine's reason for existing?

### Question J — scaling A24 → A48 → A64

Does the mapping survive 24 → 64 axes and 1 kHz → 4 kHz without qualitative change? Identify precisely which constraint binds first, and at what axis count and cycle time the architecture stops working.

---

## 12. Valid experiment outcomes

```text
"WS is the wrong layer for the cyclic core. The honest architecture runs
 a scheduled motion protocol below WS and lets WS own everything else.
 This is a scope statement, and it should be written into the docs
 rather than discovered per project."

"The canonical header is affordable when aggregated into a cyclic frame
 and unaffordable as individual PDUs. LINK needs an aggregated /
 summation-frame profile class, which it does not have."

"Total static elision on a summation-frame link is legal under CORE 2.2
 but leaves WS contributing nothing on that link. The boundary is the
 drive's comms core, not the drive."

"Endpoint storage is the wrong model for continuous streams. WS needs a
 stream concept or an explicit statement that bulk data runs beside WS
 with only its control plane in WS."

"2-bit QoS cannot express this machine. Worse, cyclic determinism is a
 schedule property that QoS is the wrong mechanism for entirely."

"Time synchronization is two different concepts. The model conflates a
 wall-clock Service with a Link-level phase reference, and only the
 former is a Service."

"Group-atomic setpoint application has no representation in WS and
 belongs to the Link profile. That is defensible, but it means the
 motion Wire is a fiction layered over a schedule WS does not control."

"Devices reachable only through a scheduled link break the Service
 ecosystem story: either the gateway becomes a proxy (losing canonical
 source, the corpus's best result) or Service traffic competes with the
 control loop."

"Flood-and-filter is actively dangerous on a cyclic link, making
 destination-pruned egress mandatory rather than optional -- a second,
 independent confirmation of SF-R6-013."

"WS fits the 80% of this machine that is not the cyclic core, and that
 80% is where the integration pain actually is. The boundary is clean
 and the product story survives."
```

Outcomes that would be **suspicious**:

```text
"WS handles the cyclic core naturally."
"Header overhead is not a concern."
"QoS is sufficient."
"Streams fit the Endpoint model."
```

---

## 13. What this archetype does not include

Out of scope for WireSpaces here:

- servo control law design, current/velocity/position loop tuning;
- cam profile mathematics and electronic line-shaft algorithms;
- web tension and registration control theory;
- functional-safety certification and safety-case argumentation (the safety chain is an opaque data path only);
- security, authentication, and authorization of any kind;
- image processing and defect classification algorithms;
- vibration/spectral analysis mathematics;
- the internal design of the scheduled motion protocol;
- Ethernet switch QoS/TSN configuration detail, beyond stating what is assumed;
- mechanical machine design;
- recipe/MES/plant IT integration.

Also excluded unless separately assigned:

- inventing a WS scheduling, reservation, or deadline mechanism — **describe what is missing instead**;
- inventing a WS stream or fragmentation mechanism — check what exists, then describe the gap;
- dynamic routing or discovery;
- CAN11/VCN analysis (there is no Classical CAN11 here);
- extended canonical addressing (77 Participants do not need it);
- modelling lockstep or replica cores as separate Participants.

---

## 14. Configuration / accounting requirements

```text
Production Participants:
    count by class, per configuration

Physical Links:
    count by type, with rate and (where applicable) cycle time

Canonical Wires:
    count, purpose, member Links, participant scope
    which Wires touch MotionBus, and why that is safe

Link profiles:
    one line per profile class
    explicit statement of what the MotionBus profile must do that
    LINK does not currently describe
    field-by-field statement of what is elided on MotionBus

Gateway / forwarding state:
    Wire -> LinkBitmask objects (one per gateway per Wire; do NOT
    inflate by counting branches separately)
    splices, composition points, and what each costs in identity terms

Timing ledger (required):
    per-cycle MotionBus byte budget, itemized
    acyclic bytes/s and what they buy
    registration-correction latency chain
    fast-I/O latency chain
    safety watchdog margin

Stream ledger (required):
    bytes/s and PDUs/s per stream
    per-PDU header overhead as a percentage
    what absorbs a stalled consumer

QoS assignment:
    every interaction class -> one of 4 values, with losses named

Scope boundary (required):
    below WS / WS / beside WS

Scaling:
    A24 -> A48 -> A64, and the first binding constraint
```

---

## 15. Comparison to archetypes 11 and 12

| | **11 — Redundant eVTOL** | **12 — Modular BESS** | **13 — Cyclic Motion + Streams** |
|---|---|---|---|
| Production Participants | 26 / 30 / 38 | 43 / 187 / 565 | 77 |
| Dominant stress | Redundancy and physical loops | Identity, commissioning, lifecycle | **Time and payload shape** |
| Fastest hard requirement | 1 kHz local, 200 Hz networked | 100 ms safety trip | **500 µs cycle, ±1 µs phase** |
| Smallest payload at highest rate | ~8 B at 200 Hz | ~8 B at 10 Hz | **7 B at 2 kHz × 96** |
| Largest single transfer | 4 MB update, off-line | 256 kB × 160, off-line | **18 MB burst during production** |
| Continuous stream | None | None | **1.84 MB/s + 24 MB/s** |
| Bandwidth pressure | Radio (selection) | Rack CAN (repetition) | **Cyclic link (schedule)** |
| Devices with one Link only | Sensors (benign) | Module monitors (CAN) | **48 drives on a scheduled bus** |
| CAN11 relevance | None | 20 identical buses | None |
| Header overhead tested | No | Partly | **Yes, directly** |
| QoS tested | Lightly | Lightly | **Directly, 12 classes into 4** |
| Time sync rigor | "Out of scope" | ~10 ms | **±1 µs, mandatory** |
| Written to | Explore | Break the identity model | **Break the timing and payload model** |

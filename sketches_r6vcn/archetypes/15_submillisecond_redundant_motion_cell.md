# Archetype 15 — Sub-Millisecond Redundant Motion-Control Cell

**ID:** 15  
**Convergence test:** **Yes** — assign 3 independent mapping agents  
**Suggested Link mix:** 1 Gbit/s deterministic industrial Ethernet / TSN- or EtherCAT-class cyclic network + redundant ring path + ordinary service Ethernet + local shared memory  
**Stress:** 125–250 µs synchronized cycles, coherent process images, distributed clocks, deterministic schedules, redundant ring recovery, large cyclic fan-out/fan-in, and maintenance traffic sharing hardware  
**Class:** **Adversarial real-time suitability test**

**Primary question:** Is WireSpaces' independent bounded-datagram model a useful representation for a hard real-time synchronized motion plane whose natural primitive is one scheduled, coherent process image exchanged by dozens of axes every cycle?

---

## 0. Adversarial intent

Most existing sketches use deadlines in the 5–100 ms range and treat each command/status interaction as an ordinary PDU. This cell operates at 4–8 kHz with sub-microsecond clock alignment. Its native network is engineered as a schedule, not merely as best-effort messages with priority.

The assignment is deliberately designed to reveal whether WireSpaces:

- adds unacceptable per-message overhead or queueing variability;
- lacks a coherent multi-axis snapshot primitive;
- pushes cyclic aggregation into an application gateway that erases useful Participant identity;
- cannot express redundant ring delivery without duplicate canonical Wires;
- is suitable only for the maintenance/diagnostic plane.

A clean result may be:

> Keep the native deterministic motion protocol for the cyclic control plane; use WireSpaces for configuration, Health, diagnostics, logs, and update.

That is a valid scope boundary, not a failed experiment.

Do not invent a time-triggered Transport, atomic group delivery, scheduled Wire, or redundant-Link profile to make the mapping pass.

---

## 1. Falsification criteria — mandatory before mapping

The mapper must commit to quantitative criteria such as:

```text
I will judge WS POOR for the cyclic motion plane if:
    - meeting the cycle requires collapsing many real axis Participants into
      one aggregate Participant solely to remove protocol overhead;
    - one-PDU-per-interaction cannot fit the serialization and processing budget;
    - coherent cycle snapshots require an unspecified atomic multicast/snapshot
      mechanism;
    - QoS is used as a substitute for a deterministic transmission schedule;
    - redundant physical delivery becomes duplicate application commands with
      no bounded reconciliation story.

I will judge WS VIABLE for the cyclic plane if:
    - canonical Participants and Endpoints survive without artificial collapse;
    - the complete cyclic exchange fits with explicit timing arithmetic;
    - bounded jitter, freshness, synchronization, and failure recovery are
      supplied by named existing mechanisms rather than assumed.
```

Report honestly against these criteria.

---

## 2. Prohibited escape hatches

| Prohibited move | Why |
|---|---|
| “1 Gbit/s is ample” without serialization and packet-rate arithmetic | Cycle time, frame overhead, processing, and jitter—not average bandwidth—are the constraints. |
| Treating QoS as a deterministic schedule | Priority does not provide fixed transmission slots, coherent cycle boundaries, or bounded worst-case queueing. |
| Collapsing all axes into one Participant to pass the budget | Each drive is independently diagnosable, replaceable, and fault-contained. |
| Inventing atomic multicast or a time-triggered WS Transport | Record the missing semantic instead. |
| Counting a native aggregated process-image frame as dozens of independently delivered canonical PDUs | State where aggregation occurs and whether canonical identity survives. |
| Ignoring the redundant ring path | Single-link failure recovery is mandatory in Config B/C. |
| Assuming destination pruning solves schedule coherence | Pruning saves bandwidth; it does not create simultaneous application of setpoints. |
| Moving the cycle into shared memory and declaring the network solved | The axes are physically distributed. |
| Treating safety traffic as ordinary background diagnostics | Safety reaction has a separate bounded deadline, although no formal safety case is required. |
| Adding new synchronization, reliability, or redundancy features | This is an evaluation, not a protocol-design exercise. |

---

## 3. Native system

A high-speed packaging and precision-converting machine coordinates servo axes, distributed I/O, registration sensors, and vision triggers. Product quality depends on applying one coherent command set across all axes at a known cycle boundary.

### Config A — 16-axis baseline line

```text
1 Motion Controller
16 servo drives
4 distributed I/O blocks
2 registration/encoder units
1 Safety Controller
1 HMI/service computer

250 µs cycle (4 kHz)
single deterministic line topology
```

### Config B — 48-axis production cell

```text
MotionController_A      active control computation
MotionController_B      shadow computation / takeover candidate
48 servo drives
12 distributed I/O blocks
4 registration/encoder units
4 vision/trigger units
1 Safety Controller
1 Cell Supervisor
1 HMI/service computer

250 µs cycle (4 kHz)
dual-ended / ring physical topology
one link break must not stop cyclic communication
```

There is no networking-layer primary role. Active/shadow selection and command acceptance are application/control-system semantics.

### Config C — 96-axis high-density variant

```text
same controller architecture
96 servo drives
24 distributed I/O blocks
8 registration/encoder units
8 vision/trigger units
8 hot-connect tool modules

125 µs cycle (8 kHz)
redundant ring
```

Tool modules are connected/disconnected only during controlled machine changeover, not arbitrary mid-cycle hot-plug. The cyclic schedule is regenerated and validated before motion resumes.

---

## 4. Physical topology

### 4.1 Deterministic motion network

```text
                  MotionController_A
                         |\
                         | \      redundant return path
                         |  \________________________
                         |                           \
                    Drive01 -- Drive02 -- ... -- Drive48
                       |                         |
                     I/O blocks -- sensors -- Safety
                         \_______________________/
                                   |
                          MotionController_B
```

The drawing is schematic. The native industrial network may use:

- one frame that traverses nodes in physical order and is modified on the fly;
- scheduled TSN windows with precomputed per-stream slots;
- a controller-generated process image distributed to all nodes;
- hardware timestamping and distributed clocks;
- a redundant reverse path selected after a link break.

The mapper must state which abstract carrier behavior it assumes. Do not combine mutually incompatible benefits from several technologies without naming the assumption.

### 4.2 Service network

```text
Cell Supervisor --- ordinary Ethernet switch --- HMI/service computer
       |                         |
MotionController_A/B -------- service gateway
```

The service network carries engineering telemetry, logs, Identity/Version/Health, configuration, recipes, and firmware update. It is not required to meet the 125–250 µs motion cycle.

### 4.3 Controller internal links

Each Motion Controller may contain:

```text
Control domain <-> shared-memory Link <-> Network/Platform domain
```

Model two Participants only if the mapper preserves the actual dispatch/fault boundary. Do not split domains merely to make the sketch larger.

---

## 5. Participant model

For Config B, use at least:

| Participant class | Count | Role |
|---|---:|---|
| Motion control domain(s) | 2 | Trajectory interpolation, coordinated setpoint generation, takeover logic |
| Motion platform/network domain(s) | 2 optional | Link ownership, schedule interface, diagnostics; include only if separately modeled |
| Servo drives | 48 | Apply setpoint at cycle boundary; return coherent feedback and fault state |
| Distributed I/O | 12 | Latch inputs and apply outputs on cycle boundary |
| Registration/encoder units | 4 | High-precision position/registration state |
| Vision/trigger units | 4 | Trigger timing and result metadata; raw images remain local |
| Safety Controller | 1 | Safe stop/torque-off commands and safety state |
| Cell Supervisor | 1 | Recipe and production-state coordination; not inner-loop controller |
| HMI/service computer | 1 | Engineering and maintenance |

Do not model motor phase-current loops, encoder raw edges, FPGA sub-blocks, or safety-channel internals as separate Participants.

---

## 6. Native cyclic communication model

### 6.1 Coherent command image

Every cycle, the active motion computation produces:

```text
CycleNumber
ApplyTime
Axis[1..N]:
    position/velocity/torque setpoint
    mode/control word
DistributedOutputs
TriggerSchedule
```

All participating drives and I/O nodes must apply values for cycle `k` coherently. Receiving one axis command from cycle `k+1` while another still applies cycle `k` is an error even if both individual datagrams arrived before their local deadlines.

### 6.2 Coherent feedback image

Every cycle, the controller consumes:

```text
Axis[1..N]:
    measured position/velocity
    status/control word
    following error / current summary
DistributedInputs
Registration timestamps
```

The control algorithm needs one cycle-indexed snapshot. Late data is marked invalid/stale; it must not silently join the next snapshot.

### 6.3 Native process-image size assumptions

Use these challenge values unless justified otherwise:

| Item | Command bytes | Feedback bytes |
|---|---:|---:|
| Servo axis | 16 | 16 |
| Distributed I/O block | 8 | 8 |
| Registration/encoder unit | 8 | 16 |
| Vision/trigger unit | 8 | 8 |
| Cycle metadata / integrity | 32 total | 32 total |

For Config B, compute the aggregate command and feedback image sizes. For Config C, repeat at 8 kHz.

### 6.4 Safety cycle

Safety state uses a separately validated black-channel style data set on the same physical deterministic network:

- safe-stop/torque-off request to all drives;
- per-drive safety state;
- sequence/freshness/integrity metadata;
- end-to-end reaction target ≤4 ms;
- ordinary firmware update or engineering telemetry must not affect this bound.

Do not design the safety protocol or claim certification.

---

## 7. Required non-cyclic interactions

These are intentionally a much more comfortable WS shape:

| Interaction | Pattern | Rate / trigger |
|---|---|---|
| Identity / Version / Health | query + low-rate publication | startup, 0.2–2 Hz, event |
| Drive and I/O faults | event + snapshot | event |
| Link status / clock status | publication/query | 1–10 Hz |
| Recipe/configuration | supervisor → selected devices | changeover |
| Engineering telemetry | selected devices → service computer | configurable |
| Logs/event history | query / bounded transfer | maintenance |
| Firmware update | service computer → one selected device/group | stopped machine only |
| Axis tuning/query RPC | service computer ↔ selected drive | commissioning |
| Schedule/version inventory | controllers/network devices | startup/changeover |

The mapper must evaluate whether cyclic and non-cyclic traffic should share canonical Wires, share only the physical network, or use separate networking systems.

---

## 8. Timing and arithmetic requirements

### 8.1 One-PDU-per-axis mapping

For Config B and Config C, compute:

```text
command PDUs per cycle
feedback PDUs per cycle
PDUs per second
canonical header bytes
Link framing bytes (state assumption)
wire serialization time
packet/frame processing budget per Participant and controller
```

Minimum Config B count if each device command and feedback is separate:

```text
48 drive commands + 48 drive feedback
+ I/O + encoder + trigger traffic
all repeated at 4 kHz
```

Do not stop at aggregate Mbit/s. A design can fit average bandwidth and still fail packet rate, jitter, or schedule constraints.

### 8.2 Aggregated process-image mapping

If aggregating many axes into one or a few PDUs, state:

- canonical `SrcParticipantId` and `DestParticipantId`;
- where individual drive identity resides;
- whether drives remain independently addressable Participants for the cyclic data;
- who packs/unpacks the image;
- whether the mapping has created an application gateway/broker absent from the native protocol;
- how partial loss or one stale axis is represented;
- whether one Endpoint storage object can express the coherent snapshot.

### 8.3 Broadcast mapping

If using one controller→broadcast command PDU:

- explain how each drive extracts only its command;
- count payload size and fragmentation;
- explain whether broadcast Endpoint semantics imply every drive receives the complete image;
- show how one drive's command can be updated without changing a shared schema;
- explain feedback fan-in separately.

### 8.4 Timing budget

For 250 µs and 125 µs cycles, allocate explicit worst-case budgets for:

```text
control computation
Endpoint submission
Router / queueing
Link framing
serialization
per-hop / per-node processing
clock boundary / apply time
feedback return
margin
```

Average or typical values are insufficient.

---

## 9. Redundancy and failure cases

The mapping must address:

1. **One cable break.** Native ring resumes cyclic traffic within the stated recovery assumption; no cycle may apply duplicate setpoints.
2. **One drive fails silent.** Other drives continue; coherent snapshot marks that axis unavailable.
3. **One drive babbles or overruns its slot.** Native schedule contains the failure; state what the assumed Link profile provides.
4. **MotionController_A fails.** Controller_B takes over according to application policy without Participant renumbering.
5. **Controller disagreement.** Network does not decide which setpoints are authoritative.
6. **Duplicate path delivery.** If both directions/paths deliver one command, show where deduplication occurs.
7. **Clock grandmaster/source changes.** State which layer maintains time continuity; do not assign this to Participant identity.
8. **Service update underway.** Safety and cyclic traffic retain bounds, or update is rejected/deferred.
9. **Tool module added at changeover.** Schedule and configuration update is validated before motion restarts.
10. **Link recovers.** Rejoining a redundant path must not create a forwarding loop or duplicate command application.

---

## 10. Required mapping arms

Every mapper must produce and compare all three arms.

### Arm A — canonical per-Participant datagrams

Map drives, I/O, sensors, controllers, and safety as ordinary Participants with directed/broadcast PDUs. This is the most literal WireSpaces mapping.

Report timing, packet rate, coherent snapshot semantics, and redundant-path behavior.

### Arm B — aggregated cyclic process image

Carry a native-like process image in one or a few large PDUs or behind one aggregate Participant/Endpoint.

Report which canonical identity and Service properties are lost, moved into payload schemas, or delegated to an aggregator.

### Arm C — split architecture

Keep the native deterministic protocol for cyclic motion/safety and use WireSpaces only for non-cyclic Services over the service network or a low-priority service channel.

Report:

- which devices remain WS Participants;
- whether the same physical Ethernet carries both systems;
- where composition/proxy occurs between diagnostics and native drive identity;
- whether this restricted scope still offers enough value to justify WS.

Do not declare Arm C a failure merely because WS does not own the inner loop.

---

## 11. Expected diagnostic questions

### Question A — datagrams vs process image

Does one canonical PDU per relationship fit the native cyclic model, or does it replace one coherent image with hundreds of independently queued objects?

### Question B — coherent delivery

Where are cycle number, apply time, atomic snapshot completeness, and stale-axis handling specified? Endpoint Queue/Snapshot semantics are local storage semantics; do not assume they provide distributed atomicity.

### Question C — deterministic schedule

Can static Wire membership plus QoS express a time-triggered schedule? If the answer depends entirely on a specialized Link profile, state whether WireSpaces is adding value or merely encapsulating the native schedule.

### Question D — redundancy

Does the redundant ring require:

- separate canonical Wires per physical path;
- one opaque redundant Physical Link;
- a new bounded redundant-Link profile;
- native protocol below WS?

Do not invent the missing option.

### Question E — source identity under aggregation

If one controller/aggregator authors the process image, is canonical per-axis source identity still meaningful for feedback? If identities move into payload records, has the canonical model ceased to describe the important cyclic semantics?

### Question F — control/service plane split

Is WS clearly useful for Identity, Health, diagnostics, logs, configuration, and update while being poorly suited to cyclic control? A split verdict is expected and acceptable.

### Question G — tiny endpoint cost

What Router, Endpoint, buffering, and schema machinery must every drive implement if WS participates only in maintenance? Is that smaller or larger than retaining the native service mailbox?

### Question H — failure auditability

Can an engineer determine from the mapping:

```text
which cycle was applied
which axes were coherent
which path delivered it
whether a duplicate was suppressed
which controller's command was accepted
```

without relying on unspecified application behavior?

---

## 12. Valid experiment outcomes

```text
"WireSpaces is a poor fit for the 125–250 µs cyclic plane because its natural
 unit is an independently routed datagram while the application requires one
 scheduled coherent process image."

"A specialized deterministic Link profile can carry WS identity, but nearly
 all valuable timing semantics remain in the native Link schedule; WS adds
 little to the inner loop."

"Aggregating the process image makes timing feasible but moves per-axis identity,
 freshness, and partial-validity semantics into payload schemas, weakening the
 value of canonical addressing."

"Separate per-axis PDUs fit raw bandwidth but fail packet-rate, jitter, or
 coherent-snapshot requirements."

"The redundant ring is best treated as one opaque native Physical Link; exposing
 both paths as Wires pushes duplicate suppression into Services."

"The split architecture is compelling: native cyclic control plus WS maintenance
 Services gives useful ecosystem continuity without forcing WS into the
 sub-millisecond schedule."

"Even maintenance-only WS is too heavy for the drive target; WS should terminate
 at the motion controller's diagnostic gateway."
```

A strong-pass result for Arm A requires complete worst-case timing and coherence evidence, not only representability.

---

## 13. What this archetype does not include

- motor current/commutation loops;
- raw camera images;
- motion-control mathematics;
- detailed TSN standards, EtherCAT conformance, or vendor protocol design;
- formal functional-safety certification;
- security, authentication, authorization, or access-control requirements;
- Internet/cloud connectivity;
- dynamic routing;
- arbitrary mid-motion hot-plug;
- a requirement to replace an existing deterministic fieldbus;
- a new WS time-triggered or redundant-Link protocol.

---

## 14. Mandatory output additions

In addition to the normal sketch template:

```text
Falsification criteria

Arm A / B / C comparison:
    canonical Participants retained
    Wire count
    PDU/frame rate
    serialized bytes per cycle
    timing mechanism
    coherent snapshot mechanism
    redundant-path behavior
    failure consequences
    configuration burden

Config A / B / C arithmetic

Explicit scope verdict:
    cyclic control plane
    safety plane
    maintenance/service plane
```


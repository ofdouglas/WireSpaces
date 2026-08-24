# Sketch 15 — Sub-Millisecond Redundant Motion-Control Cell

**Archetype:** `archetypes/15_submillisecond_redundant_motion_cell.md`  
**Agent:** `gpt56sol_20260823`  
**Output file:** `sketches/15_submillisecond_redundant_motion_cell_gpt56sol_20260823.md`  
**Date:** 2026-08-23

---

## Executive summary

**Minimum mapping:** Arm C is the minimum semantically correct deployment: the native deterministic network owns cyclic motion and safety, while 25 / 73 / 149 WS Participants share one maintenance Wire over ordinary Ethernet plus a bounded native mailbox channel where available.  
**Question A (R6):** **Poor for cyclic motion; natural for service** — canonical Participant identity survives, but independent datagrams, local Endpoint Snapshots, QoS, and loop-free Wires do not provide a distributed coherent process image, timed apply boundary, deterministic schedule, or redundant-ring reconciliation.  
**Question B (CAN11):** **N/A** — no CAN11 Link is used.  
**Worst friction (minimum Arm C path):** **Failure/topology mismatch — Mild** because the redundant motion ring is intentionally opaque to WS and must expose its status through Services.  
**Main lesson:** Config B per-device datagrams fit raw 1-Gbit/s serialization only under implausibly tight deterministic processing bounds, Config C does not fit even raw serialization, and aggregation works only by moving the important cyclic semantics into a native process-image payload and underlay.

---

## Disposition block

| Area | Assessment |
|---|---|
| Participant identity | Natural |
| Wire decomposition | Natural for service; poor as a cyclic-schedule model |
| Forwarding | Simple in Arm C |
| Non-CAN configuration | Moderate |
| CAN11 VCN fit | N/A |
| Better with CAN29? | N/A |

**Question A (R6):** R6 cleanly identifies controllers, drives, I/O, sensors, safety, supervisor, and service tooling, and one maintenance Logical Bus is natural. It does not specify the cyclic plane's defining properties: a schedule, distributed clock alignment, cycle-indexed completeness, coherent timed application, or duplicate-free ring recovery. **Question B (CAN11):** no CAN11 evidence is produced; the limits found here are independent of unified VCN.

---

## Falsification criteria

I will judge WS **POOR for the cyclic motion plane** if:

- retaining independently diagnosable axis Participants requires at least 136 Config B or 288 Config C cyclic PDUs per cycle and the complete worst-case exchange does not fit;
- meeting timing requires collapsing cyclic axis identity into one aggregate Participant or payload record solely to reduce packet overhead;
- coherent apply and feedback snapshots depend on an unspecified distributed atomic/snapshot mechanism;
- QoS or static Wire membership is used as a substitute for fixed transmission slots and bounded queueing;
- exposing two ring paths creates duplicate canonical commands or a Wire loop, while hiding them makes all recovery semantics native and opaque; or
- the timing argument has no named mechanism for clock alignment, stale-cycle rejection, babbler containment, controller takeover, and service-traffic admission.

I will judge WS **VIABLE for the cyclic motion plane** only if:

- all real device Participants remain canonical cyclic sources/destinations;
- the complete Config C 125-µs exchange fits explicit worst-case serialization and processing budgets;
- one specified mechanism provides cycle completeness and coherent apply without inventing a WS feature; and
- one cable break cannot cause a duplicate or mixed-cycle application.

I will judge restricted WS use **VALUABLE** if the service plane retains per-device identity and useful standard Services without affecting the native cyclic/safety bounds or imposing unjustified machinery on every drive.

The result is evaluated in §6.8.

---

## 1. Native communication model

The machine is naturally a scheduled process-image system. One active motion computation produces a cycle-numbered command image. Hardware-assisted distributed clocks and a deterministic network deliver that image, every drive and I/O node applies its own record at the named boundary, and feedback is assembled into one cycle-indexed image for the next control computation. The network's schedule, not average priority, bounds transmission and processing.

The production and high-density cells use a dual-ended physical ring. Native link logic selects or combines directions, suppresses duplicates, contains out-of-slot transmission, and maintains distributed time. A black-channel safety data set uses separately validated sequence, freshness, and integrity behavior on the same carrier. Ordinary Ethernet handles recipes, diagnostics, logs, and maintenance without participating in the 125–250 µs cycle.

---

## 2. Obvious conventional implementation

> **Obvious conventional implementation:** Use an EtherCAT-class cyclic process-image network with distributed clocks, dual-ended ring recovery, on-the-fly or scheduled image transfer, and native mailbox traffic; use ordinary Ethernet for engineering Services.

> **What structure does a conventional design use?** Axis and I/O records occupy preassigned offsets in a generated process image. A cycle counter, apply time, validity/working counters, native schedule, and distributed clocks define coherence. The redundant ring is one native network, not two application command channels. Service traffic is admitted only in bounded residual windows or crosses a separate service network.

This sketch assumes that abstract carrier behavior. It does not also claim unrelated TSN per-stream benefits. The assumed native link guarantees:

- generated cyclic slots and fixed process-image offsets;
- hardware distributed clocks with less than 1 µs alignment;
- a dual-ended ring that recovers a single cable break within one cycle;
- native cycle/sequence duplicate rejection, so at most one command image is applied;
- node transmit containment through fixed on-the-fly fields or scheduled gates; and
- mailbox/service transfer only outside reserved cyclic and safety capacity.

If a chosen product cannot provide those properties, the native design—not only a WS mapping—must be requalified.

---

## 3. Minimum mapping — Arm C split architecture

Arm C is primary because it is the simplest mapping that does not claim semantics R6 lacks.

### 3.1 Participants

No optional controller platform/network domains are split in the minimum mapping. Each controller has one coherent dispatch/fault domain. A later implementation may split it only if control and network/platform code are genuinely separate Endpoint Domains connected by shared memory.

| Configuration | Motion controllers | Drives | I/O | Registration | Vision | Tool modules | Safety | Supervisor | HMI | Total |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| A | 1 | 16 | 4 | 2 | 0 | 0 | 1 | 0 | 1 | **25** |
| B | 2 | 48 | 12 | 4 | 4 | 0 | 1 | 1 | 1 | **73** |
| C | 2 | 96 | 24 | 8 | 8 | 8 | 1 | 1 | 1 | **149** |

Illustrative Config B allocation:

| ParticipantId(s) | Endpoint Domain(s) | Notes |
|---|---|---|
| `0x00..0x01` | MotionController_A/B | Distinct identities survive active/shadow takeover |
| `0x10..0x3F` | Drive01..48 | Independently diagnosable and replaceable |
| `0x40..0x4B` | IO01..12 | |
| `0x50..0x53` | Registration01..04 | |
| `0x58..0x5B` | Vision01..04 | Raw images remain local |
| `0x60` | SafetyController | Service identity only; safety cycle remains native |
| `0x61` | CellSupervisor | |
| `0x62` | HMI/service computer | Preconfigured member of the service Wire |

Config C's 149 Participants fit ordinary 8-bit identity. Active/shadow selection is application state; no Participant is renumbered when Controller B takes over.

### 3.2 Wires

| Wire (#) | Participants | Physical links | Purpose |
|---|---|---|---|
| `W_Service` (`W01`) | all 25 / 73 / 149 WS Participants | ordinary service Ethernet; optional native mailbox Link through the service gateway/controller | Identity, Version, Health, faults, link/clock status, recipes, logs, update, tuning, and schedule inventory |

The cyclic process image and black-channel safety data are not WS Wires in Arm C. They remain native traffic on the deterministic network. If controller internals are later split, one device-private `W_ControllerInternal` per controller may cross shared memory; that is not required by this mapping.

### 3.3 Interactions

| Interaction | Src | Dest | Wire | Broadcast? | Notes |
|---|---|---|---|---|---|
| Identity / Version / Health | every device | Supervisor/HMI/controllers | `W_Service` | Optional low-rate publication | 0.2–2 Hz/event |
| Drive, I/O, controller faults | device | controllers/Supervisor/HMI | `W_Service` | No | Event plus Snapshot |
| Link/clock status | network-facing devices/controllers | Supervisor/HMI | `W_Service` | No | 1–10 Hz; reports native underlay state |
| Recipe/configuration | Supervisor | selected devices | `W_Service` | No | Changeover only |
| Engineering telemetry | selected device | HMI | `W_Service` | No | Configured and bounded |
| Logs/event history | device | HMI | `W_Service` | No | Bounded maintenance transfer |
| Firmware update | HMI | one selected device/group | `W_Service` | Directed | Machine stopped; no cyclic timing claim |
| Axis tuning/query | HMI | selected drive | `W_Service` | No | Commissioning |
| Schedule/version inventory | controllers/network devices | Supervisor/HMI | `W_Service` | No | Startup/changeover |

### 3.4 Forwarding and composition

| Ingress | Wire | Egress | Notes |
|---|---|---|---|
| Service Ethernet | `W_Service` | native mailbox channel | Service gateway/controller forwards canonical PDUs if the native mailbox profile preserves Participant identity |
| Native mailbox | `W_Service` | service Ethernet | Complete canonical PDU reconstructed before forwarding |

The redundant ring is one opaque Physical Link at the WS boundary. Its two directions are not two Wires or two WS Link Interfaces participating in generic forwarding. Native link telemetry must report active path, break location, clock source, duplicate suppression, and recovery state.

If the native mailbox cannot carry canonical per-device PDUs, WS terminates at MotionController/service-gateway Participants. Drive identity then appears behind explicit diagnostic proxy Endpoints, and the gateway authors translated responses with its own ParticipantId. That fallback is valid composition but offers less WS value.

### 3.5 Link profiles

| Physical link | Profile | WS Wire(s) | Notes |
|---|---|---|---|
| Ordinary service Ethernet | WS direct Ethernet or WS/UDP candidate | `W_Service` | No hard cyclic deadline |
| Deterministic ring mailbox channel | specialized master-initiated non-cyclic Link profile, if implemented | `W_Service` | Polling does not change canonical source (`CORE §1.7`) |
| Deterministic cyclic channel | native motion protocol, non-WS | none | Owns schedule, process image, clocks, and redundancy |
| Native safety channel | native black-channel data set, non-WS | none | ≤4 ms target remains outside this sketch's certification scope |
| Controller shared memory | unused in minimum; ordinary shared-memory profile if domains split | optional device-private Wire | No artificial domain split |

No CAN11 binding or VCN accounting applies.

### 3.6 Membership versus presence

All fixed devices remain configured members of `W_Service` when temporarily unreachable. Config C tool modules are present only in a committed changeover configuration. Adding or removing a tool module changes membership and the native cyclic schedule while motion is stopped; physical disconnection alone does not edit Wiring.

---

## 4. Required mapping arms and arithmetic

### 4.1 Native process-image sizes

Config C assumes each hot-connect tool module has one 8-byte command and one 8-byte feedback record, matching the smallest supplied I/O-like record. The known total without tool records is shown separately.

| Config | Command image | Feedback image | Total/cycle | Cycle rate | Native payload rate |
|---|---:|---:|---:|---:|---:|
| A | `16×16 + 4×8 + 2×8 + 32 = 336 B` | `16×16 + 4×8 + 2×16 + 32 = 352 B` | 688 B | 4 kHz | 22.016 Mbit/s |
| B | `48×16 + 12×8 + 4×8 + 4×8 + 32 = 960 B` | `48×16 + 12×8 + 4×16 + 4×8 + 32 = 992 B` | 1,952 B | 4 kHz | 62.464 Mbit/s |
| C, known devices | 1,888 B | 1,952 B | 3,840 B | 8 kHz | 245.760 Mbit/s |
| C, with 8-byte tool records | **1,952 B** | **2,016 B** | **3,968 B** | 8 kHz | **253.952 Mbit/s** |

Raw payload bandwidth fits 1 Gbit/s. That fact alone says nothing about frame count, schedule, traversal, or coherence.

### 4.2 Link-framing assumptions

For comparable lower-bound arithmetic:

- direct 1-Gbit/s Ethernet-like framing;
- preferred provisional canonical header: 6 bytes;
- one small WS PDU occupies one minimum Ethernet frame;
- minimum frame serialization including preamble and inter-frame gap: 84 wire bytes;
- a large aggregated PDU adds 38 wire bytes per Ethernet frame;
- standard 1,500-byte payload is used; Config C images require two frames per direction;
- no retries, safety telegrams, clock telegrams, or repeated cycle metadata are included unless stated.

The Arm A values are optimistic: repeated per-PDU cycle/apply/integrity metadata is not charged. It still fits inside minimum Ethernet padding for the supplied 8/16-byte records, so packet count remains the dominant serialization cost.

### 4.3 Arm A — canonical per-Participant datagrams

Mapping:

- `W_Cyclic`: controllers, drives, I/O, registration, and vision units;
- `W_Safety`: SafetyController and safety-capable drives;
- `W_Service`: all service Participants;
- one directed command PDU and one directed feedback PDU per cyclic device per cycle;
- Controller A and B retain distinct ParticipantIds; controller selection remains application state.

#### PDU and serialization lower bounds

| Config | Cyclic devices | Command + feedback PDUs/cycle | PDUs/s | Canonical header bytes/cycle | Wire bytes/cycle | Serialization/cycle | Wire rate |
|---|---:|---:|---:|---:|---:|---:|---:|
| A | 22 | 44 | 176,000 | 264 | 3,696 | 29.568 µs | 118.272 Mbit/s |
| B | 68 | 136 | 544,000 | 816 | 11,424 | 91.392 µs | 365.568 Mbit/s |
| C | 136 + 8 tools | 288 | 2,304,000 | 1,728 | 24,192 | **193.536 µs** | **1.548 Gbit/s** |

Config C fails before Router, queue, traversal, clock, computation, safety, or margin: minimum frame serialization alone exceeds both 1 Gbit/s and the 125-µs cycle.

Config B fits raw serialization but leaves a hard processing problem. The controller handles 544,000 cyclic PDUs/s. A deliberately optimistic 250-ns bound per PDU consumes 34 µs/cycle across 136 send/receive operations—about 125 cycles on a 500-MHz processor, including Endpoint, Router, queue, and bookkeeping work.

#### Optimistic Config B worst-case budget

| Stage | Budget |
|---|---:|
| Control computation | 70.0 µs |
| 68 command Endpoint/Router operations at 0.25 µs | 17.0 µs |
| Link framing and fixed queue release | 5.0 µs |
| Command serialization | 45.696 µs |
| Maximum scheduled traversal/node delay | 15.0 µs |
| Clock/apply guard | 5.0 µs |
| 68 feedback Endpoint/Router operations | 17.0 µs |
| Feedback serialization | 45.696 µs |
| Controller snapshot acceptance/checking | 17.0 µs |
| **Total** | **237.392 µs** |
| **Margin** | **12.608 µs** |

This is not a valid guarantee:

- no base WS mechanism supplies the fixed releases or the 250-ns processing bound;
- safety/clock traffic, retries, cache misses, queue contention, and cycle metadata are omitted;
- 12.6 µs is only 5% margin;
- independent Endpoint delivery does not prove all 68 command PDUs are present at every recipient's apply boundary; and
- a controller can collect cycle-numbered feedback in application state, but a local Snapshot Endpoint (`CORE §9.5`) is not a distributed atomic snapshot.

Redundancy also has only two valid representations: treat the entire native ring as one opaque Physical Link, in which case recovery/deduplication is native, or expose paths separately and confront duplicate commands/loop-free Wire limits. Arm A gains no WS solution from the latter.

**Arm A verdict:** poor. Config B is an unsupported knife-edge even before coherence; Config C is physically unrepresentable under the stated framing.

### 4.4 Arm B — aggregated cyclic process image

Arm B adds one network/platform Endpoint Domain per controller because packing, native schedule ownership, ring recovery, and feedback assembly are treated as a real dispatch/fault boundary. Config B therefore has 75 WS Participants, but only controller/platform Participants are canonical cyclic authors:

| Image | Canonical source | Canonical destination | Axis identity |
|---|---|---|---|
| Command image | active MotionController | active NetworkPlatform | Record offset / axis key inside payload; the platform emits the native process image |
| Feedback image | active NetworkPlatform aggregator | active MotionController | Record offset, validity bitmap, and cycle number inside payload |

Drives remain WS Participants for Services, but they are no longer canonical cyclic destinations/sources. Every drive receives or is processed through the whole command image, extracts its fixed record, and applies it according to native `CycleNumber`/`ApplyTime`. One drive's command changes a record value or generated image schema; it does not create an independently addressed PDU. Partial loss/staleness is represented by per-record validity/working-counter fields in the payload.

#### Aggregated framing

| Config | PDUs/cycle | Ethernet frames/cycle | Serialized bytes/cycle | Serialization/cycle | PDU rate | Wire rate |
|---|---:|---:|---:|---:|---:|---:|
| A | 2 | 2 | `688 + 12 + 76 = 776` | 6.208 µs | 8,000/s | 24.832 Mbit/s |
| B | 2 | 2 | `1,952 + 12 + 76 = 2,040` | 16.320 µs | 8,000/s | 65.280 Mbit/s |
| C | 2 | 4 | `3,968 + 12 + 152 = 4,132` | 33.056 µs | 16,000/s | 264.448 Mbit/s |

The complete Config C image now fits raw bandwidth and frame count.

#### Config B aggregated budget

| Stage | Budget |
|---|---:|
| Control computation | 70.0 µs |
| Endpoint submission + Router | 2.0 µs |
| Framing/scheduled release | 2.0 µs |
| Command serialization | 8.032 µs |
| Native ring traversal/on-the-fly processing | 20.0 µs |
| Clock/apply guard | 5.0 µs |
| Native feedback assembly/validity | 10.0 µs |
| Feedback serialization | 8.288 µs |
| Controller acceptance | 2.0 µs |
| **Total** | **127.320 µs** |
| **Margin** | **122.680 µs** |

#### Config C aggregated budget

| Stage | Budget |
|---|---:|
| Control computation | 40.0 µs |
| Endpoint submission + Router | 1.0 µs |
| Framing/scheduled release | 1.0 µs |
| Two-frame command serialization | 16.272 µs |
| Native ring traversal/on-the-fly processing | 25.0 µs |
| Clock/apply guard | 3.0 µs |
| Native feedback assembly/validity | 8.0 µs |
| Two-frame feedback serialization | 16.784 µs |
| Controller acceptance | 1.0 µs |
| **Total** | **112.056 µs** |
| **Margin** | **12.944 µs** |

These budgets can be plausible only because the named native mechanisms supply fixed release, process-image packing, distributed clocks, per-record validity, ring recovery, and duplicate suppression. WS contributes a canonical envelope around one aggregate, not the timing semantics. The large Endpoint Snapshot is coherent locally after complete PDU reassembly, but it does not cause distributed coherent application.

**Arm B verdict:** timing-feasible under the native assumptions, but canonical per-axis cyclic identity has moved into payload offsets. It is a native process image carried in WS-shaped packaging, with little inner-loop value from the generic Wire model.

### 4.5 Arm C — split architecture

Arm C uses the minimum mapping in §3.

| Measure | Result |
|---|---|
| Canonical Participants retained | All devices for non-cyclic Services if native mailboxes can reconstruct canonical PDUs |
| WS Wire count | 1 (`W_Service`); optional internal Wires only for real domain splits |
| Cyclic PDU/frame rate | 0 WS; native protocol owns it |
| WS service rate | Low-rate/event/bounded maintenance; update only while stopped |
| Timing mechanism | Native deterministic schedule and distributed clocks |
| Coherent snapshot | Native process image, cycle counters, validity, and apply time |
| Redundant path | One opaque native Physical Link with native recovery/deduplication |
| Failure consequence | Native control remains qualified independently; WS loss removes diagnostics/maintenance only |
| Configuration burden | 25 / 73 / 149 memberships plus Endpoint/profile bindings; native schedule remains separately generated |

The same physical deterministic Ethernet may carry WS mailbox traffic only if its native scheduler reserves bounded residual windows and rejects/defer service load that would affect cyclic or safety traffic. The preferred mapping uses the separate ordinary service Ethernet for bulk diagnostics/update. QoS is not credited with protecting the cyclic schedule.

**Arm C verdict:** strong. It preserves useful per-device maintenance identity and Service reuse while drawing an explicit boundary around a control plane whose natural abstraction is not an independent datagram.

### 4.6 Arm comparison

| Property | Arm A: per-device PDUs | Arm B: aggregate image | Arm C: split |
|---|---|---|---|
| Canonical cyclic axis identity | Yes | No; payload records | N/A; native |
| WS Participants retained | All | All for service; 2 aggregate cyclic authors | All for service |
| WS Wires | 3 | 3 | 1 |
| Config B cyclic PDUs/s | 544,000 | 8,000 | 0 |
| Config C cyclic PDUs/s | 2,304,000 | 16,000 | 0 |
| Config B serialized bytes/cycle | 11,424 | 2,040 | Native |
| Config C serialized bytes/cycle | 24,192 | 4,132 | Native |
| Timing mechanism | Missing unless native schedule wraps every PDU | Native schedule | Native protocol |
| Coherent snapshot | Missing distributed semantic | Payload/native process image | Native process image |
| Ring redundancy | Must be opaque or duplicates emerge | Opaque native Link | Opaque native Link |
| Failure auditability | Application must reconstruct 136/288 deliveries | Native record validity and cycle metadata | Native tooling; WS reports summaries |
| Configuration burden | High: per-PDU slots/bounds/endpoints | High native image schema; low WS packet count | Moderate WS service configuration plus existing native schedule |
| Verdict | Poor | Feasible but semantically native | Preferred |

---

## 5. Failure and lifecycle analysis

| Case | Required behavior and responsible layer |
|---|---|
| One cable break | Native dual-ended ring restores reachability within the assumed one-cycle bound. One cycle number is applied at most once; WS generic forwarding is not involved. |
| One drive silent | Native process-image validity/working counter marks that axis unavailable; other offsets remain valid. WS Health later reports the device unreachable. |
| Drive babbles/overruns | Native fixed field/scheduled-gate hardware contains it. QoS and WS Endpoint bounds are not credited with containment. |
| Controller A fails | Controller B retains its own PID and takes over under application epoch/selection policy. No network-layer primary or renumbering exists. |
| Controllers disagree | Drive command-acceptance policy/native control epoch decides; the network does not elect authority. |
| Duplicate path delivery | Native adapter compares cycle/sequence and commits one image. If the underlay cannot guarantee this, the mapping fails. |
| Clock source changes | Native distributed-clock layer provides continuity/holdover and reports the event through `W_Service`; Participant identity is unchanged. |
| Service update underway | Update is rejected/deferred while moving or confined to the separate service network. Native safety/cyclic reserved capacity is untouched. |
| Tool module added | Motion is stopped; membership, image schema, offsets, schedule, and resource bounds are regenerated and validated before restart. |
| Link recovers | Native ring rejoin suppresses duplicate direction before cyclic release. WS still sees one Physical Link, so no Wire loop appears. |

The ≤4-ms safety target equals 16 Config B cycles or 32 Config C cycles. Safety sequence, freshness, integrity, schedule slots, and reaction proof remain in the native black-channel system. WS may report safety state but is not part of the claimed reaction path.

Failure auditability requires the native controller/network telemetry to record:

- accepted controller/epoch and cycle number;
- apply time and image integrity;
- per-axis validity/staleness;
- physical path and break/recovery state;
- whether a duplicate was suppressed; and
- clock source/continuity state.

WS can transport that record on `W_Service`; it cannot derive it from ordinary PDU delivery.

---

## 6. Model pressure

### 6.1 Friction signals — preferred Arm C happy path

| Signal | None / Mild / Significant | One-sentence justification |
|---|---|---|
| Artificial Wire | None | `W_Service` is a real maintenance/diagnostic scope. |
| Wire proliferation | None | One service Wire suffices; native cyclic/safety channels are not mislabeled as Wires. |
| Artificial hierarchy | None | Active/shadow control remains application state, not a WS role. |
| VCN pressure | None | No CAN11 is used. |
| WireAlias pressure | None | No CAN11 is used. |
| Configuration burden | Mild | Membership and Endpoint bindings scale linearly to 149, while the existing native schedule remains separate. |
| Failure/topology mismatch | Mild | WS intentionally sees one opaque redundant Link and needs native telemetry to expose path failures. |

For Arm A, Configuration burden and Failure/topology mismatch are **Significant**, independent of Arm C's selected minimum.

### 6.2 Tiny Endpoint cost

A maintenance-only drive can use:

- one PID, one `W_Service` binding, and no forwarding;
- generated direct dispatch;
- bounded Identity/Version/Health, fault Snapshot, tuning RPC, and update Endpoints;
- a master-polled native mailbox LLL or ordinary Ethernet interface; and
- no cyclic image, distributed-clock, redundancy, or schedule logic in WS.

This is substantially smaller than a full WS cyclic stack, but whether it is smaller than retaining an existing native diagnostic mailbox is an implementation measurement, not an architectural fact. If even these Endpoints and canonical reconstruction are too costly, WS should terminate at the motion-controller diagnostic gateway; the cost is loss of canonical per-drive service identity.

### 6.3 If one concept could change

The missing concept is not another Wire type. It is an explicit optional contract for **cycle-indexed group completeness and timed application**, backed by a deterministic Link schedule. The experiment prohibits designing it, and Arm C shows that WS can remain useful without it.

### 6.4 Useful distinction

WS exposes that an aggregated feedback image is composition, not transparent forwarding. Once one network/platform domain packs many axis records and authors one PDU, canonical source identity names the aggregator; per-axis source, validity, and freshness are payload semantics.

### 6.5 Explicit scope verdict

| Plane | Verdict | Boundary |
|---|---|---|
| Cyclic motion | **Poor fit** | Keep native process image, schedule, distributed clocks, and ring recovery |
| Safety | **Poor fit / not claimed** | Keep native validated black-channel data and ≤4-ms proof |
| Maintenance/service | **Good fit** | Use `W_Service` for identity, health, diagnostics, configuration, logs, tuning, inventory, and stopped-machine update |

### 6.6 Configuration and changeover

The Organizer configures service identities and one Wire; the native engineering tool configures process-image offsets, schedule, distributed clocks, ring behavior, and safety data. One authoritative product build should cross-check device identity and version between those artifacts, but they remain distinct mechanisms.

At a Config C tool changeover:

1. stop motion and place safety/control in the required native state;
2. discover/verify tool UUID, ParticipantId, Endpoint capabilities, and native record requirements;
3. update `W_Service` membership if the tool set changed;
4. regenerate native process-image offsets and deterministic schedule;
5. validate frame size, traversal, clock, and 125-µs bounds;
6. commit compatible artifacts and only then permit motion restart.

No mid-cycle dynamic Wire or schedule mutation is assumed.

### 6.7 Spec pressure

`CORE §3.6` correctly says a Wire does not guarantee ordering, freshness, deadlines, reliability, redundancy, or duplicate suppression. `CORE §9.5` Snapshot guarantees one coherent local complete value, not simultaneous multi-Participant application or cycle completeness. The archetype therefore reveals a missing optional higher-level capability, not an inconsistency in existing Wire semantics.

### 6.8 Result against falsification criteria

WS is **poor for the cyclic plane**:

- Config C Arm A requires 1.548 Gbit/s of minimum-frame wire capacity on a 1-Gbit/s Link;
- Config B Arm A leaves only 12.6 µs in an optimistic budget and assumes an unsupported 250-ns deterministic PDU path;
- distributed coherent application and feedback completeness are not provided by local Snapshot Endpoints;
- aggregation makes timing feasible by moving axis identity and validity into a native process-image payload; and
- ring recovery/deduplication must remain an opaque native Link behavior.

Restricted WS use is **valuable** if native mailboxes can preserve canonical per-device service identity. Arm C keeps the WS machinery small and makes service loss independent of cyclic/safety correctness. This satisfies the restricted-use criterion without claiming that WS owns the inner loop.

---

## 7. Open questions

1. Can the selected native mailbox channel reconstruct arbitrary canonical ParticipantIds, or must WS terminate at the controller/service gateway?
2. What measured CPU, RAM, and mailbox bandwidth does the maintenance-only Endpoint set add to the smallest drive?
3. What exact native recovery bound is guaranteed for one ring break at 8 kHz, and can any cycle be missed even though none is duplicated?
4. Does the actual controller contain a separately fault-contained network/platform Endpoint Domain, or should Arm B's aggregate source use the motion-controller PID?
5. What cyclic command/feedback record sizes do hot-connect tool modules require? The arithmetic uses an explicit 8/8-byte assumption.
6. Can standard-MTU two-frame Config C images meet the assumed 25-µs traversal bound on the selected hardware?
7. Which native record identifies controller epoch/selection so disagreement and takeover remain auditable?
8. Is service Ethernet physically separate in all products, or must mailbox admission be included in the deterministic schedule validation?

---

## 8. Spec findings

| ID | Finding | Consequence |
|---|---|---|
| `SF-R6-025` | R6 has local Snapshot semantics but no distributed cycle-indexed completeness or timed group-apply semantic. | A cyclic motion image must remain native or move coherence, per-record identity, and validity into an aggregate payload/application component. |

Existing redundancy findings `SF-R6-011` and `SF-R6-012` also apply: exposing both ring paths as ordinary Wires creates loop/duplicate-delivery problems that an opaque native redundant Link avoids.

---

## 9. Diagram

```text
                         ARM C SCOPE

          native cyclic/safety plane (not WS)
   MotionController_A ============================+
          || process image / clocks / ring         ||
          ||                                        ||
       Drive01 -- Drive02 -- ... -- Drive48 -- I/O/Safety
          ||                                        ||
   MotionController_B =============================+
          |
          | bounded native mailbox Link (optional WS carrier)
          |
   -------+---------------- W_Service -------------------------
          |                    |                     |
   Cell Supervisor     HMI/service computer    service gateway

   WS transports identity, health, diagnostics, configuration,
   logs, tuning, inventory, and stopped-machine update.
```

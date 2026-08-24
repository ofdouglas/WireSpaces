# Archetype 16 — Intentional Partial Networking and Selective Sleep

**ID:** 16  
**Convergence test:** **Yes** — assign 3 independent mapping agents  
**Suggested Link mix:** Automotive Ethernet backbone + switched/power-gated CAN-FD branches + LIN sub-buses + local shared memory  
**Stress:** Configured Participants that are deliberately unreachable during normal operation, selective segment power, hardware wake-up, wake filtering, sleeping-target commands, proxy/cached state, stale-versus-asleep Health, wake storms, and bounded quiescent-current budgets  
**Class:** **Adversarial availability/lifecycle archetype**

**Primary question:** Can WireSpaces preserve stable Participant identity, Wire membership, and auditable Service semantics when most of a deployment is intentionally powered down during normal operation and communication itself may have to wake the target's Link path before any canonical PDU can be delivered?

---

## 0. Adversarial intent

Most existing archetypes assume that configured Participants and Links are normally available. Absence is generally treated as disconnection, partition, or failure. A production vehicle behaves differently: while parked, most domains are intentionally asleep, selected transceivers remain in low-power wake-detection modes, and entire bus branches may be unpowered.

This creates distinctions the mapping must not collapse:

```text
configured member
    != currently powered
    != currently reachable
    != failed
    != removed from the deployment

electrically wakeable
    != able to receive a canonical PDU now

cached state
    != current state authored by the sleeping Participant
```

The assignment is designed to reveal whether WireSpaces:

- incorrectly turns ordinary sleep/wake transitions into deployment reconfiguration;
- assumes Wire membership implies current reachability;
- requires sleeping targets to run enough of the WS stack to receive their own wake command;
- encourages gateways to impersonate sleeping Participants when reporting cached state;
- causes broad publications, diagnostics, or Health polling to wake unrelated branches;
- has a bounded place for wake intent, deferred delivery, and explicit availability state;
- can keep forwarding static while physical Link availability changes frequently.

A clean result may be:

> Keep Participant and Wire configuration stable. Treat electrical wake and segment power as Link/platform control, make availability explicit, and send the canonical Service request only after the path is awake.

That is a valid result only if the mapper explains source identity, failure behavior, queueing, timing, and configuration cost. Do not invent a new wake Transport, availability protocol, or dynamic membership mechanism to force a pass.

---

## 1. Falsification criteria — mandatory before mapping

The mapper must commit to measurable criteria before applying WireSpaces.

At minimum use or refine:

```text
I will judge the mapping POOR for intentional partial networking if:
    - ordinary park/run transitions require ParticipantId, Wire membership, or
      forwarding-table rewrites proportional to the number of sleeping devices;
    - delivering one command to a sleeping target requires waking unrelated
      branches or violating the parked-current budget;
    - the only way to wake a target is a canonical PDU that cannot reach the
      target until after it wakes;
    - queued PDUs can be mistaken for successfully delivered commands while the
      destination is asleep;
    - cached/proxy data is presented as if freshly authored by the sleeping
      Participant;
    - intentional sleep is indistinguishable from failure for required Health,
      diagnostics, or application state;
    - normal wake/sleep activity creates unbounded forwarding, retry, or
      configuration churn.

I will judge the mapping VIABLE if:
    - Participant identity and intended Wire membership remain stable across
      sleep, wake, and ordinary power-mode changes;
    - a named existing Link/platform mechanism performs electrical wake;
    - canonical Service traffic resumes with explicit readiness and failure
      semantics after wake;
    - irrelevant publications and diagnostic traffic do not wake sleeping
      branches;
    - availability, cached age, and failure remain distinguishable;
    - all queues, wake waits, retries, and retained state are bounded.
```

The final sketch must report against the mapper's original criteria.

---

## 2. Prohibited escape hatches

| Prohibited move | Why |
|---|---|
| “Sleeping Participants are removed from their Wires” without counting changes | Sleep is normal operation, not recommissioning. Count membership and forwarding churn if this is attempted. |
| “The PDU wakes the ECU” without identifying a powered receiver | The target CPU and ordinary Link stack are initially off. Name the transceiver, wake controller, gateway, or always-on domain that recognizes the wake. |
| “The gateway buffers until wake” without bounds and success semantics | Buffer lifetime, ordering, cancellation, target reboot, and sender-visible acceptance must be explicit. |
| Treating cached data as fresh target-authored state | The cache owner and sample age must remain visible. |
| Treating absence of Health traffic as proof of failure | Deliberate sleep is required behavior. |
| Waking every branch for routine polling | This defeats partial networking. Compute current and wake fan-out. |
| Treating Wire membership as electrical presence | Configured scope and current physical reachability are distinct. |
| Inventing dynamic Wire leases or a new distributed availability protocol | Record the gap instead. |
| Assuming all Link technologies share one wake mechanism | Ethernet, CAN-FD, LIN, and shared memory have different power/wake behavior. |
| Keeping every gateway and router fully awake without charging its current | Always-on infrastructure is permitted only inside the supplied budget. |
| Adding authorization/authentication requirements | This assignment tests availability, power, and communication semantics—not access control. |

---

## 3. Native system

### 3.1 Vehicle configurations

**Config A — compact vehicle**

```text
1 Central Compute assembly
1 Body/Power Supervisor, always-on low-power domain
2 Zone Controllers: Front and Rear
2 switched CAN-FD branches per Zone
4 LIN sub-buses
24 leaf ECUs
```

**Config B — production vehicle**

```text
1 Central Compute assembly
1 Body/Power Supervisor, always-on low-power domain
4 Zone Controllers: FrontLeft, FrontRight, RearLeft, RearRight
8 switched CAN-FD branches
8 LIN sub-buses
48 leaf ECUs
1 Telematics domain
1 Alarm/Access domain
1 Service connector/tool when attached
```

**Config C — long-park stress**

Apply Config B while parked for 21 days:

- Central Compute is off.
- All four Zone application domains are off.
- Only the Body/Power Supervisor, Alarm/Access low-power receiver, Telematics wake receiver, and selected zone wake controllers remain powered.
- CAN-FD branches and LIN schedules are off unless explicitly awakened.
- A remote request, door-handle event, alarm event, charger insertion, or scheduled battery check may wake a subset.

**Config D — degraded wake**

Apply Config B with one of:

- a branch fails to wake;
- a leaf ECU wakes but does not complete boot;
- a Zone wakes and then loses power;
- a wake request arrives while the vehicle is transitioning back to sleep;
- two independent wake causes target overlapping branches;
- a Service tool requests a sleeping ECU during a low-battery inhibit.

---

### 3.2 Physical topology

```text
                        Telematics
                            |
                    Automotive Ethernet
                            |
             +------- Central Compute -------+
             |              |                |
         Zone FL         Zone FR         Body/Power Supervisor
          /   \           /   \          (always-on low-power)
     CAN-FD1 CAN-FD2 CAN-FD3 CAN-FD4           |
       |       |       |       |          wake/power controls
     leaves  leaves   leaves  leaves            |
       |                         |               |
     LIN-A                     LIN-D       Alarm/Access receiver

             +------- Ethernet -------+
             |                        |
         Zone RL                   Zone RR
          /   \                     /   \
     CAN-FD5 CAN-FD6           CAN-FD7 CAN-FD8
       |       |                 |       |
     leaves  leaves            leaves  leaves
       |                           |
     LIN-E                       LIN-H
```

The diagram is logical/physical shorthand. Each Zone Controller contains:

```text
Zone application domain
Zone network/platform domain
low-power wake/power controller
CAN-FD transceivers with selective wake capability
LIN master channels
```

Only model separate Zone Participants where these are real dispatch, fault, or authorship domains. The low-power wake controller may be below the Participant modelling floor if it performs no application communication.

---

## 4. Power and availability states

The system uses these native operational states:

| State | Always-on set | Other domains | Network behavior |
|---|---|---|---|
| `RUN` | all required | powered | all production branches available |
| `ACCESSORY` | supervisor, central subset, infotainment/access zones | selective | only required branches awake |
| `PARKED_ARMED` | supervisor, alarm/access receiver, wake receivers | asleep | wake detection only |
| `PARKED_CHARGE` | supervisor, charging/BMS subset, telematics receiver | selective | charging branches and occasional telemetry |
| `SERVICE` | service-requested set | forced awake under bounded hold | diagnostic access |
| `SLEEP_PENDING` | currently awake set | quiescing | new wake causes may cancel sleep |
| `WAKE_IN_PROGRESS` | wake path + requested set | booting | canonical Service readiness not yet established |
| `DEGRADED` | available subset | failed/inhibited subset | explicit failure or low-battery policy |

These are application/platform states. Do not assume that changing vehicle power state changes Participant identity or configured communication scope.

The mapping must distinguish at least:

```text
Configured
Awake and ready
Wake in progress
Intentionally asleep but wakeable
Intentionally unavailable by power policy
Failed to wake
Failed after wake
Physically removed/replaced
```

The mapper may place these distinctions in Link status, a Service, composition state, or deployment metadata, but must state which layer authors each fact.

---

## 5. Participant model

For Config B, begin with:

| Participant class | Count | Role |
|---|---:|---|
| Central application/compute domain(s) | 2–4 | Vehicle functions, UI, aggregation; justify domain split |
| Body/Power Supervisor | 1 | Power-mode decision, wake coordination, retained availability state |
| Zone application domains | 4 | Local body/comfort application behavior |
| Zone network/platform domains | 4 optional | Physical Link ownership and branch status; include only if separately dispatched |
| Telematics domain | 1 | Remote requests and low-rate parked reporting |
| Alarm/Access domain | 1 | Alarm sensing, key/handle events, local wake cause |
| CAN-FD leaf ECUs | 32 | Lamps, closures, seats, HVAC actuators, pumps, sensing modules |
| LIN leaf devices | 16 | Switches, small actuators, simple sensors |
| Service tool | 1 temporary | Diagnostics/update when physically attached |

Expected Config B total is approximately **57–63 Participants**, depending on justified Central and Zone domain boundaries.

Do not model:

- transceiver wake filters;
- PMIC rails;
- individual relays;
- LIN scheduler slots;
- CPU cores without independent dispatch/fault semantics;
- cached records as Participants.

Participant identity must not change merely because an ECU sleeps, wakes, reboots, or moves between vehicle power modes.

---

## 6. Existing physical wake mechanisms

Use only mechanisms that already exist in the native system:

| Mechanism | Powered element | Action |
|---|---|---|
| Local hardware input | Alarm/Access or Zone wake controller | Door handle, key, impact, charger insertion |
| Low-power CAN selective wake | CAN transceiver + wake controller | Recognizes a bounded configured wake pattern and powers branch/domain |
| CAN bus wake | CAN transceiver | Any qualifying bus activity wakes the branch where selective filtering is unavailable |
| LIN wake pulse | LIN transceiver/master | Wakes a LIN cluster before normal schedule resumes |
| Ethernet wake/power control | Supervisor/Zone platform | Powers endpoint/switch port; ordinary Ethernet forwarding follows readiness |
| Retained timer/RTC | Supervisor or selected ECU | Scheduled battery/charging check |
| Local shared-memory signal | Already-powered domains | Wakes a local process/core, not an unpowered remote ECU |

Do not assume that the low-power receiver can parse a full canonical WS PDU. It may recognize only:

```text
physical wake pattern
branch number
coarse wake class
hardware line
timer event
```

After power-up, the ordinary Participant and Link stack initialize and announce or expose readiness through existing mechanisms. State the readiness criterion used before canonical traffic is attempted.

---

## 7. Required interactions

### 7.1 Awake-state operation

| Interaction | From | To | Rate / deadline |
|---|---|---|---|
| Body command/status | Central or Zone application | selected leaf | 1–50 Hz or event; 10–100 ms |
| Sensor publication | leaf | Zone/Central application | 1–100 Hz |
| Zone summary | Zone application | Central application | 1–20 Hz |
| Health/Link status | Participants/platform domains | diagnostics consumer | 0.2–2 Hz or event |
| Identity/Version | tool or organizer-facing component | selected Participant | startup/service |

The mapping must remain ordinary and unsurprising in `RUN`. The adversarial pressure is the transition to and from partial availability.

### 7.2 Parked wake interactions

| Trigger | Initial powered receiver | Required awakened set | End-to-end target |
|---|---|---|---|
| Door-handle touch | Alarm/Access receiver | Access domain + one relevant Zone + latch leaf | unlock response ≤150 ms |
| Remote cabin precondition | Telematics receiver | Supervisor + HVAC Zone/leaf set | acknowledge within 2 s; function ready ≤10 s |
| Charger insertion | local input/Supervisor | charging/BMS subset | begin negotiation ≤1 s |
| Alarm impact | Alarm receiver | Supervisor + Telematics; selected sensors | event capture ≤100 ms |
| Scheduled battery check | retained timer | Supervisor + one measurement branch | complete and return to sleep ≤5 s |
| Service-tool query | service connector | requested diagnostic path | first response ≤2 s |

The wake cause may identify a branch but not the final canonical Service Endpoint. The mapper must show the transition:

```text
physical wake intent
    -> branch/domain power
    -> Link readiness
    -> Participant readiness
    -> canonical Service request
    -> response or explicit failure
```

### 7.3 Commands arriving while asleep

Evaluate at least:

- unlock request to a sleeping latch controller;
- climate request to a sleeping HVAC controller;
- status query to a sleeping seat ECU;
- firmware update request while the target branch is asleep;
- broadcast diagnostic discovery while seven of eight CAN-FD branches are asleep;
- ordinary periodic publication addressed through a sleeping Zone;
- cancellation of a remote request while wake is in progress.

For each, state whether the sender receives:

```text
local send rejection
accepted wake transaction
accepted canonical PDU
deferred delivery
timeout
explicit asleep/inhibited response
eventual Service response
```

Do not use the word “accepted” without identifying which layer accepted what.

### 7.4 Cached and proxy state

While leaves sleep, the Body/Power Supervisor and Zones retain selected last-known values:

- closure locked/unlocked state;
- last measured cabin temperature;
- last branch voltage;
- last ECU Health summary;
- software inventory captured during prior awake state;
- last sleep-entry reason and timestamp.

The system may answer low-cost parked queries without waking a branch, but the response must reveal:

- who authored the response now;
- who authored the underlying sample;
- sample age;
- whether state is retained, inferred, or freshly measured;
- whether wake was suppressed by policy.

A gateway or supervisor may compose a new cache/proxy response. It must not silently forward old payload as if the sleeping leaf authored it at query time.

---

## 8. Power and wake arithmetic

Use these challenge values unless the mapper justifies alternatives.

### 8.1 Current assumptions

| Item | Current at 12 V |
|---|---:|
| Body/Power Supervisor retained mode | 4 mA |
| Alarm/Access low-power receiver | 2 mA |
| Telematics wake receiver | 6 mA |
| Each Zone low-power wake controller | 0.5 mA |
| Each CAN-FD selective-wake transceiver | 0.08 mA |
| Each sleeping leaf transceiver/device | 0.05 mA |
| Awake Zone platform/domain | 120 mA |
| Powered CAN-FD branch infrastructure | 35 mA |
| Average awake CAN-FD leaf ECU | 25 mA |
| Average awake LIN leaf | 8 mA |
| Central Compute awake | 1.8 A |

### 8.2 Parked budget

The complete vehicle target is:

```text
steady PARKED_ARMED current: <= 25 mA average
21-day no-start interval
12 V, 60 Ah nominal battery
usable parked allocation for this subsystem: 12 Ah
```

Compute:

- baseline parked current from the supplied always-on set;
- 21-day amp-hour consumption;
- remaining budget for periodic wakes;
- impact of keeping one Zone, one CAN-FD branch, or Central Compute continuously awake;
- impact of a wake storm.

### 8.3 Wake-energy model

Use:

```text
Zone boot to ready: 350 ms at 120 mA
CAN-FD branch stabilization: 50 ms at 35 mA
leaf ECU boot: 100–800 ms at 25 mA
minimum useful awake hold after successful wake: 5 s
sleep re-entry overhead: 1 s at the awake current
```

For these daily events, compute average amp-hour cost:

```text
40 door/access wakes, one Zone + one branch + two leaves, 8 s each
8 remote/status wakes, one Zone + one branch + four leaves, 15 s each
4 scheduled checks, one Zone + one branch + one leaf, 5 s each
1 service session, all Zones and leaves, 30 minutes
```

Also evaluate a fault storm of **60 wake attempts in 10 minutes** against one failed leaf.

### 8.4 Broad-Wire wake negative control

Evaluate a naive design where one routine diagnostic or Health query is propagated to every configured branch and any branch activity wakes its transceivers.

Report:

- branches awakened;
- leaf ECUs awakened;
- current and awake-hold energy;
- time until all responses/timeouts settle;
- whether one broad query violates the parked-current or wake-rate policy;
- what pruning, separate scopes, or application composition would avoid the fan-out.

Do not assume destination addressing prevents physical wake if the low-power transceiver wakes on any bus activity.

---

## 9. Availability, failure, and queueing cases

The mapping must explicitly handle:

1. **Normal sleep.** A configured leaf is intentionally unreachable for six hours.
2. **Selective wake success.** One request wakes exactly one Zone branch and completes.
3. **Wake amplification.** A frame on one CAN branch wakes every leaf on that branch.
4. **Failed wake.** The branch powers, but the target never becomes ready.
5. **Late boot.** The target becomes ready after the sender's deadline.
6. **Sleep race.** A request arrives after `SLEEP_PENDING` starts but before power removal.
7. **Duplicate wake causes.** Door handle and remote unlock arrive together.
8. **Queued stale command.** A climate command is superseded while wake is in progress.
9. **Gateway reset.** Retained proxy/cache state is lost or marked invalid.
10. **Low-battery inhibit.** Policy refuses a comfort-function wake but still permits alarm/access wake.
11. **Branch fault.** A CAN branch is electrically failed, not asleep.
12. **Replacement.** A leaf ECU is replaced while retaining the deployment role identity.
13. **Service hold.** A tool keeps selected branches awake, then disconnects unexpectedly.
14. **Wake storm.** A faulty sender repeatedly requests an unavailable target.
15. **Return to sleep.** Queues are drained/cancelled and no accepted command is silently abandoned.

For each case, separate:

```text
physical power state
Link readiness
Participant availability
canonical PDU acceptance
Endpoint delivery
Service completion
reported Health/availability
```

---

## 10. Required mapping arms

Every mapper must compare all three arms.

### Arm A — static Wires, Link/platform-controlled wake

Participant identity, Wire membership, and forwarding remain configured while physical Links become unavailable and available. Electrical wake occurs below or beside WS; canonical traffic begins after readiness.

Report:

- stable Wire and forwarding objects;
- how the sender requests wake;
- which Participant authors availability and wake outcome;
- what happens to a PDU submitted before readiness;
- Link-state and Service-state transitions;
- bounded retry/timeout behavior;
- current and wake fan-out.

### Arm B — dynamic membership/forwarding follows power state

Sleeping Participants or branches are removed from active Wire membership/forwarding and restored on wake.

Report:

- configuration operations per park/run transition;
- configuration operations per selective wake;
- distribution and activation ordering;
- stale configuration windows;
- behavior when the target sleeps before updates converge;
- audit/log volume;
- whether this is deployment reconfiguration disguised as availability.

This arm is intentionally suspect. Evaluate it rather than rejecting it without accounting.

### Arm C — awake proxy/facade for sleeping subtrees

An always-on Supervisor or Zone-facing Participant exposes a bounded facade for sleeping leaves. It answers cached queries and initiates wake/composition for operations requiring the real target.

Report:

- which leaf Participants remain directly addressable;
- which interactions terminate at the proxy;
- canonical source identity of cache responses;
- retained-state size and age;
- wake transaction and later composition;
- behavior when proxy and leaf disagree;
- current cost of the proxy;
- loss of transparency relative to direct Services.

The mapper may choose a hybrid as the preferred minimum, but must first make all three costs visible.

---

## 11. Configuration and churn accounting

For Config A and Config B, count:

- Participants;
- Wires;
- configured memberships;
- forwarding objects;
- Link bindings;
- branch power/wake records;
- availability-state records;
- retained proxy/cache records;
- dynamic Wire/membership changes per ordinary day under each arm;
- physical wake events per ordinary day;
- configuration changes during a `RUN`→`PARKED_ARMED`→selective wake→sleep cycle;
- objects that must survive reset;
- human-authored decisions versus generated per-branch records.

Distinguish:

```text
deployment configuration
runtime power/availability state
Link-control state
Service transaction state
retained asset/cache state
```

Do not count a runtime wake transition as a configuration change unless the mapping actually mutates Wiring.

---

## 12. Expected diagnostic questions

### Question A — membership versus availability

Does configured Wire membership mean only that a Participant may communicate on the scope when reachable, or does current R6 accidentally imply continuous reachability?

### Question B — where wake lives

Is wake:

- Link control;
- platform/power composition;
- a Service to an always-on wake coordinator;
- a Transport behavior;
- deployment reconfiguration;
- outside WS?

Choose based on existing semantics. Do not invent a new answer.

### Question C — addressing a sleeping target

Can an application express “perform this operation on Participant X, waking its path if policy permits” without claiming the canonical PDU has already been accepted for delivery?

### Question D — forwarding while a branch is down

Should a Router:

- reject immediately;
- hold one bounded latest request;
- queue a bounded set;
- trigger wake;
- report Link unavailable;
- leave all wake behavior to a composing Service?

State what current R6 provides and what remains application/platform policy.

### Question E — observation and stale cache

Can an observer retain a PDU received before sleep and later answer a query with it? If it authors a new response, is composition required? How are source and age represented?

### Question F — Health semantics

How can consumers distinguish:

```text
healthy and awake
healthy when last observed, now intentionally asleep
wake prohibited
wake in progress
failed to wake
unreachable due to Link fault
removed/replaced
```

Is this Link Status, Participant Health, operational state, or a combination?

### Question G — broad propagation and wake filtering

Can static destination pruning prevent irrelevant traffic from reaching sleeping branches? If physical wake occurs before full canonical classification, what must the Link profile guarantee?

### Question H — proxy identity

When an always-on Zone answers for a sleeping leaf, is it:

- forwarding a retained target-authored publication;
- observing and replaying historical data;
- composing a fresh cache response;
- implementing a different facade Service?

Do not blur these cases.

### Question I — bounded state

Where are wake requests, deferred commands, cancellation, retries, and response correlation stored? What happens at exhaustion or reset?

### Question J — architectural boundary

Does WireSpaces need a new partial-networking primitive, or only:

- explicit Link availability;
- a platform wake Service/composition pattern;
- destination-pruned forwarding;
- clear send rejection semantics;
- standardized availability/cache metadata?

Prefer the smallest conclusion supported by the mapping.

---

## 13. Valid experiment outcomes

Valid positive, mixed, or negative outcomes include:

```text
"Stable Participants and Wires fit partial networking well; availability is
 runtime Link/platform state and should not mutate deployment Wiring."

"Wake is necessarily below WS because the ordinary target stack is unpowered.
 WS resumes after the native wake path establishes readiness."

"A canonical Service request cannot double as an electrical wake primitive.
 An always-on coordinator must compose wake intent and later issue or enable the
 real request."

"Direct Services remain useful while awake, but parked queries should terminate
 at an explicitly authored cache/facade Service."

"Destination pruning avoids irrelevant forwarding only after enough of the
 ingress classifier is awake; selective transceiver wake remains a Link-profile
 property."

"Current R6 can represent the steady awake and asleep scopes, but lacks a clear
 standard vocabulary for intentional availability, wake-in-progress, and stale
 cached state."

"Dynamic Wire membership is operationally wrong: it turns routine power
 management into high-churn deployment reconfiguration."

"The always-on Router cost exceeds the parked budget, so WS must terminate at a
 lower-power supervisor or use a native wake gateway."
```

A “strong pass” requires power arithmetic, explicit wake sequencing, bounded queues, and correct source identity—not merely a static topology diagram.

---

## 14. What this archetype does not include

- traction, steering, braking, or other hard-real-time vehicle-control loops;
- formal functional-safety analysis;
- battery-management cell monitoring;
- infotainment media streams;
- cloud backend behavior;
- radio/cellular mobility;
- dynamic routing;
- security, authentication, authorization, trust, or anti-theft policy;
- detailed AUTOSAR, CAN partial-networking, LIN, or Ethernet conformance;
- electrical wake-pattern design;
- bootloader implementation;
- a requirement that every sleeping leaf be directly reachable;
- a proposed new WireSpaces wake or availability protocol.

The assignment tests stable logical scope under intentional absence, not access control.

---

## 15. Mandatory output additions

In addition to the normal sketch template, include:

```text
Falsification criteria

Arm A / B / C comparison:
    Participant identity stability
    Wire/membership churn
    forwarding behavior while asleep
    wake initiator and mechanism
    canonical source identity
    queue/retry/cancellation bounds
    cache freshness semantics
    parked current
    wake fan-out and energy
    failure behavior
    configuration burden

RUN -> PARKED_ARMED -> selective wake -> sleep sequence diagram

Config A / B power and configuration accounting

Explicit scope verdict:
    deployment Wiring
    runtime availability
    electrical wake
    deferred Service operation
    cached/proxy state
    Health/Link status
```

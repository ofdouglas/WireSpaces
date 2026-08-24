# Sketch — Archetype 14: Mobile Warehouse Fleet on an Opportunistic Multi-Hop Mesh

```text
**Archetype:** archetypes/14_mobile_warehouse_opportunistic_mesh.md
**Agent:** cursor_B
**Output file:** sketches/14_mobile_warehouse_opportunistic_mesh_cursor_B.md
**Date:** 2026-08-23
```

---

## Executive summary

```text
**Minimum mapping:** Config B — 208 Participants and 4 stable Wires carried over a conventional routed wireless underlay treated as one opaque Link; proximity recipients are repeated directed PDUs selected by application/underlay state.
**Question A (R6):** Poor as the mobile-mesh topology layer; useful for stable identity and Services above it.
**Question B (CAN11):** N/A.
**Worst friction (minimum path):** Failure/topology mismatch — Significant.
**Main lesson:** Static Wires either hide route/proximity semantics behind an existing routed mesh or reproduce rapidly changing route/group state; WS should stop above the mobile underlay and should not claim to model geographic neighborhoods.
```

---

## Disposition block

| Area | Assessment |
|---|---|
| Participant identity | Natural |
| Wire decomposition | poor |
| Forwarding | awkward |
| Non-CAN configuration | high |
| CAN11 VCN fit | N/A |
| Better with CAN29? | N/A |

**Explanation:** Question A has a split result. Deployment-global ParticipantIds remain natural while robots roam and routes change, and four stable Service scopes can be useful over a conventional routed substrate. The Logical Bus/forwarding model is poor for the facility mobile mesh itself: natural audiences are geographic and temporary, next hops change 2–10 times per second, partitions merge, and duplicate paths exist; static Wires either flood too broadly or require continuous unauditable reconfiguration. Question B is N/A because no CAN11 Link is used.

---

## 0. Falsification criteria (stated before mapping)

I will judge static Logical Wires **poor** for the mobile domain if any of these occur:

1. Normal motion requires Wire membership or WS forwarding updates at more than **1 facility-wide operation/s sustained**, or faster than a reviewed configuration can be distributed with a bounded stale window.
2. A broad AMR coordination Wire consumes more than **10% of an assumed 10 Mbit/s usable shared wireless channel** at ordinary pose traffic, before retries and contention.
3. More than **80%** of broad-published pose traffic reaches AMRs outside the required ~10 m audience.
4. Preserving local collision behavior during infrastructure loss requires a central broker or coordinator not required by the native system.
5. Partition/merge requires distributed Wire-state reconciliation or duplicate suppression that R6 does not define.

I will judge the model **viable** for the mobile domain only if:

1. A small stable set of Wires survives route and neighborhood churn with **zero normal-motion WS configuration changes**.
2. The wireless substrate can honestly provide unicast routing, retries, duplicate suppression, and partition-local delivery as Link behavior without claiming that WS models those mechanisms.
3. Proximity and temporary-group interactions need neither Wire-per-group nor an artificial application re-authoring broker.

The final assessment in §6 applies these criteria.

---

## 1. Native communication model

Config B contains 60 autonomous mobile robots (AMRs), 120 sleeping smart pallets, 12 charging docks, 12 fixed radio gateways/access points, two fleet schedulers, and two traffic/safety coordinators.

AMRs exchange pose/velocity envelopes and planned paths with whichever robots are physically nearby or approaching now. Immediate avoidance and zone emergency events must continue inside a 30-second to 10-minute infrastructure partition. These recipient sets change as robots move; they are not stable fleet-wide groups.

Schedulers assign work and receive progress but are not required for immediate collision avoidance. Traffic coordinators publish congestion and facility pause state, while local AMRs retain immediate safety behavior. AMRs form short-lived pair/group relationships with pallets and docks. Across the facility, 5–20 such groups start or end each second.

The wireless substrate combines roaming infrastructure Wi-Fi, direct AMR peer Wi-Fi, and a low-power sleeping-node mesh. It already performs route selection, retries, and duplicate suppression. An AMR may be reachable simultaneously through infrastructure and peers; next hops can change 2–10 times/s while semantic relationships remain unchanged.

When a partition rejoins, assignments, telemetry, and events may be stale or duplicated. The application must reconcile task state and freshness; changing network reachability does not change participant identity.

---

## 2. Obvious conventional implementation

> **Obvious conventional implementation:** Stable device identities and application messages over routed IP/Wi-Fi plus an 802.15.4-class mesh; radio-layer neighbor discovery and routing; local one-hop safety broadcasts or repeated unicast; scheduler APIs over infrastructure; application session state for pallet/dock handoff; sequence/freshness metadata and task reconciliation after partition.

> **What structure does a conventional design use?** Dynamic route/neighbor tables, radio multicast or local-broadcast groups, duplicate caches, sleeping-node queues, application task/session state, and stable infrastructure service addresses.

WireSpaces can add consistent Participant and Endpoint identity plus standard Identity/Health/update Services. It does not replace the dynamic route, proximity-group, duplicate-suppression, sleeping-node, or merge-reconciliation machinery.

---

## 3. Minimum mapping — Mapping A: opaque routed wireless Link

This is the least-bad valid mapping. The existing routed Ethernet/Wi-Fi/mesh network is exposed to WS as one **opaque routed Physical Link**. Its routing is conventional underlay behavior, not WS forwarding.

### 3.1 Participants

| ParticipantId range | Endpoint Domain | Count A / B | Notes |
|---|---|---:|---|
| 0x01–0x3C | AMR application | 12 / 60 | Stable across motion and route changes |
| 0x40–0xB7 | Smart pallet/tag | 24 / 120 | Sleeps 90–99%; stable identity |
| 0xB8–0xC3 | Charging dock | 4 / 12 | Stable fixed infrastructure |
| 0xC4–0xCF | Fixed radio gateway | 3 / 12 | Underlay reachability; no application authority |
| 0xD0–0xD1 | Fleet scheduler | 1 / 2 | Application redundancy in B |
| 0xD2–0xD3 | Traffic/safety coordinator | 1 / 2 | Local avoidance does not depend on these |

**Config A:** 45 Participants. **Config B/C:** 208 Participants. Ordinary 8-bit ParticipantIds suffice. Hardware UUID remains separate from deployment role identity for replacement history.

### 3.2 Wires

| Wire (#) | Stable members (Config B) | Physical link | Purpose |
|---|---|---|---|
| W1 FleetOps | 60 AMRs, 2 schedulers, 2 coordinators | Opaque routed facility network | Directed work, progress, congestion, pause/resume |
| W2 LocalMotion | 60 AMRs, 2 coordinators | Opaque routed facility network | Directed proximity intent/avoidance; coordinator zone events |
| W3 AssetHandoff | 60 AMRs, 120 pallets, 12 docks | Opaque routed facility network | Temporary pair/session traffic without changing Wire membership |
| W4 PlatformServices | all 208 Participants | Opaque routed facility network | Identity, Version, Health, diagnostics, parked/batched updates |

Wire membership is stable. Temporary pairing is application session state on W3, not Wire construction. Proximity is **not** represented by W2 membership: the application/underlay supplies current neighbor ParticipantIds and the sender emits repeated directed PDUs.

This mapping passes the “zero normal-motion WS configuration changes” test only because it delegates all mobile routing and neighbor discovery to the underlay.

### 3.3 Interactions

| Interaction | Src | Dest | Wire | Broadcast? | Notes |
|---|---|---|---|---|---|
| Pose/velocity envelope | AMR | each currently relevant AMR | W2 | No | Repeated directed PDUs; 10–20 Hz |
| Planned path | AMR | approaching/intersecting AMRs | W2 | No | Audience supplied outside WS |
| Immediate avoidance | detecting AMR | known affected peers | W2 | No | Partition-local underlay route required |
| Zone emergency | coordinator/AMR | current affected-zone AMRs | W2 | No | Repeated directed; stale audience is a hazard to application |
| Work assignment | scheduler | selected AMR | W1 | No | |
| Progress/state | AMR | scheduler | W1 | No | Buffers or fails during partition |
| Congestion/map update | coordinator/scheduler | selected AMRs | W1 | No | No geographic destination primitive |
| Global pause | coordinator | each reachable AMR | W1 | No | Repeated directed; facility broadcast rejected |
| Pallet pair/handoff | AMR ↔ pallet/station | selected pair | W3 | No | Session lifetime seconds–minutes |
| Dock interlock | AMR ↔ dock | selected pair | W3 | No | |
| Identity/Health/query | maintenance | participant | W4 | No | Sleeping pallet may respond later |
| Update | maintenance | parked AMR / pallet batch | W4 | No | Scheduled below local motion traffic |

**Unsupported directly by R6:** “near me now,” “entering Zone 7,” and “affected zone now.” They must become explicit directed destinations selected by application/underlay state. That preserves canonical identity but means WS is not modeling the natural audience.

### 3.4 Forwarding

| Ingress | Wire | Egress | Notes |
|---|---|---|---|
| Opaque facility Link | W1–W4 | Opaque facility Link | No WS per-hop forwarding; routed underlay resolves unicast next hop |

**WS forwarding objects at fixed gateways:** 0 for the mobile mesh; gateways are underlay routers. A WS implementation may have one binding per Wire at each endpoint, but no per-hop WS route table.

**Radio-underlay objects:** dynamic neighbor/route entries are owned by Wi-Fi/mesh firmware. At Config B scale, at least 60 AMR route sets, 120 sleeping-leaf reachability records, 12 gateways, duplicate caches, and transient neighbor edges exist; exact count is radio implementation state and cannot honestly be counted as WS configuration.

### 3.5 Link profiles

| Physical link | Profile | WS Wire(s) | Notes |
|---|---|---|---|
| Routed facility substrate | WS datagram/reliable segment over conventional routed IP/mesh | W1–W4 | Underlay owns routing, retries, duplicate suppression, roaming, sleep queues |
| Facility Ethernet | Part of routed substrate | W1, W4 | Stable infrastructure |
| Infrastructure Wi-Fi | Part of routed substrate | W1–W4 | AP roaming |
| Direct peer Wi-Fi | Part of routed substrate | W2/W3 unicast paths | Path appears/disappears |
| Low-power mesh | Part of routed substrate | W3/W4 selected traffic | Low MTU, sleeping nodes |

The “one opaque Link” abstraction hides per-hop topology, route selection, and duplicate suppression. It is acceptable only with that limitation stated: WS is payload/identity above an existing mobile network.

### 3.6 CAN11 bindings

Not applicable.

### 3.7 Broad-Wire negative-control airtime

Assumptions:

```text
pose payload                         32 bytes
canonical WS header                   6 bytes
transport/freshness/sequence          4 bytes
radio/IP/MAC framing estimate        34 bytes
framed transmission                  76 bytes = 608 bits
usable shared wireless capacity      10 Mbit/s
```

For 60 AMRs × 20 Hz:

| Average transmissions per source packet | Source packets/s | Radio transmissions/s | Payload rate | Framed rate | 10 Mbit/s use |
|---:|---:|---:|---:|---:|---:|
| 1 | 1,200 | 1,200 | 307.2 kbit/s | 729.6 kbit/s | 7.3% |
| 2 | 1,200 | 2,400 | 307.2 kbit/s | 1.459 Mbit/s | 14.6% |
| 4 | 1,200 | 4,800 | 307.2 kbit/s | 2.918 Mbit/s | 29.2% |

This excludes retries, acknowledgments, contention, path updates, other application traffic, and PHY inefficiency. At replication factor 2 or 4 it fails the predeclared 10% criterion.

If an AMR has 6 relevant neighbors within ~10 m but W2 reaches all other 59 AMRs, **53/59 ≈ 90%** of recipients are outside the intended audience. It fails the 80% criterion.

On a 250 kbit/s low-power channel, even the payload alone is **123%** of nominal capacity; framed replication is ~292%, ~584%, or ~1,167%. Including sleeping pallets in the broad scope would force wakeups or queues for irrelevant traffic, defeating 90–99% sleep duty.

Therefore W2 **cannot** use generic bus flood-and-filter. Destination-pruned unicast routing is mandatory, not an optional optimization.

### 3.8 Partition, merge, and duplicate behavior

| Case | Underlay responsibility | WS/Transport responsibility | Application responsibility |
|---|---|---|---|
| Route churn | Select new next hop; retain PID-address reachability | Canonical identity unchanged | None if delivery continues |
| Semantic neighborhood churn | Report neighbor/position state | Carry directed PDU | Select current recipients |
| Asymmetric visibility | Bidirectional route validation/retry | Delivery/failure indication | Avoid assuming “heard” means reachable |
| Duplicate paths | Duplicate suppression below WS where possible | End-to-end sequence/freshness detects residual duplicate | Idempotent event handling |
| Partition | Maintain local routes | Local directed delivery; remote failure/queue bounds | Continue safe local behavior |
| Merge | Rebuild routes; suppress duplicate packets | Preserve PID; freshness/sequence | Reconcile assignments, buffered telemetry, events |
| Sleeping pallet | Queue/poll according to mesh contract | Bounded timeout/retry | Session tolerates delayed response |
| Gateway failure | Reroute to another gateway | No identity change | Observe link degradation |

R6 defines none of the distributed merge/reconciliation behavior. This mapping is viable only because those mechanisms remain underlay/application concerns.

### 3.9 Configuration inventory

| Item | Config A | Config B/C |
|---|---:|---:|
| Participants | 45 | 208 |
| WireNumbers | 4 | 4 |
| Stable WS memberships | 4 sets | 4 sets |
| Dynamic WS memberships | 0 | 0 |
| WS gateway forwarding objects | 0 | 0 |
| Normal-motion WS config changes | 0/s | 0/s |
| Temporary application groups | pilot-dependent | 5–20 created/retired per second (18k–72k/hour) |
| Underlay route/group objects | dynamic, outside WS | dynamic, outside WS |
| Config C partition/merge WS changes | — | 0; underlay/application state changes |

---

## 4. Alternative mapping — Mapping B: expose physical hops/zones to Wires

This mapping tests whether WS can model the mobile mesh rather than riding above it. It is rejected.

### 4.1 Candidate decomposition

- 12 fixed zone Wires, one per gateway/aisle coverage area.
- One temporary pair Wire for each currently relevant AMR↔AMR direct relation.
- One temporary handoff Wire per AMR↔pallet or AMR↔dock session.
- Per-hop Link Interfaces and forwarding entries for AMR relay paths.

At an assumed average of 10 currently heard AMR peers, the undirected physical graph can contain roughly:

```text
60 AMRs × 10 peers / 2 = 300 temporary AMR pair edges/Wires
up to 120 pallet reachability/session edges
up to 12 dock session edges
12 fixed zone Wires
--------------------------------
up to ~444 contemporaneous Wire-like scopes
```

Not every heard peer is semantically relevant, but filtering this set requires exactly the geographic/session state the mapping is attempting to encode.

### 4.2 Churn accounting

Assumptions stated for diagnosis:

- Conservative semantic-neighbor churn: **6 joins/leaves per AMR/minute**.
- Radio next-hop changes: archetype’s **2–10/s per moving AMR**.
- Temporary pallet/dock groups: **5–20/s facility-wide**.
- Optimistic Organizer generation/distribution/activation latency: **250 ms**.

Results for Config B:

```text
semantic membership operations:
    60 × 6 / 60 = 6 AMR membership changes/s
    plus 5–20 temporary group changes/s
    => 11–26 semantic changes/s

per-hop forwarding operations:
    60 × (2–10) = 120–600 next-hop changes/s

stale window at 250 ms:
    ~3–7 semantic changes occur while one update is distributed
    ~30–150 route changes occur during that same update
```

Symmetric bindings and retirement acknowledgments would multiply configuration writes. Two partitions can independently update the same zone/group state; R6 provides no merge rule. This exceeds the stated 1 operation/s falsification threshold by one to three orders of magnitude.

### 4.3 Broad-publish airtime

The fixed-zone/physical-hop mapping does not eliminate replication. A pose packet traversing an average 2 or 4 hops still costs **14.6% or 29.2%** of the assumed 10 Mbit/s channel if all 60 AMRs publish into broad scopes. Narrow pair Wires avoid remote delivery but multiply source transmissions by neighbor count and create hundreds of dynamic bindings.

### 4.4 Partition/merge and duplicate paths

- Partitioned zones continue editing local membership with no authority to reconcile on merge.
- The same canonical PDU can traverse infrastructure and peer Wires as **two copies** or as two PDUs on different Wires.
- Preventing loops across changing multi-hop paths requires route convergence, TTL, sequence caches, or dynamic tree construction — prohibited inventions for this trial.
- Retiring stale pair Wires safely is slower than the physical relationship lifetime in the worst case.

### 4.5 Mapping B disposition

| Measure | Result |
|---|---|
| Wire count | 12 stable zone + potentially hundreds of transient pair/session Wires |
| Membership stability | poor; 11–26 semantic changes/s |
| Forwarding churn | 120–600 next-hop changes/s |
| Auditability | poor; reviewed static export stale almost immediately |
| Partition/merge | undefined distributed state reconciliation |
| Verdict | **Invalid as a natural static/read-mostly R6 deployment pattern** |

Mapping B merely renames mesh routes and multicast groups as Wires. It fails the falsification criteria.

---

## 5. Optional optimizations

### 5.1 One facility-wide Wire with broadcast pose

Rejected by arithmetic: 14.6–29.2% channel use at ordinary 2–4-hop replication before retries, ~90% irrelevant AMR recipients, and catastrophic low-power-mesh cost.

### 5.2 One Wire per proximity group

Rejected: 11–26 semantic configuration changes/s, 250 ms stale windows, and undefined partition merge. This is dynamic geographic multicast state under another name.

### 5.3 Central broker for local collision

Rejected: changes failure semantics and breaks required partition-local avoidance.

### 5.4 Fixed zone Wires only

Useful for low-rate congestion policy if zone membership is application selection, but insufficient for robots crossing boundaries and direct peer coordination. Dynamically changing canonical membership remains the same problem.

### 5.5 WS only on stable infrastructure

The cleanest strict boundary: use WS among schedulers, coordinators, fixed gateways, and docks over Ethernet; carry mobile robot/pallet protocols over conventional routed mesh and adapt at fixed gateway/application Services. This reduces WS scope but introduces visible composition at the boundary. It is preferable if the opaque-Link mapping’s claim of one WS deployment over the routed mesh is operationally misleading.

---

## 6. Friction signals and falsification result

| Signal | None / Mild / Significant | One-sentence justification |
|---|---|---|
| Artificial Wire | Significant | W2/W3 memberships are broad stable supersets because natural geographic/session scopes cannot be represented statically. |
| Wire proliferation | Significant | Exposing topology creates ~12 zone plus potentially hundreds of transient pair/session Wires. |
| Artificial hierarchy | None | No coordinator is invented for local safety; scheduler and traffic roles are native application roles. |
| VCN pressure | N/A | No CAN11. |
| WireAlias pressure | N/A | No CAN11. |
| Configuration burden | Significant | Mapping A hides dynamic route state; Mapping B requires 11–26 semantic and 120–600 forwarding changes/s. |
| Failure/topology mismatch | Significant | R6 has no distributed partition/merge, geographic audience, or duplicate-path convergence semantics. |

### Falsification outcome

| Criterion | Result |
|---|---|
| Stable Wires with zero mobility-driven WS updates | Mapping A passes only by delegating topology to underlay |
| Broad-Wire airtime under 10% | **Fail** at average replication ≥2 |
| ≤80% irrelevant pose recipients | **Fail** (~90%) |
| No artificial central broker | Pass in chosen mapping |
| Defined partition/merge reconciliation | **Fail** within R6; delegated to application/underlay |
| Proximity without Wire-per-group or external recipient selection | **Fail** |

**Verdict:** Static Logical Wires are **poor as the facility mobile-mesh/networking model**. R6 remains useful above the mesh for stable Participant identity, directed Service messages, and stable infrastructure. Mapping A is operationally viable only as **WS payload over a conventional routed network**, not evidence that WS models the opportunistic mesh.

---

## 7. Model pressure

- **If you could change one WS concept** to simplify this system, what would it be? No change within this experiment: dynamic geographic groups, route convergence, and partition reconciliation would turn WS into a different networking system. Keep these below/above WS and document the boundary.
- **Did WS expose a useful distinction** the conventional model obscures? Stable canonical identity survives route changes, partitions, and hardware replacement; electrical/radio visibility remains distinct from membership. It also makes explicit that proximity audience selection is missing rather than silently equating “heard frame” with authority.

### Scope verdict

```text
WS begins:
    at stable Endpoint/Service identity on AMRs, pallets, docks, and infrastructure
    when carried over an already-routed unicast substrate

WS ends:
    before neighbor discovery, geographic/proximity grouping, route selection,
    duplicate suppression, sleeping-node delivery, and partition reconciliation

Preferred deployment boundary:
    WS over conventional routed wireless for directed Services, or
    WS only on stable infrastructure with explicit composition gateways

Not recommended:
    facility-wide WS Logical Wires as the mobile mesh topology/control plane
```

---

## 8. Open questions

- Whether the existing underlay can provide stable unicast reachability by canonical Participant binding without exposing IP/mesh address churn to WS.
- Whether local emergency events require a non-WS one-hop safety broadcast alongside directed WS reporting.
- End-to-end sequence/freshness format for residual duplicates and buffered merge traffic.
- Queue/time-limit policy for sleeping pallets; unreachable is not equivalent to non-membership.
- Config D cross-site loan: independently assigned PID/Wire universes cannot be transparently forwarded together. Valid choices are explicit recommissioning at Site South while retaining hardware UUID/history, or a composition/translation boundary; plain forwarding is invalid.

---

## 9. Spec findings

| ID | Finding |
|---|---|
| SF-R6-015 | Geographic/proximity audiences changing with normal motion are not natural static Wire memberships; broad flood-and-filter is operationally poor on multi-hop wireless. |
| SF-R6-016 | Treating an opportunistic mesh as one opaque routed Link is viable only if routing, duplicate suppression, sleeping-node delivery, and partition/merge remain explicit underlay/application responsibilities; WS contributes identity/Services, not mesh topology. |
| SF-R6-017 | Exposing per-hop mesh topology to R6 requires 120–600 forwarding changes/s in Config B and distributed merge semantics absent from static/read-mostly Wires. |

---

## 10. Diagrams

### Chosen boundary

```text
                Stable WS Participants / Services
    Scheduler  Coordinator  AMRs  Pallets  Docks  Gateways
         \         |         |      |       |       /
          +-------- W1/W2/W3/W4 (stable scopes) ---+
                              |
                   WS datagrams / segments
                              |
         +--------- conventional routed underlay ----------+
         | Ethernet | Wi-Fi AP | direct peer | low-power mesh |
         | routes, retries, duplicates, sleep, partition/merge |
         +--------------------------------------------------+

WS does not model “near me now” or per-hop route topology.
```

### Mapping comparison

| Property | A — opaque routed Link | B — exposed hops/zones |
|---|---|---|
| WS Wires | 4 stable | 12 zones + potentially hundreds transient |
| WS config churn | 0/s | 11–26 semantic; 120–600 forwarding/s |
| Route visibility | hidden/delegated | exposed but immediately stale |
| Proximity audience | application-selected directed PIDs | transient Wire membership |
| Partition/merge | underlay + application | undefined WS state merge |
| Duplicate suppression | underlay/Transport | must be invented across Wires |
| Verdict | viable payload boundary; topology fit poor | poor / rejected |

---

## Devices (reference)

| Class | Count A / B | WS role |
|---|---:|---|
| AMR application | 12 / 60 | W1–W4; local recipient selection external to Wire membership |
| Smart pallet | 24 / 120 | W3/W4; sleeping Participant |
| Charging dock | 4 / 12 | W3/W4; stable infrastructure |
| Fixed radio gateway | 3 / 12 | W4 Services; underlay router, no app authority |
| Fleet scheduler | 1 / 2 | W1/W4 |
| Traffic/safety coordinator | 1 / 2 | W1/W2/W4 |

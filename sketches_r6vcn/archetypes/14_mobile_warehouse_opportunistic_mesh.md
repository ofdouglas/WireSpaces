# Archetype 14 — Mobile Warehouse Fleet on an Opportunistic Multi-Hop Mesh

**ID:** 14  
**Convergence test:** **Yes** — assign 3 independent mapping agents  
**Suggested Link mix:** Wi-Fi infrastructure + direct Wi-Fi peer links + low-power 802.15.4-class mesh + Ethernet at fixed stations  
**Stress:** Moving Participants, continuously changing next hops, geographic/proximity communication, temporary task groups, partitions that later merge, and scarce wireless airtime  
**Class:** **Adversarial boundary archetype**

**Primary question:** Can static/read-mostly loop-free Logical Buses remain useful when the natural communication structure is neither a stable bus nor a stable tree, but a mobile radio graph whose neighborhoods, routes, and temporary groups change every few seconds?

---

## 0. Adversarial intent

The existing corpus mostly assumes that:

- physical topology changes only during commissioning or failure;
- communication scopes remain stable during normal operation;
- gateways and Link Interfaces have known placement;
- group membership changes slowly enough to export reviewed configuration;
- centrally engineered static forwarding is a reasonable default.

This archetype removes those assumptions while staying inside a common embedded-networking domain: autonomous mobile robots, smart pallets, chargers, and fixed stations in a warehouse with imperfect radio coverage.

This assignment is allowed to conclude:

> WireSpaces is appropriate inside each robot and at stable fixed infrastructure, but is a poor fit as the facility-wide mobile mesh/networking layer.

That is a successful result if supported by concrete topology, airtime, and reconfiguration analysis.

Do not invent dynamic routing, geographic multicast, leases, group addressing, or a distributed membership protocol to rescue the mapping.

---

## 1. Falsification criteria — mandatory before mapping

The mapper must state measurable criteria before applying WireSpaces. At minimum address:

```text
I will judge static Logical Wires POOR for this domain if:
    - normal motion requires Wire membership or forwarding-table changes
      faster than they can be safely generated, distributed, and audited;
    - one broad facility Wire causes unacceptable radio forwarding/energy cost;
    - preserving local peer behavior requires a central broker that the native
      system does not require;
    - partitions/merges create duplicate, stale, or conflicting scope state
      that current R6 does not define how to reconcile.

I will judge the model VIABLE if:
    - a small, stable set of Wires survives mobility without topology churn;
    - the wireless underlay can honestly be treated as a bounded Physical Link
      without hiding the behavior being tested;
    - proximity and temporary-group interactions do not require Wire-per-group
      or application re-authoring through an artificial coordinator.
```

The final sketch must report against its original criteria.

---

## 2. Prohibited escape hatches

| Prohibited move | Why |
|---|---|
| “The mesh is just one Physical Link” without airtime and failure analysis | This may be valid, but it hides routing, duplicate suppression, partition behavior, and broadcast cost unless those semantics are stated explicitly. |
| One Wire per current radio route | Routes change several times per second; this simply renames dynamic routing state. |
| One Wire per proximity neighborhood or task group | Membership churn is the stress being measured. Count update rate and stale-state consequences if attempted. |
| A facility-wide broadcast Wire with “bandwidth is enough” | Wireless forwarding and sleeping-node energy require arithmetic. |
| Introducing a central message broker for collision avoidance | Local peer coordination must survive scheduler and infrastructure loss. A broker changes the natural failure model. |
| Treating every physically receivable frame as authorized membership | Electrical/radio visibility is not Wire membership. |
| Adding dynamic routing, geographic addressing, or group multicast to R6 | Record the missing capability instead. |
| Ignoring partitions because TCP/IP or the radio stack reconnects later | Partitioned autonomous operation and later merge are required behavior. |
| Solving identity by renumbering every time a robot crosses a zone | Participant identity must remain stable during normal mobility. |

---

## 3. Native system

### 3.1 Configurations

**Config A — instrumented pilot zone**

```text
12 AMRs
24 smart pallets
4 charging docks
3 fixed radio gateways
1 fleet scheduler
1 safety/traffic coordinator
```

One warehouse hall with good infrastructure coverage. Direct robot-to-robot radio is still used for close-range collision coordination.

**Config B — production warehouse**

```text
60 AMRs
120 smart pallets
12 charging docks
12 fixed radio gateways / access points
2 scheduler instances (application redundancy)
2 traffic/safety coordinators
```

Coverage contains metal-rack shadow zones. AMRs and pallets form temporary multi-hop paths around blocked infrastructure.

**Config C — infrastructure outage / partition**

Applied to Config B:

- one aisle group of 8–15 AMRs loses all fixed-gateway connectivity for 30 seconds to 10 minutes;
- robots still exchange local collision, intent, and emergency-stop information;
- some smart pallets remain reachable only through moving AMRs;
- the partition later rejoins the facility network;
- no robot changes canonical identity during partition or merge.

**Config D — cross-site loan fleet**

Six AMRs are moved for one week from Site North to independently commissioned Site South. Both sites already use overlapping Participant and Wire number allocations. The loaned robots must retain hardware history and return without losing logs.

Config D is secondary. Do not let its identity-universe problem obscure the primary mobility/topology test.

### 3.2 Physical topology

```text
                  Fleet Scheduler / Traffic Coordinator
                                |
                         Facility Ethernet
                    +-----------+-----------+
                    |           |           |
                Gateway 1   Gateway 2   Gateway N
                    :           :           :
                    :   infrastructure Wi-Fi:
                    :           :           :
                 AMR-01  ...... AMR-60
                   : \          /  :
                   :  \ direct /   :     moving peer graph
                   :   \ Wi-Fi/    :
              Pallet tags -- AMRs -- Pallet tags
                   \_____ low-power mesh _____/

Fixed chargers connect by Ethernet or infrastructure Wi-Fi.
```

The radio graph is not stable:

- an AMR may hear 3–20 peers;
- next-hop selection may change 2–10 times per second while moving;
- metal racks cause asymmetric and intermittent links;
- a unicast path may use infrastructure now and two AMR relays five seconds later;
- low-power pallet tags sleep for 90–99% of the time;
- the same Participant may be reachable over Wi-Fi and low-power mesh simultaneously;
- radio firmware performs retries, duplicate suppression, and route selection below the application API.

The mapper must decide whether this routed wireless substrate is:

1. one opaque Physical Link;
2. several zone Links;
3. explicit per-hop Links;
4. outside WireSpaces, with WS carried over a conventional routed network.

State what behavior each choice hides or exposes.

---

## 4. Participant model

Model each as one Participant unless the mapper gives a concrete dispatch/fault boundary:

| Participant class | Config B count | Role |
|---|---:|---|
| AMR application domain | 60 | Navigation intent, pose, local obstacle/collision coordination, task state, Health |
| Smart pallet/tag | 120 | Identity, load state, location beacon, battery Health, pairing/handoff state |
| Charging dock | 12 | Dock availability, charging state, local interlock, AMR handoff |
| Fixed radio gateway | 12 | Wireless↔Ethernet reachability; no application command authority |
| Fleet scheduler | 2 | Work assignment and global optimization; not required for immediate collision avoidance |
| Traffic/safety coordinator | 2 | Zone policy and facility emergency inputs; local AMRs still perform immediate avoidance |

**Config B total:** 208 Participants.

Robot motor drives, safety PLC internals, lidar sample streams, and battery-cell monitors are below the modelling floor. Do not inflate the count with components that do not participate in facility messaging.

---

## 5. Existing physical links

| Link class | Attachments | Behavior |
|---|---|---|
| Facility Ethernet | schedulers, coordinators, fixed gateways, chargers | Stable switched network |
| Infrastructure Wi-Fi | AMRs, gateways, some chargers | Roaming; AP handoff; variable loss/latency |
| Direct AMR Wi-Fi peer | nearby AMRs | Appears/disappears with proximity |
| Low-power mesh | pallet tags, AMRs acting as relays, some gateways | Multi-hop, sleeping leaves, low MTU, scarce energy |
| AMR internal network | outside assignment | Do not map |

Do not add permanent physical links between AMRs merely because they communicate temporarily.

---

## 6. Required interactions

### 6.1 Local collision and motion intent

| Interaction | From | To | Rate / deadline | Notes |
|---|---|---|---|---|
| Pose/velocity envelope | each moving AMR | AMRs currently within ~10 m | 10–20 Hz; ≤50 ms useful age | Geographic/proximity audience changes continuously |
| Planned path segment | each AMR | approaching/intersecting AMRs | 2–10 Hz | Recipient set depends on future path |
| Immediate avoidance event | detecting AMR | nearby affected AMRs | event; ≤20 ms preferred | Must work during infrastructure partition |
| Local emergency stop | AMR or fixed safety input | all AMRs currently in affected zone | event; ≤100 ms | Zone membership changes with motion |

These are not facility-wide broadcasts. Sending every AMR's 20 Hz pose to all 60 AMRs is explicitly unacceptable.

### 6.2 Fleet coordination

| Interaction | From | To | Rate / trigger |
|---|---|---|---|
| Work assignment | scheduler | selected AMR | event |
| AMR progress/state | AMR | scheduler | 0.5–2 Hz |
| Congestion summary | traffic coordinator | AMRs entering a zone | 1–5 Hz |
| Map/route update | scheduler/coordinator | selected fleet subset | on demand |
| Global pause/resume | coordinator | all reachable AMRs | rare event |

Loss of scheduler stops new assignments but does not immediately stop safely executing robots.

### 6.3 Temporary pallet and dock groups

```text
AMR approaches pallet:
    AMR + pallet form a temporary handoff relationship

AMR carries pallet:
    pallet status follows the carrying AMR's reachable path

AMR approaches charger:
    AMR + dock exchange interlock and charging state

handoff completes:
    temporary relationship ends
```

| Interaction | From | To | Typical lifetime |
|---|---|---|---|
| Pallet identify/pair | AMR ↔ pallet | selected pair | seconds |
| Load/handoff state | pallet ↔ carrying AMR / station | temporary group | minutes |
| Dock reservation/interlock | AMR ↔ charger | selected pair | seconds–minutes |

At production throughput, expect 5–20 temporary group changes per second across the facility.

### 6.4 Diagnostics and update

- Identity / Version / Health for all Participants.
- Selected logs/events from AMRs and gateways.
- AMR software/configuration update while parked.
- Pallet-tag firmware update in bounded batches.
- Link quality and route diagnostics from gateways/radio stacks.

Do not allow bulk update traffic to delay local collision or emergency traffic.

---

## 7. Partition, mobility, and lifecycle cases

The mapping must explicitly handle:

1. **Route churn without semantic change.** AMR-17 keeps the same task and peers while its radio path changes from AP3 to AMR-22→AP5.
2. **Semantic neighborhood churn.** AMR-17 moves into range of AMR-31 and out of range of AMR-08.
3. **Asymmetric visibility.** AMR-17 hears AMR-31, but AMR-31 cannot directly hear AMR-17.
4. **Duplicate physical paths.** The same PDU can arrive through infrastructure and peer mesh.
5. **Partition.** Twelve AMRs coordinate locally with no scheduler path.
6. **Merge.** Partitioned robots reconnect with stale assignments, buffered telemetry, and possibly duplicated events.
7. **Sleeping pallet.** A directed request arrives while the pallet radio is asleep.
8. **Gateway failure.** Routes move to other gateways without changing application relationships.
9. **Robot replacement.** A spare assumes an operational role but has a different hardware identity/history.
10. **Cross-site loan.** Independently commissioned identity universes meet temporarily.

Do not invent reconciliation semantics. State what R6, the Link underlay, Transport, and application each would have to provide.

---

## 8. Bandwidth, airtime, and configuration accounting

### 8.1 Facility-wide broad-Wire negative control

Compute the cost of this intentionally naive design:

```text
60 AMRs
pose envelope: 32-byte payload
canonical/profile overhead: state assumption
20 Hz
multi-hop replication factor: evaluate 1, 2, and 4 average transmissions
```

Report:

- source packets/s;
- radio transmissions/s;
- payload and framed bit rate;
- percentage of usable wireless capacity;
- how much traffic reaches AMRs more than 10 m away;
- impact on sleeping pallet tags if the same scope includes them.

### 8.2 Dynamic narrow-scope alternative

If using narrow Wires or membership updates for proximity groups, report:

```text
membership changes per AMR per minute
facility-wide configuration operations per second
configuration distribution latency
stale membership window
behavior if two partitions update the same scope independently
```

Do not hide these behind “the Organizer updates it.”

### 8.3 Opaque mesh-Link alternative

If treating the entire routed mesh as one Physical Link, state:

- who performs unicast route selection;
- whether Link-layer broadcast means facility flood or local radio broadcast;
- where duplicate suppression occurs;
- what happens during partition/merge;
- whether AMR proximity groups are Link-layer groups, Service state, or absent;
- whether WireSpaces adds useful topology semantics or is simply payload over an existing network.

### 8.4 Configuration inventory

Count for Config A/B:

- Participants and WireNumbers;
- stable Wire memberships;
- dynamic memberships, if any;
- per-gateway forwarding objects;
- radio-underlay route/group objects not owned by WS;
- temporary groups created/retired per hour;
- configuration changes required by Config C partition/merge.

---

## 9. Expected diagnostic questions

### Question A — Wire vs route

Can a Logical Bus remain stable while physical next hops change continuously, or does the mapping quietly turn the radio mesh into a black-box routed network?

### Question B — proximity and geographic scope

The natural recipients are “Participants near me now” or “AMRs entering Zone 7,” not a fixed Participant list. Is that:

- a stable Wire;
- application-level recipient selection;
- a radio-underlay multicast group;
- repeated directed PDUs;
- unsupported?

State the cost and semantics.

### Question C — static membership churn

If normal motion requires Wire reconfiguration, is static/read-mostly membership still the right abstraction?

### Question D — flood-and-filter on wireless

Does bus-oriented propagation waste enough airtime/energy that destination-pruned forwarding becomes mandatory rather than optional?

### Question E — partitions and merge

R6 defines static forwarding, not distributed convergence. What remains valid when two connected components operate independently and later merge?

### Question F — duplicate paths

If infrastructure and peer mesh both deliver the same semantic message, does the application see:

- one canonical PDU;
- two copies of one PDU;
- two PDUs on separate Wires?

Where are freshness and duplicate suppression provided?

### Question G — where WS should stop

Possible scope boundaries include:

```text
inside each AMR only
stable Ethernet/infrastructure only
WS over an opaque routed wireless substrate
facility-wide including mobile mesh
```

Choose the narrowest boundary that remains useful; do not stretch WS merely to claim full coverage.

---

## 10. Valid experiment outcomes

Valid negative or mixed outcomes include:

```text
"The mobile mesh is outside WS's design center. Treat it as an opaque routed
 Link or use conventional IP/mesh networking, while retaining WS Services
 inside robots and across stable infrastructure."

"One broad Wire is semantically valid but operationally poor because wireless
 flood-and-filter wastes airtime and energy."

"Dynamic narrow Wires reproduce multicast-group and route-convergence state
 that R6 intentionally does not provide."

"Stable Participants survive mobility, but stable Wires do not naturally
 represent geographic neighborhoods."

"The radio underlay can hide route churn successfully, but then WS contributes
 identity and Services rather than mesh topology."

"Partition/merge is an application and underlay problem; R6 offers no
 distributed reconciliation semantics and should not pretend otherwise."
```

A claim of “strong natural fit” requires explicit airtime and reconfiguration arithmetic.

---

## 11. What this archetype does not include

- robot motor-control loops or safety PLC internals;
- lidar/camera raw streams;
- warehouse optimization algorithms;
- radio PHY/MAC design;
- security, authentication, authorization, or trust policy;
- cloud analytics;
- Internet-scale routing;
- a requirement to redesign the existing mesh protocol;
- dynamic routing as a new WireSpaces feature;
- leader election;
- formal functional-safety analysis.

The assignment tests messaging scope, topology, mobility, partition behavior, and wireless cost—not access control.

---

## 12. Mandatory output additions

In addition to the normal sketch template, include:

```text
Falsification criteria

Two mappings:
    A. mesh treated as opaque routed Physical Link
    B. physical hops / zones exposed to Wire topology

For each:
    Wire count and membership stability
    forwarding/configuration churn
    broad-publish airtime arithmetic
    partition/merge behavior
    duplicate-path behavior

Scope verdict:
    where WS should begin and end
```


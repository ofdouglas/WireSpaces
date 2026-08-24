# WireSpaces R6 Architecture Trial Report

## Significant Findings Through Archetype 11C

### Executive summary

The trial series has done what it was intended to do: it has separated problems in the **canonical WireSpaces model** from problems in individual **Link projections**, deployment choices, and physical topology.

The strongest result is that the R6 shift to **deployment-global Participants + loop-free Logical Buses** has held up across every reviewed archetype, from a four-node CAN machine through heterogeneous multicore gateways, dense peer groups, a modern automotive zone architecture, and a triply redundant eVTOL. None of the trials has produced a compelling reason to restore Origin/Node roles, dynamic routing identity, or controller authority to the networking layer.

The canonical model generally gets *less* interesting as the system gets more complicated: Participants remain Participants, Wires remain propagation scopes, and Services remain Endpoints. Most complexity appears where it belongs—in constrained-Link projection, physical redundancy, application authority, or genuine deployment configuration.

The principal findings are:

1. **Participant identity is a strong success.** Multicore domains, symmetric peers, application authorities, gateways, service tools, and remote operators all fit without address-role reassignment or invented masters.
2. **Logical Bus Wires scale well when drawn by communication scope, locality, and failure boundary rather than by cable or Service.** Heterogeneous Wires spanning SHM → Ethernet → CAN were among the strongest results.
3. **Intra-device/inter-device continuity is strongly validated architecturally.** A Service interaction can cross shared memory, Ethernet, CAN-FD, CAN11, or radio while retaining the same canonical identity model.
4. **CAN11 compression behaves like a projection problem rather than contaminating the canonical architecture.** The default two-Main topology is excellent for controller+diagnostic-authority buses; small peer graphs are inexpensive with explicit maps; dense graphs eventually make CAN29 the preferable choice.
5. **The Service model scales much better than per-message network configuration.** Multiple Services reuse Participant relationships and Wire scopes; trial #10 in particular showed a large Service set without Wire-per-Service or VCN-per-Service proliferation.
6. **Forwarding/composition/observation distinctions are useful but repeatedly confused by mappers.** This points to documentation/tooling pressure, not a need to blur the concepts.
7. **The biggest unresolved architectural pressure is physical redundancy.** All three eVTOL mappings independently found the RS-485 triangle awkward under the loop-free invariant. Two of three correctly concluded that dual-homed A/B CAN paths also require separate canonical Wires today; the third independently attempted the more convenient “one Wire, redundant physical paths” abstraction that current R6 does not actually provide.
8. **Destination-pruned forwarding deserves renewed attention.** Base flood-and-filter is simple and bounded, but it can force extra Wires solely to avoid sending high-rate traffic down irrelevant constrained branches.
9. **Configuration grows substantially in large systems, but little of it appears artificial.** Much of the apparent forwarding burden in the agent reports was also over-counted relative to the preferred `Wire -> LinkBitmask` representation.

The current evidence therefore argues for **refining a few deployment and forwarding semantics, not reopening R6 identity or the core Service model**.

---

# 1. Scope of this report

This report covers the trial submissions actually reviewed in this thread:

* 01 — Small Single-Controller CAN Machine
* 02 — Raspberry Pi Robot + ECUs + Dev PC
* 05 — Heterogeneous Gateway
* 06 — Overlapping Logical Scope / Bandwidth Isolation
* 07 — Dense Peer CAN, both the original four-domain version and the later eight-domain dual-core revision
* 08 — Flat Ethernet Peer Mesh
* 09 — Distributed Chassis CAN Cell
* 10 — Automotive Zone + Central Compute Slice, independent B and C mappings
* 11 — Redundant eVTOL Flight Control, independent A, B, and C mappings

There are no reviewed #03 or #04 submissions among the files in this trial sequence, so no findings for them are invented here.

The report incorporates both the submitted mappings and the substantive corrections made during review.

---

# 2. Overall result: R6 Participant identity is holding up extremely well

This is the clearest conclusion in the corpus.

Trial 01 begins with the simplest favorable case: one controller and several CAN leaves. One Participant per Endpoint Domain, one Wire, no forwarding, and a natural MainA position are enough. 

At the opposite end, trial 11C has 30 production Participants in P8 and 38 in P16, including six independently meaningful flight-computer domains, repeated actuator controllers, sensors, avionics, telemetry, and Ground Station. It still requires no canonical primary controller, no Origin, and no ParticipantId reassignment after failures. 

The progression between those extremes reinforces the same point:

* #07's peer ECUs remain genuinely symmetric; CAN11 compression may be hierarchical, but the canonical model is not. 
* #09 cleanly represents two separate four-peer meshes plus a VehicleState producer without introducing a coordinator. 
* #10 models multiple domains inside central compute, a zone controller, and telematics as separate Participants even when several live inside one physical SoC. 
* #11 keeps Ground Station reachability distinct from Ground Station authority; whether a command is permitted remains application/Service state. 

This strongly validates one of the important R6 decisions:

> **Participant identity should describe the independently meaningful source/dispatch domain, not a physical device, bus-controller role, or authority rank.**

The network does not need to know which controller is “active,” which controller is “primary,” whether a source is a service tool, or whether an operator currently possesses actuation authority.

That is application semantics.

---

# 3. Logical Bus Wires are working best when treated as scopes, not cables

The trials repeatedly punish “one Wire per cable” and “one Wire per Service” thinking.

## 3.1 Simple physical correspondence is fine when it is genuinely correct

In #01, one CAN segment really is one meaningful plant communication scope, so one Wire is exactly right. 

Likewise #08's thirteen Ethernet peers naturally fit one Wire because the entire rack is genuinely one peer communication scope. The 78 possible peer pairs do not require 78 topology objects. 

## 3.2 Heterogeneous Wires are one of the strongest successes

Trial #05 demonstrated the more distinctive case. Plant control Wires span shared memory and a fieldbus:

```text
Core0 application
      |
   InterCore
      |
Core1 forwarder
      |
     CAN
      |
 field ECU
```

The same canonical Wire includes both sides. The application-side Participant can therefore remain the real source even though another core owns the CAN controller. 

Trial #10 scales the same idea much further. Its fieldbus Wires span central SHM → automotive Ethernet → zone SHM → CAN-FD/CAN11. Its Wires are explicitly shaped around control, fieldbus, platform, and telemetry scopes rather than around individual Services. 

That is probably the best empirical support so far for the original WireSpaces pitch:

> **A Wire can remain the same logical communication object across very unlike physical Links.**

## 3.3 A Wire is not a traffic class

Trial #06 was useful specifically because the tempting “safety Wire versus telemetry Wire” split was *not* needed. One CAN Wire plus QoS and observation was simpler. Overlapping Wires would not magically create electrical bandwidth or transform one existing PDU into another Wire. 

This sharpened the rule:

> **Wires describe propagation scope. They are not application topics, priority classes, subscription groups, or labels applied retrospectively to the same transmitted PDU.**

---

# 4. Multicore and intra-device/inter-device continuity are strongly validated

Several trials independently used shared memory as an ordinary Link rather than as a separate architectural universe.

Trial #05 showed a two-domain gateway where the application core and fieldbus core remain independently identified Participants across shared memory and CAN. 

Trial #10 made that model routine across three multi-domain computers. The second #10 mapper explicitly observed that a diagnostic Service could reach same-SoC, Ethernet, CAN-FD, committed CAN11, and Guest-CAN targets with the same canonical source/destination/Endpoint form. 

Trial #11C uses the same property for something much more safety-relevant: an actuator can receive a command whose canonical source is `FC_A_Control`, even though the PDU has crossed shared memory and a CAN-FD interface owned by `FC_A_Platform`. 

That is a significant result.

The trials support the idea that:

```text
same process
shared memory between domains
another ECU over Ethernet
field ECU over CAN
```

can all be deployment variations of the same Service-facing model, as long as the physical latency/failure/bandwidth differences are still acknowledged.

No trial has exposed a reason to create a separate “IPC service architecture” alongside the network Service architecture.

---

# 5. Forwarding, composition, splice, and observation are valuable distinctions—but need much clearer guidance

The trial agents repeatedly stumbled over these boundaries. That is itself a significant finding.

## 5.1 Transparent forwarding preserves canonical identity

Trial #05 exposed an important CAN-specific consequence. If Core0 authors a command and Core1 only forwards it onto CAN, the CAN VCN must encode **Core0 ↔ field device**, even though Core1 physically owns the transceiver.

The original agent instead mapped Core1 as MainA. Its own structure makes clear that Core0 is the sequencer and Core1 is the forwarding fieldbus engine. 

The corrected invariant is:

> **Link projection represents the canonical Participant relationship, not controller ownership of the physical Link.**

Trial #10 independently validates this again: `ZoneNet` owns the CAN controllers but does not consume a BodyCAN11 Main or Node position; `ZoneControl` and `DiagOTA` are the canonical participants encoded by the Link profile. 

## 5.2 Cross-Wire “forwarding” is not forwarding

This error recurred.

Ordinary forwarding preserves Wire identity. Therefore:

```text
W4 -> W1
```

cannot be ordinary forwarding.

It must instead be:

* composition, if a Service consumes one interaction and authors another; or
* an explicit splice, if canonical source/destination/Endpoint are preserved while Wire scope changes.

Trial #05 contained this ambiguity in maintenance and SCADA paths. Trial #11B made it particularly visible by describing `W13 → W12 → W1` as forwarding, even though those are separate canonical Wires. 

Trial #11C handled the distinction more rigorously by explicitly introducing service-scope splices. 

## 5.3 Observation preserves the original PDU

Trial #06 also caught an important modeling error. The submitted interaction table described drive status as `Drive -> Telemetry` while simultaneously saying that Telemetry merely observes the already-existing `Drive -> Motion` PDU. 

Observation does not rewrite destination identity.

Correctly:

```text
Src  = Drive
Dest = Motion
Wire = W1

Observer:
    Telemetry accepts a configured copy
```

This is valuable because observation does **not** consume an extra VCN relationship or require retransmission.

## 5.4 Splice granularity now deserves specification attention

Trial #11C needs Ground-originated service operations to cross into avionics and actuator Wires while preserving Ground Station identity. It therefore uses five splices, with active/spare splice hosts. 

That raises a real unresolved question:

> Can a splice be constrained by destination, Endpoint, or another static selector?

A completely Wire-wide projection may be too blunt for rich service-access cases. This should survive into the design backlog.

---

# 6. CAN11: the default VCN topology has a clear and useful design center

The trials have produced a much better picture of where the two-Main default map works and where it does not.

## 6.1 MainA + leaves is trivially effective

Trial #01 is the clean baseline: one real coordinator plus field leaves. The default map is nearly free configuration. 

Trial #02 repeats the same result for a Pi-controlled robot. 

These are not interesting stress tests, but they establish that the easy case remains easy.

## 6.2 Two genuine authorities are the more important positive result

Trial #06 finally exercises the intended two-Main pattern naturally:

```text
MainA = Motion
MainB = Safety

DriveLeft  <-> both
DriveRight <-> both
Telemetry  <-> Motion
```

The two positions correspond to two real independently initiating authorities. 

Trial #10 provides an even stronger and probably more commercially representative case:

```text
MainA = ZoneControl
MainB = DiagOTA

Door        <-> both
Wiper       <-> both
Access      <-> both
Lighting    <-> both
HVAC        <-> both
```



This suggests the default CAN11 graph is not merely “master/slave legacy baggage.” It fits a common embedded architecture very well:

> **one operational/control authority + one diagnostic/service authority + many leaves**

That may be the strongest justification for retaining the two-Main default map.

---

# 7. CAN11 peer graphs degrade gracefully rather than failing abruptly

The peer trials provide a useful progression.

## 7.1 Original four-peer test

The first #07 has four symmetric production Participants. The default graph cannot express all six peer pairs because there are no Node↔Node slots, so an explicit map is required. 

It uses:

* six unordered pair relationships;
* four source→broadcast relationships;
* ten ordinary VCNs total.



The original mapper rated this configuration burden “Significant” and preferred CAN29. That review conclusion was too pessimistic. Ten static relationship entries for a four-node peer system are **mild** configuration, especially because each relationship can carry many Services.

## 7.2 Revised eight-domain version

The revised #07 makes each physical corner dual-domain. It has eight production Participants but only the four Chassis domains are full peers; the Diagnostics domains primarily have local relations. The honest production graph is 14 relationships, not a fabricated 8×8 mesh. 

The trial correctly concludes that this still fits comfortably in one explicit map and that configuration burden is mild. 

The key lesson is:

> **Count actual required Participant relationships, not every theoretically possible pair.**

## 7.3 Trial #09 reaches the real CAN11 pressure point

Trial #09 deliberately increases interaction density:

* 6 chassis peer pairs
* 4 chassis broadcasts
* 6 supervisor peer pairs
* 4 supervisor broadcasts
* 4 local chassis↔supervisor pairs
* 4 VehicleState↔chassis pairs
* 1 VehicleState broadcast

Total: **29 production relationships**. 

Bring-up adds four DevPC↔Supervisor relations, giving 33. That crosses the capacity of one map and requires a second alias or CAN29. 

With VCN 3 reserved for Link control, the more accurate reporting is:

```text
31 usable ordinary relationship values per alias

Config A: 29 / 31
Config B: 33 total across two aliases
```

The significant result is not that CAN11 “fails” at 29.

It is that somewhere in the **high twenties of actual relationships**, CAN11 compression becomes a design concern rather than an invisible implementation detail. At that point CAN29 begins to win primarily on headroom and simplicity.

That is a graceful boundary.

---

# 8. CAN29 / rich links confirm that VCN complexity is Link-local

Trial #08 is a useful control experiment.

Thirteen symmetric Ethernet peers have 78 possible unordered relationships, yet no pair table is needed because full source and destination identity ride directly in the encapsulation. 

Trial #11 reaches the same result on large CAN-FD/29-bit actuator groups: the dense, critical interaction graph creates no VCN or WireAlias problem at all. 

This strongly supports the architectural separation:

```text
canonical model:
    Participants + Wires + Endpoints

CAN11:
    compressed projection of canonical relations

Ethernet / CAN29 / SHM:
    direct or nearly direct representation
```

In other words:

> **VCN complexity is a CAN11 representation cost, not a property of WireSpaces communication itself.**

That is an important success.

---

# 9. Trial #08 clarified what WS must offer on rich networks

The flat Ethernet rack is a deliberately weak networking fit.

The mapping itself is straightforward: thirteen Participants, one Wire, no forwarding, no pair enumeration. 

The agent therefore questioned whether WS adds much over ordinary UDP sockets. That is fair for the **network substrate**, but it should not be interpreted as an R6 failure.

The more useful conclusion is:

> On a flat rich network, the networking abstraction may add little. WS must justify itself there through the **Service ecosystem, common identity, tooling, deployment portability, and continuity with constrained devices**.

That is actually a healthy product boundary. WS does not need a flat Ethernet rack to be impossible without it.

It needs adoption not to become awkward when the user wants the common Services.

---

# 10. Standard Services emerged as the likely product-level payoff

Trial #10 is the strongest evidence here.

The automotive slice includes roughly twenty production Participants, several Link classes, and a broad Service set:

* Identity / Version
* Health
* Fault / DTC
* update
* Link status
* time
* telemetry
* logs/events
* diagnostic query/RPC

Yet the Services reuse existing Participant relationships and Wire scopes rather than generating new topology objects. 

The second #10 mapper states the result even more clearly: nine Service classes across twenty Participants add zero new Wires. 

Trial #11C extends the same observation across tiny sensor/actuator devices, multicore flight computers, Ethernet avionics, radio gateways, and a Ground Station. Thirteen Service classes still add no Service-specific Wires. 

This supports the desired division of responsibility:

```text
Endpoint:
    what capability / Service?

Participant:
    who authored / accepts it?

Wire:
    where may it propagate?

Link profile:
    how is that represented here?
```

That separation is holding up.

---

# 11. Failure modeling is generally a strong point

The static nature of membership has repeatedly been helpful.

A powered-off Participant does not stop being conceptually part of the deployment; it simply becomes unreachable. This avoids topology or address reassignment during ordinary faults.

Trial #10's failure cases are readable directly from the Wire realization: backbone loss partitions central and zone portions while leaving zone-local fieldbuses useful; loss of one body gateway affects its subordinate LIN functions without disturbing other buses. 

Trial #11C makes the same property explicit at much higher criticality. A lost RS-485 leg removes one pair Wire; a lost CAN-A path removes one actuator-path Wire; a lost FC simply stops sourcing without network election or renumbering. 

This is particularly compatible with systems where degraded operation is expected rather than exceptional.

---

# 12. Trial #11 exposed the major unresolved architecture pressure: physical redundancy

This is the most important open issue found by the trials.

## 12.1 RS-485 triangle: three agents independently hit the same problem

The physical topology is:

```text
A ----- B
 \     /
   \ /
    C
```

All three #11 agents independently concluded that one ordinary flooded Wire cannot use all three legs while preserving the loop-free invariant.

A and C therefore use:

```text
PeerAB
PeerBC
PeerCA
```

one canonical Wire per physical peer leg.  

This uses all three physical paths and preserves symmetry, but after loss of AB, A and B remain physically connected through C while their direct canonical PeerAB Wire is gone. Automatic alternate routing does not occur.

That is not necessarily a defect. It is a deliberate consequence of:

> **loop-free static Wire propagation rather than dynamic routing**

But three independent mappings converged on it, so the pressure is real.

## 12.2 Dual A/B actuator buses reveal the parallel-path version

Agent A and Agent C correctly conclude that dual-homed actuator buses must also become separate canonical path Wires under current R6.  

Agent B instead attempted the intuitively attractive abstraction:

```text
one FwdActuation Wire
    over FwdCAN_A + FwdCAN_B
```

with failure of one bus merely removing one egress branch. 

That mapping is not valid under the current invariant when the same Participants are dual-homed to both buses: it creates parallel paths/cycles or requires a new multipath rule.

But the fact that an independent mapper naturally reached for it is valuable evidence.

It describes exactly the abstraction we may want to investigate:

> **one logical propagation scope with a small, bounded redundant physical realization**

without turning WS into a general dynamic-routing network.

## 12.3 Current semantics push redundancy into Services

With separate A/B Wires:

```text
FC_A_Control
    -> command on FwdActA
    -> command on FwdActB
```

those are two canonical PDUs with different Wire identities.

The actuator must therefore understand that these are redundant instances of one semantic operation. Agent C explicitly calls out that duplicate-delivery behavior is currently undefined. 

This is now a legitimate design question:

> Should redundant physical delivery always remain visible as multiple canonical Wire interactions, or should Wire/Link someday have an optional bounded redundancy abstraction?

The trials do **not** yet answer that.

They do show that this is the place worth revisiting—not Participant identity.

---

# 13. Destination-pruned forwarding may be more important than originally assumed

The simplest Router model remains attractive:

```text
Wire -> LinkBitmask

egress = mask & ~IngressLink
```

It is tiny, static, and easy to audit.

But trial #11C shows that pure flood-and-filter can force extra Wire decomposition simply to avoid putting high-rate traffic on irrelevant constrained branches.

Its sensor example is clean:

```text
one Sense wire:
    IMU link
    GNSS link
    SHM
```

would cause high-rate IMU PDUs destined for Control to also be transmitted onto the GNSS serial branch under unpruned flooding. The mapper therefore creates separate IMU and GNSS Wires. 

This does **not** prove that destination pruning must become mandatory everywhere.

A better synthesis is:

> **Base flood-and-filter is sufficient for correctness, but destination-pruned egress can materially reduce unnecessary constrained-link traffic and Wire count in heterogeneous trees.**

That deserves a design review after the redundancy question.

The good news is that static Participant-location pruning could remain fully bounded; this need not imply dynamic routing.

---

# 14. Configuration burden: larger than toy protocols, but largely real

The agents frequently reported moderate or significant configuration in the larger trials.

That is true, but two qualifications matter.

## 14.1 Most large-system configuration reflects real system complexity

Trial #10 has roughly twenty Participants, ten physical Links, multiple buses, several multicore domains, a diagnostic path, telemetry, Guest coexistence, and numerous Services. Moderate configuration is appropriate. 

Trial #11 contains eighteen physical Links and extensive redundancy. Significant deployment state is unavoidable regardless of middleware. 

The fair comparison is not “100 WS objects versus nothing.” It is WS deployment state versus some combination of DBCs, routing matrices, IPC channel tables, Ethernet service catalogs, diagnostic address databases, telemetry maps, and application gateway translations.

## 14.2 Several agents systematically over-counted Router state

The reports often counted two or three “ingress masks” for a Wire that could be expressed by one static membership mask.

For the simple preferred representation:

```text
Wire -> {Link0, Link1, Link2}

egress = mask & ~IngressLink
```

a three-branch Wire is one configuration object, not three independent routing decisions.

Ingress-sensitive forwarding should remain available for exceptional asymmetric cases, but trial accounting should use the simple form first.

This correction materially reduces some of the “~100” and “~125 object” headlines without changing the conclusion that #10 and #11 are substantial systems.

---

# 15. Several documentation/tooling issues recurred enough to be considered findings

## VCN capacity should consistently be reported as 31 usable ordinary values

If VCN 3 is permanently reserved for Link control, a 5-bit map has:

```text
32 total VCN values
31 usable ordinary relationships
```

Several earlier reports wrote `29 / 32` or `11 / 32`, which obscures the real headroom.

Trial #10B had already switched to the clearer `11 / 31` form. 

## `kLocalBus` should remain the narrow local-number reuse mechanism

The later agents occasionally generalized this into “device-scoped WireNumbers.”

That is unnecessary.

The cleaner rule remains:

> `kLocalBus` is the reserved local-only canonical Wire value, independently reusable in local Router/Endpoint-Domain contexts.

Do not create a second general WireNumber namespace concept merely because several devices have private SHM Wires.

## Guest CAN material was stale across both #10 mappers

Both #10 agents described a 16-ID Guest allocation using a “Guest-3” name and a separate VCN-7 control convention.  

That does not match the newer monotonic Guest-size/VCN direction discussed during review. Therefore the *topological* Guest findings are useful, but those exact profile names and tables should not be copied into normative material until the experiment overlay is synchronized.

## Cross-corpus assertions should not be made by individual mappers

For example, one #10 mapper called itself the first test to exercise MainB, despite #06 already doing so.

Mapping agents should report their own result; synthesis should establish chronology and corpus-wide claims.

---

# 16. Trial-by-trial disposition

| Trial                           | Most important result                                                                                                                                                                             | Overall disposition                              |
| ------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------ |
| **01 Simple CAN Machine**       | One real coordinator + leaves maps almost trivially to one Wire and default MainA CAN11.                                                                                                          | **Strong baseline pass**                         |
| **02 Pi Robot**                 | Distinct plant and developer scopes are natural; gateway composition is explicit; WiFi/Ethernet can realize the same logical host scope.                                                          | **Strong pass**                                  |
| **05 Heterogeneous Gateway**    | Wires spanning SHM + CAN/RS-485 work naturally; exposed the crucial distinction between canonical source and transceiver ownership. Also exposed cross-Wire-forwarding ambiguity.                 | **Strong pass with important corrections**       |
| **07 original, 4 peers**        | R6 peer equality works; default CAN11 hierarchy does not. Explicit relation map is needed but is much less burdensome than mapper claimed.                                                        | **R6 strong; CAN11 explicit mild**               |
| **07 revised, 8 domains**       | More Participants do not imply all-to-all density. Honest interaction graph is still only 14 VCNs.                                                                                                | **Strong pass**                                  |
| **06 Motion/Safety/Telemetry**  | Best early positive control for two-Main default map. Observation avoids duplicate relation/transmission. Overlapping Wires are not bandwidth classes.                                            | **Strong pass**                                  |
| **08 Ethernet peer rack**       | Dense peer addressing is trivial on a rich Link. WS networking adds modest value here; Service ecosystem must justify adoption.                                                                   | **Natural fit, weak networking differentiation** |
| **09 Distributed Chassis Cell** | 29/31 actual CAN11 relations finally produce real VCN pressure; 33 requires second alias or CAN29.                                                                                                | **Excellent CAN11 boundary test**                |
| **10 Automotive B/C**           | Large heterogeneous system converges on scope-shaped Wires, transparent application identity, default two-Main body CAN, Guest adoption, and reusable standard Services.                          | **Very strong system-scale pass**                |
| **11 eVTOL A/B/C**              | Participant/Service model remains clean, but loop-free Wires make rings and redundant physical paths explicit as multiple Wires. Three-agent convergence establishes genuine redundancy pressure. | **R6 strong; Wire redundancy requires review**   |

---

# 17. Assessment against the principal WireSpaces goals

### Logical Bus propagation — strong evidence

The model works from one-link CAN buses through multi-Link heterogeneous trees. The main unresolved issue is redundant/cyclic physical realization, not ordinary propagation.

### Heterogeneous Link portability — very strong evidence

Trials #05, #10, and #11 repeatedly use the same canonical model across SHM, Ethernet, CAN, RS-485, UART-like sensor links, and radio.

### Tiny-target friendliness — not yet materially validated

The architecture still appears compatible with tiny nodes, but these thought experiments do not validate executable code size, RAM, CPU cost, or minimum Link-profile implementation size. That remains prototype work.

### Constrained-Link projections — strong evidence

CAN11 VCN has a well-defined design center and a graceful escape path. CAN29/richer links remain much simpler.

### Integrated Service ecosystem — strong architectural evidence, implementation unvalidated

The trials strongly support the *shape* of the ecosystem: Services reuse existing relations and scopes. They do not yet prove API quality, code reuse, conformance ergonomics, or implementation footprint.

### Static/bounded forwarding — strong on ordinary trees, current pressure point on redundancy

Tree-like heterogeneous systems work very well. Ring and multipath redundancy are the first topologies that make the static loop-free rule visibly expensive.

### Intra-device + inter-device continuity — very strong evidence

This may be one of the best-supported claims in the entire trial program.

---

# 18. What the trials did **not** validate

The architecture-sketch program should not be mistaken for prototype evidence.

It has not yet demonstrated:

* runtime/code-size cost on 8-bit or tiny 32-bit MCUs;
* actual shared-memory Router performance;
* queue/buffer/concurrency ergonomics;
* CAN-FD/29-bit final bit layout;
* final Ethernet profile;
* Guest CAN final encoding and commissioning behavior;
* real update-transfer behavior across constrained Links;
* security/authentication architecture;
* actual failure recovery timing;
* configuration generator/tooling usability;
* HIL/SIL/debug workflow quality;
* whether Service APIs feel pleasant when implemented.

Those should remain prototype or later design questions rather than being “solved” on paper.

---

# 19. Highest-priority design questions exposed by the trials

The report leaves a fairly short list of things worth revisiting.

**First is redundant physical topology.** The RS-485 ring and A/B actuator buses now have enough independent evidence to justify a focused design session. The goal should not be to add general routing; it should be to ask whether a small bounded redundancy construct can preserve WS's static/auditable nature while avoiding one canonical Wire per alternate path.

**Second is destination-pruned forwarding.** We should decide whether it remains an optional optimization or becomes a normal static Router capability for heterogeneous Wires. It may reduce Wire proliferation substantially without weakening the core model.

**Third is splice semantics.** In particular, destination/Endpoint-scoped splicing needs to be either clearly supported or deliberately rejected in favor of composition.

**Fourth is duplicate semantics across redundant paths.** If redundancy continues to be represented as several Wires, the Service model needs a clear documented pattern for correlating duplicate operations and state.

**Fifth is Guest CAN cleanup.** The profile naming, monotonic mapping, control channel, global mapping burden, and QoS limitation need to be synchronized before more trials rely on it.

Everything else looks increasingly like implementation/prototype territory rather than a reason to continue changing the canonical architecture.

---

# 20. Bottom-line architecture judgment

Across the reviewed trials, the architecture becomes more convincing as the systems become more heterogeneous.

The weakest cases are not failures. A flat Ethernet rack merely shows that WS's network abstraction is not dramatically better than sockets when the underlying network already gives you rich addressing and there are no gateways or constrained Links. A dense CAN11 peer mesh shows that compression eventually becomes bookkeeping-heavy. A redundant eVTOL shows that a strict loop-free Logical Bus does not automatically exploit alternate physical routes.

Those are believable boundaries.

Meanwhile, the features WS is actually betting on have survived surprisingly well:

* stable Participant identity;
* application authority separate from addressing;
* scope-shaped Wires;
* heterogeneous propagation;
* transparent source preservation;
* explicit composition boundaries;
* constrained-Link projection;
* Services reused across very different device and Link classes;
* static, auditable deployment state.

The most encouraging meta-result is that **system complexity has generally grown faster than WireSpaces conceptual complexity**. The automotive trial became a much richer system without inventing new network concepts. The eVTOL finally found genuine pressure, but that pressure is narrowly concentrated in redundant physical-path realization rather than spread throughout the model.

At this point, the trial evidence supports treating **Participant + Logical Bus + Service** as the stable center of R6. The next architecture work should be focused on redundancy, pruning, and splice semantics around that center—not on reopening it.

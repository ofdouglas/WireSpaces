# WireSpaces — Future Work

**Status:** Not designed. Nothing in this document is a requirement, a commitment, or a specification  
**Purpose:** Hold material that is not yet designed, so `CORE` stays close to what is actually buildable  
**Authority:** None. If this document and `CORE` disagree, `CORE` wins by default

---

# 1. How to Read This Document

Material lives here when it is **not yet designed** and its absence from the main documents **cannot cause a wrong implementation decision today**. Boundary statements that prevent wrong builds stay in `CORE`; this file holds discussion only.

> **Do not implement anything here** unless explicitly instructed. **`REG §6` is authoritative for status** — if an item appears in both places, the register entry is current and this document only expands it.

---

# 2. Zero-Copy Buffer Ownership

Copy-based operation is the baseline and is intended to stay first-class (`CORE §16`). Zero-copy is deliberately not an initial focus.

A likely later architecture is destination-owned allocation:

```text
Service
    |
createPacketFor(destination)
    |
destination Link pool allocates block
    |
Service fills header + payload
    |
submit
    |
Link TX queue -> driver
    |
buffer returned to destination pool
```

This becomes attractive for Ethernet-sized messages, CAN XL, high-throughput FPGA/softcore gateways, and large bulk transfers. A further advanced feature may permit source-owned immutable buffers with completion tracking across multiple drivers, which is what fanout by copy (`CORE §16.3`) currently avoids needing.

That complexity should not be imposed on MCU users unless measurements show it is valuable. The trigger for revisiting this is profiling data from a real high-throughput gateway, not the availability of a larger carrier.

## 2.1 What copyless must preserve

Whenever this is designed, it changes one axis and no others (`OWN-5`):

> **Copyless delivery changes storage ownership, not Endpoint semantics.** A Queue stays history-preserving and bounded, a Snapshot stays latest-value, writer concurrency rules are unchanged, and full and replacement behavior are unchanged. A queue slot holds a handle instead of a copied message; a Snapshot publishes managed storage instead of copying into place.

The point of stating this in advance is that it forbids the tempting shortcut of introducing a *second* delivery model for the copyless path. One execution model with two ownership representations is tractable; two execution models is how a framework acquires a permanent fork.

There is also a concrete case that already argues for it, which is unusual for this document. A bootloader segment arriving fragmented over CAN is reassembled in an LLL context and then **copied** into the segment Endpoint's queue slot, so the largest buffer in the system exists twice on the target least able to afford it (`CORE §9.5`). Handing reassembly buffer ownership to the Endpoint removes the duplicate outright. That makes ownership transfer valuable on small constrained targets and not only on high-throughput gateways, which is the opposite of the usual assumption about zero-copy.

Before it can be baseline, the copyless design still has to solve lifetime, fan-out, reclamation, DMA ownership, cache coherency, reset behavior, and stale-handle/ABA issues (`REG §6.6`).

---

# 3. Additional Transports

The canonical descriptor reserves a 3-bit `TransportType` and the baseline is an Unreliable Datagram (`CORE §20`). No other Transport is designed, and the registry is not allocated.

## 3.1 Sequenced / end-to-end-protected datagram

This is the transport that should be designed **first**, ahead of anything reliable, and it is much smaller than it sounds. It adds two pieces of metadata to an ordinary unreliable datagram:

```text
a sequence or liveness counter    detect gaps, duplicates, and reordering
an end-to-end check value         detect corruption the source did not cause
```

and nothing else — no retransmission, no acknowledgment, no connection state, no window. A receiver gains the ability to say "I missed something," "I have seen this already," and "this was damaged in transit," which is precisely what `CORE §20.2` argues most Services actually need. It also closes the gateway gap in hop-by-hop integrity (`CORE §20`), since the check is computed by the producing Endpoint and verified by the consuming one rather than being recomputed at every hop.

Open: the exact fields and their widths, what the check value covers, whether the counter is per-Endpoint or per-Wire, restart and epoch behavior, the acceptance window, whether a gap is reported to the Service or only counted, and how it interacts with the freshness question in `CORE §21.4`. It also needs a decision about whether it lives in a header extension (`CORE §2.3`) or in Transport metadata, which is the first real test of the extension mechanism.

Note what this does *not* provide: an end-to-end check detects accidental corruption and nothing else. It is not authentication, and anyone modifying a PDU deliberately recomputes it (`CORE §22`).

## 3.2 Reliable and bulk transport

Firmware images, files, logs, and similar data should eventually use a Transport that can:

- segment across multiple PDUs;
- retry missing data;
- bound receiver storage;
- resume where useful.

Receiver windows and credits belong at this level rather than in a Link LLL, which is why Classical-CAN Participants are not expected to implement generic Link flow control (`LINK §2.11`).

Other plausible Transports: request/response with retry and correlation, and a command Transport that accepts input from several Participants and selects a source based on health or timeout (`CORE §20.1`).

Open before any of this can be designed: the TransportType registry allocation, whether ordering guarantees are a separate declared axis from delivery guarantees, and how a reliable Transport interacts with latest-value queue replacement (`CORE §14.3`), which is designed to discard exactly the data a reliable stream wants to keep.

---

# 4. Static Traffic Analysis and Capacity Models

`DEPLOY §2.2` establishes that static capacity checking is the intended mechanism for serious deployments. The analysis itself is undesigned.

The shape assumed so far is that a Service or generated schema declares conservative traffic properties:

```text
maximum messages/sec
maximum encoded PDU bytes
QoS
burst behavior
```

and tooling traces each stream across Wires and Links to accumulate estimated worst-case load per Physical Link:

```text
CAN_A
    MotorCommand       80 kbit/s
    MotorStatus       140 kbit/s
    Diagnostics        25 kbit/s
    other bounded     110 kbit/s
                     -----------
    estimated worst   355 kbit/s
```

Even conservative summation catches obvious impossible deployments, and that is where the early value is. Anything more — network calculus, schedulability proofs, latency bounds — is a much later concern.

The same declared information may eventually support system-wide communication visibility, configuration validation, bandwidth analysis, loop detection, compatibility checks, and generated topology diagrams. None of these are prerequisites for the protocol.

Open: the declaration schema itself, how encoded size is computed per Link profile rather than per Service, how polling cadence on master-initiated Links enters the model, and whether claims are advisory or enforced.

---

# 5. Flow, WireContract, and Higher-Level Analysis

Flow declarations, WireContracts, authority analysis, schedulability analysis, and redundancy analysis remain plausible higher-level features. They should be built **after** the basic network, Link profiles, routing, discovery, and tooling are proven useful.

Two constraints already apply to whatever these become, and both live in `CORE` rather than here:

- There is no implicit WireContract in the base architecture, and contract-level policy is never implied by its absence (`CORE §19.1`).
- Any pattern must flatten to ordinary Endpoints, Wires, bindings, and bounded local state at configuration time (`CORE §19.2`).

The maturity ladder places this work at Levels 4-5 (`INTRO §6`), and the defining requirement is that it stays additive: adopting none of it must remain a complete and legitimate way to use WireSpaces.

---

# 6. Restart and Reliability Direction

`CORE §23` now covers the parts of restart behavior that are observable outside the component: separate Link and restart-unit lifecycles, runtime generation, bounded quiesce, escalation, declared isolation, reset boundaries, and external supervision. What remains here is the part that is genuinely undesigned.

Local, and mostly a matter of naming: the lifecycle state enumeration and transition API, the vocabulary for a cancelled or faulted transmit, the ownership and timeout rules for asynchronous lifecycle commands, the heartbeat and supervisor interface, and how a restart-unit dependency group is expressed in configuration.

Distributed, and harder because it crosses a trust boundary: restart *ordering* across components, state re-establishment after an LLL or Link Interface restart, and what a Service is entitled to assume about its bindings after a **peer** Domain restarts. The last is the interesting one. Local generation says nothing about a peer's restart, so either a Service treats every binding as suspect after any silence — which is expensive and vague — or something observable carries a peer's restart across the Wire, at which point it is a protocol feature with a field, a width, and a wrap rule rather than a local convention. Learned-from-ingress bindings and reply contexts (`CORE §10.6`) are where this bites first, since both are peer state held locally with no natural invalidation event.

Cyclic and redundant forwarding profiles at the routing level are also deferred. The acyclic realization rule (`CORE §12.4`) holds until such a profile exists, and redundancy composed above Wires does not need one.

---

# 7. Namespace 3 Ecosystem Registry

Namespace 3 is intended for reusable, interoperable FOSS Services (`CORE §8.2`). On rich Links, `NS3 EID 1..65535` is available; on the General Classical-CAN PDUA profile, `NS3 EID 1..1023` is directly representable.

A plausible registry policy:

- mature/stable public Services allocate upward from low IDs;
- experimental/beta allocations may come from a high region;
- stable published IDs are not silently reused for unrelated semantics;
- retired stable IDs may remain tombstoned;
- not every reusable library needs an immediately permanent public ID.

The large canonical namespace is intentional. Identifier scarcity should exist only where the carrier is genuinely constrained, not because an early registry chose unnecessarily tiny ranges.

Open: the registry *process* — who allocates, how experimental ranges are reclaimed, and how tombstones are published. None of this matters until the project publishes anything, so it is deliberately unresolved.

---

# 8. Broader Ecosystem Service Catalog

## 8.1 Level-0 Services and allocation hierarchy

A compelling base ecosystem should make one PC-connected device useful immediately without complicated network configuration. Likely Level-0 / common Services:

```text
identity / build version
heartbeat / health / reset reason
text logs / structured events
Link telemetry
firmware update / object transfer
application telemetry and control
```

Service allocation hierarchy across Namespaces:

```text
NS0 EID 1..31
    exceptionally valuable compact Common Services

NS3 EID 1..1023
    broad FOSS ecosystem Services that must work on General Classical CAN

NS3 EID 1024..65535
    richer-link ecosystem Services
```

Not every Service needs the compact NS0 Common region (`LINK §2.4`). Service schemas need portable definitions and code generation eventually; the contract is `CORE §21.3`. Freshness representation (`CORE §21.4`) and time sync should be decided together.

## 8.2 Service archetypes

Most Services fall into a small number of shapes. These are **policy starting points, not transport types** — every one of them is ordinary Endpoints and Wires underneath — but naming them saves rediscovering the same set of decisions each time:

| Archetype | Defining policy | Typical QoS |
|---|---|---|
| **State / snapshot** | latest coherent value; replacement acceptable; freshness and validity explicit | Normal |
| **Command / control** | bounded state change; explicit authority, acceptance, and commitment point; idempotency or duplicate rules | Normal, or High where analysis justifies it |
| **RPC / query** | correlation where needed, bounded responder work, explicit timeout and Service-error behavior | Normal |
| **Event stream** | discrete occurrences retained in a bounded queue; ordering, gap detection, and overflow defined | Normal |
| **Health / heartbeat** | bounded liveness, readiness, degraded state, fault summary | Normal |
| **Logging / diagnostics** | bounded, rate-limited, aggregatable; never blocks control work | Background |
| **Bulk transfer** | small control and status messages around a separately bounded segmented mechanism | Background |

Three notes on the QoS column. `Normal` is the default and most traffic should stay there — a system where everything is High has no priorities. `Critical` is absent deliberately: it belongs only to traffic that has been through explicit admission, resource, and starvation analysis, since its whole purpose is to displace other traffic. And a higher QoS is scheduling intent only; it guarantees no latency, bandwidth, delivery, or freshness (`CORE §14`).

**RPC does not imply a reliable Transport.** A request/response exchange over an unreliable datagram with a timeout is a perfectly ordinary RPC, and often the right one (`CORE §20.2`).

Request/response, command/status, and pub/sub are compositions of these over directed traffic on Wires — not special routing modes (`CORE §23`).

## 8.3 Promotion criteria and the privileged tail

The catalog is a list of candidates, not a standard library and not an allocation authority. A Service is worth promoting to a published identity when it has a clear semantic boundary, a bounded schema, an explicit privilege model, compatibility metadata, a resource profile, test vectors, and **at least one real deployment** — the last of which is the criterion that stops the catalog from growing faster than the implementation.

Two distinctions found useful in an earlier generation are worth keeping in view when this is designed:

- **Reporting a capability is not enabling it.** A Service that enumerates what a device has configured does not enable anything, create Wires, or grant authority. Nor does a Service that selects among finite precompiled profiles negotiate arbitrary encodings; picking from a fixed list is not negotiation (`LINK-8`).
- **Scalar-prototype conveniences do not scale into protocols.** A generic "read/write a named scalar" Service is genuinely useful for throwaway bring-up, and features graduate out of it the moment they need transactions, timing, richer state, or interaction. Treating it as a permanent substitute for a designed Service is how a deployment ends up with its control interface expressed as untyped key/value writes.

The dangerous tail of the catalog — memory peek/poke, arbitrary register access, fault injection, raw capture and injection, firmware update — is governed by `CORE §22.1`: a separate development build, build-time enablement, and local-only non-forwardable operation. Runtime configuration alone must never enable them.

---

# 9. Remote Maintenance

The same Service model can work across Ethernet, VPN, radio, or other remote Links — logs, identity, health, configuration, resumable transfer, selected telemetry. Constraints already in `CORE`: do not mirror high-rate internal Wires over narrow remote Links; do not splice every internal Wire (`CORE §7`, `CORE §22`). Authentication and authorization are undesigned; an external secure boundary is required today.

---

# 10. High-Throughput Gateway and Analyzer

A compelling eventual demonstration could be an FPGA/softcore embedded router fabric with many CAN FD/XL interfaces, one or more Ethernet links, high-rate WS forwarding, host tapping and packet capture, logic-analyzer capture on selected buses, bus utilization and timing diagnostics, physical-layer diagnostics on selected channels, and an FTDI FIFO or Ethernet connection to WS PC tools.

The device itself could use WS internally between RTL blocks, softcore Services, field buses, and host tools.

This is an aspiration, not a plan. It is recorded because it is the use case that would justify zero-copy (§2), per-Wire top-talker consumption (§15), and CAN XL profiles.

---

# 11. Further Classical CAN Profiles

## 11.1 29-bit identifiers

The 11-bit identifier is a deliberately constrained compatibility floor (`LINK §2`). A future 29-bit Classical CAN profile is the planned escape hatch.

The current canonical direction is a provisional 48-bit descriptor with 8-bit `WireNumber`, `SrcParticipantId`, and `DestParticipantId` fields. A CAN29 profile has enough identifier space to carry those three routing identities plus QoS and limited profile control directly, avoiding CAN11 Wire elision, VCN lookup, or participant compression in ordinary deployments. That would remove:

- CAN11's one-Wire-per-Link-profile restriction;
- VCN allocation and participant-compression constraints;
- the inability to realize overlapping Wires on one CAN11 bus;
- some pressure behind compact Endpoint allocations and CAN11 control space.

CAN29 is the preferred escalation when several Wires must share one physical CAN bus, the communication graph does not fit participant compression cleanly, VCN configuration becomes awkward, or richer payload/metadata efficiency matters. Canonically, Wire identity, source Participant, destination Participant, Endpoint, and Transport semantics remain unchanged. Any `Direction` bit would be only a CAN-profile reconstruction device, not canonical semantics.

**No 29-bit identifier or byte layout is specified.** The provisional 8-bit canonical allocations guide the profile, but neither field placement nor payload packing should be inferred from CAN11 or frozen here.

## 11.2 Guest CAN

`LINK §2` owns the CAN11 profiles. The accepted Guest baseline is no longer wholly undesigned: the legacy-bus owner allocates WireSpaces one aligned contiguous block of 16 CAN identifiers, whose four variable low bits carry a 3-bit VCN and a Link-local `Direction`. The Link Binding supplies one fixed canonical QoS for the allocation; Guest frames do not carry per-frame QoS. The all-ones VCN is reserved for Link control rather than an ordinary configured circuit.

Two constraints remain worth carrying forward:

- **Guest VCN is not a subset reinterpretation of a Native CAN11 layout.** Its allocated-range encoding is a distinct profile.
- **The allocation must be provably disjoint from every legacy owner on that bus**, in every configured state. On a bus WS does not control, WS configuration alone cannot establish that fact.

The following remain future profile/tooling work:

- larger aligned Guest allocations, such as 32- or 64-ID blocks with wider VCN fields;
- the exact fixed-QoS selection and arbitration policy;
- ordinary VCN allocation policy and VCN-table representation;
- exact participant/VCN map-fingerprint coverage and compatibility behavior;
- the commissioning and Link-control protocol carried by reserved values;
- atomic map activation, including the treatment of reassembly already in progress when a map changes;
- CAN29 identifier and byte layout.

CAN29 remains the preferred answer when the controllers and deployment support it, especially where CAN11 allocation or configuration constraints would otherwise accumulate.

---

# 12. Cross-WireSpace Identity Translation

Canonical `ParticipantId` values are unique within one coordinated deployment identity universe, and plain forwarding does not merge independently assigned universes. Likewise, a Wire is a Logical Bus with canonical identity; connecting Physical Links does not make independently defined Wires identical.

What translation looks like remains undesigned. It would at least map `ParticipantId` and Wire identity across the boundary. That is not transparent forwarding: the boundary is composition, consuming an interaction in one identity universe and authoring a new PDU in the other, with the composing Participant as canonical source. Any Endpoint or Service-semantic translation belongs to that composition as well.

Also open: whether a WireSpace should carry policy or trust semantics in addition to being an identity scope. It is deliberately claimed as nothing more than a scope today.

---

# 13. Communication Component Catalog

`CORE §19.2` adopts the constraint that higher-level patterns must flatten, and explicitly declines to establish the catalog of components that would do the flattening.

Earlier drafts sketched names such as `Harness`, `Bus`, `Channel`, `RequestResponse`, and `FreshnessMonitor`. Those were illustrative of possible behavior and were never a frozen set of classes or APIs. They are recorded here only so the names are not reinvented with different meanings.

The reason for declining the catalog is that a component set invented before the runtime exists tends to encode assumptions the runtime then has to satisfy. The flattening constraint was adopted alone because it is the part that protects the architecture *from* the catalog.

Any future catalog work should start from a real Service that cannot be expressed with plain Endpoints, Wires, and bindings — and should treat the inability to find such a Service as a useful answer.

---

# 14. Domain Control

An earlier generation described a Domain Control concept: a management interface for an Endpoint Domain, covering lifecycle, Service registration, and local policy.

It was deferred rather than rejected. Nothing in the current architecture needs it: Endpoint registration is implementation-specific (`CORE §9.2`), configuration authority is host-side (`DEPLOY §1.1`), and privileged operations require a separate build (`CORE §22.1`). The pieces it would have coordinated are currently either local implementation detail or explicitly outside the protocol.

It becomes interesting again if runtime Service replacement is ever wanted, which is also what the Endpoint lifetime problem in `CORE §9.7` is waiting on.

Two ideas from that earlier design are worth recording, because they are the parts that were actually load-bearing:

- **Serialized acceptance with supersession.** Requests to change a Domain's operating state are accepted in one order and stamped with a generation, and callers do not supply their own generations. A newer accepted request supersedes an older one for the same target, even if the two arrived through different management paths. What this buys is a definite answer to "what is the current intent," and an older request that reports `Superseded` rather than appearing pending forever. Convergence is explicitly not distributed atomic execution: a status summary distinguishing satisfied, pending, failed, and superseded is enough.
- **Gating must not gate its own control path.** If ordinary Service activity is quiesced, the path needed to issue the *next* management request and to observe convergence has to remain reachable — the control Service, its Wires, and any Link-control or status lifeline. This is the same structural mistake as a credit scheme whose credit updates are subject to credit (`CORE §15.5`), and it produces the same outcome: a system that correctly followed an instruction into a state it cannot be instructed out of.

Neither needs to be designed now, but a future Domain Control that ignores the second one is a remote-bricking mechanism.

---

# 15. Smaller Deferred Extensions

Individually minor, collected so they are not re-proposed as novel:

| Extension | Current state | Trigger to revisit |
|---|---|---|
| Same-profile cut-through forwarding | Not the generic architecture (`CORE §12`) | Measured gateway latency problem, and only if behaviorally equivalent |
| Destination-Participant branch pruning | Logical-Bus propagation and acceptance filtering are the baseline (`CORE §12.1`) | Demonstrated bandwidth pressure on a multi-branch Wire |
| Per-Wire congestion signaling | Coarse credit pools accepted (`CORE §15.6`) | A real system where head-of-line coupling is inadequate |
| Link Manager Service | Telemetry exists; no consumer (`DEPLOY §3.3`) | A gateway large enough for automatic policy to beat human diagnosis |
| Intermediate QoS profiles | Only Minimal and Full standardized (`CORE §14.1`) | Implementation experience showing a real need for `Normal+Background` |
| Extended internal-Wire profile | 127 device-private Wires assumed ample (`CORE §4.4`) | An implementation genuinely needing hundreds of internal Wires |
| Many-core profile | No special assumptions (`CORE §13.5`) | A very-large-many-core target, rather than canonical header bits spent now |
| Reserved `InternalDebugWire` number | Per-deployment convention (`CORE §7`) | Enough deployments converging on one value to make it worth reserving |
| Header-extension format | ~8-byte target, not byte-exact (`CORE §2.3`) | A Link profile or Transport that actually needs an extension defined |

The pattern in every row is the same, and it is the design rule from `INTRO §3`: the extension waits for a concrete implementation to demonstrate that the current model cannot solve the problem cleanly.

---

# 16. Declared Field Encodings

There is no schema language yet (`REG §6.15`), and the first thing needing one is telemetry (`DEPLOY §3.3`). A candidate vocabulary is recorded here because it is small, and because its interesting parts are semantic rather than syntactic:

```text
UInt<N>            unsigned, codes 0..2^N-1
SInt<N>            two's complement
Bool               one bit; unknown is NOT a third value (CORE §18.5)
Enum<N>            explicit code registry; unassigned codes are reserved and
                   are never silently mapped to a known state
Percent<N>         quantized 0..100%, with declared rounding and clamping
Ratio<N>           quantized over a declared range, e.g. occupied/capacity
SaturatingUInt<N>  stops at its maximum, which means "at least this value"
```

`SaturatingUInt<N>` and the `Enum<N>` rule are the two carrying real weight. A saturating counter whose maximum *means* "at least this" is honest about a bounded field in a way that a wrapping counter is not, and it lets a 4-bit field remain useful during a fault storm. And an enum that maps unassigned codes to a nearest-known state is how a receiver confidently reports a condition that never happened — the same fail-closed reasoning as rejecting nonzero reserved fields (`CORE §2.2`).

`N` is the serialized width, never the width of whatever host type holds the value. Each field definition then has to state its meaning and observation lifetime, width, unit, semantic range, exact quantization and clamping, every reserved and unavailable code, and its bit offset and octet order — including what happens when it crosses an octet boundary. A field missing any of those is not implementable twice, which is the only test that matters. And the general prohibition applies: native bitfields do not define a layout (`BITS §1.1`).

Nothing here is a decision. It is a starting point that has already survived one design pass, offered so that the schema question begins somewhere other than a blank page.

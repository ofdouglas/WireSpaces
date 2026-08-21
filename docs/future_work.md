# WireSpaces — Future Work

**Status:** Not designed. Nothing in this document is a requirement, a commitment, or a specification  
**Purpose:** Hold material that is not yet designed, so `CORE` stays close to what is actually buildable  
**Authority:** None. If this document and `CORE` disagree, `CORE` wins by default

---

# 1. How to Read This Document

Material lives here when both of the following are true:

1. it describes something **not yet designed**; and
2. its absence from the main documents **cannot cause a wrong implementation decision today**.

The second clause is why several things that look like future work are *not* here. Boundary statements — origin failover is not a Wire feature, composition must flatten, redundancy owns six named responsibilities, a constrained LLL must not grow into a transport — stay in `CORE` precisely because removing them would let someone build the thing they exist to prevent. Where a topic needed both, the constraint stayed in `CORE` and the discussion moved here.

Two consequences for anyone generating code or designs from this repository:

> **Do not implement anything in this document** unless a specific instruction says to. It is a record of intent, not a backlog that has been agreed.

> **`REG` is authoritative for status.** If an item appears both as an open question in `REG §6` and as a section here, the `REG` entry is the current state and this document only expands it.

While there is exactly one implementation of WireSpaces, "optional" and "future" collapse into the same thing: not yet built. Both are collected here. If the project ever publishes, they will need separating again, because a published profile has to distinguish "an implementation may omit this" from "nobody can implement this yet."

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

Open: the exact fields and their widths, what the check value covers, whether the counter is per-Endpoint or per-Wire, restart and epoch behavior, the acceptance window, whether a gap is reported to the Service or only counted, and how it interacts with the freshness question in `CORE §21.6`. It also needs a decision about whether it lives in a header extension (`CORE §2.3`) or in Transport metadata, which is the first real test of the extension mechanism.

Note what this does *not* provide: an end-to-end check detects accidental corruption and nothing else. It is not authentication, and anyone modifying a PDU deliberately recomputes it (`CORE §22`).

## 3.2 Reliable and bulk transport

Firmware images, files, logs, and similar data should eventually use a Transport that can:

- segment across multiple PDUs;
- retry missing data;
- bound receiver storage;
- resume where useful.

Receiver windows and credits belong at this level rather than in a Link LLL, which is why Classical CAN nodes are not expected to implement generic Link flow control (`LINK §2.11`).

Other plausible Transports: request/response with retry and correlation, and a command Transport that accepts input from several Origins and selects a source based on health or timeout (`CORE §20.1`).

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

`CORE §23` states which components are likely to be recoverable units. Detailed restart policy is component-specific and undesigned.

The broader project direction favors Link status counters, health reporting, bounded logging, and non-recursive diagnostic failure paths. What is missing is anything concrete about restart ordering, state re-establishment after an LLL or Link Interface restart, whether reassembly state survives, what a Service is entitled to assume about its bindings after a peer Domain restarts, and how a restart is reported.

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

`CORE §21.5` lists the Services that make one PC-connected device immediately useful. A wider catalog has been sketched but not designed:

```text
Identity / device information
Build / version information
Heartbeat / health
Text logs
Structured system events
Link status / traffic counters
Telemetry / scalar snapshots
Crash / fault records
Firmware update
Bulk data transfer
Discovery / enumeration
Lifecycle / reset control
Persistent configuration
Time synchronization
RPC-style utilities
```

Service schemas should eventually have portable, deterministic definitions and useful code generation, but the schema language and toolchain are not chosen. The contract they must express is `CORE §21.3`: a schema over bytes, from which language types are generated as views.

Time synchronization is worth flagging as coupled to an open question elsewhere: freshness representation (`CORE §21.6`) may or may not need a canonical timestamp, and that decision should be made with time sync in view rather than separately.

## 8.1 Promotion criteria and the privileged tail

The catalog is a list of candidates, not a standard library and not an allocation authority. A Service is worth promoting to a published identity when it has a clear semantic boundary, a bounded schema, an explicit privilege model, compatibility metadata, a resource profile, test vectors, and **at least one real deployment** — the last of which is the criterion that stops the catalog from growing faster than the implementation.

Two distinctions found useful in an earlier generation are worth keeping in view when this is designed:

- **Reporting a capability is not enabling it.** A Service that enumerates what a device has configured does not enable anything, create Wires, or grant authority. Nor does a Service that selects among finite precompiled profiles negotiate arbitrary encodings; picking from a fixed list is not negotiation (`LINK-8`).
- **Scalar-prototype conveniences do not scale into protocols.** A generic "read/write a named scalar" Service is genuinely useful for throwaway bring-up, and features graduate out of it the moment they need transactions, timing, richer state, or interaction. Treating it as a permanent substitute for a designed Service is how a deployment ends up with its control interface expressed as untyped key/value writes.

The dangerous tail of the catalog — memory peek/poke, arbitrary register access, fault injection, raw capture and injection, firmware update — is governed by `CORE §22.1`: a separate development build, build-time enablement, and local-only non-forwardable operation. Runtime configuration alone must never enable them.

---

# 9. Remote Maintenance

The same Service model can work across Ethernet, VPN, radio, or other remote Links. Remote maintenance should emphasize retained logs/events, identity/version, health, configuration, resumable firmware/object transfer, and selected telemetry.

Two cautions already apply: high-rate internal Wires should not automatically be mirrored over narrow remote Links, and a remote link should not splice every internal Wire (`CORE §7.4`, `CORE §22`).

Undesigned: everything about authentication and authorization, which is the reason this is future work rather than a near-term feature. WireSpaces provides no security layer, so a remote deployment currently needs an external secure boundary.

---

# 10. High-Throughput Gateway and Analyzer

A compelling eventual demonstration could be an FPGA/softcore embedded router fabric with many CAN FD/XL interfaces, one or more Ethernet links, high-rate WS forwarding, host tapping and packet capture, logic-analyzer capture on selected buses, bus utilization and timing diagnostics, physical-layer diagnostics on selected channels, and an FTDI FIFO or Ethernet connection to WS PC tools.

The device itself could use WS internally between RTL blocks, softcore Services, field buses, and host tools.

This is an aspiration, not a plan. It is recorded because it is the use case that would justify zero-copy (§2), per-Wire top-talker consumption (§15), and CAN XL profiles.

---

# 11. Further Classical CAN Profiles

## 11.1 29-bit identifiers

The 11-bit identifier is a deliberately constrained compatibility floor (`LINK §2`). A future 29-bit Classical CAN profile is the planned escape hatch.

Eighteen additional identifier bits are ample to carry a full 10-bit `WireNumber` directly, and possibly more Endpoint bits, which would retire:

- the seven-named-alias-per-Link limit (`CORE §5.1`);
- the need to spend scarce aliases on transit Wires;
- some of the pressure behind the compact NS0 EID region (`CORE §8.3`);
- the commissioning control-space squeeze (`LINK §2.13`), which exists only because 11 bits are fully spent.

Wire, Direction, NodeId, Endpoint, and Transport semantics would be unchanged; only the projection into the identifier differs. The 11-bit profile remains valuable for the smallest nodes.

**No 29-bit layout is specified**, and none should be inferred from the 11-bit field order.

## 11.2 Guest CAN

`LINK §2.1` establishes the distinction between a **committed** CAN bus, where all 11 identifier bits carry WS meaning, and a **Guest** bus, where WS traffic occupies an explicitly allocated range disjoint from existing legacy owners. Committed CAN is specified; Guest CAN is a named concept with no design behind it.

Everything about it is open: how the allocation is expressed and validated against legacy ownership, how much routing budget survives in a partial identifier range, what payload framing and fragmentation look like with fewer bits, which Transports and QoS behaviors remain available, and what capacity a Service can actually count on.

Two constraints are worth carrying forward, because they are cheap to state now and would be expensive to retrofit:

- **A Guest profile is not a reinterpretation of the committed layout.** It has fewer bits by construction, so it needs its own encoding. Reusing committed field positions in a subset of the identifier space is the tempting shortcut and the one most likely to produce silent aliasing.
- **The allocation must be provably disjoint from every legacy owner on that bus**, in every configured state. This is the same physical arbitration-safety property as `LINK-10`, and on a bus WS does not control it cannot be verified from WS configuration alone. That is a genuine limitation of the family, not an implementation gap.

The 29-bit profile in §11.1 is worth designing first. It is the better answer whenever the controllers involved support it, since a 29-bit WS range coexists with 11-bit legacy traffic without contending for the same identifiers at all.

---

# 12. Cross-WireSpace Identity Translation

`CORE §4.6` establishes that canonical identity is unique within one WireSpace and that two independently engineered WireSpaces do not become one by being connected. A plain forwarding gateway is not a translator, and using one produces silent identity collisions.

What a translating gateway actually looks like is undesigned. It would have to map WireNumbers, NodeIds, and possibly `Namespace + EndpointId` across the boundary, which conflicts directly with the forwarding invariant that only the Wire representation may change (`CORE §3.3`). So it is not a gateway in the current sense at all — it is a distinct component type that re-originates traffic, and it needs its own model of authority, loop prevention, and failure reporting.

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

It becomes interesting again if runtime Service replacement is ever wanted, which is also what the Endpoint lifetime problem in `CORE §9.5` is waiting on.

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
| NodeId-based branch pruning | Flood-and-filter is the baseline (`CORE §12.1`) | Demonstrated bandwidth pressure on a multi-branch Wire |
| Per-Wire congestion signaling | Coarse credit pools accepted (`CORE §15.6`) | A real system where head-of-line coupling is inadequate |
| Link Manager Service | Telemetry exists; no consumer (`CORE §18.5`) | A gateway large enough for automatic policy to beat human diagnosis |
| Intermediate QoS profiles | Only Minimal and Full standardized (`CORE §14.1`) | Implementation experience showing a real need for `Normal+Background` |
| Extended internal-Wire profile | 127 device-private Wires assumed ample (`CORE §4.4`) | An implementation genuinely needing hundreds of internal Wires |
| Many-core profile | No special assumptions (`CORE §13.5`) | A very-large-many-core target, rather than canonical header bits spent now |
| Reserved `InternalDebugWire` number | Per-deployment convention (`CORE §7.4`) | Enough deployments converging on one value to make it worth reserving |
| Header-extension format | ~8-byte target, not byte-exact (`CORE §2.3`) | A Link profile or Transport that actually needs an extension defined |

The pattern in every row is the same, and it is the design rule from `INTRO §3`: the extension waits for a concrete implementation to demonstrate that the current model cannot solve the problem cleanly.

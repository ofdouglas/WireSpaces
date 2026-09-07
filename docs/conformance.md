# WireSpaces — Conformance and Test Strategy

**Status:** Private first draft; vectors are a project requirement, not yet published
**Purpose:** Reference cases, boundary tests, and exit criteria for provisional status
**Authority:** Test policy only. Protocol behavior belongs to `CORE`; byte encodings belong to `LINK`

Cross-references use the document code plus a section number, for example `CONFORM §2.1`. A bare `§x` always means the current document.

---

# 1. Why Conformance Vectors Exist

WireSpaces is intended to span C++ firmware, host software, Python tooling, and RTL. Those are separate implementations *within one project*, and they will diverge from each other without shared reference cases, so **conformance vectors should be created early** even though no external implementer exists.

Useful vectors:

- provisional six-byte canonical PDU descriptor encode/decode examples;
- canonical WireNumber, source ParticipantId, destination ParticipantId, and Endpoint boundaries;
- splice application on ingress and egress, including rejection of `kLocalBus` splices and device-private egress without a splice;
- Wire-member topology examples with expected propagation masks;
- canonical local-only `kLocalBus` behavior;
- Classical CAN PDUA fragmentation/reassembly sequences;
- generation-wrap/stale-fragment tests;
- CRC golden vectors for both CRC-8 and CRC-16 cases;
- per-N capacity boundaries from `LINK §2.11`, including oversize rejection before TX;
- Endpoint dispatch examples;
- congestion/send-result behavior;
- Link telemetry snapshot examples.

This helps prevent software and RTL implementations from quietly becoming different dialects. The simulator MVP should implement a subset of these vectors; see the [simulator README](../sim/README.md).

One check is not a vector at all but belongs with them. Because the Endpoint API is a portability contract rather than an implementation detail (`SVC-9`), the strongest test of it is to **compile one unmodified Service against two implementations** and run the same behavioral cases against both. Golden vectors verify that implementations agree on bytes; only this verifies that they agree on the surface Services are written against. It is worth doing as soon as a second implementation exists, since API divergence is cheap to fix early and expensive once Services depend on it.

## 1.1 A prototype is an evidence generator, not a source of architecture

The first implementation will encounter every open question in `REG §6` and will have to do *something* at each one. That is fine and unavoidable. What is not fine is the default consequence:

> **An implementation choice does not close a specification gap, and does not acquire the status of a decision by being shipped.**

Left unstated, this fails in a specific and familiar way. Someone picks a reassembly timeout because the code needs a number; six months later it is in three implementations and a test suite, and the question "what should the timeout be, and why" has been answered by nobody while becoming expensive to reopen. The same path turns a placeholder CRC polynomial into a compatibility constraint.

Two habits are enough to prevent it. Anything chosen to make code run rather than because it was decided is **labeled provisional in the code, the vectors, and any report** — the open item in `REG §6` stays open and gains a note about what the prototype happens to do. And a test asserting a provisional value is understood to be pinning current behavior for regression purposes, not ratifying it.

The inverse error is worth naming too: the prototype is the *only* thing that can answer several of those questions, because they are measurement questions rather than design ones (§3). The point is not to defer to the documents, it is to keep straight which kind of question is being answered.

---

# 2. Test at the Boundaries, Not in the Middle

Vectors that use comfortable middle values prove very little. Every field with an allocation fence or a width limit should be exercised at the value on each side of the fence, and the expected result stated — exact reconstruction, or fail-closed rejection, never a silent remap.

The provisional canonical descriptor is six bytes:

```text
Byte 0      Control
Byte 1      WireNumber
Byte 2      SrcParticipantId
Byte 3      DestParticipantId
Bytes 4-5   Endpoint, little-endian
```

Its literal vectors remain provisional until the representation freeze, but the suite must already pin these boundaries:

| Field | Values worth pinning |
|---|---|
| descriptor length | `5` rejected, `6` exact, `7` rejected unless consumed as a separately defined following field |
| `WireNumber` (8-bit) | `0x00`, `0x01`, `0xFE`, `0xFF`, every assigned allocation fence, and the assigned canonical `kLocalBus` value |
| source `ParticipantId` (8-bit) | `0x00`, `0x01`, `0xFE`; `0xFF` rejected as source |
| destination `ParticipantId` (8-bit) | `0x00`, `0x01`, `0xFE`, `0xFF` as Wire-wide broadcast |
| Endpoint `Namespace` | all four 2-bit values |
| Endpoint `Id` (14-bit) | `0` rejected, `1`, `0x3FFF`, and one-past-width rejected before emission |
| `QoS` | all four values, plus the ordering they imply on a contended Link |
| reserved Control bits | zero accepted; each nonzero pattern rejected and counted |
| PDUA frame count | `N = 1, 2, 4`, maximum, one past maximum |
| Payload length | `0`, maximum for each `N`, one past maximum |
| Generation counter | wrap boundary, and a stale fragment from the previous generation |

No Link-profile optimization changes those canonical limits. A value that cannot be represented by the selected Link Binding rejects before a frame is emitted rather than truncating, remapping, or changing canonical identity.

Negative cases deserve equal weight: missing, duplicated, swapped, skipped, delayed, replayed, malformed, short, and overlong fragments; unexpected START; reassembly timeout; context-pool exhaustion; unknown Wire; unknown Participant; unknown Endpoint; invalid Link-local Direction; wrong ingress; unsupported TransportType; oversize PDU. The required outcome for all of them is identical and is the property most worth protecting: **counted, dropped, no partial delivery, no error response** (`CORE §18.1`).

Interleaving and isolation cases are worth their own group, because they are where implementations quietly cheat: concurrent reassembly on distinct identifiers, attempted interleaving on one identifier, a full context pool, and no cross-Link or cross-context assembly.

Several behaviors are not about encoding at all and need their own cases, because each one is a place where an implementation can be locally correct and still wrong:

```text
serialization        little-endian literal values verified on a big-endian
                     implementation model as well as a little-endian one
parser progress      a byte-stream parser fed the same PDU under arbitrary
                     chunk boundaries, including one byte at a time
ownership            ownership retained by the caller on every rejection
                     reason, and released after every acceptance
terminal outcome     every accepted PDU reaching exactly one of complete,
                     cancelled, or faulted
restart              restart with transmit and reassembly work in flight,
                     and application-owned receive buffers surviving it
storage classes      snapshot replacement distinguished from queue overflow;
                     value-copy distinguished from ownership transfer
delivery boundary     no Service code reached from a Link or dispatch context,
                     including when a Service transmits during a receive
concurrent producers  one receive Endpoint fed from several Link-driver
                     contexts, and an exclusive Endpoint with the
                     synchronization it does not need omitted
snapshot coherence    a Snapshot fed far faster than its consumer yielding
                     coherent whole values and an advancing generation
snapshot TX feedback  an unbound or unscheduled Snapshot transmit Endpoint
                     detected by a generation echo that never advances,
                     and modular comparison correct across wrap
multiplicity          two Services rejected on one Queue Endpoint or one
                     transmit Endpoint; several accepted on a Snapshot,
                     each tracking its own watermark independently
metadata              declared source, class, extension, and arrival fields
                     readable after the ingress buffer has been reused,
                     and an undeclared field unavailable rather than zero
source authority      a Service unable to transmit to a peer merely because
                     it read that peer's Wire and ParticipantId from metadata
exhaustion           queue and pool exhaustion producing bounded work and no
                     corruption, not just a counter increment
projections          generated software and RTL/static projections producing
                     identical canonical values, and failing closed when they
                     disagree
binding modes        every enabled mode, including reply contexts that are
                     rejected after expiry or reuse
```

The parser-chunking case earns its place: a framing bug that only appears when a PDU straddles two reads is invisible to any test that hands the parser whole messages, and it is the default behavior of a real UART.

## 2.1 Logical-Bus and identity semantics

These are topology tests, not codec tests. Each case states both the Links on which the PDU is logically propagated and the Participants that accept it:

```text
directed propagation  a directed PDU traverses every configured segment of
                      its loop-free Wire; only DestParticipantId accepts it
broadcast             DestParticipantId 0xFF is accepted by every Participant
                      on that Wire that implements the Endpoint
destination routing   changing only destination acceptance does not change
                      ordinary Wire propagation masks
overlapping Wires     broad and narrow Wires sharing Links remain distinct
                      and each uses only its own member-Link topology
loops                 every ordinary cyclic Wire realization is rejected by
                      configuration validation before installation
lineage               heterogeneous forwarding preserves canonical Wire,
                      source, destination, Endpoint, control, and payload
composition           a component that consumes and authors another PDU
                      becomes the source of the new PDU
identity universe     plain forwarding is accepted only within one coordinated
                      ParticipantId universe; inconsistent universes reject
splice                the configured Wire scope changes while source,
                      destination, Endpoint, metadata, and payload remain;
                      a PID collision is not silently resolved
```

Two Links carrying the same Wire may use different participant projections; both must reconstruct the same canonical identities before generic propagation or dispatch.

`kLocalBus` has dedicated cases: canonical ingress reconstructs `Wire = kLocalBus` plus canonical source and destination; local dispatch succeeds; transparent forwarding and splicing as `kLocalBus` reject; and a second local Link Binding in the same Router/Endpoint Domain rejects as ambiguous.

## 2.2 CAN11 static-profile cases

Every CAN11 Link Binding test selects exactly one Wire and exactly one of `Guest VCN`, `Native VCN`, or `Native Participant-Compressed`. A second Wire, no profile, multiple profiles, or configuration/artifacts for a different profile reject before traffic is enabled.

VCN-map configuration cases cover a self-pair rejection, duplicate VCNs for the same unordered participant pair, and duplicate broadcast VCNs for one source Participant. For both Guest and Native VCN, vectors cover A-to-B and B-to-A Direction reconstruction, a valid Participant-to-broadcast direction, rejection of the reverse broadcast direction, and classification of the reserved all-ones VCN as Link control rather than ordinary traffic.

Guest VCN adds:

```text
allocation validity   exactly one aligned contiguous 16-ID block,
                      in-range end point, explicit bus-owner reservation,
                      and rejection of every invalid allocation
range classification  first and last allocated CAN IDs classified as WS;
                      IDs immediately outside ignored as non-WS
fixed QoS             ingress reconstructs the configured canonical QoS;
                      TX requesting any different QoS rejects
```

Native VCN covers every QoS value, VCN boundaries, both Directions, broadcast, and VCN maps whose canonical ParticipantIds are above the directly compact-representable range.

Native Participant-Compressed covers direct and projected mappings; the requirement that every ordinary addressed pair include at least one participant in the compact field; rejection of general/general pairs; deterministic encoding when both participants fit the compact field; and general code `31` classified as broadcast in the valid compact-source Direction and as Link control in the opposite Direction. Reserved control combinations never enter ordinary forwarding or dispatch.

For all profiles, same-CAN-ID traffic proves complete-PDU noninterleaving: a second PDU on an identifier cannot interleave with an incomplete first PDU and produce mixed delivery.

Commissioning protocol details, `PduControl`, `N = 1` packing, CRC choices, and exact CAN29 vectors remain explicitly provisional. Regression vectors may pin current prototype behavior, but do not freeze those designs.

## 2.3 Lifecycle and restart cases

Restart behavior (`CORE §23`) needs its own group, because almost none of it is reachable from a vector file and all of it is reachable from a field failure:

```text
planned stop         a Link disabled and re-enabled without ever entering
                     a fault state, and a redundant request either idempotent
                     or explicitly rejected
in-flight work       restart with transmit accepted, reassembly partial,
                     timers pending, and queues full - every accepted
                     transmit reaching exactly one terminal outcome
bounded quiesce      stop against a driver that never completes, hitting the
                     deadline and forcing cancellation rather than hanging
generation           counters and high-water marks not compared across a
                     runtime generation change, and a pre-restart handle
                     rejected rather than honored on a reused slot
retained buffers     application-owned receive buffers surviving restart,
                     released safely afterwards, and the resulting temporary
                     capacity reduction bounded and counted
sibling isolation    a healthy Link continuing through another's restart,
                     wherever configuration claims they are independent
reset boundary       latched fault and restart evidence surviving restart,
                     generation-scoped counters resetting, and each obeying
                     its declared boundary rather than a convenient one
supervision          a hung restart unit detected from outside it, and an
                     idle Link with no traffic not reported as faulted
escalation           repeated fault injection producing bounded records,
                     bounded restart attempts, and no diagnostic storm
telemetry lifetime   live status becoming unavailable while latched status
                     stays readable; a faulted Link's status published over
                     a healthy one; no torn mixed-generation snapshot
unavailable fields   a not-applicable field distinguishable from zero, on a
                     Link with no flow control and on shared QoS queues
```

Two of these deserve emphasis because they are the ones most often skipped. Testing the *planned* stop path matters because an implementation that only ever exercises fault-driven recovery tends to have no working clean shutdown, and discovers it during a firmware update. And the diagnostic-storm case is the one where a correct-in-isolation Participant becomes the bus's problem — a restart loop that reports each attempt is worse than the fault it is reporting.

---

# 3. What a Vector Suite Cannot Tell You

Golden vectors prove encoding agreement. They say nothing about whether an implementation fits its target, so system-level measurement is a separate obligation: worst-case RAM and execution time, queue occupancy, end-to-end latency, medium load, scheduler response, and counter behavior under a sustained fault storm.

Three measurements are specific to the bounded delivery boundary (`CORE §9.4`) and are the ones a skeptic will actually ask for:

- **added end-to-end latency on a control path, under both main-loop orderings.** Servicing Links and then draining Endpoints in the same iteration costs roughly a copy; draining before servicing costs a full cycle. Ordering sensitivity is a failure mode the storage boundary introduces, and it deserves measurement rather than discovery.
- **worst-case serialization hold time** for a bounded Queue write and a Snapshot write, against the platform's interrupt-latency requirement (`CORE §1.5`).
- **Endpoint storage RAM across a realistic Service set**, since depth is per Endpoint and the total is what decides whether a design fits a constrained part (`CORE §9.5`).

On an AMP target or a simulation of one, it is also worth demonstrating that inter-core delivery genuinely goes through a Link between separate Endpoint Domains rather than through shared Endpoint internals, and that relocating two Services onto one core does not turn their exchange into synchronous Service-to-Service execution.

One effect is specific enough to name. **Fragmentation amplifies loss.** An `N`-fragment PDU is lost if any one of its fragments is lost, so at a per-frame loss rate `p` the PDU loss rate is roughly `1 - (1-p)^N` — about `N * p` for small `p`. A 1% frame loss rate becomes an 8% PDU loss rate at `N = 8`. This is a strong argument for keeping `N` small on lossy media and for measuring PDU-level rather than frame-level loss, and it is a reason a Service should not treat a large fragmented PDU as being as dependable as a small one on the same Link.

## 3.1 Load profiles

Steady-state throughput is the least informative load to measure, and the easiest. Four others are where designs actually fail:

```text
startup         everything initializing, tables loading, peers not yet up,
                and periodic Services all first firing in the same window
degraded        a Link down, a peer silent, reassembly timing out
diagnostic burst telemetry, drop journaling, and fault records at full rate
error storm     sustained malformed or unauthorized traffic from a babbling
                source, with every rejection counted
```

The specific property worth asserting under the last two is that **diagnostics cannot starve control**: configured control traffic still makes its deadline under the selected QoS discipline while diagnostics run at their maximum admitted rate. A system whose telemetry can suppress its own control path has inverted its priorities, and this is easy to build accidentally, since diagnostics are the traffic that scales with how badly things are going.

## 3.2 Budgets are inputs, not outputs

> **A resource budget is frozen before the run that measures against it.**

The reason is uncomfortable but reliable: a budget written after the measurement is a description, and it will accommodate whatever was measured. Reversing the order is what makes an overrun visible as a failure rather than as a new baseline.

What is worth reporting per configuration, since a single "RAM used" figure hides the decisions:

```text
RAM         static and peak, broken out by generated tables, Endpoint
            storage, pools, queues, reassembly contexts, and telemetry
stack       worst case, plus any initialization-only allocation
flash       by layer - core, Link profile, codecs, Services
bandwidth   encoded bytes, per-PDU overhead, cadence, resulting Link load
latency     distributions rather than averages, per stage and end to end
work        copies, atomics, critical-section time, cache maintenance,
            DMA transitions, scheduler wakeups
CPU         at idle, normal, worst admitted, restart, and error-storm load
```

One property is checkable rather than merely measurable: **no dynamic allocation on any steady-state path**. Initialization may allocate; the running system may not, and a test can assert this directly by failing the allocator after setup.

Counters need their own discipline, since they are the primary evidence for everything above. Each one declares width, unit, saturation or wrap behavior, the exact point it increments, and its reset boundary (`CORE §23.7`) — and a test asserts the *expected delta*, not merely that the counter exists. A counter nobody has predicted the value of is decoration.

## 3.3 Freeze the topology corpus before measuring headroom

The 8-bit ParticipantId and WireNumber widths have a pre-freeze corpus gate. The representative topology corpus is named, versioned, and frozen before measuring either width; changing the corpus after seeing poor margin starts a new measurement rather than repairing the result.

The corpus includes:

```text
multicore/internal Endpoint Domains
redundant controllers
gateways and several CAN buses
overlapping broad/narrow Wires
device-private and debug/platform Wires
local/sentinel reservations
plausible product growth
```

For every topology, record peak ParticipantId consumption, peak WireNumber consumption, reserved/private allocation cost, and remaining growth margin. Passing means demonstrating useful headroom across the frozen corpus, not merely fitting one current product sketch. Poor margin blocks the representation freeze and requires revisiting allocation.

---

# 4. Capability Claims and Reason Categories

Targets range from an 8-bit MCU Participant to a Linux gateway, so a single pass/fail suite would either exclude the small targets or test nothing. The resolution is to make the claim explicit and separate from the result.

Each configuration declares every optional capability as one of:

```text
required            must work; failure fails the suite
optional-enabled    claimed and enabled; its tests must run and pass
optional-disabled   supported but off here; tests not run, claim not made
unsupported         not present on this target; recorded as an expected gap
```

> **A capability may be claimed only if its tests actually ran.** A target that declares something unsupported is not failed for omitting the test — and does not inherit the claim either.

The failure mode this prevents is the silently skipped success: a suite that reports green because the flow-control tests found no flow control and returned early. Capabilities worth declaring this way include credit-based flow control, promiscuous observation, zero-copy receive, DMA involvement, hardware timestamps, multi-mailbox CAN transmit with priority-aware selection and safe cancellation, non-coherent multicore operation, out-of-band debug (`CORE §23.9`), and independently restartable sibling Links (`CORE §23.6`).

The same explicitness applies to rejections. Every drop and refusal is counted (`ERR-1`), and the counters are only useful if the categories are stable:

> **Rejection reasons come from a fixed registry, and a test asserts the reason, not merely that something was rejected.**

Otherwise a test passes for the wrong cause — an oversize PDU rejected as an unknown Wire looks identical from outside, and the bug surfaces later as traffic that mysteriously fails to route. The registry needs to be an enumeration with a declared width and no catch-all bucket large enough to hide in, and it is shared between the implementation, the telemetry schema (`REG §6.7`), and the vectors.

---

# 5. Staged Freezes

Decisions have dependencies, and the expensive mistake is building on one that has not been made. It is worth grouping the open items into stages where each stage is settled before the work that depends on it starts — not to add process, but because these are the seams where rework is cheap on one side and expensive on the other.

| Stage | Settled before | Contents |
|---|---|---|
| Representation | any codec is written | provisional six-byte descriptor layout, Wire8/PID8/Endpoint14 allocation fences, canonical `kLocalBus` allocation, topology-corpus headroom, little-endian rule |
| Endpoint API | any Service is written | Queue and Snapshot vocabulary, capacity declaration, metadata declaration, transmit Endpoint shape, decoded-representation naming (`REG §6.12`) |
| Storage and ownership | concurrency is optimized | ownership transitions on every accept and reject path, memory ordering for Snapshot publication, ISR and cross-core rules |
| Congestion and QoS | load testing means anything | send-result vocabulary, per-Endpoint capacities, drop and replacement policy, QoS discipline parameters |
| Lifecycle and telemetry | a supervisor or host tool is written | lifecycle operations, runtime generation width, reset boundaries, reason registry, counter registry, telemetry schema class |
| CAN profile | any device ships on a shared bus | identifier field positions, CRC parameters, DLC and padding, reassembly timeout, generation and reset rules, commissioning control protocol/details |

Two things follow from the ordering. Later stages can proceed with earlier ones provisional as long as the provisional status is labeled (§1.1) — the table describes where rework concentrates, not a gate that blocks all work. And the CAN row is last for a reason unrelated to difficulty: it is the only one whose mistakes are visible to other devices, so it is the only one where being wrong costs a coordinated update rather than a recompile.

---

# 6. Exit Criteria for Provisional Status

This architecture and its Link profiles are provisional. Recording what "no longer provisional" requires is useful now, because it keeps the label from becoming permanent by default. A Link profile may drop the provisional label when:

1. a named, versioned profile fixes every identifier, byte, bit, length, padding, generation, timeout, reset, malformed-input, and state-transition rule;
2. the integrity contract is byte-exact and its choice is justified, not merely stated;
3. generated tooling proves representability, transmit ownership in every configured state, authority correctness, resource bounds, and profile compatibility;
4. positive, boundary, and negative vectors cover every encoding, all canonical reconstruction, capacity limits, faults, and lifecycle behavior;
5. bounded RAM, CPU, queues, timers, and diagnostics are demonstrated on a genuinely constrained target rather than a development board;
6. QoS mapping, scheduling, and deployment timing analysis are validated on real hardware;
7. authority and observation boundaries behave as specified — configured recipients, passive observation, whole-PDU forwarding, re-origination, and commissioning phases;
8. two independent implementations produce byte-identical output and matching accept/reject decisions.

Until then, no wire interoperability is claimed, and independently developed implementations must not be presumed compatible: byte-exact encodings, CRC parameters, CAN identifier ordering, and the TransportType registry are all still open.

Host simulation should be a first-class development path. Local UDP, virtual CAN, PTYs, shared-memory channels, and simulated Links can exercise most of the architecture before hardware is available.

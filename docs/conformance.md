# WireSpaces — Conformance and Test Strategy

**Status:** Private first draft; vectors are a project requirement, not yet published  
**Purpose:** Reference cases, boundary tests, and exit criteria for provisional status  
**Authority:** Test policy only. Protocol behavior belongs to `CORE`; byte encodings belong to `LINK`

Cross-references use the document code plus a section number, for example `CONFORM §2.1`. A bare `§x` always means the current document.

---

# 1. Why Conformance Vectors Exist

WireSpaces is intended to span C++ firmware, host software, Python tooling, and RTL. Those are separate implementations *within one project*, and they will diverge from each other without shared reference cases, so **conformance vectors should be created early** even though no external implementer exists.

Useful vectors:

- canonical PDU descriptor encode/decode examples;
- WireAlias canonicalization examples;
- splice application on ingress and egress, including rejection of anonymous LocalBus splices and device-private egress without a splice;
- Router table examples with expected egress masks;
- LocalBus configured/unconfigured behavior;
- Classical CAN PDUA fragmentation/reassembly sequences;
- generation-wrap/stale-fragment tests;
- CRC golden vectors for both CRC-8 and CRC-16 cases;
- per-N capacity boundaries from `LINK §2.10`, including oversize rejection before TX;
- Endpoint dispatch examples;
- congestion/send-result behavior;
- Link telemetry snapshot examples.

This helps prevent software and RTL implementations from quietly becoming different dialects. The simulator MVP should implement a subset of these vectors; see `code/sim_rfp.md`.

One check is not a vector at all but belongs with them. Because the Endpoint API is a portability contract rather than an implementation detail (`SVC-9`), the strongest test of it is to **compile one unmodified Service against two implementations** and run the same behavioral cases against both. Golden vectors verify that implementations agree on bytes; only this verifies that they agree on the surface Services are written against. It is worth doing as soon as a second implementation exists, since API divergence is cheap to fix early and expensive once Services depend on it.

---

# 2. Test at the Boundaries, Not in the Middle

Vectors that use comfortable middle values prove very little. Every field with an allocation fence or a width limit should be exercised at the value on each side of the fence, and the expected result stated — exact reconstruction, or fail-closed rejection, never a silent remap.

| Field | Values worth pinning |
|---|---|
| `EndpointId` (Namespace 0) | `0, 1, 31, 32, 127, 128, 1023, 1024, 65535` |
| `WireNumber` | `0, 1`, last shared, first device-private, last device-private |
| `WireAlias` | `0` (`kLocalBus`), `1`, maximum for the profile |
| `NodeId` | `0` (broadcast), `1`, maximum, one past maximum |
| `QoS` | all four values, plus the ordering they imply on a contended Link |
| `Namespace` | all four values, including ones with no allocation policy yet |
| PDUA frame count | `N = 1, 2, 4`, maximum, one past maximum |
| Payload length | `0`, maximum for each `N`, one past maximum |
| Generation counter | wrap boundary, and a stale fragment from the previous generation |

The fenced Endpoint values matter most. `127/128` crosses the optimized-encoding boundary, and `1023/1024` crosses representability on 11-bit CAN — both must reject before a frame is emitted (`LINK §2.3`), not truncate.

Negative cases deserve equal weight: missing, duplicated, swapped, skipped, delayed, replayed, malformed, short, and overlong fragments; unexpected START; reassembly timeout; context-pool exhaustion; unknown Wire; unknown Endpoint; wrong Direction; wrong ingress; unsupported TransportType; oversize PDU. The required outcome for all of them is identical and is the property most worth protecting: **counted, dropped, no partial delivery, no error response** (`CORE §18.1`).

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
                     it read that peer's Wire and NodeId from metadata
exhaustion           queue and pool exhaustion producing bounded work and no
                     corruption, not just a counter increment
projections          generated software and RTL/static projections producing
                     identical canonical values, and failing closed when they
                     disagree
binding modes        every enabled mode, including reply contexts that are
                     rejected after expiry or reuse
```

The parser-chunking case earns its place: a framing bug that only appears when a PDU straddles two reads is invisible to any test that hands the parser whole messages, and it is the default behavior of a real UART.

---

# 3. What a Vector Suite Cannot Tell You

Golden vectors prove encoding agreement. They say nothing about whether an implementation fits its target, so system-level measurement is a separate obligation: worst-case RAM and execution time, queue occupancy, end-to-end latency, medium load, scheduler response, and counter behavior under a sustained fault storm.

Three measurements are specific to the bounded delivery boundary (`CORE §9.4`) and are the ones a skeptic will actually ask for:

- **added end-to-end latency on a control path, under both main-loop orderings.** Servicing Links and then draining Endpoints in the same iteration costs roughly a copy; draining before servicing costs a full cycle. Ordering sensitivity is a failure mode the storage boundary introduces, and it deserves measurement rather than discovery.
- **worst-case serialization hold time** for a bounded Queue write and a Snapshot write, against the platform's interrupt-latency requirement (`CORE §1.5`).
- **Endpoint storage RAM across a realistic Service set**, since depth is per Endpoint and the total is what decides whether a design fits a constrained part (`CORE §9.5`).

On an AMP target or a simulation of one, it is also worth demonstrating that inter-core delivery genuinely goes through a Link between separate Endpoint Domains rather than through shared Endpoint internals, and that relocating two Services onto one core does not turn their exchange into synchronous Service-to-Service execution.

One effect is specific enough to name. **Fragmentation amplifies loss.** An `N`-fragment PDU is lost if any one of its fragments is lost, so at a per-frame loss rate `p` the PDU loss rate is roughly `1 - (1-p)^N` — about `N * p` for small `p`. A 1% frame loss rate becomes an 8% PDU loss rate at `N = 8`. This is a strong argument for keeping `N` small on lossy media and for measuring PDU-level rather than frame-level loss, and it is a reason a Service should not treat a large fragmented PDU as being as dependable as a small one on the same Link.

---

# 4. Exit Criteria for Provisional Status

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

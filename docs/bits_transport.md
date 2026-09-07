# WireSpaces — BITS: Binary Image Transport, Segmented

**Status:** Main-set prototype design; no byte-exact interoperability or TransportType allocation is frozen<br>
**Purpose:** Define bounded, point-to-point finite-object transfer and its associated unreliable sideband<br>
**Authority:** This document owns BITS-specific behavior and candidate encodings. `CORE` owns shared identity, routing, ownership, and execution rules; `LINK` owns carrier adaptation; `REG` owns status.<br>
**Document code:** `BITS-TRANSPORT`

Cross-references use document codes and section numbers. `BITS` continues to mean [bit_layout.md](bit_layout.md); it is not this document's code. A bare `§x` refers to this document.

## 1. Scope and maturity

BITS moves one finite binary object reliably between two configured WS Hosts. It is one TransportType and Transport Entity with two related message classes: a reliable segmented object channel and an optional small **unreliable** user-datagram sideband. It is suitable for firmware/FPGA images, calibration blobs, diagnostic snapshots, crash dumps, captured traces, and finite log batches.

The current design is incorporated from [the updated BITS proposal](proposed/BITS_design_updated.md). That source remains provenance. This document consolidates its behavior and records the boundaries needed to implement a bounded prototype; §15 lists what still prevents protocol freeze. A prototype must label every unresolved choice and must not turn a working encoding into an interoperability claim (`CONFORM §1.1`).

| Incorporated direction | Still provisional or unresolved |
|---|---|
| One configured connection and at most one active object session per instance | Session-ID reuse, restart association, and completed-session retention |
| Separate segment storage and one shared control/sideband FIFO; deferred processing | Concrete APIs, queue depths, timing budgets, and async sink-result representation |
| Absolute segment indices, cumulative/selective ACKs, capacity-backed grants, retry and PROBE | Exact ACK encoding and safe discrimination of delayed ACKs after sequence wrap |
| Compact SETUP and SEGMENT field layouts below | Control-byte allocation, message codes, TransportType number, Extended encoding |
| Sink acceptance precedes acknowledged receipt; duplicate suppression within retained session state | Completion/close handshake, abort, persistent resume, and durability contract |
| No BITS magic or per-message CRC in the current direction | Any end-to-end wrapper, stale-packet lifetime guarantee, and Service verification scheme |

Reliability here is recovery from loss, duplication, and reordering within the declared session and lifetime assumptions. It is not unconditional delivery: exhausted retries, unavailable resources, sink failure, or a restarted peer can terminate a transfer. It does not imply timely delivery, authentication, byte-stream semantics, or exactly-once application effects across reset.

### 1.1 Responsibility split

BITS owns setup, segment numbering, bounded receiver flow control, cumulative/selective ACK state, retransmission, duplicate handling, out-of-order transfer, window probing, timers, and transfer progress. It does not own image interpretation, flash layout, erase policy, signatures, activation, rollback, filesystem naming, or authentication/authorization.

A Service owns the object format and source/sink policy. A bootloader owns its boot state machine, regions, compatibility checks, verification and activation. A Log Service owns batching, retention and overflow. BITS is not a reliable RPC channel, indefinite stream, shell, or ordered event transport. The earlier public composition `ReliableSegmented + SimpleUnreliable` is retired; internal code reuse does not require a second wire-level TransportType.

## 2. Connection, Endpoint, and authority

Each conceptual `BitsTransmitter` or `BitsReceiver` represents one point-to-point connection:

```text
configured WireNumber
configured local HostId
configured remote HostId
owning Endpoint identity and its authorized bindings
```

Transmitter/receiver names describe the direction of the **object**. Both roles exchange protocol control and may send/receive user datagrams. The common embedded configuration is fixed for the instance's lifetime. Broadcast object transfer is outside this design; both HostIds are ordinary directed identities.

Before retaining an incoming packet, verify the configured Wire, remote source, local destination, Endpoint, and BITS TransportType. Outgoing packets use the configured local source and remote destination. Incoming metadata does not grant reply authority (`DISP-15`): ACK, PROBE, REJECT, and sideband TX must be authorized by the Endpoint's configured transmit/reply bindings (`CORE §10`). A packet from another peer cannot create a connection or redirect replies.

One Endpoint exposes one Transport Entity boundary (`TRN-1`). A central Endpoint may contain a statically bounded mux/bank of 1:1 connection instances. Classification selects exactly one configured instance; it never allocates an unbounded session table. N concurrent connections cost N sets of state and declared storage. A Service may use several Endpoints when its interfaces require them.

A connection may be rebound only when no transfer is active. Idle alone does not prove safe reuse: queued ingress, pending TX, retained references, and stale-session association must also be resolved (§8, §12). The rebind API and stale-traffic exclusion mechanism remain open. No active-transfer peer changes or runtime discovery are implied.

## 3. Receive storage and execution

### 3.1 One receive boundary, two workstreams

The receive hook performs only bounded classification and retention:

```text
complete canonical WS PDU, matching configured connection
    -> validate length before reading the fixed BITS prefix
    -> supported version/profile/type, structurally safe to retain
        SEGMENT       -> bounded non-overwriting segment mailbox/queue
        other BITS    -> one bounded FIFO for protocol control + USER_DATAGRAM
    -> copy or transfer an explicitly owned reference; count outcome; return
```

The non-segment FIFO carries SETUP, ACK, PROBE, REJECT and user sideband messages in one serial ingress order. There is no separate priority queue for protocol control. Segment storage is either a depth-1 non-overwriting mailbox or a bounded queue. It is not a latest-value Snapshot: replacing an accepted segment silently loses transfer data.

Both elements declare capacity, writer concurrency, ownership, and exhaustion behavior (`DISP-16`). In the bounded prototype, full storage rejects the arriving item and counts the loss; it never overwrites accepted work or semantically coalesces ACKs/control to make room. Lost protocol messages recover through the sender's retry/probe policy. Sideband loss is not repaired by BITS. Queue depth and load limits must account for sideband competing with control.

The receive hook does not interpret ACKs or duplicates, advance a window, run SETUP/completion transitions or retries, invoke Service code, or program flash. The source proposal's direct-to-flash receive-hook optimization is not adopted. The sink may use final destination storage later in the Transport execution context (§13).

Acceptance into ingress storage **does not mean acceptance by the object sink** and does not make a segment ACKable. A full mailbox never records a received bit. The Transport Entity is the sole consumer of each protocol ingress element on behalf of its Service; the Service does not independently drain the same FIFO (`DISP-10`).

### 3.2 Copy and reference ownership

Copies remain the baseline. A reference path transfers an explicit owned handle or bounded immutable lease valid through later processing, including metadata. A raw pointer/span into reclaimed Link storage is not retention (`DISP-17`, `CORE §16`). Rejection preserves caller ownership; accepted references have a declared release point on consumption, cancellation, or failure.

A copying sink may consume a packet from a reference domain, copy the bytes it needs, then release the reference. Callback spans are scoped borrows unless the API explicitly transfers ownership. Retaining a reference does not relax queue ordering, capacity, concurrency, or full behavior. Ownership across restart, DMA/cache coherence, fan-out and handle reuse still requires the platform contract; this document does not select a universal allocator or reference-counting API.

### 3.3 Bounded processing

The owning task or superloop serially calls a conceptual `process()`:

```text
at most one segment ingress item
at most one non-segment FIFO item
one bounded state-machine/timer/TX step
```

Exact ordering and result enums remain API choices. One call never drains an arbitrary queue or emits an unbounded retry burst. Implementations declare callback work, pending-operation storage and TX-attempt bounds as part of the call's execution budget. If a sink operation completes asynchronously, its retained item and completion association are bounded; pending is not success.

Protocol state transitions, ACK coalescing and user callbacks happen here or in a later declared owning context, never in Link RX/dispatch. Calling `process()` repeatedly can increase throughput, but fairness to other work belongs to the scheduler. Progress includes timer and TX work, not only consumed RX packets.

The shared FIFO orders only its accepted non-segment messages. It does not establish a total order against the segment workstream, across different connections, or across network loss/reordering. A sender waits for accepted SETUP state before sending object data. Service commands needing “after transfer” semantics must test transfer state or use an explicit Service handshake; FIFO placement alone is insufficient.

## 4. Profiles and common envelope

The canonical descriptor's BITS TransportType number remains unallocated. BITS profiles share that one TransportType and a profile-independent Service contract. Sender and receiver must be configured for compatible supported profiles; an unknown profile is not an invitation to guess a layout.

| Field/resource | Compact prototype | Extended direction |
|---|---:|---:|
| Session ID | 8 bits | 16 bits |
| Sequence-number space | 8 bits, modulo 256 | 16 bits, modulo 65,536 |
| Receive bitmap | 16 bits / 16 positions | 32 bits / 32 positions |
| Absolute segment index | 16 bits | likely 32 bits; unresolved |

Every BITS message begins with one fixed-format control byte containing **protocol version, BITS profile, and message type**. The profile must be readable before any profile-dependent body. Exact bit positions, numeric values and reserved patterns are open; the idea of a four-bit type field is not a frozen allocation.

Expected classes are SETUP, SEGMENT, ACK, PROBE, REJECT, and USER_DATAGRAM. ABORT and COMPLETE/CLOSE remain undecided. There is no per-message magic: WS already selects the Endpoint/Transport and supplies complete datagrams. There is no BITS per-message CRC in the current direction (§11).

All multi-byte fields serialize little-endian (`BITS §1`). Encode and parse fields explicitly; C/C++ structure padding, alignment and host endianness are not wire contracts. Validate the selected message's exact length and reserved fields before semantic use. The sketches below do not allocate the control byte or authorize parsing unknown encodings.

## 5. Finite object and Compact SETUP

One transfer has one transmitter, one receiver, fixed object size and segment size, one profile, one session ID and one initial sequence number. The source must retain or regenerate identical bytes for retries until the transfer's terminal outcome. Mutating a log buffer while transmitting it violates this contract.

### 5.1 Candidate ten-byte SETUP

The updated proposal's ten-byte Compact layout is the current prototype direction. Its offsets are explicit, but it is not an interoperability-frozen message while the envelope and protocol remain open.

| Offset | Bytes | Field |
|---:|---:|---|
| 0 | 1 | control byte: version, Compact, SETUP (allocation TBD) |
| 1 | 1 | session ID |
| 2 | 1 | initial sequence number |
| 3 | 1 | reserved; TX zero, RX requires zero |
| 4 | 2 | final segment index, little-endian |
| 6 | 2 | segment size in bytes, little-endian |
| 8 | 2 | final segment size in bytes, little-endian |

Derive in a type wide enough before narrowing:

```text
segment_count = uint32(final_segment_index) + 1
object_size   = uint32(final_segment_index) * segment_size + final_segment_size
```

Require `segment_size > 0` and `1 <= final_segment_size <= segment_size`. The final segment may be full-sized. This layout represents 1 through 65,536 segments and **does not represent an empty object**. An empty-object extension is not selected. At the arithmetic maximum, `65,536 * 65,535 = 4,294,901,760` bytes; real limits are lower where path, source, sink, or local address widths require them. Check resource and offset limits before calling the sink or reserving the object.

A receiver validates syntax, supported profile, object/segment bounds, binding authority and sink readiness before accepting SETUP. Invalid sizes/reserved bytes are malformed and silently dropped; a well-formed request exceeding a declared sink/resource capability may receive the explicit protocol outcome in §10. Accepting SETUP initializes session state and the initial ACK/window. Sending SETUP alone does not authorize SEGMENT transmission.

### 5.2 Repetition and conflicts

An identical SETUP for the active session is idempotent: it returns current ACK/window state without restarting or invoking sink initialization again. Same session ID with different parameters is a conflict; it never mutates the active transfer and is counted/dropped in the prototype boundary. A different session while busy may receive BUSY for the requested session, preserving the current transfer (§10).

A receiver must retain the accepted parameters needed to compare repetitions. Behavior after completion, abort or restart depends on the unresolved retention/reuse rules (§12), not on an assumption that every later SETUP is new.

## 6. Compact SEGMENT and sink acceptance

### 6.1 Candidate four-byte header

| Offset | Bytes | Field |
|---:|---:|---|
| 0 | 1 | control byte: version, Compact, SEGMENT (allocation TBD) |
| 1 | 1 | session ID |
| 2 | 2 | absolute zero-based segment index, little-endian |
| 4 | variable | object bytes |

No separate sequence byte is sent. For sequence modulus `M`:

```text
sequence_number = (initial_sequence_number + segment_index) mod M
object_offset   = segment_index * segment_size
```

Require an active matching session, in-range absolute index, permission within the receiver's granted range, and the exact payload length: `segment_size` for a non-final segment, `final_segment_size` for the final segment. A SEGMENT before accepted SETUP, for another session, or outside the permitted range cannot become object data. Reject/count it without changing receipt state or calling the sink.

### 6.2 Receipt means committed sink acceptance

There are three distinct events:

```text
retained in ingress storage -> validated by BITS -> accepted by the object sink
```

Only the last permits advancing cumulative/selective receipt state. The sink reports success only when it has accepted responsibility for the bytes under its declared commitment contract. A mailbox enqueue, queued flash job, or pending callback result is not automatically that commitment. On failure, BITS does not set a receipt bit or report successful object completion.

The API must distinguish success, pending/backpressure if supported, and failure. Exact return enums and asynchronous completion mechanics remain open. An ambiguous failure after a possible write must not trigger blind repeated programming: resolve it under the sink's idempotency contract or fail the session. Transport duplicate suppression is not a substitute for a flash driver's failure semantics.

Within retained session state, an already accepted segment is not delivered/programmed again. A duplicate may cause the current ACK to be scheduled later; it does not advance state twice. A pending segment also needs bounded duplicate exclusion. A cumulative absolute frontier and selective receipt state distinguish accepted indices; a short wrapping sequence number alone cannot do so. No at-most-once guarantee survives loss of that state without a separately defined persistence/recovery mechanism (§12).

### 6.3 Out-of-order sinks

Segments may reach the sink out of order inside the granted window. The callback receives an absolute index or object offset and payload, not just wrapping sequence state. A random-access destination may serve as reorder storage. BITS does not require an object-sized RAM reorder buffer.

A sequential or tiny sink can grant one new segment at a time, producing stop-and-wait operation with the same protocol. Wider windows require real capacity for the permitted out-of-order work and retained duplicate/receipt state.

## 7. ACK state, grants, retries, and PROBE

### 7.1 Cumulative and selective receipt

Conceptually ACK contains the control byte, session ID, `window_base`, receive bitmap, and `max_recv_seq_num`. The Compact candidate uses one byte for each sequence value and two for the bitmap: six bytes in total if those are the only fields. Field order, any additional discriminator, and the complete wire encoding remain unresolved. Do not freeze the older proposal's extra `sequence_number` field or remove it merely from size arithmetic.

`window_base` names the wrapping sequence of the highest contiguous accepted segment. Keep an absolute frontier internally, including an empty-state sentinel; a wrapping value alone is not an object position. At accepted SETUP:

```text
absolute frontier = -1                 // conceptual empty state, not u16 wrap
window_base       = (initial_sequence_number - 1) mod M
bitmap            = 0
```

Bitmap bit 0 describes the position immediately after the base; bit 15 is base+16 in Compact (bit 31 is base+32 in Extended). Set bits describe sink-accepted segments. When a gap closes, advance the cumulative frontier and shift the selective state consistently. ACKs cannot acknowledge unsent or out-of-object indices, or move confirmed progress backward.

### 7.2 Capacity-backed permission

`max_recv_seq_num` encodes the farthest position the receiver currently permits. A maximum equal to the base represents no new positions. Internally keep the absolute granted frontier distinct from accepted progress. Grants are backed by segment ingress, pending sink work and any final-storage/reorder capacity; accepted out-of-order positions still count against the bitmap's representable span.

Limit the advertised span to the selected bitmap width and to an unambiguous modular interval below half the sequence space. This bound is a necessary interpretation constraint, not a solution to all stale ACKs (§7.3). Previously granted positions remain valid during normal operation; a receiver cannot reclaim promised capacity by advertising a smaller maximum and expecting in-flight data to disappear. Fault-driven cancellation is a separate session operation.

A depth-1 receiver can grant exactly one new segment at a time. Full local ingress still drops without acknowledging; sender retry provides recovery. A sender never exceeds the grant to compensate for a slow sink.

### 7.3 Sequence wrap and stale ACKs

Absolute SEGMENT indices disambiguate data across sequence wrap, but ACK/window fields currently contain only wrapping sequences. A delayed ACK from an earlier cycle of the same long session can look current after a complete wrap. Keeping the live window below half the modulus prevents ambiguity between nearby positions; it does **not** identify such an old ACK.

A sender must validate ACKs against its absolute sent/granted state. The final profile also needs a stale-packet lifetime/association rule or an additional absolute/epoch discriminator. That choice is open. Neither modulo arithmetic nor a random session ID proves safety for arbitrarily delayed ACKs inside one session. Compact's 65,536-segment index capacity is therefore an addressing limit, not evidence that every long transfer is safe under arbitrary delay. A prototype must declare and test its bounded-delay assumptions; unrestricted delayed/replayed traffic is not a supported guarantee before this is resolved.

### 7.4 Sender and receiver work

The sender keeps bounded outstanding state, retains or regenerates bytes, observes cumulative/selective ACKs, retries unacknowledged data after timeout, and ends with an explicit local failure under its retry/deadline policy. Local TX rejection is not delivery and does not discard the source. Retry budgets, backoff, pending-control storage and pacing are declared; there is no unlimited loop or Internet-style congestion-control claim.

The receiver processes sink results serially, advances receipt state only on success, advertises capacity as it becomes available, and generates ACKs in Transport context. It may coalesce several state changes into one current ACK there. Losing an ACK must not require duplicate sink delivery to regenerate it.

### 7.5 PROBE and closed-window recovery

When blocked by a closed window, the sender can send a session-scoped PROBE. A matching active-session PROBE carries no object data, does not advance sequence state, and asks for the current ACK/window state. Repetition is idempotent and bounded by retry policy.

This recovers when a receiver frees capacity but its window-update ACK is lost. Unknown/stale-session probes do not create sessions or free resources. Processing and responding occur later in the Transport context; the receive hook remains classification only.

### 7.6 Semantic examples, not frozen byte vectors

For an initial sequence of 254 and three initially granted segments:

| Sink accepts index | Absolute contiguous frontier | `window_base` (mod 256) | Bitmap |
|---|---:|---:|---:|
| none | -1 | 253 | `0x0000` |
| 2 | -1 | 253 | `0x0004` |
| 0 | 0 | 254 | `0x0002` |
| 1 | 2 | 0 | `0x0000` |

This exercises wrap and out-of-order gap closure without confusing sequence 0 with absolute index 0. A duplicate of index 2 makes no further sink call. The table assumes successful sink acceptance and does not resolve delayed ACKs from older cycles.

A SETUP with final index 2, segment size 21, and final size 5 describes three segments and 47 bytes. The final SEGMENT has offset 42, five object bytes, and nine BITS bytes including its four-byte header. The ten-byte SETUP's body is `session, initial_sequence, 00, 02 00, 15 00, 05 00` after its as-yet-unallocated control byte.

## 8. Session identity and lifecycle

A connection performs many sessions, but each instance has at most one active object transfer. Effective session identity includes Wire, local/remote HostIds, Endpoint, profile/version association and the profile-dependent session ID. The sender chooses the ID; HostIds are not XORed into it. It is a generation discriminator, not globally unique and not a security token.

The proposal permits pseudorandom or sequential IDs for experimentation. Neither is a stale-traffic guarantee. Compact IDs repeat after at most 256 choices, and random selection can collide sooner. Immediate reuse is unsafe while old SETUP/SEGMENT/ACK/PROBE/terminal state can remain relevant. The final design must define ID quarantine/tombstones or another exclusion mechanism, including peer restart, idle connection rebind, queued traffic and multi-hop delays.

A restarted instance discards uncertain transient state with observable loss (`RUN-6`). It does not resume a session merely because a packet has the expected HostIds and a familiar ID. Persistent resume metadata is outside the initial RAM prototype; after loss of session state, at-most-once writes and old/new session discrimination require a separately demonstrated recovery boundary.

## 9. User-datagram sideband

USER_DATAGRAM is connection-scoped, usable before, during and after object transfer, and does not inherently carry a transfer session ID. Its candidate representation is the common control byte followed by a bounded Service-defined payload. There is no BITS retransmission, object-window accounting, or reliable ordering promise for it.

User datagrams share the non-segment FIFO with protocol control (§3.1). Omitting the user callback does not remove the control FIFO; supported sideband can be consumed without application delivery when disabled, with observable local policy. A sideband command is not automatically ordered after all object segments, and BITS does not correlate it with a session for the Service.

Normal Service APIs expose datagram payload, segment payload plus absolute offset/index, and transfer status; they hide raw wire headers. Connection identity remains inspectable. Conceptual `sendDatagram(payload)` uses configured authority and reports normal local send outcomes. Exact callback/API signatures remain follow-on work, and profile widths must not leak into the Service contract.

## 10. Protocol refusal versus infrastructure rejection

`ERR-1` remains in force: malformed, unauthorized, unrepresentable or misdirected traffic is counted and dropped without a reflex response by a Link, Router, Dispatcher or BITS parser. Unknown control versions/types/profiles and invalid lengths/reserved fields cannot trigger speculative parsing to construct a reply.

BITS REJECT is a **protocol outcome for a valid SETUP**, processed by the configured Transport Entity with existing reply/TX authority. Examples are BUSY, a supported-format request exceeding local object/segment capacity, or a sink that cannot begin. It is session-correlated to the requested SETUP; BUSY never changes the already active session. A sender applies it only to a matching pending SETUP, not as an unauthenticated instruction to abort any active transfer.

The exact envelope, reason codes and retry treatment remain open. The source's UNSUPPORTED_VERSION/PROFILE and INVALID_ARGUMENT reason ideas are not adopted as automatic replies to undecodable or malformed packets. A future compatibility refusal would need a separately defined universally parseable envelope and bounded policy. No response is generated to REJECT itself or to another error response. Refusals consume declared bounded TX work and are rate-limited so repeated valid requests cannot create unbounded response load (`ERR-3`).

This distinction permits a designed peer protocol to answer a valid request without turning infrastructure rejection into remote diagnostics or bypassing authentication requirements.

## 11. Integrity and the Link boundary

BITS accepts only complete WS datagrams after the selected Link's framing, length and integrity validation. No partial Link reassembly is visible. The current BITS direction adds no per-SEGMENT or control-message CRC, relying on the configured carrier's declared integrity properties for that hop. A Link/profile with no adequate integrity protection cannot acquire one merely by selecting BITS.

A gateway validates ingress and creates a fresh egress integrity check. Corruption in gateway memory or re-encoding can therefore receive a valid outgoing checksum. **Hop validation is not end-to-end integrity** (`LINK-12`, `CORE §20`). BITS's current no-CRC direction does not close that gap, authenticate peers, prevent replay, or prove received object bytes match the source across a faulty gateway.

Whole-object CRC/hash/signature checks and expected object metadata belong to the Service, normally before firmware activation. They do not by themselves protect ACK/window/control state while a transfer is running. Deployments requiring protection for that state need an explicitly specified end-to-end/authenticated mechanism covering the relevant data and association. No generic wrapper composition or registry encoding is selected here.

A UDP tunnel epoch or secure tunnel may contribute a declared stale-association or security boundary, but is not assumed merely because a Link is routed. Session lifetime (§8), sequence ambiguity (§7.3), integrity and authorization are separate questions.

## 12. Completion, abort, and restart

Distinguish three milestones:

1. The receiver's sink has accepted every object segment under its declared commitment contract.
2. The sender has received unambiguous protocol confirmation of that fact.
3. The Service has validated, persisted as required, activated, or otherwise used the object.

BITS must eventually define how the sender determines milestone 2; it never equates it with milestone 3. Receipt ACKs do not claim power-loss durability unless the sink's contract actually promises it. An application verification/activation failure remains possible after transport receipt.

A final cumulative ACK is the initial completion candidate. Exact completion/close semantics, terminal-state retention, lost-final-ACK replay, sender/receiver timeouts and callback ordering are not frozen. The receiver cannot forget completed-session state and safely reuse its ID while assuming it can still recognize and answer old retransmissions. Tombstone contents, retention bounds and peer recovery must be designed together (§8).

Whether there is an explicit ABORT is open. If added, it is session-specific, idempotent, bounded and processed in Transport context. Local cancellation/failure must still release retained resources with a defined outcome even without an ABORT message. Retry exhaustion or peer silence does not prove the peer never wrote bytes. Late traffic cannot reactivate a cancelled session by accident.

The first RAM prototype can choose explicit provisional completion/cancellation rules to exercise the state machine. It must record their limits and demonstrate lost-final-message behavior; no production-safe completion, persistent resume, or reset-spanning exactly-once claim follows from that experiment.

## 13. Sources, sinks, and Service examples

Source and sink are semantic contracts, not a selected C++ ABI. A source can read/recreate a segment from stable storage at an absolute offset. A sink can begin an object, accept a segment at an absolute offset, report completion, and handle cancellation/failure. Begin, segment and completion outcomes are separately bounded and observable; raw callback spans cannot be retained without ownership (§3.2).

For a NOR sink, the Service may erase the target region before transfer. The sink owns program-unit alignment, page boundaries, final partial units, controller timing/execution restrictions, and write failures. Random-order programming is allowed only when those device constraints permit it. Flash work runs after receive acceptance and must satisfy its declared processing budget. No generic claim that flash writes are fast or safe in RX context is made.

A bootloader combines quiet sideband commands such as version/regions/prepare/status with segmented image data. It still owns authorization, image format, erase/program policy, whole-image verification, activation and rollback. A sideband “finalize” must verify transfer and sink state; it cannot rely on shared FIFO order against the separate segment queue.

A log producer may freeze one bounded batch for transmission while collecting new records in a second buffer. The frozen batch is recycled only at the Service's chosen terminal outcome. Overwrite, retry-exhaustion and retention policy belong to the Log Service. No filesystem or indefinite stream is required. A central collector provisions a bounded receiver bank; sender and receiver code may be independently compiled on constrained targets.

## 14. Placement, QoS, and conditional CAN11 capacity

Each complete BITS message must fit a WS datagram supported by every relevant Link on its configured path. BITS segments the **object**; the LLL may fragment/reassemble one WS datagram, and the Router only sees complete PDUs. Validate SETUP, ACK/control, sideband and SEGMENT size in both directions, including WS extensions and profile overhead. One oversized SEGMENT cannot be repaired by silently changing the negotiated segment size mid-transfer.

QoS is deployment-selected. Background object data with Normal ACK/PROBE/SETUP is useful on Links that support those choices, but Guest CAN11 has one fixed QoS per binding: incompatible per-message QoS rejects rather than being rewritten. On a fixed-QoS path, select a supported common policy or explicitly configure another suitable path. Critical remains subject to admission and latency analysis. BITS receiver grants are not network congestion control, and reliable bulk data still consumes bandwidth and retry capacity.

`LINK §2.11` owns the provisional CAN11 PDUA budget. If its START/continuation packing and CRC schedule remain unchanged, and WS extension overhead `H` is zero, the candidate four-byte Compact SEGMENT leaves:

| Classical CAN frames `N` | Conditional net PDU bytes | Object bytes (`NetPdu - H - 4`, here `H=0`) |
|---:|---:|---:|
| 4 | 25 | 21 |
| 5 | 31 | 27 |
| 6 | 38 | 34 |
| 7 | 45 | 41 |
| 8 | 52 | 48 |

For N=4, 32 raw CAN data bytes yield 21 object bytes: 65.625% of those data fields, or 84% of the conditional 25-byte PDU budget. This excludes CAN identifier/framing/bit stuffing, inter-frame time, ACKs, probes, retries, arbitration and other traffic; it is not a throughput or latency promise. At 21 bytes per segment, Compact index capacity is `65,536 * 21 = 1,376,256` bytes, subject to the session/sequence restrictions above.

Any PDUA packing/CRC revision invalidates these derived numbers. Recompute using `NetPdu(N) - H - transport_header_bytes`; never subtract the four-byte header and forget extensions. The ten-byte SETUP and candidate six-byte ACK also need placement checks, not just the data path. N<=4 is a useful constrained-node target, with N=5..8 available where supported and justified; no separate mandatory BITS CAN tier is allocated here.

## 15. Open decisions and evidence before freeze

`REG §6.16` owns the open status. This document is a main-set prototype design, not a completed interoperability specification. Remaining decisions include:

- TransportType allocation; fixed control-byte fields, message codes, lengths and reserved patterns; exact Compact ACK/PROBE/REJECT encoding; Extended fields and segment-index width.
- ACK disambiguation across sequence cycles, maximum packet lifetime/association assumptions, session-ID reuse and quarantine, tombstones, peer reset and connection rebind.
- Completion/close and ABORT; lost-final-ACK behavior; sink commitment/durability, async completion and partial-write failure; any persistent resume contract.
- Concrete source/sink and Transport APIs, result enums, bounded bank/mux APIs, queue depth/load guidance, TX quotas, retry/deadline policy and diagnostics.
- Any end-to-end integrity/authentication wrapper and its interaction with BITS control and session identity; final CAN11 PDUA capacities and path placement evidence.

Start with a fixed RAM object and Compact, including a depth-1 mailbox; flash and Extended come after the basic transfer behavior has evidence. Implementation/API and full conformance integration are a separate follow-on to this document. Needed evidence includes wrap and stale ACKs, duplicate SETUP, loss/duplication/reordering of segments, sink refusal/pending/failure, lost ACK and window updates, PROBE recovery, queue exhaustion with sideband, TX rejection, retry exhaustion, lost final confirmation, reset/session reuse and multiple bounded connections.

CAN controller retransmission can hide frame loss. To test BITS recovery, drop complete WS PDUs after successful Link reception, for example in a gateway between two CAN segments. Dropping a constituent frame instead tests PDUA reassembly. Keep those failure layers distinct and include competing traffic and scheduling delays before making resource or throughput claims.

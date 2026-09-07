# BITS — Binary Image Transport, Segmented

## Status

Prototype design for early WireSpaces implementation and validation.

BITS is a WireSpaces **TransportType** and Transport Entity specialized for moving one finite binary object reliably between two WS Hosts. It also provides an optional small unreliable datagram sideband for Service commands, status, and other control-plane payloads associated with that Endpoint.

BITS is deliberately narrower than a general-purpose reliable byte stream.

The current design is intentionally prototype-oriented: important invariants are stated, but exact byte layouts, completion/abort semantics, and some API details remain open until the first implementation is exercised.

---

## 1. Why BITS exists

Embedded systems repeatedly need to move finite binary objects:

- firmware images;
- FPGA images;
- calibration/configuration blobs;
- diagnostic snapshots;
- crash dumps;
- captured traces;
- log batches;
- lookup tables;
- other bounded files/blobs.

Many existing ecosystems can solve these problems, but often inside a larger application/network model. BITS is intended to make **finite-object transfer itself** a reusable WS building block.

A Service using BITS should be able to focus on the object-specific problem:

```text
Bootloader:
    boot FSM
    authentication / authorization
    image header and compatibility
    flash regions / slots
    erase/program policy
    hash/signature verification
    activation / rollback

Log offload:
    which logs to retain
    batching policy
    overflow policy
    storage naming / retention

Calibration:
    object format
    validation
    activation policy
```

BITS owns the generic transfer machinery:

```text
setup
bounded receiver flow control
segment numbering
selective/cumulative ACK state
retransmission
duplicate handling
out-of-order delivery
window probing
retry timers
finite-transfer state
```

This is intended to make features such as bootloading and log/file offload substantially cheaper to add to small embedded systems.

---

## 2. Non-goals

BITS is not:

- TCP;
- a byte stream;
- an indefinite stream/session;
- a shell/SSH transport;
- a generic reliable RPC transport;
- a generic ordered event stream;
- responsible for image interpretation;
- responsible for flash layout;
- responsible for image activation;
- responsible for authentication/authorization;
- responsible for whole-image signatures.

Those semantics belong to the consuming Service or to other WS mechanisms.

The optional sideband datagram channel shall remain modest. BITS should not accumulate unrelated reliable RPC/stream/subscription semantics.

---

## 3. WireSpaces TransportType model

Current WS direction:

> `TransportType` identifies the transport protocol/entity responsible for interpreting a packet and providing its delivery semantics.

A TransportType may be a simple "pure" delivery semantic, or it may internally provide several closely related message classes when doing so creates a better bounded embedded abstraction.

BITS is an example of the latter.

Earlier drafts exposed a separate `ReliableSegmented` TransportType and composed BITS from `ReliableSegmented + SimpleUnreliable`. That public layering is now retired.

The reliable-segmented machinery may still exist as an internal reusable implementation component, but it is not currently a separate WS wire-level TransportType.

**Code reuse does not require protocol-layer reuse.**

---

## 4. BITS transport model

A BITS Transport Entity exposes two logical channels behind one `TransportType`:

```text
BITS Transport Entity
    |
    +-- segmented object channel
    |     reliable
    |     finite
    |     receiver-window controlled
    |     setup / segment / ACK / probe / retry
    |
    +-- optional user datagram sideband
          unreliable
          Service-defined payload
          small control/status plane
```

The segmented channel is the core BITS capability.

The sideband exists because many finite-object Services naturally need a quiet command/status plane:

```text
Bootloader:
    GetVersion
    GetRegions
    Prepare
    <segmented firmware transfer>
    GetStatus
    Finalize

Log Service:
    ListLogs
    SelectLog
    <segmented log transfer>
```

A BITS implementation that does not need user datagrams simply omits/ignores the user-datagram callback. The same non-segment queue is still useful for BITS protocol control.

---

## 5. Service / Endpoint / Transport Entity relationship

Normal WS shape:

```text
Service instance
      |
   Endpoint
      |
Transport Entity
```

Current architectural rule:

> An Endpoint has one Transport Entity boundary.

A Service will normally use one Endpoint, although WS does not require every possible Service to use exactly one Endpoint.

For a bootloader:

```text
Bootloader Service
        |
Bootloader Endpoint
        |
   BITS Receiver
```

For a log-producing leaf:

```text
Log Service
    |
Log Endpoint
    |
BITS Transmitter
```

The BITS transmitter/receiver role refers to the direction of the **segmented object transfer**. Both roles may still send and receive BITS protocol control and user sideband datagrams as required.

---

## 6. One BITS instance = one connection

Each `BitsTransmitter` or `BitsReceiver` instance represents one point-to-point BITS connection.

Conceptually:

```cpp
struct BitsConnectionConfig {
    WireNumber wire;
    HostId local_host;
    HostId remote_host;
};
```

The Endpoint is normally implied by the owning Service/Transport Entity configuration.

The usual embedded case is fixed configuration:

```text
construct object with:
    Wire
    local Host
    remote Host

never change it
```

A connection may be rebound at runtime only while no transfer is active. Active-transfer peer changes are out of scope.

This gives the BITS state machine simple invariants:

```text
incoming Wire     == configured Wire
incoming SrcHost  == configured remote Host
incoming DestHost == configured local Host
```

and gives outgoing ACK/PROBE/control traffic an unambiguous peer and Wire.

### 6.1 Connection vs session

These are distinct:

```text
BITS connection
    fixed/configured Wire + local Host + remote Host association

BITS session
    one generation of one segmented object transfer over that connection
```

A connection may perform many sessions over its lifetime.

`session_id` distinguishes transfer generations. It does not need to encode the Host IDs.

### 6.2 Multiple simultaneous peers

A central receiver may need concurrent uploads from several MCUs.

The preferred model is **multiple bounded 1:1 `BitsReceiver` instances**, not one receiver containing a dynamic session table:

```text
Central Log Endpoint
        |
 bounded receiver bank / mux
        |
        +-- Host A -> BitsReceiver A
        +-- Host B -> BitsReceiver B
        +-- Host C -> BitsReceiver C
```

The Endpoint still has one receive boundary; a small statically provisioned mux/bank selects the configured BITS connection.

Concurrency therefore has an explicit resource cost:

```text
N simultaneous BITS connections
    -> N receiver state objects
```

A constrained implementation can intentionally provision fewer connections.

---

## 7. Endpoint receive hook

Dispatch resolves a BITS Endpoint to one receive hook.

The receive hook sees the complete BITS packet and performs only enough classification to choose bounded ingress storage.

BITS uses two ingress classes:

```text
BITS SEGMENT
    -> segment ingress storage

all non-segment BITS messages
    -> datagram FIFO
```

The non-segment FIFO contains both:

```text
BITS protocol control:
    SETUP
    ACK
    PROBE
    REJECT
    ...

Service sideband:
    USER_DATAGRAM
```

The receive hook shall not execute BITS state-machine semantics.

---

## 8. Execution-context invariant

The Link/dispatch receive context is a delivery context only.

It may:

- validate that the packet belongs to the configured BITS connection;
- inspect the BITS fixed control prefix;
- distinguish SEGMENT from non-segment messages;
- copy or enqueue the packet / packet reference into the selected bounded storage;
- return.

It shall not:

- process ACK semantics;
- advance windows;
- interpret duplicates;
- run retries;
- run SETUP state transitions;
- run completion state transitions;
- coalesce protocol control semantically;
- call application flash/programming logic in the normal implementation.

BITS protocol behavior runs later in the Transport Entity execution context.

> Classification and storage selection may happen on receive; protocol semantics do not.

---

## 9. Ingress storage

BITS intentionally uses only two normal ingress workstreams.

### 9.1 Datagram FIFO

One bounded FIFO contains:

```text
SETUP / ACK / PROBE / REJECT / other BITS control
USER_DATAGRAM sideband messages
```

This is intentionally one queue, not separate "BITS control" and "Service command" queues.

The shared FIFO provides one serial arrival order for the BITS control plane and the Service control plane.

This is especially useful for bootloaders, where the application command channel is normally quiet during the segmented transfer.

The exact queue depth and overflow policy remain implementation/prototype decisions.

Rules:

- it is bounded;
- overflow is observable;
- the receive callback does not add semantic prioritization/coalescing to make room;
- BITS retry/probe behavior should make recoverable control-message loss survivable.

### 9.2 Segment ingress

SEGMENT packets use separate bounded storage.

Common implementations:

```text
depth 1:
    mailbox
    one pending segment
    no overwrite

depth N:
    bounded queue
```

A depth-1 mailbox is a first-class configuration and is particularly useful on very small MCUs.

If segment ingress is full:

- the SEGMENT is not accepted into BITS processing;
- it is not recorded as received;
- it is not ACKed as received;
- sender retransmission provides recovery.

### 9.3 Copy and reference domains

BITS should work in either WS packet-delivery model:

```text
Copy domain:
    receive copies packet data into bounded storage

Reference domain:
    receive transfers a packet reference into bounded storage
```

Reference delivery permits zero-copy propagation but does not require every receiver/storage primitive to be zero-copy.

A copy-oriented sink or snapshot is allowed in a reference-based domain; it copies the needed bytes and releases the packet reference.

Copy and reference receive APIs should remain ownership-explicit rather than using one ambiguous pointer signature.

### 9.4 Direct-to-flash receive optimization

It is technically possible for a specialized tiny BITS receiver to use the final pre-erased flash destination itself as the segment storage, potentially eliminating an additional RAM segment buffer.

This is **not the normal receiver architecture**.

The preferred architecture keeps the receive hook trivial and performs flash programming later from BITS execution context.

Direct flash programming from the receive hook is only a specialized optimization when the target MCU's flash timing/execution restrictions and the BITS flow-control configuration make it demonstrably safe and worthwhile.

---

## 10. Bounded `process()` execution model

BITS is intended to support a serial, bounded superloop/task execution model.

Current API direction:

```cpp
TransportResult process() noexcept {
    // Process up to one received segment.
    //   - validate / update receive state
    //   - deliver accepted segment to Service/sink
    //   - update ACK/window state

    // Process up to one received non-segment datagram.
    //   - BITS control -> internal handling
    //   - USER_DATAGRAM -> Service callback

    // Run one bounded state-machine/timer step.
    //   - retries
    //   - probes
    //   - pending ACK/control TX
}
```

`process()` may be called repeatedly by a Service task/superloop to increase throughput.

One call should remain predictably bounded. It should not drain an arbitrarily large queue.

The exact ordering and return enum are still prototype API details, but the architectural intent is:

- at most one unit from each ingress workstream per call;
- protocol state changes occur serially in `process()`;
- user callbacks occur from `process()`, never from Link RX context.

`kProgress`/`kIdle` style semantics may be preferable to defining progress only as "RX packet consumed", because timer/retransmit work also counts as transport progress.

---

## 11. Service-facing callbacks

BITS hides raw WS and BITS wire headers from normal Service code.

The Service should receive semantic data:

```text
user datagram payload
segment payload + object offset/index
transfer state / completion notifications as needed
```

The Service shall still be able to inspect the connection identity (Wire, local Host, remote Host) when useful, either through callback context or through the configured connection object.

### 11.1 Segment callback

Because segments may be delivered out of order, a callback receiving only a payload is insufficient for a random-access sink.

Current API direction is conceptually:

```cpp
SegmentResult segmentCallback(
    uint32_t object_offset,
    ByteSpan segment_payload) noexcept;
```

or an equivalent segment descriptor.

BITS knows the absolute segment index and derives the object offset, so the Service should not need to reconstruct offset from wrapping sequence numbers.

The exact callback return contract is TBD, but the sink must be able to report acceptance/failure such that BITS does not ACK a segment as successfully consumed if the sink failed to accept it.

### 11.2 User datagram callback

Conceptually:

```cpp
void datagramCallback(ByteSpan payload) noexcept;
```

The payload is Service-defined.

A user datagram may exist before, during, or after an active segmented transfer.

User datagrams are unreliable and are not retransmitted by BITS.

### 11.3 Sending user datagrams

Current API direction:

```cpp
SendResult sendDatagram(ByteSpan payload) noexcept;
```

The connection's configured Wire and remote Host normally supply the destination.

The API should not call these "commands"; requests, responses, status, events, etc. are all Service semantics.

---

## 12. Profiles

BITS has multiple encoding/resource profiles under the single BITS `TransportType`.

Initial profiles:

```text
Compact
Extended
```

The profile affects widths such as:

```text
session ID
sequence-number space
ACK bitmap width
segment-index width
```

### 12.1 Compact

Current direction:

```text
session_id              u8
sequence-number space   u8
receive bitmap          u16   // 16 positions
segment_index           u16
```

Compact is the first prototype target.

### 12.2 Extended

Current direction:

```text
session_id              u16
sequence-number space   u16
receive bitmap          u32   // 32 positions
segment_index           likely u32
```

Exact Extended wire encoding remains TBD.

The Service-facing API shall not depend on the selected profile.

---

## 13. Fixed control prefix

Every BITS wire message begins with a fixed-format control byte.

That byte must contain enough information to determine how the remainder of the BITS message is parsed.

Current required fields:

```text
protocol version
BITS profile
message type
```

The exact bit allocation is not frozen.

A plausible layout uses:

```text
version : several bits
profile : 1 bit
type    : remaining bits
```

A 4-bit message type would provide 16 message types; whether that is the final split remains TBD.

The profile must be available in this fixed prefix because profile-dependent fields have different widths. It cannot be placed later in a body whose layout already depends on the profile.

---

## 14. No BITS magic number

BITS does **not** require a magic number in each message.

By the time BITS receives a packet, WS has already established that:

- the complete packet was accepted by the Link;
- it belongs to the destination Endpoint;
- its `TransportType` is BITS.

A BITS magic would therefore duplicate dispatch/framing information already provided by lower layers.

Version/profile/type in the BITS control byte are sufficient for BITS parsing.

---

## 15. Datagram integrity assumption

BITS does **not** add a per-segment or per-control-message CRC.

BITS receives complete WS datagrams from the Link layer.

The Link-layer contract is:

> deliver a complete integrity-validated WS datagram, or do not deliver it.

This applies to:

- SEGMENT;
- ACK;
- SETUP;
- PROBE;
- REJECT;
- USER_DATAGRAM;
- other BITS messages.

Whole-object integrity/authenticity remains a Service concern:

```text
CRC
hash
signature
authenticated image manifest
```

For a bootloader, whole-image verification normally occurs before activation.

Internet/routed-network stale-packet or tunnel-association protection is also orthogonal to BITS and may be provided by a UDP tunnel epoch, E2E wrapper, TCP/QUIC tunnel, or another Link mechanism.

---

## 16. Session identity

Every session-scoped BITS message carries a profile-dependent `session_id`.

```text
Compact:   u8
Extended:  u16
```

The session ID is:

- a transfer-generation discriminator;
- not globally unique;
- not a security token.

The effective transfer identity is conceptually:

```text
BITS connection
    Wire
    local Host
    remote Host
    Endpoint

plus:
    session_id
```

The Host IDs shall not be XORed/mixed into the numeric session ID. They already exist independently as connection identity.

The sender chooses the session ID. A pseudorandom value or another generation value is sufficient for the prototype.

Immediate reuse should be avoided where practical, particularly while state/tombstones from an older transfer may still be relevant.

Exact reuse/tombstone rules remain tied to the still-open completion/abort design.

USER_DATAGRAM messages are connection-scoped rather than transfer-session-scoped and therefore do not inherently need a BITS transfer session ID.

---

## 17. Transfer model

One BITS segmented transfer has exactly:

- one transmitter;
- one receiver;
- one fixed object size;
- one fixed segment size;
- one selected BITS profile;
- one session ID;
- one initial sequence number.

The object is finite.

Segments may arrive out of order within the receiver's advertised range.

A segment shall be delivered to the object sink at most once.

A `BitsTransmitter` / `BitsReceiver` instance has at most one active segmented transfer at a time.

---

## 18. Segment index and sequence number

BITS distinguishes:

```text
segment_index:
    absolute zero-based object segment number

sequence_number:
    small wrapping number used by ACK/window machinery
```

A key Compact optimization is that SEGMENT does **not** need to carry both.

Given:

```text
initial_sequence_number
segment_index
```

the sequence number is derived:

```text
sequence_number =
    initial_sequence_number + segment_index
    modulo profile sequence space
```

Therefore Compact SEGMENT can carry only the absolute `segment_index`.

This saves one byte per segment on constrained Links while preserving sequence-wrap handling.

### 18.1 Compact segment count

With `segment_index : u16`, Compact supports at most 65,536 segments in one object transfer.

The maximum object size therefore depends on negotiated/accepted `segment_size`.

A transfer requiring more segments shall use a richer profile or be rejected.


---

## 19. Current Compact SEGMENT header

Current prototype direction:

```cpp
// Precedes segment payload.
struct SegmentHeader {
    uint8_t  control_byte;
    uint8_t  session_id;
    uint16_t segment_index;
};
```

Conceptual wire size: **4 bytes**.

The multibyte field uses WS wire byte order (little-endian).

Implementations shall not rely accidentally on compiler padding; the wire representation must be explicitly known/frozen.

No separate sequence-number byte is transmitted in SEGMENT.

---

## 20. SETUP

SETUP establishes one segmented transfer.

The Compact SETUP wire representation is fixed at ten bytes:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 1 | control byte |
| 1 | 1 | session ID |
| 2 | 1 | initial sequence number |
| 3 | 1 | reserved; transmitter writes zero and receiver requires zero |
| 4 | 2 | final segment index, little-endian |
| 6 | 2 | segment size in bytes, little-endian |
| 8 | 2 | final segment size in bytes, little-endian |

The reserved byte naturally aligns every multi-byte field when the BITS PDU begins at a four-byte-aligned packet payload. Implementations shall still treat the wire representation explicitly and shall not rely on compiler struct padding.

The zero-based final segment index preserves the full Compact range of 1 through 65,536 segments without a sentinel encoding:

```text
segment_count = uint32(final_segment_index) + 1
total_size_bytes = uint32(final_segment_index) * segment_size_bytes
                   + final_segment_size_bytes
```

A valid SETUP has a nonzero segment size and a final segment size from one through the segment size, inclusive. The final segment may therefore be full-sized. A receiver reconstructs total object size once during SETUP validation and may reject unsupported version/profile, segment size, object size, a transfer while busy, or invalid arguments.

The control byte already supplies protocol version, Compact profile, and message type, so SETUP has no magic number or separate profile field.

### 20.1 SETUP idempotency

A repeated SETUP for the same active session with identical parameters shall not restart the transfer.

The receiver returns its current transfer/ACK state again.

A SETUP for a different session while the 1:1 BITS receiver already has an active transfer is rejected as BUSY in the baseline design.

---

## 21. ACK model

BITS ACK state is cumulative plus selective.

### 21.1 `window_base`

`window_base` is the highest wrapping sequence number for which all preceding segments through that sequence are known received.

At initial accepted SETUP:

```text
window_base = initial_sequence_number - 1  (mod sequence space)
bitmap      = 0
```

For Compact, if `initial_sequence_number == 0`, the empty-state `window_base` is therefore 255.

All sequence comparisons are modular.

### 21.2 Receive bitmap

The bitmap describes positions immediately after `window_base`.

Compact:

```text
bit 0   -> window_base + 1
...
bit 15  -> window_base + 16
```

Extended:

```text
bit 0   -> window_base + 1
...
bit 31  -> window_base + 32
```

A set bit means that position has been accepted/received.

### 21.3 `max_recv_seq_num`

`max_recv_seq_num` is the farthest sequence number the receiver currently permits the transmitter to send.

It is receiver-driven flow control.

The advertised permission must be backed by real bounded receiver capacity.

The receiver shall not grant more not-yet-received segment positions than it can actually accept using its committed segment ingress and sink resources.

A depth-1 receiver may advertise exactly one new segment at a time.

Previously granted positions should not be revoked during normal operation.

---

## 22. Window PROBE

A blocked transmitter needs a recovery path if a window-update ACK is lost.

BITS therefore provides PROBE.

Conceptually:

```text
Transmitter                     Receiver

        <------ ACK: window closed

                 receiver later frees capacity
                 window-update ACK is lost

PROBE ------------------------->
        <------ ACK: current state
```

PROBE:

- identifies the active session;
- carries no object data;
- does not advance transfer sequence state;
- is idempotent;
- requests the receiver's current ACK/window state.

A blocked transmitter may retry PROBE according to its retry policy.

This is BITS's intentionally small equivalent of the deadlock-recovery problem solved by TCP's persist behavior.

---

## 23. SEGMENT receive behavior

For an incoming SEGMENT, the receiver ultimately validates:

- connection identity;
- active session;
- segment index range;
- derived sequence number;
- whether the segment lies within the currently permitted receive range;
- whether it has already been accepted;
- segment payload length, including final partial segment rules.

A SEGMENT before accepted SETUP is not accepted.

A SEGMENT for another session is not accepted as part of the active transfer.

A segment outside the advertised receive range is not accepted.

Duplicate retransmissions are not delivered/programmed to the sink twice.

---

## 24. Out-of-order delivery

BITS explicitly permits segments to arrive out of order inside the receiver's advertised window.

The sink receives an absolute object offset/index.

For random-access sinks:

```text
segment 7 -> offset 7 * segment_size
segment 4 -> offset 4 * segment_size
segment 6 -> offset 6 * segment_size
```

The destination storage itself may serve as reorder storage.

This is particularly suitable for a pre-erased NOR region.

A tiny receiver may instead advertise only one segment at a time, making arrival effectively stop-and-wait while using the same BITS protocol.

---

## 25. Sender behavior

The transmitter maintains bounded state for outstanding segments.

It shall:

- obey the receiver's advertised maximum;
- retain or regenerate bytes needed for retransmission;
- process cumulative/selective ACK state;
- retransmit unacknowledged segments after timeout;
- use PROBE when blocked and window progress may have been lost;
- stop/fail according to configured retry policy.

Exact timeout/retry tuning remains an implementation/deployment matter for the prototype.

BITS is not intended to implement Internet-style congestion control.

Receiver flow control and Link/network congestion control are distinct concerns.

---

## 26. Receiver behavior

The receiver shall:

- accept at most one active transfer per `BitsReceiver` instance;
- validate SETUP/session;
- advertise only capacity it can honor;
- accept SEGMENT into bounded storage;
- process BITS semantics later in `process()`;
- deliver each segment to the sink at most once;
- update cumulative/selective receive state;
- generate ACK state;
- advance the receive window as resources become available.

ACK generation occurs in BITS execution context, not in Link RX context.

`process()` may coalesce transport work and emit one current ACK state after processing rather than requiring one ACK transmission per SEGMENT.

---

## 27. User datagram sideband

BITS includes an optional unreliable sideband message type:

```text
USER_DATAGRAM
```

Properties:

- Service-defined payload;
- connection-scoped;
- usable with no active transfer;
- usable during an active transfer;
- delivered through the shared non-segment FIFO;
- no BITS retransmission;
- no ordering guarantee beyond what naturally results from the underlying BITS ingress serialization;
- no attempt to turn it into a general reliable RPC channel.

This sideband is intended for small control/status traffic that naturally accompanies finite-object transfer.

---

## 28. Message types

Current expected BITS message classes include equivalents of:

```text
SETUP
SEGMENT
ACK
PROBE
REJECT
USER_DATAGRAM
```

Still open:

```text
ABORT
COMPLETE / CLOSE
```

Exact numeric encodings are TBD.

The current type count is comfortably below 16, but the control-byte bit allocation should be frozen only after the initial protocol implementation is exercised.

---

## 29. REJECT

The receiver shall be able to reject SETUP.

Likely reasons include:

```text
UNSUPPORTED_VERSION
UNSUPPORTED_PROFILE
UNSUPPORTED_SEGMENT_SIZE
OBJECT_TOO_LARGE
BUSY
INVALID_ARGUMENT
INTERNAL_ERROR
```

Exact values/fields are TBD.

---

## 30. ABORT

Whether BITS requires an explicit ABORT message is still open.

The first simple RAM-backed prototype does not need foolproof ABORT semantics before implementation begins.

If ABORT is added, it should be:

- session-specific;
- idempotent;
- bounded;
- handled by the BITS state machine in `process()`, not specially by the receive hook.

---

## 31. Completion

Completion semantics remain intentionally open for the prototype.

Minimum requirement:

> the transmitter can determine that the receiver has accepted the complete object.

A final cumulative/selective ACK may be sufficient.

A separate COMPLETE/CLOSE message may be added only if the prototype demonstrates a useful state-machine need.

Lost-final-message behavior and any short completed-session tombstone should be resolved before the protocol is considered stable, but are not blockers for the first prototype.

---

## 32. Source / sink abstraction

BITS transfers opaque finite object bytes.

A receiver-side sink conceptually needs something like:

```text
begin(total_size, segment_size)
write_segment(offset, data)
complete()
abort()       // if ABORT remains part of the final design
```

Exact API is TBD.

Possible sinks:

- RAM object;
- pre-erased internal NOR flash;
- external flash;
- FPGA configuration storage;
- file storage on a PC.

A transmitter-side source should be able to regenerate a segment from stable source storage rather than requiring BITS itself to retain the entire object.

Conceptually:

```text
read_segment(offset, output_span)
```

Possible sources:

- firmware image file;
- RAM object;
- frozen log buffer;
- flash region;
- calibration object;
- generated diagnostic snapshot.

---

## 33. NOR flash backend

For an initial bootloader Service, BITS may assume the target NOR region is erased before segmented transfer begins.

Random-order programming is compatible with the BITS model if the sink handles its own device restrictions.

The flash sink owns:

- program-unit alignment;
- page-boundary rules;
- final partial program units;
- flash-controller restrictions;
- duplicate-program safety;
- failure reporting.

Whole-image CRC/hash/signature verification and image activation belong to the Bootloader Service, not BITS.

Interrupted transfer recovery may initially be simple restart rather than persistent resume metadata.

---

## 34. Bootloader use

BITS is intended to remove a substantial generic transport burden from bootloader implementations.

A BITS-based bootloader can receive:

```text
USER_DATAGRAM:
    version / region / status / prepare / finalize commands

SEGMENT:
    image bytes

BITS internal:
    SETUP / ACK / PROBE / retry / flow control
```

The bootloader remains responsible for:

```text
boot FSM
security
image format
flash layout
erase/program
verification
activation
rollback
```

Every MCU in a WS lab may reasonably have a BITS bootloader. This makes bootloading itself a useful integration/stress workload for WS.

---

## 35. Log offload use

BITS is also intended for small finite log batches while a system remains operational.

Example leaf behavior:

```text
error/warning log producer
        |
bounded active log buffer
        |
periodically freeze a batch
        |
BitsTransmitter
        |
central computer / mass storage
```

The "file" may be only dozens or hundreds of bytes and need not exist as a filesystem file.

A useful tiny-MCU pattern is double buffering:

```text
buffer A:
    frozen and being transmitted

buffer B:
    current logging continues
```

On transfer completion, the frozen buffer is recycled.

Failure/overwrite policy belongs to the Log Service, not BITS.

The leaf MCU is the BITS transmitter in this use case; the central computer owns one or more BITS receivers.

---

## 36. QoS guidance

QoS is not hard-coded by BITS, but a common deployment policy is:

```text
SEGMENT bulk data:
    Background

BITS ACK / PROBE / SETUP / important control:
    Normal

Service sideband:
    Service/deployment selected
```

This lets normal/high-priority embedded traffic preempt background bulk transfer on arbitration-capable Links such as CAN.

A system should avoid having many periodic log transmitters all start on the exact same phase boundary; fixed per-node phase offsets or small deterministic jitter are sufficient.

---

## 37. Link MTU / fragmentation rule

BITS does not perform lower-layer fragmentation of one BITS message.

Every BITS message must fit in one complete WS datagram supported by the end-to-end Link path.

A Link profile may internally fragment/reassemble or project that WS datagram.

Example:

```text
BITS SEGMENT
    one complete WS datagram
        |
CAN11 PDUA
    N constituent Classical CAN frames
```

BITS sees only the complete validated WS datagram.

No partial Link-layer PDU becomes visible to BITS.

---

## 38. Classical CAN11 example: N = 4

The current provisional General CAN11 PDUA capacity calculation gives:

```text
N = 4 Classical CAN frames
raw CAN data bytes             32

PDUA framing/metadata           6
aggregate CRC-8                 1
                               --
net complete WS PDU            25 bytes
```

With the current 4-byte Compact BITS SEGMENT header:

```text
25 byte WS PDU
-4 byte BITS SEGMENT header
-------------------------------
21 byte object payload
```

Useful ratios:

```text
object / raw CAN data fields = 21 / 32 = 65.6%
object / net WS PDU          = 21 / 25 = 84.0%
```

At this segment size, Compact `segment_index : u16` covers:

```text
65,536 * 21 bytes
= 1,376,256 bytes
≈ 1.31 MiB
```

Approximate theoretical object throughput before bit stuffing, BITS ACK traffic, arbitration delays, and other bus traffic:

```text
500 kbit/s Classical CAN  ~23.6 kB/s
1 Mbit/s Classical CAN    ~47.3 kB/s
```

These CAN11 capacities are conditional on the current provisional PDUA START metadata/layout and CRC schedule. The Link profile remains authoritative.

### 38.1 CAN aggregation depth guidance

Current direction is:

```text
N = 1:
    preferred ordinary small datagrams

N = 2..4:
    normal multi-frame range
    attractive constrained-node support tier
    CRC-8 candidate

N = 5..8:
    available extended/bulk range
    expected less commonly
    CRC-16 candidate
```

The current conditional net WS PDU capacities are:

```text
N=4 -> 25 bytes
N=5 -> 31
N=6 -> 38
N=7 -> 45
N=8 -> 52
```

With a 4-byte Compact BITS SEGMENT header this corresponds to approximately:

```text
N=4 -> 21 object bytes
N=5 -> 27
N=6 -> 34
N=7 -> 41
N=8 -> 48
```

N_max = 4 therefore appears useful as a common small-node CAN11 implementation tier, while N=5..8 remains available for richer/bulk-oriented nodes.

---

## 39. Fault injection and lab validation

BITS is intended to become a major WS lab/stress workload.

Useful tests include:

- simultaneous BITS transfers;
- busy CAN bus;
- competing QoS traffic;
- depth-1 receiver mailbox;
- larger receiver windows;
- dropped segments;
- duplicated segments;
- reordered segments;
- delayed segments;
- lost ACK;
- lost window-update ACK;
- PROBE recovery;
- queue exhaustion;
- scheduling jitter;
- gateway hops;
- different physical Links;
- PC-to-MCU and MCU-to-MCU transfers.

### 39.1 CAN hardware retry caveat

Classical CAN controller retransmission can hide frame loss from BITS.

To test BITS-level loss, a useful lab setup is a software or RTL gateway between two CAN segments:

```text
CAN A ---- gateway/fault injector ---- CAN B
```

The gateway may successfully receive/ACK a frame on CAN A and intentionally suppress the corresponding forwarding on CAN B.

This prevents source CAN hardware retry from "repairing" the injected higher-layer loss.

Fault injection can be performed at multiple levels:

```text
frame level:
    drop one constituent PDUA frame
    stress Link reassembly

complete WS PDU level:
    drop/duplicate/delay complete PDU
    stress BITS

semantic host-simulation level:
    drop ACK
    drop PROBE
    reorder SEGMENT
```

An RTL gateway may eventually provide deterministic fault patterns and timing, but a software gateway is sufficient for initial work.

---

## 40. Initial prototype plan

First target: transfer a fixed RAM object.

Do not begin with flash.

Compact profile first.

The prototype should validate:

1. BITS as the actual WS `TransportType`;
2. one 1:1 BITS connection per transmitter/receiver object;
3. fixed Wire/local Host/remote Host configuration;
4. one Endpoint receive hook;
5. SEGMENT vs non-segment ingress classification only;
6. one shared FIFO for BITS control + USER_DATAGRAM;
7. separate segment mailbox/queue;
8. depth-1 segment mailbox;
9. bounded repeated `process()` execution;
10. SETUP;
11. Compact profile;
12. 4-byte Compact SEGMENT header;
13. segment-index-to-sequence derivation;
14. sequence wrap;
15. session handling;
16. ACK semantics;
17. receive window backed by actual storage capacity;
18. lost-window-update recovery using PROBE;
19. dropped/duplicated/reordered/delayed SEGMENT;
20. retransmission;
21. user sideband datagrams;
22. Link/dispatch vs BITS execution-context boundary.

ABORT, elaborate completion handshakes, flash persistence, Extended encoding, and detailed statistics are not blockers for learning from the first prototype.

---

## 41. Current wire-format direction summary

### Common first byte

```text
control_byte:
    protocol version
    profile
    message type
```

Exact bit allocation TBD.

### Compact SEGMENT

```text
u8  control_byte
u8  session_id
u16 segment_index
payload...
```

No separate sequence number.

No BITS CRC.

No magic.

### Compact SETUP

Compact SETUP uses the fixed ten-byte layout specified in Section 20: control, session ID, initial sequence number, one reserved zero byte, final segment index, segment size, and final segment size. All three 16-bit fields are little-endian and begin at naturally aligned offsets 4, 6, and 8.

### Compact ACK

Conceptually contains:

```text
control_byte
session_id
window_bitmap          u16
max_recv_seq_num       u8
window_base            u8
other ACK state only if proven necessary
```

The earlier prototype header included another `sequence_number` field; whether that field is actually required should be re-evaluated before freezing the ACK encoding.

### USER_DATAGRAM

Conceptually:

```text
control_byte = USER_DATAGRAM
Service payload...
```

No transfer session ID is inherently required if the datagram is connection-scoped.

---

## 42. Open items

Before BITS is considered stable, resolve:

- exact control-byte bit allocation;
- exact message-type numeric values;
- exact Compact ACK encoding;
- whether ACK needs any sequence field beyond `window_base`, bitmap, and max receive;
- exact Extended profile encoding;
- exact Extended segment-index width;
- completion semantics;
- whether ABORT is needed and its exact semantics;
- session-ID reuse/tombstone rules;
- datagram FIFO depth guidance and overflow policy;
- exact transmitter/receiver API;
- exact callback result/error enums;
- source/sink API;
- retry/timeout recommendations;
- optional statistics;
- static multi-connection receiver-bank/mux API;
- final CAN11 PDUA capacity after Link profile freeze.

---

## 43. Design summary

BITS is currently intended to be:

> **A bounded, point-to-point WireSpaces TransportType for reliable finite-object transfer, with an optional small unreliable sideband datagram channel for the Service controlling or accompanying the transfer.**

Key properties:

- finite object, not stream;
- one 1:1 connection per transmitter/receiver instance;
- one active segmented session per instance;
- fixed/configured Wire + local Host + remote Host in the common embedded case;
- out-of-order segment support;
- receiver-driven bounded flow control;
- cumulative + selective ACK;
- PROBE recovery for lost window updates;
- no BITS CRC;
- no BITS magic;
- Link delivers complete integrity-validated WS datagrams;
- simple fixed control byte selects version/profile/message type;
- Compact SEGMENT uses absolute `u16 segment_index`;
- sequence number is derived rather than transmitted per segment;
- shared FIFO serializes BITS control and user sideband datagrams;
- separate bounded segment storage;
- protocol semantics run outside Link RX context;
- sender/receiver implementations may be independently compiled for code-size-sensitive targets;
- suitable for bootloading, logs, calibration, diagnostics, FPGA images, and other finite binary objects.

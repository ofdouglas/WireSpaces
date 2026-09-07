# BITS — Binary Image Transport, Segmented

## Status

Prototype design for early WireSpaces implementation and validation.

BITS is a standard WireSpaces **Transport Entity composition** for moving one finite binary object from one Host to another. It combines:

- `SimpleUnreliable` for Service commands, status, and other ordinary datagrams;
- `ReliableSegmented` for reliable finite-object DATA transfer and its transport control messages.

BITS is deliberately narrower than a general-purpose reliable byte stream. It is not itself a WireSpaces `TransportType`; the packets it uses carry the underlying `SimpleUnreliable` or `ReliableSegmented` TransportType.

## Goals

BITS shall:

- reliably transfer a fixed-size binary object;
- support constrained MCUs with very small RAM;
- support larger systems without changing Service semantics;
- allow out-of-order ReliableSegmented DATA delivery;
- support bounded sender and receiver storage;
- provide receiver-driven flow control;
- encapsulate ACK, retry, window, duplicate-handling, and transport-control behavior inside a reusable Transport Entity;
- serialize Service datagrams and ReliableSegmented control processing through one Transport Entity execution context;
- remain independent of the object's meaning or storage backend.

Typical uses include firmware images, FPGA images, calibration/configuration blobs, lookup tables, and similar finite binary objects.

## Non-goals

BITS is not:

- TCP;
- a byte stream;
- bidirectional application data;
- an indefinite session;
- a shell/SSH transport;
- responsible for image interpretation, activation, authentication, or signature verification.

Those semantics belong to the consuming Service.

## WireSpaces integration

The normal WireSpaces Service shape is:

```text
Service instance
      |
   Endpoint
      |
Transport Entity
```

An Endpoint owns one Transport Entity. A Service will normally use one Endpoint, although WireSpaces does not require every possible Service to have exactly one Endpoint.

A Transport Entity may implement one TransportType or may compose several TransportTypes behind one Endpoint.

The simplest case is effectively transparent:

```text
Service
   |
Endpoint
   |
SimpleUnreliable Transport Entity
```

For a BITS-based bootloader:

```text
Bootloader Service
        |
Bootloader Endpoint
        |
   BITS Transport Entity
      /             \
SimpleUnreliable   ReliableSegmented
commands/status    binary object transfer
```

`TransportType` describes the delivery semantics of an individual WireSpaces packet. For BITS the relevant types are:

```text
SimpleUnreliable
ReliableSegmented
```

BITS itself is the combination of these TransportTypes and the Transport Entity behavior that composes them.

### Endpoint receive hook

Dispatch resolves the destination Endpoint to **one receive hook** owned by that Endpoint's Transport Entity. The callback receives the complete packet, including its `transport_type`.

The BITS receive hook performs only the minimum classification needed to select bounded ingress storage:

```text
SimpleUnreliable
    -> datagram ingress FIFO

ReliableSegmented control
    -> same datagram ingress FIFO

ReliableSegmented DATA
    -> segment ingress storage
```

The receive hook does not execute the ReliableSegmented state machine.

This intentionally places the Bootloader application control plane and the ReliableSegmented control plane in one ordered datagram FIFO. Later, `TransportEntity::process()` consumes that FIFO serially, which avoids having two independent control queues whose relative processing order must be reasoned about.

The segment store remains separate because DATA has substantially different size, buffering, and flow-control requirements.

## Transport Entity

BITS behavior is encapsulated in one Transport Entity owned by the Endpoint.

Service code shall not directly manage:

- ACKs;
- retransmission timers;
- receive windows;
- duplicate suppression;
- sequence wrap;
- receiver flow control;
- ReliableSegmented SETUP/PROBE/control handling.

Conceptually:

```text
WireSpaces Dispatcher
        |
Endpoint receive hook
        |
   +----+---------------------+
   |                          |
datagram FIFO             segment storage
SU + RS control               RS DATA
   |                          |
   +------------+-------------+
                |
       BitsTransport::process()
                |
        Service / object sink
```

The Service periodically calls the Transport Entity, for example:

```text
transport.process(...)
```

The exact API remains open. `process()` may also accept a Service-originated SimpleUnreliable command/status message to transmit.

During `process()`, the Transport Entity may:

- consume queued ReliableSegmented control messages;
- run ACK/retry/window/probe logic;
- consume accepted DATA segments;
- deliver received SimpleUnreliable datagrams to the Service;
- notify or synchronously call the Service/object sink for received segments;
- generate outbound transport-control traffic.

Callbacks or notifications from `process()` execute in the Transport Entity / Service execution context, never in Link RX context. This permits a receiver such as a bootloader sink to perform operations that would be inappropriate in the receive callback, including flash programming.

## Execution-context rule

The Link/dispatch receive context is a delivery context only.

It may:

- inspect `transport_type`;
- for ReliableSegmented, inspect only enough message metadata to distinguish DATA from control;
- copy or enqueue the packet or packet reference into the selected bounded ingress store;
- return.

It shall not execute ReliableSegmented protocol behavior such as ACK processing, window advancement, retries, duplicate interpretation, SETUP state transitions, or transfer completion.

BITS protocol logic runs later in the Transport Entity execution context.

This is a non-negotiable architecture rule: **classification and storage selection may happen on receive; protocol semantics do not.**

## Ingress storage

BITS uses two classes of ingress storage.

### Datagram ingress

One bounded FIFO stores all non-segment traffic for the Endpoint:

```text
SimpleUnreliable Service datagrams
ReliableSegmented SETUP / ACK / PROBE / REJECT / other control
```

This is deliberately a single queue rather than separate Service-command and ReliableSegmented-control queues. It gives the Transport Entity one serial order for the Endpoint's control-plane activity.

For a bootloader this is particularly attractive because Service commands are normally sparse while a segmented transfer is active.

The exact FIFO depth and overflow policy remain implementation/prototype choices. Overflow shall be observable. The prototype should keep the policy simple rather than adding semantic message prioritization in the receive callback.

### Segment ingress

ReliableSegmented DATA uses separate bounded storage because segment payloads are larger and receiver capacity directly controls the advertised window.

Storage depth is configurable.

Examples:

```text
depth 1:  mailbox on a tiny bootloader / AVR
depth N:  bounded queue on a busier ECU
```

A depth-1 mailbox is a valid and important configuration: one segment may be pending, and a second segment is rejected until the first is consumed.

The implementation may use copy-based storage, packet-reference/zero-copy storage, or another bounded implementation with equivalent semantics.

If segment ingress is full, the DATA packet is not accepted and therefore is not acknowledged. ReliableSegmented retransmission provides recovery.

A specialized tiny-target receiver may write an accepted segment directly to a pre-erased random-access sink instead of first buffering the segment in RAM, provided the execution-context and platform constraints are explicitly satisfied. This is an optimization, not the baseline receiver architecture.

## Quality of Service (QoS)

This section is TBD. It is expected that DATA packets will have `Background` QoS, while all other packets will have `Normal` QoS.

## Profiles

The `ReliableSegmented` TransportType used by BITS has multiple encoding/resource profiles. Compact and Extended do not consume separate WireSpaces `TransportType` values.

### Compact

```text
session ID            u8
sequence number       u8
window base           u8
max receive sequence  u8
receive bitmap        u16
```

Receive bitmap width: 16 sequence positions.

### Extended

```text
session ID            u16
sequence number       u16
window base           u16
max receive sequence  u16
receive bitmap        u32
```

Receive bitmap width: 32 sequence positions.

The consuming Service API shall not depend on the selected ReliableSegmented profile.

## Transfer model

One ReliableSegmented transfer has exactly:

- one sender;
- one receiver;
- one fixed object size;
- one fixed chunk size;
- one selected ReliableSegmented profile;
- one session ID;
- one initial sequence number.

The initial BITS Transport Entity supports at most one active ReliableSegmented transfer at a time. A richer system may instantiate additional Transport Entities if it genuinely requires independent concurrent transfers.

Chunks may be received and delivered to the sink out of order.

Duplicate chunks shall not be delivered to the sink twice.

## Session ID

Every ReliableSegmented message belonging to a transfer contains a session ID.

```text
Compact:   session_id u8
Extended:  session_id u16
```

SimpleUnreliable Service datagrams do not acquire a BITS session ID merely because they share the BITS Transport Entity.

The session ID distinguishes the current transfer generation from delayed or duplicated ReliableSegmented packets belonging to an older transfer. It is not globally unique and is not a security token.

A ReliableSegmented transfer is identified in context by the tuple of the existing WireSpaces addressing information plus the session ID, conceptually:

```text
source Host
destination Host
Endpoint
session_id
```

The Host IDs shall not be mixed into the numeric session ID; they are already carried independently by WireSpaces.

The sender chooses the session ID. It should vary between transfers and should avoid immediate reuse where practical. A pseudorandom value, or another generation value appropriate to the platform, is sufficient for the prototype.

## Sequence numbering

Chunk 0 uses `initial_sequence_number`. Each subsequent chunk increments modulo the sequence space of the selected profile.

```text
chunk 0 -> initial_sequence_number
chunk 1 -> initial_sequence_number + 1
chunk 2 -> initial_sequence_number + 2
...
```

Wraparound is permitted.

The active sender/receiver sequence span shall remain sufficiently smaller than the sequence-number space to make modular comparison unambiguous. The initial profiles naturally satisfy this because their advertised windows are far below half the sequence space.

The Transport Entity maps modular sequence numbers to absolute chunk positions using current transfer/window state.

## Object sizing

SETUP carries:

```text
total_size_bytes
chunk_size_bytes
```

The number of chunks is derived:

```text
ceil(total_size_bytes / chunk_size_bytes)
```

The final chunk may be shorter than `chunk_size_bytes`.

A receiver may reject object or chunk sizes it cannot support.

### Datagram / Link size rule

ReliableSegmented does not fragment its own messages.

Every SETUP, DATA, ACK, PROBE, REJECT, and other ReliableSegmented control message shall fit in one complete WireSpaces datagram supported by the end-to-end path.

A Link profile may internally fragment/reassemble or aggregate/projection-map that WireSpaces datagram as needed. BITS and ReliableSegmented see only the resulting complete integrity-validated datagram.

## SETUP

A SETUP request establishes the transfer.

Proposed fields:

```text
magic
version
message type = SETUP
profile
session_id
initial_sequence_number
chunk_size_bytes
total_size_bytes
```

Exact field widths and final byte layout remain TBD.

ReliableSegmented SETUP uses one magic value plus an explicit version field. Compact versus Extended is selected by `profile`, not by separate magic values.

### Setup acceptance

The receiver accepts by sending an ACK describing an empty receive state and the initial permitted send range.

Conceptually:

```text
window base = initial sequence - 1
receive bitmap = 0
max receive sequence = highest initially permitted sequence
```

A tiny receiver with one available segment slot may initially permit only one DATA chunk.

### Setup retry / idempotency

A repeated SETUP for the same active session with identical transfer parameters shall not restart the transfer. The receiver returns its current acceptance/ACK state again.

A SETUP for a different session while the Transport Entity already has an active ReliableSegmented transfer is rejected as BUSY in the baseline design.

## ACK semantics

ACK state is cumulative plus selective.

### `window_base`

`window_base` is the highest sequence number for which all preceding chunks through that sequence are known received.

### Receive bitmap

The bitmap represents sequence positions immediately after `window_base`.

Compact:

```text
bit 0  -> window_base + 1
...
bit 15 -> window_base + 16
```

Extended:

```text
bit 0  -> window_base + 1
...
bit 31 -> window_base + 32
```

A set bit means that sequence position has been received.

### `max_recv_seq_num`

`max_recv_seq_num` is the farthest sequence number the receiver currently permits the sender to transmit.

It provides receiver-driven flow control.

The advertised range shall be backed by real bounded receiver capacity. The receiver shall not grant more not-yet-received DATA positions than it can actually accept using committed segment-ingress and sink resources.

A receiver may advertise less than the bitmap capacity, but shall not advertise beyond the range represented by the current ACK bitmap. A depth-1 receiver may therefore permit exactly one new DATA segment at a time.

Once a sequence position has been advertised as acceptable for the active transfer, that permission should not be revoked during normal operation.


## Window probe

A sender that is blocked by receiver flow control shall be able to request the receiver's current ACK/window state.

This handles the case where the receiver later frees ingress capacity but the corresponding window-update ACK is lost.

A PROBE:

- identifies the active session;
- carries no application data;
- does not advance sequence state;
- is idempotent;
- requests the receiver to send its current ACK state.

Conceptually:

```text
Sender                         Receiver

ACK: window closed <-----------

        receiver frees storage
        window-update ACK lost

PROBE ------------------------->
              <---------------- ACK: current window state
```

A blocked sender may retry PROBE according to its retry policy until the transfer progresses or fails.

## DATA

Every DATA message contains:

```text
session_id
sequence_number
payload bytes
```

Compact DATA metadata:

```text
u8 session_id
u8 sequence_number
```

Extended DATA metadata:

```text
u16 session_id
u16 sequence_number
```

The Transport Entity maps the sequence number to the corresponding object chunk and destination offset.

## Out-of-order delivery

BITS explicitly permits DATA chunks to arrive out of order within the receiver's advertised range.

For random-access sinks, the sink may itself serve as reorder storage.

Example:

```text
chunk 7 -> offset 7 * chunk_size
chunk 4 -> offset 4 * chunk_size
chunk 6 -> offset 6 * chunk_size
```

This is particularly suitable for pre-erased NOR flash.

## Datagram integrity assumption

BITS does not add a per-DATA or per-control-message checksum.

Both SimpleUnreliable and ReliableSegmented operate on complete WireSpaces datagrams delivered by the Link layer. The Link layer is responsible for framing and integrity checking and shall either deliver a complete integrity-validated datagram or not deliver it.

This applies equally to ReliableSegmented DATA, ReliableSegmented control, and SimpleUnreliable Service messages.

Whole-object CRC, hash, signature, or other image validation may still be performed by the consuming Service. For example, a Bootloader Service may verify the completed firmware image before activation.

## Duplicate DATA

Duplicate DATA may occur because of lost ACKs or retransmission.

Duplicate DATA shall:

- be recognized using transfer/session/window state;
- contribute to ACK state as appropriate;
- not be delivered/programmed to the sink a second time.

## Sender behavior

The sender maintains bounded state for outstanding chunks.

The sender shall:

- obey `max_recv_seq_num`;
- retain or regenerate data needed for retransmission;
- process ACK state;
- retransmit unacknowledged chunks after timeout;
- stop or fail according to configured retry policy.

Exact retry timing and policy are implementation/configuration details for the prototype.

## Receiver behavior

The receiver shall:

- validate session;
- reject DATA before an accepted SETUP or for the wrong active session;
- reject DATA outside the currently permitted receive range;
- store accepted DATA in bounded ingress storage;
- process DATA later in the BITS Transport Entity context;
- update receive state;
- deliver each chunk to the sink at most once;
- generate ACK state from Transport Entity execution context;
- advance flow control as resources become available.

`process()` may consume a batch of ingress packets and then emit one current ACK state. ReliableSegmented does not require one ACK transmission per received DATA packet.

## REJECT

The receiver shall be able to reject SETUP.

A rejection carries at least:

```text
session_id
reason
```

Candidate reasons:

```text
UNSUPPORTED_VERSION
UNSUPPORTED_PROFILE
UNSUPPORTED_CHUNK_SIZE
OBJECT_TOO_LARGE
BUSY
INVALID_ARGUMENT
INTERNAL_ERROR
```

Exact values remain TBD.

## ABORT

Whether the first BITS protocol requires an explicit ABORT message is still open.

If ABORT is included, its semantics must be idempotent, bounded, and session-specific. It shall be handled by the ReliableSegmented state machine in Transport Entity execution context, not specially by the receive callback.

ABORT is not required for the first simple RAM-backed prototype unless the implementation work demonstrates that it is needed to exercise the core state machine.

## Completion

Reliable completion semantics remain intentionally open for the prototype.

The minimum required property is that the sender can determine that the receiver has accepted the complete object without requiring protocol work in receive context.

A final cumulative/selective ACK may be sufficient. A separate COMPLETE/close interaction may be added if it solves a demonstrated state-machine need.

The first prototype should establish the required behavior before the final completion message set is frozen.

## Message types

The exact ReliableSegmented message-type set remains TBD.

The design currently expects equivalents of:

```text
SETUP
DATA
ACK
PROBE
REJECT
```

`ABORT` and a separate completion/close message remain open until the prototype demonstrates the need and desired semantics.

ReliableSegmented messages carry the session-ID width selected by the active profile.

SimpleUnreliable packets sharing the same Endpoint are Service-defined datagrams. They are not ReliableSegmented control messages and do not use the ReliableSegmented message-type field.

## Source / sink abstraction

The ReliableSegmented part of BITS transfers opaque bytes.

A receiver-side sink should conceptually support:

```text
begin(total_size, chunk_size)
write_chunk(offset, data)
complete()
abort()
```

The exact API remains TBD.

Possible sinks include RAM, pre-erased NOR flash, external flash, and FPGA configuration storage.

A sender-side source similarly provides object/chunk data without exposing retry behavior to the consuming Service.

## NOR flash backend assumptions

For an initial bootloader Service, the BITS sink may assume the target NOR flash region is erased before DATA transfer begins.

Random-order programming is compatible with this model when the sink handles its own programming constraints.

The flash sink is responsible for:

- program-unit alignment;
- page-boundary handling;
- final partial programming units;
- flash-controller-specific restrictions;
- avoiding unsafe duplicate programming.

ReliableSegmented chunk size should be chosen so each chunk can be independently written without requiring data from another chunk where practical.

Whole-image CRC/hash/signature verification and image activation belong to the Bootloader Service, not BITS.

## Error and overflow behavior

Ingress-storage overflow must be observable.

For ReliableSegmented DATA ingress overflow:

- the packet is not accepted into ReliableSegmented processing;
- it is not acknowledged as received;
- retransmission provides recovery.

For the shared datagram FIFO:

- SimpleUnreliable Service datagrams and ReliableSegmented control messages share the same bounded queue;
- the receive callback shall not perform transport-semantic prioritization or coalescing to make room;
- the exact prototype depth and drop policy remain to be selected;
- ReliableSegmented retry/probe behavior should make loss of recoverable control datagrams survivable.

The shared FIFO is an intentional serialization mechanism, not merely a memory optimization.

## Statistics

Transport statistics are not required by the protocol.

Implementations may optionally expose BITS-specific statistics such as:

```text
chunks_tx
chunks_rx
retransmits
duplicate_chunks
acks_tx
acks_rx
timeouts
setup_rejects
ingress_overflows
transfer_aborts
```

Statistics should remain optional/composable so constrained targets do not pay RAM cost for unused instrumentation.

## Initial prototype

The first implementation should transfer a fixed RAM object rather than firmware.

It should use one Service, one Endpoint, and one BITS Transport Entity composed from `SimpleUnreliable` and Compact `ReliableSegmented`.

It should validate:

- one Endpoint receive hook;
- shared datagram FIFO for SimpleUnreliable + ReliableSegmented control;
- separate segment mailbox/queue for ReliableSegmented DATA;
- serial processing through `TransportEntity::process()`;
- SETUP;
- Compact profile;
- one active ReliableSegmented session;
- session handling;
- sequence wrap;
- ACK semantics;
- receiver flow control backed by actual segment-storage capacity;
- lost window-update recovery using PROBE;
- dropped, duplicated, reordered, and delayed DATA;
- retransmission;
- configurable DATA ingress depth, including depth 1;
- Link/dispatch versus Transport Entity execution boundaries.

The prototype does not need foolproof ABORT/completion semantics before useful implementation work begins.

Bootloading and flash programming should follow after the RAM-backed transport behavior is understood.

## Open items

Before BITS is considered stable, resolve:

- exact ReliableSegmented message-type encoding;
- exact SETUP byte layout and integer widths;
- exact ACK byte layout;
- exact completion semantics;
- whether an explicit ABORT message is required and, if so, its semantics;
- shared datagram-FIFO sizing and overflow policy;
- exact Service delivery API from `TransportEntity::process()` (callbacks, polling/events, or another bounded form);
- exact Service-originated SimpleUnreliable send API through the Transport Entity;
- retry/timeout recommendations;
- session-ID generation/reuse recommendations;
- source/sink API shape;
- optional statistics API.


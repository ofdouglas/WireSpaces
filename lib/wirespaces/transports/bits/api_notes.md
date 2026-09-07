# Admission and receiver lifecycle

## Canonical packet admission

Both BITS endpoint roles require supported canonical controls and BITS transport
before occupying ingress storage: extensions, nonzero reserved bits, and other
transport types are rejected. This applies to direct `receive()` calls as well
as dispatch. The constrained Arduino adapter performs the same check before
passing a payload to the engine, which accepts already-admitted BITS PDUs.

## Link admission

BITS send adapters now return `SendResult` rather than a boolean. `kSent` means
accepted by the local Link(s), not delivered remotely. The Router/generated fan-out
tries each selected Link once; it has no retry policy.
`Router::forward()` returns a byte-sized `RouteResult` describing coarse admission
or routing failure, without per-egress masks or diagnostics. BITS maps accepted,
partial, full, too-large and rejected outcomes to its corresponding send results.
No-route, invalid-ingress and no-egress outcomes map to `SendResult::kNoRoute`.
`kPartial`, `kTooLarge`, and permanent rejection are not successful BITS sends.
BITS does not automatically replay partially accepted fan-out. Its ordinary
point-to-point transport should be configured with an appropriate route.

`kFull` is transient. A transmitter returns `kBlocked` without marking a segment
sent, starting its retransmission timer, or consuming retry budget; a subsequent
BITS poll can try admission again. This is transport behavior, not Router retry.
Persistent congestion can remain blocked indefinitely; callers can cancel.
Receiver ACK congestion preserves accepted state. A peer's existing Setup,
segment, or probe retransmission supplies the next ACK opportunity. There is no
new ACK timer or Router queue. Datagram callers decide whether to retry.

## Receiver callbacks

Applications implement `beginTransfer(TransferInfo) -> TransferAdmission`,
`onTransferFailed(FailureReason)`, and `onTransferAborted(AbortReason)` in addition
to segment/datagram/completion callbacks. Admission runs after geometry/resource
validation and before accepting Setup. It can initialize/reserve sink storage,
reject for capacity/busy/application reasons, and must not reenter the engine.
An active duplicate Setup only resends the ACK; it does not reinitialize the sink.
A competing session is rejected without invoking admission. After termination,
session IDs may be reused for a newly admitted transfer.

Each accepted active transfer ends with completion, local/peer abortion, or failure
(sink rejection, permanent send failure, or inactivity expiry). Completion means all bytes reached
the sink, even when the final ACK cannot be admitted; duplicate final segments
can obtain another ACK without a second completion callback. Malformed packets
return `kError` without terminating an otherwise active transfer. A failed
admission is not an accepted transfer and gets no terminal callback.
The boot-profile application no longer decodes Setup ahead of the engine.

## ACK validation

The C++ transmitter accepts ACKs only after SETUP has been admitted locally.
Cumulative advancement must cover only bits in the current sent-segment bitmap;
an ACK covering any unsent segment is ignored without moving the window or
reporting completion. The Python stop-and-wait bench transmitter applies the
same rule to its outstanding segment. This includes new transfers that reuse a
previously terminated session's identity.

This validation does not make identical reused session/sequence values a new
wire-level generation: a delayed ACK that matches data already sent in a new
transfer is indistinguishable on the wire. Callers must avoid identity reuse
while packets from the previous generation can still arrive.

## Abandoned-transfer recovery

The receiver owns a configurable inactivity timeout, defaulting to 5,000 ms.
`BitsReceiver::process(now_ms)` must run even when no traffic arrives. Direct
engine users call `process(message, now_ms)` for each PDU and `poll(now_ms)` during
idle periods. Both take caller-supplied wrapping monotonic milliseconds; there is
no global clock dependency inside BITS. Access to one receiver remains serialized.

A valid accepted SETUP starts the timeout. Matching duplicate SETUP, correctly
sized current-session segments within the granted range (including duplicates),
and matching probes renew it, including when their ACK is blocked. Sideband
messages, competing sessions, malformed PDUs and out-of-grant segments do not.
Time is sampled when the receiver processes input, not when a driver enqueues it;
choose a timeout longer than the peer's expected retry interval and normal
receiver scheduling delay. Poll more frequently than the timeout and within one
32-bit clock wrap. An explicit zero timeout delegates recovery to the application.

At expiry, the receiver transitions to `kError` and invokes
`onTransferFailed(kInactivityTimeout)` exactly once so the sink can release its
reservation. Expiry is local: it sends no packet and does not depend on Link
admission or peer reachability. A subsequent SETUP can reserve storage again.
This bounds recovery after a lost ABORT or a disappearing peer; it does not make
ABORT reliable or let a competing session displace a still-active transfer.

Expiry is checked before input processing. The queued receiver discards pending
ingress on expiry; a direct engine call that detects expiry discards that PDU.
A new SETUP arriving at that boundary can be retried. Completed objects do not
expire and retain final-segment retransmission/ACK behavior.

Both Arduino BITS examples poll expiry. The constrained receiver example now
starts the Timer0 millisecond clock, including its size-profile build.

## Packet storage

`PacketBuffer` copy/move construction and assignment are protected. Concrete
fixed-storage packets remain normally copyable, including all their payload
storage. Public code working through the base must use `copyFrom()`, which
checks capacity and does not overwrite the destination's capacity. The prefix,
span interface, canonical encoding and packet size have not changed.

For legacy zero-egress local-dispatch routes, the Router still invokes the local
forwarder and has no physical admission bits to report. Its summary is not a
remote delivery acknowledgement; inspect local dispatch results where needed.

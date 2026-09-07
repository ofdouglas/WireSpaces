# Admission and receiver lifecycle

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
(sink rejection or permanent send failure). Completion means all bytes reached
the sink, even when the final ACK cannot be admitted; duplicate final segments
can obtain another ACK without a second completion callback. Malformed packets
return `kError` without terminating an otherwise active transfer. A failed
admission is not an accepted transfer and gets no terminal callback.
The boot-profile application no longer decodes Setup ahead of the engine.

## Packet storage

`PacketBuffer` copy/move construction and assignment are protected. Concrete
fixed-storage packets remain normally copyable, including all their payload
storage. Public code working through the base must use `copyFrom()`, which
checks capacity and does not overwrite the destination's capacity. The prefix,
span interface, canonical encoding and packet size have not changed.

For legacy zero-egress local-dispatch routes, the Router still invokes the local
forwarder and has no physical admission bits to report. Its summary is not a
remote delivery acknowledgement; inspect local dispatch results where needed.

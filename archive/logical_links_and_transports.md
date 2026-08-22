# Logical Links and Transports

## Status and authority

This document defines the **stable, implementation-neutral contracts** for Logical Link Layers (LLLs), Links, and the currently supported transport vocabulary. The contracts apply equally to software, RTL, and mixed implementations and do not stabilize any particular wire encoding. The overall layering and scope are summarized in [Architecture Overview](architecture_overview.md).

Wire interoperability exists only under a separately named, immutable profile. The serial candidate is documented in [HDLC Logical Link Profile](hdlc_logical_link_profile.md) and remains explicitly experimental. Link ownership, restart, and device-wide status are defined in [Link Engine Runtime and Status](link_engine_runtime_and_status.md).

Endpoint Domains, Wires, Routes, Direction, endpoint access, Taps, Relays, and routing-representation semantics belong to [Endpoint Domains, Wires, Routes, and Access](endpoint_domains_wires_routes_and_access.md). Canonical `Control`, `RoutingWord`, `WireBand`, `RoutingCode`, `EndpointId`, and header-extension semantics belong to [Core Protocol and Routing](core_protocol_and_routing.md). Application delivery policies belong to [Application Protocols and Services](application_protocols_and_services.md). This document uses those contracts rather than redefining them. All such identities and bindings are interpreted within one configured Wire Space; a Link profile does not make them global.

Normative terms such as **shall** and **must** apply to the stable generic contracts unless a section is marked Provisional, Experimental, Non-normative, or Open.

## 1. Layer boundaries

```text
Application / Service
        |
Endpoint Router
        |
Transport
        |
Logical Link Layer
        |
Link
```

The boundaries are:

- A **transport** adds delivery semantics to one bounded application message.
- An **LLL** maps one canonical PDU value to and from one configured link profile.
- A **Link** moves profile-specific bytes, frames, datagrams, records, or equivalent transfer units. It may be a hardware controller/driver boundary, shared-memory channel, software carrier, FIFO, RTL interface, or mixed implementation. It has no endpoint or Service semantics.
- A **Link Engine** owns and serializes the mutable runtime for one or more LLL and Link instances that implement local Route segments. A Link Engine does not own a complete Route or any Wire; see [Link Engine Runtime and Status](link_engine_runtime_and_status.md).

Physical signaling, transceivers, modulation, electrical characteristics, operating-system objects, and programming-language class hierarchies are outside the generic contracts.

## 2. Canonical bounded-PDU service

### 2.1 Canonical PDU value

At the transport/LLL boundary, one PDU is a discrete semantic value containing:

- canonical `Control`, `RoutingWord`, and `EndpointId` values defined by the
  core protocol specification;
- zero or more bounded canonical header extensions when indicated by
  `Control`;
- bounded transport-specific metadata or status;
- one bounded application payload.

This list identifies the semantic parts of a PDU and does not duplicate or alter the core descriptor or extension layout. The canonical PDU value is not necessarily a contiguous byte array, and it is not necessarily identical to a wire header. A Link profile may encode canonical fields into link-native metadata, use a link-scoped `RoutingAlias` or endpoint alias, omit fields that static configuration reconstructs unambiguously, or serialize the complete value. Receive processing shall expand aliases and reconstruct the same canonical `Control`, `RoutingWord`, `EndpointId`, bounded extensions, transport metadata, and payload before delivery upward. A `RoutingAlias` and an endpoint alias are never canonical identity above the LLL.

Whenever a representation serializes a literal multi-byte numeric value, it shall use WireSpaces little-endian order as defined by [Core Protocol and Routing](core_protocol_and_routing.md). Arbitrary-width bit fields and link-native metadata still require profile-exact field significance and packing; native C/C++ object layout is never a wire representation.

An LLL shall not reinterpret application payload bytes. It shall not terminate end-to-end transport state.

### 2.2 Message boundaries and bounds

The LLL service is message-oriented:

- One accepted TX request represents exactly one complete PDU.
- One successful RX delivery represents exactly one complete, link-validated PDU.
- Partial PDUs shall not be delivered upward.
- The generic LLL does not segment an arbitrarily large PDU. A profile-specific adapter may fragment and reassemble internally only when that behavior is part of the named profile.
- Every configured instance shall expose finite TX and RX PDU bounds.
- Length and profile compatibility shall be checked before an implementation reads or writes outside supplied storage.

The effective application-payload limit depends on the service, transport overhead, canonical metadata encoding, and selected LLL profile. Static configuration tooling should reject combinations that cannot fit.

### 2.3 Receive delivery is not callback-dependent

The stable contract requires a bounded receive-delivery port, not a mandatory callback ABI. Valid implementations include:

- a consumer-owned `try_receive`/drain interface;
- a bounded ownership-transfer queue;
- direct publication into an explicitly configured endpoint storage object;
- an optional short callback adapter layered over one of those mechanisms.

No service or LLL profile may require arbitrary application code to run in the LLL execution context. If callbacks are offered, their execution context, work bound, reentrancy rule, and ownership result shall be explicit.

### 2.4 TX acceptance and completion

A TX submission shall return promptly with an explicit result. At minimum, results shall distinguish:

- accepted into bounded LLL-owned state;
- rejected because capacity is temporarily unavailable;
- rejected because the route/profile is invalid;
- rejected because the PDU or length is invalid;
- rejected because the link is disabled or faulted.

Acceptance means that ownership has transferred as documented and the request entered the local transport/link path. It does **not** mean peer delivery, successful physical transmission, or acknowledgment.

After acceptance, the implementation shall eventually do one of the following:

- complete transmission and release/report the accepted object;
- report cancellation caused by an explicit stop or restart;
- report a terminal local fault.

Completion notification may be polled, queued, or delivered through an optional bounded callback. The implementation shall define whether completion means queued to hardware, consumed by hardware, or physically completed. The generic contract does not equate any of those events with end-to-end delivery.

## 3. Ownership, views, and bounded fan-out

A PDU data view and ownership are distinct. A byte span, stream association, scatter/gather list, or payload descriptor states what data may be accessed; it does not by itself state who owns the backing storage, how long it remains valid, or who releases it.

Every PDU backing object has one logical owner at a time unless it is explicitly published as an immutable bounded shared lease.

Two API forms are permitted:

- **Borrowed view:** immutable for a documented call or lease scope. The borrower shall not retain it after that scope or mutate the backing storage.
- **Owned handle:** exclusive ownership is transferred explicitly. The receiver may retain or enqueue the handle and shall eventually release or transfer it.

Requirements:

- Ownership transfer shall be mechanically visible in the API.
- A rejected TX submission leaves ownership with the caller.
- An accepted owning TX submission transfers ownership to the receiving layer.
- A borrowed TX submission may be accepted only if the implementation copies the required bytes before return or documents a bounded lease extending through a later completion event.
- RX storage retained by application code shall remain valid until released, including across task/core transfer and Link Engine restart. The storage allocator therefore needs a lifetime broader than restartable Link Engine state.
- Buffer reclamation shall occur through the allocator's defined owner or synchronization mechanism; arbitrary contexts shall not mutate a shared free list.
- Mutable access after publication is prohibited unless a separate, explicit protocol transfers ownership back.

Zero-copy operation is desirable but not required. Correct ownership and bounded behavior take precedence.

For fan-out to several sinks, an implementation shall either copy, complete synchronous fan-out within one borrow scope, or use an immutable bounded reference/lease mechanism. A shared lease shall define the maximum consumer count, release tracking, storage-reclamation owner, restart behavior, and handling when the sharing bound is exceeded. Unbounded reference growth or dynamic reference counting is not required; copying is the required-safe fallback when a target cannot prove bounded sharing.

The baseline receive and Relay model is bounded copy/store-and-forward: acquire bounded storage, receive the complete profile unit, validate its framing, length, and profile-exact integrity trailer when present, then publish, copy, or transfer ownership. An implementation may avoid an extra copy by receiving into final owned storage, but generic contracts do not require cut-through forwarding or publication before complete validation.

## 4. Stable Logical Link contract

For each configured instance, an LLL shall:

1. accept or reject complete canonical TX PDU values without unbounded waiting;
2. encode accepted values according to one configured named profile;
3. consume Link RX units incrementally;
4. validate profile framing, lengths, and link-level integrity before parsing untrusted PDU fields;
5. reconstruct complete canonical PDU values, including `Control`,
   `RoutingWord`, `EndpointId`, bounded header extensions, transport metadata,
   and payload, and deliver only complete valid values upward;
6. expose bounded progress, drop, parse, integrity, oversize, congestion, and lifecycle diagnostics;
7. support explicit start, stop, reset, and fault handling through its owning Link Engine;
8. preserve message ordering only to the extent promised by the selected profile and Link;
9. keep application/service dispatch outside Link drivers or carrier adapters;
10. avoid hidden dynamic allocation requirements.

An LLL may exploit link-native addressing or priority metadata, but the resulting canonical value shall obey the Wire, Route, and core protocols. Relay forwarding is compatible only when every hop can preserve or unambiguously reconstruct the canonical `RoutingWord`, endpoint identity, resolved Wire and Route Direction, and end-to-end transport semantics. Re-encoding a link-scoped `RoutingAlias` or endpoint alias on a new segment does not change the Wire, Route, or canonical identity.

## 5. Stable Link contract

A Link supplies the smallest useful transfer service below an LLL. Its profile-specific RX/TX unit may be a byte span, frame, datagram, FIFO entry, DMA descriptor, shared-memory record, socket datagram, or RTL stream transaction. The contract describes observable transfer behavior and is neutral to whether hardware, software, or both implement it.

Each instance shall provide:

- bounded RX acquisition or draining;
- bounded TX acceptance with explicit temporary rejection;
- explicit transfer-unit lengths and capacity;
- lifecycle control or integration with equivalent driver lifecycle control;
- fault and progress observations;
- a way to wake or notify the owning execution context when useful;
- ownership and completion semantics for every submitted storage object.

A Link shall not:

- parse endpoint IDs, transport headers, or application payloads;
- dispatch services;
- claim end-to-end delivery;
- silently retain caller-owned storage beyond its documented lease;
- invoke unbounded application work from an ISR.

### 5.1 Capability description

Each configured Link and LLL shall expose an immutable capability description sufficient for static composition. Relevant capabilities include:

- RX and TX transfer-unit kind;
- maximum RX and TX unit size;
- maximum decoded/reassembled PDU size;
- required alignment and contiguous-storage constraints;
- scatter/gather and zero-copy support;
- full-duplex, half-duplex, or simplex behavior;
- ordering and possible duplication behavior;
- link-provided error detection, if any;
- whether TX completion and timestamps are available;
- whether DMA, ISR production, cross-core access, or cache maintenance is involved;
- bounded queue depth or other acceptance limits.

Each configured LLL shall additionally declare, even when the answer is
unsupported or none:

- which canonical QoS levels it supports and how their semantics map to
  link-native priority;
- which `QoSDiscipline` values it supports, the bounds of each local QoS queue,
  and the configurable weight range and scheduling accounting unit where
  applicable;
- whether receiver flow control is absent or present and, when present, its
  credit/accounting bounds, update path, and reset behavior;
- which local congestion observations and remote congestion reports, if any,
  it provides;
- Link scheduling facilities on which its claims depend, including
  multiple pending TX objects, priority-aware selection, preemption, and safe
  cancellation or replacement.

Capabilities describe the local implementation. They are not a substitute for a wire profile and do not imply runtime negotiation.

### 5.2 Generated static projections

The Wiring toolchain may project the generic Link, LLL, routing, and endpoint contracts into target-specific constants, tables, switches, descriptors, state machines, configuration images, or RTL parameters. A conforming implementation is not required to instantiate runtime Link, LLL, Router, Wire, or Endpoint object graphs when generated static logic preserves the same externally visible semantics, validation, bounds, ownership, diagnostics, and compatibility fingerprint.

One authoritative Wiring source should generate every participating projection. Static validation shall reject projections that disagree about a `WireBand`, routing context, endpoint binding, profile, bound, or capability.

## 6. Named immutable profiles and compatibility

Every interoperable LLL configuration shall select a named, versioned profile. A stabilized profile is immutable. Any incompatible change requires a new name/version.

A profile shall specify all interoperability-relevant behavior, including:

- exact field and transfer-unit layout;
- byte and bit order;
- framing, escaping, padding, and reserved-value rules;
- PDU length interpretation and all minimum/maximum bounds;
- trailing FCS placement when an FCS is present, plus its exact algorithm,
  parameters, field order, coverage, serialized form, residue convention, and
  validation order;
- fragmentation/reassembly and timeout behavior, if present;
- mapping between canonical fields, static configuration, and link-native metadata;
- malformed-input and resynchronization behavior;
- required diagnostics;
- representative valid, boundary, and invalid conformance vectors.

Peer compatibility is a static deployment property. Peers shall use the same named profile and compatible bounds/configuration. Profiles are not auto-detected from traffic, and the base architecture does not require profile negotiation.

The [Experimental HDLC Logical Link Profile](hdlc_logical_link_profile.md) retains a trailing-FCS direction but intentionally does not select a generic CRC algorithm.

Static validation should reject:

- profile-name/version mismatch;
- incompatible PDU bounds;
- canonical fields that cannot be preserved or reconstructed;
- incompatible aliases or static mappings;
- service/transport messages that exceed the effective capacity;
- unsupported Link capability requirements.

## 7. Execution and concurrency

### 7.1 One serialized mutable LLL context

Each LLL instance has exactly one logical execution context that mutates parser state, reassembly state, TX scheduling state, timers, and its owned queues/pools. It may run in a bare-metal loop, a dedicated task, or another explicitly serialized executor.

Other tasks or cores shall communicate through bounded submission ports, value queues, ownership-transfer queues, immutable snapshots, or explicitly synchronized control requests. They shall not concurrently call arbitrary state-mutating LLL operations. A value SPSC queue copies or moves a bounded value into queue-owned storage; an ownership-transfer SPSC queue moves an owning handle or lease and changes the storage owner only on successful publication.

Read-only status and latest-state data may be exposed concurrently only through a coherent snapshot, seqlock, or other documented synchronization mechanism. A seqlock is suitable only with a serialized writer; it does not create multi-writer ownership.

### 7.2 Interrupt context

ISR work should be limited to:

- acknowledging hardware;
- capturing a bounded RX unit or completion record;
- publishing it to the owning context;
- waking that context.

Parsing complete PDUs, endpoint dispatch, arbitrary callbacks, blocking, and general allocator/free-list manipulation shall not occur in an ISR unless an implementation defines and verifies a stricter ISR-safe profile. Such an exception does not weaken the generic contract for other implementations.

ISR-to-task publication requires the platform's documented atomic and memory-ordering operations. `volatile` alone is not a synchronization primitive.

### 7.3 Cross-core execution

Cross-core transfer shall use an explicit publication boundary. Before publishing an owned handle, the producer completes all payload writes and performs any required cache clean/release operation. The consumer performs the matching acquire/invalidate operation before reading.

The queue or mailbox implementation shall define:

- producer and consumer identities;
- ordering and atomic-width assumptions;
- cache/coherency requirements;
- full/empty behavior;
- reset behavior while records are in flight.

An implementation may copy when zero-copy ownership cannot be made safe.

## 8. Local delivery and on-device IPC Routes

A Wire whose source and all sinks are in one Endpoint Domain is a same-domain
Wire and requires no Route. Band-0 `PathTag::Local`, where used, supports
generic envelopes and uniform dispatch; it does not create a Route inside the
Endpoint Domain. Its exact `RoutingWord`, Direction, `PeerId`, ingress, egress,
and reconstruction rules belong to
[Core Protocol and Routing](core_protocol_and_routing.md). Local delivery
shall not be emitted on an external Link or accepted from external ingress as
though it were a configured cross-domain Route.

Same-core and cross-core paths within the same Endpoint Domain shall provide the same externally visible protocol semantics:

- the same canonical PDU value and endpoint identity;
- the same application Service roles and configured Wire and endpoint binding;
- the same transport interpretation;
- the same semantic validation and configured local routing policy;
- explicit bounded acceptance and backpressure;
- the same distinction between accepted, rejected, delivered, and consumed.

The implementation mechanism may differ. A same-core path may use direct serialized dispatch; a cross-core path may use shared memory, a FIFO, a queue, or a copy. Optimization shall not make same-core delivery reentrant when the equivalent cross-core path is deferred, unless that execution difference is an explicit endpoint policy.

Two distinct Endpoint Domains on one MCU shall communicate through explicit
cross-domain Wires and one or more explicitly configured Routes with fixed
terminal A and terminal B Endpoint Domains and structural Direction. A Route
may be implemented by shared memory, a hardware mailbox, FIFO, queue, or copy,
but it is subject to the same `RoutingWord`, Direction, Wire, endpoint binding,
and validation rules as an inter-device Route. A Route name or generated key is
manifest-local and is not canonical PDU identity. Physical co-location does not
turn cross-domain communication into same-domain delivery.

Same-domain delivery does not authenticate callers or isolate tasks, processes,
or cores. Authorization requires a platform principal and enforcement
boundary, such as MPU/MMU permissions or authenticated IPC. Without such a
boundary, all code able to submit to the local path is trusted. Bounds,
descriptor, Wire, endpoint binding, payload/schema, and routing validation
still apply, but those checks shall not be represented as caller
authorization. Local corruption protection may be omitted only as a named
local profile decision.

### 8.1 Master-initiated I2C and SPI Links — feasibility only

I2C and SPI are feasible master-initiated Links. The participant that initiates, clocks, reads, or writes a physical transaction is not thereby the semantic producer of the carried Wire. Producer identity and source authority come from the resolved Wire and endpoint binding. A peripheral may therefore produce status or events while a host/master Link Engine initiates the transaction that observes them.

Plausible realizations include polling a coherent latest-value snapshot and draining a bounded event queue or FIFO. Latest-value polling may skip intermediate publications by design; an event queue shall retain distinct events up to its stated bound and define overflow and successful-pop behavior. An endpoint-selection transaction may be Link control rather than an application command.

No stable I2C or SPI Link profile is defined here. Exact byte formats, native-address/chip-select mappings, failed-read behavior, event-pop/consumption semantics, integrity and any trailing FCS, polling and transaction timing, reset/abort recovery, and optional IRQ/attention signaling are deferred to named profiles.

Physical descriptions such as *board-local*, *same PCB*, or *local peripheral bus* do not imply Band-0 `PathTag::Local`. If the host and peripheral belong to distinct Endpoint Domains, their communication uses explicit cross-domain Wires and Routes with an ordinary non-Local routing context, regardless of physical proximity.

## 9. Snapshot, queue, and ownership semantics are separate

Storage policy is above the generic LLL contract:

- A **snapshot** retains the latest complete value. A newer publication may replace an older unread value. Readers observe a coherent value but are not promised every update. A single-writer snapshot may use a seqlock when the platform memory model makes that safe.
- A **value SPSC queue** copies or moves each accepted bounded value into queue-owned slots. Its full/empty and overflow behavior is explicit; it does not silently retain a caller's borrowed PDU view.
- An **ownership-transfer SPSC queue** publishes owning handles or bounded leases. A failed push leaves ownership with the producer; a successful push transfers it to the queue/consumer path; a successful pop transfers it to the consumer.
- A **queue/event stream** retains discrete accepted events in order up to a bounded capacity. Overflow has an explicit reject, drop, or replacement policy.

Neither may masquerade as the other. In particular:

- a snapshot replacement is not a queue delivery;
- queue exhaustion shall not silently become latest-value replacement;
- snapshots and live status require coherent publication;
- value copying shall not be represented as backing-storage ownership transfer;
- a handle queue shall not be represented as borrowing;
- event, command, and reliable-protocol state normally require non-replacing storage.

Callbacks are only notification mechanisms and do not define storage or ownership semantics.

## 10. QoS, acceptance, flow control, and congestion

### 10.1 Four distinct normative contracts

The following mechanisms are distinct and shall not be represented as
interchangeable:

1. **QoS** is the canonical relative traffic intent used for local scheduling
   and link-native arbitration mapping. QoS does not grant buffer capacity,
   guarantee acceptance, or prove receiver readiness.
2. **Local backpressure and TX acceptance** report whether this local bounded
   submission path accepted a PDU now. Temporary rejection means that the PDU
   did not enter the local transport/link path; it says nothing by itself about
   remote receiver capacity.
3. **Receiver flow control** is an explicit receiver-controlled mechanism that
   bounds outstanding consumption of receiver or hop storage. It is independent
   of QoS: a high-QoS PDU cannot bypass, manufacture, or reinterpret receiver
   credit.
4. **Congestion reporting** publishes pressure, utilization, queue, rejection,
   drop, or similar observations so policy may adapt offered load. It is
   advisory unless a named protocol explicitly makes a report part of its
   control state; delayed congestion telemetry shall not substitute for
   receiver flow control where correctness depends on bounded receiver storage.

Temporary TX rejection is a normal runtime condition. No caller may assume
submission succeeds, busy-spin indefinitely, or advance protocol state as
though a rejected PDU entered the link. Local TX rejection is also distinct
from a transport timeout: rejection means the PDU never entered the local
transport/link path, while a timeout applies only after transport acceptance.

### 10.2 Canonical QoS levels and defaults

[Core Protocol and Routing](core_protocol_and_routing.md) owns the QoS field in
`Control` and defines these four canonical levels:

```text
QoS 0   Background / Bulk
QoS 1   Normal
QoS 2   High Priority
QoS 3   Critical Priority
```

The values express semantic ordering, not a required link-native numeric
encoding. An LLL may invert or otherwise map the values to native priority but
shall preserve their relative meaning.

A Service or Endpoint contract defines its default QoS as specified by
[Application Protocols and Services](application_protocols_and_services.md).
`Normal` is the ordinary Service default unless that contract states another
level. The Wiring Manifest may provide a deployment override. The effective
QoS after that override is the value presented to the LLL; static validation
shall reject an effective level or native mapping that the selected LLL cannot
preserve.

### 10.3 QoS scheduling disciplines

QoS level and local scheduling discipline are separate. The standard
disciplines are:

```text
QoSDiscipline::WeightedFair
QoSDiscipline::StrictPriority
```

`QoSDiscipline::WeightedFair` shall provide weighted progress among
continuously backlogged, transmission-eligible local QoS queues while local
transmission opportunities continue. It may use weighted round-robin or a
behaviorally equivalent bounded scheduler. Weights shall be finite, positive,
and configurable by the LLL or deployment; the implementation shall declare
its accounting unit. Unlike strict priority, this discipline shall not
intentionally starve an eligible lower-QoS queue in the local scheduler.

`QoSDiscipline::StrictPriority` shall select the highest-QoS non-empty,
transmission-eligible local queue at every local scheduling opportunity:

```text
Critical
else High
else Normal
else Background
```

Under sustained higher-priority load, lower-priority traffic may starve. That
is an explicit property of `StrictPriority`, not an implementation defect.

Neither discipline automatically provides a deadline, latency bound, bus-wide
fairness, or real-time guarantee. Such a guarantee requires complete
deployment analysis, including message rates and sizes, queue and buffer
bounds, hardware behavior, other participants, link utilization, and
implementation timing.

### 10.4 CAN scheduling requirements

A CAN LLL shall preserve the semantic QoS ordering when mapping canonical QoS
to CAN arbitration identifiers or other native priority metadata. Because a
lower numerical CAN identifier wins arbitration, the numeric native mapping
may differ from the canonical numbering.

The local scheduler shall avoid avoidable priority inversion and FIFO
head-of-line blocking. A lower-QoS frame that is queued or pending shall not
prevent a newly available higher-QoS frame from becoming eligible for the next
arbitration opportunity when the controller provides multiple TX objects,
priority-aware pending selection, or safe cancellation/replacement. The LLL
shall use those facilities where available and compatible with correct
ownership and completion reporting. A single hardware FIFO that allows
Background traffic to head-of-line block Critical traffic is not by itself a
conforming multi-QoS scheduler; an implementation with an unavoidable
controller limitation shall declare that limitation and shall not claim the
unsupported scheduling capability.

Local scheduling and shared-bus arbitration remain distinct. `WeightedFair`
can govern local admission and eligibility but cannot create bus-wide fairness;
traffic from every CAN participant arbitrates independently, and a frame that
has won arbitration is not locally preemptible. The shared bus may therefore
starve a low-priority frame even when the local scheduler provides weighted
progress.

Baseline CAN has no generic LLL credit-flow-control requirement. Services and
transports that require receiver-controlled resource protection shall provide
an explicit bounded mechanism above the baseline CAN LLL. Congestion reports
may aid management but are not receiver credit.

### 10.5 Generic receiver-credit contract

An LLL or named profile that claims receiver-credit flow control shall satisfy
all of the following:

- credit represents a bounded quantity of receiver storage, using a
  profile-defined fixed storage quantum and accounting rule rather than an
  unqualified count of variable-size PDUs;
- a sender shall consume sufficient granted credit before transmitting an
  ordinary PDU and shall never exceed, infer, or invent the peer's grant;
- credit is returned or increased only according to defined receiver storage
  release and publication rules;
- credit counters, grants, update messages, in-flight use, and reset behavior
  are finite, fail closed on ambiguity, and have explicit overflow and
  underflow handling;
- credit eligibility is independent of QoS scheduling: QoS selects among
  eligible traffic but never creates receiver capacity;
- credit updates may be piggybacked, but a bounded standalone Link-control
  update shall exist so a quiet peer can reopen a stalled data path;
- credit restoration and essential Link management shall have a bounded
  lifeline path that does not depend solely on ordinary data credit;
- lifeline capacity shall correspond to receiver capacity permanently reserved
  from ordinary traffic, shall never manufacture ordinary credit, and shall not
  carry arbitrary application or bulk traffic.

Credit flow control is hop-by-hop. A Relay or gateway may reduce or withhold
ingress credit as its bounded forwarding storage fills; that does not create a
global congestion algorithm or end-to-end transport guarantee. The
experimental HDLC LLL shall instantiate this generic contract before its
receiver flow control can be stabilized; its exact quantum, encoding, update,
and lifeline parameters remain profile-owned.

### 10.6 Bounded congestion behavior

Every transmitting Service or transport shall define bounded behavior, such
as:

- retain and replace one pending latest value;
- retain exact protocol state and retry later;
- use a bounded FIFO with a defined overflow result;
- drop or aggregate best-effort diagnostics while incrementing a counter.

An LLL shall expose the local congestion observations declared by its
capability description. These may include available slots, occupancy,
high-water mark, rejected count, dropped count, and replaced count.
Notification of newly available capacity may be polled, scheduled, queued, or
callback-based; it is not required to be a callback.

## 11. Transport profiles

### 11.1 Unreliable Datagram — stable and completed

Unreliable Datagram is the only completed transport contract in the initial architecture.

It provides:

- one bounded discrete message per PDU;
- no transport header;
- no retransmission or acknowledgment;
- no connection state;
- no delivery, uniqueness, or ordering guarantee beyond what lower layers happen to provide.

Successful local acceptance does not imply remote receipt. Applications that need freshness, duplicate handling, transactions, or retry shall define those semantics themselves or select a future standardized transport.

### 11.2 Sequenced/E2E-Protected Datagram — Provisional

Sequenced/E2E-Protected Datagram is a candidate future transport. Candidate metadata includes an alive/sequence counter and an end-to-end check value. Exact fields, coverage, state, reset behavior, acceptance window, diagnostics, and profile ID are unresolved.

No implementation shall claim interoperability from this document. A future profile must define all serialization and conformance vectors before stabilization.

E2E integrity is distinct from LLL integrity:

- LLL integrity detects corruption in one link/profile transfer or reassembly.
- E2E integrity protects selected producer-to-consumer semantics across Relay forwarding and re-encapsulation.

Neither mechanism provides cryptographic authentication.

### 11.3 Reliable Message Transport — wholly TBD

Reliable transport is intentionally undefined. This document defines no reliable header, sequence space, epoch, acknowledgment format, retry algorithm, flow control, timing, connection identity, restart behavior, or wire profile.

Older draft field sketches are not a contract and shall not be implemented as though they were one. Reliable transport shall be designed from concrete service requirements and published as a separate named profile with state-machine and conformance tests.

Until then, services may implement application-specific request/retry behavior over Unreliable Datagram, but shall not label it the standardized Reliable Message Transport.

## 12. Integrity and security distinctions

The following claims are separate:

- **Hardware error detection** concerns one physical/link transfer.
- **LLL integrity** concerns one encoded or reassembled LLL PDU.
- **E2E integrity** concerns accidental corruption or sequencing across the configured service path.
- **Cryptographic authentication** establishes an authorized origin and integrity against an attacker.
- **Confidentiality** prevents unauthorized reading.
- **Replay protection** rejects previously valid authenticated messages under defined freshness rules.

A CRC provides error detection, not authentication, confidentiality, authorization, or cryptographic replay protection. The core stack makes no default cryptographic-security claim. Secure carriers or explicitly designed secure services may be composed at an appropriate trust boundary.

## 13. Non-normative implementation guidance: bounded data planes

One suitable implementation uses statically allocated packet storage, one owner per global pool/free list, value SPSC queues for copied/moved records, and ownership-transfer SPSC queues for handles or leases.

This can provide deterministic memory use and zero-copy transfer, but it is **not** part of the LLL API or wire contract. Fixed ring value queues, linked pooled ownership queues, coherent snapshots/seqlocks, copied mailboxes, bounded shared leases, or other verified bounded mechanisms are all permitted.

If pooled linked SPSC queues are used:

- each queue has exactly one producer and one consumer;
- the pool/free list has one owner;
- handles have one owner at a time;
- publication is the synchronization boundary;
- pool and queue exhaustion have explicit behavior.

The initial/reference implementation should favor the simplest mechanism that can be verified on the target.

## 14. Generic invariants

1. PDU data views describe access, not ownership or lifetime.
2. Every backing object has one owner unless an explicit immutable bounded
   shared lease tracks all consumers.
3. Failed ownership publication leaves ownership with the producer; successful
   publication transfers it exactly once.
4. Value SPSC and ownership-transfer SPSC queues are distinct contracts.
5. Snapshots/seqlocks provide coherent latest-state semantics, not event
   retention or multi-writer serialization.
6. Copy/store-and-forward after complete profile validation is the baseline;
   zero-copy and cut-through are optional optimizations.
7. Literal multi-byte numeric serialization is little-endian; arbitrary-width
   packing remains named-profile exact.
8. Link and LLL contracts are implementation-neutral and do not require
   runtime object graphs.
9. Generated static projections preserve the same Wire Space semantics and
   fail closed on mismatch.
10. Board-local physical placement does not make a cross-domain Wire Band-0
    `PathTag::Local`.
11. Unreliable Datagram is the only completed transport contract; Reliable
    Message Transport remains wholly TBD.
12. HDLC, I2C, and SPI wire/profile details described here remain experimental
    or feasibility-only until separately stabilized.

## 15. Open issues

The following data-plane and profile issues remain open:

- exact public API types for borrowed views, owned handles, bounded shared
  leases, and completion;
- exact value-SPSC, ownership-transfer-SPSC, snapshot, and seqlock memory-model
  contracts on each platform;
- bounded fan-out release tracking, generation/ABA protection, restart
  interaction, and fallback-copy policy;
- generated-projection schemas, capability fingerprints, and cross-artifact
  consistency checks;
- exact trailing-FCS contracts for each named Link profile; there is no generic
  CRC algorithm;
- byte-exact I2C and SPI profiles, including failed-read/pop, integrity, timing,
  reset, and attention behavior;
- whether any safe cut-through profile is needed beyond the copy/store-and-
  forward baseline.

The generic architecture is not implementable safely across every target until the following platform contracts are made explicit:

- minimum atomic widths, alignment, and lock-free assumptions;
- release/acquire operations and required interrupt/core barriers;
- cache coherence assumptions for shared-memory queues and snapshots;
- DMA clean/invalidate ownership transitions and descriptor ordering;
- whether shared packet arenas may be cached;
- allocator lifetime and reclamation across Link Engine restart;
- handle generation/ABA protection when compact indices are reused;
- behavior when a producer or consumer resets with records in flight;
- permitted borrowing across asynchronous DMA and execution-context boundaries;
- MPU/MMU permissions and isolation for shared storage;
- required behavior on non-coherent multicore systems;
- whether diagnostic counters are atomic, serialized, or snapshot-copied.

These are explicit gaps, not implementation details that may be assumed away. Each platform adapter shall document and test its choices. Cross-core and DMA-backed profiles cannot be promoted to stable implementation status until their applicable items are resolved.

## 16. Generic conformance expectations

An implementation of these contracts should be tested for:

- every TX acceptance and rejection result;
- ownership retention on rejection and release after acceptance;
- RX truncation, oversize, malformed, and integrity failures;
- queue/pool exhaustion without unbounded work or corruption;
- parser progress under arbitrary chunking;
- ISR-to-task and cross-core publication ordering;
- restart with Link and LLL work in flight;
- survival and eventual release of application-owned RX buffers across restart;
- same-core and cross-core same-domain Wire semantic equivalence within one
  Endpoint Domain;
- explicit cross-domain Wires and Routes between distinct Endpoint Domains on
  one device;
- snapshot replacement versus queue overflow behavior;
- value-SPSC copy/move semantics versus ownership-transfer-SPSC handle
  semantics, including failed publication;
- bounded lease fan-out, release, restart, and fallback-copy behavior;
- generated software and RTL/static projections producing equivalent canonical
  values and fail-closed mismatch results;
- little-endian literal multi-byte serialization on little- and big-endian
  implementation models;
- board-local cross-domain I2C/SPI configurations rejecting Band-0
  `PathTag::Local`;
- all four canonical QoS levels and Service-default/deployment-override
  resolution;
- `WeightedFair` weighted local progress with configured weights and
  `StrictPriority` highest-queue-first behavior, including documented
  starvation cases;
- CAN QoS mapping, higher-QoS eligibility, multiple-TX-object/cancellation
  behavior where available, and unavoidable controller limitations;
- separation of local TX rejection, receiver credits, and congestion reports;
- bounded receiver-credit accounting, standalone updates, reset behavior, and
  lifeline operation without invented ordinary credit;
- static rejection of incompatible profile, capacity, and service combinations.

Wire-profile conformance is additional to these generic tests. System-level link, transport, congestion, restart, and archetype coverage belongs in [Prototype and Validation](prototype_and_validation.md).

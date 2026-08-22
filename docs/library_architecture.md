# WireSpaces — Core Library Architecture Sketch

**Status:** Private first draft; provisional throughout; structure and API shape only, no internals  
**Purpose:** How the prototype core library is organized, where its seams are, and what its public surface looks like  
**Authority:** Software structure only. Protocol behavior belongs to `CORE`, byte layout to `BITS` and `LINK`, test policy to `CONFORM`

Cross-references use the document code plus a section number, for example `LIB §3`. A bare `§x` always means the current document.

This sketch is a starting point for prototyping, not a decision record. Everything named here is provisional in the sense of `CONFORM §1.1`: an identifier appearing below has not settled anything, and the open items it touches stay open in `REG §6`. The type and function set is expected to grow; what should *not* grow casually is the seam count in §3 and the dependency direction in §2.

---

# 1. Scope and Ground Rules

The library is the thing between a Link driver and a Service. It owns the canonical PDU representation, forwarding, Endpoint storage, and the Link-facing contracts. It does not own the application, the schedule, or the transport medium.

| In scope | Out of scope |
|---|---|
| canonical descriptor encode/decode | Service schemas and codecs |
| Endpoint storage and dispatch | task creation, threading, loop structure |
| forwarding table and local delivery | hardware drivers themselves |
| Link and LLL contracts | host tooling, Organizer, telemetry schemas |
| static configuration types | anything in `FUTURE` |

Four rules follow from the project's C++ standards and from `CORE`, and they constrain the whole design rather than any one module:

```text
no dynamic allocation after init      storage is caller-provided or static
no exceptions, no RTTI                results are returned, never thrown
no application code in the accept path DISP-2, and it is structural, not advisory
one owner per buffer at a time         OWN-1, visible in signatures (§4.4)
```

The third is the one that shapes the API most. Because acceptance may not run Service code, the Service-facing and framework-facing surfaces of an Endpoint are genuinely different interfaces on one object, and §5 and §6 treat them separately for that reason.

---

# 2. Module Structure

Seven modules, with a strictly downward dependency direction. An arrow means "may include":

```text
  application / Services
        |
        v
+---------------------------------------------+
|  endpoint     Queue, Snapshot, Dispatcher   |
+---------------------------------------------+
        |                        ^
        v                        |
+------------------+   +---------------------+
|  forwarding      |   |  config             |
|  Router, Engine  |<--|  static wiring      |
+------------------+   +---------------------+
        |                        |
        v                        v
+---------------------------------------------+
|  link         LinkDriver, LogicalLink,      |
|               LinkCapabilities              |
+---------------------------------------------+
        |
        v
+---------------------------------------------+
|  core         Pdu, descriptor codec,        |
|               identity types, results       |
+---------------------------------------------+
        |
        v
+---------------------------------------------+
|  platform     clock, lock policies          |
+---------------------------------------------+

  profile/can, profile/bytestream    depend on core + link only
  telemetry                          depends on core only
```

`core` is the load-bearing choice. It depends on `platform` and nothing else, contains no policy, and is the only module a conformance vector generator or a host codec needs. If `core` ever needs to include something from `link` or `endpoint`, a type is in the wrong module — most likely a Link-scoped or delivery-scoped concept that drifted into the canonical set.

Two dependencies are deliberately *absent* and worth stating because both are easy to add by accident:

- **`link` does not depend on `forwarding`.** An LLL receiving a PDU hands it to an `IngressSink` (§7.3), which the forwarding engine implements. Without that interface the two modules become mutually dependent and neither is testable alone.
- **`endpoint` does not depend on `link`.** A transmit Endpoint holds a `PduSink&`, not a `LogicalLink&`. This is what lets an Endpoint be tested against a recording sink, and it is the same indirection `CORE §10.3` asks for at the Service level.

## 2.1 Namespaces and headers

The existing simulator establishes the convention: `wirespaces::sim` under `include/wirespaces/sim/`. Core protocol types are the library's main subject, so they sit directly in `wirespaces`, with adjuncts one level down:

```text
wirespaces              canonical types, endpoints, forwarding, results
wirespaces::link        driver and LLL seams, capabilities
wirespaces::can         Classical CAN profile
wirespaces::platform    port seam
wirespaces::sim         existing host simulator
```

Two layers maximum, per the project style. Headers follow declarations-first with out-of-line template definitions below a banner in the same file.

## 2.2 Reuse from `Design/Firmware`

`Design/Firmware` already contains most of the utility layer this library would otherwise invent, and reusing it is worth more than the convenience: a container that has been exercised by the bootloader and HDLC code is better tested than a fresh one, and one `Span` across the whole codebase avoids the conversion boilerplate that two would create.

**Direct reuse, no changes needed:**

| Component | Use here |
|---|---|
| `util/span.h` — `util::Span<T>` | every borrowed view in this document. There is no WireSpaces `Span` |
| `data_structures/ring_buffer.h` — `RingBuffer<T, N>` | Queue Endpoint storage (§5.3) |
| `hal/clock.h` — `hal::PlatformClock` | arrival timestamps and cadence (§9) |
| `util/static_string.h` — `util::StaticString<N>` | Wire and Link names in diagnostics, not on any wire |
| `interfaces/stream_interface.h` — `Stream::StreamInterface` | the byte-stream Link driver case (§8.1) |
| `crc/crc_algorithm.h` | the CAN aggregate CRC (see below) |

`RingBuffer` is a better fit than it first appears, and for a reason worth recording. It documents itself as safe for **one producer and one consumer**, which is exactly the multiplicity `DISP-10` fixes for a receive Queue: the framework writes, one Service reads. It also allocates `Capacity + 1` slots rather than sacrificing one, so its logical capacity is its usable capacity — which is what `CORE §9.5` asks for. Where a Queue Endpoint declares multiple writers, the serialization policy of §9 wraps the enqueue side and the ring's own guarantee covers the rest.

**The CRC finding is the most useful one.** `LINK-6` needs CRC-8 at `N = 2..4` and CRC-16 at `N = 5..8`, and `REG §6.8` lists the parameters as open. `crc/crc_algorithm.h` already provides tested candidates:

```text
crc::algorithm::AutosarCrc8       poly 0x2F, init 0xFF, xorout 0xFF
crc::algorithm::Crc16CcittFalse   poly 0x1021, init 0xFFFF, xorout 0x0000
```

Both are non-reflected, which matters because `crcBitwise` has a `static_assert` against reflection — so these two work today and a reflected polynomial would not. That does not close `REG §6.8`; it means the choice can be made from implemented, vector-tested options rather than from a table in a datasheet.

One real gap: the implementation has no incremental `update`/`finalize`, only a whole-buffer `compute`, and its own TODO says so. A PDUA CRC covering a PDU that arrives as up to eight separate frames wants to accumulate across them rather than reassemble first and then checksum. Reassembling first works and is the right phase-1 behavior; incremental CRC is a later optimization with a clear trigger.

**Needs work before reuse:**

- `data_structures/memory_pool.h` does not currently compile — a missing brace after the namespace, a `.data()` call on a pointer, and an assignment to a reference member. It is also not needed in phase 1, since nothing pools. Worth knowing before someone reaches for it.
- `RingBuffer` is declared at global scope rather than in `data_structures`, against the project's own namespace rule. A one-line fix, but it touches its existing users.
- `can/can_frame.h` is not the right base for the CAN profile. `Can::CanFrame` carries a `crc_` member and HDLC payload framing because it was built for the bootloader's CAN-over-HDLC transport, and its identifier is a template parameter constrained to an enum. WireSpaces needs a plain 11-bit identifier, a DLC, and eight bytes. Either a minimal frame type here or a refactor there — not a direct reuse.
- `util/integer.h` is the interesting one. `util::Uint24_s` is exactly the pattern the identity types in §4.1 want: a narrow value in a wider container, with `isValid`, and serialize/deserialize that *fails* rather than truncating. Its TODO asks for the general form — `UintN` in any container, with range checks — and that general form is what WireSpaces needs for a 10-bit WireNumber, a 5-bit NodeId, and a 2-bit QoS. Resolving that TODO is probably the single highest-leverage change in `Firmware/util` for this project, because it makes `PDU-2`'s reject-don't-mask behavior a property of the type rather than a rule each codec has to remember.

**Not reusable, and mentioned only so nobody looks twice:** `bootloader/`, `hdlc/protocol.h`, and `hdlc/network_management.h` are application protocols. `Firmware/wirespaces/host_demo/` is an earlier C prototype of an unrelated generation.

**Adjacent and worth revisiting later:** `hdlc/hdlc.h`'s `Receiver` is a byte-oriented framing state machine over a ring buffer that already makes progress under arbitrary chunk boundaries — the exact property `CONFORM §2` calls out as invisible to any test that hands a parser whole messages. When `LINK §3` becomes real, that is the starting point rather than a new parser. `data_structures/array_list.h` is a candidate for bounded tables such as learned bindings and reply contexts, neither of which exists in phase 1.

---

# 3. The Seams

The mechanism used at each boundary matters more than the boundary itself, so this is the section to disagree with early. Three mechanisms, chosen per seam by how often the boundary is crossed and whether the crossing needs to be substitutable at link time:

| Seam | Mechanism | Why |
|---|---|---|
| Link driver | pure virtual | per-hardware, must be mockable; crossed once per frame |
| Logical Link (LLL) | pure virtual | per-profile, substitutable; crossed once per PDU |
| Endpoint accept | pure virtual (`EndpointSink`) | heterogeneous Endpoint types behind one dispatch table |
| Endpoint read/write | non-virtual template | the portability contract (`SVC-9`); crossed per message by Service code |
| Writer serialization | template policy | must compile to nothing when a writer is exclusive |
| Clock | template policy or virtual | see §9 |
| Forwarding table | plain value type | read-mostly, no substitution needed yet |

Two of these deserve their reasoning recorded, because both will look wrong at some point.

**Endpoint accept is virtual, Endpoint read is not.** The dispatcher holds Endpoints of unrelated types — a 512-byte snapshot, an 8-byte command queue — behind one table, and that is exactly what a vtable is for. One indirect call per accepted message is negligible beside the payload copy occurring in the same operation. The Service-facing side is the opposite case: it is the surface `SVC-9` says must be portable and it is called by application code in tight loops, so it is a concrete templated API with no indirection. The consequence is a slightly unusual class shape, and it is intentional — see §6.1.

**Serialization is a template policy, not a virtual.** `DISP-13` requires an Endpoint Domain to provide serialization for concurrent writers, but the common case on a small target is a single writer needing none. A policy parameter compiles the exclusive case down to nothing, which is precisely the property `CONFORM §2` wants tested ("an exclusive Endpoint with the synchronization it does not need omitted"). A virtual lock could not deliver that.

---

# 4. `core` — Canonical Types

## 4.1 Identity

Raw `uint16_t` for four different identity spaces is how a WireNumber ends up in an EndpointId parameter. Two things are wanted here: distinct types, and a range check that rejects rather than truncates. `BITS §2` allocates 10 bits to a WireNumber and 5 to a NodeId, so a value that does not fit is a real and reachable error, and `PDU-2` says the answer is rejection.

`util/integer.h` already has the shape — `util::Uint24_s` is a narrow value in a wider container with `isValid` and a `serialize` that returns `nullopt` instead of a truncated result — and its own TODO asks for the general form. Assuming that TODO is resolved into a `util::UintN<Bits, Container>`:

```cpp
struct WireNumber { util::UintN<10U, uint16_t> value{}; };  ///< BITS §2
struct NodeId     { util::UintN<5U,  uint8_t>  value{}; };  ///< BITS §2
struct EndpointId { uint16_t value{0U}; };                  ///< full width, no narrowing
struct WireAlias  { uint8_t  value{0U}; };                  ///< Link-scoped, not on the canonical wire
struct LinkIndex  { uint8_t  value{0U}; };                  ///< local array index, never transmitted

enum class Direction     : uint8_t { kOriginToNode = 0U, kNodeToOrigin = 1U };
enum class Namespace     : uint8_t { kNs0 = 0U, kNs1, kNs2, kNs3 };
enum class TransportType : uint8_t { kUnreliableDatagram = 0U /* ... */ };
enum class Qos           : uint8_t { kCritical = 0U, kHigh, kNormal, kBackground };
```

The value of that dependency is that range checking becomes a property of the type instead of a rule the codec has to remember at each of eight field boundaries. If the TODO is not resolved first, the wrappers hold plain integers and the codec range-checks explicitly — same behavior, more places to forget it. That is the fallback, not the plan.

`Qos` values are written out because the numbering is the one place where a plausible-looking mistake is silent: `kCritical = 0` follows `CORE §14`, and the old ascending order produces a system running with its priorities exactly inverted (`REG §5`). Naming the enumerators rather than using bare integers makes the inversion a compile-time visible thing.

## 4.2 Descriptor

The decoded form is a plain container; the codec is free functions. No bitfields anywhere near it (`BITS §1.1`, `PDU-4`).

```cpp
/// Decoded canonical base descriptor. Host representation only - never overlaid on wire bytes.
struct PduDescriptor {
    static constexpr size_t kEncodedSize{5U};

    Qos           qos{Qos::kNormal};
    Namespace     name_space{Namespace::kNs0};
    bool          has_header_extensions{false};
    TransportType transport{TransportType::kUnreliableDatagram};
    NodeId        node_id{};
    Direction     direction{Direction::kOriginToNode};
    WireNumber    wire{};
    EndpointId    endpoint_id{};

    bool isValid() const;
};

/// Encode to exactly kEncodedSize bytes. Returns false if out is too small.
bool encode(const PduDescriptor& in, util::Span<uint8_t> out);

/// Decode from at least kEncodedSize bytes. Returns false on a reserved-field
/// or range violation; out is then unmodified (PDU-2 rejects rather than masks).
bool decode(util::Span<const uint8_t> in, PduDescriptor& out);
```

Returning `bool` with an out-parameter rather than an optional keeps the style uniform across a codebase with no exceptions, and makes the reject-don't-mask behavior of `decode` the obvious reading.

## 4.3 PDU and received PDU

Egress and ingress carry different context, so they are different types rather than one type with unused fields:

```cpp
/// A complete canonical PDU offered to a sink. Borrowed view; see §4.4.
struct Pdu {
    PduDescriptor             descriptor{};
    util::Span<const uint8_t> header_extensions{};
    util::Span<const uint8_t> payload{};
};

/// A PDU that arrived, with the local context acceptance needs.
struct ReceivedPdu {
    Pdu       pdu{};
    LinkIndex ingress{};       ///< kLocalDomain for domain-internal delivery
    Timestamp arrival{};       ///< captured by the LLL, not by the Endpoint (CORE §9.4)
};
```

Arrival time is captured on the ingress path and carried, rather than read inside `tryAccept`. This matters for the RTL and offload cases where timestamping happens far below the Endpoint, and it keeps `tryAccept` free of a platform dependency.

`Timestamp` is deliberately not `hal::PlatformClock::TimePoint`, even though that is where it comes from. A 64-bit nanosecond time point costs eight bytes in every stored slot, and `CORE §9.3` budgets roughly four for arrival time — on a Queue of thirty-two eight-byte messages that difference is a noticeable fraction of the Endpoint. So `Timestamp` is a narrower tick, converted from the platform clock once on the ingress path:

```cpp
struct Timestamp { uint32_t ticks{0U}; };   ///< resolution and epoch: REG §6.7
```

Which resolution, and what a wrap means for a staleness comparison, are open (`REG §6.7`). What is settled is that the conversion happens at one point and the stored form is not the platform form.

## 4.4 Views and ownership

`util::Span<const uint8_t>` is a borrowed view valid for the duration of one call, and the copy-based baseline (`OWN-3`) means every accept and submit path copies what it keeps before returning. That is the whole ownership story for phase 1, and it is worth being explicit that this is a *phase* rather than a design: the owned-handle shape that `CORE §16.1` describes is deliberately absent, so there is nothing yet whose ownership can be lost.

The one rule that must hold even in a copy-only world, because it is what a later zero-copy path will be checked against:

```text
a rejected submission leaves ownership with the caller, for every reject reason
```

With copies this is trivially true. Encoding it in tests now (`CONFORM §2`) is what makes it verifiable later.

## 4.5 Results and reasons

Two enumerations with a documented mapping, rather than one of either:

```cpp
/// Outcome of a submission. Congestion is an ordinary outcome, not a fault (QOS-3).
enum class SendResult : uint8_t {
    kAccepted = 0U,
    kCongested,
    kLinkUnavailable,
    kUnrepresentable,
    kUnknownWire,
    kUnauthorized,
    kInvalidArgument,
};

/// Registry of every counted rejection and drop. Stable numbering; append only.
enum class Reason : uint16_t { /* ... */ };

Reason toReason(SendResult result);
```

`SendResult` stays small enough for an exhaustive `switch` at a call site. `Reason` is the wide registry that telemetry and counters index, so there is one table rather than two drifting ones — the provisional answer to the question in `REG §6.7`. The mapping function is where they are kept honest, and `CONFORM §4` requires tests to assert the reason rather than merely that something was rejected.

---

# 5. `endpoint` — the Service-Facing API

This is the surface `SVC-9` makes a portability contract, so it is also the surface whose names are most worth arguing about before Services depend on them (`REG §6.12`).

## 5.1 Payload representation in phase 1

Endpoints store **bytes with a declared maximum**, and the Service decodes on read. Typed Endpoints are deferred.

The reason is not conservatism. Decoding at acceptance would require a generated codec, there is no schema language yet (`REG §6.15`), and hand-written decode called from `tryAccept` puts unbounded-by-construction work in a Link's context — the exact thing `DISP-2` exists to prevent. Storing bytes keeps acceptance a validate-copy-count operation, keeps the storage size obviously static, and defers the `T` naming question that `REG §6.12` flags rather than answering it by accident.

The consequence to accept openly: phase-1 Service code sees a byte span and does its own decode. A typed façade layers on top later without the framework side changing, which is the property worth preserving now.

## 5.2 Declared metadata

Metadata level is a template parameter, and the accessor type differs by level so that **reading an undeclared field is a compile error** rather than a zero — `REG §6.12` states the preference and this is the cheapest way to honor it:

```cpp
enum class MetadataLevel : uint8_t { kPayloadOnly, kWithSource, kFull };

template <MetadataLevel Level> struct DeliveredMetadata;   // specialized per level

template <> struct DeliveredMetadata<MetadataLevel::kPayloadOnly> {};  // empty, costs nothing

template <> struct DeliveredMetadata<MetadataLevel::kWithSource> {
    WireNumber wire{};
    Direction  direction{Direction::kOriginToNode};
    NodeId     node_id{};
    LinkIndex  ingress{};
};
// kFull adds Qos, TransportType, extension presence, and arrival.
```

A Service written against `kPayloadOnly` that tries to read `.wire` fails to build, which is the right outcome and is not achievable with a runtime "unavailable" flag. Snapshot Endpoints force at least arrival time regardless of declared level, per `CORE §9.3`.

## 5.3 Receive Endpoints

```cpp
template <size_t Capacity,
          size_t MaxPayload,
          MetadataLevel Level = MetadataLevel::kPayloadOnly,
          typename Serialization = NoSerialization>
class QueueEndpoint final : public EndpointSink {
public:
    struct Message {
        DeliveredMetadata<Level> metadata{};
        uint8_t                  payload[MaxPayload]{};
        uint16_t                 length{0U};
    };

    /// @brief Remove the oldest message, if any. Called only from the consuming Service.
    /// @return false when empty. Exactly one Service may call this (DISP-10).
    bool tryPop(Message& out);

    size_t size() const;
    size_t highWaterMark() const;
    uint32_t rejectedCount() const;

private:
    /// Framework-facing. Private so Service code cannot reach it; the Dispatcher
    /// calls it through EndpointSink&. Performs no application work (DISP-2).
    AcceptResult tryAccept(const ReceivedPdu& in) override;

    RingBuffer<Message, Capacity> queue_{};   ///< see §2.2
};

template <size_t MaxPayload,
          MetadataLevel Level = MetadataLevel::kFull,
          typename Serialization = NoSerialization>
class SnapshotEndpoint final : public EndpointSink {
public:
    struct Value { /* metadata, payload, length, as above */ };

    /// @brief Read the latest coherent value. Any number of Services may call this.
    /// @param[out] generation  Advances on every accepted value; wraps (DISP-12).
    /// @return false if nothing has ever been accepted.
    bool read(Value& out, Generation& generation) const;

private:
    AcceptResult tryAccept(const ReceivedPdu& in) override;
};
```

`Capacity` is per Endpoint, never global, because the motivating case is one deep-1 segment Endpoint beside a deeper command Endpoint in a bootloader (`CORE §9.5`).

The storage is `RingBuffer<Message, Capacity>` from `Design/Firmware` rather than a new container. Two of its properties are the reason: its single-producer/single-consumer guarantee is the multiplicity `DISP-10` already fixes, and `Capacity` means `Capacity` usable slots. It does allocate `Capacity + 1` `Message` objects, so a deep-1 segment Endpoint holds two 512-byte slots rather than one — real, and worth measuring against the frozen budget (`CONFORM §3.2`) before deciding it matters.

`tryPop` copies out of the queue, which is a second copy after acceptance. For 8-byte commands this is irrelevant; for a 512-byte segment it is not, and a scoped peek handle is the obvious fix. It is deliberately **not** in phase 1: one consumer wanting it is not two, and `INTRO §9` says an abstraction waits for the second demand. The bootloader Service is the named trigger to revisit.

## 5.4 Transmit Endpoints

The asymmetry between the two forms is load-bearing, and the signatures are where it should be visible:

```cpp
/// Explicit-acceptance transmit path for events, commands, and requests.
class TransmitEndpoint {
public:
    /// @return kAccepted means ownership was taken, never that anything was sent (OWN-1).
    ///         A rejection must not advance Service protocol state (QOS-9).
    SendResult submit(util::Span<const uint8_t> payload);
};

/// Publication path for periodic state. The LLL samples on its own cadence.
class SnapshotTransmitEndpoint {
public:
    /// @brief Publish current state. Returns nothing: this is a publication, not a
    ///        submission, so there is no acceptance to report (CORE §10.4).
    void publish(util::Span<const uint8_t> payload);

    Generation publishedGeneration() const;
    Generation lastSampledGeneration() const;

    /// @return false when the bound Link cannot report TX completion - unavailable
    ///         rather than fabricated (ERR-5).
    bool lastSentGeneration(Generation& out) const;
};
```

`publish` returning `void` is the API expressing that no ownership or acceptance question exists. The generation echo is the only way a Service learns that an unbound Snapshot transmit Endpoint is silently doing nothing, which is why it exists at all (`CORE §10.4`) — and comparison against it must be modular, so the library should provide the comparison rather than leaving `>` available:

```cpp
/// Modular difference. Never compare generations with < or >.
int32_t generationDelta(Generation newer, Generation older);
```

Both transmit forms are constructed with a `PduSink&` and a static binding, per `CORE §10.3`: a Service names its outputs, the deployment names the Wires.

---

# 6. `endpoint` — the Framework-Facing API

## 6.1 `EndpointSink`

```cpp
enum class AcceptResult : uint8_t { kAccepted = 0U, kFull, kReplaced, kRejected };

/// Framework-side face of an Endpoint. One virtual call per accepted message.
class EndpointSink {
public:
    virtual ~EndpointSink() = default;

    /// @brief Offer a PDU for acceptance. Validates, stores, counts, returns.
    ///        Must not call application code, allocate, or block (DISP-2).
    /// @note  Executes in the caller's context, typically a Link RX path.
    virtual AcceptResult tryAccept(const ReceivedPdu& in) = 0;
};
```

`kReplaced` exists so a Snapshot can report coalescing as the declared semantics it is, distinct from a Queue's `kFull` loss. Both are ordinary outcomes; only `kRejected` indicates something invalid.

The class shape is worth one note because it reads oddly: `QueueEndpoint` publicly inherits `EndpointSink` but overrides `tryAccept` privately. Service code holding a `QueueEndpoint&` cannot inject messages into its own Endpoint, while the Dispatcher holding an `EndpointSink&` can. That is the two-faces property of §3 made structural instead of documented.

## 6.2 Dispatcher

```cpp
struct EndpointBinding {
    Namespace    name_space{Namespace::kNs0};
    EndpointId   endpoint_id{};
    EndpointSink* sink{nullptr};
};

/// Maps (Namespace, EndpointId) to a local Endpoint within one Endpoint Domain.
class EndpointDispatcher {
public:
    /// @param[in] table  Caller-owned, stable for the dispatcher's lifetime.
    explicit EndpointDispatcher(util::Span<const EndpointBinding> table);

    /// @brief Resolve and offer. Executes in the caller's context.
    /// @return kUnknownEndpoint when nothing is bound; the PDU is then counted
    ///         and dropped with no response emitted (ERR-1).
    DeliveryResult deliver(const ReceivedPdu& in) const;
};
```

The table is caller-owned and built at init, which keeps the dispatcher free of storage policy and lets a generated projection supply a `constexpr` table. A tiny target that collapses this to a `switch` (`CORE §25`) replaces the class rather than configuring it — the collapse is a different implementation of the same contract, not a mode.

Ordered lookup with binary search is the likely default; a small linear scan wins below roughly a dozen Endpoints. Neither is a contract.

---

# 7. `forwarding` — Router and Engine

## 7.1 Router

```cpp
struct RouteEntry {
    WireNumber wire{};
    LinkIndex  ingress{};          ///< matched ingress, or a wildcard
    uint32_t   egress_mask{0U};    ///< bit per Link Interface
    bool       deliver_locally{false};
    WireNumber splice_to{};        ///< applied before egress (SPLICE-1); 0 = none
};

/// Local forwarding lookup. Read-only in phase 1: the table is built at init and
/// never mutated, so the hot path needs no synchronization (CORE §11.2).
class Router {
public:
    explicit Router(util::Span<const RouteEntry> table);
    bool lookup(WireNumber wire, LinkIndex ingress, RouteDecision& out) const;
};
```

Phase 1 has no runtime reconfiguration, therefore no seqlock and no pointer swap. This is not a simplification to be apologized for: `IMPL §1` says advanced table synchronization arrives after measurement, and an immutable table is the correct starting point precisely because it makes the reader path deterministic. The interface is shaped so a mutable implementation can appear behind it without touching callers.

## 7.2 Forwarding engine

```cpp
/// Applies routing to one ingress PDU: local delivery, splice, and egress.
class ForwardingEngine final : public IngressSink {
public:
    ForwardingEngine(const Router& router,
                     EndpointDispatcher& dispatcher,
                     util::Span<PduSink* const> egress_links);

private:
    void onIngress(const ReceivedPdu& in) override;
};
```

This is where flood-and-filter, splice application, and local delivery live, and it is the only place that knows about both Endpoints and Links. Keeping it small is the point; if it starts accumulating policy, that policy probably belongs in configuration.

## 7.3 The two narrow interfaces

```cpp
/// Accepts a canonical PDU for egress. Implemented by LogicalLink.
class PduSink {
public:
    virtual ~PduSink() = default;
    virtual SendResult submit(const Pdu& pdu) = 0;
};

/// Accepts a canonical PDU that arrived. Implemented by ForwardingEngine.
class IngressSink {
public:
    virtual ~IngressSink() = default;
    virtual void onIngress(const ReceivedPdu& in) = 0;
};
```

Two one-method interfaces are what keep `link` and `forwarding` independent (§2). They are also the two places a test double goes: a recording `PduSink` tests Endpoints and forwarding without a Link, and a synthetic `IngressSink` tests an LLL without a routing table.

---

# 8. `link` — Drivers and Logical Links

## 8.1 Driver seam

Per-hardware, virtual, and the natural mock point:

```cpp
class LinkDriver {
public:
    virtual ~LinkDriver() = default;

    virtual bool start() = 0;
    virtual bool stop() = 0;

    /// @brief Hand one transfer unit to the hardware. Copy-based: on kAccepted the
    ///        caller may immediately reuse the buffer (CORE §16.1).
    virtual SendResult sendUnit(util::Span<const uint8_t> unit) = 0;

    /// @brief Retrieve one available transfer unit.
    /// @return bytes written, or 0 when nothing is available. Empty is not an error.
    virtual size_t receiveUnit(util::Span<uint8_t> out) = 0;

    virtual const LinkCapabilities& capabilities() const = 0;
};
```

A "transfer unit" is a CAN frame, a UART chunk, a datagram, or a FIFO entry — the framing-neutral thing `CORE §17` calls the transfer-unit kind. Returning 0 from `receiveUnit` is explicitly the normal idle case, not a fault, which is the polled-Link rule from `CORE §1.7`.

Interrupt- and DMA-driven drivers implement the same interface with the queueing behind it; that is a per-target concern and the reason the seam is virtual rather than a policy.

## 8.2 Logical Link

```cpp
class LogicalLink : public PduSink {
public:
    /// @brief Drive one increment of RX and TX work. Bounded; returns to the caller.
    ///
    /// RX: pulls units from the driver, reassembles, validates, timestamps, and
    /// calls IngressSink::onIngress. TX: drains queued PDUs and samples bound
    /// Snapshot transmit Endpoints on their configured cadence (CORE §1.7).
    virtual void poll() = 0;

    virtual const LinkCapabilities& capabilities() const = 0;
    virtual const LinkCounters& counters() const = 0;
};
```

`poll()` is the whole active surface, and it does both directions on purpose: the LLL is the single serialized mutable context for its state (`LINK-11`), so having one entry point makes that property structural rather than a rule to remember. An interrupt-driven implementation still funnels its mutation through this context.

The LLL is also where arrival timestamping and profile decode happen, which is what allows an offloaded or RTL implementation to satisfy the same contract (`CORE §1.7`).

## 8.3 Capabilities

A plain value struct per `CORE §17`, with unsupported features stated rather than omitted — an absent field reads as an unmade decision, an explicit "none" is information (`ERR-5` applied to configuration):

```cpp
struct LinkCapabilities {
    TransferUnitKind unit_kind{TransferUnitKind::kFrame};
    uint16_t         max_pdu_bytes{0U};
    QosProfile       qos_profile{QosProfile::kMinimal};
    FlowControl      flow_control{FlowControl::kNone};
    bool             supports_fragmentation{false};
    bool             reports_tx_completion{false};
    uint16_t         tx_queue_depth{0U};
    // ... grows with each profile; see CORE §17 for the intended field set
};
```

Its first real job is static validation: refusing to bind a Wire whose PDUs cannot be represented on the selected Link, before anything is emitted (`LINK §2.3`).

---

# 9. `platform` — the Port Seam

Two things only, and keeping the list that short is a design goal rather than an accident.

**Time.** A monotonic tick for arrival timestamps and cadence, which `hal/clock.h` already provides. `hal::PlatformClock` is a monotonic 64-bit nanosecond clock with a `std::chrono` time point and a `now()` the port supplies; `hal::mock_clocks.cpp` is the host-test implementation and is already wired into the existing test builds. The core takes this as its clock rather than defining a third one beside it and the simulator's `MonotonicClock`.

The clock is used once per PDU rather than once per byte, so how it is injected barely matters for cost — but the *conversion* does: `Timestamp` (§4.3) is narrower than the clock's time point, and the reduction happens once on the ingress path so that nothing downstream carries eight bytes per stored message.

**Serialization.** A policy type satisfying a small compile-time contract:

```cpp
/// No-op policy for an Endpoint with a single writer. Compiles to nothing.
struct NoSerialization {
    struct Guard { };
    static Guard acquire() { return Guard{}; }
};

// CriticalSectionSerialization and MutexSerialization are per-platform,
// supplied by the port. DISP-13 requires the property, not a mechanism.
```

Everything else — task creation, timers, storage, GPIO — is outside the library. A Service needing them takes them as injected dependencies, which is what keeps `SVC-9`'s portability claim bounded and honest.

---

# 10. `config` — Static Wiring

Configuration is data, produced by hand for the prototype and by the Organizer later (`DEPLOY §2`). The library consumes it; it does not parse files.

```cpp
struct NodeConfig {
    util::Span<const RouteEntry>      routes;
    util::Span<const EndpointBinding> endpoints;
    util::Span<const LinkConfig>      links;
    util::Span<const TransmitBinding> transmit_bindings;
};

/// @brief Check everything a Node can verify locally before it runs.
/// @note  Structural validity only (CFG-3, CORE §19.1). Cross-node checks belong
///        to tooling, which sees the whole deployment.
ValidationResult validate(const NodeConfig& config);
```

`validate` returning a result rather than asserting matters for the host and tooling cases, where reporting every problem beats stopping at the first. The check worth building first is unambiguous resolution (`CORE §19.1`) — one ingress plus one canonical identity resolving to two actions is the failure that otherwise depends on table order and is invisible until it is not.

Set-valued and coupled acceptance (`CORE §19.1`) is a phase-2 concern, but the representation should not actively prevent it: a binding that stores single values everywhere is harder to widen than one storing ranges from the start.

---

# 11. Execution: Who Drives What

The library is passive infrastructure. It creates no tasks, owns no loop, and starts no threads — which is `IMPL §3`'s execution shape stated as a constraint on the code rather than an observation about it.

```text
active, application-owned            passive, library-owned
-----------------------------------  ----------------------------------
Service task or superloop body       Endpoints, storage
Link task, ISR, or superloop poll    Router, Dispatcher, LLL state
```

A single-threaded host or bare-metal target therefore looks like:

```cpp
for (;;) {
    for (LogicalLink* link : links) { link->poll(); }   // fills Endpoints
    application.periodic();                             // drains Endpoints
}
```

That ordering is not incidental. `CONFORM §3` requires measuring both orderings because draining before servicing costs a full cycle of latency while servicing first costs roughly a copy — the one visible price of the bounded delivery boundary, and the library cannot hide it because the choice is the application's.

An RTOS target gives each Link its own task and each Service its own, with the same objects and no library change. That equivalence is the thing to protect: if a threading model ever needs to appear inside the library, something has been designed wrong.

---

# 12. Build and Test Layout

Following the project's component structure, with the core as a sibling of the existing simulator:

```text
code/
  CMakeLists.txt                     adds core, then sim
  core/
    CMakeLists.txt
    include/wirespaces/              public headers
    src/                             non-template bodies
    test/                            gtest, one file per module
  profile/can/                       later
  platform/host/                     host port of §9
  sim/                               existing; becomes a consumer of core
```

Targets:

```text
wirespaces_core          static, -fno-exceptions -fno-rtti
wirespaces_core_tests    gtest; may use anything, being host-only
wirespaces_vectors       later: emits and checks CONFORM golden vectors
```

## 12.1 Consuming `Design/Firmware`

The two trees build differently: `Design/Firmware` uses Make with a hand-maintained `mk/modules.mk` module list and fetches gtest itself, while `WireSpaces/code` uses CMake. Everything §2.2 recommends reusing is header-only except `crc/crc.cpp`, so bridging them is a smaller problem than it looks.

The cheapest correct arrangement is an include path, not a copy:

```cmake
target_include_directories(wirespaces_core PUBLIC ${FIRMWARE_ROOT})
```

`#include "util/span.h"` and `#include "data_structures/ring_buffer.h"` then resolve as they do everywhere else in `Design/Firmware`, and there is exactly one copy of each file. The cost is that building the core requires the `Firmware` tree present — acceptable, and arguably the honest state of affairs given that `Design/Firmware/wirespaces/` is where this library plausibly ends up living (§15).

Copying the headers into `WireSpaces/code/core/third_party/` is the alternative, and it buys standalone buildability at the price of silent divergence: a fix to `RingBuffer` upstream would not reach the copy, and nothing would report that. Worth doing only if the core needs to be extractable on its own, which nothing currently requires.

There is a third option that is better than either if `Firmware` is ever touched for other reasons — give `Design/Firmware` a CMake interface library exporting its header-only modules, and have both build systems consume the same declaration. That is not phase-1 work.

The one property to keep regardless of which is chosen: **the core builds without the simulator**, which is the continuous check that §2's dependency direction has not been violated. A `Firmware` dependency does not weaken that; a `sim` dependency would.

Two more test properties are worth wiring up early because each becomes expensive to add later:
- **no allocation after init**, asserted by failing the allocator once construction completes (`CONFORM §3.2`). A test can do this directly; a review cannot.
- **invariant IDs cited in tests.** A test named for `DISP-2` or `OWN-3` connects the suite to the register, which is what makes `CONFORM §4`'s reason assertions and the exit criteria checkable rather than aspirational.

The same convention is worth using in code comments where a line exists because of a specific rule, as in the sketches above. It costs a token and answers "why is this here" permanently.

---

# 13. What Phase 1 Deliberately Omits

Each of these is absent for a stated reason, not because it was forgotten. Recording the trigger is what keeps the omission from being re-litigated or, worse, quietly added:

| Omitted | Reason | Trigger to add |
|---|---|---|
| Typed Endpoint façades | no schema language yet (`REG §6.15`) | a generated codec exists |
| Peek handle on Queue | one consumer wants it, not two | the bootloader segment Service |
| Owned-handle submission | copy-based baseline (`OWN-3`) | a measured copy cost that matters |
| Seqlock / swappable tables | no runtime reconfiguration yet | an actual reconfiguring deployment |
| Credit flow control | undesigned (`CORE §15.4`) | the HDLC profile |
| Header extensions | format not settled (`CORE §2.3`) | a Transport or profile needing one |
| Reassembly | CAN profile work | the CAN LLL |
| Splice | needs two Links to be meaningful | the second Link exists |
| Telemetry Service | schema open (`REG §6.7`) | counters exist and are worth reporting |
| Restart and lifecycle | `CORE §23` is settled, the API is not (`REG §6.17`) | a driver that can actually fail |

The last row is the one to watch. `CORE §23` now specifies lifecycle behavior in detail, so the temptation is to build the whole thing immediately. But its API vocabulary is still open, and a lifecycle interface designed against a driver that cannot fail will be wrong in ways nobody can see. Counters and a runtime generation are cheap and worth having from the start; the escalation ladder and supervisor interface should wait for something that faults.

---

# 14. Provisional Choices This Sketch Makes

Per `CONFORM §1.1`, choices made to let code exist are listed rather than left to become decisions by default. Each stays open in `REG §6`:

| Choice here | Open item |
|---|---|
| bytes in Endpoints, decode on read | `REG §6.12` — `T` and the decoded representation |
| `Timestamp` narrower than the platform clock | `REG §6.7` — resolution, epoch, wrap comparison |
| `AutosarCrc8` and `Crc16CcittFalse` as candidates | `REG §6.8` — CRC parameters unchosen |
| `MetadataLevel` template parameter | `REG §6.12` — how metadata is declared and accessed |
| compile error for undeclared metadata | `REG §6.12` — stated preference, now exercised |
| `SendResult` plus a wide `Reason` registry | `REG §6.7` — one enumeration or two |
| `Generation` at 16 bits, wrapping | `REG §6.12`, `REG §6.17` — width and comparison window |
| immutable router table | `CORE §11.2` — synchronization strategy undecided |
| `poll()` as the LLL's whole active surface | `REG §6.17` — lifecycle operations |
| `LinkDriver` transfer-unit abstraction | `CORE §17` — capability field set still growing |

None of these is settled by appearing in working code, and a test pinning one is pinning current behavior for regression purposes rather than ratifying it.

---

# 15. Questions This Sketch Does Not Answer

Six things need a decision before or during the first increment, and none is mine to make:

1. **Where the core library lives.** The project's C++ rules point reusable platform-independent libraries at `Design/Firmware`, and this qualifies. Against that, the core is defined by this document set and validated by its conformance vectors, so keeping it in `WireSpaces/code/core/` keeps spec, code, and vectors in one place. The recommendation is to keep it here until a second real target consumes it, then promote — which is the same "wait for the second demand" rule the rest of the project uses. Note that §2.2's dependency on `Design/Firmware` weakens the case for separation somewhat: a core that already includes six `Firmware` headers is not meaningfully standalone.

2. **Whether `Namespace` and `EndpointId` should be one composite type.** They are always used together for dispatch, and a combined 18-bit `EndpointKey` would make the dispatch table and its comparison obvious. Against it: they have different allocation policies and appear separately in the descriptor.

3. **Whether the descriptor codec should be `constexpr`.** It would allow compile-time vector construction and generated tables in ROM, at the cost of constraining how the implementation is written. Cheap now, awkward to retrofit.

4. **How much the first increment should attempt.** The natural first slice is `core` plus one `QueueEndpoint`, one `SnapshotEndpoint`, a dispatcher, and a recording `PduSink` — no Router, no Link, entirely host-testable, and enough to exercise the delivery boundary and the metadata declaration mechanism that §5 puts the most weight on.

5. **Include path or vendored copy for the `Firmware` utilities** (§12.1). The recommendation is an include path, on the grounds that a copy diverges silently while nothing currently requires the core to be extractable.

6. **Whether `util::Uint24_s`'s TODO becomes a general `UintN`** before the identity types are written (§4.1). It is a change to `Firmware` rather than to WireSpaces, which makes the sequencing someone else's call, but it determines how §4.1 is written.

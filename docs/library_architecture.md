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
+------------------+   +---------------------+
|  endpoint        |   |  config             |
|  Queue, Snapshot |   |  static wiring      |
|  Dispatcher      |   +---------------------+
+------------------+            |
        ^                       |
        |  (forwarding -> endpoint, one direction only)
+------------------+   +---------------------+
|  forwarding      |   |  link               |
|  Router, Engine  |   |  drivers, LLLs      |
+------------------+   +---------------------+
        |                       |         |
        |                       |         v
        |                       |  +---------------------+
        |                       |  |  platform           |
        |                       |  |  clock, lock policy |
        |                       |  +---------------------+
        v                       v
+---------------------------------------------+
|  core   canonical: Pdu, descriptor codec,   |
|         identity types                      |
|         root seams: PduSink, PduSource,     |
|         IngressSink, ReceivedPdu, results   |
+---------------------------------------------+

  profile/can, profile/bytestream    depend on core + link only
  telemetry                          depends on core only
```

**`core` depends on nothing.** That is stronger than the previous draft, which had it depending on `platform`, and the correction matters: no part of the canonical descriptor codec needs a clock or a lock. `Timestamp` is a value type; the LLL is what obtains time and reduces it. With `platform` out of the way, the descriptor codec and the conformance-vector tools build with no target-port machinery whatsoever, which is the property `core` exists to provide.

`core` holds two kinds of thing, and conflating them is what produced the cycle described below:

- **canonical types** — what `BITS` and `LINK` define byte layouts for. These headers stay clean of anything Link-scoped or delivery-scoped.
- **dependency-root runtime types** — the one-method seam interfaces, `ReceivedPdu`, `LinkIndex`, `WireAlias`, and the result enumerations. These are not canonical protocol types and never appear on a wire, but every module above needs them, so the root is where they belong.

The earlier formulation — "a Link-scoped concept in `core` means a type is in the wrong module" — was too strong and this document violated it immediately, since `ReceivedPdu` carries a `LinkIndex`. The rule that actually holds is per *header*, not per target: `pdu.h` contains nothing Link-scoped, and `runtime_interfaces.h` is allowed to.

## 2.1 The seam interfaces must sit at the root

The previous draft put `PduSink` and `IngressSink` in `forwarding`. That is a genuine dependency cycle, hidden by the diagram rather than expressed in it: `LogicalLink` implements `PduSink`, so `link` would depend on `forwarding`; a transmit Endpoint holds a `PduSink&`, so `endpoint` would too; and `forwarding` depends on both. Three edges pointing the wrong way.

The fix is not another module. The interfaces are declarations with one method each, they belong to no layer in particular, and they go in the dependency root:

```text
core/runtime_interfaces.h    PduSink, PduSource, IngressSink
```

Everything above then depends downward on them, and the absent dependencies below are real rather than asserted:

- **`link` does not depend on `forwarding`.** An LLL hands a received PDU to an `IngressSink` (§4.6), which `ForwardingEngine` implements.
- **`endpoint` does not depend on `link`.** A transmit Endpoint holds a `PduSink&`, not a `LogicalLink&`, which is what lets it be tested against a recording sink and is the indirection `CORE §10.3` asks for at the Service level.
- **`link` does not depend on `endpoint`.** This one was missing entirely from the previous draft, which had the LLL sampling bound `SnapshotTransmitEndpoint`s directly (§8.2) — a fourth wrong-way edge that the diagram did not show. The LLL samples through a `PduSource&` instead. The alternative, moving cadence sampling above the LLL, was rejected because the queue-free periodic path exists precisely so the LLL can sample on its own schedule (`CORE §10.4`).
- **`forwarding` depends on `endpoint`**, and only in that direction. `ForwardingEngine` holds an `EndpointDispatcher&`; nothing in `endpoint` includes anything from `forwarding`.

## 2.2 Namespaces and headers

The existing simulator establishes the convention: `wirespaces::sim` under `include/wirespaces/sim/`. Core protocol types are the library's main subject, so they sit directly in `wirespaces`, with adjuncts one level down:

```text
wirespaces              canonical types, endpoints, forwarding, results
wirespaces::link        driver and LLL seams, capabilities
wirespaces::can         Classical CAN profile
wirespaces::platform    port seam
wirespaces::sim         existing host simulator
```

Two layers maximum, per the project style. Headers follow declarations-first with out-of-line template definitions below a banner in the same file.

## 2.3 Reuse from `Design/Firmware`

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

`RingBuffer` is a better fit than it first appears, and for a reason worth recording. It documents itself as safe for **one producer and one consumer**, which is exactly the multiplicity `DISP-10` fixes for a receive Queue: the framework writes, one Service reads. It also allocates `Capacity + 1` slots rather than sacrificing one, so its logical capacity is its usable capacity — which is what `CORE §9.5` asks for. Where a Queue Endpoint declares multiple writers, the producer-lock policy of §9 wraps the enqueue side and the ring's own guarantee covers the rest.

Its single-producer/single-consumer guarantee is also worth reading as a limit rather than only a feature: it says nothing about a Snapshot's read coherence, which is a different problem with a different solution (§9.2). A `RingBuffer` is not the storage for a Snapshot Endpoint.

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
- `util/integer.h` is worth knowing about but **not** worth acting on first. `util::Uint24_s` is the same shape the identity types want — a narrow value in a wider container whose serialization fails rather than truncating — and its TODO asks for the general form. An earlier draft made resolving that TODO a precondition for §4.1; §4.1 now explains why that was wrong on both the technical claim and the sequencing. The stopping condition lives there.

**Not reusable, and mentioned only so nobody looks twice:** `bootloader/`, `hdlc/protocol.h`, and `hdlc/network_management.h` are application protocols. `Firmware/wirespaces/host_demo/` is an earlier C prototype of an unrelated generation.

**Adjacent and worth revisiting later:** `hdlc/hdlc.h`'s `Receiver` is a byte-oriented framing state machine over a ring buffer that already makes progress under arbitrary chunk boundaries — the exact property `CONFORM §2` calls out as invisible to any test that hands a parser whole messages. When `LINK §3` becomes real, that is the starting point rather than a new parser. `data_structures/array_list.h` is a candidate for bounded tables such as learned bindings and reply contexts, neither of which exists in phase 1.

---

# 3. The Seams

The mechanism used at each boundary matters more than the boundary itself, so this is the section to disagree with early. Three mechanisms, chosen per seam by how often the boundary is crossed and whether the crossing needs to be substitutable at link time:

| Seam | Mechanism | Why |
|---|---|---|
| Hardware driver | pure virtual, **typed per carrier** | per-hardware, must be mockable; the type is the transfer unit (§8.1) |
| Logical Link (LLL) | pure virtual | per-profile, substitutable; where carrier-neutrality actually lives |
| PDU egress / ingress / sample | pure virtual, one method each | the dependency root (§4.6); each is also a test double |
| Endpoint accept | pure virtual (`EndpointSink`) | heterogeneous Endpoint types behind one dispatch table |
| Endpoint read/write | non-virtual template | the portability contract (`SVC-9`); crossed per message by Service code |
| Producer exclusion | template policy | must compile to nothing when a writer is exclusive |
| Snapshot coherence | **not a seam** — built-in seqlock | a correct read has no configurable alternative (§9.2) |
| Clock | template policy or virtual | see §9 |
| Forwarding table | plain value type | read-mostly, no substitution needed yet |

Three of these deserve their reasoning recorded, because each will look wrong at some point.

**Endpoint accept is virtual, Endpoint read is not.** The dispatcher holds Endpoints of unrelated types — a 512-byte snapshot, an 8-byte command queue — behind one table, and that is exactly what a vtable is for. One indirect call per accepted message is negligible beside the payload copy occurring in the same operation. The Service-facing side is the opposite case: it is the surface `SVC-9` says must be portable and it is called by application code in tight loops, so it is a concrete templated API with no indirection. The consequence is a slightly unusual class shape, and it is intentional — see §6.1.

**Producer exclusion is a template policy; reader coherence is not a seam at all.** `DISP-13` requires an Endpoint Domain to serialize concurrent writers, but the common case on a small target is a single writer needing none, and a policy parameter compiles that case to nothing — precisely the property `CONFORM §2` wants tested ("an exclusive Endpoint with the synchronization it does not need omitted"). A virtual lock could not deliver that.

Snapshot read coherence is a different problem and gets a fixed answer rather than a policy: a seqlock, always present (§9.2). It is deliberately not configurable, because every alternative setting would be one where a concurrent read is unsafe, and reading a Snapshot from a Service while the framework writes it is the ordinary case rather than an advanced one. An earlier draft made this a second policy parameter and left the no-op default; that would have shipped a default that tears.

**The driver seam is typed per carrier, not per byte span.** This is the one reversal from the previous draft that changes real code, and §8.1 argues it at length. Briefly: a WireSpaces CAN frame's identifier carries descriptor content, so a byte-span signature cannot express it without inventing a private serialization format to cross a C++ interface.

---

# 4. `core` — Canonical Types and Root Seams

## 4.1 Identity

Raw `uint16_t` for four different identity spaces is how a WireNumber ends up in an EndpointId parameter. Two things are wanted here: distinct types, and a range check that rejects rather than truncates. `BITS §2` allocates 10 bits to a WireNumber and 5 to a NodeId, so a value that does not fit is a real and reachable error, and `PDU-2` says the answer is rejection.

```cpp
struct WireNumber {
    static constexpr uint16_t kMax{1023U};      ///< BITS §2, 10 bits
    uint16_t value{0U};
    bool isValid() const { return value <= kMax; }
};

struct NodeId {
    static constexpr uint8_t kMax{31U};         ///< BITS §2, 5 bits
    uint8_t value{0U};
    bool isValid() const { return value <= kMax; }
};

struct EndpointId { uint16_t value{0U}; };   ///< full width, no narrowing
struct WireAlias  { uint8_t  value{0U}; };   ///< Link-scoped, never on the canonical wire
struct LinkIndex  { uint8_t  value{0U}; };   ///< local array index, never transmitted

enum class Direction     : uint8_t { kOriginToNode = 0U, kNodeToOrigin = 1U };
enum class Namespace     : uint8_t { kNs0 = 0U, kNs1, kNs2, kNs3 };
enum class TransportType : uint8_t { kUnreliableDatagram = 0U /* ... */ };
enum class Qos           : uint8_t { kCritical = 0U, kHigh, kNormal, kBackground };
```

An earlier draft of this section proposed building these on a generalized `util::UintN<Bits, Container>`, extracted from `util::Uint24_s`'s TODO, and argued that doing so would make range validity "a property of the type rather than a rule the codec must remember." **That argument was wrong**, and it is worth recording why, because the mistake is an appealing one.

A type carrying an integer and an `isValid()` does not prevent invalid objects from existing — it only gives them a name. The codec still has to *call* `isValid()` at each field boundary, so the number of places to forget the check is unchanged. Getting the property actually claimed requires controlled construction: a private constructor with a `static std::optional<WireNumber> tryMake(uint16_t)` factory, so that no invalid instance can be formed at all. That is a real and defensible design, but it is a substantially larger utility exercise than resolving a TODO, and it constrains aggregate initialization everywhere the type appears.

The sequencing was also backwards. Blocking WireSpaces prototyping on a generalization inside another repository inverts the dependency, and it contradicts the rule this same document cites approvingly elsewhere: `INTRO §9` says an abstraction waits for the second demand. One descriptor codec is the first demand.

So the plain wrappers above are the plan, not a fallback, with an explicit stopping condition: **do not generalize the bounded-integer utility until the descriptor codec works and the duplication is visible in it.** If the codec then shows the same range-check paragraph eight times, that is evidence, and the factory-based form is what to extract — not the `isValid()` form, which would not have helped.

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

enum class EncodeResult : uint8_t { kOk = 0U, kOutputTooSmall, kFieldOutOfRange };

enum class DecodeResult : uint8_t {
    kOk = 0U,
    kInputTooShort,
    kReservedValue,       ///< a reserved encoding was used
    kWireOutOfRange,
    kNodeIdOutOfRange,
    kUnknownTransport,
};

/// Encode to exactly kEncodedSize bytes.
EncodeResult encode(const PduDescriptor& in, util::Span<uint8_t> out);

/// Decode from at least kEncodedSize bytes. On anything but kOk, out is
/// unmodified: PDU-2 rejects rather than masks.
DecodeResult decode(util::Span<const uint8_t> in, PduDescriptor& out);
```

An earlier draft returned plain `bool` here. That sat badly beside `CONFORM §4`, which requires tests to assert *why* something was rejected rather than only that it was — a `bool` makes that assertion impossible to write, so the conformance requirement and the API were in conflict from the start.

The fix is a small enumeration per operation, not a generic `Result<T, E>`. There is no exception mechanism to model, the out-parameter style stays uniform, and a per-operation enumeration lists exactly the outcomes that operation has, which keeps a call-site `switch` both exhaustive and short. `Reason` (§4.5) is what these map into for counting; the narrow types are what callers and tests use.

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

/// Registry of every counted rejection and drop. Names are stable; see below on numbering.
enum class Reason : uint16_t { /* ... */ };

Reason toReason(SendResult result);
Reason toReason(DecodeResult result);
```

`SendResult` and the per-operation result types stay small enough for exhaustive `switch`es at call sites. `Reason` is the wide registry that telemetry and counters index, so there is one table rather than two drifting ones — the provisional answer to the question in `REG §6.7`. The mapping functions are where they are kept honest, and `CONFORM §4` requires tests to assert the reason rather than merely that something was rejected.

An earlier draft promised `Reason` had "stable numbering; append only." That promise is withdrawn as premature. **Semantic names are stable; numeric values are not, until they are externally visible** — which happens when a reason is serialized into a telemetry schema or crosses a wire. Before then, numeric stability buys nothing and costs the freedom to reorganize a registry that will certainly be reorganized: reasons will merge, split, and regroup during prototyping. The right time to freeze numbers is when `REG §6.7`'s telemetry schema settles, and at that point the freeze should be explicit rather than inherited from a comment written earlier.

## 4.6 Root seam interfaces

Three one-method interfaces, in the dependency root for the reasons in §2.1. Each is a declaration only; each is also a place a test double goes, which is the second reason to keep them this narrow:

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

/// Pull-side counterpart of PduSink: a source the LLL samples on its own cadence.
/// Implemented by SnapshotTransmitEndpoint (CORE §10.4).
class PduSource {
public:
    virtual ~PduSource() = default;

    /// @brief Sample the current value, if the source has one.
    /// @return false when nothing has been published, which is not an error.
    virtual bool trySample(Pdu& out) = 0;
};
```

A recording `PduSink` tests Endpoints and forwarding with no Link present; a synthetic `IngressSink` tests an LLL with no routing table; a canned `PduSource` tests an LLL's cadence logic with no Endpoint. That the three test doubles are trivial to write is the practical evidence that the seams are in the right places.

`PduSource` is the pull direction, and it is the piece the previous draft was missing. Without it the LLL has to know what a `SnapshotTransmitEndpoint` is, which inverts the dependency and makes the LLL untestable without the Endpoint layer.

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

template <> struct DeliveredMetadata<MetadataLevel::kPayloadOnly> {};  // empty; see below

template <> struct DeliveredMetadata<MetadataLevel::kWithSource> {
    WireNumber wire{};
    Direction  direction{Direction::kOriginToNode};
    NodeId     node_id{};
    LinkIndex  ingress{};
};
// kFull adds Qos, TransportType, extension presence, and arrival.
```

A Service written against `kPayloadOnly` that tries to read `.wire` fails to build, which is the right outcome and is not achievable with a runtime "unavailable" flag. Snapshot Endpoints force at least arrival time regardless of declared level, per `CORE §9.3`.

One implementation detail that an earlier draft got wrong by calling the empty specialization "costs nothing": an empty class used as a **data member** still occupies at least one byte, and with alignment padding a `Message` can lose more than that. `[[no_unique_address]]` would fix it but is C++20, and this codebase is C++17. If the byte matters — on a Queue of 64 slots it is 64 bytes plus padding — the fix is to specialize the slot type so that `kPayloadOnly` has no metadata member at all, or to inherit from `DeliveredMetadata<Level>` and let empty base optimization apply. Whether it matters is a measurement against the frozen budget (`CONFORM §3.2`), not an assumption.

## 5.3 Receive Endpoints

```cpp
template <size_t Capacity,
          size_t MaxPayload,
          MetadataLevel Level = MetadataLevel::kPayloadOnly,
          typename ProducerLock = NoProducerLock>
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

    RingBuffer<Message, Capacity> queue_{};   ///< see §2.3
};

template <size_t MaxPayload,
          MetadataLevel Level = MetadataLevel::kFull,
          typename ProducerLock = NoProducerLock>
class SnapshotEndpoint final : public EndpointSink {
public:
    struct Value { /* metadata, payload, length, as above */ };

    /// @brief Read the latest coherent value. Any number of Services may call this,
    ///        concurrently with the writer: coherence is a seqlock (§9.2). May
    ///        retry internally; never returns a torn value.
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
enum class AcceptResult : uint8_t { kAccepted = 0U, kFull, kRejected };

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

An earlier draft had a fourth value, `kReplaced`, so that a Snapshot could report coalescing as the declared semantics it is. It is removed, and the reasoning against it is more than cosmetic: replacing the previous value is simply what a Snapshot *does*, so after the very first acceptance essentially every successful publication would return `kReplaced`. A steady-state, perfectly healthy Snapshot would report a non-`kAccepted` result on every single message, which is exactly the shape a lossy path has. Any counter or log that aggregates non-`kAccepted` outcomes — and something will — would then read a working system as a failing one. `ERR-5`'s concern about telemetry that misleads applies here in a form that originates in the API rather than the schema.

So a Snapshot returns `kAccepted`, and consumers learn what they missed from the generation counter `DISP-12` already requires: a reader whose generation advanced by more than one skipped observations, which is more informative than a per-accept flag because it says *how many*.

There is a genuinely interesting event nearby, and it is worth separating rather than conflating: overwriting a value **no consumer ever read** means a consumer is not keeping up. That is a counter on the Endpoint, not a return value, because the writer cannot act on it and the operator can.

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

    /// Splice is an explicit action, not a sentinel. WireNumber 0 is a legitimate
    /// Wire - CORE reserves a high range and one top value, not zero - so
    /// "splice_to == 0 means none" would silently disable a valid splice.
    bool       splices{false};
    WireNumber splice_to{};        ///< meaningful only when splices; applied before egress (SPLICE-1)
};

/// Local forwarding lookup. Read-only in phase 1: the table is built at init and
/// never mutated, so the hot path needs no synchronization (CORE §11.2).
class Router {
public:
    explicit Router(util::Span<const RouteEntry> table);
    bool lookup(WireNumber wire, LinkIndex ingress, RouteDecision& out) const;
};
```

Phase 1 has no runtime reconfiguration, therefore no table versioning and no pointer swap. The seqlock a Snapshot Endpoint uses (§9.2) is a different mechanism for a different problem and does not appear here; an immutable table needs no reader protocol at all. This is not a simplification to be apologized for: `IMPL §1` says advanced table synchronization arrives after measurement, and an immutable table is the correct starting point precisely because it makes the reader path deterministic. The interface is shaped so a mutable implementation can appear behind it without touching callers.

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

---

# 8. `link` — Drivers and Logical Links

## 8.1 Driver seams, one per carrier shape

The previous draft had a single `LinkDriver` with `sendUnit(util::Span<const uint8_t>)` and `receiveUnit(util::Span<uint8_t>)`, on the theory that a "transfer unit" is carrier-neutral. That was wrong, and it is the most expensive error in the draft because it would have been discovered only after a CAN driver existed.

A CAN transfer unit is an identifier, a length, and up to eight bytes. In WireSpaces the identifier is not incidental framing — `LINK §2` packs QoS, WireAlias, NodeId, and Direction into those 11 bits, so it carries descriptor content the LLL produced and the peer's LLL must recover. A byte-span signature cannot express it. The only way to force it through would be to serialize identifier, length, and payload into a private byte format purely to cross a C++ interface, and then deserialize on the other side — inventing a wire format to talk to ourselves, at a cost in both code and confusion that buys nothing.

The `TransferUnitKind` field already in `LinkCapabilities` was the tell: a seam that needs a runtime enumeration describing what shape its own arguments really are is a seam with the wrong signature.

So the driver contract is typed per carrier shape, and there are only ever a few:

```cpp
struct CanFrame {
    uint16_t id{0U};        ///< 11-bit; LINK §2 defines what the bits mean
    uint8_t  length{0U};    ///< 0..8
    uint8_t  data[8]{};
};

/// Classical CAN controller. Frame-oriented, with arbitration below this seam.
class CanDriver {
public:
    virtual ~CanDriver() = default;

    /// @brief Hand one frame to the controller. Copy-based: on kAccepted the caller
    ///        may immediately reuse its buffer (CORE §16.1).
    virtual SendResult send(const CanFrame& frame) = 0;

    /// @brief Take one received frame, if any.
    /// @return false when nothing is available, which is the normal idle case and
    ///         not a fault (CORE §1.7).
    virtual bool tryReceive(CanFrame& out) = 0;

    virtual const DriverCapabilities& capabilities() const = 0;
};

/// UART and similar. No unit boundaries exist at this seam, by nature.
class ByteStreamDriver {
public:
    virtual ~ByteStreamDriver() = default;

    virtual SendResult write(util::Span<const uint8_t> bytes) = 0;

    /// @return bytes written into out; 0 is the normal idle case.
    virtual size_t read(util::Span<uint8_t> out) = 0;

    virtual const DriverCapabilities& capabilities() const = 0;
};
```

A datagram driver gets added when a datagram Link is actually implemented, not in anticipation of one. This is the case where the lowest common denominator costs more than several honest interfaces, and the count stays small because carrier *shapes* are few even though carriers are many.

**Carrier-neutrality lives at the LLL, not the driver.** `LogicalLink` (§8.2) is what every layer above sees, and it speaks canonical PDUs regardless of what is beneath it. Nothing above `link` names `CanFrame`. That is where the neutrality claim belongs and where `CONFORM` should test it; pushing it one level lower gained nothing and cost expressiveness.

Interrupt- and DMA-driven drivers implement the same interfaces with the queueing behind them, which is a per-target concern and the reason these seams are virtual.

`start()` and `stop()` are **driver control, not WireSpaces lifecycle**. They enable and disable a peripheral; they say nothing about restart units, runtime generations, or bounded quiesce, all of which `CORE §23` specifies and §13 defers. The distinction is worth keeping in the naming so that a later lifecycle API is not mistaken for already existing:

```cpp
    virtual bool enable()  = 0;   ///< peripheral on. Not CORE §23 lifecycle.
    virtual bool disable() = 0;   ///< peripheral off, best effort, no quiesce contract.
```

## 8.2 Logical Link

```cpp
class LogicalLink : public PduSink {
public:
    /// @brief Drive one increment of RX and TX work. Bounded; returns to the caller.
    ///
    /// RX: pulls units from its driver, reassembles, validates, timestamps, and
    /// calls IngressSink::onIngress. TX: drains queued PDUs and samples its bound
    /// PduSource list on their configured cadence (CORE §1.7).
    virtual void poll() = 0;

    virtual const LinkCapabilities& capabilities() const = 0;
    virtual const LinkCounters& counters() const = 0;
};
```

`poll()` is the whole active surface, and it does both directions on purpose: the LLL is the single serialized mutable context for its state (`LINK-11`), so having one entry point makes that property structural rather than a rule to remember. An interrupt-driven implementation still funnels its mutation through this context.

The cadence sampling goes through `PduSource&` (§4.6), not through a `SnapshotTransmitEndpoint&`. The previous draft named the Endpoint type here, which would have made `link` depend on `endpoint` and made every LLL test drag in the Endpoint layer. The concrete driver is held by the concrete LLL — a CAN LLL holds a `CanDriver&` — so the driver type appears in the profile implementation and nowhere above it.

The LLL is also where arrival timestamping and profile decode happen, which is what allows an offloaded or RTL implementation to satisfy the same contract (`CORE §1.7`).

## 8.3 Capabilities, at two levels

Splitting the driver seam splits the capability descriptor with it, and the split is not bookkeeping — the two sets answer questions for different consumers.

**Driver capabilities** describe the hardware and its queueing. They are what a port author fills in and what diagnostics report:

```cpp
struct DriverCapabilities {
    uint16_t tx_queue_depth{0U};
    bool     reports_tx_completion{false};
    bool     uses_dma{false};
    bool     receives_in_isr{false};
    // per-carrier additions are allowed: a CAN driver may report filter count,
    // a byte-stream driver its RX buffer size. Nothing above `link` reads these.
};
```

**Link capabilities** describe the resulting Logical Link in canonical terms. They are what configuration validates against and what `CORE §17` is actually about:

```cpp
struct LinkCapabilities {
    uint16_t    max_pdu_bytes{0U};
    QosProfile  qos_profile{QosProfile::kMinimal};
    FlowControl flow_control{FlowControl::kNone};
    bool        supports_fragmentation{false};
    // ... grows with each profile; see CORE §17 for the intended field set
};
```

`TransferUnitKind` is gone from both. It existed to tell a caller what shape the old byte-span seam's arguments really were, and typed drivers make it unnecessary — the type is the answer.

The dividing line is whether the field survives a change of controller. A CAN Link's `max_pdu_bytes` and fragmentation behavior come from the profile and hold across any conforming CAN controller; `tx_queue_depth` and `uses_dma` do not. Mixing them is how configuration validation ends up accidentally depending on a peripheral.

Unsupported features are stated rather than omitted in both, since an absent field reads as an unmade decision while an explicit "none" is information (`ERR-5` applied to configuration).

`LinkCapabilities`' first real job is static validation: refusing to bind a Wire whose PDUs cannot be represented on the selected Link, before anything is emitted (`LINK §2.3`).

---

# 9. `platform` — the Port Seam

Two things only, and keeping the list that short is a design goal rather than an accident.

**Time.** A monotonic tick for arrival timestamps and cadence, which `hal/clock.h` already provides. `hal::PlatformClock` is a monotonic 64-bit nanosecond clock with a `std::chrono` time point and a `now()` the port supplies; `hal::mock_clocks.cpp` is the host-test implementation and is already wired into the existing test builds. The core takes this as its clock rather than defining a third one beside it and the simulator's `MonotonicClock`.

The clock is used once per PDU rather than once per byte, so how it is injected barely matters for cost — but the *conversion* does: `Timestamp` (§4.3) is narrower than the clock's time point, and the reduction happens once on the ingress path so that nothing downstream carries eight bytes per stored message.

**Mutual exclusion.** A producer-side policy type satisfying a small compile-time contract:

```cpp
/// Producer-side exclusion for an Endpoint with several writers.
/// The single-writer case compiles to nothing.
struct NoProducerLock {
    struct Guard { };
    static Guard acquire() { return Guard{}; }
};

// Per-platform implementations - critical section, or a mutex on paths where
// blocking is admissible - are supplied by the port. DISP-13 requires the
// property, not a mechanism.
```

Reader coherence is **not** a policy. A Snapshot Endpoint uses a seqlock internally, so a concurrent read is always safe and there is no configuration in which it is not (§9.2). Making it a parameter would only create a way to select the broken option.

The one platform primitive this needs is ordering — acquire/release atomics or explicit barriers — which is why it belongs in the port and not in `core`.

Everything else — task creation, timers, storage, GPIO — is outside the library. A Service needing them takes them as injected dependencies, which is what keeps `SVC-9`'s portability claim bounded and honest.

## 9.1 The phase-1 concurrency contract is deliberately narrow

The previous draft claimed an RTOS target could give every Link and every Service its own task "with the same objects and no library change." The objects shown did not support that. Four races were reachable, and they have different answers:

| Race | Answer |
|---|---|
| Snapshot reader against its writer | **solved: seqlock** (§9.2) |
| Several writers into one Endpoint | producer-lock policy, declared per Endpoint |
| Service and forwarding path both submitting to one LLL | not designed; externally serialized |
| `poll()` draining TX while another context calls `submit()` | not designed; externally serialized |

The first row is settled and is not a phase-1 omission. It is worth separating from the rest because it is the one that could not be fixed by telling the application to serialize: a Service reading a Snapshot *is* the ordinary case, so an API that made concurrent reads unsafe would push the problem onto every consumer.

The remaining three are genuinely undesigned, so the phase-1 contract is narrow but explicit:

```text
Concurrent reads of a Snapshot Endpoint are always safe.

Everything else - multiple writers into one Endpoint, and concurrent access to
one Logical Link - is externally serialized by the application, unless a
concrete type documents stronger support.
```

That is satisfied trivially by the single-threaded loop of §11, and it leaves room for concrete types to declare more once one has been built and measured. Generalizing from a working concurrent implementation is evidence; generalizing from an API sketch is not (`CONFORM §1.1`).

One contradiction the old text carried, now resolved: §6.1 requires `tryAccept` not to block, while the policy list offered a mutex. Both cannot hold. The rule is that **`tryAccept` must not block**, so a policy usable there is limited to a critical section or a lock-free sequence; a mutex-based policy is admissible only where blocking is permitted, which does not include acceptance. A port supplying a blocking policy to an Endpoint is a configuration error worth catching at compile time if it can be.

## 9.2 Snapshot coherence is a seqlock

A Snapshot Endpoint publishes with a sequence counter that is odd while a write is in progress. A reader samples the counter, copies metadata and payload, then re-reads the counter; if it changed or was odd, the copy is discarded and the read retries. `read()` therefore returns either a fully coherent value or retries — never a torn mix of metadata from one message and payload from the next.

This is the right mechanism for this shape of data and not merely an available one. The write is short and bounded, writers must never block on readers — the writer here is often a Link RX path where `DISP-2` forbids blocking outright — and readers are unlimited in number, which is exactly `DISP-12`'s multi-reader Snapshot. A mutex would invert the priority relationship and put a blocking call in acceptance; a double-buffer would need an allocation policy and a reclamation rule.

Three consequences worth stating, because each is a place a correct-looking implementation goes wrong:

**A read can retry, so it is not wait-free.** It is bounded in practice only if the writer makes progress. The safe arrangement is the natural one: the writer is the framework RX path at equal or higher priority than the reading Service, so a reader that observes a write in progress retries and immediately succeeds. The inverted arrangement — a high-priority reader spinning on a single core while a lower-priority writer sits preempted mid-write — livelocks, and it is the one case a port must not create. Worth an explicit note wherever priorities are assigned, since nothing in the type system prevents it.

**Writers still need exclusion from each other.** A seqlock coordinates one writer with many readers; it does nothing for two concurrent writers, which would interleave their sequence increments and produce a value that looks coherent and is not. A receive Snapshot Endpoint written by several Link drivers therefore needs the producer-lock policy *as well as* the seqlock. The two mechanisms solve different problems and neither substitutes for the other — which was the substance of the earlier confusion, even though the conclusion has changed.

**The sequence counter and the `Generation` of `DISP-12` are not the same number.** The sequence increments twice per publication and is odd mid-write; the generation is what a consumer compares to learn how many values it skipped. Deriving one from the other is possible and tempting, but the two have different visible contracts — `Generation` is part of the Service-facing API and wraps at a declared width (§5.4), while the sequence is an implementation detail with no exposed semantics. Keeping them separate costs a counter and avoids exporting an implementation detail as a contract.

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

An RTOS target is expected to give each Link its own task and each Service its own, and the object model is shaped so this needs no change in the library's structure. Part of it works today: a Service task may read a Snapshot Endpoint while a Link task writes it, because §9.2 makes that safe by construction. The rest — several writers into one Endpoint, and concurrent access to one LLL — is still the application's to serialize, per §9.1.

What is worth protecting is narrower than the earlier claim but more defensible: **no threading model appears inside the library**. Exclusion enters as a policy the port supplies, and if the library ever needs to create a task or own a lock to be correct, something upstream has been designed wrong.

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

The two trees build differently: `Design/Firmware` uses Make with a hand-maintained `mk/modules.mk` module list and fetches gtest itself, while `WireSpaces/code` uses CMake. Everything §2.3 recommends reusing is header-only except `crc/crc.cpp` — roughly four hundred lines in total.

The decisive fact is that **these are separate repositories**: `WireSpaces` is `ofdouglas/WireSpaces`, and `Firmware` lives inside `ofdouglas/tinkering`. An earlier draft of this section recommended simply adding `${FIRMWARE_ROOT}` to the include path. That is worse than it appeared. It makes a clean checkout of `WireSpaces` unbuildable on its own, and makes it buildable only when an unrelated repository happens to be checked out at the right relative filesystem location, at whatever revision it happens to be on. Nothing pins the version, nothing records what was used, and a build that succeeds on one machine fails on the next for reasons the repository does not describe. Four hundred lines of utility code is not enough value to make clean builds depend on ambient filesystem layout.

The dependency should be explicit and reproducible. Three arrangements are:

- **Vendored extraction with recorded provenance.** Copy the needed files into `code/core/external/`, with the upstream repository, path, and **commit hash** recorded alongside. This is the recommendation for phase 1. The objection to copying was silent divergence, and the answer is that recording the source commit makes divergence *visible* — an update becomes a deliberate, reviewable act rather than something that either happens invisibly or never happens at all.
- **Pinned fetch.** CMake `FetchContent` against `ofdouglas/tinkering` at a fixed tag or commit. Reproducible and avoids duplication, at the cost of pulling an entire unrelated repository to obtain six headers, and of requiring network access for a first build.
- **A real shared package.** Give the utility layer its own repository or CMake package that both consumers depend on by version. Correct, and clearly disproportionate today.

There is a fourth possibility that dissolves the problem instead of solving it, and it is now the more interesting question: if the core library lives in `tinkering` beside `Firmware` rather than in the `WireSpaces` repository, there is no cross-repository dependency at all. §15 lists that as open, and this section is the strongest argument yet for that side of it. A library that vendors six headers from another repository is not standalone in any meaningful sense, so the separation it was preserving is largely notional.

The one property to keep regardless: **the core builds without the simulator**, which is the continuous check that §2's dependency direction has not been violated.

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
| Swappable forwarding tables | no runtime reconfiguration yet | an actual reconfiguring deployment |
| Credit flow control | undesigned (`CORE §15.4`) | the HDLC profile |
| Header extensions | format not settled (`CORE §2.3`) | a Transport or profile needing one |
| Reassembly | CAN profile work | the CAN LLL |
| Splice | needs two Links to be meaningful | the second Link exists |
| Telemetry Service | schema open (`REG §6.7`) | counters exist and are worth reporting |
| Restart and lifecycle | `CORE §23` is settled, the API is not (`REG §6.17`) | a driver that can actually fail |
| Concurrent multi-writer Endpoints and LLL access | not yet designed (§9.1) | one RTOS target with measured races |
| Datagram driver seam | no datagram Link implemented | a UDP or shared-memory Link |

Driver `enable()`/`disable()` are not an exception to the lifecycle row. They turn a peripheral on and off and carry no quiesce, generation, or escalation contract; §8.1 keeps the naming distinct for exactly that reason.

The lifecycle row is the one to watch. `CORE §23` now specifies lifecycle behavior in detail, so the temptation is to build the whole thing immediately. But its API vocabulary is still open, and a lifecycle interface designed against a driver that cannot fail will be wrong in ways nobody can see. Counters and a runtime generation are cheap and worth having from the start; the escalation ladder and supervisor interface should wait for something that faults.

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
| per-carrier driver seams, capabilities split in two | `CORE §17` — capability field set still growing |
| seqlock sequence kept separate from `Generation` | `REG §6.12` — retry rules and ordering detail |

None of these is settled by appearing in working code, and a test pinning one is pinning current behavior for regression purposes rather than ratifying it.

Two things that were provisional in an earlier draft are **not** on this list, because they are now decisions rather than placeholders: Snapshot read coherence is a seqlock (§9.2), and the identity types are plain range-checked wrappers with a stated stopping condition rather than a pending utility generalization (§4.1).

---

# 15. Questions This Sketch Does Not Answer

Six things need a decision before or during the first increment, and none is mine to make:

1. **Where the core library lives.** The project's C++ rules point reusable platform-independent libraries at `Design/Firmware`, and this qualifies. Against that, the core is defined by this document set and validated by its conformance vectors, so keeping it in `WireSpaces/code/core/` keeps spec, code, and vectors in one place. The recommendation is to keep it here until a second real target consumes it, then promote — which is the same "wait for the second demand" rule the rest of the project uses. Note that §2.3's dependency on `Design/Firmware` weakens the case for separation somewhat: a core that already includes six `Firmware` headers is not meaningfully standalone.

2. **Whether `Namespace` and `EndpointId` should be one composite type.** They are always used together for dispatch, and a combined 18-bit `EndpointKey` would make the dispatch table and its comparison obvious. Against it: they have different allocation policies and appear separately in the descriptor.

3. **Whether the descriptor codec should be `constexpr`.** It would allow compile-time vector construction and generated tables in ROM, at the cost of constraining how the implementation is written. Cheap now, awkward to retrofit.

4. **How much the first increment should attempt.** Two increments, ordered so the second one attacks the architecture rather than confirming it.

   **First:** `core` — descriptor codec with `DecodeResult` — plus one `QueueEndpoint`, one `SnapshotEndpoint`, an `EndpointDispatcher`, and **one `TransmitEndpoint` feeding a recording `PduSink`**. No Router, no Link, entirely host-testable. Adding the transmit side costs almost nothing and exercises both faces of the Endpoint model, which is where §5 puts the most weight; a receive-only slice would leave `SendResult`, ownership-on-rejection, and the `PduSink` seam entirely untested.

   **Second: a fake Classical CAN driver and the simplest CAN LLL path**, before any byte-stream work. This is deliberately the adversarial choice. CAN immediately forces the driver seam to represent an arbitration identifier and DLC correctly, which is the exact thing the old single-`Span` seam could not do and the thing §8.1 was rewritten for — so it tests whether the new seams are genuinely carrier-neutral instead of only claimed to be. Even without aggregation or fragmentation, a single-frame CAN path exercises identifier packing (`LINK §2`), the descriptor round trip, and the `IngressSink` boundary. A byte-stream Link would exercise none of those and would let a wrong abstraction survive longer.

5. **How the `Firmware` utilities are depended upon** (§12.1). These are separate repositories, so the recommendation is a vendored extraction with the upstream commit recorded, rather than an include path into an unpinned sibling checkout. Question 1 above may dissolve this one entirely.

6. **Whether the seqlock's priority constraint should be enforced rather than documented** (§9.2). A reader spinning at higher priority than a preempted mid-write writer livelocks on a single core. The natural arrangement is safe, and nothing in the type system prevents the inverted one. Options are a note, a runtime retry-count assertion in debug builds, or a declared invariant in `REG`.

Question 6 in an earlier draft asked whether `util::Uint24_s`'s TODO should become a general `UintN` first. It is answered in §4.1: no, and the reasoning there also supplies the stopping condition for revisiting it.

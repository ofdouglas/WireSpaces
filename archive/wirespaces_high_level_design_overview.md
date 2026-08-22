# WireSpaces — High-Level Design Overview

**Status:** Provisional architecture summary  
**Audience:** Embedded software, networking, FPGA/RTL, and systems engineers  
**Purpose:** A compact engineering overview of the WireSpaces mental model, capabilities, and implementation shape. This is not the byte-exact protocol specification.

---

## 1. What WireSpaces Is

**WireSpaces (WS)** is a lightweight communication fabric for embedded systems. It is intended to scale from one PC-connected device to machines containing many MCUs, FPGAs, gateways, and field buses while keeping the same Endpoint and Service model.

The central abstraction is deliberately bus-oriented:

> A **Wire** is a logical bus with exactly one **Origin** and zero or more **Nodes**.

A Wire may coincide with one Physical Link or span several Physical Links through gateways. A device may be a Node on one Wire and the Origin of another.

```text
PC --USB--> one device

PC --USB--> Main SoC --CAN--> ECUs

FPGA --CAN/RS-485/shared memory--> MCU/RTL Endpoints

PC --Ethernet/FTDI FIFO--> gateway --> internal field buses
```

WireSpaces is intentionally useful before a complete static system model exists. A one-Link prototype can communicate with almost no Wire configuration; a larger deployment can use host-generated or fully static Wiring.

The design bias is:

> **Complexity belongs in constructing configuration and tables, not in executing the data plane.**

---

## 2. Canonical Message Identity

The current canonical base descriptor is **40 bits / 5 bytes**:

```text
Control: 8 bits
    Namespace             2
    TransportType         3
    QoS                   2
    HasHeaderExtensions   1

RoutingWord: 16 bits
    WireNumber           10
    Direction             1
    NodeId                5

EndpointId: 16 bits
--------------------------------
Base descriptor:         40 bits
```

A Physical Link profile does not have to transmit those fields literally. It may encode them in native metadata, aliases, or compact Link-specific forms and reconstruct the canonical descriptor at the LLL boundary.

`EndpointId == 0` is invalid. Four Namespaces and 16-bit EndpointIds provide up to **262,140 usable Namespace/EID combinations** canonically.

A Wire supports two directions:

```text
OriginToNode
NodeToOrigin
```

For `OriginToNode`, `NodeId == 0` means broadcast and `NodeId 1..31` selects one Node. For `NodeToOrigin`, `NodeId 1..31` identifies the transmitting Node; zero is invalid.

The Origin role is per-Wire, not per-device. WireSpaces does not define Origin election or failover in the base protocol. Redundant control can be built using another Wire with another Origin; application or Transport behavior decides when to switch authority.

---

## 3. Physical Links, LLLs, Routers, and Endpoint Domains

```text
Physical Link
    CAN / UART / RS-485 / USB / Ethernet / shared memory / FPGA FIFO / ...
        |
Link Interface
        |
Logical Link Layer (LLL)
    framing / validation / fragmentation / reassembly
        |
complete canonical WS PDU
        |
Router
   /            \
Link Interface   Endpoint Domain Dispatcher
                     |
                  Endpoint
```

A **Physical Link** is the medium or connection. A **Link Interface** is one participant's local attachment to it.

The **LLL** is Link-specific. It moves complete WS PDUs over the Physical Link and handles framing, validation, fragmentation/reassembly, and Link-local representations such as `WireAlias`. In general the LLL does **not** own Wire routing policy.

The **Router** answers: *where should this complete PDU go?* Its routing state maps canonical Wire identity and local ingress context to zero or more egress Link Interfaces and local delivery.

An **Endpoint Domain** is a local Endpoint ownership/dispatch boundary. Every Domain has a Dispatcher that resolves `(Namespace, EndpointId)` to the receiving Endpoint and records/reports invalid or unknown delivery attempts.

The Router need not be a central task. A preferred software architecture is a **shared, read-mostly routing table invoked in the caller's context** by Link RX paths and Endpoint Domains. Normal deployments keep the table static, allowing concurrent deterministic lookups. Runtime configuration may use seqlocks, immutable-table pointer swaps, or another bounded synchronization technique; frequent writes are allowed to degrade routing performance rather than complicating the normal static case.

---

## 4. Wire Scope: Named, Local-Bus, Internal, and Local-Domain

### Named Wires

A named Wire has a canonical `WireNumber` and may span Links through gateways.

### `kLocalBus` / Anonymous Wire

`kLocalBus` is a special **WireAlias**, normally alias 0, meaning the native Wire of a Physical Link.

If alias 0 is configured:

```text
CAN_A kLocalBus -> Wire 42
```

RX traffic is immediately canonicalized to Wire 42. Routing, logs, Endpoint dispatch, and later forwarding use the canonical Wire identity; `kLocalBus` is only the local Link representation.

If alias 0 is **not** configured, received traffic is still valid as an **Anonymous Local-Bus Wire** and is delivered to the local Endpoint Domain, but it cannot be generically forwarded because it has no canonical identity.

For zero-configuration TX, an anonymous LocalBus may be selected automatically when exactly one eligible Link Interface exists. With two or more eligible Links it is ambiguous and TX fails until Wiring assigns a Wire.

This lets a very small LocalBusOnly node always use alias 0 even when a gateway later maps that bus into a named multihop Wire.

### Device-private Wires

A high WireNumber range is reserved for **device-private Wires** used for inter-core/inter-domain communication. The current provisional allocation is `896..1022` (**127 internal Wires**), with `1023` reserved for `kLocalDomain`; the remaining 896 values are available for network-visible Wire allocation. Internal Wires use normal Wire semantics but must never be emitted onto an external Link Interface.

Inter-core communication itself is not a special Router concept: a shared-memory queue/channel is simply another Physical Link with Link Interfaces on each side.

### `kLocalDomain`

The highest reserved Wire value is intended as `kLocalDomain`: deliver within the originating Endpoint Domain and never enter a Link Interface. The same value can therefore be reused independently in every Domain.

The scope ladder is:

```text
kLocalDomain       one Endpoint Domain
Internal Wire      one device / SoC, possibly across cores
Named Wire         network-visible and routable
kLocalBus          Link-local alias; anonymous until mapped to a named Wire
```

---

## 5. WireAlias and Gateway Forwarding

A **WireAlias** is a compact Link-scoped representation of a Wire. The constrained 11-bit Classical CAN profile currently provides:

```text
alias 0      kLocalBus
aliases 1..7 configured aliases for named Wires
```

Aliases are independent per Link. The same canonical Wire may be alias 2 on one CAN bus and alias 6 on another.

Generic gateway forwarding uses a **complete canonical PDU**, not fragments or native Link frames:

```text
CAN frames -> CAN LLL -> canonical PDU -> Router -> UART/Ethernet/etc. LLL
```

A simple gateway table is conceptually:

```text
(WireNumber, ingress_link_interface_index)
    -> egress_link_interface_bitmask
```

An egress bitmask makes multicast cheap. Local Endpoint delivery is independent of egress forwarding.

Base forwarding is intentionally close to **flood-and-filter within the configured Wire realization**. Branch pruning by Node identity can be added as an optimization. The base multihop realization should be acyclic; more complex redundant/cyclic profiles can be added only when they have a concrete use case.

A failure to enqueue on one egress does **not** cancel successful transmission to other egresses.

---

## 6. QoS, TX Queues, and Congestion

The canonical descriptor reserves four QoS values, but platforms are not required to implement four scheduling classes. Two standardized implementation profiles are the current direction:

| QoS profile | Required classes |
|---|---|
| **Minimal** | Normal only |
| **Full** | Critical, High, Normal, Background |

A Link Interface typically owns one TX FIFO per implemented QoS class plus a configurable scheduler and queue-full policy. Queue policy is **per Link instance** so unusual Links can choose different behavior without changing WS semantics.

A congestion-related send failure is a normal communication outcome, not evidence that the driver is broken. APIs should make this explicit with a result such as `SendFailure::CongestedLink` / `E_FULL`; Services and Transports are expected to handle it without resetting the Link.

WireSpaces does not currently require a generalized end-to-end congestion-control protocol. The expected baseline is:

1. statically engineered traffic fits the Links in normal operation;
2. bounded Link queues have explicit admission/drop policies;
3. optional **hop-by-hop Link backpressure** protects aggregation Links and gateway memory;
4. reliable bulk Transports use receiver windows/credits where appropriate;
5. runtime telemetry makes saturation visible.

### Optional Link backpressure

Link-layer flow control is capability- and profile-dependent. It is especially useful on Ethernet, UART/RS-485, USB-like streams, FPGA FIFOs, or shared-memory Links feeding a slower gateway egress.

A gateway whose CAN TX buffers are filling can reduce credit advertised on the upstream Ethernet/serial Link, naturally propagating backpressure toward the producer. Classical CAN Nodes themselves are not expected to participate in generic LLL flow control; their ordinary traffic is usually bounded periodic traffic, while image/file transfer can rely on Transport flow control. CAN FD/XL may choose richer Link flow control where useful.

For credit-based profiles, flow-control information can be piggybacked as an LLL header extension. A simple representation is:

```text
QoS-Minimal:  1 x uint16 credit = 2 bytes
QoS-Full:     4 x uint16 credit = 8 bytes
```

The exact credit accounting protocol remains a Link-profile detail.

---

## 7. Link Capabilities, Errors, and Telemetry

Link capabilities should be inspectable so host tooling can reject impossible Wiring before traffic is sent. Useful capabilities include:

```text
maximum PDU / MTU
QoS profile
hop-by-hop flow-control support
fragmentation/reassembly support
header-extension support
WireAlias capacity / addressing limits
nominal link rate
```

Standard error/status semantics should distinguish normal congestion from actual faults. Representative conditions include:

```text
Congested / queue full
Link unavailable
Unsupported PDU/profile
Unknown or unmapped Wire/WireAlias
Unknown Endpoint
Reassembly timeout / integrity failure
Link-native framing / CRC / bus errors
```

**Queue overuse must be observable**, not merely final packet loss. Link drivers should expose queue utilization/high-water information and congestion-drop counts.

A useful observability ladder is:

```text
Tiny embedded
    global per-Link TX/RX/error/congestion-drop counters
    queue high-water / overuse indication

Medium embedded
    above + per-Wire traffic/drop counters
    periodic Link health snapshots

Linux / PC / large gateway
    above + circular drop journal
    timestamp + compact WS header for each congestion-dropped PDU
    (up to ~16 bytes/record with a 64-bit timestamp and <=8-byte header)
```

A proposed default **Domain-local Link Telemetry Service** can sample all Link Interfaces in its Endpoint Domain at roughly 1 Hz and report snapshots over an `InternalDebugWire`. Larger systems may also report the highest-bandwidth or highest-drop Wires for diagnosis.

---

## 8. Discovery, Configuration, and Static Capacity Checking

Static configuration is optional.

A host-side **Organizer** can discover devices and gateways, assign collision-free NodeIds, name Anonymous Wires, allocate WireAliases, inspect Link capabilities, and install gateway forwarding tables. The resulting runtime data plane remains simple and mostly static.

```text
discover
 -> assign NodeIds
 -> identify Wires/gateways
 -> name Wires where needed
 -> allocate aliases
 -> validate Link capabilities
 -> install local forwarding tables
 -> ordinary fixed forwarding
```

Pre-addressing discovery on shared media such as CAN remains below ordinary Endpoint/Wire semantics because unaddressed Nodes cannot all safely reply using the same normal CAN identity.

For controlled deployments, Wiring/Manifest tooling can also use Service-declared worst-case TX rates and encoded sizes to estimate worst-case utilization of every traversed Link and reject obvious saturation before deployment. This static analysis is expected to handle most serious embedded systems better than adding a complex adaptive congestion protocol.

---

## 9. Current Classical CAN Snapshot

Classical CAN remains an important constrained compatibility profile, not the definition of WireSpaces itself.

| Property | Current direction |
|---|---|
| Nodes per Wire | 31 Nodes + 1 Origin |
| Link-local Wire representations | `kLocalBus` + 7 named WireAliases per CAN Link |
| Canonical EndpointId | 16 bits per Namespace; current General PDUA directly represents EID 1..1023 |
| PDUA maximum | **8 CAN frames** |
| Normal design target | **N <= 4** |
| Aggregate CRC | N=1: none; **N=2..4: CRC-8**; N=5..8: CRC-16 |
| Optimized tiny CAN EIDs | Namespace 0, EID 1..127 |
| Small implementation option | MaxN=4, requiring only the standardized CRC-8 |
| Larger implementation | MaxN=8, adds CRC-16 for N=5..8 |

The PDUA frame-control design now has enough space for a 3-bit MessageGeneration and 3-bit FramesRemaining counter, reducing stale-fragment alias risk while keeping MaxN bounded at 8. Exact byte layouts, CRC parameter sets, and CAN-ID bit ordering belong in the CAN profile specification rather than this document.

---

## 10. Host, Software, and RTL Integration

A major goal is for WS to be useful with **one device**:

```text
PC
 |
USB / UART / Ethernet / FTDI FIFO
 |
Device
    identity / build info
    health / Link telemetry
    logs / structured events
    firmware update
    application Endpoints
```

The same Services and tools then scale to a whole machine through gateways.

Software and RTL are peers in the architecture. Endpoints, LLLs, and routing can be implemented in C++ firmware, host software, softcores, or pure RTL. A long or deeply pipelined on-chip connection does **not** imply another Wire; physical distance, register stages, clock-domain crossings, and interconnect pipelines are implementation details of the Link realizing that logical Wire.

The first software implementation should favor **C++ for development speed**, while keeping the core API reasonably C-shaped where practical. A separate C implementation or C ABI can be added later when a real target requires it.

---

## 11. Deliberately Outside This Overview

This is not the formal protocol definition. It intentionally omits:

- byte-exact CAN, UART, Ethernet, and other Link encodings;
- exact CRC algorithms and framing details;
- Flow, WireContract, and detailed Manifest semantics;
- advanced authority, schedulability, redundancy, and assurance analysis;
- exact commissioning collision-resolution procedures;
- a general security architecture;
- cyclic/redundant forwarding profiles;
- precise C++/C/RTL APIs.

Those details should be standardized only when implementation or concrete use cases justify them.

> **WireSpaces is a bus-oriented embedded communication fabric: small enough for constrained nodes, explicit enough for gateways and tooling, and regular enough to span software, multicore systems, host links, and RTL without changing the application communication model.**

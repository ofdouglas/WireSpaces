# WireSpaces — Introduction

**Status:** Private first draft; provisional throughout; not polished

**Purpose:** Orientation — what WireSpaces is, why it exists, and where it is going

**Detail lives elsewhere:** see the document map in §10

---

# 1. What WireSpaces Is

WireSpaces (WS) is a lightweight embedded communication fabric intended to span:

- one PC-connected device;
- small MCU networks;
- Classical CAN / CAN FD / CAN XL systems;
- UART and RS-485 links;
- USB or FTDI-style host links;
- Ethernet-connected embedded machines;
- multicore MCUs and SoCs;
- FPGA softcores and pure RTL Endpoints;
- shared-memory and FIFO communication;
- gateways between unlike physical media.

The central idea is deliberately **bus-oriented**, not socket-oriented and not arbitrary-graph-routing-oriented:

> A **Wire** is a loop-free **Logical Bus**: a connected propagation domain realized by one or more configured Links. Multiple Hosts may independently source traffic onto it.

A **Host** is an independently routed and dispatchable Endpoint Domain, not necessarily a physical device or a PC. Each Endpoint Domain has exactly one deployment-scoped `HostId`, and uses that identity on every Wire it joins. A canonical addressed PDU names its `SrcHostId` and `DestHostId`. The PDU propagates over the configured Link topology of its Wire; destination identity controls Host acceptance rather than ordinary next-hop selection.

WireSpaces should be useful in the smallest possible configuration:

```text
PC ---- one Physical Link ---- Device
```

and remain conceptually similar as the system grows:

```text
PC
 |
USB / Ethernet / FTDI FIFO
 |
Main SoC / FPGA
 |       |        |
CAN     RS-485   shared memory
 |       |        |
ECUs    devices  CPU domains / RTL
```

The same Endpoint and Service model should survive these changes in realization. A Wire may coincide with one Link, span several heterogeneous Links, exist entirely inside one device, or overlap other Wires on some of the same Links.

---

# 2. Why It Exists

Embedded systems typically use several unrelated communication mechanisms — direct calls and RTOS queues inside one MCU, shared memory between cores, CAN or RS-485 between MCUs, Ethernet or USB to a host, custom RTL interfaces inside FPGAs — while the *application-level* traffic is remarkably similar across all of them: small commands, status and telemetry, logs and events, request/response, firmware update, and bounded datagrams with a small number of known sizes.

Two further observations shape the design:

1. Traditional stacks often insert a central execution layer between producers and consumers even when the topology is statically known.
2. Embedded systems are usually **centrally engineered**. Their communication relationships are not arbitrary Internet routes discovered at runtime; they are largely fixed relationships between known components.

WireSpaces therefore treats the system as a set of **logical Wires** whose placement onto execution contexts and Physical Links is configurable. Ordinary forwarding uses static/read-mostly Wire membership, not destination routes. The same Service interaction can be realized as a direct local submission into bounded Endpoint storage, an inter-core shared-memory Link, a CAN bus, or an FPGA datapath without redesigning the Service-facing message model.

---

# 3. Design Biases

WireSpaces intentionally favors:

- **simple runtime execution** over sophisticated distributed control planes;
- **static/read-mostly tables** over dynamic distributed routing;
- **host-side configuration and validation** over large embedded management stacks;
- **bounded storage** and explicit resource exhaustion;
- **clear local invariants** over implicit "smart" behavior;
- **software/RTL symmetry** where it is natural;
- **zero-configuration bring-up** until topology becomes genuinely ambiguous;
- **implementation-driven standardization** rather than speculative completeness.

Wire scope should remain purposeful. Prefer the smallest Wire that usefully represents the required communication or broadcast scope; a broad Wire is valid, but its traffic may consume capacity on every member Link.

A useful summary is:

> **Complexity belongs in constructing configuration and tables, not in executing the data plane.**

The governing rule for all ongoing work follows from the same bias:

> **Do not add a new protocol abstraction until a concrete implementation or use case demonstrates that the current model cannot solve the problem cleanly.**

The networking foundation should remain understandable without learning any of the material in `future_work.md`.

---

# 4. Non-Goals

The base architecture is not trying to provide:

- Internet-scale routing;
- arbitrary peer-addressed graph networking as the primitive;
- mandatory dynamic routing convergence;
- mandatory distributed discovery on every target;
- a general end-to-end congestion-control algorithm;
- a universal security layer;
- automatic controller or leader election;
- mandatory full static system modeling before first communication;
- a general byte-stream abstraction;
- a requirement to use dynamic memory, an RTOS, or lock-free algorithms;
- a requirement that tiny targets implement every feature;
- a defined interface between a Service and the application code that uses it.

The last is deliberate rather than unfinished, and it is a statement about one boundary only. WireSpaces defines the Endpoint/Transport Entity boundary — bounded receive classification and storage, deferred protocol processing, declared storage/concurrency, identity, and authority (`CORE §1.6`, `CORE §20`). It does not define the Service-to-application interface. What a Service hands to *its* user code is the Service author's design problem, because the span from a bare-metal `switch` to an RTOS task to an RTL register block to a host binding is too wide for one API vocabulary to fit honestly.

The surface *below* a Service is the opposite case. The Endpoint API is meant to be a portability contract: a Service's WireSpaces-facing code should compile and behave identically across implementations on comparable stacks, so that a low-end and a high-end 32-bit MCU can run identical Service source over entirely different network stacks. That is a precondition for any Service ecosystem, and it is bounded the same way link independence is — by the Service's declared resource and timing envelope, and by whatever non-WireSpaces dependencies it reaches for.

---

# 5. Characteristic Messages

WireSpaces is **datagram-oriented**. Most Services define a small set of bounded message types; many are fixed-size or tightly bounded rather than arbitrary streams.

Illustrative sizes, not requirements:

```text
Heartbeat             4 B
TemperatureStatus     8 B
MotorCommand         12 B
LinkStatus           16 B
FaultEvent            8-24 B
BootloaderCommand     8-16 B
FirmwareSegment      tens to hundreds of bytes
```

The design pressure is:

> Common embedded Services should be easy to carry over constrained Links without forcing the entire ecosystem to adopt the smallest possible payload.

Classical CAN is an important compatibility floor. A useful tiny device may support only one CAN frame per WS PDU. Mainstream MCU Service libraries should generally remain comfortable with roughly 2-4 CAN frames per PDU for common operations, with larger aggregation available where appropriate.

---

# 6. Usage Maturity Levels

WireSpaces should be useful long before a user is ready to model, constrain, and statically analyze a whole system. The intended progression is:

```text
Level 0
    Connect devices. Send messages.
    Use a valid HostId and kLocalBus on one Link.
    Minimal or no authored named topology.

Level 1
    Discover devices. Assign Host identities and named Wires.
    Use host auto-Wiring.

Level 2
    Save and deploy a repeatable static Wiring configuration.

Level 3
    Define richer Services, generated bindings, documentation,
    and whole-system topology.

Level 4
    Add optional WireContracts and advanced static analysis.

Level 5
    Use restricted profiles and stronger assurance processes
    where required.
```

> **Advanced features must not make Level 0 or Level 1 unnecessarily difficult.**

This is a ladder of user commitment, and it is orthogonal to the implementation scaling profiles in `IMPL §2`, which are a ladder of target hardware. A tiny bare-metal MCU can legitimately participate in a Level 3 system, and a Linux gateway can legitimately sit at Level 0 on a bench.

Three consequences are worth stating directly:

- static configuration is a **capability, not an admission requirement**;
- dynamic host-side configuration can still produce a fixed data plane (`DEPLOY §1.4`);
- advanced analysis is **additive** — WireContracts, authority restrictions, schedulability and redundancy analysis build on the Wire model rather than defining the minimum viable user experience.

Level 0 is still canonical: `kLocalBus` supplies a reserved local-only WireNumber, while every ordinary PDU still has a valid source `HostId`. As a deployment matures, it can replace a broad or local bring-up Wire with named narrow and overlapping Wires when measured bandwidth, failure scope, or locality gives that extra configuration a purpose.

---

# 7. The Model at a Glance

```text
                 Endpoint Domain
          one deployment-scoped HostId
                        |
             Endpoint/Service Dispatcher
                        |
                      Router
          static/read-mostly Wire topology
              /          |          \
         Link IF      Link IF      Link IF
            |             |            |
           LLL           LLL          LLL
            |             |            |
          CAN        shared memory   Ethernet
              \          |          /
               configured Logical Wires
```

The preferred provisional ordinary canonical descriptor is **48 bits / 6 bytes**:

```text
Control
    QoS
    Reserved
    Header-extension flag
    TransportType
WireNumber
SrcHostId
DestHostId
Endpoint
```

A Wire provides the loop-free Logical Bus. Every PDU on it has canonical source and destination identity. Multiple Hosts may initiate traffic; a directed PDU still propagates over the Wire but is accepted only by its destination, while a Wire-wide broadcast may be accepted by all Hosts implementing the Endpoint.

A constrained Link does not need to transmit every canonical field literally. Its Link Binding may elide a WireNumber or project canonical Host IDs into Link-local codes, but ingress must reconstruct unambiguous canonical Wire, source, and destination values before generic forwarding or dispatch.

`kLocalBus` is the reserved canonical local-only WireNumber for one-Link bring-up. It still requires a valid source `HostId`; it may be dispatched locally but is not transparently forwarded or spliced as itself.

Wires may overlap. A broad command Wire and narrower subsystem Wires can share some Links while preserving distinct propagation scopes. Prefer the smallest useful scope because traffic on a broad Wire consumes resources across all of its member Links.

The Router propagates complete canonical PDUs through bounded local tables. Link queues expose congestion rather than hiding it. Optional hop-by-hop credits can backpressure fast aggregation Links. Configuration and tooling should prevent normal steady-state saturation without making static configuration a prerequisite for first communication. Richer systems add diagnostics and host tooling without forcing that complexity onto tiny Hosts.

> **WireSpaces is intended to make embedded communication look like a small set of logical buses carried by interchangeable Links, with host-side intelligence building simple local forwarding state that can be executed efficiently in MCU software, multicore systems, Linux gateways, and RTL.**

---

# 8. Worked Examples

## 8.1 One dev board

```text
PC
 |
USB/UART
 |
MCU
```

MCU:

```text
one Endpoint Domain
one valid HostId
one Link
Identity Service
Health Service
Log Service
Firmware Update Service
```

Behavior:

- canonical `kLocalBus`, bound to the one local Link;
- no named WireNumber configuration;
- direct/linear dispatch;
- copy-based send;
- PC tool immediately shows identity, health, logs, and update capability.

The Link still reconstructs canonical Wire, source, and destination values. `kLocalBus` removes named-Wire setup, not Host identity, and it cannot be transparently extended onto another Link.

## 8.2 Multicore MCU with telemetry

```text
Control CPU -------- shared-memory Link -------- I/O CPU
      \                        |                    /
       \---------------- W_Telemetry -------------/
                            |
                         FPGA RTL
```

Each CPU Endpoint Domain and the RTL Endpoint Domain has one deployment-scoped `HostId`. All are Hosts on `W_Telemetry`, and any of them may publish status or direct a PDU to another Host. This is a multi-initiator Logical Bus, not a controller with permanent subordinate roles. A directed PDU propagates over the configured Wire topology and only the destination accepts it; a broadcast can be accepted by every Host implementing the Endpoint.

See `CORE §13` for multicore Links and `INTRO §6` Level 1–2 for discovery and commissioning.

## 8.3 CAN-to-Ethernet gateway

```text
                     Main controller
                      /            \
                 Left Link      Right Link
                    |                |
                 Left ECUs        Right ECUs

W_Left     = Left Link
W_Right    = Right Link
W_Command  = Left Link + Right Link
```

`W_Left` and `W_Right` keep status local to each branch. The overlapping `W_Command` Wire propagates commands across both branches when that broad scope is useful. The gateway forwards by Wire topology while preserving canonical source, destination, Endpoint, control metadata, and payload; application composition creates a new PDU instead.

Native CAN11 can represent this overlap with separate WireAliases, each selecting a canonical Wire and VCN map. A Guest allocation supplies one Wire. Use CAN29 or another richer profile when alias/VCN capacity, migration headroom, or configuration cost makes CAN11 unsuitable (`LINK §2`).

See `CORE §12` for gateway forwarding and `DEPLOY §1.3` for recursive discovery through gateways.

---

# 9. Near-Term Implementation Roadmap

The architecture should now be tested by building, not expanded indefinitely on paper.

A strong first implementation sequence:

```text
 1. Canonical PDU type + preferred provisional 48-bit / 6-byte descriptor codec
 2. Queue and Snapshot Endpoints + Domain Dispatcher
 3. read-mostly Router + static Wire forwarding tables
 4. simulator evidence: canonical dispatch and directed/broadcast acceptance
 5. simulator evidence: loop-free propagation and broad/narrow overlapping Wires
 6. simulator evidence: source/destination preservation, forwarding vs composition,
    and Wire->LinkMask versus (Wire, ingress)->egress-mask representations
 7. copy-based transmit Endpoints, Queue and Snapshot
 8. one simple host/serial or UDP Link
 9. canonical kLocalBus behavior, including valid Host identity
10. gateway forwarding between unlike Links
11. device-private Wires + splice to an external Wire
12. multicore/shared-memory Link simulation
13. CAN11 Guest VCN profile for an explicitly allocated identifier block
14. CAN11 Native VCN with default and explicit Host relationship maps
15. native alias selection and migration under bounded static configuration
16. CAN29/richer-profile path when alias/VCN limits or costs do not fit
17. Link counters/telemetry + queue-pressure reporting
18. host discovery/config tooling
```

Early concurrency should use mutexes, critical sections, bounded queues, and copies. More advanced routing tables, seqlocks, lock-free MPSC queues, per-Link pools, and zero-copy should be added after measurements demonstrate a need.

Note that step 2 now lands on the least-settled part of the architecture rather than a well-worn one. The Endpoint API is a portability contract (`SVC-9`), so its shape is worth deciding deliberately before Services are written against it — the open items are in `REG §6.12`.

This is a build order, not a decision order. What has to be settled before each stage, and what is safe to leave provisional as long as it is labeled, is in `CONFORM §5`; the discipline that keeps a provisional choice from quietly becoming a decision is in `CONFORM §1.1`. The preferred 48-bit descriptor remains provisional until its conformance evidence and freeze criteria are satisfied.

Guest and Native VCN are the current CAN11 direction. Native aliases select Wire plus VCN map; the default mapping provides two central Host positions and up to fourteen leaf positions. Guest-4 is the initial allocated-block profile, with wider Guest growth directions. Compact/General is experimental only. Exact profile packing, PDUA, CRC, and migration mechanisms remain provisional in `LINK` and `REG`.

Only after the above exist should the project freeze more advanced details such as UART framing, Link credits, richer Transport behavior, or static Manifest traffic analysis.

Agents should produce **small concrete reference implementations and tests**, not generalized framework hierarchies, unless the same abstraction is already demanded by more than one real Link/target.

The current implementation state lives in `sim/`, which is at step 0: a process shell with a no-op network seam.

---

# 10. Document Map

| Document | Code | Contents |
|---|---|---|
| `introduction.md` | `INTRO` | This document: intent, non-goals, maturity ladder, examples, roadmap |
| `core_architecture.md` | `CORE` | The buildable protocol and Host runtime. The main document |
| `bit_layout.md` | `BITS` | Byte and bit ordering conventions; canonical descriptor packing |
| `link_profiles.md` | `LINK` | Per-carrier encodings: Classical CAN, UART, Ethernet, I2C/SPI, others |
| `bits_transport.md` | `BITS-TRANSPORT` | Bounded finite-object Transport and unreliable sideband; prototype design with explicit open wire/lifetime decisions |
| `deployment.md` | `DEPLOY` | Discovery, commissioning, Wiring, host tooling |
| `conformance.md` | `CONFORM` | Reference vectors, boundary tests, exit criteria for provisional status |
| `implementation.md` | `IMPL` | Language choices, scaling profiles, execution shape |
| `library_architecture.md` | `LIB` | Core library structure, seams, and public API shape |
| `future_work.md` | `FUTURE` | Material not yet designed. Nothing here is a requirement |
| `architecture_register.md` | `REG` | Confidence levels, invariants, superseded concepts, open questions |
| `history.md` | `HIST` | Revision history and provenance (not a control surface) |
| `proposal_disposition.md` | `INTEGRATION` | Proposal incorporation status and follow-on boundaries |

Cross-references use the document code plus a section number, for example `CORE §6.2`. A bare `§6.2` always means the current document. `BITS` continues to mean bit layout; `BITS-TRANSPORT` identifies the BITS protocol document.

Two conventions matter when editing:

- `REG` is the control surface. A change to any document is not finished until the confidence levels, invariants, superseded list, and open questions in `REG` reflect it.
- `FUTURE` holds content; `REG` holds status. If an item needs both, the status entry is authoritative and the future entry expands it.

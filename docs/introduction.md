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

> A **Wire** is a logical bus with exactly one **Origin** and zero or more **Nodes**.

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
ECUs    Nodes    CPU domains / RTL
```

The same Endpoint and Service model should survive these changes in realization.

---

# 2. Why It Exists

Embedded systems typically use several unrelated communication mechanisms — direct calls and RTOS queues inside one MCU, shared memory between cores, CAN or RS-485 between MCUs, Ethernet or USB to a host, custom RTL interfaces inside FPGAs — while the *application-level* traffic is remarkably similar across all of them: small commands, status and telemetry, logs and events, request/response, firmware update, and bounded datagrams with a small number of known sizes.

Two further observations shape the design:

1. Traditional stacks often insert a central execution layer between producers and consumers even when the topology is statically known.
2. Embedded systems are usually **centrally engineered**. Their communication relationships are not arbitrary Internet routes discovered at runtime; they are largely fixed relationships between known components.

WireSpaces therefore treats the system as a set of **logical Wires** whose placement onto execution contexts and Physical Links is configurable. The same Service interaction can be realized as a direct callback, an inter-core shared-memory Link, a CAN bus, or an FPGA datapath without redesigning the Service-facing message model.

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
- automatic Origin election;
- mandatory full static system modeling before first communication;
- a general byte-stream abstraction;
- a requirement to use dynamic memory, an RTOS, or lock-free algorithms;
- a requirement that tiny targets implement every feature.

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
    Minimal or no authored topology.

Level 1
    Discover devices. Assign identities and Wires.
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

This is a ladder of user commitment, and it is orthogonal to the implementation scaling profiles in `CORE §26`, which are a ladder of target hardware. A tiny bare-metal MCU can legitimately participate in a Level 3 system, and a Linux gateway can legitimately sit at Level 0 on a bench.

Three consequences are worth stating directly:

- static configuration is a **capability, not an admission requirement**;
- dynamic host-side configuration can still produce a fixed data plane (`DEPLOY §1.4`);
- advanced analysis is **additive** — WireContracts, authority restrictions, schedulability and redundancy analysis build on the Wire model rather than defining the minimum viable user experience.

---

# 7. The Model at a Glance

```text
                         Endpoint Domain
                              |
                           Dispatcher
                              |
                         Endpoint/Service
                              |
                              v
                            Router
                     shared read-mostly state
                    /          |          \
                   /           |           \
              Link IF      Link IF      Link IF
                 |             |            |
                LLL           LLL          LLL
                 |             |            |
               CAN        shared memory   Ethernet
```

The PDU carries:

```text
Namespace
TransportType
QoS
Header-extension flag
WireNumber
Direction
NodeId
EndpointId
```

A Wire provides the logical bus: one Origin, zero or more Nodes.

A constrained Link may encode a Wire using a local alias. `kLocalBus` makes one-Link prototypes work before a canonical WireNumber exists. Device-private Wires and `kLocalDomain` reuse the same model for multicore/internal communication, and a splice is the one explicit place where an internal Wire becomes externally visible.

The Router forwards complete PDUs through bounded local tables. Link queues expose congestion rather than hiding it. Optional hop-by-hop credits can backpressure fast aggregation Links. Static configuration/tooling should prevent normal steady-state saturation. Richer systems add diagnostics and host tooling without forcing that complexity onto tiny Nodes.

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
one Link
Identity Service
Health Service
Log Service
Firmware Update Service
```

Behavior:

- anonymous `kLocalBus`;
- no WireNumber configuration;
- direct/linear dispatch;
- copy-based send;
- PC tool immediately shows identity, health, logs, and update capability.

This is not a demo subset that violates the architecture. It is the architecture at its smallest useful scale.

## 8.2 Multicore MCU with telemetry

```text
                    MCU

Core 0 Domain                Core 1 Domain
-------------                -------------
SensorService                EstimatorService
ControlService               PlannerService
      \                          /
       \                        /
        shared-memory WS Link
                 |
           Core 2 Domain
           -------------
           HealthService
           LogService
           Telemetry Link
                 |
                CAN
                 |
                 PC
```

Internal application messages use device-private Wires. Same-domain traffic dispatches `Inline` or through `Serialized` Service inboxes. Cross-core traffic uses shared-memory Link Interfaces.

Selected health/debug traffic uses the Internal Debug Wire, which is spliced to the host-facing telemetry Wire so it becomes visible on CAN as a network-visible WireNumber.

No separate IPC middleware is required.

## 8.3 CAN-to-Ethernet gateway

```text
CAN_A ----\
CAN_B -----\
CAN_C ------+--> MCU/FPGA gateway --> Ethernet --> Host
CAN_D -----/
```

Receive path:

```text
CAN frame(s)
    |
CAN LLL / PDUA
    |
complete WS PDU
    |
Router table
    |
    +--> Ethernet TX
    |
    `--> optional local Endpoint Domain tap
```

Ethernet can coalesce multiple small PDUs. The gateway does not need to understand the application Service carried by every Wire.

A copy-based MCU gateway may only need CAN LLL buffers, route lookup, an Ethernet TX copy queue, and a local tap copy if configured. A future FPGA implementation can replace the copies with streaming/buffer-descriptor mechanisms without changing the topology model.

---

# 9. Near-Term Implementation Roadmap

The architecture should now be tested by building, not expanded indefinitely on paper.

A strong first implementation sequence:

```text
 1. Canonical PDU type + 40-bit descriptor helpers
 2. Endpoint Domain Dispatcher (Inline + Serialized delivery)
 3. read-mostly Router + static forwarding table
 4. copy-based send() abstraction and TX binding injection
 5. one simple host/serial or UDP Link
 6. one Classical CAN/vcan Link with PDUA + aggregate CRC
 7. LocalBus configured/unconfigured behavior
 8. gateway forwarding between unlike Links
 9. device-private Wires + splice to an external Wire
10. multicore/shared-memory Link simulation
11. Link counters/telemetry + queue-pressure reporting
12. host discovery/config tooling
```

Early concurrency should use mutexes, critical sections, bounded queues, and copies. More advanced routing tables, seqlocks, lock-free MPSC queues, per-Link pools, and zero-copy should be added after measurements demonstrate a need.

Only after the above exist should the project freeze more advanced details such as UART framing, Link credits, richer Transport behavior, or static Manifest traffic analysis.

Agents should produce **small concrete reference implementations and tests**, not generalized framework hierarchies, unless the same abstraction is already demanded by more than one real Link/target.

The current implementation state lives in `sim/`, which is at step 0: a process shell with a no-op network seam.

---

# 10. Document Map

| Document | Code | Contents |
|---|---|---|
| `introduction.md` | `INTRO` | This document: intent, non-goals, maturity ladder, examples, roadmap |
| `core_architecture.md` | `CORE` | The buildable protocol and node runtime. The main document |
| `link_profiles.md` | `LINK` | Per-carrier encodings: Classical CAN, UART, Ethernet, I2C/SPI, others |
| `deployment.md` | `DEPLOY` | Discovery, commissioning, Wiring, host tooling |
| `future_work.md` | `FUTURE` | Material not yet designed. Nothing here is a requirement |
| `architecture_register.md` | `REG` | Confidence levels, invariants, superseded concepts, open questions |

Cross-references use the document code plus a section number, for example `CORE §6.2`. A bare `§6.2` always means the current document.

Two conventions matter when editing:

- `REG` is the control surface. A change to any document is not finished until the confidence levels, invariants, superseded list, and open questions in `REG` reflect it.
- `FUTURE` holds content; `REG` holds status. If an item needs both, the status entry is authoritative and the future entry expands it.

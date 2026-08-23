# Sketch 01 — MCU Development Board

**Intent:** Test the smallest useful WireSpaces deployment, then grow the same board into a USB/UART-to-CAN gateway without turning each Physical Link into a separate logical network.

This sketch has two configurations:

- **A — Single board:** one PC and one MCU connected by USB/UART.
- **B — Board with child MCUs:** the same MCU also gateways three smaller MCUs on committed Classical CAN.

The examples assume the stable capabilities listed in [`README.md`](README.md). CAN commissioning exists but its pre-addressing narrative is intentionally omitted.

---

# Configuration A — One PC, one MCU, one cable

## 1. What changed and maturity

This is the baseline:

- one engineer PC;
- one MCU development board;
- one point-to-point USB CDC/UART Physical Link;
- no gateway and no authored topology.

**Maturity level: Level 0.** The connection can use anonymous `kLocalBus`; neither a WireNumber nor a Manifest is needed.

## 2. Native communication model

Without WireSpaces terminology:

- The PC asks the board for identity, build information, and current health.
- The board periodically reports health and emits logs and fault events.
- The PC sends development commands and configuration changes.
- The PC can transfer a firmware image to the board.
- The board never needs to choose among several remote destinations: the cable has exactly one peer at each end.

The natural communication domain is the serial connection itself. There is no reason to invent separate application, log, or firmware networks for this configuration.

## 3. Obvious conventional implementation

> **Obvious conventional implementation:** a framed serial protocol over USB CDC or UART, with a message type/opcode, request identifiers where needed, bounded receive buffers, and a bootloader transfer state machine.

Compared with that baseline, WireSpaces introduces one logical Wire with Origin/Node roles, Endpoint identities, Endpoint storage semantics, and transmit bindings. In Level 0 the Wire is implicit in the only Link, so it does not add a topology configuration artifact.

## 4. WireSpaces mapping

### 4.1 Topology

```text
Engineer PC                                      MCU development board

Host Endpoint Domain                            MCU Endpoint Domain
PC is Origin                                    Board is Node 1
       |                                               |
       +--------- anonymous kLocalBus -----------------+
                   USB CDC / UART

Identity, health, logs, commands, and update traffic all use this Wire.
```

The PC is a natural Origin: it initiates maintenance operations and is the required sink for board publications. The board is the only Node. `kLocalBus` means this one Physical Link's native Wire; it does not mean canonical WireNumber zero (`CORE §5`).

### 4.2 Devices

| Device | Endpoint Domains | Link interfaces | Role summary |
|---|---|---|---|
| Engineer PC | One host-tool Domain | USB CDC/UART | Origin; discovers, observes, commands, and updates |
| MCU board | One MCU Domain | USB CDC/UART | Node 1; exposes board Services |

### 4.3 Wires

| Wire (name / #) | Origin | Nodes | Physical realization | Notes |
|---|---|---|---|---|
| Board LocalBus / anonymous | Engineer PC | MCU board (1) | One USB CDC/UART Link | Link-local identity only; no forwarding or splice |

There is no separate maintenance diagram or maintenance Wire. All traffic already has the same physical reachability and the same PC-to-board authority shape; splitting it would add distinctions that the system does not have.

### 4.4 Interactions

| Interaction | Producer | Consumer(s) | Wire | Direction | Notes |
|---|---|---|---|---|---|
| Identity/health query | PC | MCU | Board LocalBus | OriginToNode | Request-scoped response |
| Identity/health response | MCU | PC | Board LocalBus | NodeToOrigin | Queue for replies; source is unambiguous |
| Periodic health snapshot | MCU | PC | Board LocalBus | NodeToOrigin | Snapshot transmit; unreliable datagram with freshness |
| Log or fault event | MCU | PC | Board LocalBus | NodeToOrigin | Queue; intermediate events matter |
| Development command | PC | MCU | Board LocalBus | OriginToNode | Queue; command rejection must be visible locally |
| Firmware image | PC | MCU | Board LocalBus | OriginToNode | Reliable segment transport; bounded receiver window |
| Update status | MCU | PC | Board LocalBus | NodeToOrigin | Request-scoped or bounded progress publication |

No Endpoint IDs are invented here. Stable Service-library allocations should supply the identity, health, log, and firmware-update Endpoints.

### 4.5 Runtime shape

On the MCU this can compile down to:

```text
serial RX -> framing/integrity -> EID dispatch -> bounded Endpoint storage
Service publish -> transmit Endpoint -> serial TX
```

There is no useful central Router task. A generated `switch`, fixed Endpoint storage, and one Link driver preserve the architecture (`CORE §25`).

## 5. Rejected alternatives

| Alternative | Why it is not the primary mapping |
|---|---|
| One Wire each for commands, telemetry, logs, and update | The cable and authority relationship are the same for all four; the split creates topology without a natural communication boundary |
| Device-private Internal Debug Wire plus splice | With one Endpoint Domain and one external Link, the splice adds configuration but no isolation or routing benefit |
| Assign a canonical WireNumber immediately | Valid at Level 1+, but unnecessary until the Link must participate in discovery history, forwarding, or repeatable Wiring |
| Synchronous Service callback on RX | It removes the required bounded Endpoint storage boundary and makes Link execution depend on application code (`CORE §9.4`) |

## 6. Friction signals

| Signal | Rating | Reason |
|---|---|---|
| Artificial Origin | None | The PC naturally initiates maintenance and receives board publications |
| Artificial Wire | None | Anonymous LocalBus is exactly the one physical communication domain |
| Wire proliferation | None | One Wire carries the one relationship |
| Forwarding tax | None | There is no gateway |
| Identity awkwardness | Mild | NodeId adds little with one peer, but it is trivial and prepares the same Service model for larger systems |
| Interaction awkwardness | None | Requests, replies, publications, events, and update segments all fit the two directions |
| Configuration burden | None | Single-Link anonymous TX is unambiguous and needs no authored Wiring |
| Role instability | None | The PC remains Origin and the board remains Node while connected |
| Failure mismatch | None | Loss of the only Link accurately means loss of the only remote path |

## 7. Model pressure

- **If one WireSpaces concept could change:** no change is demanded here. A constrained implementation should be allowed to compile the one-Wire routing machinery into constants, which the current small-MCU profile already permits.
- **Useful distinction exposed by WS:** Queue versus Snapshot storage makes event loss, latest-value replacement, and local send rejection explicit instead of leaving them as accidental properties of one serial receive loop.

## 8. Open questions

- Which byte-stream framing and CRC become the stable USB CDC/UART profile (`LINK §3`)?
- Which Level-0 Service catalog entries and compact Endpoint allocations are guaranteed across host tools?
- Should the host assign a temporary canonical WireNumber during discovery while still presenting the connection to the user as zero-configuration?

---

# Configuration B — Main MCU gateways three CAN child MCUs

## 1. What changed and maturity

Relative to Configuration A:

- the main MCU gains a committed Classical CAN Link Interface;
- three child MCUs share that CAN bus;
- the main MCU autonomously supervises and commands the children even when the PC is absent;
- the PC can inspect, command, and update both the main MCU and the children through the main MCU;
- the main MCU forwards canonical PDUs between USB/UART and CAN.

**Maturity level: Level 1.** The Organizer discovers the topology, assigns NodeIds and named Wires, allocates CAN aliases, and installs ephemeral forwarding. Export to a Level-2 static deployment is the natural next step, but is not required for bench use.

## 2. Native communication model

Without WireSpaces terminology:

- The main MCU owns normal operation of three child MCUs.
- The main MCU sends child setpoints, mode changes, and bounded requests.
- Each child publishes current state and fault events to the main MCU.
- The PC is a temporary maintenance authority for every MCU.
- While connected, the PC observes normal child state without requiring the child to send a duplicate CAN message.
- The PC may send diagnostics or firmware to a selected MCU.
- USB and CAN require different framing, but the main MCU should not reinterpret ordinary Service messages merely because it relays them.

There are two natural authority relationships:

1. **device control:** main MCU to child MCUs;
2. **bench maintenance:** PC to every MCU.

Those relationships overlap on the same devices and Physical Links. They are not the same relationship, and neither is merely “the USB side” or “the CAN side.”

## 3. Obvious conventional implementation

> **Obvious conventional implementation:** a framed USB protocol to the main MCU, direct CAN identifiers for child commands and publications, plus gateway firmware that maps USB requests to CAN IDs and routes CAN responses and telemetry back to the correct host operation.

That conventional gateway already needs USB framing, CAN message allocation, destination selection, response routing, bounded queues, and update state. WireSpaces additionally names two authority-shaped Wires, preserves one Service identity across both carriers, and makes forwarding/alias configuration explicit.

## 4. WireSpaces mapping

### 4.1 Topology

```text
                                DeviceControlWire
                         Main MCU is Origin
                     PC and children are Nodes

Engineer PC                   Main MCU                    CAN children
Host Domain                   MCU Domain                  one Domain each
    |                             |                         |  |  |
    | USB/UART                    | CAN                     |  |  |
    +-----------------------------+-------------------------+--+--+
             \                       gateway forwarding          /
              \                                                   /
               +--------------- BenchMaintenanceWire ------------+
                         PC is Origin
                  main MCU and children are Nodes
```

Both logical Wires span the USB/UART and CAN Links where their configured membership requires it. The main MCU forwards the same canonical Wire identity; it does not translate a “USB Wire” into a “CAN Wire” (`CORE §12`).

The Wires overlap deliberately:

- On **DeviceControlWire**, the main MCU can address children. Child `NodeToOrigin` state is delivered to the main MCU and may also be explicitly consumed by the PC as an observing Node.
- On **BenchMaintenanceWire**, the PC can address the main MCU or any child directly. Replies and autonomous maintenance events return toward the PC.

### 4.2 Devices

| Device | Endpoint Domains | Link interfaces | Role summary |
|---|---|---|---|
| Engineer PC | One host-tool Domain | USB CDC/UART | Maintenance Origin; observing Node on device control |
| Main MCU | One MCU Domain | USB CDC/UART, committed Classical CAN | Device-control Origin; maintenance Node; gateway for both Wires |
| Child A — sensor MCU | One MCU Domain | committed Classical CAN | Publishes sampled state; accepts acquisition commands |
| Child B — actuator MCU | One MCU Domain | committed Classical CAN | Publishes actuator state/faults; accepts setpoints |
| Child C — I/O MCU | One MCU Domain | committed Classical CAN | Publishes I/O state/events; accepts output commands |

Preferred NodeIds are kept stable across Wires where the participant is a Node:

```text
Main MCU    1
Child A     2
Child B     3
Child C     4
PC          5
```

The main MCU has no NodeId on DeviceControlWire because it is that Wire's Origin. The PC has no NodeId on BenchMaintenanceWire for the same reason.

### 4.3 Wires — application and maintenance

| Wire (name / #) | Origin | Nodes | Physical realization | Notes |
|---|---|---|---|---|
| DeviceControlWire / assigned | Main MCU | PC (5), Child A (2), Child B (3), Child C (4) | USB/UART + CAN through main MCU | CAN-native operational relationship; PC consumption of child publications is explicit observation as a configured Node |
| BenchMaintenanceWire / assigned | PC | Main MCU (1), Child A (2), Child B (3), Child C (4) | USB/UART + CAN through main MCU | May be ephemeral at Level 1 or remain defined in a static deployment while its PC Origin is offline |

On the committed CAN Link, one Wire can use alias 0 as the configured native Wire and the other a named alias. A reasonable allocation is:

```text
alias 0 -> DeviceControlWire
alias 1 -> BenchMaintenanceWire
```

The USB/UART profile carries the same canonical identities in its own representation. Alias values are Link-local and do not appear in Service logic.

### 4.4 Interactions

| Interaction | Producer | Consumer(s) | Wire | Direction | Notes |
|---|---|---|---|---|---|
| Child setpoint/mode | Main MCU | selected child | DeviceControlWire | OriginToNode | Unreliable/e2e datagram with freshness as required |
| Child state snapshot | child | Main MCU; PC when attached | DeviceControlWire | NodeToOrigin | One CAN publication; PC consumes the forwarded copy |
| Child fault event | child | Main MCU; PC when attached | DeviceControlWire | NodeToOrigin | Queue semantics; configured PC observation does not change the required sink |
| Main-to-child query | Main MCU | selected child | DeviceControlWire | OriginToNode | Request-scoped reply |
| Child query response | child | Main MCU | DeviceControlWire | NodeToOrigin | Reply authority comes from registration, not merely reversed Direction |
| Host identity/health query | PC | selected MCU | BenchMaintenanceWire | OriginToNode | Same Service protocol for main and children |
| Host development command | PC | selected MCU | BenchMaintenanceWire | OriginToNode | Explicit maintenance-build authority |
| MCU logs/events | any MCU | PC | BenchMaintenanceWire | NodeToOrigin | Queue; bounded/rate-controlled diagnostics |
| Firmware image | PC | selected MCU | BenchMaintenanceWire | OriginToNode | Reliable segment transport; gateway does not terminate it |
| Firmware progress/result | selected MCU | PC | BenchMaintenanceWire | NodeToOrigin | End-to-end source identity survives forwarding |

The PC does not command a child on DeviceControlWire: it is a Node there, not the Origin. It uses BenchMaintenanceWire, where that authority is explicit. Conversely, the main MCU does not need the PC to be present to operate DeviceControlWire.

### 4.5 Gateway forwarding

| Ingress link | Wire | Egress link(s) | Local delivery? | Splice? | Notes |
|---|---|---|---|---|---|
| Main MCU Domain | DeviceControlWire | CAN; USB/UART when PC branch is active | N/A — locally produced | No | Origin commands/publications fan out to configured branches |
| CAN | DeviceControlWire | USB/UART when PC branch is active | Yes | No | Child publication reaches main and configured PC from one CAN transmission |
| USB/UART | DeviceControlWire | None normally | Yes | No | Optional PC `NodeToOrigin` traffic terminates at the main-MCU Origin; it does not grant peer command authority |
| USB/UART | BenchMaintenanceWire | CAN when child is destination | Yes, when main MCU is destination | No | Flood-and-filter is sufficient at this scale |
| CAN | BenchMaintenanceWire | USB/UART | No, unless Main has an explicit observer binding | No | Child `NodeToOrigin` replies/events continue toward the PC Origin |

No Wire splice is needed. Both Wires are already network-visible named Wires, and the main MCU is forwarding between two external Physical Links rather than exposing a device-private Wire.

### 4.6 Failure behavior

- If the PC or USB Link disappears, the PC remains the configured Origin of BenchMaintenanceWire but is unreachable; Origin-dependent maintenance is unavailable and no role is reassigned.
- DeviceControlWire remains operational because its Origin is the main MCU.
- If CAN fails, the PC can still reach the main MCU on either Wire where local delivery is configured, but no forwarding policy can make the children reachable.
- If one egress queue rejects a forwarded PDU, successful local delivery or other egresses are not rolled back (`CORE §12.2`).

## 5. Rejected alternatives

| Alternative | Benefit | Why it is not the primary mapping |
|---|---|---|
| **One PC-origin Wire for everything** | Only one Wire and one CAN alias | The main MCU could not directly address child Nodes while acting as a Node; normal autonomous control would require re-origination or another Wire |
| **One main-origin Wire for everything** | Child control and telemetry are simple | The PC would be only a Node and could not directly address the other Nodes for maintenance |
| **USB Wire + CAN Wire with gateway translation** | Mirrors the cable diagram | It forces the main MCU to consume and re-author ordinary traffic, creates opcode/CAN-ID mapping, and loses end-to-end producer lineage |
| **Device-private Internal Debug Wires spliced outward** | Useful on a multicore or partitioned device | These devices each have one Domain and already have an explicit maintenance Wire; adding four private Wires and splices has no demonstrated benefit |
| **Duplicate every child publication onto both Wires** | PC sees all telemetry under its own Origin | It consumes a second CAN transmission and a second transmit binding for data the PC can explicitly observe on DeviceControlWire |

The rejected one-Wire mappings are important: the second Wire exists because there is a second real authority, not because WireSpaces or the second cable demands one.

## 6. Friction signals

| Signal | Rating | Reason |
|---|---|---|
| Artificial Origin | None | PC maintenance authority and main-MCU operational authority both exist in the native model |
| Artificial Wire | None | The two Wires correspond to two different command authorities |
| Wire proliferation | None | Exactly two real command authorities produce exactly two Wires |
| Forwarding tax | None | A USB-to-CAN gateway is required conventionally; canonical PDU forwarding replaces message-specific translation |
| Identity awkwardness | None | Stable device identity is independent of Wire-local NodeId, and no interaction is made difficult by the role-specific addressing |
| Interaction awkwardness | None | Main and PC use the Wires matching their actual authority; configured observation avoids duplicate child telemetry |
| Configuration burden | Mild | Named Wires, two CAN aliases, memberships, and forwarding must agree, but Organizer auto-Wiring can generate them |
| Role instability | None | Roles remain fixed when the PC disconnects; DeviceControlWire does not elect a replacement |
| Failure mismatch | None | Separating maintenance from device control accurately keeps local operation alive when the PC path fails |

## 7. Model pressure

- **If one WireSpaces concept could change:** no new concept is justified. The pressure is on tooling presentation: two overlapping Wires on two Links must be shown as two authority domains, not misleadingly as four cable segments.
- **Useful distinction exposed by WS:** the model separates forwarding from re-origination. Child state can reach both main MCU and PC while retaining the child's producer identity, and PC maintenance authority remains visibly distinct from normal main-MCU control.

## 8. Open questions

- Should the PC's configured consumption of `NodeToOrigin` child state be presented as ordinary Wire membership, an observation binding, or both in tooling?
- When the PC branch is absent, should its egress route be removed or remain installed against an unavailable Link with explicit counters?
- Which configuration fingerprint scope best detects a stale CAN alias map without unnecessarily coupling unrelated host settings (`DEPLOY §2.4`)?
- At what point should this Level-1 ephemeral topology be promoted to a static Level-2 deployment?
- Does firmware update require a dedicated maintenance Wire in a production build, or is Endpoint-level authority on BenchMaintenanceWire sufficient?


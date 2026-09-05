# Wiring generation

Run from examples/arduino-uno:

    make generate-wiring
    make test-codegen

Python 3 and PyYAML are required. Firmware builds regenerate demo_wiring.h when
the schema or generator changes. Keep the generated header alongside the source.
Endpoints remain handwritten in wiring_constants.h.

To generate another host:

    python3 codegen/wiring_codegen.py codegen/demo.yaml --local-host Pc -o /tmp/pc_wiring.h

Names must start with an uppercase ASCII letter, contain only letters, digits,
and underscores, and must not contain double underscores. They are used verbatim:
host Arduino becomes kArduinoHost, Wire TestWire becomes kTestWire, and interface
Uart becomes kUartEgress. Names are unique within each collection (interfaces
within their host). Generated symbol collisions are rejected. No Symbol field
or name normalization is supported.

Hosts declare explicit HostId values and Interfaces. Each interface names its
shared Link. EgressBit (0..7) is optional: explicit bits are reserved first, then
remaining interfaces are assigned the lowest free bits in interface-name order.
Adding or renaming an interface can change automatic assignments; explicit bits
pin them. Collisions and more than eight interfaces per host are rejected.
Links declare LinkType and optional BaudRate; UART_HDLC requires BaudRate.

Wires keep explicit WireId values and list Hosts, named Groups, or both. Membership
is the deduplicated union and must not be empty. Groups contain host names only:

```yaml
Groups:
  Drives: [DriveA, DriveB]

Wires:
  - Name: Powertrain
    WireId: 30
    Hosts: [Vcu, Safety, Bms]
    Groups: [Drives]
```

With neither Links nor Path, the resolver infers the unique minimal tree connecting
members. Physical Links are graph nodes, not pairwise connections between every
bus listener. Starting with the alphabetically first member, it finds paths to
the other members and requires every selected interface edge to be a graph bridge.
This rejects alternative connectivity, not just equal-length alternatives: there
is no shortest-path preference. Disconnected members fail. Cycles on irrelevant
branches are harmless. A single member is local-only, even in a cyclic topology.

Only selected attachments participate. Other hosts listening on a selected shared
bus are neither members nor relays automatically. Transit hosts get route masks,
but not local HostInfo membership. Membership is limited to six Wires per host.

When connectivity is ambiguous, choose one of:

- Legacy Links: an explicit list of physical Links. This intentionally includes
  **all** attachments on those Links, including nonmember hosts. The resulting
  propagation graph must be connected and acyclic; each Link needs two hosts.
- Path: a reusable named interface chain. It selects only the listed attachments.

```yaml
Paths:
  LongBenchPath:
    - Pc.UartVcp
    - Gateway.UartVcp
    - Gateway.CanA
    - LeafA.CanA
    - LeafA.RingA
    - LeafB.RingA

Wires:
  - Name: BenchTest
    WireId: 40
    Hosts: [Pc, LeafB]
    Path: LongBenchPath
```

Each pair is one hop over the same Link between different hosts. Consecutive
pairs join at the same transit host using different interfaces. A chain needs
at least one hop, cannot revisit a host or Link, and must include every Wire
member. Transit hosts need not be members. All named paths are validated,
including unused ones. Links and Path cannot be combined on a Wire.

There are no nested groups, wildcards, generated identities, branched explicit
paths, endpoint declarations, or authorization policies.

## Inspection and examples

Inspect resolution without generating C++:

    python3 codegen/wiring_codegen.py codegen/examples/bench.yaml --local-host Gateway --explain

The deterministic JSON includes all hosts' egress assignments and memberships,
expanded Wire members, transit hosts, selected interface attachments, and route
masks. Each result links back to named Wire, Group, Path, interface, and Link
declarations. Absent host route entries mean no participation; a member with a
zero mask is local-only. --explain cannot be combined with --output.

- demo.yaml: inferred two-host UART wiring; the generated header is unchanged.
- examples/bench.yaml: all seven planned PCB Links, including two shared CAN-FD
  buses, UART multidrop and three ring Links. LongBenchPath deliberately travels
  through Gateway, LeafA, and LeafB to LeafC. Other test Wires exercise the other
  Links using explicit selections. UART rates are example settings, not hardware
  validation; LinkType labels do not implement drivers or arbitration.
- examples/amr.yaml and examples/excavator.yaml: topology-only adaptations of
  sketches_b/07_amr.md and 08_excavator.md, using HostId, groups and inferred trees.
  Native non-WireSpaces RS-485/LIN peripherals are outside these graphs.

The implementation separates parsing/validation (wiring_schema.py), physical
graph resolution (wiring_topology.py), and C++ emission (wiring_codegen.py).

## Generated API

Output contains deployment HostIds and WireNumbers, the selected host's HostInfo,
interface masks and baud rates, routes(), and a Forwarder class. Constructor
arguments follow ascending EgressBit order. Supply an application-owned
PacketForwarder for each interface. Each selected bit invokes its bound forwarder
once, passing that interface's single-bit mask. Zero and unknown bits do nothing.
Bindings must outlive the generated Forwarder. No driver construction, heap
allocation, engagement flags, or runtime pointer table are generated; the class
stores one reference per interface and uses the existing virtual Link interface.

Routes combine all applicable local interface masks into one entry per Wire.
Local-only Wires have a zero mask; local delivery remains the application's
responsibility. An empty route set returns an empty span.
These routes are for locally originated traffic. The current Router API lacks
ingress identity, so this output does not implement gateway ingress forwarding
or reflection suppression.

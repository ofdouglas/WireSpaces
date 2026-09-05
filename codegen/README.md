# Wiring generation

Run from examples/arduino-uno:

    make generate-wiring
    make test-codegen

Python 3.11+ with PyYAML and NetworkX is required. Install the pinned compiler
dependencies in a local environment, from the repository root:

    python3 -m venv codegen/.venv
    codegen/.venv/bin/python -m pip install -r codegen/requirements.txt

If the system Python has pip but lacks ensurepip, this equivalent setup works:

    python3 -m venv --without-pip codegen/.venv
    python3 -m pip --python codegen/.venv/bin/python install -r codegen/requirements.txt

The Uno Makefile uses that environment when present, otherwise python3.
Override CODEGEN_PYTHON to use another environment. Builds never install
dependencies or fetch anything from the network automatically.

Firmware builds regenerate demo_wiring.h when the project's demo.yaml or compiler
changes. Keep both files alongside the application source. Endpoints remain
handwritten in wiring_constants.h.

To generate another host:

    codegen/.venv/bin/python codegen/wiring_codegen.py examples/arduino-uno/demo.yaml --local-host Pc -o /tmp/pc_wiring.h

Deployment YAML belongs to its project: in the project directory or a sibling or
child configuration directory. There is no compiler-owned configuration registry
and no special meaning to the input filename. codegen/demo.yaml is a standalone
language example, not the source used to build the Uno project.

Names must start with an uppercase ASCII letter, contain only letters, digits,
and underscores, and must not contain double underscores. They are used verbatim:
host Arduino becomes kArduinoHost, Wire TestWire becomes kTestWire, and interface
Uart becomes kUartEgress. Names are unique within each collection (interfaces
within their host). Generated symbol collisions are rejected. No Symbol field
or name normalization is supported.

Hosts declare explicit HostId values and Interfaces. Each interface names its
shared Link. When the interface name matches its Link, use shorthand:

```yaml
Hosts:
  - Name: Motion
    HostId: 2
    Interfaces: [PlantEthernet, ChassisCan]
```

A string expands to an interface whose Name and Link both equal that string.
Mappings require Link and may omit Name (which defaults to Link). Mix forms to
pin a bit or give an attachment a different local name:

```yaml
Interfaces:
  - PlantEthernet
  - {Link: ChassisCan, EgressBit: 3}
  - {Name: Can0, Link: PayloadCan}
```

Paths reference the resulting local name, e.g. Motion.Can0. The existing explicit
Name/Link form remains valid. Duplicate names and multiple attachments to the
same Link are rejected after normalization; shorthand never modifies the input.

EgressBit (0..7) is optional: explicit bits are reserved first, then
remaining interfaces are assigned the lowest free bits in interface-name order.
Adding or renaming an interface can change automatic assignments; explicit bits
pin them. Collisions and more than eight interfaces per host are rejected.
Links declare LinkType and optional BaudRate; UART_HDLC requires BaudRate.

### CAN frame format and timing

CAN and CAN_FD describe the frames WireSpaces transmits, not the hardware's
unused capabilities. Declare timing once on the shared Link:

```yaml
Links:
  - Name: ChassisCan
    LinkType: CAN
    ArbitrationBitrate: 500000
  - Name: DriveCan
    LinkType: CAN_FD
    ArbitrationBitrate: 500000
    DataBitrate: 2000000
```

- ArbitrationBitrate is required for CAN and CAN_FD.
- DataBitrate is permitted only for CAN_FD. Its presence enables bitrate
  switching; omission means FD frames with BRS disabled. An explicitly supplied
  data bitrate equal to the arbitration bitrate still means BRS is enabled.
- Rates are positive integer bits per second, representable in uint32_t.
  Controller/transceiver support and hardware-specific timing remain integration
  checks, not hardware capability declarations in this schema.
- CAN uses ArbitrationBitrate rather than BaudRate. CAN timing fields on other
  Link types, FdCapable and separately authored BitRateSwitch flags are rejected.

Migration: existing CAN/CAN_FD declarations must now provide ArbitrationBitrate.
No bitrate is silently chosen for an authored deployment. The bench and machine
sketch examples use explicitly labelled illustrative 500 kbit/s arbitration and
2 Mbit/s data rates; confirm these for the actual systems.

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

    codegen/.venv/bin/python codegen/wiring_codegen.py hardware/bench/topology.yaml --local-host Gateway --explain

The deterministic JSON includes all hosts' egress assignments and memberships,
expanded Wire members, transit hosts, selected interface attachments, and route
masks. Each result links back to named Wire, Group, Path, interface, and Link
declarations. Absent host route entries mean no participation; a member with a
zero mask is local-only. --explain cannot be combined with --output.

- codegen/demo.yaml: standalone inferred two-host UART example.
- examples/arduino-uno/demo.yaml: application-owned deployment used by its Makefile;
  the generated header is unchanged.
- hardware/bench/topology.yaml: all seven planned PCB Links, including two shared CAN-FD
  buses, UART multidrop and three ring Links. LongBenchPath deliberately travels
  through Gateway, LeafA, and LeafB to LeafC. Other test Wires exercise the other
  Links using explicit selections. UART rates are example settings, not hardware
  validation; LinkType labels do not implement drivers or arbitration.
- sketches_b/topologies/amr.yaml and sketches_b/topologies/excavator.yaml: topology-only adaptations of
  sketches_b/07_amr.md and 08_excavator.md, using HostId, groups and inferred trees.
  Native non-WireSpaces RS-485/LIN peripherals are outside these graphs.

## Compiler stages

The frozen records in wiring_models.py separate three representations:

1. AuthoredDeployment: validated declarations, including omitted versus explicitly
   empty fields and optional authored EgressBit choices. No inferred membership,
   attachments or automatically assigned bits are written into these records.
2. ResolvedDeployment: expanded membership, transit hosts and selected physical
   attachments, each referencing its authored Wire declaration. No route masks.
3. TargetProjection: per-host interface bits, membership tables and route masks
   for the current runtime. Constructor binding order is fixed here, not by the emitter.

Parsing/validation lives in wiring_schema.py, topology resolution in
wiring_topology.py, target lowering in wiring_projection.py, inspection serialization
in wiring_inspection.py and C++ emission in wiring_codegen.py. compile_deployment()
composes the stages; emit_header() consumes an existing projection. The original
generate_header() convenience entry point and CLI remain available.

Collections are immutable snapshots. Dictionaries are used at YAML/JSON boundaries
and for temporary indexes, not as mixed authored/resolved packet-routing records.
The current eight-interface and six-membership restrictions remain unchanged.
This iteration does not add Pydantic, JSON Schema, ingress forwarding or zero-copy.

NetworkX owns bridge discovery, breadth-first traversal, connectivity and tree
checks. WireSpaces retains the shared-bus model, explicit chain rules, legacy Link
selection, unique-path inference policy and error messages. Sorted traversal keeps
ambiguity witnesses deterministic. No shortest-path routing policy is introduced.

Tests retain compiled generated-forwarder checks and pre-refactor C++/inspection
output hashes for every example host plus a legacy explicit-bit deployment.
New CAN timing/format fields are excluded from those legacy hashes and checked
separately through validation, inspection, and compiled constant assertions.
An exhaustive 512-graph corpus cross-checks inference against enumerated simple
paths. Additional tests cover immutable stage boundaries and process-independent
inspection output.

## Generated API

Output contains deployment HostIds and WireNumbers, the selected host's HostInfo,
interface masks and baud rates, routes(), and a Forwarder class. Constructor
arguments follow ascending EgressBit order. Supply an application-owned
PacketForwarder for each interface. Each selected bit invokes its bound forwarder
once, passing that interface's single-bit mask. Zero and unknown bits do nothing.
Bindings must outlive the generated Forwarder. No driver construction, heap
allocation, engagement flags, or runtime pointer table are generated; the class
stores one reference per interface and uses the existing virtual Link interface.

CAN interfaces additionally emit k{Name}ArbitrationBitrate and k{Name}UseFdFrames.
FD interfaces emit k{Name}BitRateSwitch; k{Name}DataBitrate is emitted only when
explicitly configured. Names use the local interface name, including overrides.
These constants do not configure hardware automatically: application-owned
drivers consume them. --explain includes both the authored Link settings and
the derived CAN transmission settings (including bitrate_switch).

Routes combine all applicable local interface masks into one entry per Wire.
Local-only Wires have a zero mask; local delivery remains the application's
responsibility. An empty route set returns an empty span.
These routes are for locally originated traffic. The current Router API lacks
ingress identity, so this output does not implement gateway ingress forwarding
or reflection suppression.

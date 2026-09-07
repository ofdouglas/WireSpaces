# Wiring generation

Generated `Forwarder` constructors bind `PacketLink&` objects in egress-bit order.
Each Link implements `trySend(packet) -> LinkAdmission` without an egress mask.
`Accepted` means it has consumed/copied the bytes or acquired their lifetime;
it does not mean the remote endpoint received them.
Generated fan-out and `Router::forward()` return a byte-sized `RouteResult`:
accepted, partial, full, too large, or rejected; Router can also report no route,
invalid ingress, or no egress. A receiving leaf normally has no egress.
Partial acceptance is not rolled back or retried. When no Link accepts,
rejection takes precedence over size failure, then queue-full.
There are no per-egress reports, diagnostics, or logging in the router.
Only individual Links use `trySend()`. Local delivery is independent of outbound
admission; `receive()` returns the routing and delivery outcomes separately.
Driver-context use requires context-safe bounded Link and receiver implementations.

Run from examples/arduino-uno:

    make generate-wiring
    make test-codegen

Python 3.11+ with PyYAML, Pydantic and NetworkX is required (jsonschema tests editor
contract parity). Install the pinned compiler
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

## Structural validation and editor support

For interactive inspection, see [Read-only viewer](#read-only-viewer). The
[deployment studies](deployment_studies.md) exercise the current decisions against
a multicore gateway and a 40-host packaging line, including known limitations.

The strict Pydantic models in wiring_structure.py are the single source for field
shapes and the generated JSON Schema in wiring.schema.json. Unknown keys are
rejected at every object level; integers are not coerced from strings, booleans or
floats. Optional fields may be omitted, but explicit null is rejected. Duplicate
YAML mapping keys and duplicate membership references are errors. Semantic checks
then resolve identities/references, normalized interface uniqueness, bit collisions,
membership limits and topology. Even unused Paths and Realizations must be valid.

YAML editors with YAML Language Server support can use the included modeline:

```yaml
# yaml-language-server: $schema=../wiring.schema.json
```

Use a path relative to the YAML file (project configurations point back to codegen).
This supplies field completion, descriptions, structural diagnostics and LinkType
variants without a second hand-maintained schema. Cross-reference and graph errors
require running the compiler; JSON Schema does not resolve host or Link names.
JSON numbers have no separate lexical integer/float types, so the compiler also
rejects YAML `1.0` where an integer is required even if an editor accepts it.

Regenerate the checked-in editor schema and run the Python tests from the repo root:

    make -C codegen schema
    make -C codegen test

Equivalent schema command from the repo root:

    codegen/.venv/bin/python codegen/wiring_structure.py --output codegen/wiring.schema.json

The tests fail if the generated schema is stale
and check accepted/rejected structures against both validation implementations.

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
reference examples use explicitly labelled illustrative 500 kbit/s arbitration and
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

With no Links, Path or Realization, the resolver infers the unique minimal tree connecting
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
- Realization: a reusable exact set of interface attachments, supporting branches.

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
including unused ones. Links, Path and Realization are mutually exclusive on a Wire.

### Exact branched realizations

```yaml
Realizations:
  TestTree:
    Attachments:
      - Root.Uplink
      - Gateway.Uplink
      - Gateway.Bus
      - LeafA.Bus
      - LeafB.Bus
      - Gateway.Tail
      - LeafC.Tail
Wires:
  - Name: Test
    WireId: 1
    Hosts: [Root, LeafA, LeafB, LeafC]
    Realization: TestTree
```

Attachments name exact `Host.Interface` edges of the physical host/Link graph;
their order is immaterial. At least two distinct valid attachments are required.
Their graph must be connected and acyclic, and every selected Link needs at least
two selected attachments. Every Wire member must occur in the tree. Extra hosts
are propagation participants, not local members. Nonmember leaves are permitted
when explicitly selected; the resolver does not prune authored selections.

Branches can occur at a host (Gateway) or a shared Link (Bus). Other listeners on
Bus are excluded unless listed. The complete runnable example is
codegen/examples/branched.yaml, including an unselected physical bypass. Multiple
Wires may reuse one realization with different membership; reuse never grants
service authority. Branched Paths are not added: Paths remain linear chains.

There are no nested groups, wildcards, generated identities, endpoint declarations,
or authorization policies.

## Inspection and examples

Inspect resolution without generating C++:

    codegen/.venv/bin/python codegen/wiring_codegen.py hardware/bench/topology.yaml --local-host Gateway --explain

The deterministic JSON includes all hosts' egress assignments and memberships,
expanded Wire members, transit hosts, selected interface attachments, and route
masks and ingress indices. Each result links back to named Wire, Group, Path,
Realization, interface, and Link
declarations. Absent host route entries mean no participation; a member with a
zero mask is local-only. --explain cannot be combined with --output.

- codegen/demo.yaml: standalone inferred two-host UART example.
- examples/arduino-uno/demo.yaml: application-owned deployment used by its Makefile.
- hardware/bench/topology.yaml: all seven planned PCB Links, including two shared CAN-FD
  buses, UART multidrop and three ring Links. LongBenchPath deliberately travels
  through Gateway, LeafA, and LeafB to LeafC. Other test Wires exercise the other
  Links using explicit selections. UART rates are example settings, not hardware
  validation; LinkType labels do not implement drivers or arbitration.
- codegen/examples/amr.yaml and codegen/examples/excavator.yaml: durable topology-only
  reference models using HostId, groups and inferred trees.
  Native non-WireSpaces RS-485/LIN peripherals are outside these graphs.
- codegen/examples/branched.yaml: exact host/bus branches with a bypass and bus listener excluded.

## Compiler stages

The frozen records in wiring_models.py separate three representations:

1. AuthoredDeployment: validated declarations, including omitted versus explicitly
   empty fields and optional authored EgressBit choices. No inferred membership,
   attachments or automatically assigned bits are written into these records.
2. ResolvedDeployment: expanded membership, transit hosts and selected physical
   attachments, each referencing its authored Wire declaration. No route masks.
3. TargetProjection: per-host interface bits, membership tables and route masks
   for the current runtime. Constructor binding order is fixed here, not by the emitter.

Structural shapes live in wiring_structure.py; parsing/reference validation in
wiring_schema.py, topology resolution in
wiring_topology.py, target lowering in wiring_projection.py, inspection serialization
in wiring_inspection.py and C++ emission in wiring_codegen.py. compile_deployment()
composes the stages; emit_header() consumes an existing projection. The original
generate_header() convenience entry point and CLI remain available.

Collections are immutable snapshots. Dictionaries are used at YAML/JSON boundaries
and for temporary indexes, not as mixed authored/resolved packet-routing records.
The current eight-interface and six-membership restrictions remain unchanged.
Zero-copy remains outside this iteration.

NetworkX owns bridge discovery, breadth-first traversal, connectivity and tree
checks. WireSpaces retains the shared-bus model, explicit chain rules, legacy Link
selection, unique-path inference policy and error messages. Sorted traversal keeps
ambiguity witnesses deterministic. No shortest-path routing policy is introduced.

Tests retain compiled generated-forwarder checks and pre-refactor C++/inspection
output hashes for every example host plus a legacy explicit-bit deployment.
New CAN timing/format fields and ingress constants/annotations are excluded from those legacy hashes and checked
separately through validation, inspection, and compiled constant assertions.
An exhaustive 512-graph corpus cross-checks inference against enumerated simple
paths. Additional tests cover immutable stage boundaries and process-independent
inspection output. Python unit tests and their fixtures live in codegen/tests.
The compiled branched-network test binds every generated Forwarder to queued
simulated Links and runs the real Router, Dispatcher and EndpointReceiverQueue.
It checks all member-to-member unicast directions and broadcasts, one transmission
per selected Link, no reflection, unselected ingress rejection, transit membership
and exclusion of the extra bus listener. Core tests cover ingress bounds, metadata
copy/lifecycle/serialization, zero-mask compatibility and leaf delivery.

## Generated API

Output contains deployment HostIds and WireNumbers, the selected host's HostInfo,
interface masks, ingress indices and baud rates, routes(), and a Forwarder class. Constructor
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

## Ingress and propagation contract

Each generated `k{Name}IngressIndex` equals its host-local egress bit + 1 (1..8).
PacketBuffer stores the index in one byte of its existing alignment padding;
the prefix remains 12 bytes, payload alignment and canonical wire bytes are unchanged.
Zero means no ingress/local origin and is the default. `copyFrom` (including
EndpointReceiverQueue copying) preserves ingress; `resize` also preserves it.
Successful `initialize` and `initializeResponseTo` clear it. Failed operations
leave the previous packet unchanged. Reusing a received buffer for a new outbound
packet requires initialization or an explicit `setIngressIndex(0)`.

After validating Link framing and canonical packet length, the receiving driver
calls `router.receive(packet, k{Name}IngressIndex, dispatcher)`. A router task may
call the same API after dequeueing. Always stamp the **receiving host's** index,
including in-memory/zero-copy Links; indices have no meaning across hosts and are
never trusted from transmitted bytes. The current UART examples use this boundary.

For an admitted packet, the route's local-origin mask also describes its permitted
ingress attachments. Receive rejects index zero, indices above eight, missing Wire
routes and unselected ingress before any effects. It forwards to `route_mask &
~ingress_mask`, never back to the ingress Link. At a leaf this is zero: no forwarder
call, `kNoEgress`, but local delivery can still succeed. Each selected bus is sent
once, irrespective of how many hosts are attached. Local delivery requires local
Wire membership and the Dispatcher's unicast/broadcast destination checks. Transit
participation alone never causes delivery, even for a packet addressed to that host.

This is static tree flooding for both unicast and broadcast, not destination-pruned
routing. Arrival addressed to a local member does not stop propagation to other
selected branches. A physical bus can still deliver bits to unselected listeners;
their receive boundary drops packets without a selected route. This is routing
configuration, **not authentication or a security boundary**: ingress cannot prove
which physical peer sent a bus frame, and local-origin `forward` trusts its caller.
There is no duplicate cache or TTL; all hosts must use a consistent acyclic deployment.

`Router::forward` accepts zero-tagged local origin and validates/excludes nonzero
tags, but does not itself dispatch locally. For compatibility, a local-origin zero
route mask is still offered to a custom forwarder (e.g. LocalDispatchForwarder);
the generated Forwarder emits nothing for it. Direct Dispatcher calls retain their
existing semantics; external Link drivers should use `Router::receive` to enforce
ingress and membership. Routing and endpoint delivery report independent results;
Link forwarders currently return void, so `kForwarded` means offered, not guaranteed
physical delivery. Queue-full endpoint results do not undo propagation.

Router/Dispatcher borrow packet storage synchronously. Outbound Links and endpoint
receivers must copy or acquire ownership before returning; service work may poll
its queue later. Driver-context or router-task execution is chosen by the integrator,
as is required synchronization. This iteration does not add threads, memory pools
or zero-copy ownership. Multi-node hardware validation awaits the bench/Link drivers;
host runtime tests do not claim hardware timing or arbitration coverage.

## Read-only viewer

Generate a self-contained HTML file from the same parser and resolver:

    make -C codegen viewer
    make -C codegen viewer INPUT=examples/studies/multicore_gateway.yaml OUTPUT=build/multicore.html

Equivalent CLI from the repo root:

    codegen/.venv/bin/python codegen/wiring_viewer.py codegen/examples/packaging_line.yaml -o /tmp/line.html

Open the generated file in a browser. It needs no server, CDN, network access,
JavaScript packages, or additional Python dependencies. Nothing is sent from the
viewer. There are no editing, save, transmission or runtime-control actions.
YAML remains the source; regenerate the HTML after editing it externally.

The physical graph has explicit shared-Link nodes and interface edges. Choose a
Wire to highlight its exact selected attachments, then optionally hide everything
outside its tree. Host styling distinguishes members, transit participants and
excluded hosts. Select a host or Link to inspect identities, timing, selected
attachments, generated masks, egress bits/ingress indices and authored provenance.
Use the host and arrival selectors for an attachment-level forwarding preview;
unselected ingress shows rejection. Pan/zoom and Fit change only presentation.
Large overviews may need zoom or a Wire filter to read every label; the host picker
and inspector remain available independently of graph position.

Resolution failures retain the physical graph but do not invent a tree. If a
resolved deployment exceeds the target's membership limit, the viewer displays
the diagnostic and exact attachments with **no generated masks or indices**.
This is not a bypass for firmware generation. Structurally invalid YAML and broken
cross-references still fail the export. The exporter also rejects output paths
aliasing its input, including symlinks and hardlinks.

`make -C codegen test` covers rendering, escaping, immutability, diagnostics and
compiled runtime probes. With Node.js available, it also exercises the viewer's
pure layout/selection functions; set `NODE=/path/to/node` if it is not on PATH.
No DOM/browser automation dependency is installed by these tests.

Editing is deliberately deferred. A later editor should manipulate authored
declarations, preserve YAML comments/order/shorthand, show compiler diagnostics
before writes and offer a reviewed diff/undo. Derived routes must never become
the editable source. Device/domain grouping and read-only observation semantics
need explicit decisions first; see the deployment studies.

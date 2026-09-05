# Deployment studies and verification — 2026-09-05

These are compiler/runtime exercises, not safety designs or measurements of
industrial hardware. The durable YAML examples do not depend on retaining the
sketch documents. Interface and membership limits remain unchanged.

## Multicore AMP gateway

Source: sketches_b/05_multicore_gateway.md. The study retains three gateway domains,
all three shared-memory Links, the four heterogeneous fieldbuses, upstream plant
and workstation, and all seven Wire relationships. Two devices each represent the
drive, safety and sensor populations; the vision subsystem is one host. Each
domain has a distinct HostId. CAN/RS485 timings are illustrative.

`examples/studies/multicore_gateway.yaml` passes strict authoring/reference checks
and topology resolution. It intentionally does **not** pass target projection:
Control has four operational memberships plus plant, maintenance and internal
supervision: seven. FieldIo has six interfaces, within the eight-interface limit.
Host has six memberships and a seventh transit route. No relationship is removed
to disguise the Control limit, and no generated HostInfo array is truncated.

| Decision exercised | Result |
| --- | --- |
| Three SHM Links form a physical triangle | Inference rejects the ambiguity. Each explicit realization selects an acyclic subset. Adding the third SHM side to DriveTree is rejected as a cycle. |
| Exact attachment selection on upstream Ethernet | Maintenance excludes Plant.Upstream; plant supervision excludes Workstation.Upstream. Physical listeners do not acquire participation. |
| Internal supervision remains internal | Its only Links are ShmControlIo and ShmHostControl. No fieldbus/upstream attachment is selected. |
| Driver ownership versus membership | FieldIo forwards the operational Wires without local delivery; owning a fieldbus does not grant membership. |
| Host observation tap | **Not implemented by membership.** The study includes Host as a member to expose the mismatch. It receives broadcasts, but an ordinary unicast addressed to Control is not delivered to Host's endpoint queue. |
| Observation-only transmit authority | **Not enforced.** A Host-origin packet can be forwarded on an operational Wire. No source or endpoint authorization policy is generated. |
| Directional supervision/maintenance projections | Current forwarding floods the exact tree regardless of destination. A status from FieldIo at Control is also offered to ShmHostControl; it does not terminate just because Control is addressed. No reflection occurs on the incoming Link. |
| Host-local ingress metadata across SHM | Queued C++ probes deliberately carry an invalid previous-host tag; the receiving adapter restamps its local index before routing. No canonical header/payload byte changes. |

The runtime probes generate each of the seven Wires separately so the current
target can compile them. They use real generated Forwarders, Router, Dispatcher
and EndpointReceiverQueue, and test broadcast and directed delivery from every
member. These slices are **not** a workaround for deploying the combined seven-Wire
Control configuration. They expose the current flooding/membership behavior, not
the sketch's unimplemented authority or observation policies.

### Concurrency and ownership boundary

`localHostInfo()` is currently process/image-global. Three independently linked
AMP images can each have their own instance. Three concurrently active domains
inside one image cannot safely use different identities by switching this global;
Router and Dispatcher would need explicit domain context. The test harness
switches identity only between sequential queued events, simulating isolated
images; it does not demonstrate concurrent multicore operation.

SHM is currently a LinkType label, not a driver. Release/acquire barriers, cache
maintenance, queue-full policy, allocator/lease ownership, restart generation,
failure dependencies and nonblocking observation fan-out remain integration work.
The core callback is synchronous and returns no per-Link acceptance outcome: a
blocking/full observation adapter could delay other egresses. The topology cannot
prove independent progress or isolation. No zero-copy or cross-core hardware test
was performed.

## Four-cell packaging line

`examples/packaging_line.yaml` models infeed, processing, packaging and palletizing
cells with three motion axes, two I/O nodes, vision and two measurement nodes per
cell, plus plant, line coordinator, historian and engineering workstation.
Ethernet is modeled at shared-Link granularity, not as switches/VLANs. RS485 nodes
are assumed WireSpaces-aware; native Modbus translation is not implied.

| Measure | Resolved result |
| --- | --- |
| Hosts / physical Links / Wires | 40 / 17 / 19 |
| Maximum interfaces per host | 5 of 8 |
| Maximum local memberships | 6 of 6, at each cell controller |
| Controller route entries | 7: six memberships plus telemetry transit |
| Maintenance | 38 members; 17 selected Links; Plant and Historian excluded |
| LineTelemetry | 10 members, four transit controllers, five selected Links, 18 attachments |

Every Wire infers uniquely. Groups remove repeated device lists while membership
and transit remain distinct. Explicit LineTelemetry publications are assumed; this
is a declared publication relationship, not a passive tap on unicast control.

The compiled test instantiates all 40 generated host bindings. From every member
on every Wire it runs a broadcast and a directed probe. It checks exactly one
transmission per selected Link, no duplicate endpoint receipt, destination filtering,
nonmember exclusion, restamped ingress, unchanged bytes and queue-copy lifetime.

The important cost is now concrete: **one Maintenance unicast generates 17 physical
Link transmissions**, even when the addressed device is only two Links from the
workstation. It is delivered to only one endpoint host. This is correct under the
current static flooding contract, but meaningful for image transfer and bandwidth
planning. No throughput, latency, QoS capacity or failover guarantee follows from
the acyclic graph. Destination pruning merits a separate iteration if those costs
are unacceptable; do not silently substitute shortest-path policy in inference.

## Physical hardware verification

Tested the attached Uno (`/dev/arduino-uno`, ATmega328P) and CANtact (`can0`, slcand
configured for 500 kbit/s), sequentially flashing the repository's test images.

- Ping request/response and heartbeat: passed after fixing the heartbeat service's
  initialization. It had created a response from its own empty header, ignoring
  configured Wire/source/destination. A host regression reproduced the failure
  before the fix; the board now emits Wire 1, Host 1 → broadcast, endpoint 0xFFFE.
- UART ingress: passed unknown-Wire and wrong-destination rejection, broadcast-ping
  rejection, no received-frame reflection and subsequent valid ping recovery.
  Positive probes use bounded retries, like the existing ping test, for this
  best-effort polling UART; this is not a lossless/latency guarantee.
- BITS RAM round trips: 1, 128 and 256 bytes passed, with matching object hashes.
- Constrained BITS receiver: user datagrams, 256-byte image and 251-byte partial-final
  image passed. This image deliberately bypasses the queued Router API.
- MCP2515/CANtact: three Classical CAN frames in each direction at 500 kbit/s passed.
  The first immediate post-flash test consumed stale READY output before reset had
  completed. Draining input before the new DTR reset fixed the harness; two immediate
  post-flash repetitions passed. A unit test locks down the reset/drain ordering.
- The Arduino was restored to the corrected demo and ping/heartbeat/ingress were
  verified again. No fuses, EEPROM or bootloader settings were changed.

Final AVR flash/data bytes: demo 4078/208; BITS round-trip 10536/1156; raw CAN
1554/97; constrained test 3044/30; size-only profile 2672/30. Both boot-profile
flash gates (4096 and 3350) pass. These tests do not cover a physical multi-hop PCB,
CAN-FD, Ethernet, RS485 arbitration, or multicore SHM coherency/restart.

## Viewer and next decisions

The viewer consumes the compiler's authored/resolved/target results; it does not
reimplement YAML resolution in JavaScript. It remains useful for the multicore
study even when target emission is blocked, marking unavailable masks/indices.
Desktop browser checks cover Wire filtering, selected-tree view, ingress rejection
and egress exclusion, host/Link inspection and authored provenance.

Before an editor, resolve the observation-versus-membership contract and how domain
context is represented for multi-domain images. Later editing should operate on
the authored model, preserve comments/shorthand, offer validation plus diff/undo,
and keep device/layout metadata separate from runtime routing. No editing writes
or hosted service are introduced here.

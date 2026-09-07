# WireSpaces — Implementation Notes

**Status:** Private first draft; project engineering direction, not protocol semantics<br>
**Purpose:** Language choices, scaling profiles, execution shape, and evidence plan for the reference implementation<br>
**Authority:** Implementation guidance only. Protocol behavior belongs to `CORE`

Cross-references use the document code plus a section number, for example `IMPL §2`. A bare `§x` always means the current document.

---

# 1. Implementation Language and API Direction

The first implementation should favor **C++17** to obtain working results quickly. There is no current need to maintain parallel C and C++ cores from day one.

A good hedge is to keep important core data shapes and functions reasonably C-compatible where practical:

```cpp
namespace wirespaces {

struct Pdu;
class Router;
enum class SendResult : uint8_t;

SendResult route(...);

}  // namespace wirespaces
```

while still using C++ internally for RAII, templates where they materially help static sizing, `constexpr` configuration, stronger types, compile-time validation, and host-side convenience.

A separate C implementation or C ABI can be added later when an actual target requires it.

The embedded baseline has no dynamic allocation after one-time initialization, no exceptions, and no RTTI. Target code must also avoid standard-library facilities that depend on them. Host-only tests and measurement tools may use the full host environment.

Advanced table synchronization arrives only after a measured runtime-reconfiguration need. Immutable or read-mostly generated tables are the baseline.

---

# 2. Implementation Scaling Profiles

The same canonical Host/Wire/Endpoint architecture should scale through substantially different implementations:

- **Tiny bare-metal MCU:** one local Host, one Link, canonical `kLocalBus` or one named Wire, direct host representation where possible, copy storage, linear/switch Endpoint dispatch, and no locks.
- **Normal single-core MCU:** one or several locally hosted Hosts, fixed Endpoint and Wire-mask tables, copy queues, critical-section producer exclusion where required, and several Services.
- **Multicore MCU:** several internal Endpoint Domains with distinct Host IDs, shared-memory Links, per-Endpoint storage, generated Wire membership, and explicitly measured concurrent routing/dispatch.
- **Embedded gateway:** many Link tasks, immutable/read-mostly `Wire -> LinkMask` or `(Wire, ingress) -> EgressMask` tables, direct forwarding, profile-specific VCN/projection state below the canonical Router, and optional zero-copy only where measurements justify it.
- **Host PC:** conventional threads and queues with the same canonical semantics; maps are acceptable, but optimization waits for evidence.
- **FPGA softcore:** generated tables, DMA/FIFOs, bounded VCN/projection lookup, and potentially zero-copy payload movement.
- **Pure RTL:** BRAM Wire-mask tables, profile-specific ingress canonicalizers and egress encoders, RTL Endpoints, early destination comparators, and a streaming datapath.

`kLocalBus` is not an anonymous bypass around canonical processing. It is the reserved canonical local-only Wire used for simple one-Link bring-up: ingress still reconstructs canonical source and destination, local dispatch still applies destination selection, and forwarding emits no remote egress for that Wire.

The protocol does not require every profile above to implement the mechanisms used by every other profile. This is a ladder of target hardware, orthogonal to the ladder of user commitment in `INTRO §6`. The authority-preserving small-MCU collapse rules are in `CORE §25`.

---

# 3. "Microkernel-like" Execution Shape

WireSpaces is not an operating system, but a capable implementation has a similar execution shape:

```text
Active entities:         Service tasks, Link-driver tasks
Passive infrastructure: Wire masks, dispatch tables, static bindings, bounded queues
```

Data moves directly from producer context toward its configured destination. Queues exist where a scheduling or ownership boundary actually requires them, not because the framework mandates a central broker. The core library creates no tasks and owns no loop; `LIB §11` defines the corresponding seams.

A typical ingress execution is:

```text
Link driver receives carrier unit
    -> profile LLL validates and reconstructs canonical
       {Wire, SrcHost, DestHost, Endpoint}
    -> optional semantically equivalent early destination filter
    -> Router selects egress Link mask from Wire plus ingress
    -> ForwardingEngine preserves canonical values on every egress
    -> Dispatcher selects directed local Host, or fans broadcast out
       to all matching local Host/Endpoint bindings
```

A typical egress execution is:

```text
Service submits canonical PDU
    -> Wire-mask forwarding selects each egress Link
    -> each egress LLL independently checks its profile binding
    -> representable values are encoded
    -> unrepresentable Wire/Host/profile combinations are rejected
```

The generic Router never examines VCNs, host projection, or profile-local CAN codes. Those exist entirely inside the ingress canonicalizer, egress encoder, and their static profile configuration.

Execution measurements must cover both superloop orderings identified by `LIB §11`: service Links before draining Endpoints, and drain Endpoints before servicing Links. For RTOS targets, measure Link-task/Service-task handoff and prove the configured exclusion rules rather than assuming the host threading model carries over.

---

# 4. Forwarding and Profile Storage Costs

Measure storage from emitted binaries and map files, not only from ideal field-width arithmetic. Alignment, indices, sentinels, vtables, and generated lookup acceleration can dominate small tables.

For forwarding, record at least:

```text
dense Wire -> mask:
    ideal mask bytes = WireCount * ceil(LinkCount / 8)

explicit (Wire, ingress) -> mask:
    ideal mask bytes = WireCount * IngressCount * ceil(LinkCount / 8)
```

Report actual ROM and RAM separately, including route keys, local-delivery/splice actions, alignment, and any startup copy from ROM to RAM. Compare linear, binary-search, dense-index, and generated-switch forms where the target size makes more than one credible.

CAN11 profile accounting must distinguish:

- Guest4 VCN, including the allocated block, fixed QoS, and deployment-wide relationship definition;
- prospective Guest5/Guest6 growth profiles, preserving existing default-map meanings;
- Native VCN with the default map and explicit Host role bindings;
- Native VCN with custom maps, multiple Wire aliases, deterministic TX selection, and migration headroom.

For each, report profile codec flash, constant data/ROM, mutable RAM, worst-case stack, lookup time, configured entry capacity, and unused-capacity cost. Measure VCN cost against communication-graph edges and alias cost against active Wire/map bindings, including spare aliases for migration. Compare candidate representations behind the Link-profile seam; table ABI, fingerprints, and the complete migration protocol remain open (`LINK §2.14`).

Compact/General Host compression may be measured as an experimental alternative. Reintroducing it into the baseline requires a demonstrated material resource or complexity advantage over unified VCN on representative deployments.

---

# 5. Required Build and Runtime Measurements

Maintain reproducible build profiles that differ by one feature at a time. At minimum measure:

- canonical descriptor codec only;
- Guest4 VCN ingress/egress, with Guest5/Guest6 candidate growth comparisons;
- Native VCN using default and custom maps;
- one versus several Native aliases, including overlapping receive bindings and unique TX selection;
- forwarding disabled, dense Wire-mask forwarding, and ingress-specific forwarding;
- one local Host versus several local Hosts with broadcast fanout;
- software destination filtering versus available hardware filtering.

For every profile, capture:

```text
flash / text
read-only profile data
static and retained RAM
maximum measured stack
cycles or time per accepted, forwarded, rejected, and broadcast PDU
copies and bytes copied
hardware acceptance filters consumed
false-positive frames admitted by coarse hardware filters
```

CAN filter measurements are profile-specific. Guest VCN should record filters needed for its allocated contiguous block. Native VCN should record whether QoS/alias/VCN layouts can be covered by available masks without admitting excessive unrelated traffic. A filter count unsupported by the selected controller is a deployment cost, not a reason to change canonical semantics.

Early destination filtering is an optimization, not destination routing. Compare no early filter, software filter immediately after profile decode, and hardware/RTL filtering where available. Verify identical accepted traffic and forwarding behavior, then report CPU time, interrupt rate, queue pressure, and any extra filter resources. Directed traffic not accepted locally may still require transparent forwarding on other Wire links.

---

# 6. Topology Corpus and Provisional Widths

Before freezing `HostId`, provisional `WireNumber`, or the current preferred six-byte descriptor layout, run a representative topology corpus containing:

- multicore/internal Endpoint Domains;
- redundant controllers;
- gateways and several CAN buses;
- overlapping broad/narrow Wires on profiles that can represent them;
- device-private, debug/platform, and local-only Wires;
- sentinel/reserved allocations;
- plausible product variants and growth.

For every case record peak Host IDs, peak Wire numbers, reserved/private allocation cost, maximum Links per Wire, local Host fanout, CAN11 VCN edge count per alias, active and spare alias counts, Guest growth needs, and remaining margin. Include cases that do not fit CAN11 cleanly; the corpus is meant to expose crossovers, not prove the preferred widths by construction.

The 8-bit identity choices and six-byte layout are current preferred provisional implementation inputs. Tests may pin current encoding for regression, but documentation, APIs, and generated artifacts must not describe them as interoperability-frozen until corpus and profile evidence is reviewed.

---

# 7. CAN29 Crossover Evidence

CAN29 is the expected richer CAN option, but its final representation is not defined here. Build or model enough of a candidate to identify when CAN11 complexity stops paying.

Record the crossover using:

- number of canonical Wires required on one physical CAN bus;
- number and shape of host communication edges;
- VCN-map, alias-binding, and TX-selection ROM/RAM;
- spare-alias capacity and migration state;
- profile codec flash and execution time;
- hardware filter consumption and false positives;
- CAN payload bytes lost to profile metadata and resulting fragmentation;
- configuration entries and generated-data size;
- deployment cases rejected as unrepresentable.

Evidence should compare Guest VCN, default-map and custom-map Native VCN, and a candidate CAN29 representation on the same topology/traffic corpus. Native CAN11 already permits several Wires on one bus. Prefer CAN29 when alias or per-map VCN limits, migration headroom, configuration cost, filter pressure, or payload efficiency justify the richer identifier. Compact/General comparisons remain optional experiments. These are crossover criteria, not a normative CAN29 bit allocation.

---

# 8. Evidence Record

`BITS-TRANSPORT §15` records the finite-object Transport prototype scope and unresolved protocol decisions. Detailed BITS implementation/API sequencing remains a follow-on; the measurements here do not imply a completed BITS implementation.

Keep measurement inputs, compiler/linker flags, target/controller identity, generated configuration counts, and raw map/timing outputs beside each reported result. Report medians and worst observed values where timing varies. A number without its topology and enabled profile is not reusable evidence.

Implementation guidance may change when this evidence changes. Canonical protocol behavior does not change merely because one target's fastest table or filter arrangement is different.

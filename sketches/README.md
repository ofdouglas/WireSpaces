# WireSpaces Architecture Sketches — Approach

**Status:** Working notes; sketches are hypothetical systems, not deployment specs  
**Purpose:** Test whether the WireSpaces core design (Wires, Origin vs Node, Endpoint semantics, gateway forwarding, splices) is useful across a variety of common embedded system archetypes — not all of them, and not by proving formal correctness.

Sketches assume **stable, feature-complete WireSpaces releases** with the transport and service ecosystem described in [Assumptions](#assumptions) below. They are written against the architecture documents (`INTRO`, `CORE`, `DEPLOY`, `LIB`, `FUTURE`), not against the current prototype in `sim/`.

---

## Core invariant

> **Model the system as it naturally exists first. Apply WireSpaces with the minimum machinery necessary. Any machinery needed primarily to make the model fit is evidence — record it, do not hide it.**

The purpose of a sketch is **not** to demonstrate that WireSpaces can represent the system. Nearly any sufficiently flexible model can do that. The useful question is whether the representation is **simpler, clearer, and reasonably faithful** to the system's natural communication structure.

Do not repair an awkward mapping by introducing additional Wires, forwarding, splices, Services, or configuration unless the real system provides a corresponding architectural distinction. **Record the awkwardness instead.**

The experiment aims to **distinguish natural structure from WS-imposed structure**, not to declare victory or defeat prematurely.

---

## Phased workflow

| Phase | When | What |
|---|---|---|
| **1 — Sketch** | Now | Natural model first; minimum WS machinery; friction flags; model-pressure notes |
| **2 — Refine** | After 2–3 sketches exist | Tune format from evidence; run independent agents on the same neutral description; compare Wire decompositions |
| **3 — Review** | Later | Aggregate verdicts; status-quo essays; architecture implications |

Phase 1 does **not** require numeric scores, winner/loser verdicts, or full conventional-architecture comparisons.

---

## Goals

Sketches favor **fewer system types with multiple configurations** where topology changes teach something:

- Does the same multicore gateway design still work when more fieldbuses are added?
- What happens when a debug PC attaches — how many Wires, how much forwarding?
- Does intermittent WiFi change the maintenance story without breaking local logging?
- Where does single-Origin-per-Wire feel natural vs imposed?

---

## Assumptions

These sketches are **not** constrained to the near-term implementation roadmap in `INTRO §9`. They assume WireSpaces has reached a useful, stable baseline:

| Capability | Sketch assumption |
|---|---|
| **Core model** | Wires, Origin/Node, Endpoint Domains, Dispatcher, Router, gateway forwarding, splices, device-private Wires, `kLocalBus` bring-up |
| **Transports** | Unreliable datagram (default); **reliable segment transport** for file/image-style transfers (firmware update, bulk objects); **basic end-to-end transport** with CRC, sequence number, and freshness metadata where Services need it |
| **Service libraries** | A usable catalog exists: identity, health/heartbeat, logs/events, link telemetry, firmware update, and domain-appropriate telemetry/control helpers (`FUTURE §8.1`) |
| **Host tooling** | Organizer, discovery, auto-Wiring, export to static configuration, maintenance-port CLI (`DEPLOY`) |
| **Commissioning** | CAN pre-addressing exists in the product, but sketches **omit the narrative for now** (TODO) |

Individual sketches still choose a **maturity level** (Level 0–3, `INTRO §6`) appropriate to the project; err on the **lower** side when in doubt.

---

## Sketch catalog

Archetypes are chosen to span the design space — **not as predicted verdicts**, but as intentional stress coverage:

| Expected stress | Sketches |
|---|---|
| **Strong expected fit** | RS-485 polled sensors |
| **Likely fit** | Dev board, Pi robot |
| **Origin stress** | Peer CAN ECUs |
| **Complexity stress** | Multicore gateway, redundant gateway |

| File | System | Configurations | Primary stress |
|---|---|---|---|
| [`01_dev_board.md`](01_dev_board.md) | PC ↔ MCU dev board; MCU as gateway to smaller MCUs | **A:** single MCU (replaces `basic.md`); **B:** MCU gateways child nodes | Level 0–1 bring-up; first gateway; Origin placement |
| [`02_peer_can.md`](02_peer_can.md) | 3–5 peer MCUs on CAN: each publishes state; each may command peers; no obvious coordinator | Single configuration (optional variants later) | Artificial Origin; Wire proliferation; NodeId vs peer identity |
| [`03_rs485_sensors.md`](03_rs485_sensors.md) | RS-485 industrial sensor chain | **A:** single segment; optional **B:** repeater/segmented bus | Master-initiated / polled Link; positive control for Origin/Node |
| [`04_pi_robot.md`](04_pi_robot.md) | Small robot: Raspberry Pi + simple ECUs; PC via WiFi part-time | **A:** bench (PC ↔ Pi direct); **B:** field (WiFi telemetry); **C:** ECUs standalone (Pi absent) | Linux gateway; intermittent maintenance Link; telemetry logging on Pi always |
| [`05_multicore_gateway.md`](05_multicore_gateway.md) | Multicore AMP MCU gateway: Ethernet + named heterogeneous fieldbuses | Single configuration | Shared-memory Domains; many Link Interfaces; ASIL-A-ish timing notes |
| [`06_redundant_gateway.md`](06_redundant_gateway.md) | Rework of 5: dual redundant multicore gateways, 2+2 bus split | Side-by-side with 5 where useful | Fault isolation; cross-partner forwarding; explicit redundancy/taps; debug PC via switch |

**Write order:** this README → `01_dev_board.md` → `02_peer_can.md` → `03_rs485_sensors.md` → `04_pi_robot.md` → `05_multicore_gateway.md` → `06_redundant_gateway.md`.

`basic.md` was superseded by Config A of `01_dev_board.md` and removed.

**Phase 2 check:** After `01` and `02` exist, have **independent agents** sketch the same system from the same neutral description (peer CAN is the first candidate). Divergent Wire decompositions suggest underspecified guidance; convergence without coordination is a strong positive signal.

---

## Per-sketch structure

Every sketch file uses the same section skeleton **per configuration** (A, B, …). Omit sections only when they truly do not apply.

### 1. Title and intent

One-line description of the physical system and what WireSpaces claim is being exercised.

### 2. Configurations

For each configuration (**A**, **B**, …):

- **What changed** — 2–4 bullets vs the previous config or vs a named baseline sketch.
- **Maturity level** — Level 0–3 with one sentence of justification.

### 3. Native communication model

Describe communication relationships **without WireSpaces terminology**:

```text
MCU A periodically publishes sensor state.
MCUs B and C may independently command actuator D.
Gateway G forwards diagnostics to a PC.
...
```

This section comes **before** any WS mapping. It is the anchor for detecting whether a Wire corresponds to something real.

### 4. Obvious conventional implementation

One sentence per configuration:

> **Obvious conventional implementation:** CAN IDs directly, Modbus master/slaves, UDP sockets, shared-memory queues, etc.

Then briefly:

> **What additional conceptual objects does WS introduce compared with this?**

No benchmarking or winner declaration — just a count of extra structure (Wires, Origins, forwarding hops, configuration artifacts).

### 5. WireSpaces mapping

#### Topology diagrams

Consider **application** and **maintenance** as separate views when the system has distinct debug/bring-up paths or when splitting clarifies the design. **Do not force two planes** when a single link carries everything and splitting would make the system look more complex than it is — that complexity is itself a result worth recording.

Diagram options (pick what fits; combine as needed):

| Style | Best for | Notes |
|---|---|---|
| **Device-centric** (default) | Gateways, multicore, Pi | Each device: Endpoint Domain(s) → Link Interfaces → Physical Links. Label **Wire names** on logical buses, not only cable types. |
| **Wire-centric** | RS-485, peer CAN, heavy fieldbus layouts | One Origin at top, Nodes below; physical carrier noted beside the Wire. |
| **Traffic-plane split** | Complex gateways | Separate application vs maintenance diagrams when warranted. |
| **Sequence (sparingly)** | Non-obvious flows | Mermaid sequence for multi-hop firmware update, splice path, cross-partner status — at most 2–3 per sketch. |

Avoid the `basic.md` pattern of mixing application code, WS framework, and undecided ("TBD") bindings in one chart.

#### Tables

Use consistent column sets so sketches are comparable. **Keep tables slim in Phase 1** — invent Endpoint IDs and storage semantics only where they materially affect topology.

**Devices**

| Device | Endpoint Domains | Link interfaces | Role summary |
|---|---|---|---|

**Wires — application**

| Wire (name / #) | Origin | Nodes | Physical realization | Notes |
|---|---|---|---|---|

**Wires — maintenance / debug** (when applicable)

Same columns. Include only when maintenance topology is non-trivial or worth separating from application traffic.

**Interactions** (primary table)

| Interaction | Producer | Consumer(s) | Wire | Direction | Notes |
|---|---|---|---|---|---|

Add transport (datagram / e2e / reliable segment), forwarding, or splice notes in **Notes** only when they matter for the interaction.

**Services / Endpoints** (optional — only when topology-relevant)

| Service | Endpoint (NS/EID) | Wire | Direction | Rx / Tx storage | Binding mode | Notes |
|---|---|---|---|---|---|---|

Use when Queue vs Snapshot or binding mode affects whether the mapping is natural (e.g. polled Snapshot on RS-485, request-scoped reply). Skip invented EIDs otherwise.

**Gateway forwarding** (when applicable)

| Ingress link | Wire | Egress link(s) | Local delivery? | Splice? | Notes |
|---|---|---|---|---|---|

### 6. Friction signals

For each signal below, rate **None / Mild / Significant** and add **one sentence** of justification. No numeric score.

| Signal | Question |
|---|---|
| **Artificial Origin** | Is an Origin chosen solely because WireSpaces requires one? |
| **Artificial Wire** | Was a Wire created that does not correspond to a meaningful communication relationship? |
| **Wire proliferation** | Does one subsystem become many Wires mainly to satisfy direction or authority semantics? |
| **Forwarding tax** | Are gateways doing substantial forwarding the natural model would not need? |
| **Identity awkwardness** | Is NodeId meaningful here, or are participants really peers? |
| **Interaction awkwardness** | Does a normal request, event, or pub/sub pattern require special cases? |
| **Configuration burden** | Does the WS representation need substantially more topology configuration than the underlying system? |
| **Role instability** | Do Origin/Node roles change during normal operation? |
| **Failure mismatch** | Does the Wire abstraction make failure containment or availability harder to describe than the physical system? |

### 7. Model pressure

Two short prompts:

- **If you could change one WireSpaces concept** to make this system simpler, what would you change?
- **Did WS expose a useful distinction** that the conventional model tends to obscure?

Record simplifications as well as problems — e.g. "these four Wires always travel together," or "Origin has no semantic effect in these cases," or "splitting control from debug unexpectedly clarified ownership."

### 8. Open questions

Per-sketch follow-ups not captured above. Do not resolve architecture decisions here.

---

## Cross-cutting conventions

### Origin placement (default patterns, not rules)

When mapping to WS, these are the **usual** placements — deviate when the natural model suggests otherwise, and record why:

- **Maintenance / debug path:** PC or host dev tools are typically **Origin**; the device is a **Node**.
- **Field bus:** the gateway MCU or bus master is typically **Origin**; ECUs and sensors are **Nodes**.
- A device may be Origin on one Wire and Node on another (`CORE §3.2`).

Peer systems (`02_peer_can`) are where this default is most likely to break down; do not force a coordinator merely to satisfy Origin semantics.

### Device-private Wires and splices (default WS pattern, not mandatory)

**Consider** per-device Internal Debug Wire, splice to a host-facing maintenance Wire, and Link Telemetry Service (`CORE §7`, `DEPLOY §3.2`, `§3.3`) when the system has real maintenance/debug traffic.

If that pattern adds topology or configuration without a clear benefit in the sketch, **record that instead of forcing it into the design.** A dev board with one USB link carrying everything may legitimately use one Wire or anonymous `kLocalBus` for bring-up.

### Maturity level guidance

| Sketch / config | Typical level |
|---|---|
| 01 Config A | 0–1 |
| 01 Config B | 1–2 |
| 02 | 1–2 |
| 03 | 2 |
| 04 Config A (bench) | 1–2 |
| 04 Config B/C | 1–2 |
| 05 | 2–3 |
| 06 | 3 |

### Raspberry Pi robot (sketch 04) — fixed choices for now

- **One Linux Endpoint Domain** on the Pi for now (multiple domains → TODO).
- **WiFi** carries telemetry to the PC when up; **the Pi always logs telemetry locally** because WiFi is intermittent.
- **PC remote path** is always **PC → Pi → ECU** (no direct PC↔ECU Link).

### Multicore gateway (sketch 05) — fixed choices for now

- **Four named fieldbuses** — not all the same Link type (e.g. mix CAN, RS-485, Ethernet to subsystems).
- Note **ASIL-A-ish** timing/freshness in interactions; no formal safety case or WireContracts in sketches.

### Redundant gateway (sketch 06) — fixed choices for now

- Each redundant MCU has **exclusive access to two of four** fieldbuses (fault isolation).
- Partners **gateway fieldbus traffic for each other** where cross-visibility is required — model in the **forwarding table**, not as two Origins on one Wire.
- **Debug / service PC** attaches via a **small Ethernet switch**; explore attachment from the **system builder's** perspective.
- **Redundant delivery, observation taps, and partner status** must be **explicitly modeled** (`CORE §12.6`, `§23.11`). Do not imply redundancy from duplicate cabling alone.

---

## Deferred items

### Phase 2 — after initial sketches

| Item | Notes |
|---|---|
| **Format refinement** | Adjust sections/tables based on what first sketches actually need |
| **Independent agent comparison** | Same neutral description, compare Wire decompositions (`02_peer_can` first) |
| **Endpoint/detail tables** | Expand Services/Endpoints table where sketches show it adds value |

### Phase 3 — later review

| Item | Notes |
|---|---|
| **Aggregate verdict** | When to call WS a good or poor fit for a class of systems |
| **Status quo comparison** | Full essays vs raw CAN, Modbus, custom UART, etc. |
| **Negative archetypes** | Systems where WS is intentionally a poor fit |
| **Origin model vs peer-equal IP** | Synthesize findings from `02_peer_can` and gateway sketches |

### Out of scope for now (TODO in sketches or here)

| Item | Notes |
|---|---|
| **CAN commissioning narrative** | Unconfigured → Selected → Staged → Committed (`DEPLOY §1.2`) |
| **Guest CAN / legacy coexistence** | `FUTURE §11.2` |
| **Learned-from-ingress on shared buses** | `CORE §10.6` |
| **I2C/SPI polled archetype** | `CORE §1.7` |
| **Formal redundancy model** | Beyond explicit composition notes in sketch 06 |
| **Multiple Endpoint Domains on Linux (Pi)** | Process/partition split on sketch 04 |

---

## References

| Doc | Relevance |
|---|---|
| `docs/introduction.md` | Maturity ladder, non-goals, characteristic messages |
| `docs/core_architecture.md` | Wires, Origin/Node, Endpoints, forwarding, splices, QoS, redundancy boundaries |
| `docs/deployment.md` | Organizer, Internal Debug Wire, link telemetry, tooling |
| `docs/future_work.md` §8 | Service archetypes and Level-0 catalog |
| `docs/library_architecture.md` | Endpoint API shape (Queue/Snapshot, transmit bindings) |

Cross-references in sketch text use document codes and section numbers, e.g. `CORE §3.1`, `DEPLOY §3.2`.

---

## Guidance from sketch reviews

Assumes you have read the basic architecture docs (`INTRO`, `CORE`, `DEPLOY`, `LINK`). These are non-obvious lessons from drafting and reviewing sketches — not repeated protocol rules.

### Wires are not Physical Links

A common first mistake is assigning **one Wire per cable** (USB Wire + CAN Wire) and having the gateway **translate between them**. That recreates a USB↔CAN tunnel protocol with WS names on the segments.

When a gateway joins Links, prefer **authority-shaped Wires** that **span** the Links involved (`CORE §3`). Gateway forwarding carries the **same canonical Wire** across Link Interfaces — it does not hop between semantic networks. A second Wire usually exists because there is a **second Origin** (e.g. plant control vs PC bench), not because there is a second cable. Keep a physical-link-shaped decomposition as an **alternative** when comparing, not as the primary mapping.

### Compare conventional baselines fairly

Do not compare WS explicit Wiring against a stripped-down conventional design ("fixed CAN IDs only"). A `PC -- USB -- gateway -- CAN -- nodes` system **already** needs USB framing, request routing, CAN node/message IDs, opcode-to-CAN mapping, and response routing — implicit in gateway firmware, but real. Count that when assessing configuration burden and forwarding tax.

Note which physical topology the sketch actually uses. `PC -- PCAN -- CAN` is a different conventional baseline than `PC -- USB -- gateway -- CAN`.

### Friction ratings mean observed clunkiness

Rate **None / Mild / Significant** for friction signals based on whether the **chosen happy-path mapping** feels awkward in practice — not whether an edge-case policy could be documented, and not merely because a design choice existed. Reserve **Significant** for topology where WS structure stays burdensome even with Organizer auto-Wiring (asymmetric memberships, wire proliferation, configs that are hard to audit after generation). A richer model with small symmetric Wires over few devices is usually **Mild**, not **Significant**.

### Overlapping membership is often a feature

One participant may be **Origin on one Wire and Node on another** over the same physical topology, with per-Wire NodeIds (`CORE §3`). Example: PC is Origin on a bench Wire and Node on a plant Wire. Autonomous telemetry on the plant Wire can be published once and **forwarded** to PC on that same Wire — no duplicate bus transmission, no cross-Wire re-publish. Awkwardness appears mainly if you insist the same data also arrive on the bench Wire via a second transmit binding.

### Record the rejected mapping

When a tempting decomposition turns out wrong (physical-link Wires, forced maintenance/user split on one serial link, IDebug splice on a one-cable board), **keep it as an alternative** with a short comparison table. That is often more valuable than only showing the final answer.

### Prior sketches

| Sketch | Key lesson |
|---|---|
| `01_dev_board.md` | Authority-shaped vs physical-link Wires; fair USB-gateway baseline; dual Wire overlap on four devices is a strong fit with Mild friction at worst |
| `02_peer_can.md` | Peer telemetry via observation is clean; peer-addressed control is the fracture; nominated Origin Significant; native CAN ID matrix simpler for true peer symmetry |
| `03_rs485_sensors.md` | Strong fit Config A; Config B: semantic Origin ≠ Link poll scheduler; one Wire across two locally-polled RS-485 segments |
| `04_pi_robot.md` | Forward vs compose; B: WiFi ≠ Wire change; C: Wiring survives Origin offline, no role reassignment |

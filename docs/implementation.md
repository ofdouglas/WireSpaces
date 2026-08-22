# WireSpaces — Implementation Notes

**Status:** Private first draft; project engineering direction, not protocol semantics  
**Purpose:** Language choices, scaling profiles, and execution shape for the reference implementation  
**Authority:** Implementation guidance only. Protocol behavior belongs to `CORE`

Cross-references use the document code plus a section number, for example `IMPL §2`. A bare `§x` always means the current document.

---

# 1. Implementation Language and API Direction

The first implementation should favor **C++** to obtain working results quickly. There is no current need to maintain parallel C and C++ cores from day one.

A good hedge is to keep important core data shapes and functions reasonably C-compatible where practical:

```cpp
struct WsPdu;
struct WsRouter;
enum class WsSendResult : uint8_t;

WsSendResult wsRoute(...);
```

while still using C++ internally for RAII, templates where they materially help static sizing, `constexpr` configuration, stronger types, compile-time validation, and host-side convenience.

A separate C implementation or C ABI can be added later when an actual target requires it.

The embedded baseline should continue to avoid mandatory RTTI, exceptions, and heap allocation, consistent with the project's general C++ rules.

---

# 2. Implementation Scaling Profiles

The same conceptual architecture should scale through substantially different implementations.

| Target | Likely implementation |
|---|---|
| Tiny bare-metal MCU | copies, one Link, switch/linear EID dispatch, LocalBusOnly, no locks |
| Normal single-core MCU | fixed tables, copy queues, mutex/critical sections, several Services |
| Multicore MCU | internal shared-memory Links, per-Endpoint storage, concurrent routing/dispatch |
| Embedded gateway | many Link tasks, read-mostly tables, direct forwarding, optional seqlocks |
| Host PC | conventional threads/queues/maps; optimize only if needed |
| FPGA softcore | generated tables, DMA/FIFOs, potentially zero-copy |
| Pure RTL | BRAM routing tables, RTL LLLs, RTL Endpoints, streaming datapath |

The protocol does not require every row to implement the mechanisms used by every other row. This is a ladder of target hardware, orthogonal to the ladder of user commitment in `INTRO §6`.

The authority-preserving small-MCU collapse rules are in `CORE §25` because they constrain what a conforming tiny target may omit.

---

# 3. "Microkernel-like" Execution Shape

WireSpaces is not an operating system, but a capable implementation has a similar execution shape:

```text
Active entities:      Service tasks, Link-driver tasks
Passive infrastructure: routing tables, dispatch tables, static bindings, bounded queues
```

Data moves directly from producer context toward its configured destination. This makes the cost of communication visible: queues exist where a scheduling or ownership boundary actually requires them, not because the framework mandates a central broker.

# Decision Record: Bounded Endpoint Delivery and Snapshot Semantics

**Status:** Adopted with modifications, revision 0.9 — archived under `archive/` (provenance only; moved from `docs/`)  
**Purpose:** Record what was decided, what was rejected, and why — the normative text lives in `CORE`  
**Authority:** None. `CORE` and `REG` are authoritative; this file explains how they got here

Cross-references use the document code plus a section number. A bare `§x` means this document. This record replaced the original change proposal in place; where it says "the proposal," it means that superseded text, whose substance is reproduced here alongside the decision on it.

---

# 1. What Was Adopted

The proposal that preceded this record asked to replace the `Inline` / `Serialized` delivery pair with a single rule. That was accepted:

> **Endpoint delivery crosses a bounded storage boundary and never synchronously executes Service application code.**

Normative text is `CORE §9.4`; the invariant is `DISP-2`.

Adopted alongside it:

| Decision | Where it now lives |
|---|---|
| Queue and Snapshot as the two storage semantics | `CORE §9.5` |
| One Endpoint owns exactly one storage element | `DISP-6` |
| Semantics and writer concurrency are immutable Service properties | `DISP-6` |
| Capacity declared per Endpoint, never globally | `CORE §9.5` |
| Snapshot generation counter required, not recommended | `DISP-12` |
| Arrival time captured at acceptance | `DISP-2` |
| Endpoint Domains provide implementation-defined serialization | `DISP-13`, `CORE §1.5` |
| `TxBinding` becomes a transmit Endpoint | `CORE §10` |
| Snapshot transmit Endpoints, with a sampled/sent generation echo | `CORE §10.4` |
| One Service writes; one reads a Queue; many read a Snapshot | `DISP-10` |
| One consuming Wire binding per transmit Endpoint | `DISP-11` |
| Copyless preserves semantics and changes only ownership | `OWN-5`, `FUTURE §2.1` |
| `Port` retired; no Service-to-application interface defined | `DISP-3`, `INTRO §4` |
| The Endpoint API is a portability contract for Service-facing code | `SVC-9` |
| A slot holds declared metadata plus payload, copied at acceptance | `DISP-14`, `CORE §9.3` |
| Reading source metadata grants no transmit authority | `DISP-15` |

---

# 2. The Argument That Actually Carried It

The original proposal justified the change with a list of nine questions that every Service otherwise has to answer — which context invokes me, may I block, am I reentrant, and so on. Those are real, but they read as a convenience argument, and the decisive one was not among them:

> **`Inline` delivery makes a Link's worst-case execution time depend on every Service that might be delivered to.**

A CAN receive task that can synchronously enter arbitrary application code has no modular WCET. It cannot be analyzed in isolation, and its worst case changes when a deployment adds a Service the Link's author never saw. That is what made this a correctness change rather than a tidiness change, and it is the form the argument should take when it is challenged.

Everything else follows from it. Message topology stops becoming call topology, so stack depth no longer depends on wiring. A Service can no longer reenter itself by transmitting while handling a receive. The nine questions stop having deployment-dependent answers.

Two supporting arguments were added during review. An RTL Endpoint **is** a FIFO or a register block, so `Inline` never existed on one of the architecture's three target classes, and bounded storage is the only model that spans firmware, host software, and RTL without a special case. And the proposal's factoring turned out to be better than the `CORE §16.4` text it was meant to fit into: the existing four storage classes conflated storage semantics with ownership representation, so the refactoring into independent axes was adopted as an improvement rather than a compatibility fix.

## 2.1 The cost, stated honestly

Latency. `Inline` was the lowest-latency path and a storage boundary puts consumer scheduling delay into the loop. The proposal never stated this, and it is the objection a reviewer will raise first.

The trade is deliberate: `Inline` bought lower *typical* latency at the price of an unbounded worst case in the other direction, and for a control system an analyzable bound on both sides is worth more than a lower average. The practical magnitude depends on something that did not previously matter — a consumer servicing its Links and then draining its Endpoints in the same loop iteration pays roughly one copy, while one draining before servicing pays a full cycle. Loop ordering became an application concern and a measurement item (`CONFORM §3`).

---

# 3. What Was Rejected or Modified

## 3.1 Blanket transmit-side symmetry — rejected

The proposal asked that a Service's send should not execute Link-driver work in the Service's context, mirroring the receive rule. This was **not adopted as a requirement**.

The receive rule is justified by *arbitrary application code with no work bound*. A Link driver is bounded framework code, so the justification does not transfer: requiring a queue hop on every send would cost a copy and a context switch to prevent a problem that does not exist, and it would contradict the small-MCU profile's sanctioned direct `driver.send()` path (`CORE §25`).

What was adopted instead is better than the symmetry it replaces. Snapshot transmit Endpoints give periodic publishers a genuinely queue-free path: the Service publishes current state whenever convenient and the LLL samples on its own cadence, with no per-send copy and no accept/reject (`CORE §10.4`). Queue transmit Endpoints keep explicit acceptance for events and commands. Both directions are now the same two axes with the writer and reader roles swapped, which is a stronger unification than a uniform storage requirement would have been.

## 3.2 Multi-reader Queues — rejected

Not in the proposal, but implied by the `CORE §9.6` text it was editing. Draining is destructive, so two consumers silently split the stream: it passes every test with one consumer and loses messages in production. It is a worker-pool construct with no meaning in RTL. Recorded in `REG §5` because it will be requested again as a load-balanced handler pool.

Multi-reader **Snapshots** are supported, because reading is non-destructive. That carries an implementation rule the obvious first attempt gets wrong: the "have I seen this" watermark lives in each reader, never in the Endpoint (`DISP-12`).

## 3.3 Endpoint-level transmit fan-out — rejected

`CORE §9.6` previously allowed one Endpoint implementation to source several Wires. Allowing it on the transmit side would force per-reader sampled/sent state into every Snapshot transmit Endpoint, and the capability already exists one layer down as Wire splicing or gateway forwarding, both of which preserve source lineage. Superseded by `DISP-10`.

## 3.4 Per-Endpoint versus per-Service storage — decided in favor of per-Endpoint

The proposal priced queue depth carefully but never said whether each Endpoint owns storage or several Endpoints may share one queue. The previous text allowed a Service several Endpoints behind one inbox, so this decided whether RAM multiplies.

Decision: **one Endpoint, one storage element.** A Service with several message types multiplexes internally or declares several Endpoints. The simplification is worth the cost, most Services have few message types, and the bootloader case shows why per-Endpoint depth is the *point* rather than the price — a depth-1 segment Endpoint holding one large block beside a depth-4 command Endpoint holding small packets is exactly the allocation wanted, and any global depth gets it wrong in both directions.

## 3.5 Mutual exclusion — reworded

The proposal required a mechanism "capable of establishing mutual exclusion." In RTL there is no such primitive; there is a single write port or an arbiter. The requirement is stated as **serialization of writers**, which RTL satisfies directly and which does not imply a lock (`DISP-13`).

## 3.6 Instrumentation hooks — narrowed, not deferred

The proposal left synchronous hooks entirely open. One case turned out to be load-bearing rather than optional: with consumer latency now inside the delivery path, freshness handling cannot distinguish late production from late consumption unless arrival time is captured *at acceptance*. That is framework work, so it was specified as part of acceptance (`DISP-2`) rather than left to a hook.

Application-supplied hooks remain closed and are **not in prospect**, since an unrestricted user callback recreates exactly what `Inline` was withdrawn for. The question stays in `REG §6.12` with the list of things any such hook would have to answer.

## 3.7 Naming — the `<T>` risk

`QueueEndpoint<T, N>` and `SnapshotEndpoint<T>` were kept as working names. The Port/Endpoint objection that review raised largely dissolved once storage became 1:1 with Endpoint identity: the class genuinely *is* the Endpoint, rather than a local object borrowing the network-visible word.

One risk survives and is recorded in `REG §6.12`. `T` is the **decoded local representation**, not the wire contract — the schema is (`SVC-7`). `SnapshotEndpoint<MyPose>` invites treating a C++ struct as the wire format, which is the most common way an embedded messaging layer accidentally becomes ABI-dependent. This is a documentation and review obligation rather than an API one, but it is impossible to walk back once Services exist.

---

# 4. Consequences Not in the Original Proposal

Four things fell out of the decision that the proposal did not anticipate.

**`Port` became unnecessary.** It named the local typed interface as distinct from the network-visible Endpoint, which was a real distinction only while delivery meant invoking a handler — the function a Service wrote was genuinely a different object from the identity the Dispatcher resolved. Bounded storage collapses them, and the term was left classifying without constraining. It is retired, along with the `REG §6.12` question about whether it should become a generated API concept (`REG §5`).

**WireSpaces stopped defining the Service-to-application interface at all.** Endpoint storage semantics and writer concurrency proved expressive enough to build the Services these systems need, and the span from a bare-metal `switch` to an RTL register block to a host binding is too wide for one API vocabulary. The boundary is scoped precisely: WireSpaces owns the receive path to acceptance and the transmit path from acceptance, and a Service may not inject work back across it (`DISP-3`). Without that clause, a Service-defined user callback fired inside acceptance reintroduces `Inline` through the side door.

**A view into the ingress PDU stopped being possible.** Handing the consumer a read-only view of the arriving packet instead of copying metadata is the obvious optimization, and `Inline` was what made it safe — the consumer ran before the LLL's buffer was released. Deferred consumption removes that guarantee, so metadata is copied at acceptance (`DISP-14`). The API may still be an opaque accessor, which is worth having for encapsulation, but it is backed by copied fields. This also forced metadata to be *declared* rather than unconditional, since about 8 bytes per slot is free on a 512-byte segment and nearly doubles a queue of 8-byte commands.

**The Endpoint API became an explicit portability contract** (`SVC-9`). This was a correction during review: the silence above the Service was initially extended below it as a non-goal on Service portability, which is backwards. The Endpoint API is where a Service meets WireSpaces, and it is exactly the surface that must be stable — a low-end and a high-end 32-bit MCU should run identical Service source over entirely different network stacks, task models, and drivers. Without that there is no Service ecosystem, and `SVC-7`'s schema-over-bytes contract would guarantee only that two incompatible implementations agreed on the bytes. Portability is bounded the way link independence is (`SVC-2`): by the Service's declared resource and timing envelope, and by whatever non-WireSpaces dependencies it reaches for. Composed or injected dependencies therefore matter more in Service code than they would in application code.

**`DISP-2`'s old promise became unnecessary rather than reworded.** It used to guarantee that a declared delivery policy survived a placement change, which implied something configurable existed. Semantics are now fixed by the Service definition, so the guarantee is structural and tooling has nothing to check.

**Snapshot transmit needed a failure signal.** Not staleness reporting — a silent failure mode. A Queue transmit Endpoint that is unbound, unscheduled, or attached to a dead driver fills and rejects, so the Service finds out. A Snapshot transmit Endpoint that was never bound to a Wire behaves *identically to a working one*. The 16-bit wrapping sampled/sent generation echo exists for that, and it reuses the Snapshot generation rather than adding a mechanism (`CORE §10.4`).

---

# 5. Prototype Validation

The prototype should test this decision, not merely implement its API shape, and should be allowed to invalidate or simplify it.

Behavioral cases, now in `CONFORM §2`:

1. deliver into one Concurrent Queue Endpoint from several Link-driver contexts;
2. fill a Queue deliberately and verify explicit rejection and diagnostics;
3. feed a Snapshot far faster than its consumer and verify coherent latest-value reads and an advancing generation;
4. demonstrate an Exclusive Endpoint omitting synchronization it does not need;
5. reject two Services on one Queue Endpoint; accept several on a Snapshot, each tracking its own watermark;
6. detect an unbound Snapshot transmit Endpoint by a generation echo that never advances, and verify modular comparison across wrap;
7. verify no Service code is reachable from a Link or dispatch context, including when a Service transmits during a receive;
8. on an AMP target or simulation, show inter-core delivery going through a Link between separate Endpoint Domains;
9. confirm a handle-based storage implementation can later replace copies without changing the execution model.

Measurements, now in `CONFORM §3`:

1. **added end-to-end latency on a control path, under both main-loop orderings** — the objection a skeptic will actually raise, and the one the proposal did not measure;
2. worst-case serialization hold time for bounded Queue and Snapshot writes, against the platform's interrupt-latency requirement;
3. Endpoint storage RAM across a realistic Service set.

---

# 6. Still Open

Carried into `REG §6.12` rather than settled here:

- final type and API names, and how `T` is presented so it cannot be mistaken for the wire contract;
- whether a small documented default Queue capacity is offered, or every Queue declares its depth;
- whether any narrowly scoped synchronous instrumentation hook is ever permitted;
- whether the transmit direction keeps a vocabulary distinct from receive, given that it carries authority receive storage does not;
- how much of the Endpoint API `SVC-9` actually fixes, and how the portability contract is verified rather than merely intended;
- how delivered metadata is declared and accessed, including what happens when a Service reads a field its Endpoint did not declare;
- exact Snapshot memory ordering: atomics, barriers, seqlock retry rules, and the interaction between the value generation and the sampled/sent echo;
- the copyless ownership model in full — lifetime, fan-out, reclamation, DMA ownership, cache coherency, reset, and stale-handle/ABA (`REG §6.6`, `FUTURE §2.1`).

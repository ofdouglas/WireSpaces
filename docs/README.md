# WireSpaces

Network and messaging stack for hierarchical embedded systems.

A **Host** is an independently routed and dispatchable Endpoint Domain; a physical device may contain several Hosts.

A **Wire** is a loop-free **Logical Bus** and propagation domain realized by one or more configured Links. One or more **Hosts** may independently source traffic onto it; traffic propagates over the Wire's configured Link topology, and canonical source/destination identity controls who authored and who accepts a PDU rather than selecting an ordinary forwarding path.

Each Endpoint Domain has one deployment-scoped `HostId`, used on every Wire it joins. The canonical PDU carries `SrcHostId` and `DestHostId`; forwarding is driven by static/read-mostly Wire membership. Constrained Links may elide or project fields when their Link Binding and frame context reconstruct the same canonical Wire, source, and destination.

The same Endpoint and Service model is meant to survive whether a Wire is realized inside one device, as a shared-memory channel between cores, across a CAN bus, over Ethernet, or through an FPGA datapath.

**Status: private first draft.** The architecture is provisional, no Link profile is byte-exact, and no wire interoperability is claimed. Implementation is at an early stage in `sim/`.

---

## Documents

Read in this order if you are new to the project:

| Document | What it covers |
|---|---|
| [introduction.md](introduction.md) | What WireSpaces is and why. Non-goals, maturity ladder, worked examples, roadmap |
| [core_architecture.md](core_architecture.md) | **The main document.** The protocol model and Host runtime — everything meant to be buildable now |
| [bit_layout.md](bit_layout.md) | Byte and bit ordering conventions, and canonical descriptor packing |
| [link_profiles.md](link_profiles.md) | Per-carrier encodings: Classical CAN, UART, Ethernet, I2C/SPI, and others |
| [bits_transport.md](bits_transport.md) | BITS finite-object Transport: connections, bounded execution, candidate messages, flow control, and open completion/lifetime rules |
| [deployment.md](deployment.md) | Discovery, commissioning, Wiring, host tooling |
| [conformance.md](conformance.md) | Reference vectors, boundary tests, exit criteria for provisional status |
| [implementation.md](implementation.md) | Language choices, scaling profiles, execution shape for the reference code |
| [library_architecture.md](library_architecture.md) | Core library structure, seams, and public API shape for the prototype |
| [future_work.md](future_work.md) | Material not yet designed. Nothing here is a requirement |
| [architecture_register.md](architecture_register.md) | Confidence levels, invariants, superseded concepts, open questions |
| [history.md](history.md) | Revision history and provenance |

Other material:

- [proposal_disposition.md](proposal_disposition.md) (`INTEGRATION`) — source-by-source integration status and remaining decisions
- [`archive/`](archive/) — superseded specifications and adopted decision records (provenance only; not normative)
  - [`decision-bounded-endpoint-delivery.md`](archive/decision-bounded-endpoint-delivery.md) — revision 0.9 delivery-model decision record (detail beyond `HIST §4.2`)
- [`notes/can_id_provisioning.md`](notes/can_id_provisioning.md) — CAN commissioning / identifier working notes (not yet in `LINK` / `DEPLOY`)
- `sim/` — reference implementation, currently at an early stage
- [simulator README](../sim/README.md) — simulator requirements

Decision records and superseded specs have no authority: `CORE` owns shared behavior, `BITS-TRANSPORT` owns BITS-specific design, and `REG` owns status and invariants. They explain how the current documents got there.

## Conventions

Cross-references are document-coded: `CORE §6.2` means section 6.2 of `core_architecture.md`. A bare `§6.2` always means the current document. Document codes are listed in `INTRO §10`. `BITS` names bit layout; `BITS-TRANSPORT` names the finite-object Transport document. Invariants are cited by stable ID, such as `SPLICE-2` or `PDU-1`, and live in `architecture_register.md`.

Document scope:

- **`core_architecture.md` carries buildable runtime behavior**, including optional runtime features such as credits and QoS-Full.
- **Catalogs, host Service schemas, test vectors, and project engineering choices** live in `DEPLOY`, `FUTURE`, `CONFORM`, and `IMPL` respectively.

Three rules govern edits:

- **`architecture_register.md` is the control surface.** A change to any document is not finished until the confidence levels, invariants, superseded list, and open questions there reflect it.
- **`future_work.md` has no authority.** If it disagrees with `core_architecture.md`, the latter wins.
- **`history.md` is archival.** Revision narrative belongs there, not in `REG`.

## For agents

If you are generating code or designs from these documents:

1. `core_architecture.md` is the source of truth for shared behavior. `bits_transport.md` owns the BITS prototype design; `link_profiles.md` owns carrier encodings. Neither profile set is interoperability-frozen.
2. Do not implement anything from `future_work.md` unless explicitly asked. It records intent, not an agreed backlog.
3. The invariants in `architecture_register.md §4` are the constraints most worth checking work against.
4. Use `conformance.md` for test vectors and boundary cases; use `implementation.md` for language and scaling choices.
5. Prefer small concrete implementations and tests over generalized framework hierarchies. Do not introduce an abstraction until two real Links or targets need it.
6. Nothing in `archive/` is current. `proposed/` files are provenance/design input; consult `proposal_disposition.md` and `REG` for incorporation status.
7. `LIB §12.1` rejects ambient sibling `Design/Firmware` include paths and recommends a reproducible vendored extraction with recorded provenance for phase 1; the final core-library location remains open.

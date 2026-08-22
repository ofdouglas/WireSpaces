# WireSpaces

Network and messaging stack for hierarchical embedded systems.

A **Wire** is a logical bus with exactly one **Origin** and zero or more **Nodes**. The same Endpoint and Service model is meant to survive whether that bus is realized inside one device, as a shared-memory channel between cores, a CAN bus, an Ethernet link, or an FPGA datapath.

**Status: private first draft.** The architecture is provisional, no Link profile is byte-exact, and no wire interoperability is claimed. Implementation is at an early stage in `sim/`.

---

## Documents

Read in this order if you are new to the project:

| Document | What it covers |
|---|---|
| [introduction.md](introduction.md) | What WireSpaces is and why. Non-goals, maturity ladder, worked examples, roadmap |
| [core_architecture.md](core_architecture.md) | **The main document.** The protocol model and node runtime — everything meant to be buildable now |
| [bit_layout.md](bit_layout.md) | Byte and bit ordering conventions, and canonical descriptor packing |
| [link_profiles.md](link_profiles.md) | Per-carrier encodings: Classical CAN, UART, Ethernet, I2C/SPI, and others |
| [deployment.md](deployment.md) | Discovery, commissioning, Wiring, host tooling |
| [conformance.md](conformance.md) | Reference vectors, boundary tests, exit criteria for provisional status |
| [implementation.md](implementation.md) | Language choices, scaling profiles, execution shape for the reference code |
| [library_architecture.md](library_architecture.md) | Core library structure, seams, and public API shape for the prototype |
| [future_work.md](future_work.md) | Material not yet designed. Nothing here is a requirement |
| [architecture_register.md](architecture_register.md) | Confidence levels, invariants, superseded concepts, open questions |
| [history.md](history.md) | Revision history and provenance |

Other material:

- [decision-bounded-endpoint-delivery.md](decision-bounded-endpoint-delivery.md) — decision record for the revision 0.9 delivery model: what was adopted, rejected, and why
- `can_id_provisioning.md` — CAN identifier allocation working notes
- `sim/` — the reference implementation, currently at an early stage
- `sim_rfp.md` — simulator requirements
- `archive/` — superseded documents, retained for provenance only

A decision record explains reasoning that would bloat `CORE` if inlined. It has no authority: `CORE` and `REG` are normative, and a decision record only says how they got there.

## Conventions

Cross-references are document-coded: `CORE §6.2` means section 6.2 of `core_architecture.md`. A bare `§6.2` always means the current document. Document codes are listed in `INTRO §10`. Invariants are cited by stable ID, such as `SPLICE-2` or `PDU-1`, and live in `architecture_register.md`.

Document scope:

- **`core_architecture.md` carries buildable runtime behavior**, including optional runtime features such as credits and QoS-Full.
- **Catalogs, host Service schemas, test vectors, and project engineering choices** live in `DEPLOY`, `FUTURE`, `CONFORM`, and `IMPL` respectively.

Three rules govern edits:

- **`architecture_register.md` is the control surface.** A change to any document is not finished until the confidence levels, invariants, superseded list, and open questions there reflect it.
- **`future_work.md` has no authority.** If it disagrees with `core_architecture.md`, the latter wins.
- **`history.md` is archival.** Revision narrative belongs there, not in `REG`.

## For agents

If you are generating code or designs from these documents:

1. `core_architecture.md` is the source of truth for behavior. `link_profiles.md` owns byte-level encodings and is deliberately unfinished.
2. Do not implement anything from `future_work.md` unless explicitly asked. It records intent, not an agreed backlog.
3. The invariants in `architecture_register.md §4` are the constraints most worth checking work against.
4. Use `conformance.md` for test vectors and boundary cases; use `implementation.md` for language and scaling choices.
5. Prefer small concrete implementations and tests over generalized framework hierarchies. Do not introduce an abstraction until two real Links or targets need it.
6. Nothing in `archive/` is current.
7. `library_architecture.md` assumes a sibling `Design/Firmware` checkout on the include path when building the core library (`LIB §12.1`).

# WireSpaces — Proposal Integration Disposition

**Status:** Integration record for the main document set, revision 0.15
**Purpose:** Record which proposal content is incorporated, superseded, or still pending
**Authority:** `REG` owns status and invariants; `CORE` owns behavior; `LINK` owns carrier representations. This table does not independently standardize a proposal.
**Document code:** `INTEGRATION`

## 1. Scope of this integration

This pass implements the disposition review, Host terminology and Endpoint/Transport Entity boundary changes, and unified CAN11 VCN integration. It does not promote the detailed BITS protocol into the main set, select its TransportType number, or implement code. Source proposal files remain unchanged as provenance.

Host terminology is confirmed: `Participant` → `Host`, `ParticipantId` → `HostId`, `SrcParticipantId` → `SrcHostId`, and `DestParticipantId` → `DestHostId`. It preserves one deployment-scoped identity per Endpoint Domain and the provisional six-byte descriptor. A Host is not synonymous with a physical device or a tooling computer.

## 2. Source disposition

| Source | Disposition | Main-set destination / remaining work |
|---|---|---|
| [Early addressing ideas](proposed/idea.md) | Superseded as a standalone design. Its VCN/default-map ideas feed the unified proposal. | `LINK §2`; no independent authority for its compact/general encoding or numeric examples. |
| [Global identity and multi-Origin revision 2](<proposed/WireSpaces Change Proposal — Global Participants, Multi-Origin Wires, and Revised Addressing.md>) | Global Endpoint-Domain identity already incorporated. Per-interaction Origin/Direction and earlier encoding candidates superseded. | `CORE §3`, `REG §5`; preserve its evidence limitations and physical-device identity question. |
| [Global identity change request revision 3](<proposed/change_request_global_participants_multi_origin(1).md>) | Multi-initiator/source-destination semantics already incorporated. Four 10-bit fields, destination next-hop wording, discriminator coexistence, and direct-only compact CAN superseded. | `CORE §2–§5`, `LINK §2`; no reintroduction of its layout or mandatory migration instructions. |
| [Revision 6](<proposed/WireSpaces Change Request — Revision 6.md>) | Logical-Bus propagation, overlapping Wires, complete canonical ingress, and six-byte preferred descriptor already incorporated. | `CORE`; shared/exclusive CAN layouts and its one-Wire restriction superseded by unified VCN. Widths remain provisional, regardless of stronger wording in the source. |
| [Revision 6 refinement](proposed/WireSpaces_Revision_6_Refinement_CAN11_and_Feedback.md) | Canonical local-only `kLocalBus`, identity-universe/splice boundaries, allocated Guest coexistence, and topology/headroom gate already incorporated. | `CORE`, `REG`; three-profile baseline, native VCN8, and one-Wire-per-CAN11 restriction now superseded. |
| [Unified CAN11 VCN](proposed/WireSpaces_CAN11_Unified_VCN_Proposal.md) | Incorporated as the current CAN11 direction, with exact profile details still provisional. | `LINK §2`, `DEPLOY §2.4`, `REG`: Guest/Native VCN, two-Main default map, alias-scoped native maps, deployment-wide Guest meanings, immutable aliases, spare-alias migration. Compact/General is experimental only. |
| [CAN ID provisioning](proposed/can_id_provisioning.md) | Candidate mechanism only; not an accepted commissioning profile. | `DEPLOY §1.2`, `REG §6.8/§6.10`: review identical-response discovery. Old NodeId/alias layout and 64-bit identity are not imported; UUID versus DeviceId remains unresolved. |
| [BITS transport design](proposed/BITS_design_updated.md) | Endpoint/Transport Entity boundaries incorporated; detailed BITS protocol remains pending. | `CORE §9/§20`, `REG §6.12/§6.16`: one boundary, multiple declared ingress elements, deferred protocol processing, explicit copy/reference ownership. Separate protocol document, API and full conformance integration remain follow-on work. |

## 3. Resolutions made in this pass

- **Endpoint cardinality:** one Transport Entity boundary replaces exactly one storage element. Each ingress element still declares Queue/Snapshot semantics, concurrency, bounds, and exhaustion. Simple datagram Endpoints remain one-element implementations.
- **Execution:** receive hooks classify and retain data only. ACK/window/duplicate/retry/SETUP/completion behavior and Service callbacks run later. The proposed direct-to-flash receive optimization is not adopted.
- **Ownership:** copies remain the baseline. A reference path requires an owned handle or bounded immutable lease, not a retained borrow. Detailed ownership APIs remain deferred.
- **CAN11 baseline:** Guest and Native VCN replace native VCN8 and Compact/General. Native uses the preferred QoS2/Alias3/VCN5/Direction1 budget; the default map reserves VCN3, not the all-ones code.
- **Egress ambiguity:** configuration selects one native TX alias per admitted canonical tuple. Several receive aliases may coexist; a sender neither picks the first match nor sends duplicate representations.
- **Migration:** prepare and validate a spare alias before switching TX; pin accepted work to its original binding; retire/reuse only after stale-frame exclusion. No-spare and Guest updates need coordinated offline/cutover mechanisms rather than live in-place mutation.
- **Profile boundaries:** MainA/MainB/NodeN are mapping positions, WireAlias/VCN/Direction are Link-local fields, and the Router continues to see canonical Wire/Host identity only.

## 4. Remaining decisions and follow-on integration

`REG §6` remains authoritative. In particular:

- Freeze exact CAN identifier/profile versions, custom-map control reservations, Host assignment defaults, Guest growth profiles, and arbitration policy.
- Define fingerprints, distributed readiness, sender-selection publication, alias retirement/reuse, partial-migration recovery, and Guest cutover. The safety requirements do not constitute a complete management protocol.
- Redesign and validate PDUA control/Endpoint packing, N=1 forms, CRC, DLC/padding, and capacity. Existing capacity examples remain conditional.
- Resolve commissioning identity and prove anonymous-response behavior on the selected controller/medium before claiming support.
- Promote BITS into a dedicated document with an unambiguous cross-reference code: `BITS` currently means `bit_layout.md`. Resolve completion, session reuse, sequence/window ambiguity, sink commitment, rejection boundaries, and hop versus end-to-end integrity.
- Update detailed Transport APIs, implementation sequence, and BITS conformance in the follow-on transport pass. Current library examples describe the simple-datagram slice, not a completed BITS architecture.

Historical revision narratives and proposal filenames retain their original terminology. Current semantic prose and API sketches in the main set use Host terminology. This is documentation integration; it does not claim that repository code has implemented the revised design.

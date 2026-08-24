# Sketch — Archetype 08: Flat Ethernet Peer Mesh (Boundary Test)

```text
**Archetype:** archetypes/08_ethernet_peer_mesh.md
**Agent:** cursor_C
**Date:** 2026-08-23
```

---

## Executive summary

```text
**Minimum mapping:** 13 Participants (12 instruments + workstation), 1 Logical Bus (W1 LabEth) on WS-over-UDP/IP; no forwarding; CAN11 not used.
**Question A (R6):** Natural.
**Question B (CAN11):** N/A.
**Worst friction (minimum path):** Configuration burden — None / Mild.
**Main lesson:** R6 maps cleanly (dense peer graph is boring on Ethernet); CAN11 correctly N/A; networking-layer advantage over raw UDP is small — adoption value would depend on shared Service ecosystem and tooling, not tested here.
```

---

## Disposition block

| Area | Assessment |
|---|---|
| Participant identity | Natural |
| Wire decomposition | Natural |
| Forwarding | Simple |
| Non-CAN configuration | Low |
| CAN11 VCN fit | N/A |
| Better with CAN29? | N/A |

**Explanation:** Question A: twelve identical Linux instruments and one orchestration workstation each map to one deployment-global ParticipantId; all share one lab communication scope on a switched Ethernet segment with symmetric capability and no nominated runtime master (`CORE §3.1`, `DEPLOY §1.6`). One Logical Bus (W1 LabEth) is the honest minimum — measurement broadcasts, directed peer commands, and workstation script fan-out share one propagation scope. Multi-initiator sourcing is ordinary R6. A full peer graph (C(13,2) = 78 possible relations) requires **no pre-enumeration** on Ethernet — canonical Src/Dest per PDU — contrasting cleanly with CAN11 dense-peer friction (archetype 07). The workstation remains an ordinary peer despite orchestration convenience. **R6 fit is strong.**

Question B: **N/A** — no CAN buses in the archetype (`archetype §What this archetype does not include`). Forcing CAN11 would be dishonest. Full canonical Src/Dest on WS-over-UDP/IP is the rich-addressing path without VCN enumeration (`LINK §6`).

**Separate from Question A:** WS **networking-layer** advantage over raw UDP sockets on this isolated LAN is small — ParticipantId + Wire largely relabel what IP:port already provides. That is an adoption question, not an R6 naturalness verdict. Whether WS is worthwhile here would depend primarily on a shared Service ecosystem (health, logging, configuration, firmware update, discovery, RPC) and tooling — **not modeled in this sketch's minimum mapping**.

---

## 1. Native communication model

A laboratory test rack holds twelve identical Linux-based instruments and one developer workstation, all connected to one Gigabit Ethernet switch (LabEth). Each instrument runs its own control and data-acquisition stack and exposes small UDP/TCP services to peers.

**Symmetric peers:** any instrument may publish measurement streams (~20 Hz), accept configuration and start/stop commands from any peer, and request data or send commands to any other instrument during orchestrated experiment flows. The workstation runs batch scripts that coordinate multi-instrument sequences — convenience for scripting, **not** a permanent plant authority or runtime master.

**Time sync** (NTP or PTP) is conventional infrastructure shared by all hosts; it is not modeled as WireSpaces traffic.

**Failure:** loss of one instrument degrades the experiment; others continue if scripts allow. Loss of the switch takes the entire rack offline. No hot-standby instruments in baseline.

---

## 2. Obvious conventional implementation

> **Obvious conventional implementation:** Fixed UDP port matrix or lightweight service discovery per instrument; multicast or per-subscriber unicast for ~20 Hz measurement streams; directed request/response or fire-and-forget datagrams for start/stop and parameter commands; workstation scripts open sockets to instrument IP:port subsets; no gateway, no fieldbus, no hierarchical cell model.

> **What structure does a conventional design use?** Per-instrument IP address + port table (or DNS names); optional multicast group for telemetry; symmetric socket APIs in each instrument daemon; orchestration as ephemeral Python/shell scripts — no deployment-global identity layer, no named logical bus, no forwarding tables.

WireSpaces adds: deployment-global ParticipantIds, one named Logical Bus, a LabEth Link Binding for WS-over-UDP encapsulation, canonical Src/Dest on every PDU, and Endpoint-level consumption of broadcast measurement streams.

At the **networking substrate**, WS largely relabels what IP + UDP ports already provide. The fairer comparison for adoption — not scored as Question A — is:

```text
custom socket protocols + per-instrument service conventions + tooling
vs
one WS service environment (identity, health, logging, config, update, RPC, …)
```

A rack of twelve identical Linux instruments is plausible WS territory for standardized Services, but this sketch's minimum mapping does not model that layer.

---

## 3. Minimum mapping (required first)

### 3.1 Participants

| ParticipantId | Endpoint Domain | Device | Notes |
|---|---|---|---|
| 0x01 | Inst01 | Instrument 1 | Symmetric peer |
| 0x02 | Inst02 | Instrument 2 | Symmetric peer |
| 0x03 | Inst03 | Instrument 3 | Symmetric peer |
| 0x04 | Inst04 | Instrument 4 | Symmetric peer |
| 0x05 | Inst05 | Instrument 5 | Symmetric peer |
| 0x06 | Inst06 | Instrument 6 | Symmetric peer |
| 0x07 | Inst07 | Instrument 7 | Symmetric peer |
| 0x08 | Inst08 | Instrument 8 | Symmetric peer |
| 0x09 | Inst09 | Instrument 9 | Symmetric peer |
| 0x0A | Inst10 | Instrument 10 | Symmetric peer |
| 0x0B | Inst11 | Instrument 11 | Symmetric peer |
| 0x0C | Inst12 | Instrument 12 | Symmetric peer |
| 0x0D | DevWorkstation | Developer workstation | Orchestration scripts; not runtime master |

Thirteen Participants, one Endpoint Domain each (each Linux instrument is one independently dispatchable domain). No coordinator Participant. Workstation is a peer with orchestration convenience, not elevated Wire authority.

### 3.2 Wires

| Wire (#) | Participants | Physical links | Purpose |
|---|---|---|---|
| W1 LabEth | 0x01–0x0D | LabEth (Gigabit Ethernet switched) | Measurement streams, directed peer commands, script orchestration |

One Logical Bus matches the archetype's single lab communication scope on this switched LAN. A Wire is **not** an Ethernet broadcast domain or a multicast group per application topic — it names where communication is allowed to propagate (`W1` = lab rack scope; Endpoint names the interaction/service; Dest names the participant or broadcast). Multiple Wires could share the same physical LAN for genuinely distinct scopes (e.g. production vs calibration); this archetype has no reason to split (`brief §5` step 7 not justified; aggregate telemetry ~240 msg/s is trivial on GbE).

### 3.3 Interactions (primary)

| Interaction | Src | Dest | Wire | Broadcast? | Notes |
|---|---|---|---|---|---|
| Measurement stream | 0x01–0x0C (each) | kBroadcast | W1 | Yes | ~20 Hz per instrument; subscribers consume (`CORE §12.6`) |
| Directed command | Any 0x01–0x0D | Any other 0x01–0x0D | W1 | No | Start/stop, parameter set, experiment handshakes |
| Orchestration fan-out | 0x0D | Subset of 0x01–0x0C | W1 | No | Batch scripts — same directed-command model, not a separate Wire |
| Orchestration response | 0x01–0x0C | 0x0D | W1 | No | Acks/status to scripts when needed |
| Time sync | — | — | — | — | NTP/PTP — conventional; **not WS** |

**Directed pair capacity:** C(13,2) = 78 unordered peer relations are possible; **none need pre-enumeration** on Ethernet — canonical Src/Dest per frame (`LINK §6`). Minimum mapping uses runtime `(src, dest)` lookup, not a relation table. Useful contrast after CAN11 discussion:

```text
CAN11 dense peer graph  →  relation compression / VCN enumeration visible
Ethernet dense peer graph  →  completely boring (strong R6 evidence)
```

**Twelve instruments** publish measurement to `kBroadcast` on W1 (~20 Hz each). Any Participant on W1 may originate broadcast traffic; consumption is Endpoint-specific — not necessarily twelve separate network configuration objects. Workstation may subscribe to all or a subset.

**No** nominated master interactions. Workstation commands are peer-originated PDUs under ParticipantId 0x0D.

### 3.4 Forwarding (if any)

None. Flat switched LAN; no gateway device.

### 3.5 Link profiles

| Physical link | Profile | WS Wire(s) | Notes |
|---|---|---|---|
| LabEth | WS over UDP/IP datagram | W1 | Archetype uses UDP/TCP services; UDP WS profile chosen for datagram parity with conventional design (`LINK §6`) |

Full WireNumber and ParticipantIds carried in WS encapsulation — no field elision pressure at this scale. Ethernet profile details provisional (`LINK §6`); binding supplies Wire context and reconstruction rules.

**CAN11:** Not used. **CAN29:** Not applicable (no CAN physical link).

### 3.6 CAN11 bindings (if CAN11 used)

**N/A** — no Classical CAN in this archetype. CAN11 VCN accounting not applicable (`archetype §Mapper note`).

**Membership vs presence:** All thirteen Participants are **preconfigured** on W1. Workstation (0x0D) remains a Wire member when orchestration scripts are not running — scripts are application-level ephemera; configured wiring persists. Instruments may be unreachable when powered down; delivery fails or queues drain without removing Wire membership (`brief §6.4`).

**Non-CAN Link binding (LabEth summary):**

```text
Physical link:        LabEth (Gigabit Ethernet, switched)
Profile:              WS over UDP/IP datagram (provisional)
Link binding:         one LabEth → W1 binding (not one binding per Participant)
Wire:                 W1 LabEth
Wire members:         0x01–0x0D
Participant resolve:  PID → host/IP (13 deployment entries; analogous to 13 IPs/DNS names)
Canonical fields:     full SrcParticipantId, DestParticipantId, WireNumber
Broadcast:            Dest=kBroadcast on W1 (12 instrument publishers at application level)
Pair enumeration:     not required — dense peer graph via per-frame identity
```

---

## 4. Optional optimizations

### 4.1 Separate Wires for measurement vs command

W1 measurements + W2 commands on the same physical switch. **Problem it would solve:** isolate bandwidth or failure scope. **Rejected:** archetype bandwidth is trivial (~240 msg/s); no distinct failure scope — switch loss affects both.

### 4.2 WS directly over Ethernet (Layer 2)

Dedicated L2 WS profile without UDP/IP (`LINK §6`). **Problem it would solve:** lower overhead for FPGA/measurement paths. **Rejected for minimum mapping:** lab rack already uses IP infrastructure, routing familiarity, and conventional UDP services; UDP/IP WS is the honest match.

### 4.3 Per-experiment cohort Wires

Multiple Wires for different script-defined instrument subsets. **Rejected:** wire proliferation driven by orchestration convenience, not propagation scope; one switched LAN is one domain.

### 4.4 TCP transport for commands

Use TCP WS profile for directed commands while UDP carries measurements. **Problem it would solve:** reliable command delivery. **Rejected for minimum mapping:** archetype allows TCP but does not require transport split; single UDP profile keeps minimum mapping simpler. WS Transport semantics can cover reliability if needed without a second Wire.

### 4.5 Forcing CAN11 or CAN29

No CAN physical link exists. **Rejected** — dishonest for this archetype.

---

## 5. Friction signals (minimum mapping happy path)

| Signal | None / Mild / Significant | One-sentence justification |
|---|---|---|
| Artificial Wire | None | W1 LabEth is the shared switched segment — one meaningful propagation domain. |
| Wire proliferation | None | One Wire for thirteen peers on one LAN. |
| Artificial hierarchy | None | No MainA/MainB or coordinator role; workstation is an ordinary peer. |
| VCN pressure | N/A | No CAN11. |
| WireAlias pressure | N/A | No CAN11. |
| Configuration burden | None / Mild | One Wire, one LabEth Link binding, thirteen ParticipantIds, thirteen PID→host entries (≈ conventional IP table) — no pairwise relation table; exact UDP profile accounting TBD (`LINK §6`). |
| Failure/topology mismatch | Mild | Switch loss is total rack outage (accurate); per-instrument loss is natural; WS does not obscure either. |

---

## 6. Model pressure

- **If you could change one WS concept** to simplify this system, what would it be? Nothing required for R6 mapping — the canonical model already fits cleanly. Adoption friction, if any, is at the networking substrate (UDP already works) or in Service/tooling maturity, not in Participant/Wire decomposition.
- **Did WS expose a useful distinction** the conventional model obscures? Yes at the model layer: deployment-global ParticipantId; named communication scope (W1) vs implicit subnet; canonical Src/Dest across a dense peer graph without relation tables; workstation as peer not master. Networking-layer distinction vs raw sockets is thin; **Service ecosystem value is the open adoption question** and is not tested here.

**Synthesis note:** Encouraging boundary result — WS imposes little architectural friction even where networking abstractions add relatively little. The architecture does not need every topology to showcase gateways and fieldbuses; sometimes it should quietly become a service protocol over Ethernet. Scorecard:

```text
Participant model:              strong pass (Natural)
Logical Bus / dense peers:      strong pass (Natural)
Forwarding:                     trivial
CAN11:                          correctly N/A
Network-layer advantage vs UDP: small
Service ecosystem advantage:    not tested by this sketch
```

---

## 7. Open questions

- WS-over-UDP broadcast realization: IP multicast group per Wire vs unicast fan-out at each sender — profile choice affects configuration count, not Wire count (`LINK §6` undeveloped).
- Whether measurement subscribers use configured consumption tables vs promiscuous receive — observation semantics (`CORE §12.6`) vs conventional multicast join.
- TCP vs UDP for latency-sensitive handshakes (< 10 ms) — archetype allows both; minimum mapping picks UDP WS datagram.
- Instrument replacement: new hardware takes same ParticipantId role (`DEPLOY §1.6`) — clearer than IP reassignment in conventional designs, but only matters if WS is adopted.
- Service ecosystem (health, logging, firmware update, discovery, standard RPC): plausible adoption driver for identical instrument racks — out of scope for minimum mapping; would change the adoption comparison, not the R6 Wire decomposition.

---

## 8. Spec findings (optional)

| ID | Finding |
|---|---|
| — | No new spec finding filed. Ethernet WS-over-UDP profile remains provisional (`LINK §6`); sketch does not expose a normative gap beyond documented immaturity. |

---

## 9. Diagrams (optional)

### Device-centric

```text
LabEth (Gigabit Ethernet switch)
┌────────────────────────────────────────────────────────────┐
│  Inst01 … Inst12 (0x01–0x0C)    DevWorkstation (0x0D)      │
│  symmetric UDP/TCP services      orchestration scripts      │
│  ~20 Hz measurement each         (peer, not master)        │
└────────────────────────────────────────────────────────────┘
No gateway. No fieldbus. No CAN.
```

### Wire-centric (production minimum)

```text
Wire W1 LabEth (WS over UDP/IP)
├── 0x01 Inst01  ─┬─ measurement → kBroadcast
├── 0x02 Inst02  ─┤
├──  …           ─┤  (~20 Hz × 12)
├── 0x0C Inst12  ─┘
├── 0x0D DevWorkstation  (orchestration peer)
└── Directed commands: any ↔ any via canonical Src/Dest
    (no pre-enumerated pair table on Ethernet)

No forwarding. No hierarchy. CAN11 N/A.
```

### Peer mesh (logical)

```text
        Inst01
       /  |  \
      /   |   \
   Inst02-+-Inst03- … -Inst12
      \   |   /
       \  |  /
        Workstation (0x0D)

13 nodes, full mesh capability; 12 measurement broadcast sources.
```

### Conventional vs WS (boundary comparison)

| Aspect | Conventional UDP sockets | WS minimum mapping |
|---|---|---|
| Identity | IP:port per instrument | ParticipantId 0x01–0x0D |
| Telemetry | Multicast group or fan-out | kBroadcast on W1 (12 publishers) |
| Commands | Socket to dest IP:port | Canonical Src/Dest on W1 |
| Scope | Implicit (same subnet) | Named Wire W1 LabEth |
| Network config | ~13 host addresses + ports | 1 Wire + 1 Link binding + 13 PID→host entries |
| Services/tooling | Custom per instrument | Not tested — potential WS adoption driver |

---

## Devices (reference)

| Device | Endpoint Domains | Link interfaces | Role summary |
|---|---|---|---|
| Instrument 1–12 | Inst01–Inst12 | LabEth | W1 peers 0x01–0x0C; symmetric |
| Developer workstation | DevWorkstation | LabEth | W1 peer 0x0D; script orchestration |

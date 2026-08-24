# Archetype 08 — Flat Ethernet Peer Mesh (Boundary Test)

**ID:** 08  
**Convergence test:** No  
**Suggested Link mix:** Ethernet / CAN29 or non-CAN — **CAN11 not expected to be honest choice**  
**Stress:** **Boundary test** — many equal peers, no natural hierarchy

---

## Physical topology

```text
Laboratory test rack with twelve identical Linux-based instruments.

Each instrument:
  - Runs its own control and data acquisition stack
  - Connects to one shared Gigabit Ethernet switch
  - Exposes a small set of UDP/TCP services to peers

One developer workstation on the same switch for orchestration scripts.

No dedicated "gateway" device. No fieldbus leaves in baseline configuration.
```

---

## Device capabilities

| Device | Role |
|---|---|
| Each instrument (×12) | Publishes measurement stream ~20 Hz; accepts configuration and start/stop from any peer |
| Each instrument | May request data or send commands to any other instrument (orchestrated experiment flows) |
| Workstation | Batch scripts coordinating multi-instrument sequences; not runtime master |

---

## Existing physical links

| Link | Type | Attachments |
|---|---|---|
| LabEth | Gigabit Ethernet switched | 12 instruments + workstation |

---

## Required interactions

| Interaction | Pattern | Notes |
|---|---|---|
| Measurement stream | Instrument → many subscribers | ~20 Hz × 12 sources |
| Directed command | Any peer → any peer | Start/stop, parameter set |
| Experiment orchestration | Workstation ↔ subsets | Scripts fan out commands |
| Time sync | All ↔ all (logical) | NTP or PTP — conventional, not WS-specific |

Peers are **symmetric** in capability; the workstation is convenience, not a permanent plant authority.

---

## Failure and redundancy assumptions

- Loss of one instrument: experiment degrades; others continue if script allows.
- Switch loss: entire rack offline.
- No hot standby instruments in baseline.

---

## Bandwidth and timing constraints

- Aggregate telemetry ~240 messages/s — trivial on GbE.
- Latency-sensitive handshakes < 10 ms on LAN.
- Conventional: fixed UDP ports or service discovery per instrument.

---

## Expected diagnostic outcomes

A healthy architecture evaluation may conclude:

```text
"Flat Ethernet peer mesh maps naturally with full canonical addressing (CAN29-equivalent
 richness or native Ethernet WS profile); CAN11 VCN is N/A; forcing CAN11 would be wrong."
```

Or:

```text
"WS Wire decomposition adds little over IP sockets for this class — document as weak-fit archetype."
```

Both are valid **boundary** results.

---

## What this archetype does not include

- CAN buses
- Gateways
- Hierarchical machine cell
- Cloud connectivity

---

## Mapper note

**CAN11 VCN fit** should almost certainly be **N/A**. Question A (Logical Bus / Participant model) remains in scope for Ethernet.

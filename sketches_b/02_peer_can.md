# Sketch 02 — Four Peer ECUs on Classical CAN

**Intent:** Stress the single-Origin Wire model with a naturally peer-oriented CAN system in which every ECU publishes state and several ECUs may directly command one another.

This is deliberately not a gateway or master/slave system. The purpose is to preserve the peer relationships honestly, identify the minimum workable WireSpaces mapping, and record where native CAN remains simpler.

The example assumes the stable capabilities listed in [`README.md`](README.md). CAN commissioning exists but its pre-addressing narrative is intentionally omitted.

---

# Configuration A — Symmetric peer CAN

## 1. What changed and maturity

The system has one committed 11-bit Classical CAN bus and four independently useful ECUs:

- **Left Drive ECU** — controls and reports the left drive;
- **Right Drive ECU** — controls and reports the right drive;
- **Tool ECU** — controls a toolhead and requests coordinated vehicle motion;
- **Power ECU** — manages power limits and may request load shedding.

All four publish state and faults. Several interactions are peer-addressed:

- either drive may ask the other drive to enter a synchronized or degraded mode;
- the Tool ECU may command both drives to hold position;
- the Power ECU may command any peer to reduce load;
- any ECU detecting an immediate hazard may broadcast an emergency-stop event.

No ECU is a system coordinator. Power management, motion, and tool operation are separate authorities.

**Maturity level: Level 2.** Four overlapping named Wires and their CAN aliases are small enough to generate, but they should be repeatable and statically reviewed rather than reconstructed differently on each boot.

## 2. Native communication model

Without WireSpaces terminology:

- Every ECU periodically places its current state on CAN.
- Any interested ECU may consume another ECU's state.
- Each message has one physical producer, even when several peers consume it.
- Commands name a target ECU or are broadcast to a defined recipient set.
- A command response returns to the command producer.
- There is no bus master and no normal-operation role election.
- If one ECU fails, the other three continue to use the same CAN bus and retain their own authorities.
- CAN arbitration priority is assigned by message criticality, not by which ECU is considered “main.”

The natural conceptual objects are message classes, producers, optional destinations, and CAN priorities. The system is one physical broadcast domain with several independent authorities.

## 3. Obvious conventional implementation

> **Obvious conventional implementation:** allocate CAN identifiers by producer, message class, priority, and where needed destination; each ECU installs receive filters for the messages it consumes.

A conventional implementation still needs bounded RX/TX queues, freshness, source ownership, command validation, update compatibility, and a CAN-ID allocation table. It does not need a logical Origin or a separate communication-domain object for each producer.

The chosen WireSpaces mapping adds:

- four named Wires sharing one Physical Link;
- one Origin role per Wire;
- four overlapping membership lists;
- per-Wire NodeId interpretation;
- four CAN WireAlias assignments;
- Endpoint acceptance and transmit bindings generated from the same Wiring source.

There is no gateway, forwarding table, or splice.

## 4. WireSpaces mapping

### 4.1 Chosen mapping: one authority Wire per ECU

The minimum mapping that preserves direct peer command authority without inventing a coordinator is:

> **Each ECU is Origin of one Wire carrying the state, events, and commands that ECU authors. The other ECUs are Nodes on that Wire where they consume or answer that traffic.**

```text
LeftDriveAuthorityWire
    Origin: Left Drive
    Nodes:  Right Drive, Tool, Power

RightDriveAuthorityWire
    Origin: Right Drive
    Nodes:  Left Drive, Tool, Power

ToolAuthorityWire
    Origin: Tool
    Nodes:  Left Drive, Right Drive, Power

PowerAuthorityWire
    Origin: Power
    Nodes:  Left Drive, Right Drive, Tool

All four Wires use the same committed Classical CAN Physical Link.
```

This is not one Wire per message type or per telemetry producer. It is one Wire per independent peer-command authority. In this example every ECU has that authority, so each ECU publishes state and emits commands on its own Wire:

- state/event publication is `OriginToNode` broadcast;
- a command is `OriginToNode` unicast or broadcast;
- the selected recipient replies `NodeToOrigin` on the same Wire.

Every ECU is therefore Origin on one Wire and Node on three. Roles are stable even though the devices are physical peers (`CORE §3.2`).

### 4.2 Device and membership matrix

| Device | Endpoint Domains | Link interfaces | Own Wire role | Other Wire roles |
|---|---|---|---|---|
| Left Drive ECU | One MCU Domain | committed CAN | Origin, LeftDriveAuthorityWire | Node on RightDrive, Tool, and Power Wires |
| Right Drive ECU | One MCU Domain | committed CAN | Origin, RightDriveAuthorityWire | Node on LeftDrive, Tool, and Power Wires |
| Tool ECU | One MCU Domain | committed CAN | Origin, ToolAuthorityWire | Node on LeftDrive, RightDrive, and Power Wires |
| Power ECU | One MCU Domain | committed CAN | Origin, PowerAuthorityWire | Node on LeftDrive, RightDrive, and Tool Wires |

Preferred NodeIds remain device-stable where each ECU is a Node:

```text
Left Drive     NodeId 1
Right Drive    NodeId 2
Tool           NodeId 3
Power          NodeId 4
```

The exception is structural: a device has no NodeId on the Wire where it is Origin. Thus `NodeId 1` is a useful deployment convention for Left Drive, not a universal identity carried in every Left Drive PDU (`DEPLOY §1.8`).

### 4.3 Wires

| Wire (name / #) | Origin | Nodes | Physical realization | Notes |
|---|---|---|---|---|
| LeftDriveAuthorityWire / assigned | Left Drive | Right (2), Tool (3), Power (4) | Shared committed CAN | Left state, faults, commands, and replies to Left |
| RightDriveAuthorityWire / assigned | Right Drive | Left (1), Tool (3), Power (4) | Shared committed CAN | Right state, faults, commands, and replies to Right |
| ToolAuthorityWire / assigned | Tool | Left (1), Right (2), Power (4) | Shared committed CAN | Tool state and tool-authored requests/commands |
| PowerAuthorityWire / assigned | Power | Left (1), Right (2), Tool (3) | Shared committed CAN | Power state, limits, and load-shed commands |

All four are logical Wires over one Physical Link; there is not one CAN controller or queue per Wire.

### 4.4 CAN alias allocation

One valid generated allocation is:

```text
alias 0 -> LeftDriveAuthorityWire
alias 1 -> RightDriveAuthorityWire
alias 2 -> ToolAuthorityWire
alias 3 -> PowerAuthorityWire
```

The alias-0 choice has no application meaning. It merely selects one fully populated logical Wire as the configured identity of the CAN Link's native physical Wire (`CORE §5.2`). Choosing Left Drive rather than another ECU is representationally arbitrary and should not be presented as coordinator status.

The mapping consumes four of the eight alias codes in the developed CAN profile. Up to eight peer-authority Wires could consume alias 0 plus all seven named aliases; a ninth peer or any additional Wire would not fit this profile.

### 4.5 Primary interactions

| Interaction | Producer | Consumer(s) | Wire | Direction | Notes |
|---|---|---|---|---|---|
| Left drive state | Left Drive | interested peers | LeftDriveAuthorityWire | OriginToNode broadcast | Snapshot-style periodic state; unreliable/e2e datagram with freshness |
| Right drive state | Right Drive | interested peers | RightDriveAuthorityWire | OriginToNode broadcast | Same schema as left state; different producer Wire |
| Tool state | Tool | interested peers | ToolAuthorityWire | OriginToNode broadcast | Snapshot-style periodic state |
| Power state/limits | Power | interested peers | PowerAuthorityWire | OriginToNode broadcast | Consumers reject stale limits |
| Enter synchronized mode | Left Drive | Right Drive | LeftDriveAuthorityWire | OriginToNode unicast | Left is the command author; side-effect semantics require duplicate handling |
| Reciprocal synchronization request | Right Drive | Left Drive | RightDriveAuthorityWire | OriginToNode unicast | Symmetric interaction uses the other authority Wire |
| Hold position | Tool | Left and Right Drives | ToolAuthorityWire | OriginToNode unicasts | Two addressed PDUs unless Service semantics define one broadcast recipient set |
| Load-shed command | Power | selected ECU or all peers | PowerAuthorityWire | OriginToNode | E2E-protected datagram may be appropriate; freshness remains required |
| Command response | target ECU | command author | Author's Wire | NodeToOrigin | Request-scoped reply; no reverse authority inferred beyond the registration |
| Fault event | any ECU | interested peers | Producer's Wire | OriginToNode broadcast | Queue semantics; every event matters |
| Emergency stop | any ECU | all other ECUs | Producer's Wire | OriginToNode broadcast | Receivers must be configured to consume the emergency Endpoint on all four Wires |

The emergency-stop row exposes the core cost. The application has one semantic event with four legitimate producers, but the WS mapping expresses it as the same Service interaction admitted from four source Wires. That is workable and preserves lineage, but less direct than one native CAN message family with producer identity in the identifier or payload.

### 4.6 Endpoint and storage consequences

No Endpoint IDs are allocated in this sketch, but three storage choices matter:

- periodic peer state is latest-value data and naturally wants per-source Snapshot semantics;
- fault and emergency-stop events require Queue semantics;
- command requests and replies require source metadata or opaque request-scoped reply contexts.

A single Snapshot receiving the same state Endpoint from several peers would coalesce different producers, not merely newer values from one producer. A real Service mapping must therefore provide per-source latest-value storage or separate topology-relevant Endpoint allocations. Hiding that issue in one global “latest peer state” Snapshot would be incorrect (`CORE §9.5`).

Per-source latest-value state is not uniquely a WireSpaces cost; a raw-CAN application consuming Left and Right state must distinguish those producers too. The WS-specific question is whether portable Service wiring can express that state and multi-Wire acceptance without exposing several registrations or Endpoint objects to application code.

### 4.7 Failure behavior

- Failure of one ECU removes one Origin and its authored traffic; it does not stop the other three Wires or the CAN Link.
- No remaining ECU becomes Origin of the failed ECU's Wire automatically.
- A command that depended on the failed ECU receives no reply or fresh state according to its Service policy.
- CAN bus failure affects all four Wires together because they share one Physical Link. The separate Wires do not imply physical fault isolation.
- Duplicate or late safety commands remain a Service/Transport concern; the Router has no duplicate-suppression state.

## 5. Rejected and comparison mappings

### 5.1 One nominated-Origin PeerWire

```text
PeerWire
    Origin: Power ECU       <- nominated
    Nodes: Left, Right, Tool
```

This is attractive for telemetry:

- Nodes publish `NodeToOrigin`;
- all other Nodes may be configured to consume those publications by observation;
- the Power ECU publishes its own state `OriginToNode`;
- only one Wire and one CAN alias are required.

It fails on ordinary peer-addressed control. Left Drive cannot address Right Drive because both are Nodes. The available repairs are all visible changes to the natural model:

- add a Left-origin Wire;
- ask Power to consume and re-originate the command;
- broadcast NodeToOrigin traffic and make Right consume something not addressed to it;
- change the Service into an indirect request to Power.

Power was selected only to satisfy the one-Origin invariant. It has no native authority over drive synchronization or tool motion, so the nominated Origin is **Significant artificial structure**.

### 5.2 One Wire per directed peer relationship

Four peers permit as many as twelve directed command relationships:

```text
Left -> Right
Right -> Left
Tool -> Left
Tool -> Right
Power -> Left
...
```

Pairwise Wires make each command direction structurally exact, but they turn a four-device bus into a large edge matrix, exceed the developed CAN profile's named-alias capacity quickly, and duplicate membership/configuration for interactions that share the same producer authority. One authority Wire per producer is the smaller faithful mapping.

### 5.3 Re-originating coordinator

The Power ECU or another component could receive every peer request and emit a new command as the Wire Origin. That makes one Wire possible, but it:

- invents a runtime coordinator;
- adds a failure bottleneck;
- changes the authoritative producer identity;
- turns forwarding into a transformation Service;
- makes peer operation depend on a component the native system did not require.

It is rejected rather than hidden inside a CAN gateway or Router entry (`CORE §12.7`).

### 5.4 Raw CAN ID matrix

Raw CAN is the strongest conventional alternative for this exact topology:

| Concern | Raw CAN | Chosen WS mapping |
|---|---|---|
| Producer | CAN-ID allocation and controller ownership | Origin of producer's authority Wire |
| Destination | CAN ID, payload field, or message-specific convention | OriginToNode NodeId |
| Broadcast | Native shared-bus visibility plus filters | NodeId 0 within one Wire |
| Reply | Separate response CAN ID/correlation convention | NodeToOrigin on requester's Wire |
| Peer symmetry | Direct | Four asymmetric Wire roles, rotated symmetrically |
| Configuration | CAN-ID/filter table | Service schema plus Wires, aliases, memberships, and bindings |
| Cross-Link growth | Requires a gateway protocol/mapping | Canonical PDU forwarding is already defined |

For a closed four-ECU system that will remain one CAN bus, raw CAN has fewer conceptual objects. WireSpaces earns back some complexity only if the common Service model, tooling, host access, or later heterogeneous forwarding is valuable to the product.

The result is narrower than “peer CAN is a poor fit”:

> **WireSpaces represents this small peer-CAN system cleanly by spending one Wire and CAN alias per independent peer-command authority. Raw CAN remains simpler when the system will always remain one flat CAN network.**

## 6. Friction signals

Ratings apply to the chosen four-authority-Wire mapping, not to the rejected nominated-Origin mapping.

| Signal | Rating | Reason |
|---|---|---|
| Artificial Origin | None | Each Wire's Origin is the real author of the traffic carried on that Wire |
| Artificial Wire | Mild | Producer authority is real, but native CAN normally expresses it through identifiers rather than four named buses |
| Wire proliferation | Mild | Four regular authority Wires are easy to inspect and generate for this system; the separate eight-alias ceiling limits scaling |
| Forwarding tax | None | There is one Physical Link and no gateway |
| Identity awkwardness | Mild | Origin identity replaces NodeId on one Wire, but stable device identity and conventional NodeIds on the other Wires keep the participant unambiguous |
| Interaction awkwardness | Mild | Symmetric Services span several authority Wires, but generated multi-Wire bindings can hide that repetition; this becomes Significant only if the portable Endpoint API exposes it as manual registrations or dispatch |
| Configuration burden | Mild | Four symmetric Wires are still auditable and generator-friendly, but are materially richer than a CAN-ID/filter table |
| Role instability | None | Every ECU's per-Wire roles are fixed; physical peer equality does not require runtime role changes |
| Failure mismatch | Mild | Separate logical Wires correctly isolate producer authority, but all still fail together with the one CAN bus |

The CAN alias ceiling is a profile placement limit, not a friction rating for the four-node happy path. This mapping can represent at most eight peer-authority Wires by consuming alias 0 and all seven named aliases. A ninth authority requires a different decomposition or Link profile even if tooling makes the first eight effortless.

## 7. Model pressure

- **If one WireSpaces concept could change:** no protocol concept should change yet. The current model handles this four-peer system without weakening Direction or addressing semantics. If implementation later shows that multi-Wire Service bindings or the eight-alias ceiling block real systems, a peer-addressed Wire profile would be the candidate to investigate — but that would be a second fundamental network model and needs substantially stronger evidence.
- **Useful distinction exposed by WS:** the chosen mapping makes producer authority and re-origination explicit. A coordinator cannot silently relay a peer command while preserving the original producer identity, and request-scoped reply authority is narrower than “I saw a frame from that ECU.”

## 8. Open questions

- Do real peer systems exceed eight independent command authorities often enough to justify profile work, or is raw CAN intentionally the better tool beyond this scale?
- Can NodeId evolve into a stable participant identity independent of Origin/Node role without breaking the compact Direction semantics?
- What is the portable Endpoint shape for per-source latest-value state without forcing application code to manage one Endpoint allocation per producer?
- Can a multi-producer emergency Service be expressed as one generated multi-Wire receive binding that flattens to ordinary Endpoint registrations?
- Does selecting one authority Wire as CAN alias 0 create unacceptable tooling confusion even when the choice has no protocol semantics?
- Between five and eight peers, does configuration readability become unacceptable before alias capacity is exhausted? A ninth peer cannot fit one authority Wire per peer in the current profile.
- How should tooling compare the safety and priority consequences of two targeted CAN frames versus one broadcast command when both are valid Service encodings?


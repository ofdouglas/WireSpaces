# WireSpaces — Link-Level Polling

**Status:** Idea note; provisional
**Purpose:** Capture how master-initiated Links such as I2C, SPI, and polled RS-485 compose with Wire-level Origin/Node semantics.

## Core idea

**Wire Origin and Link-level transfer initiation are separate concepts.**

A Wire's Origin is a **semantic role**: it defines the Origin/Node relationship and PDU Direction.

A Link controller or poll scheduler is a **local transport role**: it initiates whatever physical transactions are required to move Wire PDUs across that Link.

Therefore:

```text
Wire Origin ≠ necessarily the controller/master of every Physical Link
```

A single Wire may span several locally-mastered Links without creating additional Wire Origins. This is required for useful composition across I2C, SPI, polled RS-485, and similar Links. 

## Example

```text
Wire SensorField
  Origin: Main ECU
  Nodes: 1..8

Main ECU
  RS-485 Link A
  local poll scheduler → Nodes 1..4

Field gateway
  forwards SensorField
  RS-485 Link B
  local poll scheduler → Nodes 5..8
```

The field gateway controls Link B electrically, but it is **not** the Wire Origin.

When Node 7 is polled, the resulting canonical PDU remains:

```text
Wire: SensorField
Source: Node 7
Direction: NodeToOrigin
```

The gateway's poll did not create a Service-level request. It only performed the physical operation required to carry Node 7's publication.

The same pattern applies naturally to:

* I2C controllers polling downstream devices
* SPI controllers selecting downstream peripherals
* half-duplex RS-485 masters
* other Links where Nodes cannot autonomously transmit

## Endpoint interaction

This fits especially well with Snapshot transmit Endpoints:

```text
Service:
    publishes current state

Link scheduler:
    decides when the local medium permits transfer

Wire:
    preserves producer identity and semantic Direction
```

A Node may update its Snapshot independently of the poll cadence. When the local Link controller polls it, the LLL retrieves the current value and transports it toward the Wire Origin.

Queue/event Endpoints can also work, but polling must be fast enough to prevent queue overflow.

## Configuration

A polling Link needs a local projection describing at least:

```text
Wire / Node → local Link address
Endpoint or traffic class → poll cadence
```

Examples of local addressing:

```text
Wire Node 7 → I2C address 0x48
Wire Node 7 → SPI chip-select 2
Wire Node 7 → RS-485 local address 3
```

Link-local addresses are implementation details and should remain separate from Wire NodeIds.

A gateway spanning two polled Links may therefore have its own poll schedule for downstream Nodes while forwarding the same canonical Wire.

## Consequences

Polling introduces real service constraints:

* freshness is bounded partly by poll cadence;
* QoS cannot overcome a slow polling schedule;
* downstream Links may have different cadences;
* failures must distinguish Node silence from Link/controller faults;
* Queue traffic requires sufficient poll capacity to avoid overflow.

Arrival time alone does not necessarily describe sample age. A Service may need sample timestamp or generation metadata when measurement freshness matters.

## Invariant

A useful boundary is:

> **A Link controller may initiate a physical transfer, but doing so does not create or change the semantic Wire operation.**

Local scheduling determines **when and how** a PDU can move.

The Wire determines **who it belongs to and what Direction it has**.

## Open questions

* How are poll schedules represented in deployment configuration?
* Are schedules per Node, per Endpoint, or grouped by traffic class?
* How should capacity tooling validate Queue depth against worst-case poll intervals?
* How should Link telemetry distinguish downstream Node failure from poll-controller or Link failure?
* Can some polling profiles support dynamic cadence changes without changing Wire configuration?

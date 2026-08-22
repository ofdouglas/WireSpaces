# Runtime CAN ID Provisioning by Anonymous Binary Search

## Overview

WireSpaces CAN nodes may be commissioned at runtime using a CAN-native discovery mechanism based on **anonymous identical responses** and binary search over a permanent 64-bit `DeviceId`.

Multiple unconfigured nodes may simultaneously transmit the same `DLC = 0` frame. Because the complete CAN bitstreams are identical, this is non-destructive and appears on the bus as one valid frame.

This provides a primitive equivalent to:

> “Does at least one unconfigured node matching predicate P exist?”

## CAN Identifier Layout

The 11-bit WireSpaces CAN identifier is ordered to preserve intended arbitration priority:

```text
QoS (2) | NodeId (5) | WireAlias (3) | Direction (1)
 MSB                                      LSB
```

`NodeId = 31` is reserved as `Broadcast`.

This placement guarantees that within a QoS class, any ordinary `NodeId` wins arbitration over `Broadcast`, regardless of `WireAlias` or `Direction`.

## Commissioning Channel

Commissioning uses an otherwise-forbidden routing combination:

```text
QoS       = Background
NodeId    = Broadcast (31)
Direction = NodeToOrigin
```

This gives commissioning traffic the lowest normal arbitration priority:

1. Higher QoS classes win.
2. Ordinary Background-QoS node traffic wins.
3. Broadcast Background traffic is considered afterward.

The `WireAlias` field may distinguish commissioning message types or subchannels. Exact encodings remain profile-defined.

Unconfigured nodes must not transmit normal WireSpaces application traffic.

## Device Identity

Each physical device has a permanent:

```text
DeviceId: uint64_t
```

`DeviceId` is the canonical persistent device identity used by commissioning and deployment tooling.

It must be globally unique by provisioning or manufacturing policy. Duplicate `DeviceId`s cannot be distinguished by this protocol and are therefore outside its resolvable fault model.

This avoids requiring a separate 128-bit UUID plus shortened discovery identity.

## Discovery Process

1. The Organizer asks whether any unconfigured nodes exist.
2. All unconfigured nodes respond with the same anonymous `DLC = 0` presence frame.
3. The Organizer searches the 64-bit `DeviceId` space by prefix.
4. A query asks whether any unconfigured node matches a prefix such as `0`, `01`, or `011`.
5. Every matching node transmits the same anonymous presence response.
6. Silence means no matching node exists.
7. After resolving all 64 bits, exactly one `DeviceId` is selected.
8. The Organizer sends configuration addressed to that `DeviceId`.
9. The selected node stages and confirms the configuration.
10. Once committed, it stops participating as an unconfigured node.
11. Discovery repeats until no unconfigured nodes remain.

The full `DeviceId` does not need to be transmitted during binary search; the query history identifies it.

## Commissioning State Mapping

```text
Unconfigured
    Participates in anonymous DeviceId discovery.

Selected
    Organizer has isolated one complete DeviceId.

Staged
    Selected device has received proposed Wire configuration.

Committed
    Configuration is accepted and normal WireSpaces traffic is permitted.
```

Commissioning assigns Wire-specific configuration such as `WireNumber`, `WireAlias`, and `NodeId`; it does not assign a global runtime device address.

A physical device may participate in multiple Wires and therefore may hold different Wire-specific routing configuration for each.

## Identical-Frame Exception

The normal CAN collision warning must distinguish two cases:

* Same CAN ID, **different bitstreams**: invalid contention may cause errors and retries.
* Same CAN ID, **identical bitstreams**: simultaneous transmission is valid and intentionally used for anonymous presence responses.

The commissioning mechanism relies only on the second case.

## Remaining Profile Details

The CAN commissioning profile still needs to define:

* exact `WireAlias` commissioning subtype assignments;
* query payload encoding;
* assignment and confirmation formats;
* timeout and retry behavior;
* Organizer restart and partial-commissioning recovery;
* conformance vectors for discovery and commissioning state transitions.

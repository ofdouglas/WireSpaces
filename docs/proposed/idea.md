# 2. Canonical PDU descriptor

The ordinary canonical descriptor is **48 bits / 6 bytes**.

| Field | Bits |
|---|---:|
| QoS | 2 |
| Reserved | 2 |
| HasHeaderExtensions | 1 |
| TransportType | 3 |
| WireNumber | 8 |
| SrcParticipantId | 8 |
| DestParticipantId | 8 |
| Endpoint | 16 |
| **Total** | **48** |

The intended serialized shape is byte-oriented:

```text
Byte 0      Control
Byte 1      WireNumber
Byte 2      SrcParticipantId
Byte 3      DestParticipantId
Bytes 4-5   Endpoint, little-endian
```

with:

```text
Control
    QoS                    2
    Reserved               2
    HasHeaderExtensions    1
    TransportType          3

Endpoint
    Namespace              2
    Id                    14
```

## 8.1 Classical CAN 11-bit

CAN11 is intentionally a constrained compatibility profile. It uses asymmetric participant compression plus a Link-local Direction bit to reconstruct canonical source/destination.

### WS-guest CAN11 Idea

```text
VirtualCircuitNumber    3        
Direction               1
-------------------------
                        4
```

`VirtualCircuitNumber` is statically translated by the LLL into: {WireNumber, SourceId, DestId} (8/8/8). Broadcast is possible by setting DestId = kBroadcast.
QoS is omitted because 1) there is no room for it, 2) guest use is expected to be for experimentation and/or non-critical services; a fixed QoS chosen by the user (by deciding which of the 11 ID bits go to WS) is adequate for such use cases.


### WS-Native CAN11 VCN Compression Idea

```text
QoS                     2
WireAlias               3  // defaults to 0, LocalBus
VirtualCircuitNumber    5  // default mapping could be simple: 0="Origin" broadcast to all, 1="Node1-Origin", 2="Node2-Origin"
Direction               1
-------------------------
                        11
```



### WS-exclusive CAN11 Idea

```text
QoS                     2
WireIndexA              3
WireIndexB              5
Direction               1
-------------------------
                       11
```

How to use the compression:
* By default, compression is disabled. WireIndexA/B map to the usual source and destination PIDs. This globally limits the number of CAN nodes to ~31.
* To use compression, define a wire associated with the CAN bus, and provide a static mapping of PIDs -> WireIndex. Provide the CAN LLL with the mapping and enable the compression feature.
* The LLL translates WireIndex <-> PID and adds/removes the Wire number.
* With compression on, you can have up to 254 CAN nodes globally.
* 29-bit CAN IDs are recommended over compression for complex CAN networks
* Note that with this compression, each CAN bus maps to at most one Wire.




Default 5-bit VCN Mapping:
```text
0       MainA broadcast
1       MainB broadcast
2       MainA-MainB
3       reserved (commissioning?)
4       Node0 - MainA
5       Node0 - MainB
N,N+1   NodeX - MainA/B
-------------------------
```
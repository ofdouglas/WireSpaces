# Archetype 07 sketches — index

Two mapping results for different scales of the same negative-control archetype:

| File | Scale | Ordinary VCNs (min) | Notes |
|---|---|---|---|
| [`07_peer_can_dense_4p_cursor.md`](07_peer_can_dense_4p_cursor.md) | 4 ECUs (1 domain/corner) | 10 / 32 | Historical scale; revised mild assessment |
| [`07_peer_can_dense_8d_cursor.md`](07_peer_can_dense_8d_cursor.md) | 8 domains (dual-core/corner) | 14 / 32 | Current archetype; stress boundary at ~36 if 8-node mesh |

**Shared conclusion:** R6 strong pass; default VCN does not fit node↔node peers; explicit custom map is modest at required interaction scale; CAN29 preference-dependent until alias overflow (~36 VCNs).

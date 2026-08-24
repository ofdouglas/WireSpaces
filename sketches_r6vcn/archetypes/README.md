# Neutral archetypes — R6+VCN trial

Problem statements for mapping agents. **No WireSpaces mappings** — physical topology and interactions only.

| File | ID | Convergence? |
|---|---|---|
| `01_simple_can_machine.md` | 01 | No |
| `02_pi_robot.md` | 02 | **Yes** |
| `03_dual_controller_can.md` | 03 | No |
| `04_multi_can_gateway.md` | 04 | No — dual-core gateway, 3× CAN |
| `05_heterogeneous_gateway.md` | 05 | **Yes** |
| `06_overlapping_wire_bandwidth.md` | 06 | No |
| `07_peer_can_dense.md` | 07 | **Yes** — 8 domains; peer negative control |
| `08_ethernet_peer_mesh.md` | 08 | No |
| `09_distributed_chassis_can_cell.md` | 09 | **Yes** — ~29 VCN production; bring-up alias overflow |
| `10_automotive_zone_core_slice.md` | 10 | **Yes** — heterogeneous automotive integration |
| `11_redundant_evtol_flight_control.md` | 11 | **Yes** — physical redundancy / loop-free-Wire stress |
| `12_modular_bess_field_serviced.md` | 12 | **Yes** — identity, commissioning, lifecycle adversary |
| `13_cyclic_motion_and_streams.md` | 13 | **Yes** — cyclic motion, synchronized process image, and high-rate stream negative control |
| `14_mobile_warehouse_opportunistic_mesh.md` | 14 | **Yes** — mobility, partitions, dynamic neighborhood negative control |
| `15_submillisecond_redundant_motion_cell.md` | 15 | **Yes** — deterministic cyclic/process-image negative control |
| `16_intentional_partial_networking.md` | 16 | **Yes** — selective sleep, Link availability, wake, proxy/cache, and power-budget adversary |

Assign exactly one archetype per mapping agent unless running a convergence duplicate marked above.

See [`../experiment_brief_r6_vcn.md`](../experiment_brief_r6_vcn.md) §4.

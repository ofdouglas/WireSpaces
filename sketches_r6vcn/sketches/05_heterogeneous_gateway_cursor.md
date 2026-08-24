 # Sketch — Archetype 05: Heterogeneous Gateway (CAN + UART + Shared Memory + Linux) — rev 2
 
 ```text
 **Archetype:** archetypes/05_heterogeneous_gateway.md
 **Agent:** cursor
 **Date:** 2026-08-23 (rev 2)
 ```
 
 ---
 
 ## Executive summary
 
 ```text
 **Minimum mapping:** 15 Participants, 6 Logical Buses (W1–W3 plant scopes spanning InterCore + each fieldbus; W4 PlantNet, W5 CellLink, W6 GatewayMgmt inter-core-only).
 **Question A (R6):** Natural.
 **Question B (CAN11):** Default (with corrected MainA assignment).
 **Worst friction (minimum path):** Configuration burden — Moderate.
 **Main lesson:** VCN names canonical participant relations (not CAN controller ownership); Core0 must be MainA for PlantCAN/AuxCAN VCNs when it is the canonical sequencer source. Management traffic belongs on a device-local Wire (W6) to avoid polluting fieldbus propagation.
 ```
 
 ---
 
 ## Disposition block
 
 | Area | Assessment |
 |---|---|
 | Participant identity | Natural |
 | Wire decomposition | Natural |
 | Forwarding | Moderate |
 | Non-CAN configuration | Moderate |
 | CAN11 VCN fit | Default (with MainA = Core0) |
 | Better with CAN29? | N/A |
 
 **Explanation:** Question A: dual-core separation into two Endpoint Domains remains natural; W1–W3 as scope-shaped Wires that span InterCore + fieldbus are correct. Question B: committed CAN11 default VCN map suffices, but the sketch's earlier MainA assignment conflated LLL ownership with VCN endpoints — corrected so MainA = Core0 (0x01). Management heartbeat and Core1-specific telemetry move to a device-local W6 to avoid leaking control/management onto fieldbuses.
 
 ---
 
 ## 1. Native communication model
 
 Same as archetype: Core0 sequencer, Core1 fieldbus engine, shared memory between cores, PlantCAN, AuxCAN, SensorRS485, EthPlant, CellLink, SCADA, service laptop.
 
 Key architectural point reiterated: canonical `SrcParticipantId` is independent of which core physically drives the CAN controller. Forwarding preserves canonical Src/Dest; VCN reconstruction on ingress must reflect that canonical pair.
 
 ---
 
 ## 2. Obvious conventional implementation
 
 Same summary as rev1: mailbox/RPMsg between cores, per-bus CAN ID tables, RS-485 register maps, ad hoc mirroring to SCADA. WireSpaces makes these explicit and separates scope, membership, and authority.
 
 ---
 
 ## 3. Minimum mapping (required first)
 
 ### 3.1 Participants
 
 | ParticipantId | Endpoint Domain | Device | Notes |
 |---|---|---|---|
 | 0x01 | GatewayCore0 | Core0 (supervisor) | Sequencer; EthPlant + CellLink owner; composes summaries; canonical authority for sequencer PDUs |
 | 0x02 | GatewayCore1 | Core1 (fieldbus engine) | CAN + RS-485 LLLs; egress LLL owner/forwarder |
 | 0x03 | DriveInvA | PlantCAN inverter 1 | Torque command target |
 | 0x04 | DriveInvB | PlantCAN inverter 2 | Torque command target |
 | 0x05 | ValveCtrl | AuxCAN valve | Aux command target |
 | 0x06 | PumpCtrl | AuxCAN pump | Aux command target |
 | 0x07 | Sensor1 | RS-485 sensor 1 | Polled slave |
 | 0x08 | Sensor2 | RS-485 sensor 2 | Polled slave |
 | 0x09 | Sensor3 | RS-485 sensor 3 | Polled slave |
 | 0x0A | Sensor4 | RS-485 sensor 4 | Polled slave |
 | 0x0B | CellGateway | Downstream cell gateway | CellLink handshake partner |
 | 0x0C | RemoteNodeA | Cell remote node 1 | Setpoint peer |
 | 0x0D | RemoteNodeB | Cell remote node 2 | Setpoint peer |
 | 0x0E | SCADA | SCADA HMI | Read-only observer (receives composed summaries) |
 | 0x0F | ServiceLaptop | Service laptop | Maintenance; may be offline; may use splice for preserved-Src maintenance flows |
 
 ### 3.2 Wires
 
 | Wire (#) | Participants | Physical links | Purpose |
 |---|---|---|---|
 | W1 PlantDrive | 0x01, 0x02, 0x03, 0x04 | InterCore + PlantCAN | Drive torque commands and inverter status (~100 Hz) |
 | W2 AuxCtrl | 0x01, 0x02, 0x05, 0x06 | InterCore + AuxCAN | Valve/pump commands and status (~50 Hz) |
 | W3 Sensors | 0x01, 0x02, 0x07–0x0A | InterCore + SensorRS485 | Polled sensor reads (~10 Hz) |
 | W4 PlantNet | 0x01, 0x0E | EthPlant | SCADA receives composed plant summaries authored by Core0; maintenance endpoint access |
 | W5 CellLink | 0x01, 0x0B, 0x0C, 0x0D | CellLink (Ethernet) | Cell setpoints and handshake |
 | W6 GatewayMgmt | 0x01, 0x02 | InterCore only | Heartbeat, Core1 health, link telemetry transport; never propagated to fieldbuses |
 
 Adding W6 keeps management scope device-local and prevents low-rate management PDUs from becoming fieldbus traffic.
 
 ### 3.3 Interactions (primary)
 
 | Interaction | Src | Dest | Wire | Broadcast? | Notes |
 |---|---|---|---|---|---|
 | Drive torque command | 0x01 | 0x03, 0x04 | W1 | No | ~100 Hz; Core0 is canonical Src; Core1 forwards on PlantCAN preserving Src=0x01 |
 | Drive status | 0x03, 0x04 | 0x01 | W1 | No | Return path; freshness ≤ 5 ms incl. InterCore |
 | Aux commands | 0x01 | 0x05, 0x06 | W2 | No | ~50 Hz |
 | Aux status | 0x05, 0x06 | 0x01 | W2 | No | |
 | Sensor poll request | 0x02 | 0x07–0x0A | W3 | No | Core1 master-initiated (`CORE §1.7`); poll cadence bounds latency |
 | Sensor sample delivery | 0x07–0x0A | 0x01 | W3 | No | Responses forwarded Core1→Core0; payload includes sample timestamp |
 | Cell setpoints | 0x01 ↔ 0x0B, 0x0C, 0x0D | W5 | No | Event + periodic |
 | SCADA plant summary | 0x01 | 0x0E | W4 | No | Composed periodic snapshot authored by Core0 (reduces bandwidth; preserves fieldbus privacy) |
 | Service FW update (preserve laptop Src) | 0x0F → target | field PID | W4 --splice--> W1/W2/W3 | No | Explicit **splice** configured (W4->W1) when laptop must be canonical Src; ingress classification on PlantCAN reconstructs {0x0F, dest} |
 | Service FW update (composition alternative) | 0x0F → Core0 | W4 | No | Core0 may compose/update and re-author onto W1 with Src=0x01 (composition) if splice not configured |
 | Core0 supervisor telemetry | 0x01 | 0x0F | W4 | No | Identity, health, plant summary |
 | Core1 link telemetry | 0x02 | 0x01 | W6 | No | Transported on W6; Core0 composes or forwards as needed to W4 for remote maintenance |
 | Inter-core heartbeat | 0x02 | 0x01 | W6 | No | Low-rate liveness; does not go onto PlantCAN |
 
 Notes: the maintenance-preserved-Src path uses an explicit splice (W4→W1) to change Wire scope while preserving SrcParticipantId and PDU lineage (`CORE §6`). If the deployment prefers to keep laptop out of plant scope, Core0 composes/relays the update (Src changes to 0x01).
 
 ### 3.4 Forwarding (if any)
 
 | Ingress | Wire | Egress | Notes |
 |---|---|---|---|
 | Core1 InterCore | W1 | PlantCAN | Core1 egress encodes VCN for canonical {0x01, dest}; Core1 is LLL owner but not VCN endpoint |
 | Core1 PlantCAN | W1 | InterCore | |
 | Core1 InterCore | W2 | AuxCAN | |
 | Core1 AuxCAN | W2 | InterCore | |
 | Core1 InterCore / RS-485 | W3 | SensorRS485 / InterCore | Master poll egress; slave response ingress |
 | Core0 InterCore | W1/W2/W3 (observe) | W4 EthPlant | Core0 composes periodic summaries to W4 for SCADA (composition) |
 | Splice entry (config) | W4 | W1 | Configured spliceWire: W4 -> W1 (preserves Src=0x0F) for maintenance flows when needed |
 
 Forwarding preserves canonical `SrcParticipantId`/`DestParticipantId`; hence VCN must map canonical pairs (MainA = 0x01 when Core0 is sequencer). Splice entries change Wire representation at the egress boundary while preserving the canonical PDU (`CORE §6`).
 
 ### 3.5 Link profiles
 
 | Physical link | Profile | WS Wire(s) | Notes |
 |---|---|---|---|
 | InterCore | Shared-memory queue | W1, W2, W3, W6 | Full canonical descriptor; both cores terminate |
 | PlantCAN | Committed CAN11 | W1 | Core1 LLL encodes canonical {Src,Dest} via VCN map (MainA = 0x01) |
 | AuxCAN | Committed CAN11 | W2 | Core1 LLL; MainA = 0x01 |
 | SensorRS485 | UART/RS-485 master-initiated | W3 | Half-duplex; Core1 poll cadence bounds latency |
 | EthPlant | WS over Ethernet datagram | W4 | Plant switch segment |
 | CellLink | WS over Ethernet datagram | W5 | Downstream cell |
 
 ### 3.6 CAN11 bindings (if CAN11 used)
 
 #### PlantCAN — committed CAN11
 
 ```text
 Profile:          committed
 WireAliases used:             1 / 8
   default map:                  1
   custom map:                   0
 
 Per alias:
   Alias:                        1
   Canonical Wire:               W1 (PlantDrive)
   Mapping:                      Default
   Ordinary VCNs used:           2 / 32
   VCNs available, unused:       VCN 0 (MainA broadcast), VCN 2 (MainA↔MainB),
                                 odd MainB slots, Node2+ positions
   Default map sufficient?       yes
   Custom entries (if Explicit): 0
   MainA PID / MainB PID:         0x01 / (unassigned)
   Node positions used:          2 / 14
 ```
 
 **VCN assignments (default map, MainA = 0x01):**
 
 | VCN | Relation | Status in minimum mapping |
 |---|---|---|
 | 0 | 0x01 → kBroadcast | Available by default; **unused** |
 | 3 | — | Reserved Link control |
 | 4 | 0x03 ↔ 0x01 | **Used** — DriveInvA ↔ Core0 |
 | 6 | 0x04 ↔ 0x01 | **Used** — DriveInvB ↔ Core0 |
 
 #### AuxCAN — committed CAN11
 
 ```text
 Profile:          committed
 WireAliases used:             1 / 8
   default map:                  1
   custom map:                   0
 
 Per alias:
   Alias:                        1
   Canonical Wire:               W2 (AuxCtrl)
   Mapping:                      Default
   Ordinary VCNs used:           2 / 32
   VCNs available, unused:       VCN 0, VCN 2, odd MainB slots, Node2+ positions
   Default map sufficient?       yes
   Custom entries (if Explicit): 0
   MainA PID / MainB PID:         0x01 / (unassigned)
   Node positions used:          2 / 14
 ```
 
 | VCN | Relation | Status in minimum mapping |
 |---|---|---|
 | 4 | 0x05 ↔ 0x01 | **Used** — Valve ↔ Core0 |
 | 6 | 0x06 ↔ 0x01 | **Used** — Pump ↔ Core0 |
 
 **Guest CAN11:** Not used.
 
 **Membership vs presence:** Service laptop (0x0F) is preconfigured on W4. When offline, plant control on W1–W3 and W5 continues; W4 delivery to 0x0F fails or queues drain. W6 is preconfigured for inter-core management and always exists only inside the SoC.
 
 ---
 
 ## 4. Optional optimizations
 
 - Keep W6 and add an internal aggregated telemetry Wire only if Core1 telemetry semantics justify direct W4 exposure (splicing vs composition trade-off).
 - Overlapping status Wire on PlantCAN (W1b) for heavy telemetry only if measured bandwidth justifies a second alias/binding.
 - Composition alternative for FW update (Core0 re-authors) when deployment policy forbids laptop membership on W1.
 
 ---
 
 ## 5. Friction signals (minimum mapping happy path)
 
 | Signal | None / Mild / Significant | One-sentence justification |
 |---|---|---|
 | Artificial Wire | None | W1–W3, W4, W5, W6 each match a meaningful scope. |
 | Wire proliferation | Mild | Six Wires in a rich gateway system is reasonable and clarifies scope. |
 | Artificial hierarchy | None | MainA = Core0 follows canonical authority rather than physical CAN ownership. |
 | VCN pressure | None | Default map covers canonical relations used. |
 | WireAlias pressure | None | One alias per CAN bus. |
 | Configuration burden | Moderate | Explicit participant and Wire config reflects real system complexity rather than artificial splitting. |
 | Failure/topology mismatch | Mild | W6 isolates management; forwarding behavior and splice semantics must be documented for degraded states. |
 
 ---
 
 ## 6. Model pressure
 
 - **Change suggestion:** a documented “gateway splice vs composition” pattern in DEPLOY to guide when to expose upstream maintenance peers into plant Wires vs when to compose/re-author PDU. This is a tooling/documentation need, not a protocol change.
 - **Useful distinction exposed:** canonical-source continuity across shared-memory → LLL egress (Core0 remains Src even when Core1 owns CAN controller).
 
 ---
 
 ## 7. Open questions
 
 - Should splice activation (W4->W1) be gated by an explicit commission step and audited in manifests? (safety/robustness question)
 - For RS-485 sensors, prefer separate PIDs per sensor or an aggregated SensorAggregator PID on Core1 to reduce VCN/poll table count?
 - When Core1 is unreachable, what retry scope should FW update segmentation use across Core0/Core1 boundaries (composition vs forwarded reliability)?
 
 ---
 
 ## 8. Spec findings / synthesis note
 
 SF-R6-###: VCN endpoints can be remote from the physical LLL controller. Tooling and examples must emphasize that VCN positions (MainA/MainB/Node) name canonical Participant relationships on a Wire and must be assigned to the canonical initiators, not necessarily the link-owner. This sketch's correction is concrete evidence.
 
 ---
 
 ## 9. Diagrams (abbreviated)
 
 Device-centric (management separated):
 
 ```text
    EthPlant: SCADA(0x0E)  ServiceLaptop(0x0F)
                 | W4                |
                Core0(0x01) <--- W6 ---> Core1(0x02)
                    | W1/W2/W3 (InterCore spans to fieldbuses)
                    |            ├─ PlantCAN (0x03/0x04)
                    |            ├─ AuxCAN (0x05/0x06)
                    |            └─ RS-485 sensors (0x07..0x0A)
 ```
 
 Wire-centric (high level):
 
 ```text
 W1 PlantDrive       MainA=0x01, Node0=0x03, Node1=0x04
 W2 AuxCtrl          MainA=0x01, Node0=0x05, Node1=0x06
 W3 Sensors          Core1 poll master
 W4 PlantNet         Core0 authors summaries; splice optional for maintenance-preserved-Src
 W5 CellLink
 W6 GatewayMgmt      Core0/Core1 inter-core only
 ```
 
 --- 
 
 ## Devices (reference)
 
 | Device | Endpoint Domains | Link interfaces | Role summary |
 |---|---|---|---|
 | Gateway SoC Core0 | GatewayCore0 | InterCore, EthPlant, CellLink | Sequencer; canonical Src; composes summaries; splice controller |
 | Gateway SoC Core1 | GatewayCore1 | InterCore, PlantCAN, AuxCAN, SensorRS485 | LLL owner; forwarder; W6 member |
 | ... | ... | ... | ... |
*** End Patch

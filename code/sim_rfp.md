# MVP Linux ECU-to-ECU Simulation Framework

## 1. Purpose

Create a very small Linux-hosted simulation framework for developing and integration-testing embedded networking software before moving to real hardware.

The simulator shall model each ECU as a separate Linux process running as much real production networking/service code as practical.

The framework is **not** intended to simulate MCU timing, registers, interrupts, RTOS behavior, electrical bus behavior, or detailed CAN arbitration. Its purpose is functional multi-ECU software integration.

Primary development progression:

1. Unit tests
2. Multi-ECU Linux simulation
3. Real ECU communicating with simulated peers
4. Multi-ECU HIL / real hardware

---

## 2. Core Architectural Principle

An **ECU Process** consists of:

```text
ECU Process
├── Linux runtime/framework
├── one or more simulated network links
├── protocol stack under development
├── services/endpoints
└── ECU-specific application code
```

Only the platform-dependent bottom layer should differ significantly from firmware.

The framework should remain thin. It should provide process lifecycle and Linux network interfaces, not become a general-purpose simulation environment.

---

## 3. MVP Scope

The MVP shall support:

* Linux only.
* C++17.
* One executable per simulated ECU.
* Superloop execution model.
* One or more network interfaces per ECU.
* SocketCAN virtual CAN (`vcan`).
* UART emulation using Linux pseudoterminals.
* Monotonic host time.
* Clean process shutdown.
* ECU-specific application code supplied through a small abstract interface.
* Command-line configuration of links where practical.
* Multiple ECU processes communicating in one simulation.

The MVP does **not** require:

* GUI.
* RTOS simulation.
* virtual MCU peripherals.
* register-level models.
* interrupt simulation.
* simulated CPU utilization.
* deterministic virtual time.
* electrical CAN modeling.
* CAN bit timing/arbitration simulation.
* network fault injection.
* process orchestration framework beyond a simple script.
* containerization.

These may be added later.

---

## 4. ECU Application Interface

Application-specific behavior should be separated from the host framework through composition.

Suggested interface:

```cpp
class EcuApplication {
public:
    virtual ~EcuApplication() = default;

    virtual void init() = 0;
    virtual void periodic() = 0;
};
```

The application object may own or reference:

* WireSpaces/service objects.
* endpoint implementations.
* application state.
* simulated sensors or test state.

Do not require applications to derive from the `EcuProcess` runtime itself.

---

## 5. ECU Process Runtime

Provide an `EcuProcess` abstraction responsible for:

* runtime initialization;
* network/link initialization;
* application initialization;
* polling network interfaces;
* calling application periodic work;
* sleeping between loop iterations;
* terminating cleanly.

Conceptually:

```cpp
class EcuProcess {
public:
    EcuProcess(
        NetworkStack& network,
        EcuApplication& application,
        EcuProcessConfig config);

    int run();
};
```

Typical implementation:

```cpp
int EcuProcess::run()
{
    network_.init();
    application_.init();

    while (!shutdownRequested()) {
        network_.poll();
        application_.periodic();

        sleepUntilNextIteration();
    }

    return 0;
}
```

### Requirements

* Loop period shall be configurable.
* Timing correctness shall not depend on the host running the loop at an exact frequency.
* `SIGINT` and `SIGTERM` shall request clean shutdown.
* The runtime shall use a monotonic clock.
* The runtime shall not create hidden worker threads in the MVP.

Default loop period may be approximately 1 ms, but applications must not depend on exact host scheduling.

---

## 6. Link Abstraction

The networking stack shall consume links through an interface that is also usable on embedded targets.

Exact API may follow existing WireSpaces abstractions. Conceptually:

```cpp
class DatagramLink {
public:
    virtual bool send(std::span<const uint8_t> frame) = 0;
    virtual void poll() = 0;
};
```

or equivalent callback-based interfaces.

The Linux simulation implementation should be thin adapters around native Linux facilities.

Do not create a simulation-only network protocol if an existing Linux primitive provides the required semantics.

---

## 7. Virtual CAN

### 7.1 Transport

Use Linux SocketCAN with `vcan`.

Example setup:

```bash
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0
```

Multiple ECU processes shall be able to bind to the same virtual CAN interface.

Example topology:

```text
ECU_A ─┐
ECU_B ─┼── vcan0
ECU_C ─┤
test ──┘
```

### 7.2 Requirements

The SocketCAN adapter shall:

* open a named CAN interface;
* send CAN frames;
* receive CAN frames non-blockingly or through bounded polling;
* expose CAN identifiers and payloads to the higher link layer;
* support multiple ECU processes on the same `vcan` device;
* avoid simulation-specific framing above SocketCAN.

Initial support may target Classical CAN framing.

CAN FD support should be straightforward to add and should not require architectural changes.

### 7.3 Out of Scope

`vcan` does not need to simulate:

* electrical faults;
* arbitration timing;
* propagation delay;
* bus utilization timing;
* transceiver behavior;
* error frames.

---

## 8. Virtual UART

Use Linux pseudoterminals rather than ordinary files.

The UART adapter shall interact with a PTY slave as if it were a serial character device.

Example external setup:

```bash
socat \
    PTY,raw,echo=0,link=/tmp/ecu_a_uart \
    PTY,raw,echo=0,link=/tmp/ecu_b_uart
```

Then:

```text
ECU_A                    ECU_B
  │                        │
/tmp/ecu_a_uart        /tmp/ecu_b_uart
       \                  /
             socat
```

### Requirements

The UART adapter shall:

* open a configured PTY/device path;
* configure raw mode;
* provide non-blocking or bounded reads;
* write arbitrary byte streams;
* expose the same byte-oriented interface expected by the WireSpaces UART/HDLC link implementation.

The framework should not implement UART framing itself if the production WireSpaces UART link already handles framing such as HDLC.

---

## 9. ECU Configuration

Each ECU executable should support simple command-line configuration.

Example:

```bash
./power_ecu \
    --name power \
    --can control=vcan0 \
    --uart debug=/tmp/power_debug
```

Minimum useful options:

```text
--name <ecu-name>
--loop-period-us <N>
--can <logical-name>=<interface>
--uart <logical-name>=<path>
```

Exact syntax may differ if a simpler implementation emerges.

Avoid adding YAML/JSON configuration to the MVP unless command-line configuration becomes clearly insufficient.

---

## 10. Logging

Every ECU process shall have an ECU name.

Logs should include that name so concurrent processes remain readable.

Example:

```text
[power] initialized CAN control=vcan0
[sensor] TX endpoint=0x0123 len=8
[power] RX endpoint=0x0123 len=8
```

Logging should be simple and may initially use stdout/stderr.

No logging framework dependency is required.

---

## 11. Example Simulation

Provide at least two example ECUs.

Suggested example:

### Sensor ECU

* periodically publishes a counter or synthetic sensor value;
* sends over WireSpaces through `vcan0`.

### Consumer ECU

* receives the service/message;
* prints the latest received value;
* optionally sends a command/request back.

Topology:

```text
Sensor ECU
    │
    ├──── vcan0 ──── Consumer ECU
    │
    └──── optional test harness
```

The example should use the real protocol stack APIs rather than bypassing them with simulator-specific messaging.

---

## 12. Process Launch

Provide a minimal script such as:

```text
tools/run_example_sim.py
```

or:

```text
tools/run_example_sim.sh
```

Responsibilities:

1. create `vcan0` if necessary;
2. create any required PTYs;
3. start ECU processes;
4. forward process output;
5. terminate child processes on Ctrl-C;
6. clean up temporary resources.

Do not build a general process supervisor.

---

## 13. Testing

### Unit Tests

Continue using normal host unit tests for individual protocol components.

### Integration Test

Add at least one automated multi-process integration test that:

1. creates a virtual link;
2. starts two ECU executables;
3. waits for a bounded period;
4. verifies that a message sent by one ECU is received by the other;
5. shuts both processes down;
6. returns pass/fail automatically.

The test must have bounded timeouts so CI cannot hang indefinitely.

The integration test should be suitable for normal Linux CI.

---

## 14. Hardware Interoperability Goal

The architecture shall permit a future mixed setup such as:

```text
Simulated ECU A ─┐
Simulated ECU B ─┼── real SocketCAN adapter ── physical CAN ── STM32 ECU
Simulated ECU C ─┘
```

This should require changing link configuration, not application/service logic.

Similarly, a PTY-backed UART should eventually be replaceable with `/dev/ttyUSB0` or another physical serial device.

This is an important design constraint.

---

## 15. Design Constraints

Prefer:

* composition over inheritance for runtime/application separation;
* no heap requirement in shared embedded networking code;
* platform-independent protocol stack;
* thin Linux adapters;
* explicit ownership;
* simple synchronous polling;
* minimal dependencies;
* straightforward code that an embedded engineer can inspect quickly.

Avoid:

* simulation-specific abstractions leaking into WireSpaces core;
* unnecessary asynchronous frameworks;
* Boost/ASIO dependency for the MVP;
* event-loop frameworks;
* RPC between ECU processes except where it is itself the protocol under test;
* globally shared simulator state between ECU processes.

ECUs should communicate through their simulated network interfaces, not through hidden side channels.

---

## 16. Suggested Repository Structure

```text
sim/
├── include/
│   └── ws_sim/
│       ├── ecu_application.hpp
│       ├── ecu_process.hpp
│       ├── monotonic_clock.hpp
│       ├── socketcan_link.hpp
│       └── pty_uart.hpp
│
├── src/
│   ├── ecu_process.cpp
│   ├── socketcan_link.cpp
│   └── pty_uart.cpp
│
├── examples/
│   ├── sensor_ecu/
│   │   └── main.cpp
│   └── consumer_ecu/
│       └── main.cpp
│
├── tests/
│   └── multi_ecu_test.*
│
└── tools/
    └── run_example_sim.py
```

Adapt naming and placement to the existing repository rather than forcing this exact layout.

---

## 17. MVP Acceptance Criteria

The MVP is complete when all of the following work:

* [ ] Two independent Linux ECU processes can run simultaneously.
* [ ] Each uses the real WireSpaces networking stack.
* [ ] Each has ECU-specific `EcuApplication` code.
* [ ] Both communicate through the same `vcan` bus.
* [ ] One ECU can send a WireSpaces message and the other can consume it.
* [ ] A PTY-backed UART link implementation exists and can exchange bytes with another process/device.
* [ ] `SIGINT` cleanly shuts an ECU process down.
* [ ] ECU name and loop period are configurable.
* [ ] A script launches a complete example network.
* [ ] At least one automated multi-process integration test runs successfully in Linux CI.
* [ ] No simulator-only transport is required by WireSpaces core.
* [ ] Replacing a simulated link with a real Linux CAN/UART device does not require changing ECU application logic.

---

## 18. Explicit Non-Goals for the First Version

Do not extend the MVP merely to improve simulator realism.

Stop once the acceptance criteria above are satisfied.

Future features should be motivated by concrete test or development needs.

Candidate later additions include:

* CAN FD.
* UDP links.
* deterministic simulated time.
* packet loss/delay/corruption proxies.
* bandwidth/rate limiting.
* link disconnect/reconnect.
* simulated ECU restart.
* scenario files.
* richer test orchestration.
* captured/replayed CAN traces.
* Linux simulated peers combined with HIL hardware.
* visualization/traffic inspection.

These are not part of the MVP.

# WireSpaces Linux ECU Simulator

This directory contains the first simulator increment: a small C++17 process
shell that runs ECU-specific application code in a synchronous superloop.

The current executable demonstrates process lifecycle only. Its
`NetworkRuntime` is a no-op temporary simulator seam; it does not exchange
WireSpaces messages and is not a transport for the future protocol core.

## Build and test on Linux or WSL

From the WireSpaces repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

When tests are enabled, CMake fetches GoogleTest v1.18.0 into the local build
tree. The first configuration therefore requires Git and network access, but
does not require a system-wide package or administrator privileges.

## Run the counter ECU

```sh
./build/sim/ws_sim_counter_ecu --name sensor --loop-period-us 1000
```

Press Ctrl-C to request a clean shutdown. The default loop period is 1000
microseconds when `--loop-period-us` is omitted.

```sh
./build/sim/ws_sim_counter_ecu --help
```

## Current boundaries

- Linux/WSL host execution with monotonic host time.
- One process, one application, one synchronous polling loop.
- No worker threads, GUI, virtual MCU, RTOS, or deterministic virtual time.
- No SocketCAN, PTY UART, process orchestration, or fault injection yet.
- No canonical PDU, Endpoint Dispatcher, Router, Link Engine, or Services yet.

The next independent increment should add a SocketCAN/vcan adapter and a
two-process CAN smoke test. PTY UART, orchestration/CI, and integration with the
real WireSpaces stack remain later increments.

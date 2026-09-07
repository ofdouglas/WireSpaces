# WireSpaces Open Source Services Library

This will eventually contain re-usable firmware services such as:
 - Link health telemetry
 - Firmware version
 - Hardware information
 - Heartbeat (uptime + reset cause?)
 - Bootloader
 - Logging, event tracing
 - Reset cause
 - Network discovery, network configuration
 - OS / memory / thread health telemetry
 - DID-style table accessible over network, may be useful during development.

Current prototypes:
 - Heartbeat
 - Ping
 - Peak stack utilization report
 - LED on/off control


## Request admission

Ping and LED control expose a `TransportFilterReceiver` through `receiver()`.
It accepts only Simple packets without header extensions or nonzero reserved
control bits, before consuming queue capacity. Direct receiver calls and
Dispatcher delivery use the same filter. Accepted work remains deferred until
`run()`; valid queue-full and downstream rejection results are preserved.

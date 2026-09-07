# WireSpaces — Project Summary

WireSpaces is an embedded Services platform for building distributed systems that span a wide range of devices, links, and deployment topologies.

Its goal is to make it relatively easy to integrate high-quality standard Services—and product-specific proprietary Services—across systems ranging from tiny bare-metal MCUs to multicore SoCs, gateways, Linux hosts, simulators, and development PCs.

The networking layer is important, but it is not intended to be the main thing application developers think about. Ideally, once the communication substrate is stable, most development happens in terms of Services: what a device provides, what it consumes, and how those Services behave.

## What WireSpaces is trying to provide

WireSpaces aims to support embedded products whose communication needs do not fit neatly into one transport or one class of processor.

A single product may contain:

- small 8-bit or 32-bit MCUs;
- larger application processors or Linux systems;
- multicore devices with internal communication;
- gateways between buses;
- fieldbuses such as CAN or serial links;
- richer links such as Ethernet;
- host-based simulation and test environments.

WireSpaces tries to give these systems one coherent Service model rather than requiring each link, device class, or deployment boundary to become its own software ecosystem.

## Core goals

### Portable Services across heterogeneous systems

A Service should be able to use the same basic conventions whether its peers are on the same device, another ECU, behind a gateway, or running on a host PC.

The physical communication method still matters for performance, reliability, and failure behavior, but it should not force every Service API to be redesigned.

### Friendly to constrained targets

Small devices are first-class participants rather than edge cases.

The base platform should remain practical for low-memory MCUs, bare-metal systems, and small FPGA softcores, with bounded memory use and implementations that do not assume dynamic allocation, a full operating system, or IP networking.

### Useful on richer systems too

The same ecosystem should extend upward to Linux, simulation, tooling, gateways, and higher-performance embedded computers.

Richer devices should be able to provide more capable Services without forcing tiny devices to carry the same implementation burden.

### Static and predictable where embedded systems benefit from it

WireSpaces favors systems whose communication structure is mostly known ahead of time or changes infrequently.

The platform should be easy to reason about, bounded, testable, and suitable for products where predictable resource use matters more than highly dynamic network behavior.

### Incremental adoption

A small system should not require a large deployment framework just to communicate.

It should be possible to start with a few devices and simple configuration, then add gateways, more Services, richer links, simulation, and generated deployment configuration as the product grows.

### Strong standard Service ecosystem

The long-term value of WireSpaces is intended to come from reusable Services, not just packet transport.

Examples may include:

- health and status reporting;
- identity and version information;
- firmware update;
- logging and structured events;
- diagnostics and link status;
- time synchronization;
- telemetry;
- RPC-style interactions;
- publish/subscribe patterns;
- platform and application state.

Products can add their own proprietary Services alongside these standard ones.

### Continuity between real hardware and simulation

The same Service-oriented design should work in host-based simulation, SIL, HIL, and real embedded deployments.

A component that talks to another device over a fieldbus in production should be easier to exercise against host-resident peers during development and test.

## What WireSpaces is not trying to be

WireSpaces is not intended to replace the Internet protocol stack, cloud middleware, or large dynamic distributed-systems frameworks.

It is aimed primarily at embedded machines and products whose communication topology is relatively structured and whose nodes may include very constrained hardware.

It also does not try to hide the physical realities of embedded communication. Bandwidth, latency, failure domains, and link capabilities still matter. The goal is to give them a consistent architectural home rather than pretend they do not exist.

## Project vision

A successful WireSpaces system should let engineers spend less time inventing one-off communication infrastructure and more time building and integrating useful embedded Services.

The desired end state is:

> **Excellent standard and custom Services can be integrated across a wide range of embedded system topologies, links, and device types—from tiny MCUs to host PCs—using one coherent, bounded, portable platform.**

The network layer is the foundation. The Service ecosystem is the product.

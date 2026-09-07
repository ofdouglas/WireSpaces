# Known issues

Review of `prototype1` at commit `8eac9fe`, recorded 2026-09-06.

This review focuses on structural issues in the current prototype, rather than
polish or missing future features. Given the project's early stage and solo
development, merging soon is reasonable. The core/runtime separation and staged
topology compiler are reasonable foundations. Address items 1–3 with small fixes
before merging where practical; carry the remaining items into focused follow-ups.

All items below are open. Recording them does not imply that fixes have been made.

## 1. BITS can report completion for data it never sent

**Priority:** Before merge.

ACK validation checks transfer geometry but does not verify that cumulative
acknowledgements cover transmitted segments. A probe reproduced an eight-byte
transfer reporting completion after sending only SETUP, with zero segments sent
and zero bytes received. A malformed or stale same-session ACK can therefore
produce false success.

Validate cumulative advancement against transmitted state, including during
setup. Add regression coverage for ACKs claiming unsent segments and stale ACKs
when session identifiers are reused.

**Location:** `lib/wirespaces/transports/bits/bits.cpp`,
`BitsTransmitter::handleAck()` (around line 445 at the reviewed commit).

## 2. Explicit domain identity does not extend to local forwarding

**Priority:** Before merge.

`LocalDispatchForwarder` invokes the Dispatcher overload that reads process-global
host identity. A probe reproduced a local-only send being rejected despite
matching its `DomainContext`, because the global host differed. Direct dispatch
with the context's identity accepted the same packet.

Bind local delivery to explicit identity too. Exercise multiple domains without
swapping process-global identity in fixtures, so tests cover the intended ownership
model rather than masking this dependency.

**Location:** `lib/wirespaces/core/domain.cpp`,
`LocalDispatchForwarder::forward()` (line 12 at the reviewed commit).

## 3. Unsupported packet formats can reach application behavior

**Priority:** Before merge.

Routing and dispatch validate wire, ingress, destination and endpoint, but there
is no consistent owner for validating transport type and extensions. Ping and LED
handlers interpret payloads without checking those controls; the general BITS
connection adapter also overlooks extensions.

A probe reproduced a packet with unsupported transport value 7 and extensions
enabled operating the LED handler. Establish the validation boundary and reject
formats the prototype cannot interpret. This does not require implementing those
formats. Add tests that unsupported controls cannot invoke service behavior.

**Locations:**

- `lib/wirespaces/core/dispatch.cpp`, `Dispatcher::dispatch()`.
- `lib/wirespaces/services/led_control/led_control.h`, `processRequest()`.
- `lib/wirespaces/services/ping/ping.h`, `processRequest()`.
- `lib/wirespaces/transports/bits/bits.cpp`, `matchesConnection()`.

## 4. BITS lacks a recovery policy for abandoned transfers

**Priority:** Soon after merge.

Abort is sent once and immediately terminates the transmitter locally. If that
packet is lost, the receiver remains active and rejects a new session as busy.
There is no receiver expiry or application recovery in the current example.
A probe reproduced a lost abort followed by rejection of the next session while
the receiver remained active.

Decide who owns inactivity expiry or explicit reset, and make failure paths
release resources predictably. A local receiver abort/reset can recover the
state today, but the example does not arrange that recovery. Test interrupted
transfers followed by new sessions, including lost aborts and peer disappearance.

**Locations:**

- `lib/wirespaces/transports/bits/bits.cpp`, `BitsTransmitter::abort()`.
- `lib/wirespaces/transports/bits/receiver_engine.cpp`, `handleSetup()` and
  `handleAbort()`.
- `examples/arduino-uno/bits_ram_transfer.cpp`, application polling/lifecycle.

## 5. Endpoint ownership and connection selection need an explicit model

**Priority:** Soon after merge, before expanding to multiple BITS peers per endpoint.

BITS objects represent individual connections, including remote host and wire.
The Dispatcher selects only by endpoint and returns after the first unicast
binding, even when that receiver rejects a different connection. Two BITS
receivers sharing an endpoint therefore cannot serve separate peers through that
table. A probe confirmed that the second binding was unreachable through dispatch,
although its receiver accepted the packet directly.

Choose one endpoint owner with connection selection inside it, or richer dispatch
keys. If one connection per endpoint is intentional for the prototype, enforce
that constraint. Simply continuing on every rejection would conflate connection
mismatch with an actual admission rejection; define those semantics explicitly.

**Locations:**

- `lib/wirespaces/core/dispatch.cpp`, unicast return in `Dispatcher::dispatch()`.
- `lib/wirespaces/transports/bits/bits.cpp`, `matchesConnection()`.

## 6. Tree flooding and BITS admission semantics conflict on branched wires

**Priority:** Soon after merge, before using BITS on larger generated topologies.

Generated forwarding sends unicast traffic down every selected branch. BITS
treats partial admission as fatal. Congestion on an unrelated branch can therefore
terminate a transfer whose destination branch accepted the packet.

The BITS API notes acknowledge this restriction, but configuring an appropriate
route remains an application responsibility. Either constrain supported connection
paths or define how transport progress relates to fan-out results. Add a
branched-transfer integration test with one congested branch. This finding is
based on the current forwarding and send-result contracts; the temporary probes
did not exercise this scenario.

**Locations:**

- `codegen/wiring_codegen.py`, generated `Forwarder::forward()`.
- `lib/wirespaces/transports/bits/bits.cpp`, `BitsTransmitter::forwardPacket()`
  and send-error handling in `process()`.
- `lib/wirespaces/transports/bits/api_notes.md`, link admission contract.

## 7. Default verification excludes substantial prototype subsystems

**Priority:** Before or soon after merge.

CI and `scripts/test.sh` run CMake/CTest, excluding generator tests, Python tools
and AVR compilation. The generator suite contains important compiled topology
integration coverage, so this leaves a structural regression gap in the normal
verification path.

Bring those existing checks into the normal workflow, including dependencies
needed to run the JavaScript viewer test. Hardware tests can remain separate.

**Locations:** `.github/workflows/ci.yml`, `scripts/test.sh`,
`codegen/Makefile`, and `examples/arduino-uno/Makefile`.

## Review validation

The following checks ran against the reviewed checkout:

- `bash scripts/test.sh`: all 110 C++ tests passed.
- `make -C codegen test`: 53 tests passed; one JavaScript test was skipped because
  Node was not available on the runner's PATH.
- `PYTHONPATH=tools python3 -m unittest discover -s tools/test -v`: all 16 tests passed.
- `make -C examples/arduino-uno all bits-ram-transfer bits-boot-profile-ram bits-boot-profile-size mcp2515-can-test`:
  all five AVR targets compiled successfully.
- Five temporary executable probes confirmed the unwanted behaviors in items
  1–5. The probes asserted the observed failures, so their passing results are
  evidence of reproduction, not evidence of correctness.

The temporary probe source was `/tmp/ws-review-probes.cpp`; it is not a committed
regression suite and may not survive cleanup. Each behavioral issue above records
the reproduction needed to add durable tests.

No hardware tests were run. The review did not modify tracked production files.

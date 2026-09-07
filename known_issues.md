# Known issues

Open structural issues from the review of `prototype1` at commit `8eac9fe`,
recorded 2026-09-06. These findings concern the current prototype rather than
polish or missing future features. Original issue numbers are retained.

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

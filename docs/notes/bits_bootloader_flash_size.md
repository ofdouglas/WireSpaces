# BITS receiver in a 4 KB ATmega328P boot section

**Measured:** 2026-08-30  
**Toolchain:** `avr-g++ 7.3.0`, GNU binutils 2.26  
**Target:** ATmega328P, 16 MHz CPU, UART/HDLC, Compact BITS

## Conclusion

A lean but maintainable WireSpaces/BITS receiver appears feasible in the
ATmega328P's 4,096-byte maximum boot section, but the current general-purpose
`BitsReceiver` composition does not fit.

The current bidirectional RAM-loopback test is 10,516 bytes. A receiver-only
build retaining `Router`, `Dispatcher`, queued ingress, runtime polymorphism,
and all receiver features is 5,932 bytes. Bypassing only `Dispatcher` reduces
that to 5,448 bytes, which proves that runtime removal alone is insufficient.

A straightforward boot-profile sizing proxy is 3,102 bytes. It reuses the
production `Header`, `PacketBuffer`, UART HDLC framing and CRC, and Compact BITS
message codecs. It uses direct composition, a one-segment advertised window,
no user datagrams, no queued ingress copies, and direct ACK/REJECT output. This
leaves 994 bytes for missing bootloader functions.

The 3,102-byte result is evidence of feasibility, not yet a bootloader. Its
segment sink is a stub. It does not include SPM page programming, a page buffer,
whole-image validation, boot/application selection, watchdog/reset handling,
rollback, authentication, or recovery policy. Basic SPM programming and boot
control should plausibly fit in the remaining space; richer validation or
cryptographic security may not.

## Build configuration

The existing Arduino Makefile already uses the important size settings:

```text
-Os
-ffunction-sections -fdata-sections
-Wl,--gc-sections
-fno-exceptions -fno-rtti -fno-threadsafe-statics
```

The analysis build added a linker map and cross-reference table:

```text
-Wl,-Map,bits_ram_transfer.map,--cref
```

Two additional compiler options were measured separately:

```text
-flto
-mcall-prologues
```

## Measured images

| Build | Flash | SRAM | Change from current |
|---|---:|---:|---:|
| Current bidirectional RAM loopback | 10,516 B | 877 B | baseline |
| Current loopback with LTO | 9,806 B | 871 B | -710 B |
| Current loopback with LTO and call prologues | 9,612 B | 871 B | -904 B |
| Receiver only, current Router/Dispatcher APIs | 5,932 B | 505 B | -4,584 B |
| Receiver only, direct dispatch but current BITS/Router APIs | 5,448 B | 235 B | -5,068 B |
| Lean boot-profile proxy | 3,102 B | 82 B | -7,414 B |

The receiver-only variants are sizing compositions, not repository products.
Their SRAM numbers differ because the direct-dispatch variant uses a stub sink
instead of the RAM test's 256-byte destination object. That SRAM difference
does not materially explain the flash result.

## Current full-image attribution

The map attributes the current 10,516-byte image as follows. Small linker
startup/alignment contributions account for the remaining bytes.

| Input | Flash |
|---|---:|
| `bits_transport.o` | 6,442 B |
| Arduino RAM-loopback application, including HDLC templates | 2,196 B |
| `bits_codec.o` | 824 B |
| `dispatch.o` | 261 B |
| libgcc arithmetic helpers | 152 B |
| millisecond clock | 116 B |
| `header.o` | 92 B |
| `packet.o` | 88 B |
| `router.o` | 80 B |
| `host.o` | 58 B |
| libc | 18 B |

The largest linked functions in the full image are:

| Function | Flash |
|---|---:|
| application `main` | 1,118 B |
| `BitsTransmitter::process` | 556 B |
| `BitsReceiver::handleSegment` | 546 B |
| `BitsReceiver::handleSetup` | 506 B |
| `BitsTransmitter::handleAck` | 466 B |
| HDLC encoder | 442 B |
| `BitsTransmitter::sendSegment` | 428 B |
| `BitsTransmitter::startTransfer` | 370 B |
| `BitsReceiver::receive` | 324 B |
| `BitsReceiver::handleDatagram` | 284 B |
| `BitsReceiver::updateGrant` | 278 B |
| `Dispatcher::dispatch` | 254 B |
| `BitsReceiver::sendAck` | 222 B |

This confirms that the full transmitter and the general receiver state machine
are the main costs. Header, PacketBuffer, Router, and Dispatcher together are
not the dominant 10 KB problem.

## Receiver-only attribution

The 5,448-byte direct-dispatch receiver still contains:

| Input | Flash |
|---|---:|
| current receiver state machine in `bits_transport.o` | 2,980 B |
| application, UART, HDLC, packet assembly | 1,390 B |
| required BITS codecs | 526 B |
| libgcc arithmetic helpers | 98 B |
| `header.o` | 92 B |
| `packet.o` | 88 B |
| `router.o` | 80 B |
| libc | 18 B |
| startup/linker remainder | 176 B |

The current receiver state machine plus its codecs therefore consumes about
3.5 KB before UART/HDLC framing and application glue. That is the boundary that
must change for a 4 KB image.

The lean proxy's 3,102 bytes break down as:

| Input | Flash |
|---|---:|
| boot-profile composition, UART and HDLC | 2,190 B |
| reused BITS codecs | 456 B |
| libgcc arithmetic helpers | 98 B |
| `header.o` | 92 B |
| `packet.o` | 74 B |
| libc | 18 B |
| startup/linker remainder | 174 B |

## Easy trims

1. Enable LTO for constrained AVR targets. It saved 710 bytes on the current
   full test without changing source.
2. Enable `-mcall-prologues` for the bootloader target after performance and
   stack review. It saved another 194 bytes with LTO.
3. Build only the receiver. Removing the test transmitter, echo path, timer,
   second object, and transmitter packet storage reduced the image from 10,516
   to 5,932 bytes.
4. Directly select the single boot endpoint instead of linking Dispatcher.
   The measured direct composition was 484 bytes smaller, although about 52
   bytes of that comparison is simpler sink code. `dispatch.o` and `host.o`
   themselves account for 319 bytes.
5. Use only the packet buffers the boot profile requires. Each current
   28-byte-payload packet object occupies 40 bytes of initialized `.data`, which
   costs both flash and SRAM. A synchronous one-window receiver does not need
   two segment queue slots plus a separate datagram ingress packet.

These changes help, but only the first four still leave today's receiver above
4 KB.

## Refactors that require more work

The attachment's proposed dependency inversion is directionally correct, but
the measurements refine it: removing `Router` and virtual calls is useful, not
sufficient. `router.o` is only 80 bytes in the direct receiver image, and the
visible vtables are only a few dozen bytes. The larger opportunity is to move
queueing and general feature policy outside the core receive engine.

Recommended structure:

1. Make a synchronous Compact BITS receiver engine that consumes one decoded
   BITS PDU and emits ACK/REJECT/terminal actions through a sender policy.
2. Put queued packet ownership and deferred `process()` behavior in a runtime
   adapter. The current receiver pays for packet copies, occupancy masks,
   datagram ingress, and a second processing phase even when a bootloader can
   process one UART frame synchronously.
3. Make the receive-window policy compile-time selectable. A one-slot window is
   protocol-correct for a low-bandwidth-delay boot UART and removes selective
   bitmap/grant machinery from that instantiation. The normal runtime can retain
   the 16-wide out-of-order window.
4. Make user datagrams optional. They are not needed to install an image. The
   current direct receiver links `decodeUserDatagram` (70 bytes) plus its branch,
   callback, and ingress-storage consequences.
5. Keep SETUP, SEGMENT, cumulative ACK, PROBE response, duplicate recovery,
   REJECT, and ABORT. Those are the useful reliable-transfer semantics; this
   should not become an unreliable special protocol.
6. Keep runtime polymorphism in runtime adapters (`ReceiverRef`, Router sender
   adapter), while the boot composition uses concrete sender and flash-sink
   policies directly.
7. Enforce the boundary with a constrained build target that links foundation,
   packet protocol, UART HDLC, BITS receiver engine, and no runtime library.

This should be one reusable BITS engine with selectable composition, not a
permanent `TinyBitsReceiver` fork. The sizing proxy demonstrates the desired
generated-code shape; it should not be copied into production as a parallel
state machine.

## Suggested budget and next proof

Set an initial CI ceiling of 3,200 bytes for this composition:

```text
AVR startup + UART/HDLC + Header/PacketBuffer + Compact BITS receiver
excluding the board-specific flash sink
```

Then implement the actual ATmega328P flash sink and boot handoff in the
remaining 896 bytes, while targeting the absolute 4,096-byte linker limit. The
current proxy leaves 994 bytes, and LTO/call-prologue savings were not yet
measured on that proxy, so there is some plausible additional margin.

The next decisive test is an actual boot-section ELF containing:

- SPM erase/fill/write with interrupt exclusion and boot-section placement;
- one 128-byte page buffer or an explicitly safe streaming alternative;
- final image length and CRC validation;
- watchdog/reset-cause handling and application-vector handoff;
- linker assertions for both 4,096-byte flash and SRAM budgets;
- power-loss and interrupted-transfer behavior.

If that image remains under 4,096 bytes, the architectural question is settled.
If it does not, the next trims should be feature/profile choices in the receiver
engine, not hand-written assembly or a second WireSpaces stack.

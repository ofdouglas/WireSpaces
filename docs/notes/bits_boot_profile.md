# Compact BITS constrained receiver profile

## Purpose

This profile is the intermediate proof for a WireSpaces bootloader that must
fit in the ATmega328P's 4,096-byte boot section. It proves the reusable
WireSpaces and BITS components on target before adding flash programming or
bootloader-specific policy.

The profile is not a second BITS protocol or an independent tiny
implementation. It directly composes `BitsReceiverEngine`, while the normal
runtime uses the same engine through the queued `BitsReceiver` endpoint
adapter.

## Included composition

- Compact BITS wire format only;
- receiver-side segmented transfer;
- a one-position advertised receive window;
- synchronous PDU processing and sink callbacks;
- BITS user datagram receive and transmit;
- WireSpaces `Header` and `PacketBuffer`;
- UART HDLC framing and CRC;
- caller-owned RAM object sink for the hardware test;
- direct outbound PDU sender supplied by the application.

## Excluded composition

- Arduino-side BITS transmitter;
- `Dispatcher`, general routing tables, and queued endpoint ingress;
- wide/out-of-order receive storage in the constrained instantiation;
- boot information, image metadata, or compatibility policy;
- flash erase/program operations and boot/application handoff;
- authentication, rollback, or A/B images.

The normal runtime adapter retains queued ingress and a caller-sized window up
to the Compact BITS limit of sixteen positions.

## Engine boundary

`BitsReceiverEngine::process()` consumes one already-admitted BITS PDU. It
owns Compact BITS session, window, ACK, REJECT, PROBE-response, ABORT, and
completion state. It has no dependency on endpoint admission, `Router`,
`Dispatcher`, packet queues, or a physical link.

The caller provides:

- `ReceiverCallbacks` for segment storage, user datagrams, and terminal
  notifications;
- `ReceiverPduSender` for bounded outbound payload storage and transmission;
- maximum segment size and receive-window width.

The queued `BitsReceiver` remains an `EndpointReceiver`. It validates the
WireSpaces connection, owns ingress packets, and passes their BITS payloads to
the engine from `process()`.

## Planned RAM hardware-test contract

The PC is the only segmented-transfer transmitter. The Arduino is the
constrained receiver and user-datagram responder.

Test user datagrams use the following temporary service payloads:

| Value | Name | Payload |
|---:|---|---|
| `0x01` | Echo request | opcode, transaction ID, arbitrary bytes |
| `0x81` | Echo response | opcode, transaction ID, the same arbitrary bytes |
| `0x82` | Image result | opcode, session ID, status, received length LE16, first bad offset LE16 |

The PC first verifies an exact echo response. It then transfers a 256-byte RAM
object whose byte at every offset is `uint8_t(offset)`. After BITS completion,
the Arduino verifies the length and every byte, then emits `Image result`.
`first bad offset` is `0xffff` on success. The test also transfers a shorter
object whose length is not a multiple of the segment payload size so the final
partial segment is covered.

These opcodes are test instrumentation, not the future boot-control protocol.

## Size and memory gates

Both future AVR targets use:

```text
-Os -flto -mcall-prologues -mrelax
-ffunction-sections -fdata-sections
-fno-exceptions -fno-rtti -fno-threadsafe-statics
-DWIRESPACES_BITS_RECEIVER_MAX_WINDOW_WIDTH=1
-Wl,--gc-sections
```

They also produce an ELF and linker map.

- The complete RAM hardware-test firmware must fit within 4,096 bytes.
- The production-representative receiver composition should be at most about
  3,350 bytes, leaving at least 746 bytes for flash operations, validation, and
  handoff.
- A result from 3,351 through 3,700 bytes requires a trim review before flash
  work begins.
- A result above 3,700 bytes is a no-go for the current 4 KB architecture.

Static SRAM is reported separately. The 256-byte test object is test-only; a
real bootloader is expected to retain only its flash-page buffer and protocol
state.


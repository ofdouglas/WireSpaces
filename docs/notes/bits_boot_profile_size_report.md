# Compact BITS constrained receiver size report

**Measured:** 2026-08-30
**Target:** ATmega328P, 16 MHz, UART HDLC, Compact BITS
**Toolchain:** `avr-g++ 7.3.0`, GNU binutils 2.26

## Decision

The receiver-only RAM hardware test and the production-representative core
both pass their stage gates. Development can proceed to the boot-control and
flash-writing stages after the RAM test passes on hardware.

- Complete RAM hardware-test image: **3,042 bytes**, 1,054 bytes below the
  4,096-byte boot-section limit.
- Production-representative receiver core: **2,670 bytes**, 680 bytes below the
  preferred 3,350-byte budget and 1,426 bytes below the absolute limit.

This is a qualified go rather than proof that the final bootloader fits. The
core result includes a minimal user-datagram response but not `GetInfo`, image
metadata/CRC validation, SPM page programming, boot state, reset handling, or
application handoff. Those features must replace test behavior where possible
and fit in the remaining 1,426 bytes.

## Build compositions

Both builds contain:

- AVR startup and blocking UART;
- WireSpaces `Header` and `PacketBuffer`;
- UART HDLC receive and streaming transmit with CRC-16/CCITT-FALSE;
- Compact BITS codecs;
- synchronous `BitsReceiverEngine`;
- SETUP, one-position grant, SEGMENT, ACK, PROBE response, REJECT, ABORT, and
  user datagrams;
- SETUP-time maximum segment and object-size rejection.

The RAM hardware test additionally contains a 256-byte stack-resident object,
exact datagram echo, deterministic pattern verification, and image-result
datagrams. The size-only build uses a bounded sink stub and a minimal
two-byte user-datagram response. Neither build links `BitsReceiver`,
`BitsTransmitter`, `Router`, `Dispatcher`, packet ingress queues, or timers.

## Compiler and linker configuration

```text
-Os -flto -mcall-prologues -mrelax
-ffunction-sections -fdata-sections
-fno-exceptions -fno-rtti -fno-threadsafe-statics
-DWIRESPACES_BITS_RECEIVER_MAX_WINDOW_WIDTH=1
-Wl,--gc-sections
```

The Make targets enforce 4,096-byte and 3,350-byte limits. Their products are:

```text
examples/arduino-uno/build/boot_profile/bits_boot_profile_ram.elf
examples/arduino-uno/build/boot_profile/bits_boot_profile_ram.hex
examples/arduino-uno/build/boot_profile/bits_boot_profile_ram.map
examples/arduino-uno/build/boot_profile/bits_boot_profile_size.elf
examples/arduino-uno/build/boot_profile/bits_boot_profile_size.map
```

## Final memory measurements

| Image | `.text` | `.data` | Flash | Reported static SRAM |
|---|---:|---:|---:|---:|
| RAM hardware test | 3,012 B | 30 B | **3,042 B** | 30 B |
| Size-only receiver core | 2,640 B | 30 B | **2,670 B** | 30 B |

The AVR size tool does not count automatic storage. Disassembly shows the RAM
test's non-returning `main` reserves a 407-byte stack frame containing the
256-byte image, HDLC decoder, transmit packet, callbacks, and engine. Thus its
persistent RAM use is approximately 437 bytes before nested call frames. The
streaming HDLC transmitter no longer allocates a temporary encoded-frame
buffer.

## Largest final symbols

LTO folds most receiver processing and UART admission into `main`, so the map
cannot honestly attribute that code to individual input objects.

| Symbol | RAM test | Size-only core |
|---|---:|---:|
| `main` including inlined engine/HDLC admission | 1,812 B | 1,720 B |
| `BitsReceiverEngine::sendAck` | 122 B | 122 B |
| RAM image completion/report callback | 128 B | 2 B |
| `BitsReceiverEngine::sendDatagram` | 120 B | folded into `main` |
| streaming UART PDU sender | 108 B | 108 B |
| datagram callback | 88 B | 98 B |
| segment callback | 92 B | 76 B |
| known-type codec admission | folded into `main` | folded into `main` |
| outbound packet preparation | 90 B | 90 B |
| `BitsReceiverEngine::sendReject` | 82 B | 82 B |
| 16-by-16 to 32-bit multiply helper | 30 B | 30 B |

The two vtables occupy 20 bytes of `.data`; startup data-copy support and the
initial values account for the remaining `.data` flash/SRAM cost.

## Size-reduction progression

| Change | RAM test | Size-only core |
|---|---:|---:|
| Initial reusable engine with buffered HDLC | 4,470 B | 4,212 B |
| Single-pass streaming HDLC transmit | 4,214 B | 3,898 B |
| Compile-time one-position maximum window | 3,848 B | 3,492 B |
| Running offset and subtraction-based segment count | 3,786 B | 3,400 B |
| AVR call/jump relaxation and final SETUP limit ordering | **3,702 B** | **3,332 B** |
| Aligned SETUP with explicit final-segment geometry | **3,740 B** | **3,316 B** |
| Single control decode with known-type field decoders | **3,074 B** | **2,702 B** |
| One-position ACK encoding and sequence arithmetic | **3,042 B** | **2,670 B** |

The aligned SETUP revision saves 16 bytes in the production-representative core. The complete RAM test grows by 38 bytes because its test-only session instrumentation also decodes SETUP and reconstructs total size before receiving the object. The subsequent known-type decoder path removes 666 bytes from the RAM test and 614 bytes from the core without weakening the validated public codec API. Narrow ACK specialization removes another 32 bytes. The AVR retains a 30-byte 16-by-16-to-32-bit multiplication helper for total-size reconstruction.

The important architectural reduction was compiling selective-window state
and arithmetic out of the constrained build. The default library build still
supports the full sixteen-position Compact BITS window and passes its existing
out-of-order tests.

## Hardware test status

The firmware and PC test are built but have not been run against the Arduino
in this stage. The PC test checks:

1. exact user-datagram echo including HDLC escape bytes;
2. a 256-byte `data[i] == uint8_t(i)` RAM image;
3. a 251-byte image to exercise a partial final segment;
4. the Arduino's session, length, content status, and first-bad-offset report.

The user can run it with:

```sh
cd examples/arduino-uno
make flash-bits-boot-profile PORT=/dev/arduino-uno
make test-bits-boot-profile PORT=/dev/arduino-uno
```


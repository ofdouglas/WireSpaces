# WireSpaces demo using ATMEGA328P (Arduino UNO)

The example runs a minimal WireSpaces heartbeat publisher:

- ATmega328P Timer 0 supplies the millisecond clock.
- `HeartbeatService` creates one canonical PDU per second.
- `StackReportService` publishes peak painted-stack utilization once per second.
- `LedControlService` controls the UNO built-in LED on digital pin 13.
- The core Router selects the UART egress for Wire 1.
- The HDLC Link layer frames and byte-stuffs the canonical header and payload.
- UART0 transmits at 115200 baud through the UNO USB serial connection.

The heartbeat payload contains the current 32-bit millisecond uptime. The
first-stage HDLC framing does not yet append a CRC.

Build from WSL:

```sh
make
```

Flash through the Arduino bootloader:

```sh
make flash
```

The default port is `/dev/ttyACM0`. After installing the udev rule below, you can use the stable symlink `/dev/arduino-uno` instead:

```sh
make flash PORT=/dev/arduino-uno
```

Override the port when necessary:

```sh
make flash PORT=/dev/ttyACM1
```

### WSL udev access

In WSL, attach the board with `usbipd` first, then install the udev rule:

```sh
./config/install-udev.sh
```

If WSL reports `bash\r: No such file or directory`, the script has Windows line endings. Run:

```sh
bash ./config/install-udev.sh
```

Or convert line endings safely (do not use `tr -d '\r'` — it can corrupt words like `dirname`):

```sh
python3 -c "p='config/install-udev.sh'; d=open(p,'rb').read().replace(b'\r\n',b'\n'); open(p,'wb').write(d)"
```

Add your user to `dialout` if needed (log out/in afterward):

```sh
sudo usermod -aG dialout "$USER"
```

The rule matches official Arduino UNO USB IDs (`2341:0043` and `2341:0001`), sets group `dialout`, and creates `/dev/arduino-uno`.

The generated files are `build/heartbeat.elf` and `build/heartbeat.hex`.

Receive and log WireSpaces packets until interrupted:

```sh
make receive
```

`receiver.py` incrementally decodes HDLC framing, validates its trailing
CRC-16/CCITT-FALSE, parses the canonical WireSpaces header, and emits each
packet through Python's `logging` module. The two CRC bytes are transmitted
most-significant byte first and are themselves subject to byte stuffing.
Future formatting and log destinations can be added with standard logging
formatters and handlers without changing serial/framing code.

For a bounded live test that resets the UNO and receives one packet:

```sh
python3 receiver.py --port /dev/ttyACM0 --baud 115200 \
    --reset --count 1 --timeout 5
```

Run the C++ and Python framing/receiver unit tests:

```sh
make test-receiver
```

Ping the Arduino and require a matching response:

```sh
make test-ping
```

The hardware test sends a directed request from PC Participant 2 to Arduino
Participant 1. Its default sequence, `0x7E7D`, deliberately contains both HDLC
reserved bytes so the exchange tests byte stuffing in both directions.

Validate the live stack report:

```sh
make test-stack
```

The AVR monitor paints unused SRAM once during startup, scans the paint boundary
every 4096 superloop iterations, and reports peak-used and available bytes.
The initialization guard and startup/main stack frames are conservatively
counted as used.

Control the built-in LED through directed WireSpaces requests:

```sh
make led-on
make led-off
PYTHONPATH=../../tools/python python3 -m wirespaces.led_control 10% --port /dev/ttyACM0
PYTHONPATH=../../tools/python python3 -m wirespaces.led_control 128 --port /dev/ttyACM0
make led-ramp
```

The controller waits for an acknowledgement from Arduino Participant 1 before
reporting success. The LED control Service uses Common Endpoint `0x3FFB` and an
8-bit ratiometric brightness value, where `0/255` is off and `255/255` is fully
on. Because the built-in D13/PB5 LED is not connected to a hardware PWM output,
Timer 1 interrupts generate approximately 977 Hz PWM. The ramp test sends
acknowledged 10% steps from 0% through 100% over one second.

Run the bounded hardware smoke test:

```sh
make verify
```

Expected output includes:

```text
WS packet wire=1 src=1 dst=broadcast qos=NORMAL ... payload[4]=...
```

The four payload bytes are the little-endian heartbeat uptime. The CRC trailer
is removed by the Link decoder before canonical packet parsing.

## BITS RAM transfer test

The standalone `bits_ram_transfer` firmware exercises both Compact BITS roles
without changing the heartbeat demo. The PC uploads an arbitrary object into a
fixed 256-byte UNO receive buffer. Once reception completes, the UNO snapshots
the object into a separate stable transmit buffer and starts a second BITS
transfer that echoes it into a PC-side RAM receiver. The PC command succeeds
only when both transfers complete and the returned bytes match.

Two User0 endpoints keep the simultaneous directions independent:

- endpoint 1: PC transmitter to UNO RAM receiver;
- endpoint 2: UNO RAM transmitter to PC receiver.

Both sides use Host 1 for the UNO, Host 2 for the PC, and Wire 1. UNO segment
payloads are at most 24 bytes, and its receive window is backed by two
caller-owned packet slots.

Build and flash the test firmware:

```sh
make bits-ram-transfer
make flash-bits PORT=/dev/arduino-uno
```

Run a deterministic 128-byte round trip:

```sh
make verify-bits PORT=/dev/arduino-uno
```

The PC tool also accepts exact file or hexadecimal input and generated objects
from 1 through 256 bytes:

```sh
PYTHONPATH=../../tools/python python3 -m wirespaces.bits_ram_transfer \
    --port /dev/arduino-uno --file image.bin

PYTHONPATH=../../tools/python python3 -m wirespaces.bits_ram_transfer \
    --port /dev/arduino-uno --hex "00 7e 7d ff 01"

PYTHONPATH=../../tools/python python3 -m wirespaces.bits_ram_transfer \
    --port /dev/arduino-uno --size 256 --seed 0x1234
```

Run the PC Compact BITS state-machine tests without hardware:

```sh
make test-bits
```

## BITS constrained receiver RAM test

The `bits_boot_profile_ram` firmware is the receiver-only precursor to the
4 KB bootloader. It directly composes the synchronous, one-window Compact BITS
engine with WireSpaces headers and UART HDLC. It does not link `Dispatcher`,
`Router`, queued ingress, timers, or a BITS transmitter.

Build and check both size gates:

```sh
make bits-boot-profile-ram
make bits-boot-profile-size
```

The first image contains the complete hardware-test instrumentation and must
remain at or below 4,096 bytes. The second removes RAM-pattern verification but
retains a minimal user-datagram service; it must remain at or below 3,350 bytes
before board-specific flash code is added. Both targets write ELF and map files
under `build/boot_profile/`.

Flash the hardware-test image through the existing Arduino bootloader and run
the PC test:

```sh
make flash-bits-boot-profile PORT=/dev/arduino-uno
make test-bits-boot-profile PORT=/dev/arduino-uno
```

The test verifies an exact BITS user-datagram echo, transfers a 256-byte object
where `data[i] == uint8_t(i)`, and repeats with 251 bytes to exercise a partial
final segment. The Arduino stores each object in RAM and returns its validation
result in a BITS user datagram.


## Planned WireSpaces demo

* 4-6 statically allocated packet buffers, sized to hold up to N=4 CAN PDUA payloads (the common upper bound for WS small packet size widespread compatibility)

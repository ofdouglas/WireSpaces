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

## Planned WireSpaces demo

* 4-6 statically allocated packet buffers, sized to hold up to N=4 CAN PDUA payloads (the common upper bound for WS small packet size widespread compatibility)
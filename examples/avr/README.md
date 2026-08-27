# WireSpaces demo using ATMEGA328P (Arduino UNO)

The example runs a minimal WireSpaces heartbeat publisher:

- ATmega328P Timer 0 supplies the millisecond clock.
- `HeartbeatService` creates one canonical PDU per second.
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

`receiver.py` incrementally decodes HDLC framing, parses the canonical
WireSpaces header, and emits each packet through Python's `logging` module.
Future formatting and log destinations can be added with standard logging
formatters and handlers without changing serial/framing code.

For a bounded live test that resets the UNO and receives one packet:

```sh
python3 receiver.py --port /dev/ttyACM0 --baud 115200 \
    --reset --count 1 --timeout 5
```

Run receiver unit tests:

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

Run the bounded hardware smoke test:

```sh
make verify
```

Expected output includes:

```text
WS packet wire=1 src=1 dst=broadcast qos=NORMAL ... payload[4]=...
```

The four payload bytes are the little-endian heartbeat uptime.

## Planned WireSpaces demo

* 4-6 statically allocated packet buffers, sized to hold up to N=4 CAN PDUA payloads (the common upper bound for WS small packet size widespread compatibility)
* Add HDLC CRC generation and validation.
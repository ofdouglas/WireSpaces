# WireSpaces demo using ATMEGA328P (Arduino UNO)

The example runs a minimal WireSpaces heartbeat publisher:

- ATmega328P Timer 0 supplies the millisecond clock.
- `HeartbeatService` creates one canonical PDU per second.
- The core Router selects the UART egress for Wire 1.
- The HDLC Link layer frames and byte-stuffs the canonical header and payload.
- UART0 transmits at 115200 baud through the UNO USB serial connection.

The heartbeat payload is temporarily fixed to `0xC0DEBABE`. The first-stage
HDLC framing does not yet append a CRC.

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

Verify the diagnostic heartbeat frame:

```sh
make verify
```

Expected output includes:

```text
wire=1 src=1 dst=255 endpoint=0xFFFE heartbeat=0xC0DEBABE
```

`config/verify-heartbeat.py` is deliberately a small bring-up decoder. A proper
WireSpaces receiver is outside this example's current scope.

## Planned WireSpaces demo

* 4-6 statically allocated packet buffers, sized to hold up to N=4 CAN PDUA payloads (the common upper bound for WS small packet size widespread compatibility)
* Add HDLC CRC generation and validation.
* Add a proper host receiver.
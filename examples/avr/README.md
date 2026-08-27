# WireSpaces demo using ATMEGA328P (Arduino UNO)

The current first-stage example is a minimal C++17 UART hello world. It writes
`Hello, world!` once at 9600 baud through the UNO's USB serial connection.

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

The generated files are `build/hello.elf` and `build/hello.hex`.

## Planned WireSpaces demo

* UART/HDLC link layer
* 4-6 statically allocated packet buffers, sized to hold up to N=4 CAN PDUA payloads (the common upper bound for WS small packet size widespread compatibility)
* TODO: services
 - only Heartbeat service for now
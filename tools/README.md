# Python tools

Set `PYTHONPATH=tools` when running from the repository root.

## Reusable protocol modules

- `wirespaces.packet`: canonical packet encoding, decoding, and display formatting.
- `wirespaces.hdlc`: streaming HDLC framing, escaping, and CRC validation.
- `wirespaces.bits`: Compact BITS message codecs and transmitter/receiver state
  machines. The implementation is split into `codec`, `transmitter`, and `receiver`.

These modules use only the Python standard library. BITS callers supply incoming
message bytes and monotonic timestamps to the state machines and send the returned
message bytes over their chosen transport.

```python
from wirespaces.packet import WireSpacesPacket
from wirespaces.hdlc import HdlcStreamDecoder, encode_hdlc_frame
from wirespaces.bits import CompactBitsReceiver, CompactBitsTransmitter
```

## CLI and bench adapters

The existing command entry points are unchanged:

```sh
PYTHONPATH=tools python3 -m wirespaces.receiver --port /dev/arduino-uno
PYTHONPATH=tools python3 -m wirespaces.bits_ram_transfer --size 128
PYTHONPATH=tools python3 -m wirespaces.led_control on
PYTHONPATH=tools python3 -m wirespaces.led_ramp
```

Serial commands require `pyserial`. `wirespaces.receiver` owns serial packet
reception; `wirespaces.bits_ram_transfer` owns command-line options and input loading.
The UNO RAM echo serial adapter lives in `wirespaces.bench.bits_ram_transfer`, with
shared device addresses and RAM limits in `wirespaces.bench.uno`.

The old protocol imports from `wirespaces.receiver` and
`wirespaces.bits_ram_transfer` remain available for compatibility. New consumers
should import from the protocol modules directly.

## Tests

Run all tool tests, including the simulated serial peer and CLI import compatibility:

```sh
PYTHONPATH=tools python3 -m unittest discover -s tools/test -v
```

Run only the protocol tests with site packages disabled (no serial package needed):

```sh
PYTHONPATH=tools:tools/test python3 -S -m unittest test_packet test_hdlc test_bits_ram_transfer -v
```

The historical `test_bits_ram_transfer.py` filename is retained for the existing
`make test-bits` target; its tests exercise the reusable BITS protocol directly.

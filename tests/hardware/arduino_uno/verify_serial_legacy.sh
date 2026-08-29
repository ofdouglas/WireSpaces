#!/usr/bin/env bash
set -euo pipefail

DEV="${1:-/dev/ttyACM0}"
BAUD="${2:-9600}"

export DEV BAUD
python3 - <<'PY'
import os
import serial
import time

dev = os.environ["DEV"]
baud = int(os.environ["BAUD"])

with serial.Serial(dev, baud, timeout=2) as ser:
    ser.dtr = False
    time.sleep(0.05)
    ser.dtr = True
    time.sleep(0.3)
    data = ser.read(256)

text = data.decode("utf-8", errors="replace")
print(text, end="")

if "Hello, world!" not in text:
    raise SystemExit("expected 'Hello, world!' on serial output")
PY

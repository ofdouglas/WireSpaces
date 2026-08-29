#!/usr/bin/env bash
set -euo pipefail

RULE_NAME="99-arduino-uno.rules"
SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" && pwd)"
SOURCE="${SCRIPT_DIR}/${RULE_NAME}"
DEST="/etc/udev/rules.d/${RULE_NAME}"

if [[ ! -f "${SOURCE}" ]]; then
    echo "missing rule file: ${SOURCE}" >&2
    exit 1
fi

sudo cp "${SOURCE}" "${DEST}"
sudo chmod 644 "${DEST}"
sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=tty

echo "Installed ${DEST}"
echo "Unplug and replug the Arduino, or run: sudo udevadm trigger --subsystem-match=tty"
echo "Then use /dev/arduino-uno (or /dev/ttyACM0) and ensure your user is in group dialout:"
echo "  sudo usermod -aG dialout \"${USER}\""

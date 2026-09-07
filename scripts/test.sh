#!/usr/bin/env bash
# Run all device-independent checks, including generated wiring and AVR builds.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
BUILD_TYPE="${BUILD_TYPE:-Debug}"
if [[ -x "$ROOT/.venv/bin/python" ]]; then
    PYTHON="${PYTHON:-$ROOT/.venv/bin/python}"
else
    PYTHON="${PYTHON:-python3}"
fi
export NODE="${NODE:-node}"

# Fail before building instead of silently skipping a subsystem. Setup is explicit;
# this script never installs system/Python packages or accesses connected devices.
for executable in "$PYTHON" "$NODE" cmake make g++ avr-g++ avr-objcopy avr-size; do
    if ! command -v "$executable" >/dev/null 2>&1; then
        echo "Missing required tool: $executable (see CONTRIBUTING.md)" >&2
        exit 1
    fi
done
"$NODE" --version
"$PYTHON" -c 'import yaml, networkx, pydantic, jsonschema, serial, can'
export PYTHONPATH="$ROOT/tools${PYTHONPATH:+:$PYTHONPATH}"

cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DBUILD_TESTING=ON -DWIRESPACES_BUILD_CORE_TESTS=ON \
    -DWIRESPACES_BUILD_LIBRARY_TESTS=ON -DWIRESPACES_BUILD_SIMULATOR=ON
cmake --build "$BUILD_DIR"
ctest --test-dir "$BUILD_DIR" --output-on-failure
"$PYTHON" -m unittest discover -s "$ROOT/codegen/tests" -p 'test_*.py' -v
"$PYTHON" -m unittest discover -s "$ROOT/tools/test" -p 'test_*.py' -v
"$PYTHON" -m unittest discover -s "$ROOT/tests/hardware/arduino_uno/test" -p 'test_*.py' -v
make -C "$ROOT/examples/arduino-uno" CODEGEN_PYTHON="$PYTHON" \
    all bits-ram-transfer bits-boot-profile-ram bits-boot-profile-size mcp2515-can-test

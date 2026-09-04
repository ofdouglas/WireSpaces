#!/usr/bin/env bash
# Generate compile_commands.json files for clangd (host CMake + Arduino Uno).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
BUILD_TYPE="${BUILD_TYPE:-Debug}"
ARDUINO_DIR="$ROOT/examples/arduino-uno"

cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cp "$BUILD_DIR/compile_commands.json" "$ROOT/compile_commands.json"
python3 "$ARDUINO_DIR/gen_compile_commands.py"

echo "Wrote $ROOT/compile_commands.json (host)"
echo "Wrote $ARDUINO_DIR/compile_commands.json (Arduino Uno)"

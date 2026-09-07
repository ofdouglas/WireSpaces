#!/usr/bin/env bash
# Generate build/compile_commands.json for clangd (host CMake + Arduino Uno).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
BUILD_TYPE="${BUILD_TYPE:-Debug}"

python3 "$ROOT/scripts/gen_compile_commands.py" "$BUILD_DIR" "$BUILD_TYPE"

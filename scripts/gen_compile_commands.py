#!/usr/bin/env python3
"""Merge CMake and Arduino Uno compile commands into build/compile_commands.json."""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = Path(
    sys.argv[1] if len(sys.argv) > 1 else ROOT / "build"
).resolve()
ARDUINO_DIR = ROOT / "examples" / "arduino-uno"
TOOLCHAIN_COMPAT = ROOT / "platform" / "avr" / "toolchain_compat"
WIRESPACES_INCLUDE = ROOT / "lib"

MCU = "atmega328p"
F_CPU = "16000000UL"

BASE_CPPFLAGS = (
    f"-DF_CPU={F_CPU} "
    f"-I{TOOLCHAIN_COMPAT} "
    f"-I{ROOT} "
    f"-I{WIRESPACES_INCLUDE}"
)
BASE_CXXFLAGS = (
    f"-std=c++17 -mmcu={MCU} -Os -Wall -Wextra -Wpedantic "
    "-ffunction-sections -fdata-sections -fno-exceptions -fno-rtti "
    "-fno-threadsafe-statics"
)
BOOT_PROFILE_CXXFLAGS = (
    f"{BASE_CXXFLAGS} -flto -mcall-prologues -mrelax "
    "-DWIRESPACES_BITS_RECEIVER_MAX_WINDOW_WIDTH=1"
)

ARDUINO_SOURCES = (
    ("main.cpp", BASE_CXXFLAGS),
    ("bsp.cpp", BASE_CXXFLAGS),
    ("bits_ram_transfer.cpp", BASE_CXXFLAGS),
    ("mcp2515_can_test.cpp", BASE_CXXFLAGS),
    ("bits_boot_profile_ram.cpp", BOOT_PROFILE_CXXFLAGS),
)


def arduino_entry(source: Path, cxxflags: str) -> dict[str, str]:
    source = source.resolve()
    command = (
        f"avr-g++ {BASE_CPPFLAGS} {cxxflags} "
        f"-c {source} -o {ARDUINO_DIR / 'build' / (source.stem + '.o')}"
    )
    return {
        "directory": str(ARDUINO_DIR),
        "command": command,
        "file": str(source),
    }


def arduino_entries() -> list[dict[str, str]]:
    return [
        arduino_entry(ARDUINO_DIR / name, cxxflags)
        for name, cxxflags in ARDUINO_SOURCES
        if (ARDUINO_DIR / name).is_file()
    ]


def merge_compile_commands() -> int:
    cmake_db = BUILD_DIR / "compile_commands.json"
    if not cmake_db.is_file():
        print(f"Missing {cmake_db}; run cmake first.", file=sys.stderr)
        return 1

    with cmake_db.open(encoding="utf-8") as handle:
        entries = json.load(handle)

    by_file = {entry["file"]: entry for entry in entries}
    for entry in arduino_entries():
        by_file[entry["file"]] = entry

    merged = sorted(by_file.values(), key=lambda item: item["file"])
    cmake_db.write_text(json.dumps(merged, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {cmake_db} ({len(merged)} translation units)")
    return 0


def main() -> int:
    build_type = sys.argv[2] if len(sys.argv) > 2 else "Debug"
    subprocess.run(
        [
            "cmake",
            "-S",
            str(ROOT),
            "-B",
            str(BUILD_DIR),
            f"-DCMAKE_BUILD_TYPE={build_type}",
        ],
        check=True,
    )
    return merge_compile_commands()


if __name__ == "__main__":
    raise SystemExit(main())

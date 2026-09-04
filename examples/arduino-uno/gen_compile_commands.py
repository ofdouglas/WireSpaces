#!/usr/bin/env python3
"""Generate compile_commands.json entries for the Arduino Uno Makefile build."""

from __future__ import annotations

import json
from pathlib import Path

ARDUINO_DIR = Path(__file__).resolve().parent
WIRESPACES_ROOT = ARDUINO_DIR.parents[1]
TOOLCHAIN_COMPAT = WIRESPACES_ROOT / "platform" / "avr" / "toolchain_compat"
WIRESPACES_INCLUDE = WIRESPACES_ROOT / "lib"

MCU = "atmega328p"
F_CPU = "16000000UL"

BASE_CPPFLAGS = (
    f"-DF_CPU={F_CPU} "
    f"-I{TOOLCHAIN_COMPAT} "
    f"-I{WIRESPACES_ROOT} "
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


def compile_entry(source: Path, cppflags: str, cxxflags: str) -> dict[str, str]:
    source = source.resolve()
    command = (
        f"avr-g++ {cppflags} {cxxflags} "
        f"-c {source} -o {ARDUINO_DIR / 'build' / (source.stem + '.o')}"
    )
    return {
        "directory": str(ARDUINO_DIR),
        "command": command,
        "file": str(source),
    }


def main() -> None:
    entries = [
        compile_entry(ARDUINO_DIR / "main.cpp", BASE_CPPFLAGS, BASE_CXXFLAGS),
        compile_entry(
            ARDUINO_DIR / "bits_ram_transfer.cpp", BASE_CPPFLAGS, BASE_CXXFLAGS
        ),
        compile_entry(
            ARDUINO_DIR / "mcp2515_can_test.cpp", BASE_CPPFLAGS, BASE_CXXFLAGS
        ),
        compile_entry(
            ARDUINO_DIR / "bits_boot_profile_ram.cpp",
            BASE_CPPFLAGS,
            BOOT_PROFILE_CXXFLAGS,
        ),
    ]

    output = ARDUINO_DIR / "compile_commands.json"
    output.write_text(json.dumps(entries, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {output}")


if __name__ == "__main__":
    main()

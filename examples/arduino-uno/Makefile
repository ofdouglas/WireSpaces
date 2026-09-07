# Arduino Uno entry point: board/tool settings and build, flash, test modules.
# Run `make` for the demo; existing named targets work unchanged.
.DEFAULT_GOAL := all

# --- Layout -------------------------------------------------------------------
BUILD_DIR := build
BOOT_PROFILE_BUILD_DIR := $(BUILD_DIR)/boot_profile
WIRESPACES_ROOT := ../..
WS := $(WIRESPACES_ROOT)
WS_CORE := $(WS)/lib/wirespaces/core
WS_BITS := $(WS)/lib/wirespaces/transports/bits
AVR_PLATFORM := $(WS)/platform/avr
TOOLCHAIN_COMPAT := $(WS)/platform/avr/toolchain_compat
WIRESPACES_INCLUDE := $(WS)/lib
PYTHON_ROOT := $(WS)/tools
HARDWARE_TEST_ROOT := $(WS)/tests/hardware/arduino_uno
HDLC_TEST_SOURCE := $(WS)/lib/wirespaces/links/uart_hdlc/test/hdlc_test.cpp
CODEGEN_PYTHON ?= $(if $(wildcard $(WS)/codegen/.venv/bin/python),$(WS)/codegen/.venv/bin/python,python3)

# --- MCU / toolchain ----------------------------------------------------------
MCU := atmega328p
MCU_PART := ATMEGA328P
F_CPU := 16000000UL

CXX := avr-g++
HOST_CXX := g++
CC := avr-gcc
OBJCOPY := avr-objcopy
SIZE := avr-size

# PICkit ISP is validated with the contending MCP2515 disconnected from SPI.
FLASH_METHOD ?= pickit
AVRDUDE ?= avrdude
UPLOAD_BAUD ?= 115200
MPLABX_ROOT ?= /opt/microchip/mplabx/v6.35
IPECMD ?= $(MPLABX_ROOT)/mplab_platform/mplab_ipe/ipecmd.sh
PICKIT ?= PK5
AVR_ISP_SPEED ?= 2
FLASH_ATTEMPTS ?= 3

PORT ?= /dev/arduino-uno
CANTACT_INTERFACE ?= can0

# --- Boot profile limits ------------------------------------------------------
BOOT_PROFILE_RAM_FLASH_LIMIT := 4096
BOOT_PROFILE_SIZE_FLASH_LIMIT := 3350

# Build defines the artifacts consumed by flash and test targets.
include make/build.mk
include make/flash.mk
include make/test.mk

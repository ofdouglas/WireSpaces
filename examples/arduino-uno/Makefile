# --- Firmware targets ---------------------------------------------------------
TARGET := heartbeat
BITS_TARGET := bits_ram_transfer
CAN_TARGET := mcp2515_can_test
BOOT_PROFILE_RAM_TARGET := bits_boot_profile_ram
BOOT_PROFILE_SIZE_TARGET := bits_boot_profile_size

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

MPLABX_ROOT ?= /opt/microchip/mplabx/v6.35
IPECMD ?= $(MPLABX_ROOT)/mplab_platform/mplab_ipe/ipecmd.sh
PICKIT ?= PK5
AVR_ISP_SPEED ?= 0.125
FLASH_ATTEMPTS ?= 3

PORT ?= /dev/arduino-uno
CANTACT_INTERFACE ?= can0

CPPFLAGS := -DF_CPU=$(F_CPU) \
	-I$(TOOLCHAIN_COMPAT) \
	-I$(WS) \
	-I$(WIRESPACES_INCLUDE)
CXXFLAGS := -std=c++17 -mmcu=$(MCU) -Os -Wall -Wextra -Wpedantic \
	-ffunction-sections -fdata-sections -fno-exceptions -fno-rtti \
	-fno-threadsafe-statics -MMD -MP
LDFLAGS := -mmcu=$(MCU) -Wl,--gc-sections
BOOT_PROFILE_CXXFLAGS := $(CXXFLAGS) -flto -mcall-prologues -mrelax \
	-DWIRESPACES_BITS_RECEIVER_MAX_WINDOW_WIDTH=1
BOOT_PROFILE_LDFLAGS := $(LDFLAGS) -flto -mcall-prologues -mrelax
HOST_CPPFLAGS := -I$(WIRESPACES_INCLUDE)

COMPILE = $(CXX) $(CPPFLAGS) $(CXXFLAGS) -c
BOOT_COMPILE = $(CXX) $(CPPFLAGS) $(BOOT_PROFILE_CXXFLAGS) -c

# --- Boot profile limits ------------------------------------------------------
BOOT_PROFILE_RAM_FLASH_LIMIT := 4096
BOOT_PROFILE_SIZE_FLASH_LIMIT := 3350

# --- Object inventory ---------------------------------------------------------
CORE_OBJS := dispatch header host packet router
PLATFORM_OBJS := builtin_led millisecond_clock
BITS_LIB_OBJS := bits_transport bits_receiver_engine bits_codec

objs = $(addprefix $(1)/,$(addsuffix .o,$(2)))
local_obj = $(BUILD_DIR)/$(1).o

OBJECTS := $(call local_obj,main) \
	$(call objs,$(BUILD_DIR),$(PLATFORM_OBJS)) \
	$(call objs,$(BUILD_DIR),$(CORE_OBJS))
BITS_OBJECTS := $(call local_obj,bits_ram_transfer) \
	$(call objs,$(BUILD_DIR),$(BITS_LIB_OBJS)) \
	$(call local_obj,millisecond_clock) \
	$(call objs,$(BUILD_DIR),$(CORE_OBJS))
CAN_OBJECTS := $(call local_obj,mcp2515_can_test)

BOOT_PROFILE_COMMON_OBJS := receiver_engine codec header packet
BOOT_PROFILE_RAM_OBJECTS := $(BOOT_PROFILE_BUILD_DIR)/ram_test.o \
	$(call objs,$(BOOT_PROFILE_BUILD_DIR),$(BOOT_PROFILE_COMMON_OBJS))
BOOT_PROFILE_SIZE_OBJECTS := $(BOOT_PROFILE_BUILD_DIR)/size_profile.o \
	$(call objs,$(BOOT_PROFILE_BUILD_DIR),$(BOOT_PROFILE_COMMON_OBJS))

ALL_OBJECTS := $(OBJECTS) $(BITS_OBJECTS) $(CAN_OBJECTS) \
	$(BOOT_PROFILE_RAM_OBJECTS) $(BOOT_PROFILE_SIZE_OBJECTS)
DEPENDENCIES := $(ALL_OBJECTS:.o=.d)

# --- Artifact paths -----------------------------------------------------------
elf = $(BUILD_DIR)/$(1).elf
hex = $(BUILD_DIR)/$(1).hex

ELF := $(call elf,$(TARGET))
HEX := $(call hex,$(TARGET))
BITS_ELF := $(call elf,$(BITS_TARGET))
BITS_HEX := $(call hex,$(BITS_TARGET))
CAN_ELF := $(call elf,$(CAN_TARGET))
CAN_HEX := $(call hex,$(CAN_TARGET))
BOOT_PROFILE_RAM_ELF := $(BOOT_PROFILE_BUILD_DIR)/$(BOOT_PROFILE_RAM_TARGET).elf
BOOT_PROFILE_RAM_HEX := $(BOOT_PROFILE_BUILD_DIR)/$(BOOT_PROFILE_RAM_TARGET).hex
BOOT_PROFILE_RAM_MAP := $(BOOT_PROFILE_BUILD_DIR)/$(BOOT_PROFILE_RAM_TARGET).map
BOOT_PROFILE_SIZE_ELF := $(BOOT_PROFILE_BUILD_DIR)/$(BOOT_PROFILE_SIZE_TARGET).elf
BOOT_PROFILE_SIZE_MAP := $(BOOT_PROFILE_BUILD_DIR)/$(BOOT_PROFILE_SIZE_TARGET).map

WIRING_OBJECTS := $(call local_obj,main) $(call local_obj,bits_ram_transfer) \
	$(call local_obj,mcp2515_can_test) \
	$(BOOT_PROFILE_BUILD_DIR)/ram_test.o $(BOOT_PROFILE_BUILD_DIR)/size_profile.o

# --- Phony targets ------------------------------------------------------------
.PHONY: all bits-boot-profile-ram bits-boot-profile-size flash-bits-boot-profile \
	test-bits-boot-profile
.PHONY: bits-ram-transfer bits-size clean flash flash-bits flash-mcp2515-can \
	generate-wiring led-off led-on led-ramp mcp2515-can-test receive size \
	test-bits test-can-harness test-codegen test-hdlc test-ingress test-mcp2515-can \
	test-ping test-receiver test-spi-can test-stack verify verify-bits

all: $(HEX) size

# --- Codegen ------------------------------------------------------------------
generate-wiring: demo_wiring.h

demo_wiring.h: demo.yaml $(wildcard $(WS)/codegen/wiring_*.py) $(WS)/codegen/requirements.txt
	$(CODEGEN_PYTHON) $(WS)/codegen/wiring_codegen.py $< --local-host Arduino -o $@

test-codegen:
	$(CODEGEN_PYTHON) -m unittest discover -s $(WS)/codegen/tests -p 'test_*.py' -v

# --- Build directories ----------------------------------------------------------
$(BUILD_DIR) $(BOOT_PROFILE_BUILD_DIR):
	mkdir -p $@

# --- Compile rules ------------------------------------------------------------
$(WIRING_OBJECTS): demo_wiring.h

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	$(COMPILE) $< -o $@

$(BUILD_DIR)/%.o: $(WS_CORE)/%.cpp | $(BUILD_DIR)
	$(COMPILE) $< -o $@

$(BUILD_DIR)/%.o: $(AVR_PLATFORM)/%.cpp | $(BUILD_DIR)
	$(COMPILE) $< -o $@

$(BUILD_DIR)/bits_transport.o: $(WS_BITS)/bits.cpp | $(BUILD_DIR)
	$(COMPILE) $< -o $@

$(BUILD_DIR)/bits_receiver_engine.o: $(WS_BITS)/receiver_engine.cpp | $(BUILD_DIR)
	$(COMPILE) $< -o $@

$(BUILD_DIR)/bits_codec.o: $(WS_BITS)/codec.cpp | $(BUILD_DIR)
	$(COMPILE) $< -o $@

$(BOOT_PROFILE_BUILD_DIR)/ram_test.o: bits_boot_profile_ram.cpp | $(BOOT_PROFILE_BUILD_DIR)
	$(BOOT_COMPILE) $< -o $@

$(BOOT_PROFILE_BUILD_DIR)/size_profile.o: bits_boot_profile_ram.cpp | $(BOOT_PROFILE_BUILD_DIR)
	$(BOOT_COMPILE) -DWS_BITS_BOOT_PROFILE_SIZE_ONLY=1 $< -o $@

$(BOOT_PROFILE_BUILD_DIR)/%.o: $(WS_CORE)/%.cpp | $(BOOT_PROFILE_BUILD_DIR)
	$(BOOT_COMPILE) $< -o $@

$(BOOT_PROFILE_BUILD_DIR)/receiver_engine.o: $(WS_BITS)/receiver_engine.cpp | $(BOOT_PROFILE_BUILD_DIR)
	$(BOOT_COMPILE) $< -o $@

$(BOOT_PROFILE_BUILD_DIR)/codec.o: $(WS_BITS)/codec.cpp | $(BOOT_PROFILE_BUILD_DIR)
	$(BOOT_COMPILE) $< -o $@

# --- Link / hex ---------------------------------------------------------------
$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

$(BOOT_PROFILE_BUILD_DIR)/%.hex: $(BOOT_PROFILE_BUILD_DIR)/%.elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

$(ELF): $(OBJECTS)
	$(CXX) $(LDFLAGS) $^ -o $@

$(BITS_ELF): $(BITS_OBJECTS)
	$(CXX) $(LDFLAGS) $^ -o $@

$(CAN_ELF): $(CAN_OBJECTS)
	$(CXX) $(LDFLAGS) $^ -o $@

$(BOOT_PROFILE_RAM_ELF): $(BOOT_PROFILE_RAM_OBJECTS)
	$(CXX) $(BOOT_PROFILE_LDFLAGS) -Wl,-Map,$(BOOT_PROFILE_RAM_MAP),--cref $^ -o $@

$(BOOT_PROFILE_SIZE_ELF): $(BOOT_PROFILE_SIZE_OBJECTS)
	$(CXX) $(BOOT_PROFILE_LDFLAGS) -Wl,-Map,$(BOOT_PROFILE_SIZE_MAP),--cref $^ -o $@

# --- Size reporting -----------------------------------------------------------
define avr_size
	$(SIZE) --format=avr --mcu=$(MCU) $(1)
endef

size: $(ELF)
	$(call avr_size,$<)

bits-size: $(BITS_ELF)
	$(call avr_size,$<)

bits-ram-transfer: $(BITS_HEX) bits-size

mcp2515-can-test: $(CAN_HEX)
	$(call avr_size,$(CAN_ELF))

define boot_profile_size_check
	$(call avr_size,$(1))
	@flash_bytes=`$(SIZE) --format=berkeley $(1) | awk 'NR == 2 { print $$1 + $$2 }'`; \
	if [ "$$flash_bytes" -gt "$(2)" ]; then \
		echo "$(3) exceeds $(2) bytes"; exit 1; \
	fi
endef

bits-boot-profile-ram: $(BOOT_PROFILE_RAM_HEX)
	$(call boot_profile_size_check,$(BOOT_PROFILE_RAM_ELF),$(BOOT_PROFILE_RAM_FLASH_LIMIT),boot-profile RAM image)

bits-boot-profile-size: $(BOOT_PROFILE_SIZE_ELF)
	$(call boot_profile_size_check,$(BOOT_PROFILE_SIZE_ELF),$(BOOT_PROFILE_SIZE_FLASH_LIMIT),boot-profile core)

# --- Flash (PICkit 5 via ipecmd) ----------------------------------------------
# ipecmd.sh cd's to its install dir, so hex paths must be absolute.
# -TP selects a programmer by tool type; ATmega328P programming uses AVR ISP.
# PICkit 5 can intermittently fail an ISP write, so retry the complete operation.
define flash_hex
	@attempt=1; \
	while [ $$attempt -le $(FLASH_ATTEMPTS) ]; do \
		echo "Flashing $(notdir $(1)) (attempt $$attempt/$(FLASH_ATTEMPTS))"; \
		if $(IPECMD) \
			-TP$(PICKIT) \
			-P$(MCU_PART) \
			-ORIISP \
			-ORS$(AVR_ISP_SPEED) \
			-M \
			-F$(abspath $(1)); then \
			exit 0; \
		else \
			status=$$?; \
		fi; \
		attempt=$$((attempt + 1)); \
		if [ $$attempt -le $(FLASH_ATTEMPTS) ]; then \
			echo "Flash failed; retrying..."; \
			sleep 1; \
		fi; \
	done; \
	exit $$status
endef

flash: $(HEX)
	$(call flash_hex,$(HEX))

flash-bits: $(BITS_HEX)
	$(call flash_hex,$(BITS_HEX))

flash-bits-boot-profile: $(BOOT_PROFILE_RAM_HEX)
	$(call flash_hex,$(BOOT_PROFILE_RAM_HEX))

flash-mcp2515-can: $(CAN_HEX)
	$(call flash_hex,$(CAN_HEX))

# --- Host / serial tests --------------------------------------------------------
led-on:
	PYTHONPATH=$(PYTHON_ROOT) python3 -m wirespaces.led_control on --port $(PORT) --baud 115200

led-off:
	PYTHONPATH=$(PYTHON_ROOT) python3 -m wirespaces.led_control off --port $(PORT) --baud 115200

led-ramp:
	PYTHONPATH=$(PYTHON_ROOT) python3 -m wirespaces.led_ramp --port $(PORT) --baud 115200

receive:
	PYTHONPATH=$(PYTHON_ROOT) python3 -m wirespaces.receiver --port $(PORT) --baud 115200

test-hdlc: | $(BUILD_DIR)
	$(HOST_CXX) -std=c++17 -Wall -Wextra -Wpedantic $(HOST_CPPFLAGS) \
		$(HDLC_TEST_SOURCE) -o $(BUILD_DIR)/test_hdlc
	$(BUILD_DIR)/test_hdlc

test-receiver: test-hdlc
	PYTHONPATH=$(PYTHON_ROOT) python3 -m unittest discover -v \
		-s $(PYTHON_ROOT)/test -p 'test_*.py'

test-bits:
	PYTHONPATH=$(PYTHON_ROOT) python3 -m unittest discover -v \
		-s $(PYTHON_ROOT)/test -p 'test_bits_ram_transfer.py'

test-ping:
	PYTHONPATH=$(PYTHON_ROOT) python3 $(HARDWARE_TEST_ROOT)/ping_test.py \
		--port $(PORT) --baud 115200 --reset --timeout 5

test-ingress:
	PYTHONPATH=$(PYTHON_ROOT) python3 $(HARDWARE_TEST_ROOT)/ingress_test.py --port $(PORT)

test-can-harness:
	python3 -m unittest discover -s $(HARDWARE_TEST_ROOT)/test -p 'test_*.py' -v

test-spi-can:
	PYTHONPATH=$(PYTHON_ROOT) python3 $(HARDWARE_TEST_ROOT)/mcp2515_spi_test.py \
		--port $(PORT) --baud 115200 --timeout 5

test-bits-boot-profile:
	PYTHONPATH=$(PYTHON_ROOT) python3 $(HARDWARE_TEST_ROOT)/bits_boot_profile_ram_test.py \
		--port $(PORT) --baud 115200 --reset --timeout 10

test-mcp2515-can:
	python3 $(HARDWARE_TEST_ROOT)/mcp2515_can_test.py \
		--arduino-port $(PORT) --cantact-interface $(CANTACT_INTERFACE) --timeout 5

test-stack:
	PYTHONPATH=$(PYTHON_ROOT) python3 $(HARDWARE_TEST_ROOT)/stack_report_test.py \
		--port $(PORT) --baud 115200 --reset --timeout 5

verify:
	PYTHONPATH=$(PYTHON_ROOT) python3 -m wirespaces.receiver \
		--port $(PORT) --baud 115200 --reset --count 1 --timeout 5

verify-bits:
	PYTHONPATH=$(PYTHON_ROOT) python3 -m wirespaces.bits_ram_transfer \
		--port $(PORT) --baud 115200 --reset

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPENDENCIES)

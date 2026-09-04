TARGET := heartbeat
BITS_TARGET := bits_ram_transfer
CAN_TARGET := mcp2515_can_test
BOOT_PROFILE_RAM_TARGET := bits_boot_profile_ram
BOOT_PROFILE_SIZE_TARGET := bits_boot_profile_size
BOOT_PROFILE_BUILD_DIR = $(BUILD_DIR)/boot_profile
BOOT_PROFILE_RAM_FLASH_LIMIT := 4096
BOOT_PROFILE_SIZE_FLASH_LIMIT := 3350
BUILD_DIR := build
WIRESPACES_ROOT := ../..

MCU := atmega328p
F_CPU := 16000000UL

CXX := avr-g++
HOST_CXX := g++
CC := avr-gcc
OBJCOPY := avr-objcopy
SIZE := avr-size
AVRDUDE := avrdude

PORT ?= /dev/arduino-uno
CANTACT_INTERFACE ?= can0
PROGRAMMER ?= arduino
UPLOAD_BAUD ?= 115200

WIRESPACES_INCLUDE := $(WIRESPACES_ROOT)/lib
TOOLCHAIN_COMPAT := $(WIRESPACES_ROOT)/platform/avr/toolchain_compat
PYTHON_ROOT := $(WIRESPACES_ROOT)/tools
HARDWARE_TEST_ROOT := $(WIRESPACES_ROOT)/tests/hardware/arduino_uno
HDLC_TEST_SOURCE := $(WIRESPACES_ROOT)/lib/wirespaces/links/uart_hdlc/test/hdlc_test.cpp

CPPFLAGS := -DF_CPU=$(F_CPU) \
	-I$(TOOLCHAIN_COMPAT) \
	-I$(WIRESPACES_ROOT) \
	-I$(WIRESPACES_INCLUDE)
CXXFLAGS := -std=c++17 -mmcu=$(MCU) -Os -Wall -Wextra -Wpedantic \
	-ffunction-sections -fdata-sections -fno-exceptions -fno-rtti \
	-fno-threadsafe-statics -MMD -MP
LDFLAGS := -mmcu=$(MCU) -Wl,--gc-sections
BOOT_PROFILE_CXXFLAGS := $(CXXFLAGS) -flto -mcall-prologues -mrelax \
	-DWIRESPACES_BITS_RECEIVER_MAX_WINDOW_WIDTH=1
BOOT_PROFILE_LDFLAGS := $(LDFLAGS) -flto -mcall-prologues -mrelax
HOST_CPPFLAGS := -I$(WIRESPACES_INCLUDE)

ELF := $(BUILD_DIR)/$(TARGET).elf
HEX := $(BUILD_DIR)/$(TARGET).hex
BITS_ELF := $(BUILD_DIR)/$(BITS_TARGET).elf
BITS_HEX := $(BUILD_DIR)/$(BITS_TARGET).hex
CAN_ELF := $(BUILD_DIR)/$(CAN_TARGET).elf
CAN_HEX := $(BUILD_DIR)/$(CAN_TARGET).hex
BOOT_PROFILE_RAM_ELF := $(BOOT_PROFILE_BUILD_DIR)/$(BOOT_PROFILE_RAM_TARGET).elf
BOOT_PROFILE_RAM_HEX := $(BOOT_PROFILE_BUILD_DIR)/$(BOOT_PROFILE_RAM_TARGET).hex
BOOT_PROFILE_RAM_MAP := $(BOOT_PROFILE_BUILD_DIR)/$(BOOT_PROFILE_RAM_TARGET).map
BOOT_PROFILE_SIZE_ELF := $(BOOT_PROFILE_BUILD_DIR)/$(BOOT_PROFILE_SIZE_TARGET).elf
BOOT_PROFILE_SIZE_MAP := $(BOOT_PROFILE_BUILD_DIR)/$(BOOT_PROFILE_SIZE_TARGET).map
OBJECTS := \
	$(BUILD_DIR)/main.o \
	$(BUILD_DIR)/builtin_led.o \
	$(BUILD_DIR)/millisecond_clock.o \
	$(BUILD_DIR)/dispatch.o \
	$(BUILD_DIR)/header.o \
	$(BUILD_DIR)/host.o \
	$(BUILD_DIR)/packet.o \
	$(BUILD_DIR)/router.o
BITS_OBJECTS := \
	$(BUILD_DIR)/bits_ram_transfer.o \
	$(BUILD_DIR)/bits_transport.o \
	$(BUILD_DIR)/bits_receiver_engine.o \
	$(BUILD_DIR)/bits_codec.o \
	$(BUILD_DIR)/millisecond_clock.o \
	$(BUILD_DIR)/dispatch.o \
	$(BUILD_DIR)/header.o \
	$(BUILD_DIR)/host.o \
	$(BUILD_DIR)/packet.o \
	$(BUILD_DIR)/router.o
CAN_OBJECTS := $(BUILD_DIR)/mcp2515_can_test.o
BOOT_PROFILE_COMMON_OBJECTS := \
	$(BOOT_PROFILE_BUILD_DIR)/receiver_engine.o \
	$(BOOT_PROFILE_BUILD_DIR)/codec.o \
	$(BOOT_PROFILE_BUILD_DIR)/header.o \
	$(BOOT_PROFILE_BUILD_DIR)/packet.o
BOOT_PROFILE_RAM_OBJECTS := $(BOOT_PROFILE_BUILD_DIR)/ram_test.o $(BOOT_PROFILE_COMMON_OBJECTS)
BOOT_PROFILE_SIZE_OBJECTS := $(BOOT_PROFILE_BUILD_DIR)/size_profile.o $(BOOT_PROFILE_COMMON_OBJECTS)
DEPENDENCIES := $(OBJECTS:.o=.d) $(BITS_OBJECTS:.o=.d) $(CAN_OBJECTS:.o=.d) \
	$(BOOT_PROFILE_RAM_OBJECTS:.o=.d) $(BOOT_PROFILE_SIZE_OBJECTS:.o=.d)

.PHONY: bits-boot-profile-ram bits-boot-profile-size flash-bits-boot-profile \
	test-bits-boot-profile
.PHONY: all bits-ram-transfer bits-size clean flash flash-bits flash-mcp2515-can \
	led-off led-on led-ramp mcp2515-can-test receive size test-bits test-hdlc \
	test-mcp2515-can test-ping test-receiver test-spi-can test-stack verify \
	verify-bits

all: $(HEX) size

$(BOOT_PROFILE_BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR):
	mkdir -p $@

$(BOOT_PROFILE_BUILD_DIR)/ram_test.o: bits_boot_profile_ram.cpp | $(BOOT_PROFILE_BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(BOOT_PROFILE_CXXFLAGS) -c $< -o $@

$(BOOT_PROFILE_BUILD_DIR)/size_profile.o: bits_boot_profile_ram.cpp | $(BOOT_PROFILE_BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(BOOT_PROFILE_CXXFLAGS) -DWS_BITS_BOOT_PROFILE_SIZE_ONLY=1 -c $< -o $@

$(BOOT_PROFILE_BUILD_DIR)/receiver_engine.o: $(WIRESPACES_ROOT)/lib/wirespaces/transports/bits/receiver_engine.cpp | $(BOOT_PROFILE_BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(BOOT_PROFILE_CXXFLAGS) -c $< -o $@

$(BOOT_PROFILE_BUILD_DIR)/codec.o: $(WIRESPACES_ROOT)/lib/wirespaces/transports/bits/codec.cpp | $(BOOT_PROFILE_BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(BOOT_PROFILE_CXXFLAGS) -c $< -o $@

$(BOOT_PROFILE_BUILD_DIR)/header.o: $(WIRESPACES_ROOT)/lib/wirespaces/core/header.cpp | $(BOOT_PROFILE_BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(BOOT_PROFILE_CXXFLAGS) -c $< -o $@

$(BOOT_PROFILE_BUILD_DIR)/packet.o: $(WIRESPACES_ROOT)/lib/wirespaces/core/packet.cpp | $(BOOT_PROFILE_BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(BOOT_PROFILE_CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/main.o: main.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/bits_ram_transfer.o: bits_ram_transfer.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
$(BUILD_DIR)/mcp2515_can_test.o: mcp2515_can_test.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@


$(BUILD_DIR)/bits_transport.o: $(WIRESPACES_ROOT)/lib/wirespaces/transports/bits/bits.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/bits_receiver_engine.o: $(WIRESPACES_ROOT)/lib/wirespaces/transports/bits/receiver_engine.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/bits_codec.o: $(WIRESPACES_ROOT)/lib/wirespaces/transports/bits/codec.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/builtin_led.o: $(WIRESPACES_ROOT)/platform/avr/builtin_led.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/millisecond_clock.o: $(WIRESPACES_ROOT)/platform/avr/millisecond_clock.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(WIRESPACES_ROOT)/lib/wirespaces/core/%.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(ELF): $(OBJECTS)
	$(CXX) $(LDFLAGS) $^ -o $@

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex -R .eeprom $< $@

$(BITS_ELF): $(BITS_OBJECTS)
	$(CXX) $(LDFLAGS) $^ -o $@

$(BITS_HEX): $(BITS_ELF)
	$(OBJCOPY) -O ihex -R .eeprom $< $@

$(BOOT_PROFILE_RAM_ELF): $(BOOT_PROFILE_RAM_OBJECTS)
	$(CXX) $(BOOT_PROFILE_LDFLAGS) -Wl,-Map,$(BOOT_PROFILE_RAM_MAP),--cref $^ -o $@

$(BOOT_PROFILE_RAM_HEX): $(BOOT_PROFILE_RAM_ELF)
	$(OBJCOPY) -O ihex -R .eeprom $< $@

$(BOOT_PROFILE_SIZE_ELF): $(BOOT_PROFILE_SIZE_OBJECTS)
	$(CXX) $(BOOT_PROFILE_LDFLAGS) -Wl,-Map,$(BOOT_PROFILE_SIZE_MAP),--cref $^ -o $@

$(CAN_ELF): $(CAN_OBJECTS)
	$(CXX) $(LDFLAGS) $^ -o $@

$(CAN_HEX): $(CAN_ELF)
	$(OBJCOPY) -O ihex -R .eeprom $< $@

bits-boot-profile-ram: $(BOOT_PROFILE_RAM_HEX)
	$(SIZE) --format=avr --mcu=$(MCU) $(BOOT_PROFILE_RAM_ELF)
	@flash_bytes=`$(SIZE) --format=berkeley $(BOOT_PROFILE_RAM_ELF) | awk 'NR == 2 { print $$1 + $$2 }'`; \
	if [ "$$flash_bytes" -gt "$(BOOT_PROFILE_RAM_FLASH_LIMIT)" ]; then \
		echo "boot-profile RAM image exceeds $(BOOT_PROFILE_RAM_FLASH_LIMIT) bytes"; exit 1; \
	fi

bits-boot-profile-size: $(BOOT_PROFILE_SIZE_ELF)
	$(SIZE) --format=avr --mcu=$(MCU) $(BOOT_PROFILE_SIZE_ELF)
	@flash_bytes=`$(SIZE) --format=berkeley $(BOOT_PROFILE_SIZE_ELF) | awk 'NR == 2 { print $$1 + $$2 }'`; \
	if [ "$$flash_bytes" -gt "$(BOOT_PROFILE_SIZE_FLASH_LIMIT)" ]; then \
		echo "boot-profile core exceeds $(BOOT_PROFILE_SIZE_FLASH_LIMIT) bytes"; exit 1; \
	fi

mcp2515-can-test: $(CAN_HEX)
	$(SIZE) --format=avr --mcu=$(MCU) $(CAN_ELF)

size: $(ELF)
	$(SIZE) --format=avr --mcu=$(MCU) $<

bits-ram-transfer: $(BITS_HEX) bits-size

bits-size: $(BITS_ELF)
	$(SIZE) --format=avr --mcu=$(MCU) $<

flash: $(HEX)
	$(AVRDUDE) -p $(MCU) -c $(PROGRAMMER) -P $(PORT) \
		-b $(UPLOAD_BAUD) -D -U flash:w:$(HEX):i

flash-bits: $(BITS_HEX)
	$(AVRDUDE) -p $(MCU) -c $(PROGRAMMER) -P $(PORT) \
		-b $(UPLOAD_BAUD) -D -U flash:w:$(BITS_HEX):i

flash-bits-boot-profile: $(BOOT_PROFILE_RAM_HEX)
	$(AVRDUDE) -p $(MCU) -c $(PROGRAMMER) -P $(PORT) \
		-b $(UPLOAD_BAUD) -D -U flash:w:$(BOOT_PROFILE_RAM_HEX):i

flash-mcp2515-can: $(CAN_HEX)
	$(AVRDUDE) -p $(MCU) -c $(PROGRAMMER) -P $(PORT) \
		-b $(UPLOAD_BAUD) -D -U flash:w:$(CAN_HEX):i

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

TARGET := heartbeat
BITS_TARGET := bits_ram_transfer
CAN_TARGET := mcp2515_can_test
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

PORT ?= /dev/ttyACM0
CANTACT_PORT ?= /dev/ttyACM0
PROGRAMMER ?= arduino
UPLOAD_BAUD ?= 115200

WIRESPACES_INCLUDE := $(WIRESPACES_ROOT)/lib
TOOLCHAIN_COMPAT := $(WIRESPACES_ROOT)/platform/avr/toolchain_compat
PYTHON_ROOT := $(WIRESPACES_ROOT)/tools/python
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
HOST_CPPFLAGS := -I$(WIRESPACES_INCLUDE)

ELF := $(BUILD_DIR)/$(TARGET).elf
HEX := $(BUILD_DIR)/$(TARGET).hex
BITS_ELF := $(BUILD_DIR)/$(BITS_TARGET).elf
BITS_HEX := $(BUILD_DIR)/$(BITS_TARGET).hex
CAN_ELF := $(BUILD_DIR)/$(CAN_TARGET).elf
CAN_HEX := $(BUILD_DIR)/$(CAN_TARGET).hex
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
	$(BUILD_DIR)/bits_codec.o \
	$(BUILD_DIR)/millisecond_clock.o \
	$(BUILD_DIR)/dispatch.o \
	$(BUILD_DIR)/header.o \
	$(BUILD_DIR)/host.o \
	$(BUILD_DIR)/packet.o \
	$(BUILD_DIR)/router.o
CAN_OBJECTS := $(BUILD_DIR)/mcp2515_can_test.o
DEPENDENCIES := $(OBJECTS:.o=.d) $(BITS_OBJECTS:.o=.d) $(CAN_OBJECTS:.o=.d)

.PHONY: all bits-ram-transfer bits-size clean flash flash-bits flash-mcp2515-can \
	led-off led-on led-ramp mcp2515-can-test receive size test-bits test-hdlc \
	test-mcp2515-can test-ping test-receiver test-spi-can test-stack verify \
	verify-bits

all: $(HEX) size

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/main.o: main.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/bits_ram_transfer.o: bits_ram_transfer.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
$(BUILD_DIR)/mcp2515_can_test.o: mcp2515_can_test.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@


$(BUILD_DIR)/bits_transport.o: $(WIRESPACES_ROOT)/lib/wirespaces/transports/bits/bits.cpp | $(BUILD_DIR)
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

$(CAN_ELF): $(CAN_OBJECTS)
	$(CXX) $(LDFLAGS) $^ -o $@

$(CAN_HEX): $(CAN_ELF)
	$(OBJCOPY) -O ihex -R .eeprom $< $@

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

test-mcp2515-can:
	python3 $(HARDWARE_TEST_ROOT)/mcp2515_can_test.py \
		--arduino-port $(PORT) --cantact-port $(CANTACT_PORT) --timeout 5

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

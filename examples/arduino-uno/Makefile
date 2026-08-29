TARGET := heartbeat
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
PROGRAMMER ?= arduino
UPLOAD_BAUD ?= 115200

CPP_INCLUDE := $(WIRESPACES_ROOT)/cpp
TOOLCHAIN_COMPAT := $(WIRESPACES_ROOT)/platform/avr/toolchain_compat
PYTHON_ROOT := $(WIRESPACES_ROOT)/tools/python
HARDWARE_TEST_ROOT := $(WIRESPACES_ROOT)/tests/hardware/arduino_uno
HDLC_TEST_SOURCE := $(WIRESPACES_ROOT)/cpp/links/uart_hdlc/test/hdlc_test.cpp

CPPFLAGS := -DF_CPU=$(F_CPU) \
	-I$(TOOLCHAIN_COMPAT) \
	-I$(WIRESPACES_ROOT) \
	-I$(CPP_INCLUDE)
CXXFLAGS := -std=c++17 -mmcu=$(MCU) -Os -Wall -Wextra -Wpedantic \
	-ffunction-sections -fdata-sections -fno-exceptions -fno-rtti \
	-fno-threadsafe-statics -MMD -MP
LDFLAGS := -mmcu=$(MCU) -Wl,--gc-sections
HOST_CPPFLAGS := -I$(CPP_INCLUDE)

ELF := $(BUILD_DIR)/$(TARGET).elf
HEX := $(BUILD_DIR)/$(TARGET).hex
OBJECTS := \
	$(BUILD_DIR)/main.o \
	$(BUILD_DIR)/dispatch.o \
	$(BUILD_DIR)/header.o \
	$(BUILD_DIR)/host.o \
	$(BUILD_DIR)/packet.o \
	$(BUILD_DIR)/router.o
DEPENDENCIES := $(OBJECTS:.o=.d)

.PHONY: all clean flash led-off led-on led-ramp receive size test-hdlc test-ping test-receiver test-stack verify

all: $(HEX) size

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/main.o: main.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(WIRESPACES_ROOT)/core/%.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(ELF): $(OBJECTS)
	$(CXX) $(LDFLAGS) $^ -o $@

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex -R .eeprom $< $@

size: $(ELF)
	$(SIZE) --format=avr --mcu=$(MCU) $<

flash: $(HEX)
	$(AVRDUDE) -p $(MCU) -c $(PROGRAMMER) -P $(PORT) \
		-b $(UPLOAD_BAUD) -D -U flash:w:$(HEX):i

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

test-ping:
	PYTHONPATH=$(PYTHON_ROOT) python3 $(HARDWARE_TEST_ROOT)/ping_test.py \
		--port $(PORT) --baud 115200 --reset --timeout 5

test-stack:
	PYTHONPATH=$(PYTHON_ROOT) python3 $(HARDWARE_TEST_ROOT)/stack_report_test.py \
		--port $(PORT) --baud 115200 --reset --timeout 5

verify:
	PYTHONPATH=$(PYTHON_ROOT) python3 -m wirespaces.receiver \
		--port $(PORT) --baud 115200 --reset --count 1 --timeout 5

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPENDENCIES)

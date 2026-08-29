TARGET := heartbeat
BUILD_DIR := build
WIRESPACES_ROOT := ../..
DESIGN_ROOT := ../../..

MCU := atmega328p
F_CPU := 16000000UL

CXX := avr-g++
CC := avr-gcc
OBJCOPY := avr-objcopy
SIZE := avr-size
AVRDUDE := avrdude

PORT ?= /dev/ttyACM0
PROGRAMMER ?= arduino
UPLOAD_BAUD ?= 115200

CPPFLAGS := -DF_CPU=$(F_CPU) \
	-Icompat \
	-I$(DESIGN_ROOT) \
	-I$(WIRESPACES_ROOT)/cpp \
	-I$(WIRESPACES_ROOT)/core/inc
CXXFLAGS := -std=c++17 -mmcu=$(MCU) -Os -Wall -Wextra -Wpedantic \
	-ffunction-sections -fdata-sections -fno-exceptions -fno-rtti \
	-fno-threadsafe-statics
CFLAGS := -std=c11 -mmcu=$(MCU) -Os -Wall -Wextra -Wpedantic \
	-ffunction-sections -fdata-sections
LDFLAGS := -mmcu=$(MCU) -Wl,--gc-sections

ELF := $(BUILD_DIR)/$(TARGET).elf
HEX := $(BUILD_DIR)/$(TARGET).hex
OBJECTS := \
	$(BUILD_DIR)/main.o \
	$(BUILD_DIR)/dispatch.o \
	$(BUILD_DIR)/header.o \
	$(BUILD_DIR)/host.o \
	$(BUILD_DIR)/packet.o \
	$(BUILD_DIR)/router.o

.PHONY: all clean flash led-off led-on led-ramp receive size test-ping test-receiver test-stack verify

all: $(HEX) size

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/main.o: main.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/dispatch.o: $(WIRESPACES_ROOT)/core/src/dispatch.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/header.o: $(WIRESPACES_ROOT)/core/src/header.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/host.o: $(WIRESPACES_ROOT)/core/src/host.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/packet.o: $(WIRESPACES_ROOT)/core/src/packet.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/router.o: $(WIRESPACES_ROOT)/core/src/router.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

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
	python3 led_control.py on --port $(PORT) --baud 115200

led-off:
	python3 led_control.py off --port $(PORT) --baud 115200

led-ramp:
	python3 led_ramp.py --port $(PORT) --baud 115200

receive:
	python3 receiver.py --port $(PORT) --baud 115200

test-receiver:
	python3 -m unittest -v test_receiver.py

test-ping:
	python3 ping_test.py --port $(PORT) --baud 115200 \
		--reset --timeout 5

test-stack:
	python3 stack_report_test.py --port $(PORT) --baud 115200 \
		--reset --timeout 5

verify:
	python3 receiver.py --port $(PORT) --baud 115200 \
		--reset --count 1 --timeout 5

clean:
	rm -rf $(BUILD_DIR)

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
	$(BUILD_DIR)/header.o \
	$(BUILD_DIR)/packet.o \
	$(BUILD_DIR)/router.o

.PHONY: all clean flash size verify

all: $(HEX) size

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/main.o: main.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/header.o: $(WIRESPACES_ROOT)/core/src/header.c | $(BUILD_DIR)
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

verify:
	python3 config/verify-heartbeat.py $(PORT) 115200

clean:
	rm -rf $(BUILD_DIR)

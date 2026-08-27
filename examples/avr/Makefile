TARGET := hello
BUILD_DIR := build

MCU := atmega328p
F_CPU := 16000000UL

CXX := avr-g++
OBJCOPY := avr-objcopy
SIZE := avr-size
AVRDUDE := avrdude

PORT ?= /dev/ttyACM0
PROGRAMMER ?= arduino
UPLOAD_BAUD ?= 115200

CPPFLAGS := -DF_CPU=$(F_CPU)
CXXFLAGS := -std=c++17 -mmcu=$(MCU) -Os -Wall -Wextra -Wpedantic \
	-ffunction-sections -fdata-sections -fno-exceptions -fno-rtti \
	-fno-threadsafe-statics
LDFLAGS := -mmcu=$(MCU) -Wl,--gc-sections

ELF := $(BUILD_DIR)/$(TARGET).elf
HEX := $(BUILD_DIR)/$(TARGET).hex
OBJECTS := $(BUILD_DIR)/main.o

.PHONY: all clean flash size

all: $(HEX) size

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/main.o: main.cpp | $(BUILD_DIR)
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

clean:
	rm -rf $(BUILD_DIR)

# Firmware inventory, compile/link rules, generated wiring and AVR size gates.
# Included by ../Makefile; run make from the arduino-uno directory.

.PHONY: all generate-wiring size bits-size bits-ram-transfer mcp2515-can-test \
	bits-boot-profile-ram bits-boot-profile-size clean

# --- Firmware targets ---------------------------------------------------------
TARGET := heartbeat
BITS_TARGET := bits_ram_transfer
CAN_TARGET := mcp2515_can_test
BOOT_PROFILE_RAM_TARGET := bits_boot_profile_ram
BOOT_PROFILE_SIZE_TARGET := bits_boot_profile_size

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


# --- Object inventory ---------------------------------------------------------
CORE_OBJS := dispatch domain host packet router
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

BOOT_PROFILE_COMMON_OBJS := receiver_engine codec packet millisecond_clock
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

all: $(HEX) size

# --- Codegen ------------------------------------------------------------------
generate-wiring: demo_wiring.h

demo_wiring.h: demo.yaml $(wildcard $(WS)/codegen/wiring_*.py) $(WS)/codegen/requirements.txt
	$(CODEGEN_PYTHON) $(WS)/codegen/wiring_codegen.py $< --local-host Arduino -o $@

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

$(BOOT_PROFILE_BUILD_DIR)/millisecond_clock.o: $(AVR_PLATFORM)/millisecond_clock.cpp | $(BOOT_PROFILE_BUILD_DIR)
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

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPENDENCIES)

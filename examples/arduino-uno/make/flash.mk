# Programmer backends and public flash targets. Defaults are in ../Makefile.
.PHONY: flash flash-bits flash-bits-boot-profile flash-mcp2515-can

# --- Flash (PICkit 5 ISP by default; optional UART bootloader) -----------------
ifeq ($(FLASH_METHOD),uart)
define flash_hex
	$(AVRDUDE) -p $(MCU) -c arduino -P "$(PORT)" -b $(UPLOAD_BAUD) \
		-D -U "flash:w:$(abspath $(1)):i"
endef
else ifeq ($(FLASH_METHOD),pickit)
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
			-OAS0 \
			-OL \
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
else
$(error Unsupported FLASH_METHOD '$(FLASH_METHOD)'; use uart or pickit)
endif

flash: $(HEX)
	$(call flash_hex,$(HEX))

flash-bits: $(BITS_HEX)
	$(call flash_hex,$(BITS_HEX))

flash-bits-boot-profile: $(BOOT_PROFILE_RAM_HEX)
	$(call flash_hex,$(BOOT_PROFILE_RAM_HEX))

flash-mcp2515-can: $(CAN_HEX)
	$(call flash_hex,$(CAN_HEX))


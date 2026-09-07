# Host checks, serial tools and hardware tests. These do not flash firmware.
.PHONY: test-codegen test-hdlc test-receiver test-bits test-ping test-ingress \
	test-can-harness test-spi-can test-bits-boot-profile test-mcp2515-can test-stack \
	verify verify-bits led-on led-off led-ramp receive

# --- Generator tests ----------------------------------------------------------
test-codegen:
	$(CODEGEN_PYTHON) -m unittest discover -s $(WS)/codegen/tests -p 'test_*.py' -v

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


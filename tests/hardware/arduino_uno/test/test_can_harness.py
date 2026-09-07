"""CAN hardware harness regression: discard stale readiness before the requested reset edge."""

from pathlib import Path
import sys
import unittest
from unittest.mock import MagicMock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import mcp2515_can_test as harness


class CanHarnessTest(unittest.TestCase):
    def test_reset_drains_stale_banner_before_waiting_for_new_ready(self):
        events = []

        class Uart:
            def __enter__(self):
                return self

            def __exit__(self, *args):
                return False

            @property
            def dtr(self):
                return False

            @dtr.setter
            def dtr(self, value):
                events.append(("dtr", value))

            def reset_input_buffer(self):
                events.append(("drain",))

        arguments = MagicMock(cantact_interface="can0", arduino_port="port", baud=115200, timeout=5)
        with patch.object(harness.serial, "Serial", return_value=Uart()), \
             patch.object(harness.can.interface, "Bus"), \
             patch.object(harness.time, "sleep", side_effect=lambda value: events.append(("sleep", value))), \
             patch.object(harness, "wait_for_ready", side_effect=lambda *args: events.append(("ready",))), \
             patch.object(harness, "CANTACT_TO_ARDUINO", ()), patch.object(harness, "ARDUINO_TO_CANTACT", ()):
            harness.run_test(arguments)
        self.assertEqual(events, [("dtr", False), ("sleep", .05), ("drain",), ("dtr", True), ("ready",)])


if __name__ == "__main__":
    unittest.main()

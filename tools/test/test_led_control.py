"""LED command brightness parsing."""

import unittest

from wirespaces.led_control import parse_brightness


class LedBrightnessParserTest(unittest.TestCase):
    """Verify 8-bit brightness parsing and aliases."""

    def test_accepts_endpoints_and_aliases(self) -> None:
        self.assertEqual(parse_brightness("off"), 0)
        self.assertEqual(parse_brightness("0"), 0)
        self.assertEqual(parse_brightness("10%"), 26)
        self.assertEqual(parse_brightness("50%"), 128)
        self.assertEqual(parse_brightness("128"), 128)
        self.assertEqual(parse_brightness("0xff"), 255)
        self.assertEqual(parse_brightness("100%"), 255)
        self.assertEqual(parse_brightness("on"), 255)

    def test_rejects_out_of_range_brightness(self) -> None:
        with self.assertRaises(ValueError):
            parse_brightness("256")
        with self.assertRaises(ValueError):
            parse_brightness("101%")


if __name__ == "__main__":
    unittest.main()

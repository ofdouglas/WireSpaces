"""Protocol independence from optional serial packages and legacy import compatibility."""

from pathlib import Path
import subprocess
import sys
import unittest


class ProtocolImportsTest(unittest.TestCase):
    def test_protocol_suites_without_site_packages_or_cli_imports(self) -> None:
        """Run real codec/state-machine tests with third-party packages unavailable."""
        tools = Path(__file__).resolve().parents[1]
        result = subprocess.run(
            [sys.executable, "-I", "-S", "-c", """
import sys
import unittest
sys.path[:0] = sys.argv[1:]
suite = unittest.defaultTestLoader.loadTestsFromNames([
    'test_packet', 'test_hdlc', 'test_bits_ram_transfer',
])
result = unittest.TextTestRunner().run(suite)
assert not any(name == 'serial' or name.startswith('serial.') for name in sys.modules)
assert not any(name.startswith('wirespaces.bench') for name in sys.modules)
assert 'wirespaces.receiver' not in sys.modules
assert 'wirespaces.bits_ram_transfer' not in sys.modules
sys.exit(not result.wasSuccessful())
""", str(tools), str(tools / "test")],
            capture_output=True, text=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_original_import_paths_preserve_public_protocol_objects(self) -> None:
        """Existing callers see the same types and codecs through the legacy modules."""
        from wirespaces import bits, bits_ram_transfer, hdlc, packet, receiver

        for module, legacy, names in (
            (packet, receiver, ("WireSpacesPacket", "format_packet")),
            (hdlc, receiver, ("HdlcStreamDecoder", "encode_hdlc_frame", "crc16_ccitt_false")),
            (bits, bits_ram_transfer, bits.__all__),
        ):
            for name in names:
                with self.subTest(name=name):
                    self.assertIs(getattr(module, name), getattr(legacy, name))

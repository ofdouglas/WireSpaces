"""Interface shorthand normalization and CAN transmission settings through every compiler stage."""

import copy
from pathlib import Path
import subprocess
import tempfile
import unittest

import test_support  # Make compiler modules importable for standalone runs.

from test_wiring_topology import topology
from wiring_codegen import compile_deployment, emit_header
from wiring_inspection import explain_deployment
from wiring_schema import parse_deployment


class LinkConfigTest(unittest.TestCase):
    def setUp(self):
        self.data = topology({"Bus": ["A", "B"], "Tail": ["A", "C"]}, ["A", "B"])

    def test_shorthand_forms_are_equivalent_and_do_not_mutate_input(self):
        expected = compile_deployment(self.data)
        for interfaces in (["Bus", "Tail"], [{"Link": "Bus"}, {"Link": "Tail"}],
                           ["Bus", {"Name": "Tail", "Link": "Tail"}]):
            with self.subTest(interfaces=interfaces):
                data = copy.deepcopy(self.data)
                data["Hosts"][0]["Interfaces"] = interfaces
                original = copy.deepcopy(data)
                target = compile_deployment(data)
                self.assertEqual(target, expected)
                self.assertEqual(explain_deployment(target, "A"), explain_deployment(expected, "A"))
                self.assertEqual(emit_header(target, namespace="test", local_host_name="A"),
                                 emit_header(expected, namespace="test", local_host_name="A"))
                self.assertEqual(data, original)

    def test_shorthand_supports_bits_aliases_and_path_references(self):
        self.data["Hosts"][0]["Interfaces"] = [
            {"Link": "Bus", "EgressBit": 3}, {"Name": "Can0", "Link": "Tail"}]
        self.data["Hosts"][1]["Interfaces"] = ["Bus"]
        self.data["Hosts"][2]["Interfaces"] = ["Tail"]
        self.data["Paths"] = {"ViaA": ["B.Bus", "A.Bus", "A.Can0", "C.Tail"]}
        self.data["Wires"][0].update(Hosts=["B", "C"], Path="ViaA")
        target = compile_deployment(self.data)
        self.assertEqual([(i.declaration.name, i.egress_bit) for i in target.hosts["A"].interfaces],
                         [("Can0", 0), ("Bus", 3)])
        self.assertEqual(target.hosts["A"].routes[0].egress_mask, 9)
        self.data["Paths"]["ViaA"][2] = "A.Tail"
        with self.assertRaisesRegex(ValueError, "unknown interface"):
            compile_deployment(self.data)

    def test_invalid_shorthand(self):
        for interfaces in (
            "Bus", [None], [True], [1], [{}], [{"Name": "Bus"}], ["Missing"],
            ["invalid-name"], [{"Link": None}], [{"Link": "Bus", "Name": None}],
            ["Bus", {"Link": "Bus"}], ["Bus", {"Name": "Other", "Link": "Bus"}],
            ["Bus", {"Name": "Bus", "Link": "Tail"}],
            [{"Link": "Bus", "Symbol": "Other"}],
            [{"Link": "Bus", "EgressBit": True}],
        ):
            with self.subTest(interfaces=interfaces):
                data = copy.deepcopy(self.data)
                data["Hosts"][0]["Interfaces"] = interfaces
                with self.assertRaises(ValueError):
                    parse_deployment(data)

    def test_can_frame_format_and_bitrate_switching(self):
        for link_type, data_bitrate in (("CAN", None), ("CAN_FD", None),
                                        ("CAN_FD", 2000000), ("CAN_FD", 500000)):
            with self.subTest(link_type=link_type, data_bitrate=data_bitrate):
                data = copy.deepcopy(self.data)
                data["Links"][0]["LinkType"] = link_type
                if data_bitrate is not None:
                    data["Links"][0]["DataBitrate"] = data_bitrate
                target = compile_deployment(data)
                can = target.hosts["A"].interfaces[0].can
                self.assertEqual(can.arbitration_bitrate, 500000)
                self.assertEqual(can.fd_frames, link_type == "CAN_FD")
                self.assertEqual(can.data_bitrate, data_bitrate)
                self.assertEqual(can.bitrate_switch, data_bitrate is not None)
                details = explain_deployment(target, "A")["hosts"]["A"]["interfaces"]["Bus"]
                self.assertEqual(details["can"]["bitrate_switch"], data_bitrate is not None)
                self.assertEqual(details["link_declaration"]["ArbitrationBitrate"], 500000)
                self.assertEqual("DataBitrate" in details["link_declaration"], data_bitrate is not None)
                header = emit_header(target, namespace="test", local_host_name="A")
                assertions = [
                    "static_assert(test::kBusArbitrationBitrate == 500000UL);",
                    f"static_assert(test::kBusUseFdFrames == {str(link_type == 'CAN_FD').lower()});",
                ]
                if link_type == "CAN_FD":
                    assertions.append(f"static_assert(test::kBusBitRateSwitch == {str(data_bitrate is not None).lower()});")
                else:
                    self.assertNotIn("kBusBitRateSwitch", header)
                if data_bitrate is not None:
                    assertions.append(f"static_assert(test::kBusDataBitrate == {data_bitrate}UL);")
                else:
                    self.assertNotIn("kBusDataBitrate", header)
                with tempfile.TemporaryDirectory() as directory:
                    source = Path(directory) / "check.cpp"
                    source.write_text(header.replace("#pragma once\n", "") + "\n".join(assertions))
                    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-fsyntax-only",
                                    "-I", str(Path(__file__).resolve().parents[2] / "lib"), str(source)],
                                   check=True, capture_output=True)

    def test_can_requires_positive_integer_rates_and_rejects_unrelated_fields(self):
        cases = [
            {"LinkType": "CAN"}, {"LinkType": "CAN_FD"},
            {"LinkType": "CAN", "ArbitrationBitrate": 500000, "DataBitrate": 2000000},
            {"LinkType": "CAN", "ArbitrationBitrate": 500000, "BaudRate": 500000},
            {"LinkType": "UART_HDLC", "BaudRate": 115200, "ArbitrationBitrate": 500000},
            {"LinkType": "ETHERNET", "DataBitrate": 2000000},
            {"LinkType": "CAN_FD", "ArbitrationBitrate": 500000, "FdCapable": True},
            {"LinkType": "CAN_FD", "ArbitrationBitrate": 500000, "BitRateSwitch": True},
        ]
        for bad in (None, True, 0, -1, 1.5, "500000", 0x100000000):
            cases.append({"LinkType": "CAN", "ArbitrationBitrate": bad})
            cases.append({"LinkType": "CAN_FD", "ArbitrationBitrate": 500000, "DataBitrate": bad})
        for config in cases:
            with self.subTest(config=config):
                data = copy.deepcopy(self.data)
                data["Links"][0] = {"Name": "Bus", **config}
                with self.assertRaises(ValueError):
                    compile_deployment(data)


if __name__ == "__main__":
    unittest.main()

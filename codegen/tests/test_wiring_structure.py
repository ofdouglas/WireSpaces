"""Strict authoring/editor parity: unknown fields, types, nulls, choices and boundaries."""

import copy
import json
from pathlib import Path
import unittest

import test_support
from jsonschema import Draft202012Validator

from test_wiring_topology import topology
from wiring_schema import parse_deployment
from wiring_structure import deployment_json_schema, validate_structure

ROOT = Path(__file__).resolve().parents[1]


class StructureTest(unittest.TestCase):
    def setUp(self):
        self.data = topology({"Bus": ["A", "B"]}, ["A", "B"])
        self.schema = deployment_json_schema()
        self.editor = Draft202012Validator(self.schema)

    def rejects_in_both(self, data):
        with self.assertRaises(ValueError):
            validate_structure(data)
        self.assertFalse(self.editor.is_valid(data), data)

    def test_generated_schema_is_current(self):
        Draft202012Validator.check_schema(self.schema)
        self.assertEqual(json.loads((ROOT / "wiring.schema.json").read_text()), self.schema)

    def test_strict_ids_names_and_nulls(self):
        for key, values in {"HostId": [True, "1", 1.5, -1, 255, None],
                            "Name": [False, 1, "lower", "Bad__Name", "A.B", "A\n", None]}.items():
            for value in values:
                data = copy.deepcopy(self.data)
                data["Hosts"][0][key] = value
                with self.subTest(key=key, value=value):
                    self.rejects_in_both(data)
        for key in ("Groups", "Paths", "Realizations"):
            self.rejects_in_both({**self.data, key: None})

    def test_unknown_fields_at_every_mapping_boundary(self):
        for path in ((), ("Hosts", 0), ("Hosts", 0, "Interfaces", 0), ("Links", 0),
                     ("Wires", 0), ("Realizations", "Tree")):
            data = copy.deepcopy(self.data)
            data["Realizations"] = {"Tree": {"Attachments": ["A.Bus", "B.Bus"]}}
            node = data
            for key in path:
                node = node[key]
            node["Typo"] = 1
            with self.subTest(path=path):
                self.rejects_in_both(data)

    def test_selection_membership_and_references_shape(self):
        for update in ({"Links": ["Bus"], "Path": "Chain"}, {"Links": [], "Realization": "Tree"},
                       {"Path": "Chain", "Realization": "Tree"}, {"Hosts": []},
                       {"Hosts": ["A", "A"]}, {"Groups": ["Both", "Both"]}, {"Path": None}):
            data = copy.deepcopy(self.data)
            data["Wires"][0].update(update)
            self.rejects_in_both(data)
        for value in ([], ["A.Bus"], ["A.Bus", "A.Bus"], ["A.Bus", "NoDot"]):
            self.rejects_in_both({**self.data, "Realizations": {"Tree": {"Attachments": value}}})

    def test_link_variants_share_the_editor_contract(self):
        for link in (
            {"LinkType": "CAN"}, {"LinkType": "CAN", "ArbitrationBitrate": True},
            {"LinkType": "CAN", "ArbitrationBitrate": 500000, "DataBitrate": 2000000},
            {"LinkType": "CAN_FD", "ArbitrationBitrate": 500000, "DataBitrate": None},
            {"LinkType": "UART_HDLC"}, {"LinkType": "Ethernet", "ArbitrationBitrate": 1},
        ):
            self.rejects_in_both({**self.data, "Links": [{"Name": "Bus", **link}]})
        for link in (
            {"LinkType": "CAN", "ArbitrationBitrate": 500000},
            {"LinkType": "CAN_FD", "ArbitrationBitrate": 500000},
            {"LinkType": "CAN_FD", "ArbitrationBitrate": 500000, "DataBitrate": 2000000},
            {"LinkType": "UART_HDLC", "BaudRate": 115200}, {"LinkType": "Ethernet"},
        ):
            data = {**self.data, "Links": [{"Name": "Bus", **link}]}
            validate_structure(data)
            self.editor.validate(data)

    def test_editor_checks_structure_not_reference_resolution(self):
        data = copy.deepcopy(self.data)
        data["Wires"][0]["Hosts"] = ["Missing"]
        self.editor.validate(data)
        with self.assertRaisesRegex(ValueError, "Unknown reference"):
            parse_deployment(data)


if __name__ == "__main__":
    unittest.main()

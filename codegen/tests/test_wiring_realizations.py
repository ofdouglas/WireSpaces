"""Exact attachment trees: branches, excluded bus listeners, semantic errors and runtime propagation."""

import copy
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

import test_support
import yaml

from wiring_codegen import compile_deployment, emit_header
from wiring_inspection import explain_deployment
from wiring_structure import deployment_json_schema
from jsonschema import Draft202012Validator

ROOT = Path(__file__).resolve().parents[1]


class RealizationTest(unittest.TestCase):
    def setUp(self):
        self.data = yaml.safe_load((ROOT / "examples/branched.yaml").read_text())

    def test_exact_tree_and_provenance(self):
        model = compile_deployment(self.data)
        wire = model.resolved.wires["Test"]
        self.assertEqual(wire.transit_hosts, ("Gateway",))
        self.assertEqual(wire.selection, "realization")
        refs = sorted(self.data["Realizations"]["TestTree"]["Attachments"])
        self.assertEqual([a.reference for a in wire.attachments], refs)
        self.assertEqual(model.hosts["Gateway"].memberships, ())
        self.assertEqual(model.hosts["Observer"].routes, ())
        self.assertEqual(model.hosts["Gateway"].routes[0].egress_mask, 0b1101)
        details = explain_deployment(model, "Gateway")
        self.assertEqual(details["wires"]["Test"]["source"]["realization"],
                         {"TestTree": {"Attachments": refs}})
        self.assertEqual(details["hosts"]["Gateway"]["interfaces"]["Bypass"]["ingress_index"], 2)
        baseline = json.dumps(details, sort_keys=True)
        self.data["Realizations"]["TestTree"]["Attachments"].reverse()
        self.data["Hosts"].reverse()
        self.assertEqual(json.dumps(explain_deployment(compile_deployment(self.data), "Gateway"),
                                    sort_keys=True), baseline)

    def test_rejects_invalid_realizations_even_when_unused(self):
        for attachments, message in (
            (["Missing.Bus", "LeafA.Bus"], "unknown interface"),
            (["Root.Uplink", "Gateway.Uplink", "Gateway.Tail"], "dangling Link Tail"),
            (["Root.Uplink", "Gateway.Uplink", "LeafA.Bus", "LeafB.Bus"], "disconnected"),
            (["Root.Uplink", "Gateway.Uplink", "Root.Bypass", "Gateway.Bypass"], "cycle"),
        ):
            data = copy.deepcopy(self.data)
            data["Realizations"]["Unused"] = {"Attachments": attachments}
            with self.subTest(attachments=attachments), self.assertRaisesRegex(ValueError, message):
                compile_deployment(data)

    def test_unknown_selection_and_missing_members(self):
        self.data["Wires"][0]["Realization"] = "Missing"
        with self.assertRaisesRegex(ValueError, "Wire Test: unknown Realization"):
            compile_deployment(self.data)
        self.data["Wires"][0]["Realization"] = "TestTree"
        self.data["Wires"][0]["Hosts"] = ["Observer"]
        with self.assertRaisesRegex(ValueError, "Wire Test.*missing members.*Observer"):
            compile_deployment(self.data)

    def test_all_durable_examples_match_editor_schema(self):
        validator = Draft202012Validator(deployment_json_schema())
        paths = list((ROOT / "examples").glob("*.yaml")) + [ROOT / "demo.yaml",
            ROOT.parent / "examples/arduino-uno/demo.yaml", ROOT.parent / "hardware/bench/topology.yaml"]
        for path in paths:
            with self.subTest(path=path):
                modeline = path.read_text().splitlines()[0]
                self.assertTrue(modeline.startswith("# yaml-language-server: $schema="))
                self.assertEqual((path.parent / modeline.split("=", 1)[1]).resolve(), ROOT / "wiring.schema.json")
                data = yaml.safe_load(path.read_text())
                validator.validate(data)
                compile_deployment(data)

    def test_compiled_branched_network(self):
        model = compile_deployment(self.data)
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory)
            for name in model.hosts:
                (target / f"{name}.h").write_text(emit_header(model, namespace=name, local_host_name=name))
            core = ROOT.parent / "lib/wirespaces/core"
            command = ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic",
                       "-I", str(ROOT.parent / "lib"), "-I", str(target),
                       str(ROOT / "tests/support/branched_runtime.cpp"),
                       *[str(core / f"{name}.cpp") for name in ("packet", "header", "host", "dispatch", "router")],
                       "-o", str(target / "runtime")]
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            subprocess.run([str(target / "runtime")], check=True, timeout=10)


if __name__ == "__main__":
    unittest.main()

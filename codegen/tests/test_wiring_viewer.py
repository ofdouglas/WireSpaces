"""Read-only viewer projection, failure visibility, offline output and DOM-free interaction rules."""

import copy
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

import test_support
import yaml

from wiring_viewer import render_viewer, viewer_model

ROOT = Path(__file__).resolve().parents[1]


class ViewerTest(unittest.TestCase):
    def setUp(self):
        self.data = yaml.safe_load((ROOT / "examples/branched.yaml").read_text())

    def test_uses_resolved_data_without_mutation_and_is_deterministic(self):
        original = copy.deepcopy(self.data)
        model = viewer_model(self.data, "Branch")
        self.assertTrue(model["target_available"])
        self.assertEqual(model["diagnostics"], [])
        self.assertEqual(model["wires"][0]["route_masks"]["Gateway"], 13)
        self.assertNotIn("Observer.Bus", model["wires"][0]["attachments"])
        self.assertEqual(render_viewer(self.data), render_viewer(self.data))
        self.assertEqual(self.data, original)

    def test_target_limit_failure_is_visible_not_silently_truncated(self):
        data = yaml.safe_load((ROOT / "examples/studies/multicore_gateway.yaml").read_text())
        model = viewer_model(data, "Multicore")
        self.assertFalse(model["target_available"])
        self.assertEqual(model["diagnostics"][0]["stage"], "target")
        self.assertIn("Host Control exceeds six", model["diagnostics"][0]["message"])
        self.assertTrue(all(w["resolved"] and w["route_masks"] is None for w in model["wires"]))
        self.assertEqual(len(model["wires"]), 7)

    def test_ambiguity_keeps_physical_graph_but_does_not_invent_selection(self):
        del self.data["Wires"][0]["Realization"]
        model = viewer_model(self.data, "Ambiguous")
        self.assertEqual(model["diagnostics"][0]["stage"], "resolution")
        self.assertIn("ambiguous", model["diagnostics"][0]["message"])
        self.assertFalse(model["wires"][0]["resolved"])
        self.assertEqual(len(model["attachments"]), 10)
        self.assertEqual(model["wires"][0]["attachments"], [])

    def test_html_escaping_offline_policy_and_inert_data(self):
        title = '</script><script>alert(1)</script>@@MODEL@@'
        document = render_viewer(self.data, title)
        self.assertNotIn('<script>alert(1)</script>', document)
        encoded = re.search(r'<script id="topology-data" type="application/json">(.*?)</script>', document, re.S)[1]
        self.assertEqual(json.loads(encoded)["title"], title)
        self.assertNotIn("<", encoded)
        self.assertIn("connect-src 'none'", document)
        self.assertNotRegex(document, r'<(?:script|link)[^>]+(?:src|href)=')

    def test_cli_never_overwrites_input_and_rejects_invalid_structure(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "source.yaml"
            source.write_text(yaml.safe_dump(self.data))
            before = source.read_bytes()
            command = [sys.executable, str(ROOT / "wiring_viewer.py"), str(source)]
            result = subprocess.run(command + ["-o", str(source)], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("cannot overwrite", result.stderr)
            self.assertEqual(source.read_bytes(), before)
            alias = Path(directory) / "alias.html"
            alias.hardlink_to(source)
            result = subprocess.run(command + ["-o", str(alias)], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(source.read_bytes(), before)
            source.write_text("Hosts: broken\n")
            output = Path(directory) / "viewer.html"
            result = subprocess.run(command + ["-o", str(output)], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(output.exists())

    def test_javascript_preview_and_layout_contracts(self):
        node = os.environ.get("NODE") or shutil.which("node")
        if not node:
            self.skipTest("Node.js is needed for DOM-free viewer interaction tests (set NODE)")
        model = viewer_model(self.data, "Branch")
        with tempfile.TemporaryDirectory() as directory:
            model_file = Path(directory) / "model.json"
            model_file.write_text(json.dumps(model))
            result = subprocess.run([node, str(ROOT / "tests/support/viewer_test.cjs"), str(model_file)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()

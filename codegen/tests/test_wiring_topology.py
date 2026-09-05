"""Groups, bits, graph bridges, legacy trees, explicit chains, inspection and example generation."""

import copy
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

import test_support  # Make the compiler modules importable from this test directory.
import yaml

from wiring_codegen import generate_header, compile_deployment
from wiring_inspection import explain_deployment
from wiring_projection import project_deployment
from wiring_topology import resolve_deployment

ROOT = Path(__file__).resolve().parents[1]
PROJECT_SCHEMAS = (
    ROOT.parent / "hardware/bench/topology.yaml",
    ROOT / "examples/amr.yaml",
    ROOT / "examples/excavator.yaml",
)


def attachment_refs(wire):
    return [a.reference for a in wire.attachments]


def route_masks(model, wire):
    return {name: route.egress_mask for name, host in project_deployment(model).hosts.items()
            for route in host.routes if route.wire.name == wire.declaration.name}


def topology(links, members):
    """Build fixtures from Link -> host lists, using the Link name as interface name."""
    names = sorted({host for hosts in links.values() for host in hosts})
    return {
        "Hosts": [{"Name": host, "HostId": index, "Interfaces": [
            {"Name": link, "Link": link} for link, attached in links.items() if host in attached]}
            for index, host in enumerate(names)],
        "Links": [{"Name": link, "LinkType": "CAN", "ArbitrationBitrate": 500000} for link in links],
        "Wires": [{"Name": "Test", "WireId": 1, "Hosts": members}],
    }


class TopologyTest(unittest.TestCase):
    def setUp(self):
        self.data = topology({"Bus": ["A", "B", "Listener"], "Tail": ["B", "C"]}, ["A", "C"])

    def resolved(self, data=None):
        return resolve_deployment(self.data if data is None else data)

    def test_tree_and_shared_bus_selection(self):
        model = self.resolved()
        wire = model.wires["Test"]
        self.assertEqual(list(wire.members), ["A", "C"])
        self.assertEqual(list(wire.transit_hosts), ["B"])
        self.assertEqual(attachment_refs(wire), ["A.Bus", "B.Bus", "B.Tail", "C.Tail"])
        self.assertEqual(route_masks(model, wire), {"A": 1, "B": 3, "C": 1})
        header = generate_header(self.data, namespace="test", header_name="", local_host_name="Listener")
        self.assertIn("return {};", header)
        self.assertIn("kListenerHostInfo{kListenerHost, 0U, {}}", header)
        transit = generate_header(self.data, namespace="test", header_name="", local_host_name="B")
        self.assertIn("kBHostInfo{kBHost, 0U, {}}", transit)

    def test_groups_union_and_references(self):
        self.data["Groups"] = {"Ends": ["A", "C"], "Overlap": ["C"]}
        self.data["Wires"][0].update(Hosts=["A"], Groups=["Ends", "Overlap"])
        self.assertEqual(list(self.resolved().wires["Test"].members), ["A", "C"])
        del self.data["Wires"][0]["Hosts"]
        self.assertEqual(list(self.resolved().wires["Test"].members), ["A", "C"])
        for groups in ({"Ends": ["Missing"]}, {"Ends": ["Overlap"], "Overlap": ["A"]},
                       {"bad-name": ["A"]}, []):
            data = copy.deepcopy(self.data)
            data["Groups"] = groups
            with self.subTest(groups=groups), self.assertRaises(ValueError):
                self.resolved(data)
        self.data["Wires"][0]["Groups"] = ["Missing"]
        with self.assertRaisesRegex(ValueError, "Wire Test Groups"):
            self.resolved()

    def test_local_only_even_on_cycle(self):
        data = topology({"Bus": ["A", "B"], "Parallel": ["A", "B"]}, ["A"])
        wire = self.resolved(data).wires["Test"]
        self.assertEqual(attachment_refs(wire), [])
        self.assertEqual(route_masks(self.resolved(data), wire), {"A": 0})
        data["Wires"][0].pop("Hosts")
        with self.assertRaisesRegex(ValueError, "member"):
            self.resolved(data)

    def test_irrelevant_cycle(self):
        data = topology({"Main": ["A", "B", "C"], "Branch": ["B", "D"], "Parallel": ["B", "D"]},
                        ["A", "C"])
        self.assertEqual(attachment_refs(self.resolved(data).wires["Test"]), ["A.Main", "C.Main"])

    def test_disconnection_parallel_and_ring(self):
        for links, pattern in [
            ({"One": ["A", "B"], "Two": ["C", "D"]}, "disconnected"),
            ({"One": ["A", "C"], "Two": ["A", "C"]}, "ambiguous.*Path, Realization or Links"),
            ({"One": ["A", "B"], "Two": ["B", "C"], "Three": ["C", "A"]}, "ambiguous"),
            ({"Short": ["A", "C"], "Ab": ["A", "B"], "Bd": ["B", "D"], "Dc": ["D", "C"]},
             "ambiguous"),  # A shorter route is not a reason to select it automatically.
        ]:
            with self.subTest(links=links), self.assertRaisesRegex(ValueError, "Wire Test.*" + pattern):
                self.resolved(topology(links, ["A", "C"]))

    def test_legacy_includes_every_attachment(self):
        self.data["Wires"][0]["Links"] = ["Bus", "Tail"]
        model = self.resolved()
        self.assertIn("Listener.Bus", attachment_refs(model.wires["Test"]))
        self.assertEqual(list(model.wires["Test"].transit_hosts), ["B", "Listener"])
        self.assertEqual(route_masks(model, model.wires["Test"])["Listener"], 1)

    def test_explicit_bits_reserved_before_automatic(self):
        data = topology({"Zulu": ["A"], "Alpha": ["A"], "Middle": ["A"]}, ["A"])
        data["Hosts"][0]["Interfaces"][0]["EgressBit"] = 0
        expected = {"Zulu": 0, "Alpha": 1, "Middle": 2}
        for _ in range(2):
            interfaces = project_deployment(self.resolved(data)).hosts["A"].interfaces
            self.assertEqual({i.declaration.name: i.egress_bit for i in interfaces}, expected)
            data["Hosts"][0]["Interfaces"].reverse()
        with self.assertRaisesRegex(ValueError, "at most 8"):
            self.resolved(topology({f"Link{i}": ["A"] for i in range(9)}, ["A"]))

    def path_data(self):
        data = copy.deepcopy(self.data)
        data["Paths"] = {"ViaB": ["A.Bus", "B.Bus", "B.Tail", "C.Tail"]}
        data["Wires"][0]["Path"] = "ViaB"
        return data

    def test_explicit_transit_and_bus_selection(self):
        data = self.path_data()
        self.assertEqual(attachment_refs(self.resolved(data).wires["Test"]),
                         attachment_refs(self.resolved().wires["Test"]))
        data["Wires"][0]["Hosts"].append("Listener")
        with self.assertRaisesRegex(ValueError, "Wire Test.*missing members"):
            self.resolved(data)

    def test_invalid_chains(self):
        cases = [
            ([], "at least 2"), (["A.Bus"], "at least 2"),
            (["A.Bus", "Unknown.Bus"], "unknown interface"),
            (["A.Bus", "C.Tail"], "invalid hop"),
            (["A.Bus", "A.Bus"], "invalid hop"),
            (["A.Bus", "B.Bus", "C.Tail", "B.Tail"], "same host"),
            (["A.Bus", "B.Bus", "B.Bus", "Listener.Bus"], "distinct interfaces"),
        ]
        for chain, message in cases:
            data = self.path_data()
            data["Paths"]["ViaB"] = chain
            with self.subTest(chain=chain), self.assertRaisesRegex(ValueError, message):
                self.resolved(data)
        data = topology({"Ab": ["A", "B"], "Bc": ["B", "C"], "Ca": ["C", "A"]}, ["A", "C"])
        data["Paths"] = {"Cycle": ["A.Ab", "B.Ab", "B.Bc", "C.Bc", "C.Ca", "A.Ca"]}
        with self.assertRaisesRegex(ValueError, "cycle"):
            self.resolved(data)  # Unused declarations must also be valid.
        data = topology({"Bus": ["A", "B", "C", "D"], "Bc": ["B", "C"]}, ["A", "D"])
        data["Paths"] = {"Cycle": ["A.Bus", "B.Bus", "B.Bc", "C.Bc", "C.Bus", "D.Bus"]}
        with self.assertRaisesRegex(ValueError, "cycle"):
            self.resolved(data)

    def test_unknown_path_and_conflicting_selection(self):
        data = self.path_data()
        data["Wires"][0]["Path"] = "Missing"
        with self.assertRaisesRegex(ValueError, "unknown Path"):
            self.resolved(data)
        data = self.path_data()
        data["Wires"][0]["Links"] = ["Bus"]
        with self.assertRaisesRegex(ValueError, "mutually exclusive"):
            self.resolved(data)

    def test_explain_traceability_and_determinism(self):
        data = self.path_data()
        data["Groups"] = {"Ends": ["C", "A"]}
        data["Wires"][0].update(Groups=["Ends"], Hosts=["A"])
        expected = explain_deployment(compile_deployment(data), "B")
        wire = expected["wires"]["Test"]
        self.assertEqual(wire["source"]["path"]["ViaB"], data["Paths"]["ViaB"])
        self.assertEqual(wire["source"]["groups"]["Ends"], ["A", "C"])
        self.assertEqual(wire["route_masks"], {"A": 1, "B": 3, "C": 1})
        self.assertEqual(expected["hosts"]["B"]["interfaces"]["Bus"]["source"], "Hosts.B.Interfaces.Bus")
        data["Hosts"].reverse()
        data["Links"].reverse()
        data["Groups"]["Ends"].reverse()
        for host in data["Hosts"]:
            host["Interfaces"].reverse()
        self.assertEqual(json.dumps(expected, sort_keys=True),
                         json.dumps(explain_deployment(compile_deployment(data), "B"), sort_keys=True))
        command = [sys.executable, str(ROOT / "wiring_codegen.py"), str(ROOT / "demo.yaml"),
                   "--local-host", "Arduino", "--explain"]
        first = subprocess.run(command, check=True, capture_output=True, text=True).stdout
        self.assertEqual(first, subprocess.run(command, check=True, capture_output=True, text=True).stdout)
        self.assertEqual(json.loads(first)["wires"]["TestWire"]["route_masks"], {"Arduino": 1, "Pc": 1})
        self.assertNotEqual(subprocess.run(command + ["--output", "/tmp/unused_wiring.h"],
                                          capture_output=True).returncode, 0)

    def test_examples_generate_for_every_host(self):
        for path in PROJECT_SCHEMAS:
            data = yaml.safe_load(path.read_text())
            for host in data["Hosts"]:
                with self.subTest(example=path.name, host=host["Name"]):
                    header = generate_header(data, namespace="example", header_name="", local_host_name=host["Name"])
                    with tempfile.TemporaryDirectory() as directory:
                        source = Path(directory) / "example.cpp"
                        source.write_text(header.replace("#pragma once\n", ""))
                        subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-fsyntax-only",
                                        "-I", str(ROOT.parent / "lib"), str(source)], check=True, capture_output=True)


if __name__ == "__main__":
    unittest.main()

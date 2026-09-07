"""Typed stage boundaries, immutable snapshots, legacy output parity and graph-policy regression."""

import copy
from dataclasses import FrozenInstanceError
import hashlib
import itertools
import json
import os
import re
from pathlib import Path
import subprocess
import sys
import unittest

import networkx as nx
import test_support  # Make the compiler modules importable from this test directory.
import yaml

import test_wiring_codegen as legacy_tests
from test_wiring_topology import topology
from wiring_codegen import compile_deployment, emit_header
from wiring_inspection import explain_deployment
from wiring_models import AuthoredDeployment, ResolvedDeployment, TargetProjection
from wiring_projection import project_deployment
from wiring_schema import parse_deployment
from wiring_topology import resolve_deployment

ROOT = Path(__file__).resolve().parents[1]


class ModelTest(unittest.TestCase):
    def test_stages_do_not_mutate_or_mix_representations(self):
        data = topology({"Bus": ["A", "B"], "Tail": ["A", "C"]}, ["A", "B"])
        original = copy.deepcopy(data)
        authored = parse_deployment(data)
        self.assertIsInstance(authored, AuthoredDeployment)
        self.assertIsNone(authored.hosts["A"].interfaces[0].egress_bit)
        resolved = resolve_deployment(authored)
        self.assertIsInstance(resolved, ResolvedDeployment)
        self.assertIs(resolved.authored, authored)
        self.assertFalse(hasattr(resolved.wires["Test"], "egress_mask"))
        target = project_deployment(resolved)
        self.assertIsInstance(target, TargetProjection)
        self.assertIs(target.resolved, resolved)
        self.assertEqual(target.hosts["A"].routes[0].egress_mask, 1)
        self.assertEqual([i.egress_bit for i in target.hosts["A"].interfaces], [0, 1])
        self.assertEqual(data, original)
        data["Hosts"][0]["Interfaces"][0]["Link"] = "Changed"
        self.assertEqual(authored.hosts["A"].interfaces[0].link, "Bus")
        with self.assertRaises(TypeError):
            authored.hosts["A"] = authored.hosts["B"]
        with self.assertRaises(FrozenInstanceError):
            authored.hosts["A"].interfaces[0].egress_bit = 7
        with self.assertRaises(TypeError):
            resolved.wires["Test"] = None
        with self.assertRaises(FrozenInstanceError):
            target.hosts["A"].routes[0].egress_mask = 0

    def test_authored_omission_and_explicit_empty_are_preserved(self):
        data = topology({"Bus": ["A", "B"]}, ["A", "B"])
        data["Groups"] = {"Both": ["A", "B"]}
        data["Wires"][0].update(Groups=["Both"])
        del data["Wires"][0]["Hosts"]
        authored = parse_deployment(data)
        self.assertIsNone(authored.wires["Test"].hosts)
        source = explain_deployment(compile_deployment(authored), "A")["wires"]["Test"]["source"]["wire"]
        self.assertNotIn("Hosts", source)
        data["Wires"][0]["Hosts"] = []
        authored = parse_deployment(data)
        self.assertEqual(authored.wires["Test"].hosts, ())
        source = explain_deployment(compile_deployment(authored), "A")["wires"]["Test"]["source"]["wire"]
        self.assertEqual(source["Hosts"], [])

    def test_bits_are_only_assigned_in_projection(self):
        data = topology({"Zulu": ["A"], "Alpha": ["A"], "Middle": ["A"]}, ["A"])
        data["Hosts"][0]["Interfaces"][0]["EgressBit"] = 0
        resolved = resolve_deployment(parse_deployment(data))
        target = project_deployment(resolved)
        self.assertEqual([(i.declaration.name, i.egress_bit) for i in target.hosts["A"].interfaces],
                         [("Zulu", 0), ("Alpha", 1), ("Middle", 2)])
        self.assertEqual([i.egress_bit for i in resolved.authored.hosts["A"].interfaces], [0, None, None])
        self.assertEqual([i.assignment for i in target.hosts["A"].interfaces],
                         ["explicit", "automatic", "automatic"])

    def test_outputs_match_pre_refactor_snapshots(self):
        fixtures = {
            "demo": ROOT / "demo.yaml",
            "bench": ROOT.parent / "hardware/bench/topology.yaml",
            "amr": ROOT / "examples/amr.yaml",
            "excavator": ROOT / "examples/excavator.yaml",
        }
        cases = {name: yaml.safe_load(path.read_text()) for name, path in fixtures.items()}
        legacy = legacy_tests.WiringTest()
        legacy.setUp()
        cases["Legacy"] = legacy.two_links()
        expected = json.loads((ROOT / "tests/testdata/output_sha256.json").read_text())
        observed = {}
        for name, data in cases.items():
            target = compile_deployment(data)
            for host in target.hosts:
                header = emit_header(target, namespace="compat", local_host_name=host)
                # Preserve historical hashes across the interface-mask type rename.
                header = header.replace("wirespaces::InterfaceSet", "wirespaces::EgressSet")
                # Normalize only the intentional Link-admission API change for these
                # historical hashes; compiled forwarder tests exercise the new contract.
                header = header.replace("wirespaces::PacketLink&", "wirespaces::PacketForwarder&")
                header = header.replace("wirespaces::RouteResult forward(", "void forward(")
                header = header.replace("        wirespaces::RouteResult result{wirespaces::RouteResult::kNoEgress};\n", "")
                header = header.replace("        return result == wirespaces::RouteResult::kNoEgress ? wirespaces::RouteResult::kAccepted : result;\n", "")
                header = re.sub(r"result = wirespaces::combineAdmission\(result, (link\d+_)\.trySend\(packet\)\);",
                                lambda m: f"{m[1]}.forward(packet, " + next(
                                    f"k{i.declaration.name}Egress" for i in target.hosts[host].interfaces
                                    if f"link{i.egress_bit}_" == m[1]) + ");", header)
                # CAN timing/format output is new; retain the old snapshots as a
                # regression check of every unchanged topology and C++ declaration.
                header = re.sub(
                    r"^constexpr (?:std::uint32_t|bool) k\w+(?:ArbitrationBitrate|DataBitrate|UseFdFrames|BitRateSwitch)\{.*\};\n",
                    "", header, flags=re.MULTILINE)
                header = re.sub(r"^constexpr std::uint8_t k\w+IngressIndex\{.*\};\n", "", header, flags=re.MULTILINE)
                header = header.replace(
                    "// Routes are local-origin masks; Router::receive validates and excludes the selected ingress.",
                    "// Routes describe locally originated traffic; ingress-aware gateway routing is not provided.")
                details = explain_deployment(target, host)
                details["route_semantics"] = "local-origin; ingress participation is not enforced"
                for detail in details["hosts"].values():
                    for interface in detail["interfaces"].values():
                        interface.pop("can", None)
                        interface.pop("ingress_index", None)
                        interface["link_declaration"].pop("ArbitrationBitrate", None)
                        interface["link_declaration"].pop("DataBitrate", None)
                inspection = json.dumps(details, sort_keys=True, indent=2) + "\n"
                observed[f"{name}/{host}"] = {
                    "header": hashlib.sha256(header.encode()).hexdigest(),
                    "explain": hashlib.sha256(inspection.encode()).hexdigest(),
                }
        self.assertEqual(observed, expected)

    def test_ambiguity_diagnostic_is_order_independent(self):
        data = topology({"First": ["A", "B"], "Second": ["A", "C"], "Third": ["C", "B"]}, ["A", "B"])
        with self.assertRaises(ValueError) as expected:
            resolve_deployment(data)
        data["Hosts"].reverse()
        data["Links"].reverse()
        data["Wires"][0]["Hosts"].reverse()
        for host in data["Hosts"]:
            host["Interfaces"].reverse()
        with self.assertRaises(ValueError) as actual:
            resolve_deployment(data)
        self.assertEqual(str(actual.exception), str(expected.exception))

    def test_inspection_is_independent_of_process_hash_seed(self):
        command = [sys.executable, str(ROOT / "wiring_codegen.py"),
                   str(ROOT.parent / "hardware/bench/topology.yaml"), "--local-host", "Gateway", "--explain"]
        results = [subprocess.run(command, env={**os.environ, "PYTHONHASHSEED": seed},
                                  check=True, capture_output=True).stdout for seed in ("1", "2", "99")]
        self.assertEqual(results[0], results[1])
        self.assertEqual(results[1], results[2])

    def test_unique_path_policy_exhaustive_small_bipartite_graphs(self):
        # Independent oracle: enumerate paths, rather than using bridges or BFS.
        hosts, links = ("A", "B", "C"), ("One", "Two", "Three")
        edges = list(itertools.product(hosts, links))
        for mask in range(1 << len(edges)):
            graph = nx.Graph()
            graph.add_nodes_from(("host", h) for h in hosts)
            selected = [(h, link) for index, (h, link) in enumerate(edges) if mask & (1 << index)]
            graph.add_edges_from((("host", h), ("link", link)) for h, link in selected)
            data = {
                "Hosts": [{"Name": h, "HostId": index, "Interfaces": [
                    {"Name": link, "Link": link} for host, link in selected if host == h]}
                    for index, h in enumerate(hosts)],
                "Links": [{"Name": link, "LinkType": "CAN", "ArbitrationBitrate": 500000} for link in links],
                "Wires": [{"Name": "Test", "WireId": 1, "Hosts": list(hosts)}],
            }
            paths = [list(nx.all_simple_paths(graph, ("host", "A"), ("host", h))) for h in ("B", "C")]
            with self.subTest(mask=mask):
                if any(len(options) != 1 for options in paths):
                    with self.assertRaises(ValueError):
                        resolve_deployment(data)
                    continue
                expected = set()
                for options in paths:
                    for a, b in zip(options[0], options[0][1:]):
                        host, link = (a, b) if a[0] == "host" else (b, a)
                        expected.add(f"{host[1]}.{link[1]}")
                resolved = resolve_deployment(data)
                self.assertEqual({a.reference for a in resolved.wires["Test"].attachments}, expected)


if __name__ == "__main__":
    unittest.main()

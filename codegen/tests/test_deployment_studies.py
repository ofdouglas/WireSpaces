"""Faithful multicore constraints and a 40-host line, including compiled per-Wire propagation."""

import copy
from pathlib import Path
import subprocess
import tempfile
import unittest

import test_support
import yaml

from wiring_codegen import compile_deployment, emit_header
from wiring_topology import resolve_deployment

ROOT = Path(__file__).resolve().parents[1]


def load_example(name):
    return yaml.safe_load((ROOT / "examples" / name).read_text())


class DeploymentStudyTest(unittest.TestCase):
    def test_multicore_full_relationships_exceed_target_without_changing_limits(self):
        data = load_example("studies/multicore_gateway.yaml")
        resolved = resolve_deployment(data)
        self.assertEqual(len(resolved.authored.hosts), 12)
        self.assertEqual(sum("Control" in w.members for w in resolved.wires.values()), 7)
        with self.assertRaisesRegex(ValueError, "Host Control exceeds six"):
            compile_deployment(data)
        maintenance = {a.reference for a in resolved.wires["Maintenance"].attachments}
        self.assertNotIn("FieldIo.ShmControlIo", maintenance)
        self.assertNotIn("Plant.Upstream", maintenance)
        self.assertEqual({a.link for a in resolved.wires["GatewaySupervision"].attachments},
                         {"ShmHostControl", "ShmControlIo"})
        self.assertEqual(resolved.wires["DriveControl"].transit_hosts, ("FieldIo",))
        self.assertIn("Host", resolved.wires["DriveControl"].members)

    def test_multicore_triangle_requires_per_wire_selection(self):
        data = load_example("studies/multicore_gateway.yaml")
        del data["Wires"][0]["Realization"]
        with self.assertRaisesRegex(ValueError, "Wire DriveControl: ambiguous"):
            resolve_deployment(data)
        data = load_example("studies/multicore_gateway.yaml")
        data["Realizations"]["DriveTree"]["Attachments"] += ["Control.ShmHostControl", "Host.ShmHostControl"]
        with self.assertRaisesRegex(ValueError, "Realization DriveTree: propagation cycle"):
            resolve_deployment(data)

    def test_large_line_limits_and_shared_bus_exclusions(self):
        model = compile_deployment(load_example("packaging_line.yaml"))
        self.assertEqual((len(model.hosts), len(model.resolved.authored.links), len(model.resolved.wires)), (40, 17, 19))
        self.assertEqual(max(len(h.interfaces) for h in model.hosts.values()), 5)
        self.assertEqual(max(len(h.memberships) for h in model.hosts.values()), 6)
        control = model.hosts["InfeedControl"]
        self.assertEqual(len(control.routes), 7)  # Transit routes are not local memberships.
        self.assertNotIn("LineTelemetry", [w.name for w in control.memberships])
        wire = model.resolved.wires["LineTelemetry"]
        self.assertNotIn("Plant.Backbone", [a.reference for a in wire.attachments])
        self.assertEqual(len(wire.transit_hosts), 4)
        self.assertEqual(len(model.resolved.wires["Maintenance"].members), 38)

    def test_compiled_large_line_and_individual_multicore_wires(self):
        cases = [("Line", load_example("packaging_line.yaml"))]
        multicore = load_example("studies/multicore_gateway.yaml")
        # Each slice tests actual routing semantics, not deployment of the over-limit aggregate.
        for wire in multicore["Wires"]:
            case = copy.deepcopy(multicore)
            case["Wires"] = [copy.deepcopy(wire)]
            cases.append((wire["Name"], case))
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory)
            includes = ['#include "deployment_runtime.hpp"']
            functions = []
            for case, data in cases:
                model = compile_deployment(data)
                lines = [f"void check{case}() {{", "    study::Network network;"]
                for name, host in model.hosts.items():
                    namespace = f"{case}_{name}"
                    (target / f"{namespace}.h").write_text(emit_header(model, namespace=namespace, local_host_name=name))
                    includes.append(f'#include "{namespace}.h"')
                    drivers = []
                    for interface in host.interfaces:
                        driver = f"link_{name}_{interface.declaration.name}"
                        drivers.append(driver)
                        lines.append(f'    study::Link {driver}{{network, {host.declaration.host_id}U, "{interface.declaration.link}"}};')
                    lines += [f"    {namespace}::Forwarder forwarder_{name}{{{', '.join(drivers)}}};",
                              f"    study::Node host_{name}{{{namespace}::k{name}HostInfo, {namespace}::routes(), forwarder_{name}}};",
                              f"    network.nodes.push_back(&host_{name});"]
                    for interface in host.interfaces:
                        lines.append(f'    network.attach(host_{name}, "{interface.declaration.link}", {interface.ingress_index}U);')
                for wire in model.resolved.wires.values():
                    member_ids = ", ".join(str(model.hosts[n].declaration.host_id) + "U" for n in wire.members)
                    links = ", ".join('"' + name + '"' for name in sorted({a.link for a in wire.attachments}))
                    for name in wire.members:
                        lines.append(f"    network.check(host_{name}, {wire.declaration.wire_id}U, {{{member_ids}}}, {{{links}}});")
                lines.append("}")
                functions.append("\n".join(lines))
            source = "\n".join(includes + functions + ["int main() {", *[f"    check{case}();" for case, _ in cases], "}"])
            (target / "study.cpp").write_text(source)
            core = ROOT.parent / "lib/wirespaces/core"
            result = subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic",
                "-I", str(ROOT.parent / "lib"), "-I", str(ROOT / "tests/support"), "-I", str(target),
                str(target / "study.cpp"), *[str(core / f"{name}.cpp") for name in ("packet", "header", "host", "dispatch", "router")],
                "-o", str(target / "study")], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            subprocess.run([str(target / "study")], check=True, timeout=30)


if __name__ == "__main__":
    unittest.main()

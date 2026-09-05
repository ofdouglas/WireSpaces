"""Topology validation and compiled generated fanout behavior."""

import copy
import subprocess
import tempfile
import unittest
from pathlib import Path

import yaml

from wiring_codegen import SchemaLoader, generate_header

ROOT = Path(__file__).resolve().parents[1]


class WiringTest(unittest.TestCase):
    def setUp(self):
        self.data = yaml.safe_load((ROOT / "codegen/demo.yaml").read_text())

    def generate(self, data=None, host="Arduino"):
        return generate_header(self.data if data is None else data, namespace="demo_wiring",
                               header_name="generated.h", local_host_name=host)

    def test_demo_is_reproducible(self):
        self.assertEqual(self.generate(), (ROOT / "examples/arduino-uno/demo_wiring.h").read_text())
        self.assertIn("kPcHostInfo", self.generate(host="Pc"))

    def test_invalid_schema(self):
        mutations = [
            lambda d: d["Hosts"][0].update(Name="Arduino UNO"),
            lambda d: d["Hosts"][0].update(Symbol="Arduino"),
            lambda d: d["Hosts"][1].update(HostId=1),
            lambda d: d["Hosts"][1].update(HostId=255),
            lambda d: d["Hosts"][0]["Interfaces"][0].update(EgressBit=8),
            lambda d: d["Hosts"][0]["Interfaces"][0].update(EgressBit=True),
            lambda d: d["Hosts"][0]["Interfaces"][0].update(Link="Missing"),
            lambda d: d["Wires"][0].update(Links=[]),
            lambda d: d["Wires"][0].update(Hosts=["Missing"]),
            lambda d: d["Wires"].append(copy.deepcopy(d["Wires"][0])),
        ]
        for mutation in mutations:
            with self.subTest(mutation=mutation):
                data = copy.deepcopy(self.data)
                mutation(data)
                with self.assertRaises(ValueError):
                    self.generate(data)

    def two_links(self):
        data = copy.deepcopy(self.data)
        data["Hosts"][0]["Interfaces"][0]["EgressBit"] = 0
        data["Wires"][0]["Links"] = ["VcpUart"]
        data["Links"].append(dict(Name="CanBus", LinkType="CAN"))
        data["Hosts"][0]["Interfaces"].append(dict(Name="Can", Link="CanBus", EgressBit=3))
        data["Hosts"].append(dict(Name="Sensor", HostId=3, Interfaces=[
            dict(Name="Can", Link="CanBus", EgressBit=0)]))
        data["Wires"][0]["Hosts"].append("Sensor")
        data["Wires"][0]["Links"].append("CanBus")
        return data

    def test_duplicate_bits_and_cycles(self):
        data = self.two_links()
        data["Hosts"][0]["Interfaces"][1]["EgressBit"] = 0
        with self.assertRaisesRegex(ValueError, "EgressBit"):
            self.generate(data)
        data = self.two_links()
        data["Hosts"][1]["Interfaces"].append(dict(Name="Can", Link="CanBus", EgressBit=1))
        with self.assertRaisesRegex(ValueError, "loop"):
            self.generate(data)

    def test_multiple_wires_per_link(self):
        self.data["Wires"].append(dict(Name="DebugWire", WireId=2,
                                      Hosts=["Arduino", "Pc"], Links=["VcpUart"]))
        header = self.generate()
        self.assertIn("{kTestWire, kUartEgress}", header)
        self.assertIn("{kDebugWire, kUartEgress}", header)

    def test_empty_host(self):
        data = dict(Hosts=[dict(Name="Solo", HostId=0, Interfaces=[])], Links=[], Wires=[])
        self.assertIn("return {};", self.generate(data, host="Solo"))

    def test_duplicate_yaml_keys(self):
        with self.assertRaisesRegex(ValueError, "Duplicate schema key"):
            yaml.load("Hosts: []\nHosts: []\n", Loader=SchemaLoader)

    def test_symbol_collision_and_membership_limit(self):
        self.data["Wires"][0]["Name"] = "ArduinoHost"
        with self.assertRaisesRegex(ValueError, "collision"):
            self.generate()
        self.setUp()
        self.data["Wires"] = [
            dict(Name=f"Wire{index}", WireId=index, Hosts=["Arduino", "Pc"], Links=["VcpUart"])
            for index in range(1, 8)]
        with self.assertRaisesRegex(ValueError, "six"):
            self.generate()

    def test_order_independence(self):
        data = self.two_links()
        expected = self.generate(data)
        data["Hosts"].reverse()
        data["Links"].reverse()
        for host in data["Hosts"]:
            host["Interfaces"].reverse()
        self.assertEqual(expected, self.generate(data))

    def test_compiled_fanout(self):
        # Nonconsecutive bits catch accidental use of an index/ID as a bitmask.
        source = r"""
#include "generated.h"
#include <cassert>
struct Recorder : wirespaces::PacketForwarder {
    int calls{};
    wirespaces::EgressSet mask{};
    void forward(const wirespaces::PacketBuffer&, wirespaces::EgressSet selected) noexcept override {
        ++calls;
        mask = selected;
    }
};
WS_PACKET_BUFFER_DEFINE(Packet, 4);
int main() {
    Recorder uart, can;
    demo_wiring::Forwarder forwarder{uart, can};
    Packet packet;
    assert(demo_wiring::routes().size() == 1);
    auto mask = demo_wiring::routes()[0].egress_set;
    assert(mask == 9);
    forwarder.forward(packet, 0);
    forwarder.forward(packet, 128);
    assert(uart.calls == 0 && can.calls == 0);
    forwarder.forward(packet, 8);
    assert(uart.calls == 0 && can.calls == 1 && can.mask == 8);
    forwarder.forward(packet, mask);
    assert(uart.calls == 1 && can.calls == 2 && uart.mask == 1);
}
"""
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "generated.h").write_text(self.generate(self.two_links()))
            (path / "test.cpp").write_text(source)
            subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                            "-I", str(ROOT / "lib"), str(path / "test.cpp"),
                            str(ROOT / "lib/wirespaces/core/packet.cpp"),
                            str(ROOT / "lib/wirespaces/core/header.cpp"),
                            "-o", str(path / "test")], check=True, capture_output=True)
            subprocess.run([str(path / "test")], check=True)
            # The physical Can attachment remains bound, but is absent from inferred routes.
            data = self.two_links()
            data["Wires"][0].pop("Links")
            data["Wires"][0]["Hosts"].remove("Sensor")
            (path / "generated.h").write_text(self.generate(data))
            (path / "test.cpp").write_text(source.replace("assert(mask == 9)", "assert(mask == 1)")
                                           .replace("can.calls == 2", "can.calls == 1"))
            subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                            "-I", str(ROOT / "lib"), str(path / "test.cpp"),
                            str(ROOT / "lib/wirespaces/core/packet.cpp"),
                            str(ROOT / "lib/wirespaces/core/header.cpp"),
                            "-o", str(path / "test")], check=True, capture_output=True)
            subprocess.run([str(path / "test")], check=True)


if __name__ == "__main__":
    unittest.main()

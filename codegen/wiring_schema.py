"""Parsing and structural validation for deployment authoring."""

import copy
import re

import yaml

class SchemaLoader(yaml.SafeLoader):
    """Reject duplicate mapping keys instead of silently overriding configuration."""


def unique_mapping(loader, node):
    result = {}
    for key_node, value_node in node.value:
        key = loader.construct_object(key_node)
        require(isinstance(key, str), "Schema keys must be strings")
        require(key not in result, f"Duplicate schema key: {key}")
        result[key] = loader.construct_object(value_node)
    return result


SchemaLoader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, unique_mapping)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def identifier(name):
    # Uppercase initial avoids C++ keywords; disallow implementation-reserved names.
    require(isinstance(name, str) and re.fullmatch(r"[A-Z][A-Za-z0-9_]*", name)
            and "__" not in name, f"Invalid Name {name!r}: use an uppercase-leading C++ identifier")
    return name


def fields(item, required, optional=()):
    require(isinstance(item, dict), "Expected a mapping")
    require(set(required) <= item.keys(), f"Missing fields: {set(required) - item.keys()}")
    require(item.keys() <= set(required) | set(optional),
            f"Unknown fields: {item.keys() - set(required) - set(optional)}")


def integer(value, low, high, label):
    require(type(value) is int and low <= value <= high, f"{label} must be an integer in {low}..{high}")


def named(items, label):
    require(isinstance(items, list), f"{label} must be a list")
    result = {}
    for item in items:
        require(isinstance(item, dict) and "Name" in item, f"{label} entries require Name")
        name = identifier(item["Name"])
        require(name not in result, f"Duplicate {label} name: {name}")
        result[name] = item
    return result


def references(items, known, label):
    require(isinstance(items, list) and all(isinstance(x, str) for x in items), f"{label} must list names")
    require(len(items) == len(set(items)), f"Duplicate reference in {label}")
    require(set(items) <= known.keys(), f"Unknown reference in {label}: {set(items) - known.keys()}")


def parse_deployment(data):
    fields(data, ("Hosts", "Links", "Wires"), ("Groups", "Paths"))
    data = copy.deepcopy(data)
    hosts = named(data["Hosts"], "Hosts")
    links = named(data["Links"], "Links")
    wires = named(data["Wires"], "Wires")
    require(hosts, "At least one host is required")
    groups, paths = data.get("Groups", {}), data.get("Paths", {})
    require(isinstance(groups, dict), "Groups must be a mapping")
    require(isinstance(paths, dict), "Paths must be a mapping")
    for name, members in groups.items():
        identifier(name)
        references(members, hosts, f"Group {name} Hosts (nested groups are not supported)")
    for name, chain in paths.items():
        identifier(name)
        require(isinstance(chain, list) and all(isinstance(ref, str) for ref in chain),
                f"Path {name} must list interface references")
    used_ids = set()
    for host in hosts.values():
        fields(host, ("Name", "HostId", "Interfaces"))
        integer(host["HostId"], 0, 254, "HostId")
        require(host["HostId"] not in used_ids, "Duplicate HostId")
        used_ids.add(host["HostId"])
        interfaces = named(host["Interfaces"], "Interfaces")
        require(len(interfaces) <= 8, f"Host {host['Name']} exceeds eight interfaces")
        bits, attached = set(), set()
        for interface in interfaces.values():
            fields(interface, ("Name", "Link"), ("EgressBit",))
            if "EgressBit" in interface:
                integer(interface["EgressBit"], 0, 7, "EgressBit")
                require(interface["EgressBit"] not in bits, "Duplicate host-local EgressBit")
                bits.add(interface["EgressBit"])
            require(isinstance(interface["Link"], str) and interface["Link"] in links, "Unknown interface Link")
            require(interface["Link"] not in attached, "A host may attach to a Link only once")
            attached.add(interface["Link"])
        for name in sorted(interfaces):
            interface = interfaces[name]
            interface["BitAssignment"] = "explicit" if "EgressBit" in interface else "automatic"
            if "EgressBit" not in interface:
                bit = next(bit for bit in range(8) if bit not in bits)
                interface["EgressBit"] = bit
                bits.add(bit)
    for link in links.values():
        fields(link, ("Name", "LinkType"), ("BaudRate",))
        identifier(link["LinkType"])
        if "BaudRate" in link:
            integer(link["BaudRate"], 1, 0xFFFFFFFF, "BaudRate")
        if link["LinkType"] == "UART_HDLC":
            require("BaudRate" in link, "UART_HDLC requires BaudRate")
    used_ids.clear()
    for wire in wires.values():
        fields(wire, ("Name", "WireId"), ("Hosts", "Groups", "Links", "Path"))
        label = f"Wire {wire['Name']}"
        integer(wire["WireId"], 1, 254, "WireId (named network Wire)")
        require(wire["WireId"] not in used_ids, "Duplicate WireId")
        used_ids.add(wire["WireId"])
        references(wire.get("Hosts", []), hosts, f"{label} Hosts")
        references(wire.get("Groups", []), groups, f"{label} Groups")
        require(not ("Links" in wire and "Path" in wire), f"{label}: Links and Path are mutually exclusive")
        if "Links" in wire:
            references(wire["Links"], links, f"{label} Links")
        if "Path" in wire:
            require(isinstance(wire["Path"], str) and wire["Path"] in paths, f"{label}: unknown Path")
        members = set(wire.get("Hosts", []))
        for group in wire.get("Groups", []):
            members.update(groups[group])
        require(members, f"{label} requires at least one member host")
        wire["Members"] = sorted(members)
    for host in hosts:
        require(sum(host in w["Members"] for w in wires.values()) <= 6,
                f"Host {host} exceeds six membership Wires")
    return hosts, links, wires, groups, paths


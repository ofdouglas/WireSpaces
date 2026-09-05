"""Parsing and structural validation for deployment authoring."""

import re

import yaml

from wiring_models import (
    AuthoredDeployment, GroupDeclaration, HostDeclaration, InterfaceDeclaration,
    LinkDeclaration, PathDeclaration, WireDeclaration, frozen_mapping,
)

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


def interface_declarations(items):
    """Normalize string and Link-only shorthand without modifying the input."""
    require(isinstance(items, list), "Interfaces must be a list")
    declarations = []
    for item in items:
        if isinstance(item, str):
            declarations.append({"Name": item, "Link": item})
        else:
            fields(item, ("Link",), ("Name", "EgressBit"))
            declarations.append({**item, "Name": item.get("Name", item["Link"])})
    return named(declarations, "Interfaces")


def parse_deployment(data) -> AuthoredDeployment:
    """Validate the existing YAML syntax and take a typed snapshot, without derived fields."""
    fields(data, ("Hosts", "Links", "Wires"), ("Groups", "Paths"))
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
    host_interfaces = {}
    for host in hosts.values():
        fields(host, ("Name", "HostId", "Interfaces"))
        integer(host["HostId"], 0, 254, "HostId")
        require(host["HostId"] not in used_ids, "Duplicate HostId")
        used_ids.add(host["HostId"])
        interfaces = interface_declarations(host["Interfaces"])
        host_interfaces[host["Name"]] = interfaces
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
    for link in links.values():
        fields(link, ("Name", "LinkType"), ("BaudRate", "ArbitrationBitrate", "DataBitrate"))
        identifier(link["LinkType"])
        label = f"Link {link['Name']}"
        is_can = link["LinkType"] in ("CAN", "CAN_FD")
        if is_can:
            require("ArbitrationBitrate" in link, f"{label}: {link['LinkType']} requires ArbitrationBitrate")
            require("BaudRate" not in link, f"{label}: use ArbitrationBitrate, not BaudRate, for CAN")
        if "ArbitrationBitrate" in link:
            require(is_can, f"{label}: ArbitrationBitrate is only valid for CAN or CAN_FD")
            integer(link["ArbitrationBitrate"], 1, 0xFFFFFFFF, f"{label} ArbitrationBitrate")
        if "DataBitrate" in link:
            require(link["LinkType"] == "CAN_FD", f"{label}: DataBitrate is only valid for CAN_FD")
            integer(link["DataBitrate"], 1, 0xFFFFFFFF, f"{label} DataBitrate")
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
    return AuthoredDeployment(
        hosts=frozen_mapping({
            name: HostDeclaration(name, host["HostId"], tuple(
                InterfaceDeclaration(i["Name"], i["Link"], i.get("EgressBit"))
                for i in host_interfaces[name].values()))
            for name, host in hosts.items()}),
        links=frozen_mapping({
            name: LinkDeclaration(name, link["LinkType"], link.get("BaudRate"),
                                  link.get("ArbitrationBitrate"), link.get("DataBitrate"))
            for name, link in links.items()}),
        wires=frozen_mapping({
            name: WireDeclaration(
                name, wire["WireId"],
                tuple(wire["Hosts"]) if "Hosts" in wire else None,
                tuple(wire["Groups"]) if "Groups" in wire else None,
                tuple(wire["Links"]) if "Links" in wire else None,
                wire.get("Path"))
            for name, wire in wires.items()}),
        groups=frozen_mapping({name: GroupDeclaration(name, tuple(members)) for name, members in groups.items()}),
        paths=frozen_mapping({name: PathDeclaration(name, tuple(chain)) for name, chain in paths.items()}),
    )

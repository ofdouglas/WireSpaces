"""Parsing and structural validation for deployment authoring."""

import yaml

from wiring_models import (
    AuthoredDeployment, GroupDeclaration, HostDeclaration, InterfaceDeclaration,
    LinkDeclaration, PathDeclaration, RealizationDeclaration, WireDeclaration, frozen_mapping,
)
from wiring_structure import validate_structure

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


def named(items, label):
    """Index structurally validated declarations, rejecting duplicate identities."""
    result = {}
    for item in items:
        name = item["Name"]
        require(name not in result, f"Duplicate {label} name: {name}")
        result[name] = item
    return result


def references(items, known, label):
    require(set(items) <= known.keys(), f"Unknown reference in {label}: {set(items) - known.keys()}")


def interface_declarations(items):
    """Normalize string and Link-only shorthand without modifying the input."""
    declarations = []
    for item in items:
        if isinstance(item, str):
            declarations.append({"Name": item, "Link": item})
        else:
            declarations.append({**item, "Name": item.get("Name", item["Link"])})
    return named(declarations, "Interfaces")


def parse_deployment(data) -> AuthoredDeployment:
    """Validate the existing YAML syntax and take a typed snapshot, without derived fields."""
    data = validate_structure(data).model_dump(exclude_unset=True)
    hosts = named(data["Hosts"], "Hosts")
    links = named(data["Links"], "Links")
    wires = named(data["Wires"], "Wires")
    groups, paths = data.get("Groups", {}), data.get("Paths", {})
    realizations = data.get("Realizations", {})
    for name, members in groups.items():
        references(members, hosts, f"Group {name} Hosts (nested groups are not supported)")
    used_ids = set()
    host_interfaces = {}
    for host in hosts.values():
        require(host["HostId"] not in used_ids, "Duplicate HostId")
        used_ids.add(host["HostId"])
        interfaces = interface_declarations(host["Interfaces"])
        host_interfaces[host["Name"]] = interfaces
        bits, attached = set(), set()
        for interface in interfaces.values():
            if "EgressBit" in interface:
                require(interface["EgressBit"] not in bits, "Duplicate host-local EgressBit")
                bits.add(interface["EgressBit"])
            require(interface["Link"] in links, "Unknown interface Link")
            require(interface["Link"] not in attached, "A host may attach to a Link only once")
            attached.add(interface["Link"])
    used_ids.clear()
    for wire in wires.values():
        label = f"Wire {wire['Name']}"
        require(wire["WireId"] not in used_ids, "Duplicate WireId")
        used_ids.add(wire["WireId"])
        references(wire.get("Hosts", []), hosts, f"{label} Hosts")
        references(wire.get("Groups", []), groups, f"{label} Groups")
        if "Links" in wire:
            references(wire["Links"], links, f"{label} Links")
        if "Path" in wire:
            require(wire["Path"] in paths, f"{label}: unknown Path")
        if "Realization" in wire:
            require(wire["Realization"] in realizations, f"{label}: unknown Realization")
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
                wire.get("Path"), wire.get("Realization"))
            for name, wire in wires.items()}),
        groups=frozen_mapping({name: GroupDeclaration(name, tuple(members)) for name, members in groups.items()}),
        paths=frozen_mapping({name: PathDeclaration(name, tuple(chain)) for name, chain in paths.items()}),
        realizations=frozen_mapping({name: RealizationDeclaration(name, tuple(value["Attachments"]))
                                     for name, value in realizations.items()}),
    )

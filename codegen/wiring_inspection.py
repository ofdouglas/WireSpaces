"""Serialize the typed compiler result to the existing deterministic inspection format."""

from wiring_models import TargetProjection
from wiring_schema import require


def explain_deployment(projection: TargetProjection, local_host: str) -> dict:
    """Keep authored provenance and target details in JSON without mixing the internal models."""
    require(local_host in projection.hosts, f"Unknown local host: {local_host}")
    authored = projection.resolved.authored
    hosts = {}
    for name, host in projection.hosts.items():
        interfaces = {}
        for interface in host.interfaces:
            declaration = interface.declaration
            link = authored.links[declaration.link]
            link_declaration = {"Name": link.name, "LinkType": link.link_type}
            if link.baud_rate is not None:
                link_declaration["BaudRate"] = link.baud_rate
            if link.arbitration_bitrate is not None:
                link_declaration["ArbitrationBitrate"] = link.arbitration_bitrate
            if link.data_bitrate is not None:
                link_declaration["DataBitrate"] = link.data_bitrate
            interfaces[declaration.name] = {
                "link": declaration.link, "egress_bit": interface.egress_bit,
                "assignment": interface.assignment,
                "source": f"Hosts.{name}.Interfaces.{declaration.name}",
                "link_declaration": link_declaration,
            }
            if interface.can is not None:
                interfaces[declaration.name]["can"] = {
                    "fd_frames": interface.can.fd_frames,
                    "arbitration_bitrate": interface.can.arbitration_bitrate,
                    "data_bitrate": interface.can.data_bitrate,
                    "bitrate_switch": interface.can.bitrate_switch,
                }
        hosts[name] = {"host_id": host.declaration.host_id, "source": f"Hosts.{name}",
                       "interfaces": interfaces, "memberships": sorted(w.name for w in host.memberships)}
    wires = {}
    for name, wire in projection.resolved.wires.items():
        declaration = wire.declaration
        wire_source = {"Name": declaration.name, "WireId": declaration.wire_id}
        for key, value in (("Hosts", declaration.hosts), ("Groups", declaration.groups), ("Links", declaration.links)):
            if value is not None:
                wire_source[key] = sorted(value)
        source = {"wire": wire_source,
                  "groups": {g: sorted(authored.groups[g].hosts) for g in declaration.groups or ()}}
        if declaration.path is not None:
            wire_source["Path"] = declaration.path
            source["path"] = {declaration.path: list(authored.paths[declaration.path].interfaces)}
        masks = {h: route.egress_mask for h, host in projection.hosts.items()
                 for route in host.routes if route.wire.name == name}
        wires[name] = {
            "wire_id": declaration.wire_id, "members": list(wire.members),
            "transit_hosts": list(wire.transit_hosts), "selection": wire.selection,
            "attachments": [a.reference for a in wire.attachments], "route_masks": masks,
            "source": source,
        }
    return {"local_host": local_host, "route_semantics": "local-origin; ingress participation is not enforced",
            "hosts": hosts, "wires": wires}

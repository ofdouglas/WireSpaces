"""Lower resolved attachments to the current runtime's fixed masks and HostInfo tables."""

from wiring_models import (
    CanProjection, HostProjection, InterfaceProjection, ResolvedDeployment, RouteProjection,
    TargetProjection, frozen_mapping,
)
from wiring_schema import require


def project_deployment(resolved: ResolvedDeployment) -> TargetProjection:
    """Reserve explicit bits, then assign automatic bits by name; retain existing limits."""
    hosts = {}
    wires = sorted(resolved.wires.values(), key=lambda w: w.declaration.wire_id)
    for name, host in resolved.authored.hosts.items():
        bits = {i.egress_bit for i in host.interfaces if i.egress_bit is not None}
        interfaces = []
        for interface in sorted(host.interfaces, key=lambda i: i.name):
            bit = interface.egress_bit
            if bit is None:
                bit = next(bit for bit in range(8) if bit not in bits)
                bits.add(bit)
            link = resolved.authored.links[interface.link]
            can = None
            if link.link_type in ("CAN", "CAN_FD"):
                can = CanProjection(link.arbitration_bitrate, link.data_bitrate, link.link_type == "CAN_FD")
            interfaces.append(InterfaceProjection(interface, bit, link.baud_rate, can))
        interfaces.sort(key=lambda i: i.egress_bit)
        memberships = tuple(w.declaration for w in wires if name in w.members)
        require(len(memberships) <= 6, f"Host {name} exceeds six membership Wires")
        routes = []
        for wire in wires:
            selected = {a.interface for a in wire.attachments if a.host == name}
            mask = sum(i.mask for i in interfaces if i.declaration.name in selected)
            if mask or name in wire.members:
                routes.append(RouteProjection(wire.declaration, mask))
        hosts[name] = HostProjection(host, tuple(interfaces), memberships, tuple(routes))
    return TargetProjection(resolved, frozen_mapping(hosts))

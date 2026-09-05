"""Resolve WireSpaces topology policy over a NetworkX host/Link graph."""

from __future__ import annotations

from collections.abc import Mapping

import networkx as nx

from wiring_models import (
    Attachment, AuthoredDeployment, HostDeclaration, PathDeclaration,
    ResolvedDeployment, ResolvedWire, WireDeclaration, frozen_mapping,
)
from wiring_schema import parse_deployment, require

Node = tuple[str, str]


class PhysicalGraph:
    """Shared Links are nodes, and interfaces are edges, not pairwise bus connections."""

    def __init__(self, hosts: Mapping[str, HostDeclaration]):
        self.graph = nx.Graph()
        self.interfaces: dict[str, Attachment] = {}
        self.graph.add_nodes_from(("host", name) for name in sorted(hosts))
        for name, host in sorted(hosts.items()):
            for interface in sorted(host.interfaces, key=lambda i: i.name):
                attachment = Attachment(name, interface.name, interface.link)
                self.interfaces[attachment.reference] = attachment
                self.graph.add_edge(("host", name), ("link", interface.link), attachment=attachment)
        self.bridges = {self.attachment(a, b) for a, b in nx.bridges(self.graph)}

    def attachment(self, a: Node, b: Node) -> Attachment:
        return self.graph.edges[a, b]["attachment"]

    def infer(self, wire: WireDeclaration, members: tuple[str, ...]) -> set[Attachment]:
        if len(members) == 1:
            return set()
        root = ("host", members[0])
        # BFS only supplies a witness path. Every edge must be a bridge: a shorter
        # path is never preferred over an alternative, even a much longer one.
        parents = {root: None, **dict(nx.bfs_predecessors(self.graph, root, sort_neighbors=sorted))}
        selected: set[Attachment] = set()
        for member in members[1:]:
            node = ("host", member)
            require(node in parents, f"Wire {wire.name}: disconnected members {members[0]} and {member}")
            while parents[node] is not None:
                parent = parents[node]
                attachment = self.attachment(node, parent)
                if attachment not in self.bridges:
                    alternative = self.alternative_path(node, parent, attachment)
                    raise ValueError(
                        f"Wire {wire.name}: ambiguous connectivity from {members[0]} to {member}; "
                        f"{attachment.reference} on Link {attachment.link} conflicts with "
                        f"attachments {' -> '.join(alternative)}. Choose an explicit Path or Links selection.")
                selected.add(attachment)
                node = parent
        return selected

    def alternative_path(self, start: Node, end: Node, excluded: Attachment) -> list[str]:
        """Get a deterministic ambiguity witness without changing the physical graph."""
        graph = nx.subgraph_view(
            self.graph, filter_edge=lambda a, b: self.attachment(a, b) != excluded)
        parents = dict(nx.bfs_predecessors(graph, start, sort_neighbors=sorted))
        path, node = [], end
        while node != start:
            parent = parents[node]
            path.append(self.attachment(node, parent).reference)
            node = parent
        return list(reversed(path))

    def chain(self, path: PathDeclaration) -> tuple[set[Attachment], set[str]]:
        # These are WireSpaces chain grammar rules, not generic graph traversal.
        chain, label = path.interfaces, f"Path {path.name}"
        require(len(chain) >= 2 and len(chain) % 2 == 0, f"{label}: requires complete hop pairs (at least one hop)")
        for ref in chain:
            require(ref in self.interfaces, f"{label}: unknown interface {ref}")
        visited_hosts, visited_links = set(), set()
        previous_host = previous_interface = None
        for index in range(0, len(chain), 2):
            left, right = chain[index:index + 2]
            a, b = self.interfaces[left], self.interfaces[right]
            require(a.link == b.link and a.host != b.host, f"{label}: invalid hop pair {left}, {right}")
            if previous_host is None:
                visited_hosts.add(a.host)
            else:
                require(a.host == previous_host, f"{label}: consecutive hops must join through the same host")
                require(left != previous_interface, f"{label}: transit host must use distinct interfaces")
            require(b.host not in visited_hosts and a.link not in visited_links,
                    f"{label}: repeated host or Link creates a cycle")
            visited_hosts.add(b.host)
            visited_links.add(a.link)
            previous_host, previous_interface = b.host, right
        return {self.interfaces[ref] for ref in chain}, visited_hosts

    def legacy(self, wire: WireDeclaration, members: tuple[str, ...]) -> set[Attachment]:
        selected_links = set(wire.links or ())
        selected = {a for a in self.interfaces.values() if a.link in selected_links}
        graph = nx.Graph()
        graph.add_nodes_from(("host", name) for name in members)
        for link in sorted(selected_links):
            node = ("link", link)
            require(node in self.graph and self.graph.degree(node) >= 2,
                    f"Wire {wire.name}: Link {link} needs at least two attached hosts")
        graph.add_edges_from((("host", a.host), ("link", a.link)) for a in selected)
        # Preserve legacy diagnostic precedence: examine the root component for
        # cycles before reporting other disconnected components.
        component = graph.subgraph(nx.node_connected_component(graph, min(graph)))
        require(nx.is_tree(component), f"Wire {wire.name} contains a propagation loop")
        require(nx.is_connected(graph), f"Wire {wire.name} is disconnected")
        return selected


def resolve_deployment(data: AuthoredDeployment | dict) -> ResolvedDeployment:
    """Expand membership and resolve attachments; never assign target egress bits."""
    authored = data if isinstance(data, AuthoredDeployment) else parse_deployment(data)
    graph = PhysicalGraph(authored.hosts)
    chains = {name: graph.chain(path) for name, path in authored.paths.items()}
    wires = {}
    for wire in authored.wires.values():
        expanded = set(wire.hosts or ())
        for group in wire.groups or ():
            expanded.update(authored.groups[group].hosts)
        require(expanded, f"Wire {wire.name} requires at least one member host")
        members = tuple(sorted(expanded))
        if wire.path is not None:
            selected, visited = chains[wire.path]
            require(expanded <= visited,
                    f"Wire {wire.name}: Path {wire.path} is missing members {sorted(expanded - visited)}")
            mode = "path"
        elif wire.links is not None:
            selected, mode = graph.legacy(wire, members), "links"
        else:
            selected, mode = graph.infer(wire, members), "inferred"
        wires[wire.name] = ResolvedWire(
            wire, members, tuple(sorted({a.host for a in selected} - expanded)),
            tuple(sorted(selected, key=lambda a: a.reference)), mode)
    return ResolvedDeployment(authored, frozen_mapping(wires))

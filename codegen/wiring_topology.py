"""Resolve authored topology into selected interface attachments, without runtime policy."""

from collections import deque
from dataclasses import dataclass

from wiring_schema import parse_deployment, require


@dataclass
class ResolvedDeployment:
    hosts: dict
    links: dict
    wires: dict
    groups: dict
    paths: dict

    def route_masks(self, wire):
        masks = {host: 0 for host in wire["Members"]}
        for host, declaration in self.hosts.items():
            for interface in declaration["Interfaces"]:
                if f"{host}.{interface['Name']}" in wire["Attachments"]:
                    masks[host] = masks.get(host, 0) | (1 << interface["EgressBit"])
        return dict(sorted(masks.items()))

    def explain(self, local_host):
        """Stable inspection with named source declarations, independent of YAML list order."""
        hosts = {}
        for name, host in sorted(self.hosts.items()):
            interfaces = {}
            for interface in sorted(host["Interfaces"], key=lambda i: i["EgressBit"]):
                interfaces[interface["Name"]] = {
                    "link": interface["Link"], "egress_bit": interface["EgressBit"],
                    "assignment": interface["BitAssignment"],
                    "source": f"Hosts.{name}.Interfaces.{interface['Name']}",
                    "link_declaration": self.links[interface["Link"]],
                }
            hosts[name] = {"host_id": host["HostId"], "source": f"Hosts.{name}", "interfaces": interfaces,
                           "memberships": sorted(w["Name"] for w in self.wires.values() if name in w["Members"])}
        wires = {}
        for name, wire in sorted(self.wires.items()):
            source = {"wire": {k: wire[k] for k in ("Name", "WireId", "Hosts", "Groups", "Links", "Path")
                               if k in wire},
                      "groups": {g: self.groups[g] for g in wire.get("Groups", [])}}
            if "Path" in wire:
                source["path"] = {wire["Path"]: self.paths[wire["Path"]]}
            # Membership and legacy Link declarations are sets, not ordered paths.
            for key in ("Hosts", "Groups", "Links"):
                if key in source["wire"]:
                    source["wire"][key] = sorted(source["wire"][key])
            source["groups"] = {g: sorted(members) for g, members in source["groups"].items()}
            wires[name] = {
                "wire_id": wire["WireId"], "members": wire["Members"],
                "transit_hosts": wire["TransitHosts"], "selection": wire["Selection"],
                "attachments": wire["Attachments"], "route_masks": self.route_masks(wire),
                "source": source,
            }
        return {"local_host": local_host, "route_semantics": "local-origin; ingress participation is not enforced",
                "hosts": hosts, "wires": wires}


class PhysicalGraph:
    """Bipartite host/Link graph; each edge is one physical interface attachment."""

    def __init__(self, hosts):
        self.adj = {("host", name): {} for name in hosts}
        self.interfaces = {}
        for name, host in sorted(hosts.items()):
            for interface in sorted(host["Interfaces"], key=lambda i: i["Name"]):
                ref = f"{name}.{interface['Name']}"
                a, b = ("host", name), ("link", interface["Link"])
                self.interfaces[ref] = (a, b)
                self.adj[a][b] = ref
                self.adj.setdefault(b, {})[a] = ref
        self.bridges = self.find_bridges()

    def find_bridges(self):
        # Iterative Tarjan traversal also handles long chains without Python recursion limits.
        entered, low, bridges = {}, {}, set()
        for root in sorted(self.adj):
            if root in entered:
                continue
            entered[root] = low[root] = len(entered)
            stack = [(root, None, iter(sorted(self.adj[root])))]
            while stack:
                node, parent, peers = stack[-1]
                peer = next(peers, None)
                if peer is None:
                    stack.pop()
                    if parent is not None:
                        low[parent] = min(low[parent], low[node])
                        if low[node] > entered[parent]:
                            bridges.add(self.adj[node][parent])
                elif peer != parent:
                    if peer in entered:
                        low[node] = min(low[node], entered[peer])
                    else:
                        entered[peer] = low[peer] = len(entered)
                        stack.append((peer, node, iter(sorted(self.adj[peer]))))
        return bridges

    def infer(self, wire):
        members = wire["Members"]
        if len(members) == 1:
            return set()
        root = ("host", members[0])
        parents = {root: None}
        queue = deque([root])
        while queue:
            node = queue.popleft()
            for peer in sorted(self.adj[node]):
                if peer not in parents:
                    parents[peer] = node
                    queue.append(peer)
        selected = set()
        for member in members[1:]:
            node = ("host", member)
            require(node in parents, f"Wire {wire['Name']}: disconnected members {members[0]} and {member}")
            while parents[node] is not None:
                parent = parents[node]
                ref = self.adj[node][parent]
                if ref not in self.bridges:
                    alternative = self.alternative_path(node, parent, ref)
                    raise ValueError(
                        f"Wire {wire['Name']}: ambiguous connectivity from {members[0]} to {member}; "
                        f"{ref} on Link {self.interfaces[ref][1][1]} conflicts with "
                        f"attachments {' -> '.join(alternative)}. Choose an explicit Path or Links selection.")
                selected.add(ref)
                node = parent
        return selected

    def alternative_path(self, start, end, excluded):
        """Return a concrete ambiguity witness, excluding the disputed attachment."""
        parents = {start: None}
        queue = deque([start])
        while queue and end not in parents:
            node = queue.popleft()
            for peer, ref in sorted(self.adj[node].items()):
                if ref != excluded and peer not in parents:
                    parents[peer] = node
                    queue.append(peer)
        path, node = [], end
        while parents[node] is not None:
            parent = parents[node]
            path.append(self.adj[node][parent])
            node = parent
        return list(reversed(path))

    def chain(self, name, chain):
        label = f"Path {name}"
        require(len(chain) >= 2 and len(chain) % 2 == 0, f"{label}: requires complete hop pairs (at least one hop)")
        for ref in chain:
            require(ref in self.interfaces, f"{label}: unknown interface {ref}")
        visited_hosts, visited_links = set(), set()
        previous_host = previous_interface = None
        for index in range(0, len(chain), 2):
            left, right = chain[index:index + 2]
            (a, link), (b, other_link) = self.interfaces[left], self.interfaces[right]
            require(link == other_link and a != b, f"{label}: invalid hop pair {left}, {right}")
            if previous_host is None:
                visited_hosts.add(a[1])
            else:
                require(a == previous_host, f"{label}: consecutive hops must join through the same host")
                require(left != previous_interface, f"{label}: transit host must use distinct interfaces")
            require(b[1] not in visited_hosts and link[1] not in visited_links,
                    f"{label}: repeated host or Link creates a cycle")
            visited_hosts.add(b[1])
            visited_links.add(link[1])
            previous_host, previous_interface = b, right
        return set(chain), visited_hosts

    def legacy(self, wire):
        selected_links = set(wire["Links"])
        selected = {ref for ref, (_, link) in self.interfaces.items() if link[1] in selected_links}
        graph = {("host", name): set() for name in wire["Members"]}
        for link in selected_links:
            node = ("link", link)
            require(len(self.adj.get(node, {})) >= 2,
                    f"Wire {wire['Name']}: Link {link} needs at least two attached hosts")
        for ref in selected:
            a, b = self.interfaces[ref]
            graph.setdefault(a, set()).add(b)
            graph.setdefault(b, set()).add(a)
        seen, stack = set(), [(min(graph), None)]
        while stack:
            node, parent = stack.pop()
            require(node not in seen, f"Wire {wire['Name']} contains a propagation loop")
            seen.add(node)
            stack.extend((peer, node) for peer in sorted(graph[node]) if peer != parent)
        require(len(seen) == len(graph), f"Wire {wire['Name']} is disconnected")
        return selected


def resolve_deployment(data):
    """Validate once, then resolve each Wire without granting membership to transit hosts."""
    hosts, links, wires, groups, paths = parse_deployment(data)
    graph = PhysicalGraph(hosts)
    chains = {name: graph.chain(name, chain) for name, chain in sorted(paths.items())}
    for wire in wires.values():
        if "Path" in wire:
            selected, visited = chains[wire["Path"]]
            require(set(wire["Members"]) <= visited,
                    f"Wire {wire['Name']}: Path {wire['Path']} is missing members "
                    f"{sorted(set(wire['Members']) - visited)}")
            mode = "path"
        elif "Links" in wire:
            selected, mode = graph.legacy(wire), "links"
        else:
            selected, mode = graph.infer(wire), "inferred"
        wire["Selection"] = mode
        wire["Attachments"] = sorted(selected)
        wire["TransitHosts"] = sorted({graph.interfaces[ref][0][1] for ref in selected} - set(wire["Members"]))
    return ResolvedDeployment(hosts, links, wires, groups, paths)

/* Offline presentation over resolved compiler output. No schema editing or packet I/O. */
"use strict";

function forwardingPreview(wire, host, ingress) {
    if (!wire) return {status: "Choose a Wire to preview forwarding.", outgoing: []};
    if (!wire.resolved) return {status: "Resolution failed; no forwarding result is available.", outgoing: []};
    const local = wire.attachments.filter(ref => ref.startsWith(host + "."));
    if (!local.length && !wire.members.includes(host)) {
        return {status: "No route at this host. No forwarding or local delivery.", outgoing: []};
    }
    const incoming = ingress ? host + "." + ingress : null;
    if (incoming && !local.includes(incoming)) {
        return {status: "Rejected: this ingress attachment is not selected for the Wire.", outgoing: []};
    }
    const outgoing = local.filter(ref => ref !== incoming);
    return {status: outgoing.length ? "Egress: " + outgoing.join(", ") :
            (incoming ? (wire.members.includes(host) ? "Leaf: no egress. Local membership permits destination/endpoint checks." :
                         "Leaf: no egress. Transit-only host: no local delivery.") :
                        "Local-only: no physical egress. Local delivery is application-owned."), outgoing};
}

function hostRole(wire, name) {
    if (!wire) return "physical";
    if (!wire.resolved) return "unresolved";
    if (wire.members.includes(name)) return "member";
    return wire.transit_hosts.includes(name) ? "transit" : "excluded";
}

function graphLayout(model, wire, selectedOnly) {
    const filter = Boolean(selectedOnly && wire && wire.resolved);
    const edges = model.attachments.filter(a => !filter || wire.attachments.includes(a.reference));
    const activeHosts = new Set(edges.map(a => a.host));
    const activeLinks = new Set(edges.map(a => a.link));
    if (wire) wire.members.forEach(name => activeHosts.add(name));
    const nodes = [
        ...model.hosts.filter(h => !filter || activeHosts.has(h.name)).map(h => ({id: "h:" + h.name, name: h.name, kind: "host"})),
        ...model.links.filter(l => !filter || activeLinks.has(l.name)).map(l => ({id: "l:" + l.name, name: l.name, kind: "link"})),
    ];
    const adjacent = new Map(nodes.map(n => [n.id, []]));
    edges.forEach(a => { adjacent.get("h:" + a.host).push("l:" + a.link); adjacent.get("l:" + a.link).push("h:" + a.host); });
    const ordered = nodes.slice().sort((a, b) => adjacent.get(b.id).length - adjacent.get(a.id).length || a.id.localeCompare(b.id, "en"));
    const visited = new Set();
    let componentY = 0;
    // Layer by physical hops; wrap dense layers rather than producing a single 40-node column.
    for (const root of ordered) {
        if (visited.has(root.id)) continue;
        const layers = [[root.id]];
        visited.add(root.id);
        for (let depth = 0; depth < layers.length; depth++) {
            const next = [];
            for (const id of layers[depth]) {
                for (const other of adjacent.get(id).slice().sort()) {
                    if (!visited.has(other)) { visited.add(other); next.push(other); }
                }
            }
            if (next.length) layers.push(next);
        }
        const rows = Math.max(3, Math.ceil(Math.sqrt(layers.flat().length)));
        let x = 120;
        for (const layer of layers) {
            layer.forEach((id, index) => {
                const node = nodes.find(n => n.id === id);
                node.x = x + Math.floor(index / rows) * 235;
                node.y = componentY + 70 + (index % rows) * 90;
            });
            x += Math.ceil(layer.length / rows) * 235;
        }
        componentY += Math.min(rows, Math.max(...layers.map(layer => layer.length))) * 90 + 100;
    }
    return {nodes, edges};
}

// Pure functions also run under Node in regression tests, without a DOM dependency.
if (typeof module !== "undefined" && module.exports) module.exports = {forwardingPreview, hostRole, graphLayout};

if (typeof document !== "undefined") (() => {
    const model = JSON.parse(document.getElementById("topology-data").textContent);
    const byId = id => document.getElementById(id);
    const wireSelect = byId("wire"), hostSelect = byId("host"), ingressSelect = byId("ingress");
    const graph = byId("graph"), only = byId("selected-only");
    let selected = null, drawing = null, bounds = [0, 0, 1000, 700];
    const currentWire = () => model.wires.find(w => w.name === wireSelect.value);
    const element = (tag, text, parent) => {
        const node = document.createElement(tag);
        if (text !== undefined) node.textContent = text;
        if (parent) parent.append(node);
        return node;
    };
    const option = (select, value, text) => { const node = element("option", text, select); node.value = value; };
    const svg = (tag, attrs, parent) => {
        const node = document.createElementNS("http://www.w3.org/2000/svg", tag);
        for (const [key, value] of Object.entries(attrs)) node.setAttribute(key, String(value));
        if (parent) parent.append(node);
        return node;
    };
    model.wires.forEach(w => option(wireSelect, w.name, `${w.name} · ${w.wire_id}`));
    option(hostSelect, "", "Select a host");
    model.hosts.forEach(h => option(hostSelect, h.name, `${h.name} · Host ${h.host_id}`));
    byId("source").textContent = JSON.stringify(model.authored, null, 2);
    model.diagnostics.forEach(d => element("p", `${d.stage.toUpperCase()}: ${d.message}`, byId("diagnostics")));
    if (!model.target_available) element("p", "Not deployable on the current target. No generated masks or ingress indices are claimed; resolved attachments remain inspectable.", byId("diagnostics"));

    function setHost(name) {
        hostSelect.value = name;
        selected = "h:" + name;
        ingressSelect.replaceChildren();
        option(ingressSelect, "", "Local origin (index 0)");
        const host = model.hosts.find(h => h.name === name);
        host.interfaces.forEach(i => option(ingressSelect, i.name, `${i.name} · index ${i.ingress_index ?? "unavailable"}`));
        ingressSelect.disabled = false;
    }

    function inspect() {
        const wire = currentWire();
        const name = selected.slice(2), isHost = selected.startsWith("h:");
        byId("selection-title").textContent = name;
        const content = byId("details"); content.replaceChildren();
        if (isHost) {
            const host = model.hosts.find(h => h.name === name);
            const role = hostRole(wire, name);
            const mask = wire?.route_masks?.[name];
            byId("selection-role").textContent = `Host ${host.host_id} · ${role}` +
                (mask !== undefined ? ` · route mask 0x${mask.toString(16).padStart(2, "0")}` : "");
            const table = element("table", undefined, content);
            const head = element("tr", undefined, element("thead", undefined, table));
            ["Interface / Link", "Bit / Ingress", "Selected"].forEach(text => element("th", text, head));
            const body = element("tbody", undefined, table);
            for (const i of host.interfaces) {
                const row = element("tr", undefined, body);
                element("td", `${i.name} → ${i.link}`, row);
                element("td", i.egress_bit === null ? "Unavailable" : `${i.egress_bit} / ${i.ingress_index} (${i.authored_bit === null ? "auto" : "pinned"})`, row);
                element("td", wire?.resolved ? (wire.attachments.includes(`${name}.${i.name}`) ? "Yes" : "No") : "—", row);
            }
            const preview = forwardingPreview(wire, name, ingressSelect.value);
            byId("preview").textContent = preview.status;
        } else {
            const link = model.links.find(l => l.name === name);
            byId("selection-role").textContent = `Physical Link · ${link.link_type}`;
            element("p", [link.arbitration_bitrate && `Arbitration ${link.arbitration_bitrate} bit/s`,
                          link.data_bitrate && `Data ${link.data_bitrate} bit/s`,
                          link.baud_rate && `${link.baud_rate} baud`].filter(Boolean).join(" · ") || "Timing is not specified.", content);
            const list = element("ul", undefined, content);
            model.attachments.filter(a => a.link === name).forEach(a => element("li", a.reference +
                (wire?.resolved ? (wire.attachments.includes(a.reference) ? " · selected" : " · excluded") : ""), list));
            byId("preview").textContent = "Choose a host for an ingress preview.";
        }
        const authoredEntity = isHost ? model.authored.Hosts.find(h => h.Name === name) : model.authored.Links.find(l => l.Name === name);
        const provenance = {source: `${isHost ? "Hosts" : "Links"}.${name}`, declaration: authoredEntity};
        if (wire) {
            provenance.wire = model.authored.Wires.find(w => w.Name === wire.name);
            provenance.groups = Object.fromEntries((provenance.wire.Groups || []).map(g => [g, model.authored.Groups[g]]));
            if (provenance.wire.Path) provenance.path = model.authored.Paths[provenance.wire.Path];
            if (provenance.wire.Realization) provenance.realization = model.authored.Realizations[provenance.wire.Realization];
        }
        byId("provenance").textContent = JSON.stringify(provenance, null, 2);
    }

    function render(resetView = false) {
        const wire = currentWire();
        only.disabled = !wire?.resolved;
        drawing = graphLayout(model, wire, only.checked);
        graph.replaceChildren();
        const active = new Set(wire?.attachments || []);
        const isHost = selected.startsWith("h:");
        const preview = isHost ? forwardingPreview(wire, selected.slice(2), ingressSelect.value) : {outgoing: []};
        const lookup = new Map(drawing.nodes.map(n => [n.id, n]));
        const incoming = isHost && ingressSelect.value ? `${selected.slice(2)}.${ingressSelect.value}` : null;
        for (const edge of drawing.edges) {
            const a = lookup.get("h:" + edge.host), b = lookup.get("l:" + edge.link);
            const classes = ["edge", active.has(edge.reference) ? "selected" : (wire?.resolved ? "excluded" : ""),
                             preview.outgoing.includes(edge.reference) ? "out" : "", edge.reference === incoming ? "in" : ""];
            const line = svg("line", {x1:a.x, y1:a.y, x2:b.x, y2:b.y, class:classes.join(" "), "data-reference":edge.reference, "aria-hidden":true}, graph);
            svg("title", {}, line).textContent = edge.reference;
        }
        for (const node of drawing.nodes) {
            const role = node.kind === "host" ? hostRole(wire, node.name) :
                (wire?.resolved && !drawing.edges.some(e => e.link === node.name && active.has(e.reference)) ? "excluded" : "physical");
            const group = svg("g", {class:`node ${role} ${selected === node.id ? "current" : ""}`, transform:`translate(${node.x},${node.y})`,
                                     tabindex:0, role:"button", "aria-label":`Inspect ${node.kind} ${node.name}`, "data-node":node.id}, graph);
            if (node.kind === "host") svg("rect", {x:-101,y:-28,width:202,height:56,rx:6}, group);
            else {
                // Include the label in the hit area; SVG text intentionally ignores pointer events.
                svg("rect", {x:-101,y:-32,width:202,height:70,class:"hit","pointer-events":"all"}, group);
                svg("circle", {r:10,cy:-18}, group);
            }
            svg("text", {"text-anchor":"middle",y:node.kind === "host" ? -2 : 9,class:"name",
                         "data-name":node.name,"data-kind":node.kind}, group).textContent = node.name;
            svg("text", {"text-anchor":"middle",y:node.kind === "host" ? 17 : 26,class:"sub"}, group).textContent = node.kind === "host" ? role : "shared Link";
            const activate = () => {
                if (node.kind === "host") setHost(node.name);
                else { selected = node.id; hostSelect.value = ""; ingressSelect.disabled = true; }
                render();
                graph.querySelector(`[data-node="${selected}"]`)?.focus();
            };
            group.addEventListener("click", activate);
            group.addEventListener("keydown", event => { if (["Enter", " "].includes(event.key)) { event.preventDefault(); activate(); } });
        }
        byId("summary").textContent = wire ? (wire.resolved ?
            `${wire.name} · ${wire.selection} · ${wire.members.length} members · ${wire.transit_hosts.length} transit hosts · ${wire.attachments.length} selected attachments` : `${wire.name} · unresolved`) :
            `${model.hosts.length} hosts · ${model.links.length} physical Links · ${model.wires.length} Wires`;
        inspect();
        if (resetView) fit(); else applyBounds();
    }

    function applyBounds() {
        graph.setAttribute("viewBox", bounds.join(" "));
        const rect = graph.getBoundingClientRect();
        const scale = Math.min(rect.width / bounds[2], rect.height / bounds[3]);
        const size = Math.max(13, Math.min(25, 11 / Math.max(scale, .01)));
        graph.querySelectorAll("text.name").forEach(text => {
            const name = text.dataset.name;
            const limit = Math.floor(190 / (size * .57));
            let lines = [name];
            if (name.length > limit) {
                const breaks = [...name.matchAll(/[a-z0-9]([A-Z])/g)].map(m => m.index + 1);
                const mid = breaks.sort((a,b) => Math.abs(a-name.length/2)-Math.abs(b-name.length/2))[0] || Math.ceil(name.length/2);
                lines = [name.slice(0,mid), name.slice(mid)];
            }
            text.replaceChildren();
            text.style.fontSize = `${size}px`;
            const y = text.dataset.kind === "link" ? 9 : (lines.length > 1 ? -size*.25 : 4);
            lines.forEach((line,index) => { svg("tspan", {x:0,y:y+index*size}, text).textContent = line; });
        });
        graph.querySelectorAll("text.sub").forEach(text => { text.style.display = size > 15 ? "none" : ""; });
    }
    function fit() {
        if (!drawing.nodes.length) return;
        const xs = drawing.nodes.map(n => n.x), ys = drawing.nodes.map(n => n.y);
        bounds = [Math.min(...xs)-130, Math.min(...ys)-65, Math.max(...xs)-Math.min(...xs)+260, Math.max(...ys)-Math.min(...ys)+130];
        applyBounds();
    }
    function zoom(factor) {
        const nextWidth = bounds[2] * factor;
        if (nextWidth < 200 || nextWidth > 12000) return;
        bounds = [bounds[0] + bounds[2]*(1-factor)/2, bounds[1]+bounds[3]*(1-factor)/2, nextWidth, bounds[3]*factor];
        applyBounds();
    }
    let drag = null;
    graph.addEventListener("pointerdown", e => {
        if (e.target.closest(".node")) return;
        const matrix = graph.getScreenCTM();
        if (!matrix) return;
        drag = {x:e.clientX,y:e.clientY,bounds:bounds.slice(),scaleX:matrix.a,scaleY:matrix.d};
        graph.setPointerCapture(e.pointerId);
    });
    graph.addEventListener("pointermove", e => {
        if (!drag) return;
        bounds = [drag.bounds[0]-(e.clientX-drag.x)/drag.scaleX, drag.bounds[1]-(e.clientY-drag.y)/drag.scaleY, ...drag.bounds.slice(2)];
        applyBounds();
    });
    graph.addEventListener("pointerup", () => { drag = null; });
    graph.addEventListener("pointercancel", () => { drag = null; });
    graph.addEventListener("wheel", e => { e.preventDefault(); zoom(e.deltaY > 0 ? 1.15 : 1/1.15); }, {passive:false});
    byId("fit").addEventListener("click", fit);
    new ResizeObserver(applyBounds).observe(graph);
    byId("zoom-in").addEventListener("click", () => zoom(.8));
    byId("zoom-out").addEventListener("click", () => zoom(1.25));
    wireSelect.addEventListener("change", () => render(true));
    hostSelect.addEventListener("change", () => { if (hostSelect.value) { setHost(hostSelect.value); render(); } });
    ingressSelect.addEventListener("change", () => render());
    only.addEventListener("change", () => render(true));
    setHost(model.hosts.slice().sort((a,b) => b.interfaces.length-a.interfaces.length)[0].name);
    render(true);
})();

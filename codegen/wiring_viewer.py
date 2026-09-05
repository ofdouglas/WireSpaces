#!/usr/bin/env python3
"""Export a self-contained, offline, read-only topology viewer from deployment YAML."""

from __future__ import annotations

import argparse
from dataclasses import asdict
import html
import json
from pathlib import Path
import re

import yaml

from wiring_inspection import explain_deployment
from wiring_projection import project_deployment
from wiring_schema import SchemaLoader, parse_deployment
from wiring_topology import resolve_deployment

ASSETS = Path(__file__).resolve().parent / "viewer"


def viewer_model(data, title: str) -> dict:
    """Keep physical topology inspectable when resolution or target limits reject deployment.

    No alternative routing policy or target-limit bypass is used. Unavailable
    results are marked explicitly rather than synthesized into plausible masks.
    """
    authored = parse_deployment(data)
    resolved = projection = inspection = None
    diagnostics = []
    try:
        resolved = resolve_deployment(authored)
    except ValueError as error:
        diagnostics.append({"stage": "resolution", "message": str(error)})
    if resolved is not None:
        try:
            projection = project_deployment(resolved)
        except ValueError as error:
            diagnostics.append({"stage": "target", "message": str(error)})
    if projection is not None:
        inspection = explain_deployment(projection, next(iter(authored.hosts)))
    hosts = []
    attachments = []
    for name, host in authored.hosts.items():
        interfaces = []
        for interface in sorted(host.interfaces, key=lambda item: item.name):
            details = inspection["hosts"][name]["interfaces"][interface.name] if inspection else {}
            interfaces.append({"name": interface.name, "link": interface.link,
                               "egress_bit": details.get("egress_bit"),
                               "ingress_index": details.get("ingress_index"),
                               "authored_bit": interface.egress_bit})
            attachments.append({"host": name, "interface": interface.name, "link": interface.link,
                                "reference": f"{name}.{interface.name}"})
        hosts.append({"name": name, "host_id": host.host_id, "interfaces": interfaces})
    wires = []
    for name, declaration in authored.wires.items():
        wire = resolved.wires[name] if resolved else None
        wires.append({"name": name, "wire_id": declaration.wire_id,
                      "resolved": wire is not None,
                      "selection": wire.selection if wire else None,
                      "members": list(wire.members) if wire else [],
                      "transit_hosts": list(wire.transit_hosts) if wire else [],
                      "attachments": [a.reference for a in wire.attachments] if wire else [],
                      "route_masks": inspection["wires"][name]["route_masks"] if inspection else None,
                      "source": asdict(declaration)})
    return {"version": 1, "title": title, "target_available": projection is not None,
            "diagnostics": diagnostics, "hosts": hosts,
            "links": [asdict(link) for link in authored.links.values()],
            "attachments": attachments, "wires": wires, "authored": data}


def render_viewer(data, title: str = "WireSpaces topology") -> str:
    """Embed validated data as inert JSON; no network, file-edit or executable YAML surface."""
    model = viewer_model(data, title)
    encoded = json.dumps(model, sort_keys=True, separators=(",", ":"), ensure_ascii=True)
    encoded = encoded.replace("<", "\\u003c").replace(">", "\\u003e").replace("&", "\\u0026")
    template = (ASSETS / "index.html").read_text(encoding="utf-8")
    # One replacement pass: user-controlled text cannot expand another placeholder.
    replacements = {"TITLE": html.escape(title), "STYLE": (ASSETS / "viewer.css").read_text(),
                    "MODEL": encoded, "SCRIPT": (ASSETS / "viewer.js").read_text()}
    return re.sub(r"@@(TITLE|STYLE|MODEL|SCRIPT)@@", lambda match: replacements[match[1]], template)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("-o", "--output", required=True, type=Path)
    parser.add_argument("--title", default=None)
    args = parser.parse_args(argv)
    try:
        if args.input.resolve() == args.output.resolve() or (args.output.exists() and args.input.samefile(args.output)):
            raise ValueError("Read-only viewer cannot overwrite its input YAML")
        if args.output.suffix.lower() != ".html":
            raise ValueError("Viewer output must have an .html extension")
        data = yaml.load(args.input.read_text(encoding="utf-8"), Loader=SchemaLoader)
        rendered = render_viewer(data, args.title or args.input.stem)
        args.output.write_text(rendered, encoding="utf-8")
    except (ValueError, OSError, yaml.YAMLError) as error:
        parser.exit(2, f"wiring_viewer: {error}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

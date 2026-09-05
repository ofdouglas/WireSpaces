"""Strict authoring shapes and their generated JSON Schema (semantic references resolve later)."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Annotated, Literal

from pydantic import AfterValidator, BaseModel, ConfigDict, Field, ValidationError, model_validator

# Uppercase initial avoids C++ keywords; double underscores are implementation-reserved.
# The end assertion also excludes a trailing newline (unlike a bare regex "$" anchor).
# Python regex supports the lookahead used in the emitted JSON Schema.
NAME_PATTERN = r"^(?!.*__)[A-Z][A-Za-z0-9_]*(?![\s\S])"
Identifier = Annotated[str, Field(pattern=re.compile(NAME_PATTERN))]
InterfaceReference = Annotated[str, Field(pattern=re.compile(
    r"^(?!.*__)[A-Z][A-Za-z0-9_]*\.[A-Z][A-Za-z0-9_]*(?![\s\S])"))]
Bitrate = Annotated[int, Field(ge=1, le=0xFFFFFFFF, description="Positive integer bits per second.")]


def unique_names(values):
    if len(values) != len(set(values)):
        raise ValueError("Duplicate reference")
    return values


Names = Annotated[list[Identifier], AfterValidator(unique_names), Field(json_schema_extra={"uniqueItems": True})]
AttachmentList = Annotated[list[InterfaceReference], AfterValidator(unique_names),
                        Field(min_length=2, json_schema_extra={"uniqueItems": True})]


class StrictInput(BaseModel):
    model_config = ConfigDict(strict=True, extra="forbid")

    @model_validator(mode="before")
    @classmethod
    def reject_explicit_null(cls, value):
        # Optional authoring fields can be omitted, but null is not a spelling
        # for omission. The schema exporter applies that same rule for editors.
        if isinstance(value, dict):
            for name, field in value.items():
                if field is None:
                    raise ValueError(f"{name}: explicit null is not allowed; omit the field")
        return value


class InterfaceInput(StrictInput):
    Link: Identifier = Field(description="Name of the shared physical Link.")
    Name: Identifier | None = Field(default=None, description="Local interface name; defaults to Link.")
    EgressBit: Annotated[int, Field(ge=0, le=7)] | None = Field(
        default=None, description="Pinned host-local bit; omit for automatic name-ordered assignment.")


class HostInput(StrictInput):
    Name: Identifier
    HostId: Annotated[int, Field(ge=0, le=254)] = Field(description="Explicit deployment-wide host identity.")
    Interfaces: Annotated[list[Identifier | InterfaceInput], Field(max_length=8)] = Field(
        description="Physical attachments. A string uses the Link name as the interface name.")


class CanLinkInput(StrictInput):
    Name: Identifier
    LinkType: Literal["CAN"]
    ArbitrationBitrate: Bitrate


class FdLinkInput(StrictInput):
    Name: Identifier
    LinkType: Literal["CAN_FD"]
    ArbitrationBitrate: Bitrate
    DataBitrate: Bitrate | None = Field(default=None, description="Presence enables BRS; omit to disable switching.")


class UartLinkInput(StrictInput):
    Name: Identifier
    LinkType: Literal["UART_HDLC"]
    BaudRate: Bitrate


class OtherLinkInput(StrictInput):
    Name: Identifier
    LinkType: Annotated[str, Field(pattern=re.compile(
        r"^(?!(?:CAN|CAN_FD|UART_HDLC)$)(?!.*__)[A-Z][A-Za-z0-9_]*(?![\s\S])"))]
    BaudRate: Bitrate | None = None


SELECTION_FIELDS = ("Links", "Path", "Realization")
SELECTION_RULES = [
    {"not": {"required": [left, right]}}
    for index, left in enumerate(SELECTION_FIELDS) for right in SELECTION_FIELDS[index + 1:]
]


class WireInput(StrictInput):
    model_config = ConfigDict(json_schema_extra={
        "allOf": SELECTION_RULES,
        "anyOf": [{"required": [field], "properties": {field: {"minItems": 1}}} for field in ("Hosts", "Groups")],
    })
    Name: Identifier
    WireId: Annotated[int, Field(ge=1, le=254)]
    Hosts: Names | None = Field(default=None, description="Local Wire members, not all physical bus listeners.")
    Groups: Names | None = Field(default=None, description="Named host groups; combined with Hosts.")
    Links: Names | None = Field(default=None, description="Legacy selection: includes every attachment on each Link.")
    Path: Identifier | None = Field(default=None, description="Named explicit linear chain.")
    Realization: Identifier | None = Field(default=None, description="Named exact attachment tree, including branches.")

    @model_validator(mode="after")
    def membership_and_selection(self):
        if sum(getattr(self, field) is not None for field in SELECTION_FIELDS) > 1:
            raise ValueError("Links, Path and Realization are mutually exclusive")
        if not self.Hosts and not self.Groups:
            raise ValueError("A Wire requires at least one member host or group")
        return self


class RealizationInput(StrictInput):
    Attachments: AttachmentList = Field(description="Exact Host.Interface attachments forming one connected tree.")


class DeploymentInput(StrictInput):
    """WireSpaces deployment: physical topology, local membership and static propagation."""

    Hosts: Annotated[list[HostInput], Field(min_length=1)]
    Links: list[CanLinkInput | FdLinkInput | UartLinkInput | OtherLinkInput]
    Wires: list[WireInput]
    Groups: dict[Identifier, Names] = Field(default_factory=dict, description="Host lists only; no nested groups.")
    Paths: dict[Identifier, Annotated[list[InterfaceReference], Field(min_length=2)]] = Field(
        default_factory=dict, description="Ordered hop pairs sharing a Link, joined through transit hosts.")
    Realizations: dict[Identifier, RealizationInput] = Field(
        default_factory=dict, description="Named exact attachment trees. Selection never grants membership.")


def validate_structure(data) -> DeploymentInput:
    """Reject coercion and unknown fields before normalization or topology resolution."""
    try:
        return DeploymentInput.model_validate(data)
    except ValidationError as error:
        # Report locations without including arbitrary user input or library help URLs.
        messages = [".".join(map(str, item["loc"])) + ": " + item["msg"]
                    for item in error.errors(include_url=False, include_input=False)]
        raise ValueError("Invalid deployment structure:\n" + "\n".join(messages)) from None


def deployment_json_schema() -> dict:
    """Generate the editor contract from the same shapes used for strict validation."""
    schema = DeploymentInput.model_json_schema()

    def omit_null(node):
        if isinstance(node, dict):
            if node.get("default", object()) is None:
                del node["default"]
            if "anyOf" in node:
                variants = [v for v in node["anyOf"] if v != {"type": "null"}]
                if len(variants) == 1:
                    del node["anyOf"]
                    node.update(variants[0])
                else:
                    node["anyOf"] = variants
            for value in node.values():
                omit_null(value)
        elif isinstance(node, list):
            for value in node:
                omit_null(value)

    omit_null(schema)
    schema["$schema"] = "https://json-schema.org/draft/2020-12/schema"
    return schema


def main():
    parser = argparse.ArgumentParser(description="Generate the WireSpaces JSON Schema for YAML editors.")
    parser.add_argument("-o", "--output", type=Path)
    args = parser.parse_args()
    content = json.dumps(deployment_json_schema(), sort_keys=True, indent=2) + "\n"
    if args.output:
        args.output.write_text(content, encoding="utf-8")
    else:
        print(content, end="")


if __name__ == "__main__":
    main()

"""Immutable compiler records. YAML dictionaries exist only at input/output boundaries."""

from __future__ import annotations

from collections.abc import Mapping
from dataclasses import dataclass
from types import MappingProxyType
from typing import Literal, TypeVar

T = TypeVar("T")


def frozen_mapping(values: Mapping[str, T]) -> Mapping[str, T]:
    """Take an immutable snapshot with deterministic name ordering."""
    return MappingProxyType(dict(sorted(values.items())))


@dataclass(frozen=True, slots=True)
class InterfaceDeclaration:
    name: str
    link: str
    egress_bit: int | None = None


@dataclass(frozen=True, slots=True)
class HostDeclaration:
    name: str
    host_id: int
    interfaces: tuple[InterfaceDeclaration, ...]


@dataclass(frozen=True, slots=True)
class LinkDeclaration:
    name: str
    link_type: str
    baud_rate: int | None = None
    arbitration_bitrate: int | None = None
    data_bitrate: int | None = None


@dataclass(frozen=True, slots=True)
class GroupDeclaration:
    name: str
    hosts: tuple[str, ...]


@dataclass(frozen=True, slots=True)
class PathDeclaration:
    name: str
    interfaces: tuple[str, ...]


@dataclass(frozen=True, slots=True)
class RealizationDeclaration:
    name: str
    attachments: tuple[str, ...]


@dataclass(frozen=True, slots=True)
class WireDeclaration:
    name: str
    wire_id: int
    # None preserves an omitted field; () preserves an explicitly empty list.
    hosts: tuple[str, ...] | None = None
    groups: tuple[str, ...] | None = None
    links: tuple[str, ...] | None = None
    path: str | None = None
    realization: str | None = None


@dataclass(frozen=True, slots=True)
class AuthoredDeployment:
    hosts: Mapping[str, HostDeclaration]
    links: Mapping[str, LinkDeclaration]
    wires: Mapping[str, WireDeclaration]
    groups: Mapping[str, GroupDeclaration]
    paths: Mapping[str, PathDeclaration]
    realizations: Mapping[str, RealizationDeclaration]


@dataclass(frozen=True, slots=True)
class Attachment:
    host: str
    interface: str
    link: str

    @property
    def reference(self) -> str:
        return f"{self.host}.{self.interface}"


@dataclass(frozen=True, slots=True)
class ResolvedWire:
    declaration: WireDeclaration
    members: tuple[str, ...]
    transit_hosts: tuple[str, ...]
    attachments: tuple[Attachment, ...]
    selection: Literal["inferred", "links", "path", "realization"]


@dataclass(frozen=True, slots=True)
class ResolvedDeployment:
    authored: AuthoredDeployment
    wires: Mapping[str, ResolvedWire]


@dataclass(frozen=True, slots=True)
class CanProjection:
    """Configured WireSpaces frame format and timing, not hardware capabilities."""

    arbitration_bitrate: int
    data_bitrate: int | None
    fd_frames: bool

    @property
    def bitrate_switch(self) -> bool:
        return self.fd_frames and self.data_bitrate is not None


@dataclass(frozen=True, slots=True)
class InterfaceProjection:
    declaration: InterfaceDeclaration
    egress_bit: int
    baud_rate: int | None
    can: CanProjection | None = None

    @property
    def mask(self) -> int:
        return 1 << self.egress_bit

    @property
    def ingress_index(self) -> int:
        return self.egress_bit + 1

    @property
    def assignment(self) -> str:
        return "automatic" if self.declaration.egress_bit is None else "explicit"


@dataclass(frozen=True, slots=True)
class RouteProjection:
    wire: WireDeclaration
    egress_mask: int


@dataclass(frozen=True, slots=True)
class HostProjection:
    declaration: HostDeclaration
    interfaces: tuple[InterfaceProjection, ...]  # Constructor binding order: ascending bit.
    memberships: tuple[WireDeclaration, ...]
    routes: tuple[RouteProjection, ...]


@dataclass(frozen=True, slots=True)
class TargetProjection:
    resolved: ResolvedDeployment
    hosts: Mapping[str, HostProjection]

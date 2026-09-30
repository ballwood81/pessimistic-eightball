"""Assembly placements and electronics clearance envelopes."""

from dataclasses import dataclass

from build123d import Compound, Shape

from .display_mount import orient_display
from .hardware import battery_envelope, display_envelope, service_connector_envelope
from .params import EnclosureParams, P
from .shell import lower_shell, upper_shell


@dataclass(frozen=True)
class AssemblyComponent:
    name: str
    shape: Shape
    color: tuple[int, int, int]


def placed_display_envelope(p: EnclosureParams = P) -> Shape:
    local = display_envelope(p).translate(
        (0, 0, p.face_distance - p.display_setback)
    )
    return orient_display(local, p)


def assembly_components(
    p: EnclosureParams = P, exploded: bool = False
) -> list[AssemblyComponent]:
    upper_shift = 28.0 if exploded else 0.0
    lower_shift = -28.0 if exploded else 0.0
    upper = upper_shell(p).translate((0, 0, upper_shift))
    lower = lower_shell(p).translate((0, 0, lower_shift))
    display = placed_display_envelope(p).translate((0, 0, upper_shift))
    battery = battery_envelope(p).translate((0, 0, lower_shift))
    connector = service_connector_envelope(p).translate((0, 0, lower_shift))
    return [
        AssemblyComponent("upper_shell", upper, (225, 180, 45)),
        AssemblyComponent("lower_shell", lower, (80, 125, 175)),
        AssemblyComponent("round_display_xiao_keepout", display, (45, 170, 90)),
        AssemblyComponent("battery_keepout", battery, (190, 75, 65)),
        AssemblyComponent("service_connector_keepout", connector, (235, 145, 40)),
    ]


def assembled_step(p: EnclosureParams = P) -> Compound:
    return Compound(children=[c.shape for c in assembly_components(p, exploded=False)])


def exploded_step(p: EnclosureParams = P) -> Compound:
    return Compound(children=[c.shape for c in assembly_components(p, exploded=True)])

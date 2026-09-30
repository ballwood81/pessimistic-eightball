"""Reference envelopes extracted from Seeed's official Eagle files.

Both v1.0 (230308) and v1.1 (230407) contain the same three 2.2 mm plated
mounting holes. v1.1 changes the charging IC and adds a two-position switch;
the enclosure therefore uses the larger v1.1 component keep-out by default.
"""

from dataclasses import dataclass

from build123d import Align, Box, Cylinder, Shape

from .battery import cradle_bottom_z
from .params import EnclosureParams, P

EAGLE_HOLES = (
    (0.0, 19.431),
    (11.811, -15.621),
    (-11.811, -15.621),
)


@dataclass(frozen=True)
class RoundDisplayReference:
    revision: str
    board_diameter: float
    board_thickness: float
    holes: tuple[tuple[float, float], ...]
    drill: float
    rear_keepout_depth: float
    xiao_width: float
    xiao_length: float
    xiao_installed_depth: float


ROUND_DISPLAY = {
    "v1.0": RoundDisplayReference(
        "v1.0", 39.0, 1.6, EAGLE_HOLES, 2.2, 15.0, 17.5, 21.0, 12.0
    ),
    "v1.1": RoundDisplayReference(
        "v1.1", 39.0, 1.6, EAGLE_HOLES, 2.2, 17.0, 17.5, 21.0, 12.0
    ),
}


def display_envelope(p: EnclosureParams = P) -> Shape:
    """Conservative local envelope: glass/PCB disc plus installed XIAO."""
    ref = ROUND_DISPLAY[p.pcb_revision]
    pcb = Cylinder(
        ref.board_diameter / 2,
        ref.board_thickness,
        align=(Align.CENTER, Align.CENTER, Align.MIN),
    ).translate(
        (0, 0, -ref.board_thickness)
    )
    xiao = Box(
        ref.xiao_width,
        ref.xiao_length,
        ref.xiao_installed_depth,
        align=(Align.MIN, Align.MIN, Align.MIN),
    ).translate(
        (
            -ref.xiao_width / 2,
            -ref.xiao_length / 2,
            -ref.board_thickness - ref.xiao_installed_depth,
        )
    )
    return pcb + xiao


def battery_envelope(p: EnclosureParams = P) -> Shape:
    """Pack body plus local thick cable-exit region."""
    body = Box(
        p.battery_length,
        p.battery_width,
        p.battery_thickness,
        align=(Align.MIN, Align.MIN, Align.MIN),
    ).translate(
        (
            -p.battery_length / 2,
            -p.battery_width / 2,
            cradle_bottom_z(p) + p.cradle_floor_thickness,
        )
    )
    cable_end = Box(
        12.0,
        p.battery_width,
        p.battery_cable_end_thickness,
        align=(Align.MIN, Align.MIN, Align.MIN),
    ).translate(
        (
            p.battery_length / 2 - 12.0,
            -p.battery_width / 2,
            cradle_bottom_z(p) + p.cradle_floor_thickness,
        )
    )
    return body + cable_end


def service_connector_envelope(p: EnclosureParams = P) -> Shape:
    x, y, z = p.service_connector_size
    return Box(x, y, z, align=(Align.MIN, Align.MIN, Align.MIN)).translate(
        (-x / 2, -y / 2, -15.0)
    )

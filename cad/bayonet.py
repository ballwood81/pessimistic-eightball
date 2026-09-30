"""Three-lug bayonet adapted from jhermann/things (Apache-2.0).

Changed from the upstream hose connector: three lugs, rectangular printable
sections, independent XYZ clearances, 25-degree travel, hard stops, detents,
wire guard, and dimensions derived from the spherical shell. See NOTICE.
"""

from math import degrees

from build123d import Shape

from .geometry import annular_sector, radial_box, ring
from .params import EnclosureParams, P


def lug_angle_deg(p: EnclosureParams = P) -> float:
    male_outer = p.collar_inner_radius - p.radial_clearance
    return degrees(p.lug_tangential_length / male_outer)


def male_bayonet(p: EnclosureParams = P, test_ring: bool = False) -> Shape:
    male_outer = p.collar_inner_radius - p.radial_clearance
    lug_angle = lug_angle_deg(p)
    z0 = 0.0 if test_ring else -p.shell_thickness
    collar = ring(male_outer - p.collar_wall, male_outer, p.collar_height, z0)
    lug_z = 2.4
    lugs: Shape | None = None
    for angle in range(0, 360, 120):
        lug = radial_box(
            male_outer + p.lug_radial_depth / 2 - 0.1,
            p.lug_tangential_length,
            p.lug_radial_depth + 0.2,
            p.lug_axial_height,
            angle,
            lug_z,
        )
        # 45-degree insertion lead-in on the leading edge.
        lead = radial_box(
            male_outer + p.lug_radial_depth / 2 - 0.1,
            1.5,
            p.lug_radial_depth + 0.2,
            p.lug_axial_height / 2,
            angle - lug_angle / 2,
            lug_z + p.lug_axial_height / 2,
        )
        lug = lug + lead
        lugs = lug if lugs is None else lugs + lug
    if lugs is None:
        raise ValueError("bayonet requires at least one lug")
    return collar + lugs


def female_bayonet(p: EnclosureParams = P, test_ring: bool = False) -> Shape:
    inner = p.collar_inner_radius
    outer = p.collar_outer_radius
    collar = ring(inner, outer, p.collar_height, 0)
    lug_angle = lug_angle_deg(p)
    tangential_angle = degrees(p.tangential_clearance / inner)
    track_outer = inner + p.lug_radial_depth + p.radial_clearance
    lug_z = 2.4

    for angle in range(0, 360, 120):
        entry = annular_sector(
            inner - p.radial_clearance - 0.5,
            track_outer,
            lug_z + p.lug_axial_height + p.axial_clearance + 0.8,
            angle - (lug_angle + tangential_angle + 4.0) / 2,
            lug_angle + tangential_angle + 4.0,
            z=-0.1,
        )
        track = annular_sector(
            inner - p.radial_clearance - 0.2,
            track_outer,
            p.lug_axial_height + 2 * p.axial_clearance,
            angle - lug_angle / 2,
            p.lock_angle_deg + lug_angle + tangential_angle,
            z=lug_z - p.axial_clearance,
        )
        collar = collar - entry - track

        detent_angle = angle + p.lock_angle_deg - 7.5
        detent = radial_box(
            track_outer - p.radial_clearance - p.detent_depth / 2 + 0.1,
            p.detent_width,
            p.detent_depth + p.radial_clearance + 0.2,
            p.lug_axial_height,
            detent_angle,
            lug_z,
        )
        collar = collar + detent

    # Inner guard keeps service wires out of tracks.
    guard = ring(
        inner - p.track_guard,
        inner + 0.2,
        min(2.0, p.collar_height),
        p.collar_height - min(2.0, p.collar_height),
    )
    collar = collar + guard
    return collar


def bayonet_test_parts(p: EnclosureParams = P) -> tuple[Shape, Shape]:
    return female_bayonet(p, test_ring=True), male_bayonet(p, test_ring=True)

"""One-piece snap cradle for the shrink-wrapped 18650 pair.

The reference case is the right overall size and the side hooks are the right
idea, but it has two bores and a gap. This pack is one wrapped body, so the
pocket is a single channel.
"""

from math import sqrt

from build123d import Align, Box, Cylinder, Face, Shape, Vector, Wire, extrude

from .params import EnclosureParams, P


def _pocket_width(p: EnclosureParams) -> float:
    return p.battery_width + 2 * p.battery_pocket_clearance


def _pocket_height(p: EnclosureParams) -> float:
    return p.battery_thickness + p.snap_wrap_extra


def cradle_outer_size(p: EnclosureParams = P) -> tuple[float, float]:
    """Overall length and width, including walls and clip-deflection room."""
    length = p.snap_example_length + p.snap_extra_length
    width = _pocket_width(p) + 2 * p.clip_thickness + 2 * p.clip_deflection_space
    return length, width


def cradle_bottom_z(p: EnclosureParams = P) -> float:
    """Lowest position whose corners stay inside the inner sphere."""
    length, width = cradle_outer_size(p)
    limit = p.inner_radius - 1.0
    radial = sqrt((length / 2) ** 2 + (width / 2) ** 2)
    if radial >= limit:
        raise ValueError("snap cradle is too large for the inner sphere")
    return -sqrt(limit**2 - radial**2)


def _yz_extrude(points: list[tuple[float, float]], x0: float, length: float) -> Shape:
    face = Face(Wire.make_polygon([Vector(x0, y, z) for y, z in points], close=True))
    return extrude(face, amount=length)


def _side_wall(
    p: EnclosureParams,
    *,
    x0: float,
    length: float,
    y_inner: float,
    outward: float,
    z0: float,
) -> Shape:
    """Wall with a 45-degree hook. No center divider."""
    wall = p.clip_thickness
    overlap = p.clip_overlap
    z_top = z0 + p.cradle_floor_thickness + _pocket_height(p)
    y_outer = y_inner + outward * wall
    tip = y_inner - outward * overlap
    if outward > 0:
        points = [
            (y_outer, z0),
            (y_outer, z_top),
            (tip, z_top),
            (y_inner, z_top - overlap),
            (y_inner, z0),
        ]
    else:
        points = [
            (y_outer, z0),
            (y_inner, z0),
            (y_inner, z_top - overlap),
            (tip, z_top),
            (y_outer, z_top),
        ]
    return _yz_extrude(points, x0, length)


def battery_cradle(p: EnclosureParams = P, fit_test: bool = False) -> Shape:
    length, _width = cradle_outer_size(p)
    pocket_w = _pocket_width(p)
    pocket_h = _pocket_height(p)
    z0 = 0.0 if fit_test else cradle_bottom_z(p)
    x0 = -length / 2
    end_wall = 3.0
    cable_relief = 18.0
    hook_length = length - end_wall - cable_relief

    floor = Box(
        length,
        pocket_w + 2 * p.clip_thickness,
        p.cradle_floor_thickness,
        align=(Align.MIN, Align.MIN, Align.MIN),
    ).translate((x0, -(pocket_w + 2 * p.clip_thickness) / 2, z0))
    walls = _side_wall(
        p, x0=x0 + end_wall, length=hook_length, y_inner=pocket_w / 2, outward=1, z0=z0
    ) + _side_wall(
        p, x0=x0 + end_wall, length=hook_length, y_inner=-pocket_w / 2, outward=-1, z0=z0
    )
    stop = Box(
        end_wall,
        pocket_w,
        pocket_h,
        align=(Align.MIN, Align.MIN, Align.MIN),
    ).translate((x0, -pocket_w / 2, z0 + p.cradle_floor_thickness))
    # Low side sills at the cable end. The middle stays open for the 21 mm bump.
    sill_h = 6.0
    sills = None
    for y in (-pocket_w / 2, pocket_w / 2 - 8.0):
        sill = Box(
            3.0, 8.0, sill_h, align=(Align.MIN, Align.MIN, Align.MIN)
        ).translate((x0 + length - 3.0, y, z0 + p.cradle_floor_thickness))
        sills = sill if sills is None else sills + sill
    cradle = floor + walls + stop + sills
    if fit_test:
        return cradle

    # The flat base is only a ring; the middle is open. Posts run from each
    # cradle corner into the spherical wall so the cradle cannot print loose.
    solid_w = pocket_w + 2 * p.clip_thickness
    wall_r = sqrt(p.inner_radius**2 - z0**2)
    posts = None
    for sx in (-1.0, 1.0):
        for sy in (-1.0, 1.0):
            x = sx * length / 2
            y = sy * solid_w / 2
            radial = sqrt(x * x + y * y)
            mid = (radial + wall_r) / 2 + 0.4
            scale = mid / radial
            post = Cylinder(3.0, 3.2).translate((x * scale, y * scale, z0 + 0.4))
            posts = post if posts is None else posts + post
    return cradle + posts


def battery_fit_coupon(p: EnclosureParams = P) -> Shape:
    return battery_cradle(p, fit_test=True)

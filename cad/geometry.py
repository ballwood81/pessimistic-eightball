"""Small reusable geometric operations."""

from math import cos, radians, sin

from build123d import Align, Axis, Cylinder, Face, Shape, Vector, Wire, extrude


def annular_sector(
    inner_radius: float,
    outer_radius: float,
    height: float,
    start_deg: float,
    sweep_deg: float,
    z: float = 0.0,
    segments: int = 24,
) -> Shape:
    """Extruded annular sector; positive sweep is counter-clockwise."""
    steps = max(2, round(segments * abs(sweep_deg) / 90))
    outer = [
        Vector(
            outer_radius * cos(radians(start_deg + sweep_deg * i / steps)),
            outer_radius * sin(radians(start_deg + sweep_deg * i / steps)),
            z,
        )
        for i in range(steps + 1)
    ]
    inner = [
        Vector(
            inner_radius * cos(radians(start_deg + sweep_deg * i / steps)),
            inner_radius * sin(radians(start_deg + sweep_deg * i / steps)),
            z,
        )
        for i in reversed(range(steps + 1))
    ]
    return extrude(Face(Wire.make_polygon(outer + inner, close=True)), amount=height)


def radial_box(
    radius: float,
    tangential: float,
    radial: float,
    height: float,
    angle_deg: float,
    z: float,
) -> Shape:
    """Box centred on a radius and tangent to a circle."""
    from build123d import Box

    part = Box(
        radial, tangential, height, align=(Align.MIN, Align.MIN, Align.MIN)
    ).translate(
        (radius - radial / 2, -tangential / 2, z)
    )
    return part.rotate(Axis.Z, angle_deg)


def ring(inner_radius: float, outer_radius: float, height: float, z: float = 0) -> Shape:
    # ponytail: 0.1 mm overcut avoids coincident-face OCCT failures.
    return (
        Cylinder(
            outer_radius,
            height,
            align=(Align.CENTER, Align.CENTER, Align.MIN),
        ).translate((0, 0, z))
        - Cylinder(
            inner_radius,
            height + 0.2,
            align=(Align.CENTER, Align.CENTER, Align.MIN),
        ).translate((0, 0, z - 0.1))
    )

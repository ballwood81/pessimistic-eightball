"""Angled display face, screenshot-derived curved ledges, and test coupon."""

from math import atan2, cos, degrees, hypot, radians, sin

from build123d import Align, Axis, Cylinder, Face, Shape, Vector, Wire, extrude

from .geometry import annular_sector
from .params import EnclosureParams, P


def orient_display(shape: Shape, p: EnclosureParams = P) -> Shape:
    """Rotate local +Z toward the front (-Y)."""
    return shape.rotate(Axis.X, p.display_angle_deg)


def _hex_prism(across_flats: float, height: float, z: float) -> Shape:
    radius = across_flats / (2 * cos(radians(30)))
    points = [
        Vector(radius * cos(radians(30 + i * 60)), radius * sin(radians(30 + i * 60)), z)
        for i in range(6)
    ]
    return extrude(Face(Wire.make_polygon(points, close=True)), amount=height)


def display_face_plate(p: EnclosureParams = P) -> Shape:
    """Planar shell patch; aperture remains exactly 43.5 mm."""
    z = p.face_distance - p.shell_thickness
    plate = Cylinder(
        p.display_face_diameter / 2,
        p.shell_thickness,
        align=(Align.CENTER, Align.CENTER, Align.MIN),
    ).translate((0, 0, z))
    aperture = Cylinder(
        p.display_opening_diameter / 2,
        p.shell_thickness + 2,
        align=(Align.CENTER, Align.CENTER, Align.MIN),
    ).translate((0, 0, z - 1))
    return orient_display(plate - aperture, p)


def display_reinforcement(p: EnclosureParams = P) -> Shape:
    """Internal annulus ties face, ledges, and curved sphere together."""
    z = p.face_distance - p.shell_thickness - 1.5
    reinforcement = (
        Cylinder(
            p.display_face_diameter / 2 + 2.0,
            1.7,
            align=(Align.CENTER, Align.CENTER, Align.MIN),
        ).translate((0, 0, z))
        - Cylinder(
            p.display_opening_diameter / 2,
            2.1,
            align=(Align.CENTER, Align.CENTER, Align.MIN),
        ).translate((0, 0, z - 0.2))
    )
    return orient_display(reinforcement, p)


def display_ledges(p: EnclosureParams = P) -> Shape:
    """Three curved PCB supports reconstructed from supplied screenshots."""
    contact_z = p.face_distance - p.display_setback
    z = contact_z - p.display_ledge_thickness
    supports: Shape | None = None

    for x, y in p.display_holes:
        centre_angle = degrees(atan2(y, x))
        radius = hypot(x, y)
        # Screenshot supports follow the opening and widen around each screw.
        support = annular_sector(
            max(15.8, radius - 4.0),
            radius + 5.0,
            p.display_ledge_thickness,
            centre_angle - 25.0,
            50.0,
            z=z,
        )
        root = annular_sector(
            p.display_opening_diameter / 2,
            p.display_face_diameter / 2 + 2.0,
            p.display_setback - p.shell_thickness + p.display_ledge_thickness + 0.5,
            centre_angle - 18.0,
            36.0,
            z=z - 1.5,
        )
        hole = Cylinder(
            p.display_hole_diameter / 2,
            p.display_ledge_thickness + 5,
            align=(Align.CENTER, Align.CENTER, Align.MIN),
        ).translate((x, y, z - 2))
        support = support + root - hole

        # Nut pocket is behind the ledge, never inside its 2.5 mm section.
        boss = Cylinder(
            3.2, 2.6, align=(Align.CENTER, Align.CENTER, Align.MIN)
        ).translate((x, y, z - 2.4))
        nut = _hex_prism(4.2, 1.8, z - 2.5).translate((x, y, 0))
        tool = Cylinder(
            2.5, 5.0, align=(Align.CENTER, Align.CENTER, Align.MIN)
        ).translate((x, y, z - 3.0))
        support = support + boss - nut - tool
        supports = support if supports is None else supports + support

    if supports is None:
        raise ValueError("display mount requires at least one support")
    return orient_display(supports, p)


def display_mount(p: EnclosureParams = P) -> Shape:
    return display_face_plate(p) + display_reinforcement(p) + display_ledges(p)


def display_mount_coupon(p: EnclosureParams = P) -> Shape:
    """Small standalone fit coupon with production aperture and support stack."""
    coupon_face_z = 0.0
    face = Cylinder(
        28.0,
        p.shell_thickness,
        align=(Align.CENTER, Align.CENTER, Align.MIN),
    ).translate(
        (0, 0, coupon_face_z - p.shell_thickness)
    )
    opening = Cylinder(
        p.display_opening_diameter / 2,
        20,
        align=(Align.CENTER, Align.CENTER, Align.MIN),
    ).translate((0, 0, -15))
    face = face - opening
    reinforcement = (
        Cylinder(
            p.display_face_diameter / 2 + 2.0,
            1.7,
            align=(Align.CENTER, Align.CENTER, Align.MIN),
        ).translate((0, 0, -4.5))
        - Cylinder(
            p.display_opening_diameter / 2,
            2.1,
            align=(Align.CENTER, Align.CENTER, Align.MIN),
        ).translate((0, 0, -4.7))
    )
    face += reinforcement

    ledges: Shape | None = None
    z = -p.display_setback - p.display_ledge_thickness
    for x, y in p.display_holes:
        a = degrees(atan2(y, x))
        r = hypot(x, y)
        ledge = annular_sector(
            max(15.8, r - 4),
            r + 5,
            p.display_ledge_thickness,
            a - 25,
            50,
            z,
        )
        ledge = ledge - Cylinder(
            p.display_hole_diameter / 2,
            5,
            align=(Align.CENTER, Align.CENTER, Align.MIN),
        ).translate(
            (x, y, z - 1)
        )
        bridge = annular_sector(
            p.display_opening_diameter / 2,
            p.display_face_diameter / 2 + 2.0,
            p.display_setback - p.shell_thickness + p.display_ledge_thickness + 0.5,
            a - 18,
            36,
            z - 1.5,
        )
        ledge = ledge + bridge
        ledges = ledge if ledges is None else ledges + ledge
    if ledges is None:
        raise ValueError("display coupon requires at least one ledge")
    return face + ledges

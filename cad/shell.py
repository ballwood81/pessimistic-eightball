"""Spherical upper/lower shells and service openings."""

from build123d import Align, Axis, Box, Compound, Cylinder, Shape, Sphere

from .battery import battery_cradle
from .bayonet import female_bayonet, male_bayonet
from .display_mount import display_mount, orient_display
from .geometry import radial_box, ring
from .params import EnclosureParams, P


def _sphere_shell(p: EnclosureParams) -> Shape:
    return Sphere(p.outer_radius) - Sphere(p.inner_radius)


def _upper_half_space(p: EnclosureParams) -> Shape:
    size = p.sphere_diameter * 2
    return Box(
        size, size, p.sphere_diameter, align=(Align.MIN, Align.MIN, Align.MIN)
    ).translate((-size / 2, -size / 2, 0))


def _lower_half_space(p: EnclosureParams) -> Shape:
    size = p.sphere_diameter * 2
    return Box(
        size, size, -p.base_z, align=(Align.MIN, Align.MIN, Align.MIN)
    ).translate((-size / 2, -size / 2, p.base_z))


def _display_trim_cutter(p: EnclosureParams) -> Shape:
    size = p.sphere_diameter * 2
    return (
        Box(size, size, p.sphere_diameter, align=(Align.MIN, Align.MIN, Align.MIN))
        .translate((-size / 2, -size / 2, p.face_distance))
        .rotate(Axis.X, p.display_angle_deg)
    )


def _display_aperture(p: EnclosureParams) -> Shape:
    return orient_display(
        Cylinder(p.display_opening_diameter / 2, 60).translate(
            (0, 0, p.face_distance)
        ),
        p,
    )


def usb_plug_path(p: EnclosureParams = P) -> Shape:
    """Full moulded-plug insertion envelope through the upper rear."""
    return Box(
        p.usb_plug_width + 1.0,
        p.usb_plug_length + 10.0,
        p.usb_plug_height + 1.0,
        align=(Align.MIN, Align.MIN, Align.MIN),
    ).translate(
        (
            -(p.usb_plug_width + 1.0) / 2,
            p.outer_radius - p.usb_plug_length - 5.0,
            12.0,
        )
    )


def _connection_flange(p: EnclosureParams) -> Shape:
    """Join the upper track ring into the shell wall."""
    return ring(
        p.collar_outer_radius - 1.0,
        p.outer_radius - 0.8,
        p.shell_thickness,
        0,
    )


def _lower_bayonet_web(p: EnclosureParams) -> Shape:
    """Join the smaller lower lug ring into the shell wall.

    The upper flange stops outside this ring, leaving the gap visible in the slicer.
    """
    male_outer = p.collar_inner_radius - p.radial_clearance
    return ring(
        male_outer - 1.0,
        p.inner_radius + 1.5,
        p.shell_thickness,
        -p.shell_thickness,
    )


def upper_shell(p: EnclosureParams = P) -> Shape:
    shell = (_sphere_shell(p) & _upper_half_space(p)) - _display_trim_cutter(p)
    mark = radial_box(p.outer_radius - 0.5, 4.0, 2.0, 1.5, 0.0, 0.2)
    shell = shell - mark - _display_aperture(p)
    shell = shell + display_mount(p)
    shell = shell - usb_plug_path(p)
    collar = female_bayonet(p) + _connection_flange(p)
    return Compound(children=[shell, collar])


def lower_shell(p: EnclosureParams = P) -> Shape:
    shell = _sphere_shell(p) & _lower_half_space(p)
    mark = radial_box(p.outer_radius - 0.5, 2.0, 2.0, 1.5, 0.0, -1.3)
    shell = shell - mark
    shell = shell + male_bayonet(p) + _lower_bayonet_web(p)
    shell = shell + battery_cradle(p)

    return shell


def usb_strain_relief(p: EnclosureParams = P) -> Shape:
    """Separate clamp: captures jacket, never bears on the XIAO socket."""
    width = p.usb_plug_width + 8.0
    clamp = Box(width, 8.0, 5.0, align=(Align.MIN, Align.MIN, Align.MIN))
    cable = (
        Cylinder(p.usb_cable_diameter / 2, 10.0)
        .rotate(Axis.X, 90)
        .translate((width / 2, 9.0, 2.5))
    )
    clamp = clamp - cable
    for x in (3.0, width - 3.0):
        clamp -= Cylinder(1.2, 8.0).translate((x, 4.0, -1.0))
    return clamp

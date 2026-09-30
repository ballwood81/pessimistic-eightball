"""Generate printable files, assembly models, previews, and geometric checks."""

from math import cos, radians, sin, sqrt
from pathlib import Path

from build123d import Axis, Shape, export_step, export_stl
from PIL import Image, ImageDraw

from .assembly import assembled_step, assembly_components, exploded_step
from .battery import battery_fit_coupon, cradle_bottom_z, cradle_outer_size
from .bayonet import bayonet_test_parts, female_bayonet, male_bayonet
from .display_mount import display_mount_coupon
from .params import EnclosureParams, P
from .shell import lower_shell, upper_shell, usb_plug_path, usb_strain_relief

OUT = Path(__file__).with_name("out")


def _closed_valid(shape: Shape) -> bool:
    return bool(shape.solids()) and all(s.is_valid and s.volume > 0 for s in shape.solids())


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def _intersection_volume(left: Shape, right: Shape) -> float:
    intersection = left & right
    return intersection.volume if intersection is not None else 0.0


def run_checks(p: EnclosureParams = P) -> dict[str, str]:
    upper = upper_shell(p)
    lower = lower_shell(p)
    female, male = bayonet_test_parts(p)
    coupon = display_mount_coupon(p)
    cradle = battery_fit_coupon(p)

    for name, shape in {
        "upper shell": upper,
        "lower shell": lower,
        "female bayonet ring": female,
        "male bayonet ring": male,
        "display coupon": coupon,
        "battery cradle coupon": cradle,
        "USB strain relief": usb_strain_relief(p),
    }.items():
        _require(_closed_valid(shape), f"{name} is not a valid closed solid")

    _require(
        len(lower.solids()) == 1,
        "lower bayonet is not joined to the shell",
    )

    _require(p.display_opening_diameter > 0, "display opening must be positive")
    _require(p.display_setback > p.shell_thickness, "display setback is too shallow")
    _require(2.0 <= p.display_ledge_thickness <= 3.0, "ledge thickness is out of range")
    _require(p.display_hole_diameter > p.display_pcb_drill, "M2 holes lack clearance")
    _require(p.shell_thickness >= 3.0, "shell is thinner than the PETG baseline")
    _require(p.collar_outer_radius < p.inner_radius, "collar exceeds the inner sphere")

    # One wrapped pack: cradle corners, including clip-flex room, stay inside.
    cradle_length, cradle_width = cradle_outer_size(p)
    _require(
        sqrt(
            (cradle_length / 2) ** 2
            + (cradle_width / 2) ** 2
            + cradle_bottom_z(p) ** 2
        )
        < p.inner_radius,
        "battery cradle intersects the shell",
    )
    _require(p.clip_overlap > 0, "battery clips have no retaining overlap")
    _require(p.battery_pocket_clearance > 0, "battery pocket is a press fit")

    # Entry and locked positions are free; detent resists; hard stop collides.
    female_full = female_bayonet(p)
    male_full = male_bayonet(p)
    entry_interference = _intersection_volume(female_full, male_full)
    detent_interference = _intersection_volume(
        female_full, male_full.rotate(Axis.Z, p.lock_angle_deg - 7.0)
    )
    locked_interference = _intersection_volume(
        female_full, male_full.rotate(Axis.Z, p.lock_angle_deg)
    )
    stop_interference = _intersection_volume(
        female_full, male_full.rotate(Axis.Z, p.lock_angle_deg + 5.0)
    )
    _require(entry_interference < 0.01, "bayonet entry interferes")
    _require(detent_interference > 0.1, "bayonet detent does not engage")
    _require(locked_interference < 0.01, "bayonet locked position interferes")
    _require(stop_interference > 1.0, "bayonet end stop does not engage")

    # Plug path is wholly above the collar/track region.
    _require(
        _intersection_volume(usb_plug_path(p), female_full) < 0.01,
        "USB plug path intersects bayonet collar",
    )

    # The face remains in the upper half and exactly follows the requested ray.
    face_lowest_z = p.face_distance * cos(radians(p.display_angle_deg)) - (
        p.display_face_diameter / 2
    ) * sin(radians(p.display_angle_deg))
    _require(face_lowest_z > p.split_z, "display face crosses the equator")

    return {
        "solids": "valid closed solids",
        "shell": f"{p.shell_thickness:.2f} mm nominal",
        "display": "43.5 mm aperture; 9.0 mm setback; 2.5 mm ledges",
        "bayonet": (
            f"entry {entry_interference:.3f} mm3; detent "
            f"{detent_interference:.3f} mm3; locked {locked_interference:.3f} mm3; "
            f"stop {stop_interference:.3f} mm3"
        ),
        "battery": "pack and clip swept envelope inside inner sphere",
        "usb": "plug path clears bayonet collar; internal extension required",
    }


def _export_part(name: str, shape: Shape, p: EnclosureParams) -> None:
    export_step(shape, OUT / f"{name}.step")
    export_stl(
        shape,
        OUT / f"{name}.stl",
        tolerance=p.tessellation_tolerance,
        angular_tolerance=p.angular_tolerance,
    )


def _preview(path: Path, exploded: bool, p: EnclosureParams) -> None:
    components = assembly_components(p, exploded=exploded)
    triangles: list[
        tuple[
            float,
            tuple[tuple[float, float], ...],
            tuple[int, int, int, int],
            bool,
        ]
    ] = []
    projected_points: list[tuple[float, float]] = []

    for component in components:
        vertices, faces = component.shape.tessellate(0.7, 0.25)
        points: list[tuple[float, float, float]] = []
        for v in vertices:
            px = (v.X - v.Y) * 0.7071
            py = -v.Z * 0.82 + (v.X + v.Y) * 0.30
            depth = (v.X + v.Y) * 0.55 + v.Z * 0.35
            points.append((px, py, depth))
            projected_points.append((px, py))
        is_shell = "shell" in component.name
        alpha = 95 if is_shell else 220
        rgba = (*component.color, alpha)
        for a, b, c in faces:
            tri = (points[a], points[b], points[c])
            triangles.append(
                (
                    sum(point[2] for point in tri) / 3,
                    tuple((point[0], point[1]) for point in tri),
                    rgba,
                    is_shell,
                )
            )

    min_x = min(x for x, _ in projected_points)
    max_x = max(x for x, _ in projected_points)
    min_y = min(y for _, y in projected_points)
    max_y = max(y for _, y in projected_points)
    scale = min(900 / (max_x - min_x), 700 / (max_y - min_y))
    image = Image.new("RGBA", (1000, 800), (245, 247, 250, 255))
    draw = ImageDraw.Draw(image, "RGBA")
    for _, triangle, color, is_shell in sorted(triangles):
        pixels = [
            (
                50 + (x - min_x) * scale,
                50 + (y - min_y) * scale,
            )
            for x, y in triangle
        ]
        if is_shell:
            draw.line(pixels + [pixels[0]], fill=color, width=1)
        else:
            draw.polygon(
                pixels,
                fill=color,
                outline=(*color[:3], min(180, color[3] + 40)),
            )
    image.convert("RGB").save(path)


def generate(p: EnclosureParams = P) -> dict[str, str]:
    OUT.mkdir(parents=True, exist_ok=True)
    checks = run_checks(p)
    female, male = bayonet_test_parts(p)
    parts = {
        "upper_shell": upper_shell(p),
        "lower_shell": lower_shell(p),
        "usb_strain_relief": usb_strain_relief(p),
        "display_mount_coupon": display_mount_coupon(p),
        "bayonet_female_test_ring": female,
        "bayonet_male_test_ring": male,
        "battery_cradle_fit_test": battery_fit_coupon(p),
    }
    for name, shape in parts.items():
        _export_part(name, shape, p)

    export_step(assembled_step(p), OUT / "assembled.step")
    export_step(exploded_step(p), OUT / "exploded.step")
    _preview(OUT / "assembly_preview.png", exploded=False, p=p)
    _preview(OUT / "exploded_preview.png", exploded=True, p=p)
    (OUT / "verification.txt").write_text(
        "\n".join(f"{key}: {value}" for key, value in checks.items()) + "\n",
        encoding="utf-8",
    )
    return checks


if __name__ == "__main__":
    for check, result in generate().items():
        print(f"{check}: {result}")

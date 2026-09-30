"""All enclosure dimensions in millimetres.

Provenance:
  TESTED      physical PETG fit supplied by the project owner
  REFERENCE   Seeed Eagle/wiki or measured reference model
  PROVISIONAL first-print value; tune from the supplied coupons
"""

from dataclasses import dataclass, field
from math import sqrt

TESTED = "tested"
REFERENCE = "reference"
PROVISIONAL = "provisional"


@dataclass(frozen=True)
class EnclosureParams:
    sphere_diameter: float = 150.0
    shell_thickness: float = 3.0
    base_footprint_diameter: float = 65.0
    split_z: float = 0.0

    display_face_diameter: float = 50.0
    display_opening_diameter: float = 43.5  # TESTED: do not add clearance
    display_angle_deg: float = 40.0
    display_setback: float = 9.0  # TESTED: exterior face to PCB support face
    display_ledge_thickness: float = 2.5
    display_hole_diameter: float = 2.4
    display_holes: tuple[tuple[float, float], ...] = (
        (0.0, 19.431),
        (11.811, -15.621),
        (-11.811, -15.621),
    )
    display_pcb_drill: float = 2.2
    display_pcb_diameter: float = 39.0
    display_keepout_depth: float = 17.0
    xiao_size: tuple[float, float, float] = (21.0, 17.5, 12.0)
    pcb_revision: str = "v1.1"

    collar_height: float = 10.0
    collar_wall: float = 3.0
    collar_edge_margin: float = 1.0
    lug_count: int = 3
    lug_radial_depth: float = 3.2
    lug_axial_height: float = 3.0
    lug_tangential_length: float = 9.0
    lock_angle_deg: float = 25.0
    radial_clearance: float = 0.35
    axial_clearance: float = 0.30
    tangential_clearance: float = 0.40
    detent_depth: float = 0.45
    detent_width: float = 2.2
    track_guard: float = 1.5

    battery_length: float = 66.0
    battery_width: float = 36.0
    battery_thickness: float = 19.0
    battery_cable_end_thickness: float = 21.0
    battery_pocket_clearance: float = 0.6
    # Measured external size of .pio/models/18650_snap_case.stl.
    snap_example_length: float = 71.5
    snap_extra_length: float = 4.0  # within the requested 2-5 mm
    snap_wrap_extra: float = 1.0  # one shrink-wrap layer beyond a bare cell
    cradle_floor_thickness: float = 2.4
    clip_length: float = 18.0
    clip_thickness: float = 2.0
    clip_overlap: float = 2.0
    clip_deflection_space: float = 3.0
    cable_end_yaw_deg: float = 0.0
    service_connector_size: tuple[float, float, float] = (10.0, 6.0, 8.0)

    usb_plug_width: float = 12.5
    usb_plug_height: float = 7.0
    usb_plug_length: float = 22.0
    usb_cable_diameter: float = 4.5
    usb_bend_radius: float = 15.0

    tessellation_tolerance: float = 0.08
    angular_tolerance: float = 0.2

    provenance: dict[str, str] = field(
        default_factory=lambda: {
            "display_opening_diameter": TESTED,
            "display_setback": TESTED,
            "display_holes": REFERENCE,
            "display_pcb_drill": REFERENCE,
            "display_pcb_diameter": REFERENCE,
            "radial_clearance": PROVISIONAL,
            "battery_dimensions": PROVISIONAL,
            "clip_geometry": PROVISIONAL,
            "detent_geometry": PROVISIONAL,
            "usb_plug_envelope": PROVISIONAL,
        }
    )

    def __post_init__(self) -> None:
        if not 30.0 <= self.display_angle_deg <= 45.0:
            raise ValueError("display_angle_deg must be between 30 and 45")
        if not 2.0 <= self.display_ledge_thickness <= 3.0:
            raise ValueError("display ledge must be 2-3 mm thick")
        if self.lug_count != 3:
            raise ValueError("this enclosure requires exactly three bayonet lugs")
        if self.pcb_revision not in {"v1.0", "v1.1"}:
            raise ValueError("pcb_revision must be 'v1.0' or 'v1.1'")
        if not self.display_holes:
            raise ValueError("at least one display mounting hole is required")

    @property
    def outer_radius(self) -> float:
        return self.sphere_diameter / 2

    @property
    def inner_radius(self) -> float:
        return self.outer_radius - self.shell_thickness

    @property
    def base_z(self) -> float:
        return -sqrt(self.outer_radius**2 - (self.base_footprint_diameter / 2) ** 2)

    @property
    def face_distance(self) -> float:
        return sqrt(self.outer_radius**2 - (self.display_face_diameter / 2) ** 2)

    @property
    def collar_outer_radius(self) -> float:
        half_height = self.collar_height / 2
        available = sqrt(self.inner_radius**2 - half_height**2)
        return available - self.collar_edge_margin

    @property
    def collar_inner_radius(self) -> float:
        return (
            self.collar_outer_radius
            - self.collar_wall
            - self.lug_radial_depth
            - self.radial_clearance
        )


P = EnclosureParams()

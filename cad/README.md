# Pessimistic Eight Ball enclosure

Parametric Build123d source for a PETG, 150 mm spherical enclosure. This is a
reviewable first-print design, not a verified production fit.

## Generate

Python 3.10 is the minimum; Python 3.11+ is recommended.

```powershell
python -m pip install -r cad/requirements.txt
python -m cad.export
```

Outputs are written to `cad/out/`:

- `upper_shell.{step,stl}`
- `lower_shell.{step,stl}`
- `usb_strain_relief.{step,stl}`
- `display_mount_coupon.{step,stl}`
- `bayonet_{female,male}_test_ring.{step,stl}`
- `battery_cradle_fit_test.{step,stl}`
- `assembled.step`, `exploded.step`
- `assembly_preview.png`, `exploded_preview.png`
- `verification.txt`

Edit `cad/params.py`, then regenerate. All dimensions are millimetres.

## Dimension status

Physically tested:

- Display aperture: **43.5 mm exactly**. No added clearance.
- Display support contact face: **9.0 mm** behind the exterior face.

Verified from both official Seeed Eagle boards:

- v1.0 and v1.1 mounting holes are identical: `(0, 19.431)`,
  `(11.811, -15.621)`, `(-11.811, -15.621)`, drill 2.2 mm.
- Printed ledge holes are 2.4 mm for M2 fit testing.
- Seeed describes the board as 39 mm. v1.1 changes the charge IC and adds the
  two-position switch; this model defaults to the larger v1.1 rear keep-out.
- Facing the glass, XIAO USB-C points right and the display switch is lower
  left, matching [Seeed's documented orientation][seeed].

Provisional and deliberately adjustable:

- 3.0 mm shell; 65 mm base footprint; 40 degree display angle.
- 2.5 mm curved ledges. Their shape follows the supplied screenshots: three
  local arcs around the 43.5 mm opening, with deeper root ribs. Screenshot
  construction points were not used as screw centres.
- Bayonet radial clearance 0.35 mm per side; axial 0.30 mm; tangential
  0.40 mm. Detent depth, clip retention, and shake resistance are untested.
- Battery pocket 0.6 mm clearance; clips 1.8 mm thick with 1.2 mm overlap.
- Moulded USB plug envelope 12.5 x 7 x 22 mm and 15 mm bend radius.
- Three-wire service connector envelope. `JST-XH 3-pin` is only a planning
  placeholder; set dimensions to the connector actually fitted.

## Design

The sphere is split at its equator. A derived internal collar carries three
120-degree bayonet lugs. Travel is 25 degrees. Entry slots have lead-ins;
tracks have positive end walls, radial detent bumps, and an inner wire guard.
One wide and one narrow seam notch form discreet insertion marks.

The upper face is a plane normal to a ray 40 degrees from vertical. Its
distance is derived from a 50 mm sphere chord. The 43.5 mm aperture is cut
normal to that plane. Three curved 2.5 mm ledges place their PCB contact faces
9 mm behind the exterior. Nut pockets are in thicker rear bosses, not in the
ledges.

Use three M2 x 6 mm pan-head screws and M2 nuts as the first stack trial:
1.6 mm reference PCB + 2.5 mm ledge + 1.6 mm nut. Verify the actual board,
head seating, nut engagement, and tip clearance before powering it. The
fasteners load the PCB mounting holes; they must not clamp the glass.

The reference snap case is 71.5 mm long and suits two loose cells with a gap
between them. This cradle is one channel for the shrink-wrapped pair: 4 mm
longer than that case, and 1 mm taller than the 19 mm pack body for the wrap.
Side hooks cam open and close over the pack. They stop before the 21 mm cable
end. There is no center divider. The cable end of the channel is open in the
middle. Route all three wires out that opening to a removable three-pin
connector inside the track guard.
Unplug it before separating the halves.

The rear upper-shell USB opening passes the full provisional moulded plug and
stays above the bayonet. Direct alignment to the sideways XIAO socket is not
practical in this geometry. Fit a short panel-free USB-C male-to-female
extension internally. Secure its cable jacket with the separate M2 strain
relief; do not transfer cable loads to the XIAO socket.

## Assembly order

1. Print and test the display coupon, bayonet rings, and battery cradle.
2. Install M2 nuts in the display bosses. Seat the Round Display without
   loading the active glass, fit the XIAO and headers, then install screws.
3. Fit the USB-C extension and clamp its jacket.
4. Push the battery into the lower cradle, cable end toward the configured
   `+X` end. Connect the removable three-wire lead and leave a service loop.
5. Keep the loop inside the guard, align seam marks, insert the upper half,
   rotate through the detent to the hard stop.
6. To service, rotate back, lift only enough to reach the connector, unplug,
   then separate fully.

## Printing

- Material: PETG. Start with 0.20 mm layers, 4 perimeters, and 5 top/bottom
  layers. Tune from coupons before printing shells.
- Upper shell: place the exterior display plane on the bed. Support is still
  required under portions of the inner sphere, root ribs, nut bosses, and
  bayonet roof. Block support from the 43.5 mm aperture and screw holes.
- Lower shell: flat base on the bed. Use local support only where the collar
  needs it. Clips print vertically so layer lines do not split across their
  root.
- Bayonet rings: flat annular face on the bed.
- Display coupon: exterior face on the bed.
- Battery coupon: cradle floor on the bed. Do not force the wrapped pack into
  an undersized print.

## Verification and outstanding checks

`python -m cad.export` fails if a printable body is not a valid closed solid,
the battery/clip envelope leaves the inner sphere, bayonet entry or locked
positions interfere, the detent has no engagement, the end stop has no
collision, or the USB plug path enters the collar.

CAD checks do not validate printer compensation, PETG creep, clip fatigue,
nut capture, connector orientation, touch access, cable flex life, detent
force, drop strength, or shake resistance. Print the three coupons first.
Measure the actual assembled XIAO/header depth and actual plug/connector before
the full shells.

## Attribution

Bayonet concepts are adapted from Johannes Hermann's
[AC Hose Bayonet Quick Connectors][bayonet], Apache-2.0. This version changes
the lug count, sections, fit model, path, stops, detents, wire guard, and
enclosure integration. See `NOTICE` and `LICENSE-Apache-2.0.txt`.

[bayonet]: https://github.com/jhermann/things/tree/main/models/ac-bayonet-connectors
[seeed]: https://wiki.seeedstudio.com/get_start_round_display/

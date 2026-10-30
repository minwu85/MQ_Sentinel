"""Laser elbow (v23, mm): screws into the threaded socket in the bottom of the encoder housing and holds the laser horizontally.
Run from CAD/: python laser.py -> laser.step (assembled position: stub z0-13 in the socket (10 mm in plate + encoder, 3 mm lead above), flange against the housing/insert underside).
Stub: hollow bolt (core R8, ridge to R10, bore 8 for the laser wires). Flange dia 30 (z-3..0). Column dia 20, then a horizontal tube
along +X (0 deg = sensor notch 1): OD 16.5, bore 10.5 for the laser module, 70 long, one M3 grub-screw hole on top.
The thread phase is fixed so that, screwed in until the flange touches, the tube points at 0 deg (one of the three sensors, 0/120/240)."""
import cadquery as cq

PITCH, W0, W1, CORE_R, TIP_R, BORE_R = 2.5, 1.4, 0.5, 8.0, 10.0, 4.0
STUB_TOP = 13.0                                  # stub top (platform 1 top is z10): 3 mm lead-in above it, so it is fully engaged in plate + encoder
M_PHASE = 0.0                                    # male helix start z (ridge at +X at z = 0, 2.5, ...)
FLANGE_D, FLANGE_T, COL_D = 30.0, 3.0, 20.0
TUBE_OD, TUBE_BORE, TUBE_L, TUBE_Z = 16.5, 10.5, 70.0, -13.0   # v28: axis z-13 = 150 below the sensor ring centreline (z137)
SLOT_X0, SLOT_X1, SLOT_W = 14.0, 56.0, 6.0     # long open slot on top of the tube (easier to print, laser can be pressed in)
GRUB_X, GRUB_D = 63.0, 2.6

def disc(d, z0, t):
    return cq.Workplane("XY").workplane(offset=z0).circle(d / 2).extrude(t)

helix = cq.Wire.makeHelix(PITCH, STUB_TOP + 2 * PITCH, CORE_R - 0.3)
prof = cq.Workplane("XZ").polyline([(CORE_R - 0.3, -W0 / 2), (TIP_R, -W1 / 2), (TIP_R, W1 / 2), (CORE_R - 0.3, W0 / 2)]).close()
thread = (prof.sweep(cq.Workplane("XY").newObject([helix]), isFrenet=True).translate((0, 0, M_PHASE - PITCH))
          .intersect(disc(2 * TIP_R + 2, 0.0, STUB_TOP)))
stub = disc(2 * CORE_R, 0.0, STUB_TOP).union(thread)
flange = disc(FLANGE_D, -FLANGE_T, FLANGE_T)
tube_r = TUBE_OD / 2
column = disc(COL_D, TUBE_Z - tube_r, tube_r * 2 + (-FLANGE_T - (TUBE_Z + tube_r)) )
tube = cq.Workplane("YZ").workplane(offset=0).center(0, TUBE_Z).circle(tube_r).extrude(TUBE_L)
model = stub.union(flange).union(column).union(tube)
model = model.cut(cq.Workplane("YZ").center(0, TUBE_Z).circle(TUBE_BORE / 2).extrude(TUBE_L))          # laser bore, from the tip to the axis
model = model.cut(disc(2 * BORE_R, TUBE_Z, STUB_TOP - TUBE_Z))                                                # wire bore up through the stub
model = model.cut(cq.Workplane("XY").workplane(offset=TUBE_Z).center((SLOT_X0 + SLOT_X1) / 2, 0).rect(SLOT_X1 - SLOT_X0, SLOT_W).extrude(tube_r + 1))   # long slot on top (v25)
model = model.cut(cq.Workplane("XZ").workplane(offset=-(tube_r + 1)).center((SLOT_X0 + SLOT_X1) / 2, TUBE_Z).rect(SLOT_X1 - SLOT_X0, SLOT_W).extrude(2 * tube_r + 2))   # side window through both walls (balanced)
model = model.cut(cq.Workplane("XY").workplane(offset=TUBE_Z).center(GRUB_X, 0).circle(GRUB_D / 2).extrude(tube_r + 1))

if __name__ == "__main__":
    b = model.val().BoundingBox()
    print(f"laser: valid {model.val().isValid()}, solids {len(model.val().Solids())}, x {b.xmin:.1f}..{b.xmax:.1f} z {b.zmin:.1f}..{b.zmax:.1f}")
    cq.exporters.export(model, "laser.step")

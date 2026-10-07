"""Flat lid with handle and threaded skirt for the top ring (mm). Run: python lid.py -> lid.step/.stl
Shown in assembled position (underside at z = ring top = 135). Shares thread constants with encoder_housing.py."""
import math
import cadquery as cq

D, T_LID, Z_RING_TOP = 150.0, 4.0, 135.0
SKIRT_IN, SKIRT_OUT, SKIRT_H = 55.0, 58.0, 10.0     # 1 cm threaded skirt below the lid
T_PITCH, T_R0, T_R1, T_W0, T_W1 = 4.0, 57.9, 59.7, 2.0, 0.6   # external ridge, mates ring's 60.1 -> 58.5
HOLE_R, HOLE_D = 67.5, 3.0                           # same 4 holes as the ring
HANDLE_W, HANDLE_T, HANDLE_H = 60.0, 6.0, 20.0
HANDLE_SLOT_L, HANDLE_SLOT_W = 10.0, 6.0             # 1 cm slot through the handle apex       # arched handle

lid = cq.Workplane("XY").workplane(offset=Z_RING_TOP).circle(D / 2).extrude(T_LID)
lid = lid.cut(cq.Workplane("XY").workplane(offset=Z_RING_TOP).polarArray(HOLE_R, 45, 360, 4).circle(HOLE_D / 2).extrude(T_LID))

z_skirt = Z_RING_TOP - SKIRT_H
skirt = (cq.Workplane("XY").workplane(offset=z_skirt).circle(SKIRT_OUT).circle(SKIRT_IN).extrude(SKIRT_H))
helix = cq.Wire.makeHelix(T_PITCH, SKIRT_H + 3 * T_PITCH, T_R0)
prof = cq.Workplane("XZ").polyline([(T_R0, -T_W0 / 2), (T_R1, -T_W1 / 2), (T_R1, T_W1 / 2), (T_R0, T_W0 / 2)]).close()
thread = (prof.sweep(cq.Workplane("XY").newObject([helix]), isFrenet=True)
          .translate((0, 0, z_skirt - T_PITCH / 2))   # half-pitch phase so ridges sit in the ring grooves when seated
          .intersect(cq.Workplane("XY").workplane(offset=z_skirt).circle(T_R1 + 1).extrude(SKIRT_H)))

a, b = HANDLE_W / 2, HANDLE_W / 2 - HANDLE_T
leg = HANDLE_H / 2
handle = (cq.Workplane("XZ").workplane(offset=0).moveTo(-a, 0).lineTo(-a, leg)
          .threePointArc((0, HANDLE_H), (a, leg)).lineTo(a, 0).lineTo(b, 0).lineTo(b, leg)
          .threePointArc((0, HANDLE_H - HANDLE_T), (-b, leg)).lineTo(-b, 0).close()
          .extrude(6, both=True).translate((0, 0, Z_RING_TOP + T_LID)))

model = lid.union(skirt).union(thread).union(handle)
z_apex = Z_RING_TOP + T_LID + HANDLE_H
model = model.cut(cq.Workplane("XY").workplane(offset=z_apex - HANDLE_T - 2)
                  .rect(HANDLE_SLOT_L, HANDLE_SLOT_W).extrude(HANDLE_T + 4))
bb = model.val().BoundingBox()
print(f"lid bbox: {bb.xlen:.1f} x {bb.ylen:.1f} x {bb.zlen:.1f} mm, solids {len(model.val().Solids())}")
cq.exporters.export(model, "lid.step")
cq.exporters.export(model, "lid.stl")

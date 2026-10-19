"""Cap (lid) for the top ring, 4-section quarter-turn thread like a bottle lid (mm).
Run from CAD/: python lid.py -> lid.step/.stl. Shown seated on the housing (flange on the ring top, z=133).
Overall housing + cap height = 170 mm (Appendix 1)."""
import math
import cadquery as cq

D, Z_RING_TOP, Z_TOP = 150.0, 133.0, 170.0
FLANGE_T, TOP_T, SHELL_WALL = 3.0, 4.0, 3.0              # flange on the ring, top disc, shell wall
SKIRT_IN, SKIRT_OUT, SKIRT_H = 55.0, 58.0, 10.0          # 1 cm threaded skirt, goes into the ring bore
T_PITCH, T_R0, T_R1, T_W0, T_W1 = 4.0, 57.9, 59.7, 2.0, 0.6   # external ridge, mates the ring's 60.1 -> 58.5
LID_SECT = 8.0                                           # half-angle of each lid section (passes the ring gaps)
HOLE_R, HOLE_D = 67.5, 3.0                               # same 4 holes as the ring
SLOT_L, SLOT_W = 20.0, 8.0                               # finger slot in the top (no raised handle: 170 mm limit)

def disc(d, z0, t):
    return cq.Workplane("XY").workplane(offset=z0).circle(d / 2).extrude(t)

def sectors(centres, half, z0, h):
    tt = math.tan(math.radians(half))
    out = None
    for a in centres:
        w = (cq.Workplane("XY").workplane(offset=z0).polyline([(0, 0), (200, -200 * tt), (200, 200 * tt)]).close()
             .extrude(h).rotate((0, 0, 0), (0, 0, 1), a))
        out = w if out is None else out.union(w)
    return out

z_skirt = Z_RING_TOP - SKIRT_H
top = disc(D, Z_TOP - TOP_T, TOP_T)
shell = disc(D, Z_RING_TOP + FLANGE_T, Z_TOP - TOP_T - Z_RING_TOP - FLANGE_T).cut(
    disc(D - 2 * SHELL_WALL, Z_RING_TOP + FLANGE_T, Z_TOP - TOP_T - Z_RING_TOP - FLANGE_T))
flange = disc(D, Z_RING_TOP, FLANGE_T).cut(disc(2 * SKIRT_IN, Z_RING_TOP, FLANGE_T))
skirt = disc(2 * SKIRT_OUT, z_skirt, SKIRT_H + FLANGE_T).cut(disc(2 * SKIRT_IN, z_skirt, SKIRT_H + FLANGE_T))

helix = cq.Wire.makeHelix(T_PITCH, SKIRT_H + 3 * T_PITCH, T_R0)
prof = cq.Workplane("XZ").polyline([(T_R0, -T_W0 / 2), (T_R1, -T_W1 / 2), (T_R1, T_W1 / 2), (T_R0, T_W0 / 2)]).close()
thread = (prof.sweep(cq.Workplane("XY").newObject([helix]), isFrenet=True)
          .translate((0, 0, z_skirt - T_PITCH / 2))      # half-pitch phase so ridges sit in the ring grooves when seated
          .intersect(disc(2 * T_R1 + 2, z_skirt, SKIRT_H))
          .intersect(sectors((45, 135, 225, 315), LID_SECT, z_skirt, SKIRT_H)))

model = top.union(shell).union(flange).union(skirt).union(thread)
model = model.cut(cq.Workplane("XY").workplane(offset=Z_TOP - TOP_T).polarArray(HOLE_R, 45, 360, 4).circle(HOLE_D / 2).extrude(TOP_T))
model = model.cut(cq.Workplane("XY").workplane(offset=Z_RING_TOP).polarArray(HOLE_R, 45, 360, 4).circle(HOLE_D / 2).extrude(FLANGE_T))
model = model.cut(cq.Workplane("XY").workplane(offset=Z_TOP - TOP_T).rect(SLOT_L, SLOT_W).extrude(TOP_T))

bb = model.val().BoundingBox()
print(f"cap bbox: {bb.xlen:.1f} x {bb.ylen:.1f} x z {bb.zmin:.1f}..{bb.zmax:.1f}, solids {len(model.val().Solids())}")
cq.exporters.export(model, "lid.step")
cq.exporters.export(model, "lid.stl")

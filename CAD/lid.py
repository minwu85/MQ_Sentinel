"""Cap (lid) for the top ring, 4-section quarter-turn thread like a bottle lid (mm).
Run from CAD/: python lid.py -> lid.step/.stl. Shown seated on the housing (flange on the ring top, z=133).
v24: ears lowered to z177.3-183.3 (M5), 4 flange pilot holes, small hanger. v23: top z197 (20 mm above the 172 mm pipe, which sits on platform 0 and ends at z177); all holes filled; no inner support;
4 ears on the diagonals with one M3 hole each (screw to the insert walls); arch hanger (loop) on the top instead of the keyhole."""
import math
import cadquery as cq

D, Z_RING_TOP, Z_TOP = 150.0, 133.0, 197.0   # top = pipe top (z177) + 20 mm
FLANGE_T, TOP_T, SHELL_WALL = 6.0, 6.0, 3.0              # flange on the ring, top disc, shell wall
SKIRT_IN, SKIRT_OUT, SKIRT_H = 55.0, 58.0, 10.0          # 1 cm threaded skirt, goes into the ring bore
T_PITCH, T_R0, T_R1, T_W0, T_W1 = 4.0, 57.9, 59.7, 2.0, 0.6   # external ridge, mates the ring's 60.1 -> 58.5
LID_SECT = 8.0                                           # half-angle of each lid section (passes the ring gaps)
EAR_R0, EAR_R1, EAR_HALF = 73.0, 91.3, 13.0              # 4 ears on the diagonals, lowered to the level just above the pipe top (z177)
EAR_Z0, EAR_T = 177.3, 6.0                               # 0.3 mm over the wall tops (z177)
SCREW_R, SCREW_D, CBORE_D, CBORE_H = 85.3, 5.5, 10.0, 2.0   # one M5 screw per ear into the wall top (bigger holes)
FL_HOLE_R, FL_HOLE_D = 67.5, 5.5                         # v25: 3 through holes in the lid flange at 60/180/300 deg, same as the ring (M5)
HANGER_W, HANGER_T, HANGER_H, PAD_L, PAD_W, PAD_T = 18.0, 8.0, 14.0, 24.0, 10.0, 3.0   # small balanced hanger
HANGER_HOLE_D, HANGER_SLOT_W = 8.0, 4.5                  # flipped keyhole: round at the bottom, narrow slot above

def disc(d, z0, t):
    return cq.Workplane("XY").workplane(offset=z0).circle(d / 2).extrude(t)

def union_all(parts):
    out = parts[0]
    for p in parts[1:]:
        out = out.union(p)
    return out

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

def ear(a):
    t = math.tan(math.radians(EAR_HALF))
    w = (cq.Workplane("XY").workplane(offset=EAR_Z0).polyline([(0, 0), (200, -200 * t), (200, 200 * t)]).close()
         .extrude(EAR_T).rotate((0, 0, 0), (0, 0, 1), a))
    return disc(2 * EAR_R1, EAR_Z0, EAR_T).cut(disc(2 * EAR_R0, EAR_Z0, EAR_T)).intersect(w)
ears = union_all([ear(a) for a in (45, 135, 225, 315)])

# small hanger on the top centre: pad + rounded block (XZ profile, extruded along Y) with a flipped keyhole through it (round at the bottom, narrow slot above)
z0 = Z_TOP + PAD_T
pad = cq.Workplane("XY").workplane(offset=Z_TOP).rect(PAD_L, PAD_W).extrude(PAD_T)
r = HANGER_W / 2
block = (cq.Workplane("XZ").workplane(offset=-HANGER_T / 2).moveTo(-r, z0).lineTo(-r, z0 + HANGER_H - r)
         .threePointArc((0, z0 + HANGER_H), (r, z0 + HANGER_H - r)).lineTo(r, z0).close().extrude(HANGER_T))
hole = cq.Workplane("XZ").workplane(offset=-HANGER_T / 2 - 1).center(0, z0 + HANGER_HOLE_D / 2 + 1.0).circle(HANGER_HOLE_D / 2).extrude(HANGER_T + 2)
slot = (cq.Workplane("XZ").workplane(offset=-HANGER_T / 2 - 1).center(0, z0 + HANGER_HOLE_D / 2 + 1.0 + (HANGER_H - 3.0 - HANGER_HOLE_D) / 2 + 1.0)
        .slot2D(HANGER_H - 3.0 - HANGER_HOLE_D + 2 * HANGER_SLOT_W / 2 + 2.0, HANGER_SLOT_W, 90).extrude(HANGER_T + 2))
hanger = block.cut(hole).cut(slot)
model = top.union(shell).union(flange).union(skirt).union(thread).union(ears).union(pad).union(hanger)
for a in (45, 135, 225, 315):
    pt = (SCREW_R * math.cos(math.radians(a)), SCREW_R * math.sin(math.radians(a)))
    model = model.cut(cq.Workplane("XY").workplane(offset=EAR_Z0).center(*pt).circle(SCREW_D / 2).extrude(EAR_T))
    model = model.cut(cq.Workplane("XY").workplane(offset=EAR_Z0 + EAR_T - CBORE_H).center(*pt).circle(CBORE_D / 2).extrude(CBORE_H))
for a in (60, 180, 300):
    pt = (FL_HOLE_R * math.cos(math.radians(a)), FL_HOLE_R * math.sin(math.radians(a)))
    model = model.cut(cq.Workplane("XY").workplane(offset=Z_RING_TOP).center(*pt).circle(FL_HOLE_D / 2).extrude(FLANGE_T))

bb = model.val().BoundingBox()
print(f"cap bbox: {bb.xlen:.1f} x {bb.ylen:.1f} x z {bb.zmin:.1f}..{bb.zmax:.1f}, solids {len(model.val().Solids())}")
cq.exporters.export(model, "lid.step")
cq.exporters.export(model, "lid.stl")

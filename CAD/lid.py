"""Cap (lid) for the top ring, 4-section quarter-turn thread like a bottle lid (mm).
Run from CAD/: python lid.py -> lid.step/.stl. Shown seated on the housing (flange on the ring top, z=133).
v23: top z197 (20 mm above the 172 mm pipe, which sits on platform 0 and ends at z177); all holes filled; no inner support;
4 ears on the diagonals with one M3 hole each (screw to the insert walls); arch hanger (loop) on the top instead of the keyhole."""
import math
import cadquery as cq

D, Z_RING_TOP, Z_TOP = 150.0, 133.0, 197.0   # top = pipe top (z177) + 20 mm
FLANGE_T, TOP_T, SHELL_WALL = 3.0, 6.0, 3.0              # flange on the ring, top disc, shell wall
SKIRT_IN, SKIRT_OUT, SKIRT_H = 55.0, 58.0, 10.0          # 1 cm threaded skirt, goes into the ring bore
T_PITCH, T_R0, T_R1, T_W0, T_W1 = 4.0, 57.9, 59.7, 2.0, 0.6   # external ridge, mates the ring's 60.1 -> 58.5
LID_SECT = 8.0                                           # half-angle of each lid section (passes the ring gaps)
EAR_R0, EAR_R1, EAR_HALF = 70.0, 91.3, 12.0              # 4 ears on the diagonals (over the insert walls R80.3-90.3, +-9 deg), flush with the lid top
SCREW_R, SCREW_D, CBORE_D, CBORE_H = 85.3, 3.4, 6.5, 2.0 # one M3 screw per ear into the wall top
ARCH_W, ARCH_LEG, ARCH_STRAIGHT, ARCH_T = 32.0, 8.0, 6.0, 10.0   # hanger arch: 32 wide, 8 mm legs, 10 mm thick (Y)
PAD_L, PAD_W, PAD_T = 44.0, 18.0, 3.0                    # base pad under the arch

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
    w = (cq.Workplane("XY").workplane(offset=Z_TOP - TOP_T).polyline([(0, 0), (200, -200 * t), (200, 200 * t)]).close()
         .extrude(TOP_T).rotate((0, 0, 0), (0, 0, 1), a))
    return disc(2 * EAR_R1, Z_TOP - TOP_T, TOP_T).cut(disc(2 * EAR_R0, Z_TOP - TOP_T, TOP_T)).intersect(w)
ears = union_all([ear(a) for a in (45, 135, 225, 315)])

# hanger arch (a loop, no hole in the face): pad + arch profile in XZ, extruded along Y
r_out, r_in = ARCH_W / 2, ARCH_W / 2 - ARCH_LEG
def arch_profile(r, z0):
    return (cq.Workplane("XZ").workplane(offset=-ARCH_T / 2).moveTo(-r, z0).lineTo(-r, z0 + ARCH_STRAIGHT)
            .threePointArc((0, z0 + ARCH_STRAIGHT + r), (r, z0 + ARCH_STRAIGHT)).lineTo(r, z0).close().extrude(ARCH_T))
pad = cq.Workplane("XY").workplane(offset=Z_TOP).rect(PAD_L, PAD_W).extrude(PAD_T)
arch = arch_profile(r_out, Z_TOP + PAD_T).cut(arch_profile(r_in, Z_TOP + PAD_T - 0.01))
model = top.union(shell).union(flange).union(skirt).union(thread).union(ears).union(pad).union(arch)
for a in (45, 135, 225, 315):
    pt = (SCREW_R * math.cos(math.radians(a)), SCREW_R * math.sin(math.radians(a)))
    model = model.cut(cq.Workplane("XY").workplane(offset=Z_TOP - TOP_T).center(*pt).circle(SCREW_D / 2).extrude(TOP_T))
    model = model.cut(cq.Workplane("XY").workplane(offset=Z_TOP - CBORE_H).center(*pt).circle(CBORE_D / 2).extrude(CBORE_H))

bb = model.val().BoundingBox()
print(f"cap bbox: {bb.xlen:.1f} x {bb.ylen:.1f} x z {bb.zmin:.1f}..{bb.zmax:.1f}, solids {len(model.val().Solids())}")
cq.exporters.export(model, "lid.step")
cq.exporters.export(model, "lid.stl")

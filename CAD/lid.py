"""Cap (lid) for the top ring, 4-section quarter-turn thread like a bottle lid (mm).
Run from CAD/: python lid.py -> lid.step/.stl. Shown seated on the housing (flange on the ring top, z=133).
v22: only the cap height (top z177, flush with the top of the 172 mm pipe sitting on platform 0) and a keyhole hanger changed.
Added: keyhole hanger with a thick boss + ribs in the centre, 8 screw holes for the insert walls (their tops end at the lid underside)."""
import math
import cadquery as cq

D, Z_RING_TOP, Z_TOP = 150.0, 133.0, 177.0
FLANGE_T, TOP_T, SHELL_WALL = 3.0, 4.0, 3.0              # flange on the ring, top disc, shell wall
SKIRT_IN, SKIRT_OUT, SKIRT_H = 55.0, 58.0, 10.0          # 1 cm threaded skirt, goes into the ring bore
T_PITCH, T_R0, T_R1, T_W0, T_W1 = 4.0, 57.9, 59.7, 2.0, 0.6   # external ridge, mates the ring's 60.1 -> 58.5
LID_SECT = 8.0                                           # half-angle of each lid section (passes the ring gaps)
HOLE_R, HOLE_D = 67.5, 3.0                               # same 4 holes as the ring
WALL_HOLE_R, WALL_HOLE_ANG = 49.5, 12.0                   # insert-wall screw holes: R49.5, +-12 deg either side of 0/90/180/270
WALL_HOLE_D, WALL_CBORE_D, WALL_CBORE_H = 3.4, 6.5, 2.0   # M3 clearance + counterbore
KEY_ENTRY_D, KEY_SLOT_W, KEY_C = 10.0, 5.5, 7.0           # keyhole: entry dia 10 at y-7, slot 5.5 wide to y+7 (hang by sliding up)
KEY_POCKET_W, KEY_POCKET_H = 10.4, 4.5                    # head pocket behind the 4 mm face (head dia <= 9.5)
BOSS_W, BOSS_L, BOSS_H = 30.0, 60.0, 12.0                 # thick boss under the top disc around the keyhole
RIB_T, RIB_H, RIB_R = 4.0, 10.0, 58.0                     # 4 ribs on the diagonals, boss to R58 (clear of the R67.5 screw holes)

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

boss = (cq.Workplane("XY").workplane(offset=Z_TOP - TOP_T - BOSS_H + TOP_T).slot2D(BOSS_L, BOSS_W, 90).extrude(BOSS_H - TOP_T))
ribs = union_all([cq.Workplane("XY").workplane(offset=Z_TOP - TOP_T - RIB_H).center(RIB_R / 2, 0).rect(RIB_R, RIB_T)
                  .extrude(RIB_H).rotate((0, 0, 0), (0, 0, 1), a) for a in (45, 135, 225, 315)])
model = top.union(shell).union(flange).union(skirt).union(thread).union(boss).union(ribs)
model = model.cut(cq.Workplane("XY").workplane(offset=Z_TOP - TOP_T).polarArray(HOLE_R, 45, 360, 4).circle(HOLE_D / 2).extrude(TOP_T))
model = model.cut(cq.Workplane("XY").workplane(offset=Z_RING_TOP).polarArray(HOLE_R, 45, 360, 4).circle(HOLE_D / 2).extrude(FLANGE_T))
# keyhole (through the 4 mm face) + head pocket behind it in the boss
def key_shape(z0, h, entry_d, slot_w):
    entry = cq.Workplane("XY").workplane(offset=z0).center(0, -KEY_C).circle(entry_d / 2).extrude(h)
    slot = cq.Workplane("XY").workplane(offset=z0).slot2D(2 * KEY_C + slot_w, slot_w, 90).extrude(h)
    return entry.union(slot)
model = model.cut(key_shape(Z_TOP - TOP_T, TOP_T, KEY_ENTRY_D, KEY_SLOT_W))
model = model.cut(key_shape(Z_TOP - TOP_T - KEY_POCKET_H, KEY_POCKET_H, KEY_ENTRY_D, KEY_POCKET_W))
for base in (0, 90, 180, 270):
    for off in (-WALL_HOLE_ANG, WALL_HOLE_ANG):
        pt = (WALL_HOLE_R * math.cos(math.radians(base + off)), WALL_HOLE_R * math.sin(math.radians(base + off)))
        model = model.cut(cq.Workplane("XY").workplane(offset=Z_TOP - TOP_T).center(*pt).circle(WALL_HOLE_D / 2).extrude(TOP_T))
        model = model.cut(cq.Workplane("XY").workplane(offset=Z_TOP - WALL_CBORE_H).center(*pt).circle(WALL_CBORE_D / 2).extrude(WALL_CBORE_H))

bb = model.val().BoundingBox()
print(f"cap bbox: {bb.xlen:.1f} x {bb.ylen:.1f} x z {bb.zmin:.1f}..{bb.zmax:.1f}, solids {len(model.val().Solids())}")
cq.exporters.export(model, "lid.step")
cq.exporters.export(model, "lid.stl")

"""Yoke inserts (platform -1 / -2), v17 form on the v16 frame (encoder_housing.step, unchanged). mm.
Run from CAD/: python inserts.py -> insert_minus1.step, insert_minus2.step
Each yoke: round base plate on platform 1 + two curved side walls (handles) + a top bar. The walls are taller than the
tube (tube top z95, yoke top z100). Plates are big concentric circles so the two yokes stack, join and stand balanced.
Envelope check: max diameter 138 (limit 160), top z100 (limit 172)."""
import math
import cadquery as cq

BASE_TOP = 13.0                                  # platform 1 top (v16)
TUBE_OD, TUBE_Z1 = 44.0, 95.0                    # v16 motor sleeve, top at z95
PLATE_T, HUB_A, HUB_B, WEDGE = 2.0, 100.0, 110.0, 30.0   # circle plates (r50 / r55 stay inside the posts at r60.5)
HOLE_D = 10.0                                    # centre hole (matches platform 0/1)
WALL_R0, WALL_T, WALL_SPAN = 66.0, 3.0, 27.0     # curved side walls, +-27 deg, inside the deck windows (R71)
BAR_W, BAR_T, BAR_OFF = 10.0, 5.0, 28.0          # off-centre bars: tube opening stays clear (bar edge r23 > tube r22)
Z_TOP = TUBE_Z1 + 5.0                            # 100: 5 mm taller than the tube
BAR_Z0 = Z_TOP - BAR_T                           # bars z95-100

def disc(d, z0, t):
    return cq.Workplane("XY").workplane(offset=z0).circle(d / 2).extrude(t)

def wedge(angle, z0, h, half=WEDGE, reach=200.0):
    t = math.tan(math.radians(half))
    return (cq.Workplane("XY").workplane(offset=z0).polyline([(0, 0), (reach, -reach * t), (reach, reach * t)])
            .close().extrude(h).rotate((0, 0, 0), (0, 0, 1), angle))

def union_all(parts):
    out = parts[0]
    for p in parts[1:]:
        out = out.union(p)
    return out

def yoke(axis_deg, hub_d, z_plate):
    plate = disc(hub_d, z_plate, PLATE_T)
    for a in (axis_deg, axis_deg + 180):
        plate = plate.union(wedge(a, z_plate, PLATE_T).intersect(disc(2 * (WALL_R0 + WALL_T), z_plate, PLATE_T)))
    plate = plate.cut(disc(HOLE_D, z_plate, PLATE_T))
    h = Z_TOP - z_plate
    walls = []
    for a in (axis_deg, axis_deg + 180):
        ring = disc(2 * (WALL_R0 + WALL_T), z_plate, h).cut(disc(2 * WALL_R0, z_plate, h))
        walls.append(ring.intersect(wedge(a, z_plate, h, WALL_SPAN)))
    if axis_deg == 0:       # bar A on the +Y side, half-lap notch on top
        bar = cq.Workplane("XY").workplane(offset=BAR_Z0).center(0, BAR_OFF).rect(2 * (WALL_R0 + 1), BAR_W).extrude(BAR_T)
        notch = cq.Workplane("XY").workplane(offset=BAR_Z0 + BAR_T / 2).center(-BAR_OFF, BAR_OFF).rect(BAR_W, BAR_W).extrude(BAR_T / 2)
    else:                   # bar B on the -X side, half-lap notch underneath
        bar = cq.Workplane("XY").workplane(offset=BAR_Z0).center(-BAR_OFF, 0).rect(BAR_W, 2 * (WALL_R0 + 1)).extrude(BAR_T)
        notch = cq.Workplane("XY").workplane(offset=BAR_Z0).center(-BAR_OFF, BAR_OFF).rect(BAR_W, BAR_W).extrude(BAR_T / 2)
    return union_all([plate] + walls + [bar.cut(notch)])

yoke_a = yoke(0, HUB_A, BASE_TOP)                 # platform -1: plate z13-15
yoke_b = yoke(90, HUB_B, BASE_TOP + PLATE_T)      # platform -2: plate z15-17, sits on plate A
cq.exporters.export(yoke_a, "insert_minus1.step")
cq.exporters.export(yoke_b, "insert_minus2.step")
for n, p in (("insert_minus1", yoke_a), ("insert_minus2", yoke_b)):
    b = p.val().BoundingBox()
    print(f"{n}: valid {p.val().isValid()}, solids {len(p.val().Solids())}, z {b.zmin:.1f}..{b.zmax:.1f}, xy {b.xlen:.1f} x {b.ylen:.1f}")

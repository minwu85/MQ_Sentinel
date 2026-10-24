"""Insert (v23, mm) - ONE part (was yoke -1 + yoke -2). Run from CAD/: python inserts.py -> insert.step (run after encoder_housing.py).
A 4 mm plate that fills the underside pocket of platform 0 exactly (centre circle 60 + 4 arms 60 deg wide on the diagonals 45/135/225/315,
arms run out to R90.3) with a 4 flat walls at the arm ends, OUTSIDE the pipe (inner face R80.3, 0.3 mm off the pipe OD 160), 10 mm thick, smooth inside.
The walls are on the diagonals so none blocks the sensor notches at 0/120/240. Wall tops stop 0.3 mm under the lid ears; one M3 pilot hole in each."""
import math
import cadquery as cq

PLATE_T, HUB_D, ARM_HALF, ARM_R = 4.0, 60.0, 30.0, 90.3
HOLE_D = 20.8                                    # centre hole (laser stub: ridge tip R10, hole R10.4; plate R10.4)
WALL_R0, WALL_R1, WALL_HALF = 80.3, 90.3, 9.0    # 10 mm thick, +-9 deg (7 deg clear of the sensor at 120/240 incl. notch)
WALL_TOP = 190.7                                 # lid ear underside (lid top 197 - 6) minus 0.3
PILOT_R, PILOT_D, PILOT_DEPTH = 85.3, 2.6, 15.0
ANGLES = (45, 135, 225, 315)

def disc(d, z0, t):
    return cq.Workplane("XY").workplane(offset=z0).circle(d / 2).extrude(t)

def wedge(angle, z0, h, half, reach=200.0):
    t = math.tan(math.radians(half))
    return (cq.Workplane("XY").workplane(offset=z0).polyline([(0, 0), (reach, -reach * t), (reach, reach * t)])
            .close().extrude(h).rotate((0, 0, 0), (0, 0, 1), angle))

def union_all(parts):
    out = parts[0]
    for p in parts[1:]:
        out = out.union(p)
    return out

def build():
    plate = union_all([disc(HUB_D, 0, PLATE_T)] + [wedge(a, 0, PLATE_T, ARM_HALF) for a in ANGLES]).intersect(disc(2 * ARM_R, 0, PLATE_T))
    plate = plate.cut(disc(HOLE_D, 0, PLATE_T))
    parts = [plate]
    for a in ANGLES:
        h = WALL_TOP
        wall = disc(2 * WALL_R1, 0, h).cut(disc(2 * WALL_R0, 0, h)).intersect(wedge(a, 0, h, WALL_HALF))
        parts.append(wall)
    body = union_all(parts)
    for a in ANGLES:
        x, y = PILOT_R * math.cos(math.radians(a)), PILOT_R * math.sin(math.radians(a))
        body = body.cut(cq.Workplane("XY").workplane(offset=WALL_TOP - PILOT_DEPTH).center(x, y).circle(PILOT_D / 2).extrude(PILOT_DEPTH))
    return body

if __name__ == "__main__":
    ins = build()
    cq.exporters.export(ins, "insert.step")
    b = ins.val().BoundingBox()
    print(f"insert: valid {ins.val().isValid()}, solids {len(ins.val().Solids())}, z {b.zmin:.1f}..{b.zmax:.1f}, xy {b.xlen:.1f} x {b.ylen:.1f}")

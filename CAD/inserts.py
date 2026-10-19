"""Yoke inserts (platform -1 / -2), v17 form on the v16 frame (encoder_housing.step, platform 0 = 5 mm). mm.
Run from CAD/: python inserts.py -> insert_minus1.step, insert_minus2.step (run AFTER encoder_housing.py)
v17 yoke without the bar: round plate on platform 1 + two curved side walls (handles), one 2 x 1 cm hole in each wall. Only change: the side walls are
165 mm high (top at z165) and sit at R54-57, inside the sensor ring bore (R60) so they pass the ring."""
import math
import cadquery as cq

BASE_TOP = 10.0                                  # platform 1 top (platform 0 = 5, platform 1 = 5)
PLATE_T, HUB_A, HUB_B, WEDGE = 2.0, 50.0, 60.0, 30.0     # v17 plates
HOLE_D = 10.0
WALL_R0, WALL_T, WALL_SPAN = 54.0, 3.0, 22.0     # curved walls R54-57, +-22 deg (clear of the deck X arms at 26 deg)
WALL_TOP = 165.0                                 # side height 165
HOLE_W, HOLE_H = 20.0, 10.0                     # 2 x 1 cm hole through each wall
HOLE_Z_A, HOLE_Z_B = 155.0, 140.0                # hole centres: yoke -1 z150-160, yoke -2 z135-145 (no overlap, above the ring top z133)

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

def yoke(axis_deg, hub_d, z_plate, hole_z):
    R = WALL_R0 + WALL_T
    plate = disc(hub_d, z_plate, PLATE_T)
    for a in (axis_deg, axis_deg + 180):
        plate = plate.union(wedge(a, z_plate, PLATE_T).intersect(disc(2 * R, z_plate, PLATE_T)))
    plate = plate.cut(disc(HOLE_D, z_plate, PLATE_T))
    h = WALL_TOP - z_plate
    walls = []
    for a in (axis_deg, axis_deg + 180):
        ring = disc(2 * R, z_plate, h).cut(disc(2 * WALL_R0, z_plate, h))
        walls.append(ring.intersect(wedge(a, z_plate, h, WALL_SPAN)))
    body = union_all([plate] + walls)
    # one 2 x 1 cm (20 wide x 10 tall) hole through each wall, for a block that runs through both walls
    for ang in (axis_deg, axis_deg + 180):
        cutter = (cq.Workplane("XY").workplane(offset=hole_z - HOLE_H / 2).center(WALL_R0 + WALL_T / 2, 0)
                  .rect(WALL_T + 6, HOLE_W).extrude(HOLE_H).rotate((0, 0, 0), (0, 0, 1), ang))
        body = body.cut(cutter)
    return body

yoke_a = yoke(0, HUB_A, BASE_TOP, HOLE_Z_A)                 # platform -1: plate z10-12
yoke_b = yoke(90, HUB_B, BASE_TOP + PLATE_T, HOLE_Z_B)      # platform -2: plate z12-14
cq.exporters.export(yoke_a, "insert_minus1.step")
cq.exporters.export(yoke_b, "insert_minus2.step")
for n, p in (("insert_minus1", yoke_a), ("insert_minus2", yoke_b)):
    b = p.val().BoundingBox()
    print(f"{n}: valid {p.val().isValid()}, solids {len(p.val().Solids())}, z {b.zmin:.1f}..{b.zmax:.1f}, xy {b.xlen:.1f} x {b.ylen:.1f}")

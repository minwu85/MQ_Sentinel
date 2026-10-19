"""Yoke inserts (platform -1 / -2), v17 form on the v16 frame (encoder_housing.step, platform 0 = 5 mm). mm.
Run from CAD/: python inserts.py -> insert_minus1.step, insert_minus2.step (run AFTER encoder_housing.py)
v17 yoke: round plate on platform 1 + two curved side walls (handles) + a top bar. Only change: the side walls are
165 mm high (top at z165) and sit at R54-57, inside the sensor ring bore (R60) so they pass the ring."""
import math
import cadquery as cq

BASE_TOP = 10.0                                  # platform 1 top (platform 0 = 5, platform 1 = 5)
PLATE_T, HUB_A, HUB_B, WEDGE = 2.0, 50.0, 60.0, 30.0     # v17 plates
HOLE_D = 10.0
WALL_R0, WALL_T, WALL_SPAN = 54.0, 3.0, 22.0     # curved walls R54-57, +-22 deg (clear of the deck X arms at 26 deg)
WALL_TOP = 165.0                                 # side height 165
BAR_W, BAR_T, BAR_OFF, BAR_Z0 = 10.0, 5.0, 28.0, 90.0   # v17 bars: z90-95, off-centre so the tube stays open
STUB_X0, STUB_X1, STUB_Y0 = 46.0, 53.0, 15.0     # bar ends turn in to meet the walls

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

def box(x0, x1, y0, y1, z0, h):
    return cq.Workplane("XY").workplane(offset=z0).center((x0 + x1) / 2, (y0 + y1) / 2).rect(abs(x1 - x0), abs(y1 - y0)).extrude(h)

def yoke(axis_deg, hub_d, z_plate):
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
    # bar along the axis at +Y offset (A) with end stubs turning toward the walls; built along x then rotated for B
    half = STUB_X1
    bar = box(-half, half, BAR_OFF - BAR_W / 2, BAR_OFF + BAR_W / 2, BAR_Z0, BAR_T)
    for sx in (-1, 1):
        bar = bar.union(box(sx * STUB_X0, sx * STUB_X1, STUB_Y0, BAR_OFF + BAR_W / 2, BAR_Z0, BAR_T))
    notch_top = axis_deg == 0
    # half-lap at the crossing with the other bar (x = -28, y = 28): A notched on top, B notched underneath
    if notch_top:
        notch = box(-BAR_OFF - BAR_W / 2, -BAR_OFF + BAR_W / 2, BAR_OFF - BAR_W / 2, BAR_OFF + BAR_W / 2, BAR_Z0 + BAR_T / 2, BAR_T / 2)
    else:
        # B bar = A bar rotated +90 deg about z, mirrored so it lies on the -X side: (x, y) -> (-y, x)
        bar = bar.rotate((0, 0, 0), (0, 0, 1), 90)
        notch = box(-BAR_OFF - BAR_W / 2, -BAR_OFF + BAR_W / 2, BAR_OFF - BAR_W / 2, BAR_OFF + BAR_W / 2, BAR_Z0, BAR_T / 2)
    return union_all([plate] + walls + [bar.cut(notch)])

yoke_a = yoke(0, HUB_A, BASE_TOP)                 # platform -1: plate z10-12
yoke_b = yoke(90, HUB_B, BASE_TOP + PLATE_T)      # platform -2: plate z12-14
cq.exporters.export(yoke_a, "insert_minus1.step")
cq.exporters.export(yoke_b, "insert_minus2.step")
for n, p in (("insert_minus1", yoke_a), ("insert_minus2", yoke_b)):
    b = p.val().BoundingBox()
    print(f"{n}: valid {p.val().isValid()}, solids {len(p.val().Solids())}, z {b.zmin:.1f}..{b.zmax:.1f}, xy {b.xlen:.1f} x {b.ylen:.1f}")

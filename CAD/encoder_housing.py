"""Encoder/motor housing v17 (mm). Run from CAD/: python encoder_housing.py
Outputs: encoder_housing.step/.stl (frame), insert_minus1.step, insert_minus2.step (the two yokes), motor_tube.step.
Platform numbering (bottom to top): 0 = 160 mm base layer, 1 = 150 mm base, 2 = middle deck, 3 = top sensor ring."""
import math
import cadquery as cq

D = 150.0            # body diameter
H = 133.0            # housing height = ring top (sensor ring centreline z130 = 40 mm below a 170 mm top)
P0_T, P1_T, P2_T, P3_T = 8.0, 5.0, 5.0, 6.0
BASE_TOP = P0_T + P1_T                        # 13
P2_Z0 = 85.0                                  # middle deck underside
P2_Z1 = P2_Z0 + P2_T                          # 90
P3_Z0 = H - P3_T                              # 127
SKIRT_D = 160.0                               # platform 0 (R80 limit)
POST_W, POST_RAD, POST_R = 10.0, 14.0, 67.5   # rectangular posts, 1 cm wide
ARM2_W, WIN_R = 36.0, 71.0                    # deck X-cross arm width; axis windows to R71 (4 mm rim)
RING_ID = 120.0
SENS_L, SENS_W, N_SENS = 8.0, 7.0, 3          # sensor notch 8 (tangential) x 7 (radial) x ring thickness
CENTRE_HOLE_D = 10.0                          # centre hole through platforms 0/1
POST_ANG = (45, 135, 225, 315)

# motor tube (separate part): hangs from the yoke bars by its top flange
TUBE_BORE, TUBE_WALL = 38.0, 3.0
TUBE_OD = TUBE_BORE + 2 * TUBE_WALL           # 44
TUBE_Z0, TUBE_H = 54.0, 44.0                  # z54 to 98, closed 3 mm bottom
FLANGE_D, FLANGE_T = 56.0, 3.0                # rests on the bars
DECK_HOLE_D = 46.0                            # deck hole: tube passes with 1 mm clearance
WIN_W, WIN_H = 10.0, 20.0                     # wire window 1 x 2 cm, front (-Y)

# yokes (platform -1 along 0/180 deg, platform -2 along 90/270 deg)
PLATE_T, HUB_A, HUB_B, WEDGE = 2.0, 50.0, 60.0, 30.0
WALL_R0, WALL_T, WALL_SPAN = 66.0, 3.0, 27.0  # curved side walls (the "handles"), +-27 deg
BAR_W, BAR_T, BAR_OFF = 10.0, 5.0, 28.0       # top bars, off-centre so the tube stays open
BAR_Z0 = P2_Z1                                # bars sit on the deck top: z90 to 95

def disc(d, z0, t):
    return cq.Workplane("XY").workplane(offset=z0).circle(d / 2).extrude(t)

def rect_at(angle, r, radial, width, z0, h):
    return (cq.Workplane("XY").workplane(offset=z0).center(r, 0).rect(radial, width).extrude(h)
            .rotate((0, 0, 0), (0, 0, 1), angle))

def union_all(parts):
    out = parts[0]
    for p in parts[1:]:
        out = out.union(p)
    return out

def wedge(angle, z0, h, half=WEDGE, reach=200.0):
    t = math.tan(math.radians(half))
    return (cq.Workplane("XY").workplane(offset=z0).polyline([(0, 0), (reach, -reach * t), (reach, reach * t)])
            .close().extrude(h).rotate((0, 0, 0), (0, 0, 1), angle))

# ---------------- frame ----------------
p0 = disc(SKIRT_D, 0, P0_T)
p1 = disc(D, P0_T, P1_T)
lower = union_all([rect_at(a, POST_R, POST_RAD, POST_W, BASE_TOP - 1.0, P2_Z0 - BASE_TOP + 1.0) for a in POST_ANG])
upper = union_all([rect_at(a, POST_R, POST_RAD, POST_W, P2_Z1, P3_Z0 - P2_Z1) for a in POST_ANG])

# platform 2: X cross, axis wedges cut to R71 (4 mm rim), central hole for the tube
x2 = union_all([cq.Workplane("XY").workplane(offset=P2_Z0).rect(300, ARM2_W).extrude(P2_T)
                .rotate((0, 0, 0), (0, 0, 1), a) for a in (45, 135)])
p2 = disc(D, P2_Z0, P2_T).cut(disc(WIN_R * 2, P2_Z0, P2_T).cut(x2)).cut(disc(DECK_HOLE_D, P2_Z0, P2_T))

# platform 3: complete sensor ring, inside flat (no thread), open top
p3 = disc(D, P3_Z0, P3_T).cut(disc(RING_ID, P3_Z0, P3_T))
p3 = p3.cut(cq.Workplane("XY").workplane(offset=P3_Z0).polarArray(67.5, 45, 360, 4).circle(1.5).extrude(P3_T))
for i in range(N_SENS):
    p3 = p3.cut(cq.Workplane("XY").workplane(offset=P3_Z0).center(D / 2 - SENS_W / 2, 0).rect(SENS_W, SENS_L)
                .extrude(P3_T).rotate((0, 0, 0), (0, 0, 1), 360.0 / N_SENS * i))

model = union_all([p0, p1, lower, p2, upper, p3])
model = model.cut(cq.Workplane("XY").circle(CENTRE_HOLE_D / 2).extrude(BASE_TOP))
bb = model.val().BoundingBox()
print(f"frame bbox: {bb.xlen:.1f} x {bb.ylen:.1f} x {bb.zlen:.1f} mm, solids {len(model.val().Solids())}")
cq.exporters.export(model, "encoder_housing.step")
cq.exporters.export(model, "encoder_housing.stl")

# ---------------- yokes (image 2: plate + curved side walls + top bar) ----------------
def yoke(axis_deg, hub_d, z_plate):
    plate = disc(hub_d, z_plate, PLATE_T)
    for a in (axis_deg, axis_deg + 180):
        arm = wedge(a, z_plate, PLATE_T).intersect(disc(2 * (WALL_R0 + WALL_T), z_plate, PLATE_T))
        plate = plate.union(arm)
    plate = plate.cut(disc(CENTRE_HOLE_D, z_plate, PLATE_T))
    z_top = BAR_Z0 + BAR_T
    walls = []
    for a in (axis_deg, axis_deg + 180):
        ring = disc(2 * (WALL_R0 + WALL_T), z_plate, z_top - z_plate).cut(disc(2 * WALL_R0, z_plate, z_top - z_plate))
        walls.append(ring.intersect(wedge(a, z_plate, z_top - z_plate, WALL_SPAN)))
    # bar along the yoke axis, off-centre by BAR_OFF (A: +Y side, B: -X side); half-lap at the crossing
    if axis_deg == 0:
        bar = cq.Workplane("XY").workplane(offset=BAR_Z0).center(0, BAR_OFF).rect(2 * (WALL_R0 + 1), BAR_W).extrude(BAR_T)
        notch = cq.Workplane("XY").workplane(offset=BAR_Z0 + BAR_T / 2).center(-BAR_OFF, BAR_OFF).rect(BAR_W, BAR_W).extrude(BAR_T / 2)
    else:
        bar = cq.Workplane("XY").workplane(offset=BAR_Z0).center(-BAR_OFF, 0).rect(BAR_W, 2 * (WALL_R0 + 1)).extrude(BAR_T)
        notch = cq.Workplane("XY").workplane(offset=BAR_Z0).center(-BAR_OFF, BAR_OFF).rect(BAR_W, BAR_W).extrude(BAR_T / 2)
    return union_all([plate] + walls + [bar.cut(notch)])

yoke_a = yoke(0, HUB_A, BASE_TOP)               # platform -1: plate z13-15
yoke_b = yoke(90, HUB_B, BASE_TOP + PLATE_T)    # platform -2: plate z15-17
cq.exporters.export(yoke_a, "insert_minus1.step")
cq.exporters.export(yoke_b, "insert_minus2.step")

# ---------------- motor tube (hangs from the bars by its flange) ----------------
tube = disc(TUBE_OD, TUBE_Z0, TUBE_H).union(disc(FLANGE_D, TUBE_Z0 + TUBE_H - FLANGE_T, FLANGE_T))
tube = tube.cut(disc(TUBE_BORE, TUBE_Z0 + TUBE_WALL, TUBE_H - TUBE_WALL))
tube = tube.cut(cq.Workplane("XY").workplane(offset=TUBE_Z0 + TUBE_WALL).center(0, -(TUBE_BORE / 2 + TUBE_WALL / 2))
                .rect(WIN_W, TUBE_WALL + 2).extrude(WIN_H))
cq.exporters.export(tube, "motor_tube.step")

for name, part in (("yoke -1", yoke_a), ("yoke -2", yoke_b), ("tube", tube)):
    b = part.val().BoundingBox()
    print(f"{name}: valid {part.val().isValid()}, solids {len(part.val().Solids())}, z {b.zmin:.1f}..{b.zmax:.1f}")

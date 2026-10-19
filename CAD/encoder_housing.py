"""Encoder/motor housing v12 (mm). Run from CAD/: python encoder_housing.py -> encoder_housing.step/.stl
Platform numbering (bottom to top): 1 = base, 2 = middle deck, 3 = top sensor ring."""
import math
import cadquery as cq

D = 150.0            # overall diameter
H = 135.0            # overall height
P1_T, P2_T, P3_T = 5.0, 5.0, 6.0
P2_Z0 = 75.0                 # platform 2 underside
P2_Z1 = P2_Z0 + P2_T         # 80
P3_Z0 = H - P3_T             # 129
POST_W, POST_RAD, POST_R = 10.0, 14.0, 67.5   # rectangular posts, 1 cm wide, under the ring
ARM2_W, WIN_R = 36.0, 72.0                    # platform 2 X-cross arm width; wedges run out to a 3 mm rim
ARM3_W, SHAFT_HOLE_D = 30.0, 12.0             # platform 3 plus-cross inside the ring, shaft hole
RING_ID = 120.0
SENS_L, SENS_W, N_SENS = 8.0, 7.0, 3          # sensor notch 8 (tangential) x 7 (radial) x ring thickness
SKIRT_D, SKIRT_T = 160.0, 1.0                 # thin bottom layer (R80 limit)
CENTRE_HOLE_D = 10.0                          # centre hole in platform 1
P1_HUB_D, P1_REC, P1_WEDGE = 50.0, 2.0, 30.0  # platform 1: 5 cm circle + two 60 deg wedges recessed 2 mm

# motor sleeve: full-round tube, grey body half below / half above platform 2, closed bottom
MOTOR_D, BORE_D, WALL = 37.0, 38.0, 3.0
ENC_H, GREY_H = 12.0, 53.0                    # encoder 12; grey = 29 can + 24 gearbox (datasheet)
GREY_Z0 = (P2_Z0 + P2_T / 2) - GREY_H / 2     # grey centred on platform 2 mid-plane -> 51 to 104
SLEEVE_Z0 = GREY_Z0 - ENC_H - WALL            # 36, covered bottom
SLEEVE_Z1 = GREY_Z0 + GREY_H + 2.0            # 106
WIN_W, WIN_H = 10.0, 20.0                     # wire window 1 x 2 cm, front (-Y), at the encoder

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

POST_ANG = (45, 135, 225, 315)                # 4 posts below and 4 above platform 2, on the X arms
P1_FLOOR = P1_T - P1_REC
lower = union_all([rect_at(a, POST_R, POST_RAD, POST_W, P1_FLOOR, P2_Z0 - P1_FLOOR) for a in POST_ANG])
upper = union_all([rect_at(a, POST_R, POST_RAD, POST_W, P2_Z1, P3_Z0 - P2_Z1) for a in POST_ANG])

# platform 1: 5 mm. Centre circle + two opposite 60 deg wedges (90/270 deg) go 2 mm in;
# the two 120 deg sectors (0/180 deg) stay full height -> 6 equal 60 deg divisions
t = math.tan(math.radians(P1_WEDGE))
recess = disc(P1_HUB_D, P1_FLOOR, P1_REC + 1)
for a in (90, 270):
    recess = recess.union(cq.Workplane("XY").workplane(offset=P1_FLOOR)
                          .polyline([(0, 0), (200, -200 * t), (200, 200 * t)]).close().extrude(P1_REC + 1)
                          .rotate((0, 0, 0), (0, 0, 1), a))
p1 = disc(D, 0, P1_T).cut(recess)

# platform 2 (middle deck): X cross; the four axis wedges are cut away, 3 mm rim kept
x2 = union_all([cq.Workplane("XY").workplane(offset=P2_Z0).rect(300, ARM2_W).extrude(P2_T)
                .rotate((0, 0, 0), (0, 0, 1), a) for a in (45, 135)])
p2 = disc(D, P2_Z0, P2_T).cut(disc(WIN_R * 2, P2_Z0, P2_T).cut(x2))

# platform 3 (top sensor ring, never removed): full ring + plus cross inside it, corner windows open
ring = disc(D, P3_Z0, P3_T).cut(disc(RING_ID, P3_Z0, P3_T))
plus = (cq.Workplane("XY").workplane(offset=P3_Z0).rect(ARM3_W, RING_ID + 4).extrude(P3_T)
        .union(cq.Workplane("XY").workplane(offset=P3_Z0).rect(RING_ID + 4, ARM3_W).extrude(P3_T)))
p3 = ring.union(plus).cut(disc(SHAFT_HOLE_D, P3_Z0, P3_T))
p3 = p3.cut(cq.Workplane("XY").workplane(offset=P3_Z0).polarArray(67.5, 45, 360, 4).circle(1.5).extrude(P3_T))
for i in range(N_SENS):
    p3 = p3.cut(cq.Workplane("XY").workplane(offset=P3_Z0).center(D / 2 - SENS_W / 2, 0).rect(SENS_W, SENS_L)
                .extrude(P3_T).rotate((0, 0, 0), (0, 0, 1), 360.0 / N_SENS * i))

# motor sleeve: full tube through platform 2, covered bottom, 1 x 2 cm wire window at the side
sleeve = disc(BORE_D + 2 * WALL, SLEEVE_Z0, SLEEVE_Z1 - SLEEVE_Z0)

model = union_all([p1, lower, p2, upper, p3, sleeve])
model = model.cut(disc(BORE_D, SLEEVE_Z0 + WALL, SLEEVE_Z1 - SLEEVE_Z0 - WALL))
model = model.cut(cq.Workplane("XY").workplane(offset=SLEEVE_Z0 + WALL).center(0, -(BORE_D / 2 + WALL / 2))
                  .rect(WIN_W, WALL + 2).extrude(WIN_H))
model = model.union(disc(SKIRT_D, 0, SKIRT_T))
model = model.cut(cq.Workplane("XY").circle(CENTRE_HOLE_D / 2).extrude(P1_T))

bb = model.val().BoundingBox()
print(f"bbox: {bb.xlen:.1f} x {bb.ylen:.1f} x {bb.zlen:.1f} mm, solids {len(model.val().Solids())}")
cq.exporters.export(model, "encoder_housing.step")
cq.exporters.export(model, "encoder_housing.stl")

"""Encoder/motor housing (mm). Run: python encoder_housing.py -> encoder_housing.step/.stl"""
import math
import cadquery as cq

D = 150.0            # overall diameter
H = 135.0            # overall height
P1_T, GAP1, P2_T, P3_T = 5.0, 70.0, 5.0, 6.0
P1_Z0 = 0.0
P2_Z0 = P1_T + GAP1          # 75
P2_Z1 = P2_Z0 + P2_T         # 85
P3_Z0 = H - P3_T             # 130
POST_D, POST_R = 14.0, 67.5   # posts sit outside the lid thread (r<=60)
RING_ID = 120.0
BORE_D = 40.0                # semicircle radius 2 cm
CLAMP_Z0, CLAMP_H = 80.0, 45.0     # 4.5 cm box on platform 2, top z=125 (under ring at 129)
CLAMP_W, CLAMP_L = 60.0, 30.0       # 6 x 3 cm box
INSET_H, INSET = 25.0, 1.0          # top 2.5 cm stepped in 1 mm
BORE_PLAIN_H, GROOVE_H, GROOVE_IN = 30.0, 15.0, 1.0   # plain R20 for 3 cm, then 2 cm groove 1 mm wider in the wall
HOLE_D, HOLE_DEPTH = 3.0, 45.0      # vertical holes in the top face of both arms
HOLE_X, HOLE_Y = 24.5, (8.0, 22.0)  # 2 per arm
ARM_W, WIN_R = 40.0, 56.0           # platform 2 cross: equal 40 mm arms, larger corner windows
SENS_L, SENS_W, N_SENS = 8.0, 7.0, 6   # sensor notch 8 (tangential) x 7 (radial) x ring thickness
# lid thread (internal ridge on the ring, external ridge on lid/lid.py)
T_PITCH, T_RING_R0, T_RING_R1, T_W0, T_W1 = 4.0, 60.1, 58.5, 2.0, 0.6
SKIRT_D, SKIRT_T = 160.0, 1.0   # thin 1 mm layer (R80 limit)
CENTRE_HOLE_D = 10.0         # centre hole in platform 1
SLOT_W, SLOT_L = 20.0, 10.0  # 2 x 1 cm slot in platform 2

def disc(d, z0, t):
    return cq.Workplane("XY").workplane(offset=z0).circle(d / 2).extrude(t)

def ridge(r0, r1, pitch, height, z0):
    """Helical trapezoid ridge, base radius r0 -> tip radius r1, starting at z0."""
    helix = cq.Wire.makeHelix(pitch, height, r0)
    path = cq.Workplane("XY").newObject([helix])
    prof = cq.Workplane("XZ").polyline([(r0, -T_W0 / 2), (r1, -T_W1 / 2), (r1, T_W1 / 2), (r0, T_W0 / 2)]).close()
    return prof.sweep(path, isFrenet=True).translate((0, 0, z0))

def posts(n, z0, h, start_deg):
    pts = [(POST_R * math.cos(math.radians(start_deg + i * 360 / n)),
            POST_R * math.sin(math.radians(start_deg + i * 360 / n))) for i in range(n)]
    return cq.Workplane("XY").workplane(offset=z0).pushPoints(pts).circle(POST_D / 2).extrude(h)

p1 = disc(D, P1_Z0, P1_T)
p2 = disc(D, P2_Z0, P2_T)
p3 = disc(D, P3_Z0, P3_T).cut(cq.Workplane("XY").workplane(offset=P3_Z0).circle(RING_ID / 2).extrude(P3_T))

lower = posts(3, P1_T, GAP1, 90)
upper = posts(4, P2_Z1, P3_Z0 - P2_Z1, 45)

p2 = disc(D, P2_Z0, P2_T).cut(
    cq.Workplane("XY").workplane(offset=P2_Z0).circle(WIN_R).extrude(P2_T).cut(
        cq.Workplane("XY").workplane(offset=P2_Z0).rect(ARM_W, 200).extrude(P2_T)).cut(
        cq.Workplane("XY").workplane(offset=P2_Z0).rect(200, ARM_W).extrude(P2_T)))

low_h = CLAMP_H - INSET_H
clamp = (cq.Workplane("XY").workplane(offset=CLAMP_Z0).center(0, CLAMP_L / 2).rect(CLAMP_W, CLAMP_L).extrude(low_h)
         .union(cq.Workplane("XY").workplane(offset=CLAMP_Z0 + low_h).center(0, (CLAMP_L - INSET) / 2)
                .rect(CLAMP_W - 2 * INSET, CLAMP_L - INSET).extrude(INSET_H)))

model = p1.union(lower).union(p2).union(upper).union(p3).union(clamp)
z0 = CLAMP_Z0
for r, h, dz in ((BORE_D / 2, BORE_PLAIN_H, 0.0),
                 (BORE_D / 2 + GROOVE_IN, GROOVE_H, BORE_PLAIN_H),
                 (BORE_D / 2, CLAMP_H - BORE_PLAIN_H - GROOVE_H, BORE_PLAIN_H + GROOVE_H)):
    if h > 0:
        model = model.cut(cq.Workplane("XY").workplane(offset=z0 + dz).circle(r).extrude(h))
for sx in (-1, 1):
    for y in HOLE_Y:
        model = model.cut(cq.Workplane("XY").workplane(offset=CLAMP_Z0 + CLAMP_H - HOLE_DEPTH)
                          .center(sx * HOLE_X, y).circle(HOLE_D / 2).extrude(HOLE_DEPTH))
for i in range(N_SENS):
    a = 360.0 / N_SENS * i
    model = model.cut(cq.Workplane("XY").workplane(offset=P3_Z0)
                      .center(D / 2 - SENS_W / 2, 0).rect(SENS_W, SENS_L).extrude(P3_T)
                      .rotate((0, 0, 0), (0, 0, 1), a))
thread = ridge(T_RING_R0, T_RING_R1, T_PITCH, 3 * T_PITCH, P3_Z0 - T_PITCH).intersect(
    cq.Workplane("XY").workplane(offset=P3_Z0).circle(T_RING_R0 + 1).extrude(P3_T))
model = model.union(thread)
holes = (cq.Workplane("XY").workplane(offset=P3_Z0)
         .polarArray(67.5, 45, 360, 4).circle(1.5).extrude(P3_T))
model = model.cut(holes)
model = model.union(disc(SKIRT_D, 0, SKIRT_T))
model = model.cut(cq.Workplane("XY").circle(CENTRE_HOLE_D / 2).extrude(P1_T))
model = model.cut(cq.Workplane("XY").workplane(offset=P2_Z0).center(0, -25).rect(SLOT_W, SLOT_L).extrude(P2_T))

bb = model.val().BoundingBox()
print(f"bbox: {bb.xlen:.1f} x {bb.ylen:.1f} x {bb.zlen:.1f} mm")
cq.exporters.export(model, "encoder_housing.step")
cq.exporters.export(model, "encoder_housing.stl")

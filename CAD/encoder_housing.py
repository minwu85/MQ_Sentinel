"""Encoder/motor housing (mm). Run: python CAD/encoder_housing.py -> CAD/encoder_housing.step/.stl"""
import math
import cadquery as cq

D = 150.0            # overall diameter
H = 135.0            # overall height
P1_T, GAP1, P2_T, P3_T = 5.0, 70.0, 10.0, 5.0
P1_Z0 = 0.0
P2_Z0 = P1_T + GAP1          # 75
P2_Z1 = P2_Z0 + P2_T         # 85
P3_Z0 = H - P3_T             # 130
POST_D, POST_R = 15.0, 62.0
RING_ID = 120.0
BORE_D = 39.0                # motor clearance (3.9 cm)
CLAMP_H = 45.0 

def disc(d, z0, t):
    return cq.Workplane("XY").workplane(offset=z0).circle(d / 2).extrude(t)

def posts(n, z0, h, start_deg):
    pts = [(POST_R * math.cos(math.radians(start_deg + i * 360 / n)),
            POST_R * math.sin(math.radians(start_deg + i * 360 / n))) for i in range(n)]
    return cq.Workplane("XY").workplane(offset=z0).pushPoints(pts).circle(POST_D / 2).extrude(h)

p1 = disc(D, P1_Z0, P1_T)
p2 = disc(D, P2_Z0, P2_T)
p3 = disc(D, P3_Z0, P3_T).cut(cq.Workplane("XY").workplane(offset=P3_Z0).circle(RING_ID / 2).extrude(P3_T))

lower = posts(3, P1_T, GAP1, 90)
upper = posts(4, P2_Z1, P3_Z0 - P2_Z1, 45)

clamp = (cq.Workplane("XY").workplane(offset=P2_Z1)
         .center(0, 22.5).rect(60, 45).extrude(CLAMP_H)
         .cut(cq.Workplane("XY").workplane(offset=P2_Z1).circle(BORE_D / 2).extrude(CLAMP_H)))

model = p1.union(lower).union(p2).union(upper).union(p3).union(clamp)
holes = (cq.Workplane("XY").workplane(offset=P3_Z0)
         .polarArray(67.5, 45, 360, 4).circle(1.5).extrude(P3_T))
model = model.cut(holes)

bb = model.val().BoundingBox()
print(f"bbox: {bb.xlen:.1f} x {bb.ylen:.1f} x {bb.zlen:.1f} mm")
cq.exporters.export(model, "CAD/encoder_housing.step")
cq.exporters.export(model, "CAD/encoder_housing.stl")

"""Encoder/motor housing v25 (mm). Run from CAD/: python encoder_housing.py -> encoder_housing.step/.stl
Platform numbering (bottom to top): 1 = base, 2 = middle deck, 3 = top sensor ring."""
import math
import cadquery as cq

D = 150.0            # overall diameter
H = 133.0            # housing height = ring top. Sensor ring centreline z130 = 40 mm below the 170 mm top (Appendix 1); the cap adds 37 mm
P0_T, P1_T, P2_T, P3_T = 5.0, 5.0, 5.0, 6.0    # platform 0 (160 mm) now 5 mm, platform 1 (150 mm) 5 mm on top of it
BASE_TOP = P0_T + P1_T                        # 10
P2_Z0 = 85.0                 # platform 2 underside
P2_Z1 = P2_Z0 + P2_T         # 80
P3_Z0 = H - P3_T             # 127
POST_W, POST_R0, POST_R1 = 20.0, 55.0, 75.0    # v22 posts 2 x 2 cm (tangential x radial, R55-75, trimmed to the 150 mm body)
POST_STEP_Z = 5.0                              # the last 5 mm under the ring only reach R60 (lid skirt R55-58 sits there)
RIM_FLANGE_R, RIM_FLANGE_H = 69.0, 5.0         # v22 stiffening flange under the deck rim (R69-75)
ARM2_W, WIN_R = 36.0, 60.0                    # platform 2 X-cross arm width; v22 wedges run out to R60 = 15 mm rim (was 4 mm)
RING_ID = 120.0
# interrupted (quarter-turn) thread for the cap: 4 internal ridge sections centred on 45/135/225/315 deg
T_PITCH, T_R0, T_R1, T_W0, T_W1 = 4.0, 60.1, 58.5, 2.0, 0.6
T_SECT = 35.0                                 # half-angle of each ring thread section (gaps at 0/90/180/270 deg)
SENS_L, SENS_W, N_SENS = 8.0, 7.0, 3          # sensor notch 8 (tangential) x 7 (radial) x ring thickness
SKIRT_D = 160.0                               # platform 0: 160 mm (R80 limit)
# v23 laser socket: threaded centre hole (female thread z4-10) the laser elbow (laser.py) screws into from below.
# Thread constants must match laser.py: pitch 2.5, male core R8 / tip R10, female root R10.4 / tip R8.4, widths 1.4 / 0.5
CENTRE_HOLE_D = 20.8
L_PITCH, L_ROOT, L_TIP, L_W0, L_W1, L_Z0 = 2.5, 10.4, 8.4, 1.4, 0.5, 4.0
L_PHASE = 0.1                                 # helix start offset from the fit test: free window -0.65..+0.15; 0.1 puts the loaded flank 0.1 mm off contact so the laser seats at 0 deg +-15 deg
INS_D, P1_WEDGE = 60.0, 30.0                   # v23 one insert plate: 6 cm centre circle + 4 wedges 60 deg wide (half-angle 30) at 45/135/225/315

# motor sleeve: full-round tube, grey body half below / half above platform 2, closed bottom
MOTOR_D, BORE_D, WALL = 37.0, 38.0, 3.0
COLLAR_D, COLLAR_Z0 = BORE_D - 2.0, 85.0       # v22 collar bore 36 (radius 1 mm smaller) from the deck up; below it the bore stays 38
ENC_H, GREY_H = 12.0, 53.0                    # encoder 12; grey = 29 can + 24 gearbox (datasheet)
SLEEVE_Z0 = 51.0                              # tube shifted up 5 mm (same size); covered bottom
SLEEVE_Z1 = SLEEVE_Z0 + 46.0                             # 97: v22 collar 2 mm taller (was 95)
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
P1_STAND = BASE_TOP - 1.0                     # lower posts start 1 mm inside platform 1
def post(a, r0, r1, z0, h):
    return (rect_at(a, (r0 + r1) / 2, r1 - r0, POST_W, z0, h)).intersect(disc(D, z0, h))
lower = union_all([post(a, POST_R0, POST_R1, P1_STAND, P2_Z0 - P1_STAND) for a in POST_ANG])
upper = union_all([post(a, RING_ID / 2, POST_R1, P2_Z1, P3_Z0 - P2_Z1) for a in POST_ANG])   # v24: R60-75 only, clear of the flywheel

# platform 0 (160 x 10) + platform 1 (150 x 5 on top). Platform 1 top is flat, only the 10 mm centre hole.
# Underside pocket for the single insert plate (insert.step, 4 mm): centre circle + 4 wedges on the diagonals (clear of the 0/120/240 sensors)
t = math.tan(math.radians(P1_WEDGE))
def wedges(angles, z0, h):
    out = None
    for a in angles:
        w = (cq.Workplane("XY").workplane(offset=z0).polyline([(0, 0), (200, -200 * t), (200, 200 * t)]).close()
             .extrude(h).rotate((0, 0, 0), (0, 0, 1), a))
        out = w if out is None else out.union(w)
    return out
recess = disc(INS_D, 0, 4.0).union(wedges((45, 135, 225, 315), 0, 4.0))
p1 = disc(SKIRT_D, 0, P0_T).union(disc(D, P0_T, P1_T))

# platform 2 (middle deck): X cross; the four axis wedges are cut away, 3 mm rim kept
x2 = union_all([cq.Workplane("XY").workplane(offset=P2_Z0).rect(300, ARM2_W).extrude(P2_T)
                .rotate((0, 0, 0), (0, 0, 1), a) for a in (45, 135)])
p2 = disc(D, P2_Z0, P2_T).cut(disc(WIN_R * 2, P2_Z0, P2_T).cut(x2))
rim_flange = disc(D, P2_Z0 - RIM_FLANGE_H, RIM_FLANGE_H).cut(disc(2 * RIM_FLANGE_R, P2_Z0 - RIM_FLANGE_H, RIM_FLANGE_H))

# platform 3 (top sensor ring, never removed): ring only, open top so the motor can be dropped in from above
p3 = disc(D, P3_Z0, P3_T).cut(disc(RING_ID, P3_Z0, P3_T))
RING_HOLE_R, RING_HOLE_D, RING_HOLES = 67.5, 5.5, 3   # v25: 3 equal M5 holes at 60/180/300 deg, 60 deg from the sensors (0/120/240), clear of the thread ridge (R60.1)
p3 = p3.cut(cq.Workplane("XY").workplane(offset=P3_Z0).polarArray(RING_HOLE_R, 60, 360, RING_HOLES).circle(RING_HOLE_D / 2).extrude(P3_T))
for i in range(N_SENS):
    p3 = p3.cut(cq.Workplane("XY").workplane(offset=P3_Z0).center(D / 2 - SENS_W / 2, 0).rect(SENS_W, SENS_L)
                .extrude(P3_T).rotate((0, 0, 0), (0, 0, 1), 360.0 / N_SENS * i))

def ridge(r0, r1, pitch, height, z0):
    helix = cq.Wire.makeHelix(pitch, height, r0)
    prof = cq.Workplane("XZ").polyline([(r0, -T_W0 / 2), (r1, -T_W1 / 2), (r1, T_W1 / 2), (r0, T_W0 / 2)]).close()
    return prof.sweep(cq.Workplane("XY").newObject([helix]), isFrenet=True).translate((0, 0, z0))

def sectors(centres, half, z0, h):
    tt = math.tan(math.radians(half))
    return union_all([cq.Workplane("XY").workplane(offset=z0).polyline([(0, 0), (200, -200 * tt), (200, 200 * tt)])
                      .close().extrude(h).rotate((0, 0, 0), (0, 0, 1), a) for a in centres])

thread = (ridge(T_R0, T_R1, T_PITCH, 3 * T_PITCH, P3_Z0 - T_PITCH)
          .intersect(disc(T_R0 * 2 + 2, P3_Z0, P3_T)).intersect(sectors((45, 135, 225, 315), T_SECT, P3_Z0, P3_T)))
p3 = p3.union(thread)

# motor sleeve: full tube through platform 2, covered bottom, 1 x 2 cm wire window at the side
sleeve = disc(BORE_D + 2 * WALL, SLEEVE_Z0, SLEEVE_Z1 - SLEEVE_Z0)

model = union_all([p1, lower, p2, rim_flange, upper, p3, sleeve])
model = model.cut(disc(BORE_D, SLEEVE_Z0 + WALL, COLLAR_Z0 - SLEEVE_Z0 - WALL))
model = model.cut(disc(COLLAR_D, COLLAR_Z0, SLEEVE_Z1 - COLLAR_Z0))
model = model.cut(cq.Workplane("XY").workplane(offset=SLEEVE_Z0 + WALL).center(0, -(BORE_D / 2 + WALL / 2))
                  .rect(WIN_W, WALL + 2).extrude(WIN_H))
model = model.cut(recess)
model = model.cut(cq.Workplane("XY").circle(CENTRE_HOLE_D / 2).extrude(BASE_TOP))
helix = cq.Wire.makeHelix(L_PITCH, BASE_TOP - L_Z0 + 2 * L_PITCH, L_ROOT + 0.3)
prof = cq.Workplane("XZ").polyline([(L_ROOT + 0.3, -L_W0 / 2), (L_TIP, -L_W1 / 2), (L_TIP, L_W1 / 2), (L_ROOT + 0.3, L_W0 / 2)]).close()
l_thread = (prof.sweep(cq.Workplane("XY").newObject([helix]), isFrenet=True).translate((0, 0, L_Z0 + L_PHASE - L_PITCH))
            .intersect(disc(2 * (L_ROOT + 0.5), L_Z0, BASE_TOP - L_Z0)))
model = model.union(l_thread)

bb = model.val().BoundingBox()
print(f"bbox: {bb.xlen:.1f} x {bb.ylen:.1f} x {bb.zlen:.1f} mm, solids {len(model.val().Solids())}")
cq.exporters.export(model, "encoder_housing.step")
cq.exporters.export(model, "encoder_housing.stl")

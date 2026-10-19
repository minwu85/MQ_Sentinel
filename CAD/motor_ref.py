"""Reference bodies (not for printing): FIT0186 motor + hub + flywheel, for fit/interference checks.
Run: python motor_ref.py -> motor_ref.step. Z positions are in the housing frame."""
import cadquery as cq

MOTOR_Z0 = 33.0                       # bottom of black encoder; 91 mm overall -> shaft tip z=124
ENC_D, ENC_H = 36.0, 15.0             # black encoder cap
BODY_D, BODY_H = 35.0, 54.0           # grey gearbox/motor body (half = 27 mm sits under platform 2)
SHAFT_D, HUB_D = 6.0, 12.0
FLY_D, FLY_T, FLY_Z0 = 100.0, 10.0, 113.0   # flywheel top 123, clear of the lid skirt (125)

def cyl(d, z0, h):
    return cq.Workplane("XY").workplane(offset=z0).circle(d / 2).extrude(h)

enc = cyl(ENC_D, MOTOR_Z0, ENC_H)
body = cyl(BODY_D, MOTOR_Z0 + ENC_H, BODY_H)
shaft_top = MOTOR_Z0 + 91.0
shaft = cyl(SHAFT_D, MOTOR_Z0 + ENC_H + BODY_H, shaft_top - (MOTOR_Z0 + ENC_H + BODY_H))
hub = cyl(HUB_D, FLY_Z0 - 1.0, FLY_T + 1.0)
fly = cyl(FLY_D, FLY_Z0, FLY_T)
model = enc.union(body).union(shaft).union(hub).union(fly)
cq.exporters.export(model, "motor_ref.step")

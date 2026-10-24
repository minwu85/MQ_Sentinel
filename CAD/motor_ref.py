"""Reference bodies (not for printing): FIT0186 motor + flywheel, for fit/interference checks.
Run from CAD/: python motor_ref.py -> reference/motor_ref.step. Z positions are in the housing frame. Dimensions from the datasheet drawing."""
import cadquery as cq

Z_BASE = 54.0                          # motor bottom sits on the tube floor (tube z51, 3 mm floor)
ENC_D, ENC_H = 36.0, 12.0              # encoder cap (measured 3.6 cm)
GREY_D, GREY_H = 35.0, 57.0            # grey body (measured 3.5 cm); encoder + grey = 69 mm body length
BOSS_D, BOSS_H = 12.0, 6.0             # first step at 75 mm from the base
SHAFT_D, SHAFT_H = 6.0, 16.0           # overall 91 mm
FLY_D, FLY_T = 100.0, 10.0

z_grey = Z_BASE + ENC_H
z_boss = z_grey + GREY_H
z_shaft = z_boss + BOSS_H
fly_z0 = z_shaft                        # flywheel on the shaft above the boss

def cyl(d, z0, h):
    return cq.Workplane("XY").workplane(offset=z0).circle(d / 2).extrude(h)

model = (cyl(ENC_D, Z_BASE, ENC_H).union(cyl(GREY_D, z_grey, GREY_H))
         .union(cyl(BOSS_D, z_boss, BOSS_H)).union(cyl(SHAFT_D, z_shaft, SHAFT_H))
         .union(cyl(FLY_D, fly_z0, FLY_T)))
print(f"motor z {Z_BASE:.1f} to {z_shaft + SHAFT_H:.1f}, flywheel z {fly_z0:.1f} to {fly_z0 + FLY_T:.1f}")
cq.exporters.export(model, "reference/motor_ref.step")

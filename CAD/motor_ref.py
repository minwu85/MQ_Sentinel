"""Reference bodies (not for printing): FIT0186 motor + flywheel, for fit/interference checks.
Run: python motor_ref.py -> motor_ref.step. Z positions are in the housing frame. Dimensions from the datasheet drawing."""
import cadquery as cq

P2_MID = 77.5                          # platform 2 mid-plane
ENC_D, ENC_H = 33.0, 12.0              # encoder (black)
CAN_D, CAN_H = 33.0, 29.0              # motor can (grey)
GBX_D, GBX_H = 37.0, 24.0              # gearbox (grey)
BOSS_D, BOSS_H = 12.0, 6.0
SHAFT_D, SHAFT_H = 6.0, 21.0
FLY_D, FLY_T = 100.0, 10.0

grey_h = CAN_H + GBX_H
z_grey = P2_MID - grey_h / 2           # grey body centred on platform 2 -> 51 to 104
z_enc = z_grey - ENC_H
z_gbx = z_grey + CAN_H
z_boss = z_grey + grey_h
z_shaft = z_boss + BOSS_H
fly_z0 = z_shaft                        # flywheel on the shaft above the boss: z 110 to 120

def cyl(d, z0, h):
    return cq.Workplane("XY").workplane(offset=z0).circle(d / 2).extrude(h)

model = (cyl(ENC_D, z_enc, ENC_H).union(cyl(CAN_D, z_grey, CAN_H)).union(cyl(GBX_D, z_gbx, GBX_H))
         .union(cyl(BOSS_D, z_boss, BOSS_H)).union(cyl(SHAFT_D, z_shaft, SHAFT_H))
         .union(cyl(FLY_D, fly_z0, FLY_T)))
print(f"motor z {z_enc:.1f} to {z_shaft + SHAFT_H:.1f}, flywheel z {fly_z0:.1f} to {fly_z0 + FLY_T:.1f}")
cq.exporters.export(model, "motor_ref.step")

"""Yoke inserts (platform -1 / -2), v22 (mm). Run from CAD/: python inserts.py -> insert_minus1.step, insert_minus2.step
(run AFTER encoder_housing.py and lid.py; they are fitted from below through the arc slots in platforms 0/1).
v22: the plates now fill the underside pockets of platform 0 exactly (arms out to R80 = 160 wide); the walls are 10 mm thick
(R44.5-54.5), run from the plates up to the lid underside (z173) and the lid screws onto them; 3 inward belts per wall and a
10 mm thick wall everywhere except the flywheel band (z127-141), where the wall is thinned to clear the 100 mm flywheel."""
import math
import cadquery as cq

PLATE_T, HUB_A, HUB_B, WEDGE, SKIRT_D = 2.0, 50.0, 60.0, 30.0, 160.0   # plates z0-2 / z2-4 in the pockets of platform 0
HOLE_D = 21.0                                    # centre hole for the laser straw (OD 20)
WALL_R0, WALL_R1, WALL_SPAN = 44.5, 54.5, 19.0   # wall 10 thick, +-19 deg (clear of the deck X arms: 44.5 sin 26 = 19.5 > 18)
FLY_Z0, FLY_Z1, FLY_R = 127.0, 141.0, 50.5       # flywheel band (R50 flywheel z129-139): wall thinned to R50.5-54.5 here
BELT_R0 = 34.5                                   # belts add 10 mm inward (R34.5-44.5), 20 mm tall
BELTS = ((14.0, 34.0), (100.0, 120.0), (147.0, 167.0))   # 3 belts, clear of the deck (z85-90), wire window (z54-74), flywheel band
WALL_TOP = 173.0                                 # lid underside (lid top 177 - 4 mm disc)
HOLE_W, HOLE_H = 20.0, 10.0                      # 2 x 1 cm hole through each wall
HOLE_Z_A, HOLE_Z_B = 155.0, 140.0                # yoke -1 z150-160, yoke -2 z135-145
LID_HOLE_R, LID_HOLE_ANG, LID_PILOT_D, LID_PILOT_DEPTH = 49.5, 12.0, 2.6, 15.0   # 2 M3 pilot holes in each wall top

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

def arc(r0, r1, z0, h, angle):
    return disc(2 * r1, z0, h).cut(disc(2 * r0, z0, h)).intersect(wedge(angle, z0, h, WALL_SPAN))

def plate(shape_axes, hub_d, z0):
    p = disc(hub_d, z0, PLATE_T)
    for a in shape_axes:
        p = p.union(wedge(a, z0, PLATE_T))
    return p.intersect(disc(SKIRT_D, z0, PLATE_T)).cut(disc(HOLE_D, z0, PLATE_T))

def yoke(axis_deg, plate_solid, wall_z0, hole_z):
    parts = [plate_solid]
    for a in (axis_deg, axis_deg + 180):
        h = WALL_TOP - wall_z0
        wall = arc(WALL_R0, WALL_R1, wall_z0, h, a).cut(arc(WALL_R0 - 1, FLY_R, FLY_Z0, FLY_Z1 - FLY_Z0, a))
        parts.append(wall)
        for z0, z1 in BELTS:
            parts.append(arc(BELT_R0, WALL_R0 + 0.5, z0, z1 - z0, a))
    body = union_all(parts)
    for a in (axis_deg, axis_deg + 180):
        cutter = (cq.Workplane("XY").workplane(offset=hole_z - HOLE_H / 2).center((BELT_R0 - 5 + WALL_R1 + 6) / 2, 0)
                  .rect(WALL_R1 + 6 - (BELT_R0 - 5), HOLE_W).extrude(HOLE_H).rotate((0, 0, 0), (0, 0, 1), a))
        body = body.cut(cutter)
        for off in (-LID_HOLE_ANG, LID_HOLE_ANG):
            x, y = LID_HOLE_R * math.cos(math.radians(a + off)), LID_HOLE_R * math.sin(math.radians(a + off))
            body = body.cut(cq.Workplane("XY").workplane(offset=WALL_TOP - LID_PILOT_DEPTH).center(x, y)
                            .circle(LID_PILOT_D / 2).extrude(LID_PILOT_DEPTH))
    return body

if __name__ == "__main__":
    # plate -1 fills the whole z0-2 pocket (both shapes) so the underside is flush; plate -2 is z2-4
    plate_a = plate((0, 180), HUB_A, 0.0).union(plate((90, 270), HUB_B, 0.0))
    plate_b = plate((90, 270), HUB_B, PLATE_T)
    yoke_a = yoke(0, plate_a, 0.0, HOLE_Z_A)
    yoke_b = yoke(90, plate_b, PLATE_T, HOLE_Z_B)
    cq.exporters.export(yoke_a, "insert_minus1.step")
    cq.exporters.export(yoke_b, "insert_minus2.step")
    for n, p in (("insert_minus1", yoke_a), ("insert_minus2", yoke_b)):
        b = p.val().BoundingBox()
        print(f"{n}: valid {p.val().isValid()}, solids {len(p.val().Solids())}, z {b.zmin:.1f}..{b.zmax:.1f}, xy {b.xlen:.1f} x {b.ylen:.1f}")

"""Laser straw (v22, mm): a two-part threaded tube through the centre of platforms 0/1 and both insert plates.
Run from CAD/: python laser_straw.py -> laser_straw_a.step (hollow bolt, goes in from the top) and laser_straw_b.step (nut, screws on from below).
Part A: flange on platform 1 (z10-13), tube OD 18 with an external ridge to OD 20, bore 10 for the laser, z-14 to 10.
Part B: nut under the inserts (z-10 to 0), internal ridge; tightening it clamps platform 0/1 and plates -1/-2 between the flange and the nut,
so the inserts are joined to the module. Same ridge construction as the lid / sensor ring thread."""
import cadquery as cq

BORE_D, CORE_R, TIP_R = 10.0, 9.0, 10.0           # laser bore, tube core radius, thread tip radius
PITCH, W0, W1 = 2.5, 1.4, 0.5                     # ridge root / tip width
A_Z0, A_Z1, FLANGE_D, FLANGE_T, PLATFORM_TOP = -14.0, 10.0, 34.0, 3.0, 10.0
NUT_Z0, NUT_Z1, NUT_D, NUT_ROOT_R, NUT_TIP_R = -10.0, 0.0, 34.0, 10.4, 9.4

def disc(d, z0, t):
    return cq.Workplane("XY").workplane(offset=z0).circle(d / 2).extrude(t)

def ridge(r0, r1, z_start, height):
    helix = cq.Wire.makeHelix(PITCH, height, r0)
    prof = cq.Workplane("XZ").polyline([(r0, -W0 / 2), (r1, -W1 / 2), (r1, W1 / 2), (r0, W0 / 2)]).close()
    return prof.sweep(cq.Workplane("XY").newObject([helix]), isFrenet=True).translate((0, 0, z_start))

a_body = disc(2 * CORE_R, A_Z0, A_Z1 - A_Z0).union(disc(FLANGE_D, PLATFORM_TOP, FLANGE_T))
a_thread = ridge(CORE_R, TIP_R, A_Z0, A_Z1 - A_Z0 + PITCH).intersect(disc(2 * TIP_R + 2, A_Z0, A_Z1 - A_Z0))
part_a = a_body.union(a_thread).cut(disc(BORE_D, A_Z0, A_Z1 + FLANGE_T - A_Z0))

b_start = A_Z0 + PITCH / 2 + PITCH * 1            # half-pitch phase against part A so the ridges interleave (root sunk 0.3 into the nut wall so it fuses)
b_thread = ridge(NUT_ROOT_R + 0.3, NUT_TIP_R, b_start, NUT_Z1 - b_start + PITCH).intersect(disc(2 * NUT_ROOT_R + 1, NUT_Z0, NUT_Z1 - NUT_Z0))
part_b = disc(NUT_D, NUT_Z0, NUT_Z1 - NUT_Z0).cut(disc(2 * NUT_ROOT_R, NUT_Z0, NUT_Z1 - NUT_Z0)).union(b_thread)

if __name__ == "__main__":
    for n, p in (("laser_straw_a", part_a), ("laser_straw_b", part_b)):
        b = p.val().BoundingBox()
        print(f"{n}: valid {p.val().isValid()}, solids {len(p.val().Solids())}, z {b.zmin:.1f}..{b.zmax:.1f}, dia {b.xlen:.1f}")
        cq.exporters.export(p, n + ".step")

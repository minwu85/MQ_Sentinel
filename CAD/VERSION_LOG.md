# MQ Sentinel Encoder Housing - Version Log

Keep this file updated every time a new model version is exported. Add the newest version at the top of the
"Version history" section, and update "Part names" if a part is added or renamed.
Source: `encoder_housing.py` (housing), `lid.py` (lid), `motor_ref.py` (motor and flywheel reference, not printed).
Current dimensions: `MQ_Sentinel_Encoder_Housing_Dimensions.txt`. All units mm.

## Part names

Use these names when asking for a change.

```
            +-----------------------+
            |  HANDLE (arched, slot)|             LID  (lid.step)
   +--------+-----------------------+--------+    Lid disc  z135-139
   |            LID DISC + 4 holes           |    Lid skirt  (threaded tube, 10 mm, below the disc)
   +-----+---------------------------+-------+
         |  LID SKIRT (thread)       |
   ======+===========================+=======  <- PLATFORM 3 (top ring / sensor ring)  z129-135
   | |   [ sensor notches x3 ]       | |           ring thread inside, 4 screw holes, 3 sensor notches
   | |   +-----------------------+   | |
   | | UPPER POSTS x4 (rect)     |   | |        MOTOR CLAMP (box, half-round bore, groove, pads) z80-110
   | |   |  FLYWHEEL z113-123    |   | |        UPPER POSTS  z80-129
   | |   +-----------+-----------+   | |
   | |  MOTOR CLAMP  |  z80-110      | |
   ====================================== <- PLATFORM 2 (middle deck) z75-80
   | |  motor hole + WIRE SLOT       | |           cross shape, 4 corner windows, motor pass-through
   | |   MOTOR (black encoder        | |
   | |   + lower half of grey body)  | |        MOTOR  z33-124 (reference only)
   | |   under platform 2            | |        LOWER POSTS x3 (rect)  z3-75
   ====================================== <- PLATFORM 1 (base)  z0-3
   (BOTTOM THIN LAYER, 160 mm, 1 mm, under platform 1)
```

| Name | What it is |
|---|---|
| Platform 1 (base) | Bottom disc, 150 mm OD, 4 mm thick, centre hole 10 mm |
| Bottom thin layer | 160 mm OD, 1 mm thick disc under platform 1 (the only part wider than 150) |
| Lower posts | 3 rectangular posts (10 mm wide) joining platform 1 to platform 2 |
| Platform 2 (middle deck) | 150 mm OD, 5 mm thick. Cross shape with 4 corner windows, motor pass-through, wire slot |
| Motor pass-through | 40 mm full-circle hole in platform 2 |
| Motor sleeve | Full-round tube under platform 2 (OD 46, 47 mm tall) holding the black encoder and 32 mm of the grey body |
| Wire window | 20 x 10 mm opening at the bottom front of the motor sleeve |
| Upper posts | 4 rectangular posts (10 mm wide) joining platform 2 to platform 3 |
| Platform 3 (top ring / sensor ring) | 150 mm OD, 120 mm ID, 6 mm thick |
| Sensor notches | 3 notches, 8 x 7 mm, 120 deg apart, on the outer edge of platform 3 |
| Screw holes | 4 holes, 3 mm, at radius 67.5 on platform 3 and on the lid |
| Motor clamp | 60 x 30 mm box with a half-round bore, standing on platform 2 |
| Bore / groove | 40 mm half-round bore; the upper 15 mm has a 1 mm deeper groove |
| Centring pads | 3 small pads in the lower bore (30, 90, 150 deg) that hold the motor on the housing axis |
| Arm holes | 4 vertical 3 mm holes in the top faces of the clamp arms |
| (lid parts below: not updated since v9, no ring thread) | |
| Lid disc | Flat 150 mm disc, 4 mm thick, with the 4 screw holes |
| Handle | Arched handle on the lid, with a 10 x 6 mm slot at the apex |
| Lid skirt | 10 mm tall threaded tube under the lid disc |
| Flywheel (reference) | 100 mm x 10 mm mild steel, z113-123 |

## Version history

### v10 - full motor sleeve, thread removed, thicker base
- Issue: the motor was held only by a half-round clamp and a hole in platform 2.
  Fix: a full-round sleeve (OD 46, bore 40, 47 mm tall) under platform 2 now holds the black encoder (15 mm) and 32 mm of the grey body. The motor bottom is at z28.
- Issue: the motor wires need a way out.
  Fix: a 20 x 10 mm wire window at the bottom front of the sleeve. The wire slot in platform 2 is gone.
- Issue: with the motor 5 mm lower, the 30 mm clamp would hit the flywheel (now z108-118).
  Fix: clamp 25 mm tall (z80-105). Plain bore 10 mm, groove 15 mm, top step 10 mm, arm holes 25 mm deep.
- Request: remove the thread inside the sensor ring.
  Result: ring thread removed. `lid.py` is no longer updated and does not match the ring. The lid files are left as they were in v9.
- Issue: the base was too thin to stand on.
  Fix: platform 1 is now 4 mm (the lower posts start at z4).
- Checks: one valid solid, 160 x 160 x 135. 0 mm3 interference with the motor/flywheel reference.
- Open: the upper grey part (22 mm) is held only by the clamp pads and the half-round; no front clip yet.

### v9 - sunk motor, rectangular posts, 3 sensors, thin base
Requested: lower the flywheel; sink the motor under platform 2; rectangular posts; 3 sensors; centre the motor; 3 mm base.
- Issue: the flywheel sat above the ring, far too high.
  Fix: the motor now sits 42 mm lower (bottom at z33). The black encoder and the lower half of the grey body are under platform 2. The flywheel is at z113-123, under the lid skirt (z125).
- Issue: the motor had to go through platform 2.
  Fix: a 40 mm motor pass-through in platform 2, plus a 20 mm wide wire slot running out the open front for the motor wires.
- Issue: the clamp would have hit the flywheel after the motor moved down.
  Fix: the clamp is now 30 mm tall (z80-110). The 1 mm top step is now 15 mm tall. The bore is 15 mm plain, then a 15 mm groove. Arm holes are 30 mm deep.
- Issue: round posts.
  Fix: posts are now rectangular, 10 mm wide and 14 mm radial, at radius 67.5. They stay outside the lid thread. The ring screw holes still land on the upper posts.
- Issue: 6 sensor notches.
  Fix: 3 notches, 120 deg apart (0, 120, 240).
- Issue: the motor sat roughly in the centre, not exactly.
  Fix: the bore stays centred on the housing axis (0,0). Three 2 mm centring pads reduce the bore to radius 18 against the 17.5 mm grey body. The motor can sit at most 0.5 mm off centre. The 36 mm encoder cap is below platform 2 and does not touch the pads.
- Issue: platform 1 was too thick.
  Fix: 3 mm. Platform 2 stays at z75.
- Checks: housing, lid and motor/flywheel reference have 0 mm3 interference. The housing is 160 x 160 x 135, the lid top is at z159.
- Open: the motor is held by the bore pads and the pass-through only. If it still moves, a clip across the open front is the next step.
- Open: the motor length split (black 15 mm, grey 54 mm, shaft 22 mm) is assumed from the 91 mm and 69 mm figures. Check against the real motor.

### v8 - lid handle slot
- Request: a hole at the centre of the handle, 1 cm long.
- Fix: a 10 x 6 mm slot through the handle apex. A round 10 mm hole did not fit the 12 mm wide handle.

### v7 - lid and ring thread
- Issue: the clamp (65 mm) stuck out above the ring.
  Fix: clamp cut to 45 mm. The bore groove shortened to 15 mm.
- Issue: a lid was needed.
  Fix: a flat lid with the same 4 holes, a handle and a 10 mm threaded skirt. The ring got a matching internal thread (pitch 4, loose fit).
- Issue: the 10 mm skirt hit the upper posts.
  Fix: posts moved out to radius 67.5 and made 14 mm wide.
- Issue: the lid and ring ridges overlapped at first.
  Fix: half-pitch phase offset on the lid thread. The check is now 0 mm3.

### v6 - groove, side holes, large windows, sensor notches
- Fix: the half-round bore was a plain bore with a 1 mm deeper groove again.
- Fix: no holes inside the bore; 4 vertical holes in the arm top faces instead.
- Fix: platform 2 centre solid again. Corner windows larger (40 mm arms, windows to R56).
- Fix: platform 3 thickened to 6 mm; 6 sensor notches, 8 x 7 mm, 60 deg apart.
- Issue: the clamp was 65 mm tall and stuck above the ring.
  Result: kept 65 mm for this version (fixed in v7).

### v5 - clamp cleanup
- Fix: clamp shortened to 45 mm so it stays under the ring.
- Fix: the groove ledge removed (later restored in v6 in a different form).

### v4 - stale export and Appendix 1 check
- Issue: the exported STEP/STL files did not update. The export path was wrong, so Onshape kept showing the old model.
  Fix: export paths corrected. Always re-import the STEP file in Onshape after a new version.
- Fix: bottom thin layer 1 mm, platform 2 reduced to 5 mm.
- Check: Appendix 1 (R80 / R76, 170 mm height) satisfied.

### v3 - clamp, slot, thin layer
- Added: 10 mm centre hole in platform 1, 2 mm thin bottom layer (160 mm), clamp 60 x 30 x 65 with a 40 mm half-round and a 2 x 1 cm slot in platform 2.

### v2 - first model
- Platform 1 (5 mm), platform 2 (10 mm), ring (5 mm), 3 and 4 cylindrical posts (15 mm), clamp with a 39 mm bore, ring holes.
- Built from the first photos and the motor figures. Exported as STEP and STL.

### v1 - reference prototype (printed)
- White printed prototype in the photos: 3 platforms and cylindrical posts. This is the model the dimensions in v2 are based on.

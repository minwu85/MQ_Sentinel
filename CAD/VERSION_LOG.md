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
| Platform 1 (base) | Bottom disc, 150 mm OD, 5 mm thick; flat top; the underside has a centre circle and two 60 deg wedges recessed 2 mm; centre hole 10 mm |
| Bottom layer 0 | 160 mm OD, 5 mm thick, under platform 1; carries the underside pattern (the only part wider than 150) |
| Lower posts | 4 rectangular posts (10 mm wide) joining platform 1 to platform 2 |
| Platform 2 (middle deck) | 150 mm OD, 5 mm thick. X cross (arms on the diagonals), motor sleeve through the centre |
| Upper posts | 4 rectangular posts (10 mm wide) joining platform 2 to platform 3 |
| Platform 3 (top sensor ring) | 150 mm OD, 120 mm ID, 6 mm thick, always a full ring, open top, 3 sensor notches |
| Motor sleeve | Full tube through the middle of platform 2 (bore 38, OD 44, z36-80) holding the encoder and motor can; closed bottom |
| Wire window | 10 x 20 mm opening at the side of the motor sleeve, at the encoder |
| Screw holes | 4 holes, 3 mm, at radius 67.5 on platform 3 and on the lid |
| (lid parts below: not updated since v9, no ring thread) | |
| Lid disc | Flat 150 mm disc, 4 mm thick, with the 4 screw holes |
| Handle | Arched handle on the lid, with a 10 x 6 mm slot at the apex |
| Lid skirt | 10 mm tall threaded tube under the lid disc |
| Flywheel (reference) | 100 mm x 10 mm mild steel, z113-123 |

## Dimensions by module (current version, v17)

| Module | Diameter / size (mm) | Thickness / height (mm) | Z range |
|---|---|---|---|
| Platform 0 | 160 dia | 8 | 0 - 8 |
| Platform 1 | 150 dia | 5 | 8 - 13 |
| Lower posts (4) | 10 x 14 rectangle, radius 67.5 | 73 | 12 - 85 |
| Platform 2 (middle deck) | 150 dia, X arms 36 wide, rim 4, centre hole 46 | 5 | 85 - 90 |
| Upper posts (4) | 10 x 14 rectangle, radius 67.5 | 37 | 90 - 127 |
| Platform 3 (sensor ring) | 150 OD / 120 ID, inside flat | 6 | 127 - 133 |
| Yoke -1 plate | 50 dia circle + 60 deg arms to R69 | 2 | 13 - 15 |
| Yoke -2 plate | 60 dia circle + 60 deg arms to R69 | 2 | 15 - 17 |
| Yoke walls (4) | R66-69, 54 deg wide | 82 / 80 | 13 / 15 - 95 |
| Yoke bars (2) | 10 wide, at Y +28 and X -28 | 5 | 90 - 95 |
| Motor tube | 38 bore / 44 OD, flange 56 | 44 | 54 - 98 |
| Motor (reference) | 36 encoder, 35 body, 6 shaft | 91 | 57 - 148 |
| Flywheel (reference) | 100 dia | 10 | 132 - 142 |

## Dimension history

| Version | Platform 0 (dia x t) | Platform 1 (dia x t) | Platform 2 (dia x t) | Platform 3 (OD/ID x t) | Overall height |
|---|---|---|---|---|---|
| v22 | 160 x 5 | 150 x 5 | 150 x 5 | 150/120 x 6 | 133 housing, 177 with cap |
| v17 | 160 x 8 | 150 x 5 | 150 x 5 | 150/120 x 6 | 133 (no cap) |
| v16 | 160 x 8 | 150 x 5 | 150 x 5 | 150/120 x 6 | 133 housing, 170 with cap |
| v15 | 160 x 10 | 150 x 5 | 150 x 5 | 150/120 x 6 | 145 |
| v14 | 160 x 5 | 150 x 5 | 150 x 5 | 150/120 x 6 | 135 |
| v13 | 160 x 1 | 150 x 5 | 150 x 5 | 150/120 x 6 | 135 |
| v12 | 160 x 1 | 150 x 5 | 150 x 5 | 150/120 x 6 | 135 |
| v11 | 160 x 1 | 150 x 5 | 150 x 5 | 150/120 x 6 | 135 |
| v10 | 160 x 1 | 150 x 4 | 150 x 5 | 150/120 x 6 | 135 |

Add a new row here and update the module table whenever a new version is exported.

## Version history

### v23 - 2026-10-24 (second pass) - 4 parts only: lid, encoder, insert, laser
Not committed or pushed. Re-import the 4 `.step` files in Onshape. Parts now: `lid.step`, `encoder_housing.step`, `insert.step` (new, replaces insert_minus1/2), `laser.step` (new, replaces the straw A/B). Removed: insert_minus1/2.step, laser_straw_a/b. Moved to `reference/`: motor_ref.step, motor_tube.step (not printed).

**Issues from v22 and fixes**
- Lid had too many holes -> all filled (the 4 screw holes at R67.5 and the 8 wall holes are gone). Only 4 M3 holes remain, one in each ear (see below).
- Lid inner support (boss + ribs) -> removed.
- Keyhole was a hole in the flat face -> replaced by a raised arch hanger (loop) on the top: 32 wide, 8 mm legs, 10 mm thick, opening 16 x 14, on a 44 x 18 x 3 pad. Top disc 4 -> 6 mm.
- Lid height: top z197 = pipe top (z177) + 20 mm. Shell z136-191. Arch top z222.
- Insert walls were inside the pipe and had belts -> walls are now OUTSIDE the pipe (inner face R80.3, 10 mm thick, to R90.3), smooth inside, belts removed, no holes.
- Two inserts -> one part: 4 mm plate (centre circle 60 + 4 arms) with 4 walls at the arm ends. It fills the pocket in the bottom of the encoder (100% at z0 except the centre hole).
- Walls are on the diagonals (45/135/225/315), not 0/90/180/270 as in v22: walls at 0/180 would sit in front of the sensor at 0 deg. +-9 deg wide.
- Wall top z190.7, lid top z197: the lid has 4 ears (R70-91.3, 6 mm, flush with the top) over the wall tops, one M3 screw per ear into a 2.6 mm pilot hole in the wall (15 deep). 0.3 mm gap so the lid can still turn into place.
- Encoder: the 4 arc slots (red in image 5) are filled. The underside pocket is now the single-plate shape (4 mm, circle + 4 arms on the diagonals).
- Encoder: centre hole is now a threaded socket (hole R10.4, female ridge z4-10, pitch 2.5), same ridge method as the lid.
- Laser: new elbow part. Hollow stub screws into the socket from below (bore 8 for wires), flange 30 against the insert/housing underside, then a column and a horizontal tube along +X (OD 16.5, bore 10.5, 70 long, M3 grub-screw hole). The thread phase is set so that when it is screwed to the flange the tube points at 0 deg = sensor notch 1 (of 0/120/240). Tightening clamps the insert plate to the housing.

**Checks (CadQuery on the exported files)**
- 6 solids (4 parts + motor/flywheel reference + a 172 mm pipe, R75-80, z5-177, as a check body): all valid, one solid each. Overlap between every pair 0.0 mm3, also with the lid turned to the drop-in orientation.
- Thread fit scan: the laser stub turns freely when the female phase is -0.65 to +0.15; set to +0.1. Laser tube direction 0.0 deg.
- Lid top minus pipe top = 20 mm. Wall top 190.7. Lid is 0.3 mm inside the pipe ID, walls 0.3 mm outside the pipe OD.

**Open / please check**
- The insert is 174.4 wide (R87.2 at the corners, walls to R90.3). Appendix 1 shows R80 as the envelope: this is over it by up to 10 mm. Needed because the walls are outside the pipe.
- Sensor ring is now 67 mm below the lid top (brief: 40 mm below the top of the body). Not touched, as asked.
- Wall edge to sensor notch is 2.5 deg (about 4 mm) at 120/240 deg. Narrow walls, but the walls stand on a 4 mm plate: check stiffness.
- Laser seats at 0 deg +-15 deg (thread play 0.1 mm). Add a mark/flat on the flange if you want a visual stop. The 70 mm tube sticks out at z-24..-8, under the insert.
- Laser module size (bore 10.5) not confirmed. The 6 mm skin of platform 0/1 (z4-10) gives 2.4 thread turns for the socket.
- The ring's own 4 screw holes at R67.5 are still in the sensor ring (ring not touched), but the lid no longer has holes over them.

### v22 - 2026-10-24 - stronger frame, taller lid with keyhole, 10 mm insert walls, laser straw

**1 Frame (`encoder_housing.step`, `encoder_housing.py`)**
- 1.1 Deck rim (the "ring for platform 4", red circles in image 7 = platform 2 rim): was 4 mm wide, now 15 mm (R60-75, window radius 71 -> 60). Plus a 5 x 6 mm flange under the rim (z80-85, R69-75) so the rim does not flex between the posts.
- 1.3 Posts: 10 x 14 -> 20 x 20 mm (R55-75 on the 45/135/225/315 deg arms, trimmed to the 150 mm body). The last 5 mm under the sensor ring only reach R60, because the lid skirt (R55-58, from z123) sits there.
- 1.2 + 1.4 Motor centring: the sleeve bore is 38 from the base up to the deck (z54-85, unchanged: it holds the 36 mm encoder). From the deck up (z85-97) there is now a collar with a 36 mm bore (radius 1 mm smaller), and the collar is 2 mm taller (top z95 -> 97). The 35 mm grey body is now held 0.5 mm per side and on the axis (bore centre = housing centre, measured 0.000, 0.000).
- Needed for the inserts (not asked directly): 4 arc slots R44-55, +-20 deg, through platforms 0/1 at 0/90/180/270 deg so the walls pass; centre hole 10 -> 21 for the laser straw.

<img width="628" height="635" alt="image" src="https://github.com/user-attachments/assets/1438d997-3b14-4dc3-abd5-817767653a5d" />
<img width="597" height="657" alt="image" src="https://github.com/user-attachments/assets/5aee3776-568c-4814-a7bf-2b322df7290f" />

**2 Lid (`lid.step`, `lid.py`)** - only the height and the hanger changed
- Top z170 -> 177 (shell 7 mm longer). Reason: tube is 172 mm and sits on platform 0 (z5-177); the lid now ends flush with its top. Sensor ring not touched.
- Keyhole hanger at the centre: entry hole dia 10, slot 5.5 wide (sliding 14 mm), head pocket behind the 4 mm face, thick boss 30 x 60 x 12 under the disc + 4 diagonal ribs 4 x 10 mm. Replaces the 20 x 8 finger slot (same place).
- 8 countersunk M3 holes (R49.5, +-12 deg either side of each wall) to screw the four insert walls to the lid.
<img width="635" height="496" alt="image" src="https://github.com/user-attachments/assets/aaeca7f7-6e5e-4aab-bbca-3bbce65eacaf" />
<img width="617" height="556" alt="image" src="https://github.com/user-attachments/assets/565e287c-7ef6-49cc-b24c-977a487344f6" />

**3 Inserts (`insert_minus1.step`, `insert_minus2.step`, `inserts.py`)**
- 3.1/3.2 Plates fill the underside pockets of platform 0 exactly. Plate -1 (z0-2) now fills the whole 0-2 pocket (both shapes), plate -2 is z2-4. Arms reach R80 = 160 mm wide. Bottom face is 100% flush (checked); only the 21 mm centre hole is open.
- 3.3 Walls: 3 -> 10 mm thick (R44.5-54.5, +-19 deg), top z173 = lid underside, height 173 (>= 172). Thinned to 4 mm (R50.5-54.5) only for z127-141 where the 100 mm flywheel passes.
- Purple supports: 3 belts per wall (20 mm tall, 10 mm extra inward, z14-34, z100-120, z147-167) placed clear of the deck (z85-90), wire window (z54-74) and flywheel band.
- 2 x 1 cm hole in each wall kept (yoke -1 z150-160, yoke -2 z135-145), plus 2 M3 pilot holes (dia 2.6, 15 deep) in each wall top for the lid screws.
- Fit from below: yoke -2 first, then yoke -1, then the lid.
<img width="558" height="636" alt="image" src="https://github.com/user-attachments/assets/1914d0f9-c9ed-4cff-8f0f-c0d030f8f9c7" />

**5 Laser straw (new, `laser_straw.py` -> `laser_straw_a.step`, `laser_straw_b.step`)**
- Part A: hollow bolt, flange dia 34 on platform 1 (z10-13), tube OD 18 + ridge to OD 20, bore 10 for the laser, z-14 to 10, goes through platforms 0/1 and both plates.
- Part B: nut dia 34, z-10 to 0, screws on from below. Tightening clamps module + plates -1/-2 together (joins insert to module). Same ridge method as the lid thread, pitch 2.5.

**6** Lid and sides now meet at z173/177 (wall tops touch the lid underside; the 8 screws tie them).

**4 Checks (all run in CadQuery on the exported STEP files)**
- All 7 parts valid single solids. Interference between every pair (housing, lid, both inserts, both straw parts, motor + flywheel reference): 0.0 mm3. Also 0.0 with the lid turned to the drop-in orientation. Detection sanity-checked (bolt/nut in phase = 90 mm3).
- Bottom pocket fill: 100% at z0 except the 21 mm hole; 0.5% slot clearance at z2-4.
- Wall clearances: flywheel 0.5 mm, deck X arm 1.5 mm, lid flange/skirt 0.5 mm, ring thread tip 4 mm.
- Bad versions rechecked: v16 deck rim 1.5/4 mm and 10 x 14 posts (weak) fixed; v21 plates sitting on platform 1 did not carry the module (now under it); v21 walls 3 mm (now 10); v19/v20 "sleeve fused so yokes do not lift the tube" is still true, see open points.

**Open / assumptions (please check)**
- Wall is 10 mm except 4 mm at z127-141. A full 10 mm there would hit the flywheel (R50) or the lid skirt (R55).
- The sensor ring is 47 mm below the lid top now (brief: 40 mm below the top of the body). The ring was not touched, as asked; it would need to move up ~7 mm.
- Assumed the pipe sits on platform 0 (top at z177 for a 172 mm pipe). If it sits at z0, lid top could be 172.
- Collar bore 36 around a 35 mm grey body: 0.5 mm clearance, tight for a print. The encoder (36 dia) stays in the 38 bore. "Radius 1 mm smaller" was applied to the collar only.
- Keyhole slot is not raised above the face (that would break the lid = side height); it has a boss behind it instead. Check the screw head: head dia <= 9.5, shank <= 5.
- The laser module is assumed to fit the 10 mm bore (Duinotech XC4490 size not confirmed).
- The sleeve is still fused to the deck. The inserts hang the module through the lid screws and the straw clamp, not by lifting the tube.
- Threads (straw, lid, ring) untested in a print.

### v21 - inserts: bars removed, two block holes each
- `encoder_housing.step` not changed.
- Request: remove the green area (the top bars with their end stubs) from both yokes. Each yoke is now a round plate with two curved walls (up to z165).
- Request: two holes per module for blocks that make it stable. Each yoke has one 2 x 1 cm hole (20 wide x 10 tall) in each of its two walls: yoke -1 at z150-160, yoke -2 at z135-145. They are at different heights (5 mm apart) and above the ring top (z133), so they do not overlap.
- Checks: both yokes valid solids; 0 mm3 interference with the frame, with each other and with the motor reference; holes confirmed open and the wall solid just above them.
- Open: with the bars gone, the two yokes are only joined by their stacked plates.

side1: <img width="693" height="993" alt="image" src="https://github.com/user-attachments/assets/175e3ecc-16a5-4ad3-815c-a4674ae4ea22" />
side2: <img width="734" height="1036" alt="image" src="https://github.com/user-attachments/assets/b7f9fa6f-b470-4bff-9704-8b4ce08674e8" />


### v20 - platform 0 at 5 mm, v17 inserts with 165 mm side walls
- Request: keep `encoder_housing.step` as v16 and only make platform 0 5 mm thick (was 8). Platform 1 now sits at z5-10 and the lower posts start 1 mm lower (z9). Nothing else in the frame changed. Frame is still one valid solid, 160 x 160 x 133.
- Request: back to the v17 inserts; only make the green side parts (the curved walls) higher, 165 mm. The wall top is now z165 (32 mm above the ring top, so they work as handles). Plates sit on the new platform 1 top (z10-12 and z12-14); plate sizes (dia 50 / 60), the 10 mm hole and the bars (z90-95, off-centre) are as in v17.
- Why the walls moved: at 165 mm the v17 walls (R66-69) would run through the complete sensor ring (R60-75). They are now R54-57, inside the ring bore, and 44 deg wide (v17 was 54 deg) so they clear the deck X arms. The bar ends turn in to meet the walls.
- Checks: both yokes valid solids; 0 mm3 interference with the frame, with each other and with the motor reference.
- Open: the v16 sleeve is fused to the deck, so the yokes surround the tube and do not lift it.

Main: <img width="715" height="790" alt="image" src="https://github.com/user-attachments/assets/9f2d5833-526e-4397-8f85-6338d5f9fae4" />

Side1:<img width="761" height="1039" alt="image" src="https://github.com/user-attachments/assets/d911497f-5401-4ea4-8346-417335394206" />
Side2: <img width="1128" height="1072" alt="image" src="https://github.com/user-attachments/assets/2dbacff1-7b16-4442-a1ee-3aa55d27ed08" />


### v19 - inserts back to the v17 yoke form, taller than the tube, larger circle plates
- `encoder_housing.step` not changed (v16).
- Request: back to the v17 insert design (round plate on platform 1, two curved walls, a top bar), with the two inserts taller than the tube. The yoke top is z100, 5 mm above the tube top (z95). Plates sit on platform 1 (z13-15 and z15-17), as in v17.
- Request: "172 mm long section of 160 mm". Taken as the envelope: the yokes are 138 wide (limit 160) and top out at z100 (limit 172).
- Request: more circle parts so the modules join and balance. The plates are now big concentric circles (dia 100 for platform -1, dia 110 for -2, up from 50 / 60). They stack on each other, stay inside the posts (r60.5), and widen the footprint. Both keep the 10 mm centre hole.
- Checks: both yokes valid solids; 0 mm3 interference with the v16 frame and with each other.
- Open: the v16 sleeve is fused to the deck, so the yokes surround the tube and do not lift it.
- Open: the meaning of "three modules" is a guess (frame + the two yokes).


### v18 - frame back to v16, inserts resized to the tube
- Request: `encoder_housing.step` back to v16 (not changed). Restored from the v16 commit, with `encoder_housing.py` and `motor_ref.py`. The v17 frame (open deck hole, separate tube, no thread, no pockets) is dropped. `motor_tube.step` from v17 is no longer used.
- Request: only change `insert_minus1.step` / `insert_minus2.step`. New script `inserts.py`. The yokes are sized to the v16 sleeve (tube OD 44, z51-95, 44 tall): top at z100 (5 mm taller than the tube) and a 45 mm opening (0.5 mm clearance each side). Yoke -1: 49 tall; yoke -2: 47 tall (its plate is 2 mm higher).
- Yoke parts: ring plate around the tube base, two curved side walls (R66-69, 54 deg), top bar. The bars are off-centre and cross with a half-lap.
- Checks: both yokes valid solids; 0 mm3 interference with the v16 frame and with each other.
- Open: the v16 sleeve is fused to the deck, so the yokes do not lift the tube; they surround it. A removable tube would need a frame change.
- Open: "within 175 mm" in the request: the limit in the brief is 170 mm; the frame is 133 mm, the yokes top out at z100.

main:
<img width="853" height="877" alt="image" src="https://github.com/user-attachments/assets/4b4c322d-08a8-45ac-be26-93414ea89486" />

side1: <img width="1111" height="815" alt="image" src="https://github.com/user-attachments/assets/b0ea044e-e8a0-431d-84b9-742b856ccd87" />
side2: <img width="1160" height="893" alt="image" src="https://github.com/user-attachments/assets/dc8b51e3-4adf-4709-8331-835ce575bd0e" />


### v17 - yoke cage with hanging motor tube, ring flat
- Request: nothing more on the sensor ring. The ring thread is removed; the ring inside is flat. The cap/lid is dropped (`lid.py` and `lid.step` are left as they were and no longer match; not updated).
- Request: rebuild platform -1/-2 like images 1-3: a base plate, curved side walls and a top bar. Each yoke is now: plate (circle with 10 mm hole + two 60 deg arms) + two curved walls (R66-69, 54 deg wide, up to z95) + a bar between the walls at the top. Yoke -1 is along 0/180 deg, yoke -2 along 90/270 deg. The bars sit off-centre (Y +28 and X -28) so the tube centre stays open, and cross with a half-lap notch.
- The motor tube is now a separate part: 38 bore / 44 OD, 44 tall, closed bottom, with a 56 mm flange on top. It hangs from the two bars by the flange. The deck has a 46 mm centre hole so the tube passes through.
- Lifting the yokes by the curved walls lifts the tube with them; the walls pass through the deck axis windows (R71).
- The pockets in platform 0/1 are gone (the yokes sit on flat platform 1).
- The motor reference moved to z57 (tube floor top); flywheel z132-142 (above the ring plane, inside the 120 mm ring opening).
- Checks: every pair of frame, yokes, tube and motor has 0 mm3 interference.
- Open: the bars hold the tube by two off-centre contact points; the real assembly may need a third support.
- Open: the yokes and the tube are untested for printing (the 82 mm walls need orientation checked).

main:
<img width="691" height="805" alt="image" src="https://github.com/user-attachments/assets/44c60368-ad25-4295-878a-f53b5c9b5595" />


en: 
<img width="734" height="703" alt="image" src="https://github.com/user-attachments/assets/947d9319-7de5-4d56-bca2-5bf05bcc65ea" />

side1:
<img width="1019" height="698" alt="image" src="https://github.com/user-attachments/assets/70db13ab-d9ac-48ab-960b-7424dcf13818" />

side2:
<img width="854" height="610" alt="image" src="https://github.com/user-attachments/assets/9630fbf5-5342-43d4-91c9-1dd87583acae" />


### v16 - 8 mm platform 0, taller body with four-section cap, stronger rim
- Request: make the platform 0 160 x 8 mm and keep everything else unchanged. Platform 1 now sits at z8-13; the lower posts are 2 mm longer so the deck stays at z85-90.
- Request: the four rim arcs on the middle deck were too thin. The rim is now 4 mm wide (R71 to R75).
- Request: raise the side height to meet the 170 mm pipe. Overall height is now 170 mm, using a cap. Appendix 1 puts the sensor ring 40 mm below the top, so the ring moved down to z127-133 (centreline z130). Upper posts are 37 mm tall.
- Request: a bottle-lid style way to screw it together. The cap is a four-section quarter-turn screw cap: 4 thread sections on the ring (gaps at 0/90/180/270 deg) and 4 matching sections on the cap skirt. Drop it in with the sections on the axes, then turn 45 deg. Cap height z123-170 (37 mm above the ring). The raised handle is removed (170 mm limit) and replaced with a 20 x 8 mm finger slot.
- Not done: item 2 of the request ("Based on the document information provided I want to make") was cut off. The project brief is noted: 170 mm x 150 mm pipe, sensor ring and laser pointer heights, modular parts that come out in under 10 minutes. No laser mount has been modelled yet.
- Checks: housing and cap are valid solids; 0 mm3 interference between housing, cap (seated and drop-in orientation), inserts and the motor reference.
- Open: the platform -1/-2 inserts still have no walls or hanging tube (the earlier request was cut off).
- Open: the quarter-turn thread is untested in a print; the 10 mm skirt only engages the 6 mm ring.

<img width="768" height="765" alt="image" src="https://github.com/user-attachments/assets/8ca8e64b-5dda-4d83-b53b-b8393e2811bb" />

<img width="1119" height="634" alt="image" src="https://github.com/user-attachments/assets/025bda5e-38b5-460e-8d17-59bbfb8c53f4" />


### v15 - platform 0 / 1 split, insert plates -1 and -2
- Request (user clarified): platform 0 is 160 mm x 10 mm thick; platform 1 is 150 mm x 5 mm (same width as platform 2) on top of it. Everything above moved up 10 mm. Overall height 145.
- Platform -1 and -2 as separate insert plates (2 mm, 10 mm centre hole) fitted into underside pockets of platform 0: -1 = 5 cm circle + wedges at 0/180 deg; -2 = 6 cm circle + wedges at 90/270 deg. Exported as `insert_minus1.step` and `insert_minus2.step`; 0 mm3 overlap with the housing and with each other.
- Not done: the curved walls and top tab from the user's image 1, and hanging the motor tube from the inserts. The request text was cut off ("the tube is"), so the tube is still held by platform 2.
- Motor tube, wire window and motor reference moved up 10 mm with the rest. 0 mm3 interference with the motor reference.
- The old 2 mm underside pattern (v13/v14) is replaced by the insert pockets.

### v14 - thick layer 0, shifted motor tube, thinner deck rim
- Issue: the leftover thin 160 mm ring outside the pattern was the unwanted part. Layer 0 (160 mm) is now 5 mm thick, so it is the base itself, and the underside pattern (2 mm deep) runs out to the 160 mm edge.
- Request: move the motor tube up 0.5 cm, same size. The tube is z41-85 (44 tall), 5 mm above platform 2. The wire window moves with it (z44-64).
- Motor reference updated to the measured values: encoder cap 36, grey body 35, 69 mm body, first step 75, overall 91. Motor sits on the tube floor at z44; flywheel z119-129 (under the ring bottom at 129).
- Request: thinner ring on platform 2. The rim at the edge of the middle deck is 1.5 mm wide (was 3 mm).
- Checks: one valid solid, 160 x 160 x 135; 0 mm3 interference with the motor/flywheel reference.
- Open: the shaft tip is now flush with the ring top (z135). The flywheel is now higher than in v13, because the motor moved up.
<img width="1057" height="949" alt="image" src="https://github.com/user-attachments/assets/7d1c9c0f-b9c4-417c-bfc7-14cd6ad6519c" />

### v13 - flat base, open top, shorter motor tube
- Issue: the pattern was on the top of the base. It now goes into the underside of the bottom layer (2 mm deep, inside the 150 mm body; the 160 mm edge stays whole). The base top is flat with only the 10 mm centre hole.
- Issue: the top plus cross covered the motor. It is removed. Platform 3 is the ring only (sensor ring kept, with notches and screw holes), so the motor and flywheel drop in from above. Check: no housing material above z80 inside r50.
- Issue: the motor tube was too tall (70 mm). It is now 44 mm (z36-80), top flush with platform 2: it holds the encoder (12) and the motor can (29); the 37 mm gearbox sits above it.
- Check: bore 38 vs motor 37 (datasheet) and 36 (measured cap) - fits. 0 mm3 interference with the motor/flywheel reference.
- Open: the real encoder cap diameter. The datasheet drawing gives 33 for the can and 37 for the gearbox; the earlier measured values were 35-36.

### v12 - platform numbering corrected, sensor ring restored
- Correction from the user: the earlier platform numbers were counted the other way round. Platform 1 = base (bottom), platform 2 = middle deck, platform 3 = top sensor ring.
- The patterns from the v11 request are now on the right platforms: the top ring (3) gets the plus cross (sketch 2) and the middle deck (2) gets the X cross (sketch 3).
- Platform 3: the sensor ring is complete again and must never be removed. A 30 mm wide plus cross sits inside it with a 12 mm shaft hole. The 3 sensor notches are back (0, 120, 240 deg).
- Platform 2: X cross with 36 mm arms; the four axis wedges are cut away to R72, leaving a 3 mm rim. The landing pads are gone.
- Platform 1: now matches the SketchUp images. The centre circle and two opposite 60 deg wedges (90 / 270 deg) are recessed 2 mm; the two 120 deg sectors stay full height (6 divisions of 60 deg). Lower posts start at Z 3 so they sit on the recessed floor.
- Posts: 4 rectangular posts above platform 2 (between platforms 2 and 3). The lower posts are now also 4, on the X arms at 45 / 135 / 225 / 315 deg (they were 3 on the axes, which no longer carry material).
- Checks: one valid solid, 160 x 160 x 135; 0 mm3 interference with the motor/flywheel reference.
- Open: the lid is still not updated and does not match this housing.

### v11 - motor tube, new platform layouts, recessed base
- Request: replace the half-round clamp and pads with a full cylinder.
  Result: clamp, pads, groove and arm holes removed. A full-round sleeve (bore 38, OD 44, Z 36-106) goes through the middle of platform 2. It has a closed 3 mm bottom and a 10 x 20 mm wire window at the side, at the encoder.
- The grey body (can + gearbox, 53 mm from the datasheet drawing) is centred on platform 2: 26.5 mm below and 26.5 mm above. The encoder is 12 mm under that.
- Flywheel reference is now at Z 110-120, on the shaft above the boss.
- Platform 2: windows now run out to a 3 mm rim, with a landing pad kept under each of the 7 posts.
- Platform 3: now the opposite pattern (X cross). Only the ring segments on the 45/135 deg diagonals remain.
- Platform 1: 5 mm thick. A 50 mm centre circle stays full height; the 90 and 270 deg sectors are recessed 2 mm. The lower posts start at Z 3 so they sit on the recessed floor.
- Issue: the 3 sensor notches (0, 120, 240 deg) fall in the cut-away areas of platform 3 and are gone.
  Open: tell me where the 3 sensors should go.
- Issue: the motor drawing gives a 12 mm encoder and 29 + 24 mm grey, not the earlier 15 mm / 32 mm. v11 uses the drawing.
- Open: the lid does not match this housing (no ring, no thread). It is not updated.
- Checks: one valid solid, 160 x 160 x 135; 0 mm3 interference with the motor/flywheel reference.

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

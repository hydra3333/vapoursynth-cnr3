# test_D3_once_only_harness_AB.vpy
#
# D.3 variant: proves LIVE BRANCH-(d) FLOOR-FRESH-START recovery. When the bounded descending
# search finds NO in-window anchor, recovery fresh-starts the floor frame max(0,N-B) (output =
# copyFrame(source[floor]), chroma UNCHANGED, no predecessor) then fills holes ascending to N.
#
#   D.3 N=3 floor-fresh-start golden chain (8-bit YUV420, threshold=255 strength=255 narrow,
#   luma const 128; distinct from D.1 80/176.. and D.2 72/184.. sets):
#     source[0]: Y128 U56  V176 -> output[0] = source[0]        (FLOOR fresh-start, chroma unchanged)
#     source[1]: Y128 U200 V48  -> output[1] = Y128 U144 V111   (hole-1, filled from floor)
#     source[2]: Y128 U168 V80  -> output[2] = Y128 U145 V109   (hole-2, filled from hole-1)
#     source[3]: Y128 U120 V152 -> output[3] = Y128 U144 V113   (TARGET = D.3 GOLDEN)
#
#   The FLOOR-BYTE check (output[0] == source[0] == 56/176 via cache-hit) is the D.3-specific
#   discriminator: it proves the floor was FRESH-STARTED (chroma unchanged), not blended.
#   Target discriminators (output[3]=144/113): passthrough(source[3])=120/152; wrong-pred(source[2])
#   =163/94. Distinct from D.1 147/109 and D.2 148/100.
#
#   CORE-CACHE DEFEAT (R76 mode=0): re-requested/skipped frames re-enter CNR3::getFrame.
#
#   How floor-start is triggered: a COLD request for output[3] (nothing cached below it). Routing:
#   pin output[3]? miss. N==0? no. pin output[2]? MISS (cold) -> recovery. Recovery descending search
#   in [max(0,3-50),2]=[0,2] finds nothing present -> floor=0 fresh-start, holes=[1,2], target=3.
#   (B=50 >> N=3, so the window floor is 0.) This is the reachable no-in-window-anchor case that D.3
#   converts from refusal to floor-fresh-start.
#
#   MODES:
#     mode=floorstart : rendered = filtered[3:4]  -> SINGLE cold request for output[3]. Nothing cached
#                       below -> floor=0 fresh-start, fill holes 1,2, target 3. THE D.3 PROOF.
#     mode=floorbytes : rendered = filtered[3:4] + filtered[0:1] + filtered[1:2] + filtered[2:3]
#                       -> floor-start populates output[0],[1],[2] in cache, then re-request 0,1,2 ->
#                       CACHE-HIT returns floor 56/176 (fresh-start proof) and holes 144/111, 145/109.
#     mode=d2regress  : D.2 exact-anchor k==2 on the D.2 chain (72/184,208/40,176/72,128/144):
#                       rendered = filtered_d2[0:1] + filtered_d2[3:4]  -> anchor present at 0,
#                       branch=RECOVER recover_branch=EXACT-ANCHOR (NOT floor), output[3]=148/100.
#     mode=d1regress  : D.1 single-hole on the D.1 chain (80/176,208/48,176/80):
#                       rendered = filtered_d1[0:1] + filtered_d1[2:3] -> recovered output[2]=147/109.
#     mode=processing : rendered = filtered[0:4] -> 0,1,2,3 sequential. NEGATIVE control: frame 3 has
#                       output[2] present -> PREDECESSOR-PRESENT, not recovery. Sequential D.3 chain
#                       1->144/111, 2->145/109, 3->144/113.
#     mode=passthrough: source bypass (no CNR3).
#
#   PASS = KDT presence is definitive. Floor-start MUST show branch=RECOVER
#   recover_branch=floor-fresh-start floor=0 floor_outcome=computed hole_count=2 holes=[1,2]
#   pin_list_size=3 pin_balance=0. bytes-match without the matching KDT line = INCONCLUSIVE.

import os
import vapoursynth as vs

core = vs.core

MODE_PASSTHROUGH = "passthrough"
MODE_PROCESSING  = "processing"
MODE_FLOORSTART  = "floorstart"
MODE_FLOORBYTES  = "floorbytes"
MODE_D2REGRESS   = "d2regress"
MODE_D1REGRESS   = "d1regress"
MODE_DEFAULT     = MODE_PROCESSING
VALID_MODES = [MODE_PASSTHROUGH, MODE_PROCESSING, MODE_FLOORSTART, MODE_FLOORBYTES, MODE_D2REGRESS, MODE_D1REGRESS]

run_mode = str(globals().get("mode", MODE_DEFAULT)).lower()
if run_mode not in VALID_MODES:
    raise ValueError(f"'mode' argument '{run_mode}' must instead be one of '{VALID_MODES}'")

W = 16
H = 16

def cc(u, v, length=1):
    return core.std.BlankClip(width=W, height=H, format=vs.YUV420P8, length=length, color=[128, u, v], keep=True)

# --- D.3 floor-fresh-start chain frames 0..3 ---
d3_0 = cc(56, 176)
d3_1 = cc(200, 48)
d3_2 = cc(168, 80)
d3_3 = cc(120, 152)
d3_source = core.std.Splice([d3_0, d3_1, d3_2, d3_3], mismatch=False)
d3_source = core.std.SetFieldBased(d3_source, 0)

# --- D.2 chain (exact-anchor k==2 regression) ---
d2_0 = cc(72, 184); d2_1 = cc(208, 40); d2_2 = cc(176, 72); d2_3 = cc(128, 144)
d2_source = core.std.Splice([d2_0, d2_1, d2_2, d2_3], mismatch=False)
d2_source = core.std.SetFieldBased(d2_source, 0)

# --- D.1 chain (single-hole k==1 regression) ---
d1_0 = cc(80, 176); d1_1 = cc(208, 48); d1_2 = cc(176, 80)
d1_source = core.std.Splice([d1_0, d1_1, d1_2], mismatch=False)
d1_source = core.std.SetFieldBased(d1_source, 0)

if run_mode == MODE_PASSTHROUGH:
    rendered = d3_source
elif run_mode == MODE_D2REGRESS:
    filtered_d2 = core.cnr3.CNR3(d2_source)
    filtered_d2.std.SetVideoCache(mode=0)
    rendered = filtered_d2[0:1] + filtered_d2[3:4]          # exact-anchor k==2 (anchor 0, holes 1,2)
elif run_mode == MODE_D1REGRESS:
    filtered_d1 = core.cnr3.CNR3(d1_source)
    filtered_d1.std.SetVideoCache(mode=0)
    rendered = filtered_d1[0:1] + filtered_d1[2:3]          # single-hole k==1 (anchor 0, hole 1)
else:
    filtered = core.cnr3.CNR3(d3_source)
    filtered.std.SetVideoCache(mode=0)                       # defeat core cache on the CNR3 node
    if run_mode == MODE_PROCESSING:
        rendered = filtered[0:4]                              # 0,1,2,3 sequential: neg control
    elif run_mode == MODE_FLOORSTART:
        rendered = filtered[3:4]                              # COLD output[3] -> floor=0 fresh-start
    elif run_mode == MODE_FLOORBYTES:
        rendered = filtered[3:4] + filtered[0:1] + filtered[1:2] + filtered[2:3]  # floor-start, then cache-hit 0,1,2

rendered.set_output()

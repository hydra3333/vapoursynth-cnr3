# test_P11C4_branch_d_recovery_scene_detection.vpy
#
# CNR3 P.11C.4 -- LIVE BRANCH-(d) RECOVERY SCENE-DETECTION PROOF.   [designer-owned]
# Adapted from test_D3_once_only_harness_AB.vpy (proven floor-fresh-start recovery trigger).
#
# Proves that scene detection runs PER HOLE and at the TARGET during branch-d recovery, that a
# detected cut resets + stores-as-checkpoint, and that the floor stays not-applicable.
#
# Geometry: 16x16 luma / 8x8 chroma grid (64 samples) / 8-bit 4:2:0, luma-only (scene_chroma=false).
# Live default scdthr=10.0 -> derived threshold = round(10*16*16*219/100) = 5606.
# Detector: per chroma-grid sample, ds_luma=(2x2 box+2)>>2; diff=|ds_cur-ds_pred|<<2; early-stop >5606.
# CNR3 copies luma, so detection's (source[N] vs output[N-1]) luma == (source[N] vs source[N-1]).
#
# RECOVERY TRIGGER (from D3): mode renders filtered[3:4] with SetVideoCache(mode=0) -> single COLD
# request for output[3] -> bounded search [0,2] finds nothing -> FLOOR=0 fresh-start, holes=[1,2],
# target=3. Frames COMPUTED (detection applicable): holes 1,2 then target 3. Floor=0 not-applicable.
#
# CUT construction: a cut frame has UNIFORM luma 255 (vs its ~100-valued predecessor -> ds-delta ~154
# at all 64 samples -> early-stop at sample 10 -> diff_total ~6160, detected=1). Uniform luma keeps the
# output planes constant so the constant-plane checker works.
#
# CHROMA (blend-vs-reset byte discriminator; differing pred/cur so blend != reset):
#   default floor chroma U=100 V=120; frame source chroma U=160 V=80.
#   *** D-cut-at-hole TARGET degeneracy fix: after hole2 resets to 160/80, the target f3 source chroma
#       is overridden to 100/120 so the target blend (160/80 vs 100/120) is distinguishable from reset.
#
#   NO-CUT frame output chroma = recursive BLEND (response-table dependent; first-run-observed golden).
#   CUT frame output chroma = current source chroma EXACTLY (reset; known golden).
#
# MODES:
#   dcontrol     : filtered_ctrl[3:4]  -> floor0 + holes[1,2] + target3, NO cut anywhere.
#   dcutathole   : filtered_hole[3:4]  -> cut at hole 2 (frame 2). hole2 resets+checkpoints.
#   dcutattarget : filtered_tgt[3:4]   -> cut at target (frame 3). target resets+checkpoint(expected).
#   passthrough  : source bypass.
#
# PASS = KDT presence is definitive (recover_branch=floor-fresh-start + the per-hole/target scene fields);
# bytes-match WITHOUT the matching KDT line = INCONCLUSIVE.

import os
import vapoursynth as vs
core = vs.core

MODE_PASSTHROUGH   = "passthrough"
MODE_DCONTROL      = "dcontrol"
MODE_DCUTATHOLE    = "dcutathole"
MODE_DCUTATTARGET  = "dcutattarget"
MODE_DEFAULT       = MODE_DCONTROL
VALID_MODES = [MODE_PASSTHROUGH, MODE_DCONTROL, MODE_DCUTATHOLE, MODE_DCUTATTARGET]

run_mode = str(globals().get("mode", MODE_DEFAULT)).lower()
if run_mode not in VALID_MODES:
    raise ValueError(f"'mode' argument '{run_mode}' must be one of '{VALID_MODES}'")

W = 16
H = 16

def cc(y, u, v, length=1):
    # constant-plane clip, keep=True so each is an independent owned frame
    return core.std.BlankClip(width=W, height=H, format=vs.YUV420P8, length=length,
                              color=[y, u, v], keep=True)

# ---- D-control chain: no cut. luma +1 each step; chroma 160/80 (floor chroma 100/120). ----
# floor frame 0 chroma is copied unchanged at fresh-start, so set source[0] chroma to the floor vector.
ctrl_0 = cc(100, 100, 120)   # floor: output[0] = source[0] (fresh-start copy)
ctrl_1 = cc(101, 160,  80)   # hole1: luma 101 vs out0 100 -> ds-delta 1 -> NO-CUT
ctrl_2 = cc(102, 160,  80)   # hole2: luma 102 vs out1 101 -> NO-CUT
ctrl_3 = cc(103, 160,  80)   # target: luma 103 vs out2 102 -> NO-CUT
ctrl_source = core.std.Splice([ctrl_0, ctrl_1, ctrl_2, ctrl_3], mismatch=False)
ctrl_source = core.std.SetFieldBased(ctrl_source, 0)

# ---- D-cut-at-hole chain: cut at frame 2. ----
# floor 100/100/120; hole1 101/160/80 (no-cut); hole2 CUT uniform luma 255 (vs out1 luma 101) chroma 160/80
# -> reset to 160/80; target f3 luma == out2 luma (255) -> delta0 NO-CUT, chroma OVERRIDDEN to 100/120
# (degeneracy fix: out2 chroma after reset == 160/80, so target source must differ to make blend != reset).
hole_0 = cc(100, 100, 120)   # floor
hole_1 = cc(101, 160,  80)   # hole1 NO-CUT
hole_2 = cc(255, 160,  80)   # hole2 CUT (uniform luma 255) -> reset to 160/80
hole_3 = cc(255, 100, 120)   # target: luma 255 == out2 luma -> delta0 NO-CUT; chroma 100/120 (fix)
hole_source = core.std.Splice([hole_0, hole_1, hole_2, hole_3], mismatch=False)
hole_source = core.std.SetFieldBased(hole_source, 0)

# ---- D-cut-at-target chain: cut at frame 3. ----
# floor 100/100/120; holes 1,2 no-cut (luma 101,102); target CUT uniform luma 255 (vs out2 luma 102) chroma 160/80
tgt_0 = cc(100, 100, 120)    # floor
tgt_1 = cc(101, 160,  80)    # hole1 NO-CUT
tgt_2 = cc(102, 160,  80)    # hole2 NO-CUT
tgt_3 = cc(255, 160,  80)    # target CUT (uniform luma 255) -> reset to 160/80
tgt_source = core.std.Splice([tgt_0, tgt_1, tgt_2, tgt_3], mismatch=False)
tgt_source = core.std.SetFieldBased(tgt_source, 0)

if run_mode == MODE_PASSTHROUGH:
    rendered = ctrl_source
else:
    if run_mode == MODE_DCONTROL:
        src = ctrl_source
    elif run_mode == MODE_DCUTATHOLE:
        src = hole_source
    elif run_mode == MODE_DCUTATTARGET:
        src = tgt_source
    filtered = core.cnr3.CNR3(src)
    filtered.std.SetVideoCache(mode=0)   # defeat core cache (in-place side effect; SetVideoCache returns None, do NOT assign)
    rendered = filtered[3:4]                # COLD output[3] -> floor=0 fresh-start, holes=[1,2], target=3

rendered.set_output()

# ============================================================================
# EXPECTED (assert from stderr [KDT] recovery line; threshold=5606):
#
#   ALL modes: branch=RECOVER recover_branch=floor-fresh-start floor=0 floor_outcome=computed
#              hole_count=2 holes=[1,2] floor_scene_change_not_applicable=1
#
#   dcontrol:
#     hole=1 ... hole_scene_change_detected=0 hole_scene_change_diff_total=256 hole_scene_change_samples_examined=64
#                hole_recursive_chroma_blend_used=1 hole_store_as_checkpoint=0 hole_resulting_slot_is_checkpoint=0
#     hole=2 ... (same shape as hole=1: detected=0, 256/64, blend, store=0)
#     target ... target_scene_change_detected=0 target_store_as_checkpoint=0 target_resulting_slot_is_checkpoint_expected=0
#
#   dcutathole:
#     hole=1 ... detected=0 256/64 blend store=0
#     hole=2 ... hole_scene_change_detected=1 hole_scene_change_diff_total=6160 hole_scene_change_samples_examined=10
#                hole_scene_change_reset_output_used=1 hole_recursive_chroma_blend_used=0
#                hole_store_as_checkpoint=1 hole_resulting_slot_is_checkpoint=1 hole_checkpoint_promoted=<0|1>
#     target ... target_scene_change_detected=0 (diff_total=0 samples=64) store=0
#     BYTE (rendered y4m idx0 = target3): output = blend(pred out2=160/80, cur 100/120) -> first-run observed,
#          must be uniform, between, and != 100/120 (not reset). hole2 reset byte proven in dcutathole via
#          a floorbytes-style re-request if desired (optional; KDT already proves reset).
#
#   dcutattarget:
#     hole=1, hole=2 ... detected=0 256/64 blend store=0
#     target ... target_scene_change_detected=1 target_scene_change_diff_total=6120
#                target_scene_change_samples_examined=10 target_scene_change_reset_output_used=1
#                target_recursive_chroma_blend_used=0 target_store_as_checkpoint=1
#                target_resulting_slot_is_checkpoint_expected=1
#     BYTE (rendered y4m idx0 = target3): output = current source chroma EXACTLY -> Y=255 U=160 V=80 (reset).
# ============================================================================

#!/usr/bin/env vspipe
# ============================================================================
# CNR3 P.11C.3 -- BRANCH-C SCENE-DETECTION LIVE PROOF (.vpy)   [designer-owned]
# ----------------------------------------------------------------------------
# Proves LIVE branch-c (predecessor-present) scene-change behaviour that the
# cache-core selftest cannot:
#   C-control: predecessor present, NO cut  -> recursive blend used.
#   C-cut:     predecessor present, hard cut -> reset copy (output chroma == current source).
#
# Proven at the LIVE DEFAULT scdthr=10.0 -- NO threshold hook, NO plugin parameter.
#   Frame: 8x8 luma / 4x4 chroma / 8-bit / 4:2:0, luma-only detection (scene_chroma=false).
#   Derived threshold = round(10.0 * 8 * 8 * 219 / 100.0 * 255/255) = 1402.
#
# DETECTOR MODEL (verified against source): the detector compares DOWNSAMPLED luma.
# Each 4x4 chroma-grid sample's luma = (2x2 box sum + 2) >> 2 over its luma block.
# Per chroma sample: |ds_cur - ds_pred| << (subw+subh)=<<2 ; early-stop once total > 1402.
#
# GOLDENS (verified):
#   C-control: every luma = pred+1 -> ds diff 1 per sample -> 1<<2=4 -> 16*4 = 64
#              diff_total=64, samples_examined=16, detected=0
#   C-cut:     luma blocks for the FIRST TWO chroma samples go pred=0 -> cur=255
#              -> ds diff 255 -> 255<<2 = 1020 per sample
#              sample0 cum=1020 (<=1402, continue); sample1 cum=2040 (>1402, STOP)
#              diff_total=2040, samples_examined=2, detected=1
#
# The 8x8 frame size is LOAD-BEARING: threshold=1402 derives from W*H. Do not resize
# without recomputing the golden (see CNR3_P11C_3_Designer_Acceptance_NoHook).
#
# RUN (two separate invocations so each case's [KDT] N=1 line is unambiguous):
#   set CNR3_P11C3_CASE=control & vspipe -r 1 --container y4m test_P11C3_branch_c_scene_detection.vpy NUL
#   set CNR3_P11C3_CASE=cut     & vspipe -r 1 --container y4m test_P11C3_branch_c_scene_detection.vpy NUL
# Capture stderr for the [KDT] lines. Plugin must be built with the keystone dev-trace on.
# ============================================================================

import os
import numpy as np
import vapoursynth as vs
core = vs.core

W, H = 8, 8                    # full-frame luma; 4:2:0 -> 4x4 chroma grid (16 samples)

# 8-bit pixel values
PRED_Y = 100                   # predecessor (frame 0) luma baseline
PRED_U, PRED_V = 100, 120      # predecessor chroma
CUR_U,  CUR_V  = 160,  80      # current (frame 1) chroma -- DIFFERENT from predecessor
                               # so the blend-vs-reset byte check can actually fail.

def _blank(y, u, v, length=1):
    return core.std.BlankClip(format=vs.YUV420P8, width=W, height=H,
                              length=length, color=[y, u, v])

def _set_luma_rect(clip, x, y, w, h, value):
    """Return clip with luma plane rectangle [x:x+w, y:y+h] set to value (chroma untouched)."""
    def _modify(n, f):
        fout = f.copy()
        luma = np.asarray(fout[0])         # (H, W) luma plane, writable
        luma[y:y+h, x:x+w] = value
        return fout
    return core.std.ModifyFrame(clip=clip, clips=clip, selector=_modify)

CASE = os.environ.get("CNR3_P11C3_CASE", "control")

if CASE == "control":
    # predecessor: flat
    pred = _blank(PRED_Y, PRED_U, PRED_V)
    # current: luma = PRED_Y + 1 everywhere (every chroma-sample sees ds diff 1), chroma = current
    cur  = _blank(PRED_Y + 1, CUR_U, CUR_V)
    src  = core.std.Splice([pred, cur], mismatch=False)

elif CASE == "cut":
    # The first TWO raster-order chroma samples (chroma (0,0) and (1,0)) map to luma
    # cols 0..3, rows 0..1 (two horizontally-adjacent 2x2 blocks).
    # predecessor: luma 100 everywhere, EXCEPT those blocks set to 0.
    pred = _blank(PRED_Y, PRED_U, PRED_V)
    pred = _set_luma_rect(pred, x=0, y=0, w=4, h=2, value=0)
    # current: luma 100 everywhere, EXCEPT those blocks set to 255; chroma = current.
    cur  = _blank(PRED_Y, CUR_U, CUR_V)
    cur  = _set_luma_rect(cur, x=0, y=0, w=4, h=2, value=255)
    src  = core.std.Splice([pred, cur], mismatch=False)

else:
    raise ValueError("set CNR3_P11C3_CASE=control or cut")

out = core.cnr3.CNR3(src)
out.set_output()

# ============================================================================
# EXPECTED (assert from stderr [KDT] N=1 branch=PREDECESSOR-PRESENT-COMPUTE):
#
# C-control:
#   scene_change_threshold=1402 scene_change_diff_total=64 scene_change_samples_examined=16
#   scene_change_detected=0 scene_change_reset_output_used=0 recursive_chroma_blend_used=1
#   store_as_checkpoint=0 resulting_slot_is_checkpoint_expected=0
#   BYTE (frame 1 output chroma): a BLEND of predecessor & current ->
#     strictly 100 < outU < 160  AND  80 < outV < 120  (i.e. != current; proves blend, not reset).
#     [non-degeneracy guard: if outU==CUR_U the blend collapsed -> vector bad, investigate.]
#
# C-cut:
#   scene_change_threshold=1402 scene_change_diff_total=2040 scene_change_samples_examined=2
#   scene_change_detected=1 scene_change_reset_output_used=1 recursive_chroma_blend_used=0
#   store_as_checkpoint=1 resulting_slot_is_checkpoint_expected=1
#   BYTE (frame 1 output chroma): == current EXACTLY -> outU==160 AND outV==80
#     AND != predecessor (100/120). Proves reset copy, not blend.
#
# Frame N=1 is non-grid (CHECKPOINT_INTERVAL) so C-control store_as_checkpoint=0 is expected;
# confirm the interval if a different N is used.
# ============================================================================

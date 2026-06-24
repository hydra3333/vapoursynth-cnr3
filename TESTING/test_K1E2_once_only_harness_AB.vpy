# test_K1E2_once_only_harness_AB.vpy
#
# K.1E.2 variant of the K.1D A/B harness. DIFFERENCE FROM K.1D:
#   K.1D proved frame-0 fresh-start (branch a) with N>0 a clean not-yet-implemented failure.
#   K.1E.2 proves the first RECURSIVE compute: frame 1 = P.11B( source[1], cached output[0] ),
#   i.e. the live predecessor-present compute path (branch c) for N==1, with N>1 still a clean
#   not-yet-implemented refusal.
#
#   Because frame 1 is a COMPUTED blend (not a copy), it cannot be proven by an A/B compare of
#   two runs the way fresh-start frame 0 is. It is proven by a KNOWN-ANSWER byte check against a
#   golden frame computed by hand from P.11B arithmetic + the K.1E.2 proof/default response
#   tables + cached output[0] as predecessor. For the golden bytes to be exact and hand-checkable,
#   this harness drives a SYNTHETIC DETERMINISTIC clip with constant planes, NOT a real video.
#
#   SYNTHETIC SOURCE (8-bit YUV420, constant planes per frame):
#     source frame 0:  Y=128  U=96   V=160      -> output[0] == source[0] (K.1D fresh-start)
#     source frame 1:  Y=128  U=224  V=32       (large chroma diff: strong three-way discriminator)
#   GOLDEN EXPECTED (verified independently against the committed response-table + P.11B source):
#     output frame 1:  Y=128  U=161  V=95
#   Distinct from BOTH predecessor (U=96/V=160) AND source[1] (U=224/V=32), so a predecessor-copy
#   bug, a source-passthrough bug, and a P.11B-bypass bug all FAIL the byte check.
#
#   The .bat selects which exact output frame the plugin's getFrame is asked for via
#   vspipe --start N --end N :
#     --start 0 --end 0  -> getFrame for frame 0  (real fresh-start output; A/B vs passthrough)
#     --start 1 --end 1  -> getFrame for frame 1  (predecessor-present compute; known-answer check)
#     --start 2 --end 2  -> getFrame for frame 2  (must FAIL CLEANLY: N>1 not-yet-implemented)

import os
from typing import Optional, Tuple
import vapoursynth as vs

core = vs.core

# --- CONFIGURATION CONSTANTS FOR MAINTAINERS (must be lowercase arg values) ---
MODE_PASSTHROUGH = "passthrough"
MODE_PROCESSING = "processing"
MODE_DEFAULT = MODE_PROCESSING
VALID_MODES = [MODE_PASSTHROUGH, MODE_PROCESSING]

# mode: processing (CNR3 applied) or passthrough (CNR3 bypassed, source returned)
run_mode = str(globals().get("mode", MODE_DEFAULT)).lower()
if run_mode not in VALID_MODES:
    raise ValueError(f"'mode' argument '{run_mode}' must instead be one of '{VALID_MODES}'")

# ==============================================================================
# SYNTHETIC DETERMINISTIC SOURCE
# ------------------------------------------------------------------------------
# A two-frame 8-bit YUV420P8 clip with constant planes per frame, built so that the
# K.1E.2 frame-1 blend has a single exact known answer. Constant planes mean every
# active chroma sample shares one value, so the byte check is a single (Y,U,V) triple
# per frame regardless of dimensions.
#
# Dimensions: small but valid for YUV420 (even width/height). 16x16 is ample; the plugin
# processes whatever it is handed. (BlankClip needs no real video file, so the harness has
# no external-source dependency and the bytes are fully under our control.)
#
# GOLDEN VALUES (do not edit without recomputing the expected output against the committed
# response-table + P.11B source):
#   frame 0: Y=128 U=96  V=160
#   frame 1: Y=128 U=224 V=32
# ==============================================================================

SYNTH_WIDTH = 16
SYNTH_HEIGHT = 16

# Frame 0: predecessor source. output[0] == source[0] by K.1D fresh-start.
f0 = core.std.BlankClip(
    width=SYNTH_WIDTH, height=SYNTH_HEIGHT, format=vs.YUV420P8,
    length=1, color=[128, 96, 160], keep=True,
)
# Frame 1: current source, large chroma diff from the predecessor.
f1 = core.std.BlankClip(
    width=SYNTH_WIDTH, height=SYNTH_HEIGHT, format=vs.YUV420P8,
    length=1, color=[128, 224, 32], keep=True,
)
# Frame 2: present only so getFrame can be asked for frame 2 (the N>1 clean-refusal run).
# Its content is irrelevant; frame 2 must be refused before any compute, so it is never read.
f2 = core.std.BlankClip(
    width=SYNTH_WIDTH, height=SYNTH_HEIGHT, format=vs.YUV420P8,
    length=1, color=[128, 128, 128], keep=True,
)

# Splice into a single 3-frame clip: [frame0, frame1, frame2].
clip = core.std.Splice([f0, f1, f2], mismatch=False)

# Mark progressive so the plugin's single-instance path is exercised (no field split here;
# K.1E.2 is the progressive predecessor-present proof, matching the K.1D harness's primary path).
clip = core.std.SetFieldBased(clip, 0)

# ==============================================================================
# APPLY CNR3 (or bypass), exactly as the K.1D harness mode switch does.
# Progressive single-instance path only; the field-split sibling path is a later (interlaced)
# pass and is intentionally not exercised by this synthetic proof.
# ==============================================================================
if run_mode == MODE_PASSTHROUGH:
    clip_denoised = clip
else:
    clip_denoised = core.cnr3.CNR3(clip)

# Output the (mode-selected) clip. The .bat requests a single exact frame via --start/--end.
clip_denoised.set_output()

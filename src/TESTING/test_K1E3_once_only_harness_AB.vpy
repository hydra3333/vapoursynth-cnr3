# test_K1E3_once_only_harness_AB.vpy
#
# K.1E.3 variant: proves the RECURSIVE FILTERED-PREDECESSOR DISTINCTION at N=2.
#
#   K.1E.2 proved frame 1 cleans against cached output[0]. But at N=1, output[0]==source[0]
#   (fresh start), so "filtered output vs source" was byte-indistinguishable. K.1E.3 proves
#   frame 2 cleans against cached FILTERED output[1] -- which differs from source[1] -- so the
#   R-ARCH-06 "predecessor is filtered output, NOT source[N-1]" distinction becomes byte-observable.
#
#   SYNTHETIC SOURCE (8-bit YUV420, constant planes per frame):
#     source[0]:  Y=128 U=96   V=160   -> output[0] = source[0]      (fresh start)
#     source[1]:  Y=128 U=224  V=32    -> output[1] = Y128 U161 V95  (K.1E.2 golden)
#     source[2]:  Y=128 U=192  V=64    -> output[2] = Y128 U163 V93  (K.1E.3 golden)
#   GOLDEN EXPECTED output[2] (verified independently against response-table + P.11B source):
#     output[2]:  Y=128 U=163 V=93
#
#   DISCRIMINATOR (this is the whole proof):
#     correct  pred=filtered output[1]=161/95  -> output[2] = 163/93   <- golden
#     wrong    pred=source[1]        =224/32   -> output[2] = 222/34   <- R-ARCH-06 bug, must NOT match
#     passthrough source[2]          =192/64   -> output[2] = 192/64   <- must NOT match
#   All three are byte-distinct on BOTH chroma planes (correct vs wrong differ by 59 each), so
#   only genuine use of cached filtered output[1] hits 163/93.
#
#   RECURSION REQUIREMENT: output[2]'s predecessor output[1] is itself a recursive compute whose
#   predecessor output[0] must exist. So the frame-2 proof MUST request frames 0..2 in ONE process
#   (--start 0 --end 2), so the whole chain output[0]->output[1]->output[2] is built in the same
#   filter instance. An isolated --start 2 cannot work, by design.
#
#   Frame 3 exists only so getFrame can be asked for frame 3 (the N>2 clean-refusal run). Its
#   content is irrelevant; frame 3 is refused before any compute, so it is never read.

import os
import vapoursynth as vs

core = vs.core

MODE_PASSTHROUGH = "passthrough"
MODE_PROCESSING = "processing"
MODE_DEFAULT = MODE_PROCESSING
VALID_MODES = [MODE_PASSTHROUGH, MODE_PROCESSING]

run_mode = str(globals().get("mode", MODE_DEFAULT)).lower()
if run_mode not in VALID_MODES:
    raise ValueError(f"'mode' argument '{run_mode}' must instead be one of '{VALID_MODES}'")

SYNTH_WIDTH = 16
SYNTH_HEIGHT = 16

# GOLDEN CHAIN (do not edit without recomputing expected outputs against response-table + P.11B):
#   frame 0: Y=128 U=96  V=160
#   frame 1: Y=128 U=224 V=32
#   frame 2: Y=128 U=192 V=64
f0 = core.std.BlankClip(
    width=SYNTH_WIDTH, height=SYNTH_HEIGHT, format=vs.YUV420P8,
    length=1, color=[128, 96, 160], keep=True,
)
f1 = core.std.BlankClip(
    width=SYNTH_WIDTH, height=SYNTH_HEIGHT, format=vs.YUV420P8,
    length=1, color=[128, 224, 32], keep=True,
)
f2 = core.std.BlankClip(
    width=SYNTH_WIDTH, height=SYNTH_HEIGHT, format=vs.YUV420P8,
    length=1, color=[128, 192, 64], keep=True,
)
# Frame 3: filler only, for the N>2 clean-refusal run. Never read (refused before compute).
f3 = core.std.BlankClip(
    width=SYNTH_WIDTH, height=SYNTH_HEIGHT, format=vs.YUV420P8,
    length=1, color=[128, 128, 128], keep=True,
)

# Splice into a single 4-frame clip: [frame0, frame1, frame2, frame3].
clip = core.std.Splice([f0, f1, f2, f3], mismatch=False)
clip = core.std.SetFieldBased(clip, 0)  # progressive single-instance path

if run_mode == MODE_PASSTHROUGH:
    clip_denoised = clip
else:
    clip_denoised = core.cnr3.CNR3(clip)

clip_denoised.set_output()

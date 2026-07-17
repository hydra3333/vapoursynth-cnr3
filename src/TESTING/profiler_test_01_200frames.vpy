# profiler_test_01_200frames.vpy
#
# PURPOSE: a MINIMAL, purely-sequential throughput-profiling harness for CNR3.
#   200 in-order frames (0..199) through one progressive CNR3 instance, with the
#   VapourSynth core cache DEFEATED so every request re-enters CNR3::getFrame and
#   its real per-frame work (pixel blend, plane traversal, cache store/prune,
#   pin/unpin) is what the profiler samples.
#
# WHY THIS IS DELIBERATELY BARE (not the S1..B7 scenario harness):
#   A production encode is overwhelmingly SEQUENTIAL: predecessor N-1 is present,
#   cache-hit, no recovery. The jump/shuffle/cold-seek scenarios exercise the
#   RECOVERY path on purpose, which is exactly what a throughput profile must NOT
#   be dominated by. So there are no jumps, no shuffles, no field-split machinery
#   here: just source -> CNR3 -> 200 sequential frames. This is the production
#   hot path, isolated. Checkpoints still lay down at the interval cadence and the
#   hot zone tracks the wavefront, exactly as in a real encode.
#
# HOW TO RUN (single-thread, deterministic attribution for the first pass):
#   vspipe -r 1 --container y4m profiler_test_01_200frames.vpy NUL
#
# PROFILING NOTES (systems-dev checklist before you trust the numbers):
#   1. DLL: the plugin loaded from the VS plugins folder must be the NORMAL
#      (production-constant) Release build, NOT the tiny-cache diag build. The tiny
#      profile prunes ~10x more often per frame and would inflate the prune path
#      into a false bottleneck. Confirm profile=normal in any KDT/--info output.
#   2. DEV-TRACE: for the truest throughput picture build the DLL with
#      CNR3_KEYSTONE_DEV_TRACE *off*. With it on, every one of these 200 frames
#      emits a KDT fprintf to stderr under mode=0; that stdio time is real in the
#      profile but ABSENT in production, so it would masquerade as algorithm cost.
#      (Keep a separate dev-trace-ON run for COUNTING trigger events, not timing.)
#   3. SYMBOLS: ensure cnr3.pdb sits next to the loaded cnr3.dll (copy it into the
#      plugins folder alongside the DLL) or the call tree resolves to [cnr3.dll].
#   4. -r 1 measures PER-FRAME work (deterministic, additive). Dropping -r 1 to
#      use all cores is a valid SECOND pass, but it measures scaling/contention,
#      not per-frame cost, and is noisier. Start here.

import vapoursynth as vs

core = vs.core

# --- Source: real progressive PAL footage (edit to your clip). ---
#   576p25 vs 576p50 changes frame RATE, not per-frame COST, so the hot-path
#   ratios are identical either way; pick whichever real clip you have.
source_filename = r"D:\TEST\000_Example_576p25.mp4"

FRAME_COUNT = 200  # enough for a steady sampling window; raise for more samples.

clip = core.bs.VideoSource(source_filename)

# Force the progressive single-instance path (matches the other harnesses).
clip = core.std.SetFieldBased(clip, 0)

# Guard: the clip must actually have the frames we intend to profile (fail LOUD).
if clip.num_frames < FRAME_COUNT:
    raise ValueError(
        f"Source has {clip.num_frames} frames but the profile needs {FRAME_COUNT}. "
        f"Use a longer clip or lower FRAME_COUNT."
    )

# One progressive CNR3 instance (production / normal-constant DLL).
clip_denoised = core.cnr3.CNR3(clip)

# Take the first FRAME_COUNT frames, strictly in order (the production hot path).
clip_denoised = clip_denoised[0:FRAME_COUNT]

# Defeat the VS core cache so every request re-enters CNR3::getFrame (K.1F / W.2
# mechanism). In place: returns None, do NOT reassign.
clip_denoised.std.SetVideoCache(mode=0)

clip_denoised.set_output()

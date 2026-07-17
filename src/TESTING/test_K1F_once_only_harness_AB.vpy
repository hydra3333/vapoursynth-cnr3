# test_K1F_once_only_harness_AB.vpy
#
# K.1F variant: proves LIVE DIRECT CACHED-OUTPUT RETURN (branch-b).
#
#   K.1E.3 proved the recursive compute chain output[0]->output[1]->output[2].
#   K.1F proves that when output[N] is ALREADY PRESENT in the per-instance cache, live
#   getFrame returns the cached frame directly: no pixel compute (no P.11B/P.11C), correct
#   ownership, byte-identical output, via the Option-C lifecycle (request source[N] as a
#   trigger so arAllFramesReady fires, return the cached output there).
#
#   SYNTHETIC SOURCE (8-bit YUV420, constant planes) -- SAME golden chain as K.1E.3:
#     source[0]: Y=128 U=96  V=160  -> output[0] = source[0]      (fresh start)
#     source[1]: Y=128 U=224 V=32   -> output[1] = Y128 U161 V95  (K.1E.2 golden)
#     source[2]: Y=128 U=192 V=64   -> output[2] = Y128 U163 V93  (K.1E.3 golden)
#
#   CRITICAL -- DEFEAT THE VAPOURSYNTH CORE CACHE (verified R76 mechanism):
#     CNR3 registers fmUnordered with no no-cache flag, so the VS core inserts a cache node
#     downstream of CNR3. Without overriding it, a re-request of an already-produced frame could
#     be served by the VS CORE cache and NEVER re-enter CNR3::getFrame -- making the cache-hit
#     proof a FALSE PASS. We therefore call std.SetVideoCache(filtered, mode=0) on the CNR3
#     output node: R76 documents mode=0 as "always disables caching". With the node cache
#     disabled, the second request for an already-produced frame re-enters CNR3::getFrame, where
#     CNR3's OWN per-instance output cache satisfies it via branch=CACHE-HIT.
#
#   HOW THE CACHE-HIT IS FORCED:
#     mode=cachehit : rendered = filtered[0:3] + filtered[2:3]   -> output positions 0,1,2,2
#                     positions 0,1,2 compute (populating CNR3's cache); position 3 re-requests
#                     frame 2 -> output[2] already present in CNR3's cache -> branch=CACHE-HIT.
#     mode=repeat0  : rendered = filtered[0:1] + filtered[0:1]   -> output positions 0,0
#                     position 0 fresh-start computes output[0]; position 1 re-requests frame 0 ->
#                     CACHE-HIT, proving present-N dispatch precedes the n==0 fresh-start gate.
#     mode=processing : rendered = filtered (no duplicate) -> positions 0,1,2, all FIRST/uncached,
#                     must COMPUTE (branch != CACHE-HIT): K.1E.3 regression (1->161/95, 2->163/93)
#                     and the negative control (a first request must not take cache-hit).
#     mode=passthrough : source bypass (no CNR3).
#
#   PASS CONDITION (mandatory -- KDT presence is the definitive proof the branch fired):
#     bytes-match ALONE is NOT a pass. The re-requested frame MUST show branch=CACHE-HIT in
#     stderr with pixel_compute=0, p11b_called=0, p11c_called=0, balanced trigger/pin/return
#     ledgers. bytes-match WITHOUT a CACHE-HIT KDT line = core cache intercepted = INCONCLUSIVE.

import os
import vapoursynth as vs

core = vs.core

MODE_PASSTHROUGH = "passthrough"
MODE_PROCESSING  = "processing"
MODE_CACHEHIT    = "cachehit"
MODE_REPEAT0     = "repeat0"
MODE_DEFAULT     = MODE_PROCESSING
VALID_MODES = [MODE_PASSTHROUGH, MODE_PROCESSING, MODE_CACHEHIT, MODE_REPEAT0]

run_mode = str(globals().get("mode", MODE_DEFAULT)).lower()
if run_mode not in VALID_MODES:
    raise ValueError(f"'mode' argument '{run_mode}' must instead be one of '{VALID_MODES}'")

SYNTH_WIDTH = 16
SYNTH_HEIGHT = 16

# GOLDEN CHAIN (identical to K.1E.3; do not edit without recomputing against response-table + P.11B):
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

source = core.std.Splice([f0, f1, f2], mismatch=False)
source = core.std.SetFieldBased(source, 0)  # progressive single-instance path

if run_mode == MODE_PASSTHROUGH:
    rendered = source
else:
    filtered = core.cnr3.CNR3(source)
    # DEFEAT THE CORE CACHE on the CNR3 node so re-requests re-enter CNR3::getFrame (R76 mode=0).
    # IMPORTANT: SetVideoCache mutates the node IN PLACE and returns None -- call it for its
    # side effect on the node; do NOT reassign (reassigning sets filtered=None).
    filtered.std.SetVideoCache(mode=0)

    if run_mode == MODE_PROCESSING:
        rendered = filtered                              # [0,1,2] first/uncached: compute + regression
    elif run_mode == MODE_CACHEHIT:
        rendered = filtered[0:3] + filtered[2:3]          # [0,1,2,2]: position 3 = CACHE-HIT on output[2]
    elif run_mode == MODE_REPEAT0:
        rendered = filtered[0:1] + filtered[0:1]          # [0,0]: position 1 = CACHE-HIT on output[0]

rendered.set_output()

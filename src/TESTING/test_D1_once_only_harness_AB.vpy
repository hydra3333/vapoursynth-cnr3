# test_D1_once_only_harness_AB.vpy
#
# D.1 variant: proves LIVE BRANCH-(d) EXACT-ANCHOR SINGLE-HOLE RECOVERY.
#
#   When output[N] is requested, output[N-1] is ABSENT, and output[N-2] is PRESENT (the anchor),
#   recovery reconstructs the missing predecessor output[N-1] from the anchor, then computes the
#   target output[N] from the now-present output[N-1].
#
#   SYNTHETIC SOURCE (8-bit YUV420, constant planes) -- D.1 golden chain, chosen DISTINCT from the
#   K.1E.3 set so a recovery PASS cannot be a stale-cache false pass:
#     source[0]: Y=128 U=80  V=176  -> output[0] = source[0]        (fresh start = anchor)
#     source[1]: Y=128 U=208 V=48   -> output[1] = Y128 U145 V111   (the HOLE, filled by recovery)
#     source[2]: Y=128 U=176 V=80   -> output[2] = Y128 U147 V109   (the TARGET = D.1 GOLDEN)
#
#   Target discriminators (output[2]); a correct recovery is byte-distinct from every wrong path:
#     correct recovery (prev = filled output[1])  -> 147/109   (PASS value)
#     passthrough (source[2])                       -> 176/80
#     wrong-pred = source[N-1] (R-ARCH-06 break)    -> 206/50
#     skip-hole (prev = anchor output[0] directly)  -> 111/145
#     K.1E.3 chain target                           -> 163/93
#
#   CRITICAL -- DEFEAT THE VAPOURSYNTH CORE CACHE (verified R76 mechanism, same as K.1F):
#     std.SetVideoCache(filtered, mode=0) on the CNR3 node so re-requests / skipped frames re-enter
#     CNR3::getFrame rather than being served by the VS core cache. Without this the recovery proof
#     could be a FALSE PASS (a different cache satisfying the request).
#
#   HOW RECOVERY IS FORCED:
#     mode=recovery : rendered = filtered[0:1] + filtered[2:3]   -> output positions 0, 2 (SKIP 1)
#                     position 0 computes output[0] (fresh-start: the anchor, present).
#                     position 1 requests output[2]: output[1] is ABSENT (never requested),
#                     output[0] is PRESENT (anchor at N-2) -> branch=RECOVER, hole=[1], anchor=0.
#                     output[2] must come back 147/109 via the filled output[1]=145/111.
#
#     mode=processing : rendered = filtered  -> positions 0,1,2 all FIRST/uncached, must COMPUTE.
#                     This is the BRANCH-C regression (predecessor-present): because branch-c's entry
#                     contract changed (now receives an already-recorded predecessor pin from the
#                     A-safe-1 routing), this MUST still produce frame1=145/111-equivalent... NOTE:
#                     this D.1 chain's sequential goldens are 1 -> 145/111, 2 -> 147/109 (NOT the
#                     K.1E.3 161/95, 163/93 -- different source values). It is ALSO the NEGATIVE
#                     CONTROL: frame 2 here is requested with output[1] PRESENT, so it must take
#                     PREDECESSOR-PRESENT (branch-c), NOT recovery.
#
#     mode=cachehit : rendered = filtered[0:3] + filtered[2:3]  -> positions 0,1,2,2.
#                     position 3 re-requests frame 2 -> output[2] already present -> branch=CACHE-HIT.
#                     K.1F cache-hit regression (routing changed, so re-prove): cache-hit returns
#                     147/109 with no compute. (NOTE: 147/109 here, the D.1 chain's output[2].)
#
#     mode=passthrough : source bypass (no CNR3) -> output[2] = source[2] = 176/80.
#
#   PASS CONDITION (mandatory -- KDT presence is the definitive proof the branch fired):
#     For recovery: the target frame MUST show branch=RECOVER in stderr with recover_branch=
#     exact-anchor, anchor=0, hole_count=1, holes=[1], source_requests=[1,2], hole=1 outcome=computed,
#     pin_balance=0, AND p11c_called=0 scene_change_deferred=1 (P.11C deferral, consistent with
#     branch-c). bytes-match (147/109) WITHOUT a RECOVER KDT line = INCONCLUSIVE, not a pass.
#     FAIL-CLOSED ORDERING: the KDT must show FRAME0-FRESH-START for N=0 BEFORE branch=RECOVER N=2
#     anchor=0 -- i.e. the anchor was established present before the recovery for frame 2 ran. If the
#     scheduler did not actually create "N-1 absent, N-2 present", the run must FAIL or be reported
#     INCONCLUSIVE, never pass on bytes alone.

import os
import vapoursynth as vs

core = vs.core

MODE_PASSTHROUGH = "passthrough"
MODE_PROCESSING  = "processing"
MODE_CACHEHIT    = "cachehit"
MODE_RECOVERY    = "recovery"
MODE_DEFAULT     = MODE_PROCESSING
VALID_MODES = [MODE_PASSTHROUGH, MODE_PROCESSING, MODE_CACHEHIT, MODE_RECOVERY]

run_mode = str(globals().get("mode", MODE_DEFAULT)).lower()
if run_mode not in VALID_MODES:
    raise ValueError(f"'mode' argument '{run_mode}' must instead be one of '{VALID_MODES}'")

SYNTH_WIDTH = 16
SYNTH_HEIGHT = 16

# D.1 GOLDEN CHAIN (distinct from K.1E.3; do not edit without recomputing against response-table + P.11B):
f0 = core.std.BlankClip(
    width=SYNTH_WIDTH, height=SYNTH_HEIGHT, format=vs.YUV420P8,
    length=1, color=[128, 80, 176], keep=True,
)
f1 = core.std.BlankClip(
    width=SYNTH_WIDTH, height=SYNTH_HEIGHT, format=vs.YUV420P8,
    length=1, color=[128, 208, 48], keep=True,
)
f2 = core.std.BlankClip(
    width=SYNTH_WIDTH, height=SYNTH_HEIGHT, format=vs.YUV420P8,
    length=1, color=[128, 176, 80], keep=True,
)

source = core.std.Splice([f0, f1, f2], mismatch=False)
source = core.std.SetFieldBased(source, 0)  # progressive single-instance path

if run_mode == MODE_PASSTHROUGH:
    rendered = source
else:
    filtered = core.cnr3.CNR3(source)
    # DEFEAT THE CORE CACHE on the CNR3 node (R76 mode=0). Call for side effect; do NOT reassign.
    filtered.std.SetVideoCache(mode=0)

    if run_mode == MODE_PROCESSING:
        rendered = filtered                              # [0,1,2] first/uncached: compute + branch-c regression + neg control
    elif run_mode == MODE_CACHEHIT:
        rendered = filtered[0:3] + filtered[2:3]          # [0,1,2,2]: position 3 = CACHE-HIT on output[2]
    elif run_mode == MODE_RECOVERY:
        rendered = filtered[0:1] + filtered[2:3]          # [0,2]: SKIP 1 -> output[1] absent, output[0] anchor -> RECOVER

rendered.set_output()

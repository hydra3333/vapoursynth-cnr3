# test_D2_once_only_harness_AB.vpy
#
# D.2 variant: proves LIVE BRANCH-(d) exact-anchor MULTI-hole recovery (k=2) AND the
# bounded-window REFUSAL (no in-window anchor).
#
#   D.2 multi-hole golden chain (8-bit YUV420, threshold=255 strength=255 narrow, luma const 128;
#   distinct from the D.1 and K.1E.3 sets so a recovery PASS cannot be a stale-cache false pass):
#     source[0]: Y128 U72  V184 -> output[0] = source[0]        (fresh start = anchor at N-3)
#     source[1]: Y128 U208 V40  -> output[1] = Y128 U148 V96    (hole-1, filled from anchor)
#     source[2]: Y128 U176 V72  -> output[2] = Y128 U149 V95    (hole-2, filled from hole-1)
#     source[3]: Y128 U128 V144 -> output[3] = Y128 U148 V100   (TARGET = D.2 GOLDEN)
#
#   Target discriminators (output[3]=148/100): passthrough(source[3])=128/144; wrong-pred(source[2])
#   =163/86; skip-hole2(prev=output[1])=147/101 (ONLY 1 LSB off -> the target byte is NOT the primary
#   proof). PRIMARY proof = KDT mechanism (hole_count=2, both computed, pin_list_size=3, pin_balance=0)
#   + the FILLED HOLE BYTES via cache-hit follow-up (148/96, 149/95, well separated).
#
#   CRITICAL -- DEFEAT THE VS CORE CACHE (R76 mode=0, same as D.1/K.1F) so skipped/re-requested frames
#   re-enter CNR3::getFrame instead of being served by the core cache.
#
#   BOUNDED RECOVERY BACK-RADIUS B = 50 (compile-time). The in-window search for target N is
#   [max(0,N-B), N-1]. For RUN C, N=52 -> window [2,51]; output[0] is present but at 0, OUTSIDE the
#   window -> planner finds NO in-window anchor -> clean refusal reason=no-in-window-anchor. The
#   bounded-window-exceeded NATURE is proven BY CONSTRUCTION (we know output[0] exists), not by a
#   code-emitted "exceeded" label.
#
#   MODES:
#     mode=recovery   : rendered = filtered[0:1] + filtered[3:4]   -> output positions 0, 3 (SKIP 1,2)
#                       position 0 computes output[0] (fresh-start anchor).
#                       position 1 requests output[3]: output[1] AND output[2] ABSENT, output[0]
#                       PRESENT (anchor at N-3) -> branch=RECOVER hole_count=2 holes=[1,2] -> 148/100.
#     mode=holebytes  : rendered = filtered[0:1] + filtered[3:4] + filtered[1:2] + filtered[2:3]
#                       -> positions 0,3 (recovery fills holes 1,2 into cache), then positions 1,2
#                       re-requested -> CACHE-HIT returning the FILLED hole bytes 148/96 and 149/95.
#     mode=refusal    : rendered = filtered[0:1] + filtered[52:53]  -> output positions 0, 52 (SKIP 1..51)
#                       position 0 fresh-start. position 1 requests output[52]: nearest anchor=0 is 52
#                       back > B=50 -> NO in-window anchor -> clean refusal (reason=no-in-window-anchor).
#     mode=processing : rendered = filtered[0:4]  -> 0,1,2,3 all first/uncached compute. NEGATIVE
#                       control: frame 3 has output[2] PRESENT -> PREDECESSOR-PRESENT, NOT recovery.
#                       Sequential goldens for this D.2 chain: 1->148/96, 2->149/95, 3->148/100.
#     mode=d1regress  : rendered = filtered_d1[0:1] + filtered_d1[2:3]  -> the D.1 single-hole case on
#                       the D.1 chain (80/176,208/48,176/80) -> recovered output[2]=147/109 (k==1).
#     mode=passthrough: source bypass (no CNR3).
#
#   PASS = KDT presence is definitive. Recovery MUST show branch=RECOVER ... hole_count=2 holes=[1,2]
#   pin_list_size=3 pin_balance=0; refusal MUST show REFUSED branch=no-in-window-anchor. bytes-match
#   without the matching KDT line = INCONCLUSIVE.

import os
import vapoursynth as vs

core = vs.core

MODE_PASSTHROUGH = "passthrough"
MODE_PROCESSING  = "processing"
MODE_RECOVERY    = "recovery"
MODE_HOLEBYTES   = "holebytes"
MODE_REFUSAL     = "refusal"
MODE_D1REGRESS   = "d1regress"
MODE_DEFAULT     = MODE_PROCESSING
VALID_MODES = [MODE_PASSTHROUGH, MODE_PROCESSING, MODE_RECOVERY, MODE_HOLEBYTES, MODE_REFUSAL, MODE_D1REGRESS]

run_mode = str(globals().get("mode", MODE_DEFAULT)).lower()
if run_mode not in VALID_MODES:
    raise ValueError(f"'mode' argument '{run_mode}' must instead be one of '{VALID_MODES}'")

W = 16
H = 16

def cc(u, v):
    return core.std.BlankClip(width=W, height=H, format=vs.YUV420P8, length=1, color=[128, u, v], keep=True)

# --- D.2 multi-hole chain frames 0..3 ---
d2_0 = cc(72, 184)
d2_1 = cc(208, 40)
d2_2 = cc(176, 72)
d2_3 = cc(128, 144)

# --- filler frames 4..52 for the refusal run (need frame index 52 to exist). Values are irrelevant;
#     they are never rendered as outputs. Use a distinct constant so nothing aliases the chain. ---
filler = core.std.BlankClip(width=W, height=H, format=vs.YUV420P8, length=49, color=[128, 100, 100], keep=True)  # indices 4..52

d2_source = core.std.Splice([d2_0, d2_1, d2_2, d2_3, filler], mismatch=False)
d2_source = core.std.SetFieldBased(d2_source, 0)

# --- D.1 chain (for the k==1 regression) ---
d1_0 = cc(80, 176)
d1_1 = cc(208, 48)
d1_2 = cc(176, 80)
d1_source = core.std.Splice([d1_0, d1_1, d1_2], mismatch=False)
d1_source = core.std.SetFieldBased(d1_source, 0)

if run_mode == MODE_PASSTHROUGH:
    rendered = d2_source
elif run_mode == MODE_D1REGRESS:
    filtered_d1 = core.cnr3.CNR3(d1_source)
    filtered_d1.std.SetVideoCache(mode=0)
    rendered = filtered_d1[0:1] + filtered_d1[2:3]          # [0,2] skip 1 -> D.1 single-hole recovery
else:
    filtered = core.cnr3.CNR3(d2_source)
    filtered.std.SetVideoCache(mode=0)                       # defeat core cache on the CNR3 node
    if run_mode == MODE_PROCESSING:
        rendered = filtered[0:4]                              # [0,1,2,3] first/uncached: compute + neg control
    elif run_mode == MODE_RECOVERY:
        rendered = filtered[0:1] + filtered[3:4]              # [0,3] skip 1,2 -> multi-hole RECOVER on output[3]
    elif run_mode == MODE_HOLEBYTES:
        rendered = filtered[0:1] + filtered[3:4] + filtered[1:2] + filtered[2:3]  # recover, then cache-hit holes 1,2
    elif run_mode == MODE_REFUSAL:
        rendered = filtered[0:1] + filtered[52:53]            # [0,52] skip 1..51 -> anchor 0 outside window -> refuse

rendered.set_output()

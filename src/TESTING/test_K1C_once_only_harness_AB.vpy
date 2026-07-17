# K1C_once_only_harness_AB.vpy

import os
from typing import Callable, Optional, Tuple, List
from enum import Enum
import random  # for the in-zone request shuffle

import vapoursynth as vs
core = vs.core

# --- CONFIGURATION CONSTANTS FOR MAINTAINERS (must be lowercase) ---
MODE_PASSTHROUGH = "passthrough"
MODE_PROCESSING = "processing"
MODE_DEFAULT = MODE_PROCESSING
VALID_MODES = [ MODE_PASSTHROUGH, MODE_PROCESSING ]

# --- CONFIGURATION LOGIC ---
# argument 'scenario' processing is defined at the bottom of this vpy where it is processed

# mode for either processing or passthrough
run_mode = str(globals().get("mode", MODE_DEFAULT)).lower()
if run_mode not in VALID_MODES:
    raise ValueError(f"'mode' argument '{run_mode}' must instead be one of '{VALID_MODES}'")

# --- Known Sources ---
## r"D:\TEST\000_Example_576i.mp4"     # interlaced  (LATER second pass)
## r"D:\TEST\000_Example_576p25.mp4"   # progressive (PRIMARY recovery-test clip)
## r"D:\TEST\000_Example_576p50.mp4"   # progressive (PRIMARY recovery-test clip)
#
raw_sourcefile  = str(globals().get("sourcefile", ""))
if not os.path.isfile(raw_sourcefile ):
    raise ValueError(f"'sourcefile' argument does not exist on disk: '{raw_sourcefile }'")

# This queries Windows and forces the exact case of the path/filename stored on the disk drive
# so that bestsource and other source filters work correctly
source_filename = os.path.realpath(os.path.normpath(raw_sourcefile))

# Open the video clip using the true system-cased path
clip = core.bs.VideoSource(source_filename)

# Original video length — authoritative, CNR3-independent target for the end-of-clip
# (EOF) scenarios below. Captured from the SOURCE clip BEFORE any field detection or
# CNR3 is applied, so it reflects the real video on disk and never depends on the
# field-split -> filter -> reweave round-trip being frame-count-correct.
SOURCE_FRAME_COUNT = clip.num_frames

# ============================================================================
# FIELD PROCESSORS
# ----------------------------------------------------------------------------
# For processing interlaced material.
# ============================================================================

SplitResult = Tuple[str, vs.VideoNode, Optional[vs.VideoNode]]

def split_into_fields(clip: vs.VideoNode) -> SplitResult:
    """
    Inspect a clip's field order and separate it into same-parity field streams.
    Returns:       a 3-tuple: (scan_tag, first, second)
        scan_tag:  "P"   = progressive
                   "TFF" = interlaced, top field first
                   "BFF" = interlaced, bottom field first
        first:     Depending on the source, one of:
                   Progressive: the original clip unchanged.
                   TFF: the top    fields [T0, T1, T2, ...] clip
                   BFF: the bottom fields [B0, B1, B2, ...] clip
        second:    Depending on the source, one of:
                   Progressive: None
                   TFF: the bottom fields [B0, B1, B2, ...] clip
                   BFF: the top    fields [T0, T1, T2, ...] clip
    The caller is responsible for processing first and second clips independently,
    then passing all three return values (or replacements for first/second)
    to reweave_fields().
    SeparateFields output ordering:
        TFF: [T0, B0, T1, B1, T2, B2, ...]  -> first=offsets[0], second=offsets[1]
        BFF: [B0, T0, B1, T1, B2, T2, ...]  -> first=offsets[0], second=offsets[1]
    The variable names "first" and "second" reflect position in the separated
    stream, not top/bottom.  reweave_fields() uses the scan_tag to reconstruct
    the correct field order, so the caller never needs to track tff explicitly.
    """
    frame0 = clip.get_frame(0)
    field_based = int(frame0.props.get("_FieldBased", 0))
    # _FieldBased: 0=progressive, 1=BFF, 2=TFF
    if field_based == 0:
        return ("P", clip, None)
    tff = (field_based == 2)
    tag = "TFF" if tff else "BFF"
    fields = core.std.SeparateFields(clip, tff=tff)
    first  = core.std.SelectEvery(fields, cycle=2, offsets=[0])
    second = core.std.SelectEvery(fields, cycle=2, offsets=[1])
    return (tag, first, second)

def reweave_fields(
    scan_tag: str,
    first: vs.VideoNode,
    second: Optional[vs.VideoNode],
) -> vs.VideoNode:
    """
    Reweave two and processed field streams separated by "split_into_fields"
    back into a full-height interlaced clip, or return the processed
    progressive clip directly.
    Args:
        scan_tag:  The scan_tag returned by split_into_fields() — "P", "TFF", or "BFF".
        first:     The processed first field stream (or whole clip if progressive).
        second:    The processed second field stream, or None if progressive.
    Returns:
        Progressive ("P"):        first is returned directly.
        Interlaced ("TFF"/"BFF"): first and second are interleaved and woven back
                                  into a full-height interlaced clip at the original
                                  framerate, with the original field order restored.
    """
    if scan_tag == "P":
        return first
    if second is None:
        raise ValueError(
                "reweave_fields: second field stream is None but scan_tag is not 'P'. "
                "Pass the second field stream returned by split_into_fields()."
        )
    if scan_tag not in ("TFF", "BFF"):
        raise ValueError(
                f"reweave_fields: unrecognised scan_tag {scan_tag!r}. "
                "Expected 'P', 'TFF', or 'BFF'."
        )
    tff = (scan_tag == "TFF")
    # Interleave restores the original SeparateFields ordering:
    #   TFF: [T0, B0, T1, B1, ...]
    #   BFF: [B0, T0, B1, T1, ...]
    reinterleaved = core.std.Interleave([first, second])
    # DoubleWeave produces pairs of frames:
    #   even index = clean weave of same-parity neighbours  <- keep these
    #   odd  index = dirty weave of adjacent-parity fields  <- discard these
    rewoven = core.std.DoubleWeave(reinterleaved, tff=tff)
    # Discard the dirty odd-indexed overlap frames, keeping only the clean
    # even-indexed weaves.  Output is back to original resolution and framerate.
    return core.std.SelectEvery(rewoven, cycle=2, offsets=[0])

# NOTE: denoise_across_scan_type is currently UNUSED. The main body below uses the
#   split_into_fields + reweave_fields pair (the only valid means here), and this
#   function duplicates the field-detection logic. Kept so its original use case
#   can be checked before any decision to remove it.
def denoise_across_scan_type(
    clip: vs.VideoNode,
    my_function: Callable[[vs.VideoNode], vs.VideoNode],
) -> vs.VideoNode:
    """
    All-in-one scan-type-aware wrapper: detect progressive vs interlaced, apply
    `my_function` correctly for that scan type, and return a clip in the same
    structure as the input.

    This is the SINGLE-CALL equivalent of the split_into_fields() + reweave_fields()
    pair: for an interlaced clip it splits into same-parity field streams, applies
    `my_function` to EACH stream independently, then reweaves; for a progressive
    clip it applies `my_function` directly. It is NOT used by the main body below —
    that uses the explicit split_into_fields() + reweave_fields() pair instead,
    which keeps the per-stream intermediate clips named. Prefer the explicit pair
    when the two field streams need DIFFERENT handling (e.g. different filter
    parameters per parity) or when you want to inspect/diagnose each stream
    separately; this wrapper is the convenience form when the same processing
    applies uniformly to both. Kept here as that documented alternative.

    Args:
        clip:        Source clip. Scan type is read from frame 0's _FieldBased prop
                     (0 = progressive, 1 = BFF, 2 = TFF).
        my_function: A callable taking one VideoNode and returning one VideoNode.
                     Applied to the whole clip (progressive) or to each field stream
                     (interlaced), so it must be safe to apply to a single field
                     stream. Example: lambda f: core.cnr3.CNR3(f)

    Returns:
        Progressive ("P"):        my_function(clip), structure unchanged.
        Interlaced ("TFF"/"BFF"): the two processed field streams reinterleaved and
                                  woven back into a full-height interlaced clip at the
                                  original framerate, with the original field order
                                  restored.
    """
    frame0 = clip.get_frame(0)
    field_based = int(frame0.props.get("_FieldBased", 0))
    is_interlaced = field_based in (1, 2)
    tff = (field_based == 2)
    if not is_interlaced:
        return my_function(clip)
    fields = core.std.SeparateFields(clip, tff=tff)
    first_fields  = core.std.SelectEvery(fields, cycle=2, offsets=[0])
    second_fields = core.std.SelectEvery(fields, cycle=2, offsets=[1])
    first_processed  = my_function(first_fields)
    second_processed = my_function(second_fields)
    reinterleaved = core.std.Interleave([first_processed, second_processed])
    rewoven = core.std.DoubleWeave(reinterleaved, tff=tff)
    return core.std.SelectEvery(rewoven, cycle=2, offsets=[0])

# ============================================================================
# REQUEST-ORDER TEST-CLIP BUILDERS
# ----------------------------------------------------------------------------
# These build an output clip whose frames map to chosen source frames, so that
# (because VapourSynth pulls BACKWARD up the graph) CNR3 receives the intended
# out-of-order / jumping request sequence at its own getFrame.
#
# Run with  vspipe -r 1  (single request thread): requests reach CNR3 strictly
# one at a time, in deterministic order. This validates CNR3's RECOVERY /
# predecessor logic and makes the dev-trace diff-able. It is NOT a concurrency
# test — true simultaneous-thread races need num_threads>1 and a higher -r, a
# LATER exercise. So "shuffle_in_zones" reproduces the ARRIVAL ORDER a thread
# pool might produce, serialised by -r 1 — not actual parallel races.
# ============================================================================

def shuffle_requests_in_zones(clip: vs.VideoNode, zone_size: int, clip_size: int) -> vs.VideoNode:
    """Reorder request order WITHIN moving zones (no frame missed or repeated),
    reproducing the out-of-order arrival a pool of `zone_size` workers might
    produce. NOTE: zone_size must be >= 3 to actually reorder; at 1 or 2 the
    shuffle often stays sequential.
    """
    if zone_size < 1:
        raise ValueError(f"shuffle_requests_in_zones: zone_size must be >= 1. Got {zone_size}.")
    # 1. Slice the source clip to the requested total clip size
    trimmed_clip = clip[:clip_size]
    # 2. Build the out-of-order pattern array
    pattern = []
    num_zones = clip_size // zone_size
    # Process full window zones
    for z in range(num_zones):
        base_idx = z * zone_size
        # Create a list of relative indices for the current zone [0, 1, 2...]
        zone_indices = list(range(zone_size))
        # Randomly shuffle the requests within this zone to simulate thread concurrency bubbling
        random.shuffle(zone_indices)
        for i in zone_indices:
            pattern.append(base_idx + i)
    # Append any remaining trailing frames linearly
    # trailing frames that don't fill a whole zone: keep them in order
    for idx in range(num_zones * zone_size, clip_size):
        pattern.append(idx)
    return core.std.SelectEvery(trimmed_clip, cycle=clip_size, offsets=pattern)

def assemble_jump_segments(
    base_clip: vs.VideoNode,
    jump_to_frames: List[int],
    segment_length: int,
    shuffle_in_zones: bool,
    zone_size: int,
) -> Tuple[vs.VideoNode, int]:
    """Build a test clip by jumping to each frame in `jump_to_frames`, taking a
    `segment_length`-frame segment from each, optionally shuffling request order
    within zones, then splicing the segments. Returns (clip, total_frame_count).
    """
    if segment_length <= 0:
        raise ValueError(f"assemble_jump_segments: segment_length must be > 0. Got {segment_length}.")
    # Ensure we have at least one starting point
    if not jump_to_frames:
        jump_to_frames = [0]
    segments = []
    for start in jump_to_frames:
        if start < 0 or start >= base_clip.num_frames:
            raise ValueError(
                f"assemble_jump_segments: jump frame {start} out of bounds "
                f"for source ({base_clip.num_frames} frames)."
            )
        # 1. Extract the segment from the base clip. Ensure we don't exceed the source clip length
        end_pos = min(start + segment_length, base_clip.num_frames)
        seg = base_clip[start:end_pos]
        # 2. Apply shuffling (bubbling) if requested
        if shuffle_in_zones and seg.num_frames > 0:
            seg = shuffle_requests_in_zones(seg, zone_size=zone_size, clip_size=seg.num_frames)
        segments.append(seg)
    # 3. Splice all segments together into one long test clip
    final_clip = core.std.Splice(segments)
    return final_clip, final_clip.num_frames

# --------------------------------------------------------------------------------------------------------------
# MAIN: detect scan type, apply CNR3 (per field stream if interlaced), reweave.
# Progressive -> ONE CNR3 instance (clean recovery behaviour; PRIMARY test path).
# Interlaced  -> TWO CNR3 instances on the field streams, then reweave (LATER pass).
# --------------------------------------------------------------------------------------------------------------
# Check if PROGRESSIVE or INTERLACED and return a tag and split the fields if need be
tag, first, second = split_into_fields(clip)
# Depending on the return tag, process the video as PROGRESSIVE or as INTERLACED
if tag == "P":  # PROGRESSIVE source
    if run_mode == MODE_PASSTHROUGH:
        clip_denoised_base = first
    else:
        clip_denoised_base = core.cnr3.CNR3(first)
else:           # INTERLACED source
    if run_mode == MODE_PASSTHROUGH:
        first_denoised  = first
        second_denoised = second
    else:
        first_denoised  = core.cnr3.CNR3(first)
        second_denoised = core.cnr3.CNR3(second)
    clip_denoised_base = reweave_fields(tag, first_denoised, second_denoised)

# ==============================================================================
# TEST SCENARIOS
#
#   Recovery bounds these are sized against:
#     BACK_RADIUS = 50         search reach / floor = N - 50
#     CHECKPOINT_INTERVAL = 10 an anchor (checkpoint) is retained every 10 frames
#
#   Run under  vspipe -r 1 .  "Dev-trace watch" = what the keystone dev-trace output
#   should show. Scenarios are sized deliberately SMALL (segment lengths ~10-200; recovery
#   bounded to ~50) so a run reads at most ~1,000 frames — keeps the dev-trace short.
#   (Adjust jump distances if your build's constants differ.)
#
#   DEV-TRACE MARKER: the keystone build emits one stderr line per frame prefixed [KDT]
#   (KeystoneDevTrace), plus one [KDT-SUMMARY] line at the end. Grep "[KDT]" to isolate the
#   trace; the "Dev-trace watch" notes below describe what those [KDT] lines should show.
#   (These tokens must match the coder-side build — compile guard CNR3_KEYSTONE_DEV_TRACE.)
#
#   S1..S8 = functional scenarios.  B1..B7 = boundary / stress scenarios.
# ==============================================================================

# --- SCENARIO SELECTION ---
# The .bat passes --arg scenario="S1" (etc). Default is S1 if unset.
# Each entry: (jump_to_frames, segment_length, shuffle_in_zones, zone_size)
# Full descriptions (Proves / Dev-trace watch) are in the comment block below the table.
SCENARIOS = {
    # --- functional ---
    "S1": ([0],                      10,  False, 1),   # in-order baseline, 10 frames
    "S2": ([0],                      120, False, 1),   # in-order, lays checkpoints
    "S3": ([0],                      120, True,  5),   # in-zone shuffle, zone 5
    "S4": ([0, 45],                  30,  False, 1),   # small in-bound jump (exact recovery)
    "S5": ([0, 2000],                60,  False, 1),   # hot-zone move then far jump (floor reset)
    "S6": ([5000],                   60,  False, 1),   # deep cold seek (length independence)
    "S7": ([0, 3000, 1000, 2000],    200, False, 8),   # repeated realistic jumping
    "S8": ([0, 3000, 1000, 2000],    200, True,  8),   # as S7 + in-zone shuffle
    # --- boundary / stress ---
    "B1":  ([40],                    15,  False, 1),   # frame-0 clamp (N<B)
    "B2a": ([0, 58],                 10,  False, 1),   # bound straddle, inside (gap 49)
    "B2b": ([0, 61],                 10,  False, 1),   # bound straddle, outside (gap 52)
    "B3":  ([0, 50],                 101, False, 1),   # backward re-request (cache-hit)
    "B4":  ([0],                     100, True,  20),  # heavy out-of-order, wide zone
    "B5":  ([0, 500, 100, 800, 200, 1200, 300], 30, False, 1),  # hot-zone cap / churn
    # --- end-of-clip (targets computed below, since they need SOURCE_FRAME_COUNT) ---
    # B6, B7a, B7b are added after SOURCE_FRAME_COUNT is known — see EOF block.
}
# EOF scenarios depend on the real clip length, so build them here:
SCENARIOS["B6"]  = ([SOURCE_FRAME_COUNT - 30], 60, False, 1)   # in-sequence into EOF
SCENARIOS["B7a"] = ([SOURCE_FRAME_COUNT - 1],  5,  False, 1)   # cold jump to last frame exactly
SCENARIOS["B7b"] = ([SOURCE_FRAME_COUNT - 5],  10, False, 1)   # cold jump, few frames at edge
#
# CLIP-LENGTH PRECONDITION: a scenario's jump_to_frames values must all be < the loaded
# clip's frame count. The field split does NOT change that count — for interlaced,
# SeparateFields yields a half-height double-framerate intermediate, but split_into_fields
# immediately takes first/second from it, so first/second/rewoven each have X frames (= the
# source frame count), same as progressive. So the precondition is purely "frame number <= X".
# The 576p50 (~7250 frames) is the standard test source and satisfies EVERY scenario,
# including S6's 5000. The shorter 576p25 / 576i (~3625 frames) do NOT: S6 ([5000]) would
# raise "jump frame 5000 out of bounds" on them. assemble_jump_segments() guards this and
# errors clearly, so an out-of-range pick fails loudly rather than misbehaving — but use the
# p50 to exercise the whole set.
#
# The .upper().replace(...) dance is because Windows args are case-insensitive-ish and
# the B2a/B7a labels have lowercase suffixes — it normalises b2a->B2a
scenario_name = str(globals().get("scenario", "S1")).upper().replace("B2A","B2a").replace("B2B","B2b").replace("B7A","B7a").replace("B7B","B7b")
if scenario_name not in SCENARIOS:
    raise ValueError(f"'scenario' argument {scenario_name!r} must be one of {sorted(SCENARIOS)}")
sel_jumps, sel_seg_len, sel_shuffle, sel_zone_size = SCENARIOS[scenario_name]

# ==============================================================================
# SCENARIO REFERENCE PLAYBOOK — what each scenario proves and what to watch for in the
# [KDT] dev-trace. (Reference only; the active scenario is chosen by the 'scenario' arg
# above. These notes matter once real filtering exists and recovery actually runs — for
# the K.1C passthrough scaffold the proof is the A/B byte-compare, not the trace.)
#
# --- FUNCTIONAL (S1..S8) ---
# S1 — in-order, 10 frames.
#   Proves: every predecessor is output[N-1] present; recovery never fires.
#   Watch: all in-order; NO recovery; NO floor flag.
# S2 — in-order, 120 frames (lays checkpoints at the 10-frame cadence; hot-zone tracks the wavefront).
#   Watch: (1) in-order throughout, checkpoints recorded, NO floor flag;
#          (2) the hot-zone travelling along, tracking the wavefront.
# S3 — in-zone shuffle, 120 frames, zone 5.
#   Proves: CNR3 recovers a valid predecessor when requests arrive jumbled within a window.
#   Watch: (1) short EXACT recoveries inside the bound, NO floor flag;
#          (2) the hot-zone tracking the bubbling "average" wavefront.
# S4 — process 0..29, then jump to 45 (gap 30..44 = 15 frames, INSIDE BACK_RADIUS=50).
#   NOTE: deliberately small — the only way to land inside the bound and prove EXACT recovery.
#   Proves: output[44] absent -> search finds anchor at 29 within bound -> exact fill 30..44 -> 45.
#   Watch: recovery entered, start point ~29 (within bound), holes walked; NO floor flag.
# S5 — sweep 0..59, then jump far to 2000 (segment 60 = bound + small margin).
#   Proves: output[1999] absent, floor=1950, nothing present in [1950,1999] -> FLOOR FRESH-START
#           at 1950 (NOT frame 0), then walk forward to 2000.
#   Watch: recovery entered, search reaches floor, floor fresh-start ~1950,
#          FLOOR-APPROXIMATION FLAG SET; reconstruction bounded to ~50 frames.
# S6 — jump straight to 5000 from cold (segment 60, same as S5 for a clean comparison).
#   Proves: a far seek does NOT rebuild from frame 0; floor at 4950, <= ~50 frames reconstructed
#           regardless of clip length.
#   Watch: floor fresh-start ~4950, FLAG SET, bounded cost; SAME reconstruction span as S5
#          despite the far larger frame number (compare S5 vs S6 = length-independence).
# S7 — four segments at [0, 3000, 1000, 2000], 200 frames each, sequential.
#   Proves: each jump bounds its OWN recovery; hot-zone tracking/eviction across distant
#           regions; per-jump cost stays bounded (no growth with jump count).
#   Watch: a bounded recovery at each bounce; floor flag where a region is cold, exact where an
#          anchor survives; per-jump cost bounded each time.
# S8 — as S7 but with in-zone shuffle (the hardest sequential case under -r 1).
#   Proves: combined jumping + out-of-order arrival within each segment.
#   Watch: bounded recovery per bounce AND correct predecessor handling for the in-zone shuffle;
#          no unbounded growth; floor flag only where genuinely cold.
#
# --- BOUNDARY / STRESS (B1..B7) ---
# These poke the EDGES of the bound / clamp / hot-zone cap. The dev-trace reveals which side of
# a boundary each fell on; read it rather than assuming exact frame math (an off-by-one in the
# implementation is exactly what these can expose).
# B1 — frame-0 clamp: jump straight to frame 40 from cold (N=40 < BACK_RADIUS=50).
#   Probes: floor = max(0, 40-50) clamps to 0, so the floor IS frame 0 -> a genuine FRAME-0
#           fresh-start fires ("frame 0 only reached when N<B").
#   Watch: recovery entered, floor=0, frame-0 fresh-start, flag SET; fill 1..39 then 40.
# B2a / B2b — bound straddle (a PAIR: run each, compare). Warm 0..9, then one far jump.
#   B2a target 58: nearest real output (9) is at gap 49 -> INSIDE bound (floor=8) -> EXACT recovery.
#   B2b target 61: nearest real output (9) is at gap 52 -> OUTSIDE bound (floor=11) -> FLOOR reset.
#   Proves: the bound crossover sits at ~50 (49 finds the anchor; 52 does not) — differ only by 3.
#   Watch: B2a -> anchor=9, NO floor flag.  B2b -> floor=11, anchor=FLOOR, flag SET.
# B3 — backward re-request: process 0..100, then a segment starting back at 50.
#   Probes: frame 50 was already produced AND is a checkpoint (mult of 10) -> CACHE-HIT direct
#           return on the backward request; following frames (51..) may be evicted non-checkpoints
#           -> short recoveries. Tests the cache-hit path + backward seeks.
#   Watch: at the jump, frame 50 = CACHE-HIT (present); mixed present / short-recovery after.
# B4 — heavy out-of-order: in-zone shuffle with a WIDE zone (20) over 100 frames.
#   Probes: deep reordering within a 20-frame window -> many predecessors absent at request time
#           -> frequent short recoveries, all within the warm region (no floor reset).
#   Watch: frequent EXACT in-zone recoveries; NO floor flag; no unbounded growth.
# B5 — hot-zone cap / churn: 7 distinct jump regions (MAX_HOT_ZONES is 5).
#   Probes: more distinct active regions than hot zones -> hot-zone EVICTION must occur; each jump
#           still bounds its own recovery. Tests the hot-zone cap and eviction behaviour.
#   Watch: a bounded recovery at each of the 7 jumps; hot-zone count caps at 5 with evictions;
#          floor flag where a region is cold; per-jump cost stays bounded.
# B6 / B7 — END-OF-CLIP. The hot-zone FORWARD radius (~10) and the prune calc reason about frames
#   AHEAD of the current one; near the last frame those are PAST end-of-clip = null space. Targets
#   use SOURCE_FRAME_COUNT so they hit the genuine last frame on whatever source is loaded.
# B6 — in-sequence run INTO EOF: ~30 frames in order ending exactly on the last frame.
#   Probes: forward hot-zone span and prune calc reach PAST the last frame -> must clamp to EOF,
#           not compute / index into out-of-range frames.
#   Watch: in-order to the last frame; forward span CLAMPED to the last frame (no window into
#          frames > last); prune handles the boundary; NO floor flag.
# B7a / B7b — cold JUMP to the last frame (recovery AND the forward edge, simultaneously).
#   B7a target = last frame exactly (1 frame out): TIGHTEST boundary — forward radius reaches
#       wholly into null space while recovery's floor search runs backward at the edge.
#   B7b target = last-5 (a few frames at the edge): catches a forward-radius / prune issue that
#       only bites on the 2nd/3rd boundary frame, not the 1st.
#   Watch: recovery entered (floor reset, flag SET — cold jump); forward span CLAMPED to the last
#          frame; no index past EOF; bounded cost.
# ==============================================================================

# ==============================================================================

clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments(
    clip_denoised_base, jump_to_frames=sel_jumps, segment_length=sel_seg_len,
    shuffle_in_zones=sel_shuffle, zone_size=sel_zone_size)

# ==============================================================================

# Output the selected scenario clip (TEST_CLIP_LENGTH frames).
clip_denoised.set_output()

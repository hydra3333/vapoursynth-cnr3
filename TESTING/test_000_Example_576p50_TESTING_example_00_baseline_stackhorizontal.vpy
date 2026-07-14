import sys
from typing import Callable, Optional, Tuple, List
from enum import Enum
import random  # for the in-zone request shuffle
random.seed(20260706)

import vapoursynth as vs

core = vs.core

#****************************************************************************************
# =================================================================================
# VPY / VSPIPE TEST ARGUMENTS
# ----------------------------------------------------------------------------
# vspipe --arg key=value injects string globals into this script.
#
# Example:
#   vspipe --arg cnr3_core_threads=8 ...
#
# Important distinction:
#   core.num_threads controls VapourSynth core worker threads.
#   vspipe -r controls concurrent output frame requests, not worker threads.
#
# Therefore:
#   1. Use cnr3_core_threads to model smaller worker-thread pools.
# =================================================================================

def _cnr3_get_arg_str(name: str, default: str) -> str:
    value = globals().get(name, default)
    if value is None:
        return default
    return str(value).strip()

def _cnr3_get_arg_int(name: str, default: int, *, minimum: Optional[int] = None) -> int:
    raw = _cnr3_get_arg_str(name, str(default))
    try:
        value = int(raw, 10)
    except ValueError as exc:
        raise ValueError(f"{name}: expected integer, got {raw!r}") from exc

    if minimum is not None and value < minimum:
        raise ValueError(f"{name}: expected >= {minimum}, got {value}")

    return value

CNR3_VPY_CORE_THREADS = _cnr3_get_arg_int("cnr3_core_threads", 0, minimum=0)

# If cnr3_core_threads is set to a positive integer, apply it to core.num_threads.
if CNR3_VPY_CORE_THREADS > 0:
    core.num_threads = CNR3_VPY_CORE_THREADS
    sys.stderr.write(f"--- [vpy] RESET core.num_threads = {core.num_threads}\n")
    sys.stderr.flush()

#****************************************************************************************

# --- Source selection: uncomment ONE ---
#source_filename = r"D:\TEST\000_Example_576i.mp4"     # interlaced  (LATER second pass)
#source_filename = r"D:\TEST\000_Example_576p25.mp4"   # progressive (PRIMARY recovery-test clip)
source_filename = r"D:\TEST\000_Example_576p50.mp4"   # progressive (PRIMARY recovery-test clip)

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

def assemble_jump_segments_pair(
    input_base_clip: vs.VideoNode,
    output_base_clip: vs.VideoNode,
    jump_to_frames: List[int],
    segment_length: int,
    shuffle_in_zones: bool,
    zone_size: int,
) -> Tuple[vs.VideoNode, vs.VideoNode, int]:
    """Build matching input/output clips from the same segment cuts and the
    same optional in-zone request order. Returns (input_clip, output_clip,
    total_frame_count), suitable for side-by-side visual comparison.
    """
    if segment_length <= 0:
        raise ValueError(f"assemble_jump_segments_pair: segment_length must be > 0. Got {segment_length}.")
    if shuffle_in_zones and zone_size < 1:
        raise ValueError(f"assemble_jump_segments_pair: zone_size must be >= 1. Got {zone_size}.")
    if input_base_clip.num_frames != output_base_clip.num_frames:
        raise ValueError(
            "assemble_jump_segments_pair: input and output base clips must have the same frame count "
            f"({input_base_clip.num_frames} vs {output_base_clip.num_frames})."
        )
    if not jump_to_frames:
        jump_to_frames = [0]

    input_segments = []
    output_segments = []

    for start in jump_to_frames:
        if start < 0 or start >= output_base_clip.num_frames:
            raise ValueError(
                f"assemble_jump_segments_pair: jump frame {start} out of bounds "
                f"for clip ({output_base_clip.num_frames} frames)."
            )

        end_pos = min(start + segment_length, output_base_clip.num_frames)
        seg_len = end_pos - start
        if seg_len <= 0:
            continue

        pattern = list(range(seg_len))
        if shuffle_in_zones:
            pattern = []
            num_zones = seg_len // zone_size

            for z in range(num_zones):
                base_idx = z * zone_size
                zone_indices = list(range(zone_size))
                random.shuffle(zone_indices)
                for i in zone_indices:
                    pattern.append(base_idx + i)

            for idx in range(num_zones * zone_size, seg_len):
                pattern.append(idx)

        input_seg = input_base_clip[start:end_pos]
        output_seg = output_base_clip[start:end_pos]

        input_segments.append(core.std.SelectEvery(input_seg, cycle=seg_len, offsets=pattern))
        output_segments.append(core.std.SelectEvery(output_seg, cycle=seg_len, offsets=pattern))

    input_final = core.std.Splice(input_segments)
    output_final = core.std.Splice(output_segments)

    if input_final.num_frames != output_final.num_frames:
        raise ValueError(
            "assemble_jump_segments_pair: internal mismatch after splice "
            f"({input_final.num_frames} vs {output_final.num_frames})."
        )

    return input_final, output_final, output_final.num_frames

# --------------------------------------------------------------------------------------------------------------
# MAIN: detect scan type, apply CNR3 (per field stream if interlaced), reweave.
# Progressive -> ONE CNR3 instance (clean recovery behaviour; PRIMARY test path).
# Interlaced  -> TWO CNR3 instances on the field streams, then reweave (LATER pass).
# --------------------------------------------------------------------------------------------------------------
# Check if PROGRESSIVE or INTERLACED and return a tag and split the fields if need be
tag, first, second = split_into_fields(clip)
# Depending on the return tag, process the video as PROGRESSIVE or as INTERLACED
if tag == "P":  # PROGRESSIVE source
    clip_input_base = first
    clip_denoised_base = core.cnr3.CNR3(first)
else:           # INTERLACED source
    first_denoised  = core.cnr3.CNR3(first)
    second_denoised = core.cnr3.CNR3(second)
    clip_input_base = reweave_fields(tag, first, second)
    clip_denoised_base = reweave_fields(tag, first_denoised, second_denoised)

# ==============================================================================
# TEST SCENARIOS — uncomment exactly ONE. Each paired scenario returns (clip_input_selected, clip_denoised, TEST_CLIP_LENGTH).
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

# ---------------------------- FUNCTIONAL (S1..S8) -----------------------------

# ---------- IN-ORDER BASELINES (no recovery; reference output) ----------
# *** SCENARIO S1 — in-order, 10 frames.
#   Proves: every predecessor is output[N-1] present; recovery never fires.
#   Dev-trace watch: all in-order; NO recovery; NO floor flag.
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[0], segment_length=10,  shuffle_in_zones=False, zone_size=1)

# *** SCENARIO S2 — in-order, 120 frames (lays down checkpoints at the 10-frame cadence, the hot-zone tracks the wavefront).
#   Dev-trace watch: (1) in-order throughout; checkpoints recorded; NO floor flag.
#                    (2) the hot-zone travelling along in about the right place tracking with the wavefront
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[0], segment_length=120, shuffle_in_zones=False, zone_size=1)

# ---------- OUT-OF-ORDER ARRIVAL WITHIN A ZONE (short recovery; sequence, not concurrency) ----------
# *** SCENARIO S3 — in-zone shuffle, 120 frames, zone 5.
#   Proves: CNR3 recovers a valid predecessor when requests arrive jumbled within a window,
#           The hot-zone travelling along in about the right place tracking with the bubbling "average" wavefront.
#   Dev-trace watch: (1) short EXACT recoveries inside the bound; NO floor flag.
#                    (2) the hot-zone travelling along tracking with the wavefront
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[0], segment_length=120, shuffle_in_zones=True,  zone_size=5)

# ---------- SMALL IN-BOUND JUMP (exact recovery; DELIBERATELY small — see note) ----------
# *** SCENARIO S4 — process 0..29, then jump to 45 (the gap 30..44 is 15 frames, INSIDE BACK_RADIUS=50).
#   NOTE: this jump is intentionally SMALL. It is the only way to land inside the
#   bound and prove EXACT recovery (anchor found, holes filled, NO floor reset).
#   The realistic large jumps are S5..S8.
#   Proves: output[44] absent -> search finds anchor at 29 within the bound -> exact fill 30..44 -> 45.
#   Dev-trace watch: recovery entered, start point ~29 (within bound), holes walked; NO floor flag.
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[0, 45], segment_length=30, shuffle_in_zones=False, zone_size=1)

# ---------- HOT-ZONE MOVE THEN FAR JUMP (the bound; floor reset is NOT from frame 0) ----------
# *** SCENARIO S5 — sweep 0..59, then jump far to 2000.  (segment 60 = bound + a small margin)
#   Proves: output[1999] absent, floor=1950, nothing present in [1950,1999] -> FLOOR
#           FRESH-START at 1950 (NOT frame 0), then walk forward to 2000.
#   Dev-trace watch: recovery entered, search reaches floor, floor fresh-start ~1950,
#           FLOOR-APPROXIMATION FLAG SET; reconstruction bounded to ~50 frames.
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[0, 2000], segment_length=60, shuffle_in_zones=False, zone_size=1)

# ---------- DEEP COLD SEEK (clip-length independence) ----------
# *** SCENARIO S6 — jump straight to 5000 from cold.  (segment 60, same as S5, for a clean comparison)
#   Proves: a far seek does NOT rebuild from frame 0; floor at 4950, <= ~50 frames
#           reconstructed regardless of the ~5,500-frame clip length.
#   Dev-trace watch: floor fresh-start ~4950, FLAG SET, bounded cost; SAME reconstruction
#           span as S5 despite the far larger frame number (compare S5 vs S6 = length-independence).
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[5000], segment_length=60, shuffle_in_zones=False, zone_size=1)

# ---------- REPEATED REALISTIC JUMPING (your original values; each jump independently bounded) ----------
# *** SCENARIO S7 — four segments at [0, 3000, 1000, 2000], 200 frames each, sequential.
#   Proves: each jump bounds its OWN recovery; hot-zone tracking/eviction across distant
#           regions; per-jump cost stays bounded (no growth with jump count).
#   Dev-trace watch: a bounded recovery at each bounce; floor flag where a region is cold,
#           exact where an anchor survives; per-jump cost bounded each time.
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[0, 3000, 1000, 2000], segment_length=200, shuffle_in_zones=False, zone_size=8)

# *** SCENARIO S8 — as S7 but with in-zone shuffle (the hardest sequential case under -r 1).
#   Proves: combined jumping + out-of-order arrival within each segment.
#   Dev-trace watch: bounded recovery per bounce AND correct predecessor handling for the
#           in-zone shuffle; no unbounded growth; floor flag only where genuinely cold.
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[0, 3000, 1000, 2000], segment_length=200, shuffle_in_zones=True,  zone_size=8)

# *** SCENARIO S9 — cache-overflow + REVISIT thrash: distinct regions beyond hot-zone cap, then RETURN to pruned regions.
#   Purpose: force pruning between jumps (D-SUM-10 ring/prune) AND revisit evicted regions so they must RECOMPUTE
#   (D-SUM-13 recalculation/thrash). Best run under CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY (ceiling 100).
#   6 distinct forward regions (> MAX_HOT_ZONES) then revisit 0/1000/2000 after they've been pruned. -r 1.
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[0, 500, 1000, 1500, 2000, 2500, 0, 1000, 2000], segment_length=150, shuffle_in_zones=False, zone_size=1)

# *** SCENARIO S9b — as S9 but with in-zone shuffle (prune/thrash + arrival-disorder recovery, hardest combined case).
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[0, 500, 1000, 1500, 2000, 2500, 0, 1000, 2000], segment_length=150, shuffle_in_zones=True,  zone_size=8)

# *** SCENARIO S9c — over-prune probe: single contiguous 250-frame run, in-zone shuffle, NO jumps.
#   Under TINY-100 (ceiling 100, back_radius 15) a shuffle span well inside back_radius should yield
#   ~zero frames_evicted_then_re_requested. Any non-zero => something pruned inside the protected window.
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[0], segment_length=250, shuffle_in_zones=True, zone_size=5)

# *** SCENARIO S9d — disjoint jumps, no revisit, no overlap: isolates whether evict-then-re-request
#   is purely an artifact of RE-REQUESTING an evicted range. Each 250 segment is a fresh region,
#   spaced > segment_length apart so ranges (and their rebuild-lookback) never intersect; no target repeats.
#   Prediction: frames_evicted_then_re_requested == 0 (only cold-forward eviction, nothing ever asked twice).
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[0, 1000, 2000, 3000, 1500, 500], segment_length=250, shuffle_in_zones=True, zone_size=5)

# *** SCENARIO S9e — recency positive control: revisit a JUST-evicted region so eviction_gap <= Z.
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[0, 5], segment_length=120, shuffle_in_zones=False, zone_size=1)
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[0, 95], segment_length=100, shuffle_in_zones=False, zone_size=1)

# S9e attempt 1: minimal overflow, immediate revisit
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[0, 0], segment_length=108, shuffle_in_zones=False, zone_size=1 )

# S9e attempt 2: minimal overflow, near-start revisit
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[0, 4], segment_length=108, shuffle_in_zones=False, zone_size=1)

# S9e attempt 3: slightly larger overflow, still close revisit
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[0, 4],segment_length=112,shuffle_in_zones=False,zone_size=1)

# S9e attempt 3: slightly larger overflow, still close revisit
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[0, 4],segment_length=112,shuffle_in_zones=False,zone_size=1)

# S9e attempt 4: controlled just-evicted revisit
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[0, 8],segment_length=112,shuffle_in_zones=False,zone_size=1)

# *** SCENARIO S9e — recency positive control: revisit a JUST-evicted region.
#   Under TINY-100, this lands evict-then-refetch events in the recovery-local population.
#   Expect frames_recently_evicted_then_re_requested > 0. -r 1.
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[0, 8],segment_length=108,shuffle_in_zones=False,zone_size=1)

# ===============================================================================================
# ===============================================================================================
# ===============================================================================================
# =============================== LINEAR PRODUCTION-CASE (L1..L4) ===============================
# The 99% real use case: vspipe -> ffmpeg linear playthrough of a whole clip, NO jumps.
# This is "bubbling linear" -- a single contiguous forward sweep of the ENTIRE source.
# Run these under the NORMAL profile (TINY-100 OFF); a clip longer than the NORMAL ceiling (1000)
# will evict naturally as the wavefront advances, which is exactly the production behaviour to watch.
#
# segment_length = SOURCE_FRAME_COUNT  -> the whole clip (there is no "ALL" keyword; pass the count).
# jump_to_frames = [0]                 -> start at 0, one contiguous segment, no jumps.
#
# The L1<->L2 and (with -r 1 removed) the L*-noR pairs isolate the two variables independently:
#   L1  vs L2   : arrival ORDER (clean vs shuffle8) at fixed single-thread -r 1.
#   L1  vs L1noR: THREADING (single vs multi) at fixed clean order.
# Read D-SUM-01 out_of_order/backward/forward as the disorder signal; D-SUM-12 recovery_rate as the
# cache-miss consequence; D-SUM-10 recent_rechurn + [DSUM10-GAP-HISTO] as the eviction-quality signal;
# D-SUM-13 recalculated_frame_count as the "did disorder force real recompute" signal; and the
# [DSUM-HEALTH] block for the at-a-glance ratios.

# ***
# ***
# *** SCENARIO example_00 — LINEAR, 3000 frames clip, in-order, NO shuffle. 
clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[0], segment_length=3000, shuffle_in_zones=False, zone_size=1)    ####
# ***
# ***
# ***

# *** SCENARIO example_01 — LINEAR, 200 frames clip, in-order, NO shuffle.  Used with or without '-r 1' and with fmunordered or fmparallel
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[0], segment_length=200, shuffle_in_zones=False, zone_size=1)    ####

# *** SCENARIO L1 — LINEAR, whole clip, in-order, NO shuffle.  The clean 99% baseline.
#   Prediction (-r 1): recovery ~0%, out_of_order 0, recent_rechurn 0, recalc 0, cache_hit ~99%.
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[0], segment_length=SOURCE_FRAME_COUNT, shuffle_in_zones=False, zone_size=1)    ####

# *** SCENARIO L2 — LINEAR, whole clip, in-zone shuffle size 8.  Arrival-disorder stand-in.
#   Prediction (-r 1): out_of_order > 0, small recovery bump, recent_rechurn likely still 0
#   (an 8-frame shuffle stays within BACK_RADIUS=50 so predecessors remain resident), recalc likely 0.
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[0], segment_length=SOURCE_FRAME_COUNT, shuffle_in_zones=True,  zone_size=8)    ####

# *** SCENARIO L1noR / L2noR — SAME two lines as L1 / L2, but run the .bat with rrr= EMPTY (remove -r 1).
#   This is the ONLY way to see genuine multi-thread "bubbling" (out-of-order EXECUTION across threads).
#   Use the L1 line for L1noR (clean order + threads) and the L2 line for L2noR (disorder + threads).
#   Prediction: L1noR is the key run -- if the cache/hot-zone handles the production case, recovery stays
#   low; if threads outrun the predecessor chain, recovery/recent_rechurn/recalc appear. That gap is the
#   first real A1 question. (No separate scenario line needed; just toggle rrr in the .bat.)
# ===============================================================================================
# ===============================================================================================
# ===============================================================================================

# ------------------------------- BOUNDARY / STRESS (B1..B7) ---------------------------------
# These poke the EDGES of the bound / clamp / hot-zone cap. The dev-trace reveals
# which side of a boundary each fell on; read it rather than assuming exact frame
# math (an off-by-one in the implementation is exactly what these can expose).

# *** SCENARIO B1 — frame-0 clamp: jump straight to frame 40 from cold (N=40 < BACK_RADIUS=50).
#   Probes: floor = max(0, 40-50) clamps to 0, so the floor IS frame 0 -> a genuine
#           FRAME-0 fresh-start fires. This is the "frame 0 only reached when N<B" case.
#   Dev-trace watch: recovery entered, floor=0, frame-0 fresh-start, flag SET; fill 1..39 then 40.
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[40], segment_length=15, shuffle_in_zones=False, zone_size=1)

# *** SCENARIO B2 — bound straddle (a PAIR: run each, compare). Warm 0..9, then one far jump.
#   B2a target 58: nearest real output (9) is at gap 49 -> INSIDE the bound (floor=8) -> EXACT recovery.
#   B2b target 61: nearest real output (9) is at gap 52 -> OUTSIDE the bound (floor=11) -> FLOOR reset.
#   Proves: the bound crossover sits at ~50 (49 finds the anchor; 52 does not). They differ only by 3.
#   Dev-trace watch: B2a -> anchor=9, NO floor flag.  B2b -> floor=11, anchor=FLOOR, flag SET.
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[0, 58], segment_length=10, shuffle_in_zones=False, zone_size=1)   # B2a (inside)
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[0, 61], segment_length=10, shuffle_in_zones=False, zone_size=1)   # B2b (outside)

# *** SCENARIO B3 — backward re-request: process 0..100, then a segment starting back at 50.
#   Probes: frame 50 was already produced AND is a checkpoint (multiple of 10) -> CACHE-HIT
#           direct return on the backward request; following frames (51..) may be evicted
#           non-checkpoints -> short recoveries. Tests the cache-hit path + backward seeks.
#   Dev-trace watch: at the jump, frame 50 = CACHE-HIT (present); mixed present / short-recovery after.
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[0, 50], segment_length=101, shuffle_in_zones=False, zone_size=1)

# *** SCENARIO B4 — heavy out-of-order: in-zone shuffle with a WIDE zone (20) over 100 frames.
#   Probes: deep reordering within a 20-frame window -> many predecessors absent at request
#           time -> frequent short recoveries, all within the warm region (no floor reset).
#   Dev-trace watch: frequent EXACT in-zone recoveries; NO floor flag; no unbounded growth.
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[0], segment_length=100, shuffle_in_zones=True, zone_size=20)

# *** SCENARIO B5 — hot-zone cap / churn: 7 distinct jump regions (MAX_HOT_ZONES is 5).
#   Probes: more distinct active regions than hot zones -> hot-zone EVICTION must occur; each
#           jump still bounds its own recovery. Tests the hot-zone cap and eviction behaviour.
#   Dev-trace watch: a bounded recovery at each of the 7 jumps; hot-zone count caps at 5 with
#           evictions; floor flag where a region is cold; per-jump cost stays bounded.
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[0, 500, 100, 800, 200, 1200, 300], segment_length=30, shuffle_in_zones=False, zone_size=1)

# ---------- END-OF-CLIP (EOF) — forward-looking calcs (hot-zone forward radius, prune) at the edge ----------
# The hot-zone FORWARD radius (~10) and the prune calc reason about frames AHEAD of the
# current one. Near the last frame those forward frames are PAST end-of-clip = null space.
# These scenarios fetch the genuine last frame so that behaviour is exercised. Targets use
# SOURCE_FRAME_COUNT (the real video length) so they are correct for whichever source is loaded.
# NOTE (interlaced source): SOURCE_FRAME_COUNT is the displayed-FRAME count; CNR3 runs on the
#   field streams internally, so the last frame maps to the last field-pair hitting EOF in BOTH
#   field-stream recursions at once — a useful extra stressor, not a problem.

# *** SCENARIO B6 — in-sequence run INTO EOF: ~30 frames in order ending exactly on the last frame.
#   (segment_length 60 > 30 so the min(...) clamp trims the segment to land on the final frame.)
#   Probes: during normal sequential fetch near the end, the forward hot-zone span and the
#           prune calc reach PAST the last frame -> must clamp to EOF, not compute / index into
#           out-of-range frames.
#   Dev-trace watch: in-order to the last frame; forward hot-zone span CLAMPED to the last frame
#           (no window into frames > last); prune handles the boundary; NO floor flag.
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[SOURCE_FRAME_COUNT - 30], segment_length=60, shuffle_in_zones=False, zone_size=1)

# *** SCENARIO B7 — cold JUMP to the last frame (recovery AND the forward edge, simultaneously).
#   Two variants — run each:
#   B7a target = last frame exactly (1 frame of output): the TIGHTEST boundary — forward radius
#       reaches wholly into null space while recovery's floor search runs backward at the edge.
#   B7b target = last-5 (a few frames running AT the edge): catches a forward-radius / prune issue
#       that only bites on the 2nd/3rd boundary frame, not the 1st.
#   Probes: cold recovery (floor search backward) AND forward-looking calcs (hot zone / prune)
#           both at the clip edge at once.
#   Dev-trace watch: recovery entered (floor reset, flag SET — cold jump); forward span CLAMPED
#           to the last frame; no index past EOF; bounded cost.
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[SOURCE_FRAME_COUNT - 1], segment_length=5,  shuffle_in_zones=False, zone_size=1)   # B7a (last frame exactly)
#clip_input_selected, clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments_pair(clip_input_base, clip_denoised_base, jump_to_frames=[SOURCE_FRAME_COUNT - 5], segment_length=10, shuffle_in_zones=False, zone_size=1)   # B7b (few frames at the edge)

# ==============================================================================

# Output input and CNR3 output side-by-side for visual comparison (TEST_CLIP_LENGTH frames).
clip_compare = core.std.StackHorizontal([clip_input_selected, clip_denoised])
clip_compare.set_output()

# Print info to stderr
sys.stderr.write(f"\n--- [vpy] Vapoursynth Information ---\n")
sys.stderr.write(f"--- [vpy] === VapourSynth core info ===\n")
sys.stderr.write(f"--- [vpy] core.num_threads     = {core.num_threads}\n")
sys.stderr.write(f"--- [vpy] core.max_cache_size  = {core.max_cache_size}\n")
sys.stderr.write(f"--- [vpy] core.used_cache_size = {core.used_cache_size}\n")
sys.stderr.write(f"--- [vpy] core.core_version    = {core.core_version}\n")
sys.stderr.write(f"--- [vpy] core.api_version     = {core.api_version}\n")
#
sys.stderr.write(f"\n--- [vpy] CNR3 TEST: VapourSynth core.num_threads={core.num_threads}\n")
sys.stderr.write(f"--- [vpy] CNR3 TEST:                numThreads/2 ={core.num_threads // 2}\n")
#
sys.stderr.write(f"\n--- [vpy] Clip Information ---\n")
sys.stderr.write(f"--- [vpy] Original source frames = {SOURCE_FRAME_COUNT}\n")
sys.stderr.write(f"--- [vpy] Final output frames    = {TEST_CLIP_LENGTH}\n")
sys.stderr.write(f"--- [vpy] ------------------------\n\n")
#
sys.stderr.flush()
from typing import Callable, Optional, Tuple
import vapoursynth as vs

core = vs.core
from vscnr2_bm3d import cnr2_bm3d, cnr2_bm3d_precheck_video_file

source_filename = r"D:\TEST\000_Example_576i.mp4"
#source_filename = r"D:\TEST\000_Example_576p25.mp4"
#source_filename = r"D:\TEST\000_Example_576p50.mp4"

clip = core.bs.VideoSource(source_filename)

######################################################################################
# INTERLACED/PROGRESSIVE HELPER FUNCTIONS
#
def _process_progressive_or_interlaced(
    clip: vs.VideoNode,
    my_function: Callable[[vs.VideoNode], vs.VideoNode],
) -> vs.VideoNode:
    """
    Detects and works with any of Progressive/TFF/BFF.
    Split an interlaced clip into same-parity field streams, apply a processing
    function to each stream independently, then reweave back into the original
    interlaced format with the original field order preserved.
    For a progressive clip the processing function is applied directly and the
    clip is returned unchanged in structure.
    WHY FIELD-SPLITTING IS NECESSARY FOR TEMPORAL PROCESSING:
        A raw interlaced clip has alternating field lines from two different
        moments in time.  Temporal filters such as BM3D compare adjacent frames
        to find matching blocks.  On an unsplit interlaced clip those adjacent
        frames contain *opposite* field parities — the spatial grids are offset
        by half a line.  This causes block-matching to compare misaligned content
        and produces artefacts along every horizontal edge.
        Splitting into same-parity streams (top-only, bottom-only) ensures that
        every temporal neighbour shares the same spatial sampling grid, so block-
        matching compares correctly aligned content.
    FIELD-ORDER INVARIANT:
        SeparateFields(tff=True)  -> [T0, B0, T1, B1, T2, B2, ...]
        SeparateFields(tff=False) -> [B0, T0, B1, T1, B2, T2, ...]
        In both cases SelectEvery(cycle=2, offsets=[0]) picks the first field
        of each original frame and SelectEvery(cycle=2, offsets=[1]) picks the
        second.  
        The variable names below reflect the TFF convention; for BFF
        sources "first_fields" holds the bottom fields and "second_fields" holds
        the top fields.  The names are irrelevant because the same tff value is
        passed consistently through SeparateFields -> DoubleWeave, so the original
        field order is always reconstructed correctly.
    REWEAVE INVARIANT:
        Interleave([first_processed, second_processed]) reconstructs the same field
        ordering that SeparateFields produced, regardless of TFF or BFF.
        DoubleWeave(tff=<original>) produces pairs of frames where even-indexed
        frames are clean weaves of same-parity neighbours and odd-indexed frames
        are dirty overlapping weaves.  SelectEvery(cycle=2, offsets=[0]) discards
        the dirty odd frames, restoring the original resolution and framerate.
    Args:
        clip:           Input YUV clip.  Must be constant format with
                        known frame count and a constant framerate.
        my_function:    A callable that accepts and returns a VapourSynth
                        VideoNode. It is called once per field-parity stream.
                        The field-parity streams are progressive half-height
                        clips at the original framerate.
    Returns:
        For interlaced input: a rewoven interlaced clip at the original
        resolution, framerate, and field order.
        For progressive input: the result of my_function(clip) directly.
    Call like:
        result = _process_progressive_or_interlaced(
                            clip,
                            my_function=lambda f: _my_denoise_function(f, sigma=3.5),)
    """
    frame0 = clip.get_frame(0)
    field_based = int(frame0.props.get("_FieldBased", 0))
    # _FieldBased values:
    #   0 = progressive
    #   1 = interlaced, bottom field first (BFF)
    #   2 = interlaced, top field first (TFF)
    is_interlaced = field_based in (1, 2)
    tff = (field_based == 2)  # True for TFF, False for BFF
    # --- Progressive path ---
    if not is_interlaced:
        # Progressive: apply processing directly, no field handling needed.
        return my_function(clip)
    # --- Interlaced path ---
    # SeparateFields output for TFF:  [T0, B0, T1, B1, T2, B2, ...]
    # SeparateFields output for BFF:  [B0, T0, B1, T1, B2, T2, ...]
    # The tff parameter here must match the source field order so that
    # Weave below can reconstruct the correct weave.
    fields = core.std.SeparateFields(clip, tff=tff)
    # offsets=[0] -> every even field in the separated stream  (first  field of each frame)
    # offsets=[1] -> every odd  field in the separated stream  (second field of each frame)
    # For TFF: first_fields=[T0,T1,...], second_fields=[B0,B1,...]
    # For BFF: first_fields=[B0,B1,...], second_fields=[T0,T1,...]
    first_fields  = core.std.SelectEvery(fields, cycle=2, offsets=[0])
    second_fields = core.std.SelectEvery(fields, cycle=2, offsets=[1])
    # Apply the caller-supplied processing function to each same-parity stream
    # independently.  Each stream is a progressive half-height clip at the
    # original framerate, so temporal neighbours within each stream are always
    # same-parity fields = correctly aligned spatial grids.
    first_processed  = my_function(first_fields)
    second_processed = my_function(second_fields)
    # Interleave restores the original SeparateFields ordering:
    #   TFF: [T0, B0, T1, B1, ...]
    #   BFF: [B0, T0, B1, T1, ...]
    # This is correct in both cases because we always pass [first_fields, second_fields]
    # in the same order that SeparateFields produced them.
    reinterleaved = core.std.Interleave([first_processed, second_processed])
    # DoubleWeave produces pairs of frames:
    #   even index = clean weave of same-parity neighbours  <- keep these
    #   odd  index = dirty weave of adjacent-parity fields  <- discard these
    # tff must match the original source so top/bottom plane assignment is correct.
    rewoven = core.std.DoubleWeave(reinterleaved, tff=tff)
    # Discard the dirty odd-indexed overlap frames, keeping only the clean
    # even-indexed weaves.  Output is back to original resolution and framerate.
    return core.std.SelectEvery(rewoven, cycle=2, offsets=[0])
#
#######################################
#
SplitResult = Tuple[str, vs.VideoNode, Optional[vs.VideoNode]]
def split_fields(clip: vs.VideoNode) -> SplitResult:
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
#
#######################################
#
def reweave_fields(
    scan_tag: str,
    first: vs.VideoNode,
    second: Optional[vs.VideoNode],
) -> vs.VideoNode:
    """
    Reweave two and processed field streams separated by "split_fields"
    back into a full-height interlaced clip, or return the processed
    progressive clip directly.
    Args:
        scan_tag:  The scan_tag returned by split_fields() — "P", "TFF", or "BFF".
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
            "reweave_fields: second is None but scan_tag is not 'P'. "
            "Pass the second field stream returned by split_fields()."
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
    # tff must match the original source so top/bottom plane assignment is correct.
    rewoven = core.std.DoubleWeave(reinterleaved, tff=tff)
    # Discard the dirty odd-indexed overlap frames, keeping only the clean
    # even-indexed weaves.  Output is back to original resolution and framerate.
    return core.std.SelectEvery(rewoven, cycle=2, offsets=[0])
#
#######################################
# EXAMPLE USAGE OF split_fields and reweave_fields:
#       tag, first, second = split_fields(clip)
#       # do whatever you need with first and second, eg denoising
#       first  = some_operation(first)  # eg core.cnr3.CNR3(first)
#       second = some_operation(second) # eg second = core.cnr3.CNR3(second)
#       result = reweave_fields(tag, first, second)
######################################################################################

def make_bubbling_test_clip(clip: vs.VideoNode, zone_size: int, clip_size: int) -> vs.VideoNode:
    """
    Generates a test clip of an exact size where frames bubble out of order 
    within a moving zone, ensuring NO frames are missed or repeated.
    :param clip: The base input video clip.
    :param zone_size: The moving window size (simulates parallel thread concurrency).
    :param clip_size: The exact total number of frames for the output test clip.
    """
    core = vs.core
    #
    # 1. Slice the source clip to the requested total clip size
    trimmed_clip = clip[:clip_size]
    # 2. Build the out-of-order pattern array
    pattern = []
    # Process full window zones
    num_zones = clip_size // zone_size
    for z in range(num_zones):
        base_idx = z * zone_size
        left = 0
        right = zone_size - 1
        # Interleave requests from the outside-in within this zone
        while left <= right:
            if left == right:
                pattern.append(base_idx + left)
            else:
                pattern.append(base_idx + left)
                pattern.append(base_idx + right)
            left += 1
            right -= 1
    # 3. Append any remaining trailing frames linearly so nothing is missed
    leftover_start = num_zones * zone_size
    for idx in range(leftover_start, clip_size):
        pattern.append(idx)
    # 4. Apply the exact mapping sequence over the entire trimmed length
    return core.std.SelectEvery(trimmed_clip, cycle=clip_size, offsets=pattern)

# --------------------------------------------------------------------------------------------------------------
# --------------------------------------------------------------------------------------------------------------

# --- START Define bubbling out-of-order clip parameters ---
THREADS_ZONE = 5     # Simulates X concurrent out-of-order frame worker requests
TEST_CLIP_LEN = 20   # The output clip will be exactly Y frames long
SIMULATE_OUT_OF_ORDER = True
# --- END Define bubbling out-of-order clip parameters ---

# FIRSTLY, CHECK IF PROGRESSIVE or INTERLACED AND RETURN A TAG AND SPLIT FIELDS IF NEED BE
tag, first, second = split_fields(clip)

# SECONDLY, DEPENDING ON THE RETURN TAG, PROCESS THE VIDEO AS PROGRESSIVE OR AS INTERLACED, BUGGLING OR NOT
blend_value = 1
if tag == "P":  # original clip was PROGRESSIVE
    clip_denoised_base = core.cnr3.CNR3(first, blend=blend_value)
else:           # original clip was INTERLACED
    first_denoised_base  = core.cnr3.CNR3(first, blend=blend_value)
    second_denoised_base = core.cnr3.CNR3(second, blend=blend_value)
    clip_denoised_base = reweave_fields(tag, first_denoised_base, second_denoised_base)
#    
if SIMULATE_OUT_OF_ORDER:
    clip_denoised = make_bubbling_test_clip(clip_denoised_base, zone_size=THREADS_ZONE, clip_size=TEST_CLIP_LEN)
else:
    clip_denoised = clip_denoised_base[:TEST_CLIP_LEN]
#
#stacked_comparison = core.std.StackHorizontal([ clip, clip_denoised,])
#stacked_comparison.set_output()
clip_denoised.set_output()

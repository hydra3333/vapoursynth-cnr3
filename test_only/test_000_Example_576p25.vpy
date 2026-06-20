from typing import Callable, Optional, Tuple, List
from enum import Enum
import random  # Added for random bubbling logic

import vapoursynth as vs

core = vs.core
# from vscnr2_bm3d import cnr2_bm3d, cnr2_bm3d_precheck_video_file

#source_filename = r"D:\TEST\000_Example_576i.mp4"
source_filename = r"D:\TEST\000_Example_576p25.mp4"
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
    # tff match the original source so top/bottom plane assignment is correct.
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

######################################################################################
# BUBBLING LOGIC
######################################################################################

def make_bubbling_test_clip(clip: vs.VideoNode, zone_size: int, clip_size: int) -> vs.VideoNode:
    """
    Generates a test clip of an exact size where frames bubble out of order 
    within a moving zone, ensuring NO frames are missed or repeated.
    :param clip: The base input video clip.
    :param zone_size: The moving window size (simulates parallel thread concurrency).
    :param clip_size: The exact total number of frames for the output test clip.
    """
    # --------------------------------------------------------------------------------
    # BOUND CHECK: Abort if zone_size is invalid.
    # --------------------------------------------------------------------------------
    if zone_size < 1:
        raise ValueError(f"make_bubbling_test_clip error: zone_size must be 1 or greater. Got {zone_size}.")

    # --------------------------------------------------------------------------------
    # LOGIC NOTE: To see actual out-of-order "bubbling," zone_size must be 3 or greater.
    # At zone_size=1 or 2, the random shuffle result often remains sequential.
    # --------------------------------------------------------------------------------
    
    core = vs.core
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

    # 3. Append any remaining trailing frames linearly
    leftover_start = num_zones * zone_size
    for idx in range(leftover_start, clip_size):
        pattern.append(idx)
    # 4. Apply the exact mapping sequence
    return core.std.SelectEvery(trimmed_clip, cycle=clip_size, offsets=pattern)

def create_bouncing_test_clip(
    base_clip: vs.VideoNode, 
    bounce_frames: List[int], 
    segment_len: int, 
    zone_size: int, 
    do_bubble: bool
) -> Tuple[vs.VideoNode, int]:
    """
    Abstracts the bubbling logic to handle multiple jump points (bounces).
    It takes segments from specified start frames and joins them.
    
    Returns: (final_processed_clip, actual_total_frame_count)
    """
    # ----------------------------------------------------------------------------
    # BOUND CHECK: Abort if segment_len is invalid
    # ----------------------------------------------------------------------------
    if segment_len <= 0:
        raise ValueError(f"create_bouncing_test_clip error: segment_len must be > 0. Got {segment_len}.")

    segments = []
    
    # Ensure we have at least one starting point
    if not bounce_frames:
        bounce_frames = [0]
        
    for start in bounce_frames:
        # ----------------------------------------------------------------------------
        # BOUND CHECK: Verify jump point exists in the source file
        # ----------------------------------------------------------------------------
        if start < 0 or start >= base_clip.num_frames:
             raise ValueError(f"create_bouncing_test_clip error: Jump frame {start} is out of bounds for source ({base_clip.num_frames} frames).")

        # 1. Extract the segment from the base clip
        # Ensure we don't exceed the source clip length
        end_pos = min(start + segment_len, base_clip.num_frames)
        seg = base_clip[start:end_pos]
        
        actual_seg_len = seg.num_frames
        
        # 2. Apply bubbling if requested
        if do_bubble and actual_seg_len > 0:
            seg = make_bubbling_test_clip(seg, zone_size=zone_size, clip_size=actual_seg_len)
            
        segments.append(seg)
        
    # 3. Splice all segments together into one long test clip
    final_clip = core.std.Splice(segments)
    return final_clip, final_clip.num_frames


# --------------------------------------------------------------------------------------------------------------
# --------------------------------------------------------------------------------------------------------------

# Check if PROGRESSIVE or INTERLACED and return a tag and split the fields if need be
tag, first, second = split_fields(clip)

# Depending on the return tag, process the video as PROGRESSIVE or as INTERLACED
if tag == "P":  # original clip was PROGRESSIVE
    clip_denoised_base = core.cnr3.CNR3(first)
else:           # original clip was INTERLACED
    first_denoised_base  = core.cnr3.CNR3(first)
    second_denoised_base = core.cnr3.CNR3(second)
    clip_denoised_base = reweave_fields(tag, first_denoised_base, second_denoised_base)

# --- START Define bubbling out-of-order clip parameters ---
#bounce_frames = [0, 3000, 1000, 2000]      # Points to jump to in the source file
#segment_len = 200                          # Size (length) of each segment starting at a bounce point
#do_bubble = True                           # Set to False if you want sequential-frames-only within segments (no bubbling)
#zone_size = 5                              # Simulates X concurrent out-of-order frame worker requests (bubbling)
# --- END Define bubbling out-of-order clip parameters ---

# Create the final clip from the segments and bubbling.
# Function create_bouncing_test_clip handles:
#   - "bouncing" (extracting segments based on starting frame numbers)
#   - "bubbling" (randomly reordering frames within those segments)

# --- VALID CALL EXAMPLES WITH DESCRIPTIONS ---
# Un-comment ONLY the one you want to test.
# Note: Every call returns (clip, frame_count) tuple.

clip_denoised, TEST_CLIP_LEN = create_bouncing_test_clip( clip_denoised_base, bounce_frames=[0], segment_len=5, zone_size=1, do_bubble=False )        # 5 frames total, no bouncing, one continuous segment, no zones, no bubbling
#clip_denoised, TEST_CLIP_LEN = create_bouncing_test_clip( clip_denoised_base, bounce_frames=[0], segment_len=10, zone_size=1, do_bubble=False )       # 10 frames total, no bouncing, one segment, no zones, no bubbling
#clip_denoised, TEST_CLIP_LEN = create_bouncing_test_clip( clip_denoised_base, bounce_frames=[0], segment_len=20, zone_size=1, do_bubble=False )       # 20 frames total, no bouncing, one segment, no zones, no bubbling
#clip_denoised, TEST_CLIP_LEN = create_bouncing_test_clip( clip_denoised_base, bounce_frames=[0], segment_len=100, zone_size=1, do_bubble=False )      # 100 frames total, no bouncing, one segment, no zones, no bubbling
#clip_denoised, TEST_CLIP_LEN = create_bouncing_test_clip( clip_denoised_base, bounce_frames=[0], segment_len=200, zone_size=1, do_bubble=False )      # 200 frames total, no bouncing, one segment, no zones, no bubbling
#clip_denoised, TEST_CLIP_LEN = create_bouncing_test_clip( clip_denoised_base, bounce_frames=[0], segment_len=500, zone_size=1, do_bubble=False )      # 500 frames total, no bouncing, one segment, no zones, no bubbling
#
#clip_denoised, TEST_CLIP_LEN = create_bouncing_test_clip( clip_denoised_base, bounce_frames=[0], segment_len=5, zone_size=2, do_bubble=True )        # 5 frames total, random shuffle in zones of 2, bubbling on
#clip_denoised, TEST_CLIP_LEN = create_bouncing_test_clip( clip_denoised_base, bounce_frames=[0], segment_len=10, zone_size=2, do_bubble=True )       # 10 frames total, random shuffle in zones of 2, bubbling on
#clip_denoised, TEST_CLIP_LEN = create_bouncing_test_clip( clip_denoised_base, bounce_frames=[0], segment_len=20, zone_size=2, do_bubble=True )       # 20 frames total, random shuffle in zones of 2, bubbling on
#clip_denoised, TEST_CLIP_LEN = create_bouncing_test_clip( clip_denoised_base, bounce_frames=[0], segment_len=100, zone_size=2, do_bubble=True )      # 100 frames total, random shuffle in zones of 2, bubbling on
#clip_denoised, TEST_CLIP_LEN = create_bouncing_test_clip( clip_denoised_base, bounce_frames=[0], segment_len=200, zone_size=2, do_bubble=True )      # 200 frames total, random shuffle in zones of 2, bubbling on
#clip_denoised, TEST_CLIP_LEN = create_bouncing_test_clip( clip_denoised_base, bounce_frames=[0], segment_len=500, zone_size=2, do_bubble=True )      # 500 frames total, random shuffle in zones of 2, bubbling on
#
#clip_denoised, TEST_CLIP_LEN = create_bouncing_test_clip( clip_denoised_base, bounce_frames=[0], segment_len=5, zone_size=5, do_bubble=True )        # 5 frames total, random shuffle in zones of 5, bubbling on
#clip_denoised, TEST_CLIP_LEN = create_bouncing_test_clip( clip_denoised_base, bounce_frames=[0], segment_len=10, zone_size=5, do_bubble=True )       # 10 frames total, random shuffle in zones of 5, bubbling on
#clip_denoised, TEST_CLIP_LEN = create_bouncing_test_clip( clip_denoised_base, bounce_frames=[0], segment_len=20, zone_size=5, do_bubble=True )       # 20 frames total, random shuffle in zones of 5, bubbling on
#clip_denoised, TEST_CLIP_LEN = create_bouncing_test_clip( clip_denoised_base, bounce_frames=[0], segment_len=100, zone_size=5, do_bubble=True )      # 100 frames total, random shuffle in zones of 5, bubbling on
#clip_denoised, TEST_CLIP_LEN = create_bouncing_test_clip( clip_denoised_base, bounce_frames=[0], segment_len=200, zone_size=5, do_bubble=True )      # 200 frames total, random shuffle in zones of 5, bubbling on
#clip_denoised, TEST_CLIP_LEN = create_bouncing_test_clip( clip_denoised_base, bounce_frames=[0], segment_len=500, zone_size=5, do_bubble=True )      # 500 frames total, random shuffle in zones of 5, bubbling on
#
#clip_denoised, TEST_CLIP_LEN = create_bouncing_test_clip( clip_denoised_base, bounce_frames=[0, 3000, 1000, 2000], segment_len=200, zone_size=8, do_bubble=False ) # 800 frames total, 4 segments, sequential, no bubbling
#clip_denoised, TEST_CLIP_LEN = create_bouncing_test_clip( clip_denoised_base, bounce_frames=[0, 3000, 1000, 2000], segment_len=200, zone_size=8, do_bubble=True )  # 800 frames total, 4 segments, random bubbling

# --- END VALID CALL EXAMPLES WITH DESCRIPTIONS ---

# Output the final bubbled or non-bubbled clip
# Size of the final output clip will be TEST_CLIP_LEN frames.
clip_denoised.set_output()

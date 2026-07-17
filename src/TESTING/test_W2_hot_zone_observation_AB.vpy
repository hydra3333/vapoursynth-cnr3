# test_W2_hot_zone_observation_AB.vpy
#
# DESIGNER-OWNED proof harness for W.2 (CMS07.14 §7.6: live hot-zone observation at arInitial).
# Adapted from the golden request-order catalogue test_000_Example_576p50.vpy (the assemble_jump_segments
# request-order builder is reused verbatim) and the K.1F core-cache-defeat lesson.
#
# WHAT W.2 ADDS TO THE LIVE PATH:
#   A single record_hot_zone_observation(N) at the top of cnr3_arInitial (before the cache-hit lookup),
#   on EVERY getFrame branch, plus a temporary KDT line:
#       [KDT] instance=%d N=%d HOT-ZONE-OBSERVED status=%s
#   It is OBSERVE-ONLY: nothing in the live path consumes hot-zone state until W.3, so output bytes are
#   unchanged. W.2's proof is therefore two things, neither inferable from "it ran":
#     (1) BRANCH COVERAGE: the HOT-ZONE-OBSERVED line fires, with the right N, on each of the four branches;
#     (2) OUTPUT NEUTRALITY: the produced bytes are byte-identical to the pre-W.2 (W.1) build.
#
# WHY SetVideoCache(mode=0) IS REQUIRED HERE (K.1F lesson):
#   CNR3 registers fmUnordered with no no-cache flag, so the VS core inserts a cache node downstream of CNR3.
#   Without overriding it, a re-requested (already-produced) frame can be served by the VS CORE cache and
#   NEVER re-enter CNR3::getFrame -- so its arInitial observation would not fire and the cache-hit branch
#   would be invisible (a false gap). We call std.SetVideoCache(mode=0) on the CNR3 node (R76 mode=0 =
#   always disable) so EVERY request re-enters CNR3::getFrame and its arInitial observation is emitted.
#   IMPORTANT: SetVideoCache mutates the node IN PLACE and returns None -- call it for its side effect;
#   do NOT reassign (reassigning sets the node to None).
#
# THE SINGLE COMBINED SCENARIO (hits all four branches in one -r 1 run):
#   assemble_jump_segments(base, jump_to_frames=[0, 50, 2000], segment_length=60, shuffle=False)
#     segment 1  source 0..59    -> N=0   FRAME0-FRESH-START ; N=1..59  PREDECESSOR-PRESENT-COMPUTE
#                                   (lays down outputs 0..59; checkpoints at 0,10,20,30,40,50)
#     segment 2  source 50..109  -> N=50..59  CACHE-HIT (already present; re-entered via mode=0) ;
#                                   N=60..109  PREDECESSOR-PRESENT-COMPUTE
#     segment 3  source 2000..2059 -> N=2000  RECOVERY (floor-fresh-start: 1999 absent, floor=1950 barren) ;
#                                   N=2001..2059 PREDECESSOR-PRESENT-COMPUTE
#   EXPECTED OBSERVATION LINES (one HOT-ZONE-OBSERVED per requested N; the four branch-coverage anchors are):
#     HOT-ZONE-OBSERVED N=0     accompanies the frame-0 fresh-start branch
#     HOT-ZONE-OBSERVED N=1     accompanies a predecessor-present branch
#     HOT-ZONE-OBSERVED N=50    accompanies the cache-hit branch
#     HOT-ZONE-OBSERVED N=2000  accompanies the recovery (floor-fresh-start) branch
#   The .bat correlates each N's HOT-ZONE-OBSERVED line with that N's existing per-frame branch KDT line.
#
#   Run under  vspipe -r 1  (single request thread, deterministic order). NOT a concurrency test
#   (W.2 is single-activation per SR-D-04; concurrent observation is the fmParallel arc, FI-06/07).
#   shuffle_in_zones is OFF for determinism (byte-compare must be reproducible).
#
# DEV-TRACE GUARD: the temporary HOT-ZONE-OBSERVED KDT (and the predecessor/cache-hit/recover branch lines)
#   require the plugin built with CNR3_KEYSTONE_DEV_TRACE defined. The frame-0 branch correlation line is
#   ADDITIONALLY guarded on CNR3_KEYSTONE_LIVE_GETFRAME_FRAME0_PROOF (verified cnr3_arAllFramesReady.cpp:34),
#   so it is informational in the .bat (its absence is not a failure; N=0 -> frame-0 is unambiguous here).

from typing import List, Tuple
import vapoursynth as vs

core = vs.core

# --- Source: progressive 576p50 (single-instance path; W.2 proof regime is single-activation). ---
#   2000-frame reach below needs a clip of >= ~2060 frames; the golden 576p50 clip (~5500) suffices.
source_filename = r"D:\TEST\000_Example_576p50.mp4"

clip = core.bs.VideoSource(source_filename)
clip = core.std.SetFieldBased(clip, 0)  # force progressive single-instance path


def assemble_jump_segments(
    base_clip: vs.VideoNode,
    jump_to_frames: List[int],
    segment_length: int,
) -> Tuple[vs.VideoNode, int]:
    """Build a test clip by jumping to each frame in jump_to_frames and taking a
    segment_length-frame in-order segment from each, then splicing. Because VapourSynth
    pulls BACKWARD up the graph, CNR3 receives the intended jumping request sequence at its
    own getFrame. (Reused from test_000_Example_576p50.vpy; shuffle path dropped -- W.2 needs
    deterministic order for the byte-compare.)"""
    if segment_length <= 0:
        raise ValueError(f"assemble_jump_segments: segment_length must be > 0. Got {segment_length}.")
    if not jump_to_frames:
        jump_to_frames = [0]
    segments = []
    for start in jump_to_frames:
        if start < 0 or start >= base_clip.num_frames:
            raise ValueError(
                f"assemble_jump_segments: jump frame {start} out of bounds "
                f"for source ({base_clip.num_frames} frames)."
            )
        end_pos = min(start + segment_length, base_clip.num_frames)
        segments.append(base_clip[start:end_pos])
    final_clip = core.std.Splice(segments)
    return final_clip, final_clip.num_frames


# Apply CNR3 (single progressive instance), then DEFEAT the downstream VS core cache so every
# request re-enters CNR3::getFrame and its arInitial observation fires (K.1F mechanism).
clip_denoised_base = core.cnr3.CNR3(clip)
clip_denoised_base.std.SetVideoCache(mode=0)  # in place; returns None; do NOT reassign

# The single combined four-branch scenario.
clip_denoised, TEST_CLIP_LENGTH = assemble_jump_segments(
    clip_denoised_base,
    jump_to_frames=[0, 50, 2000],
    segment_length=60,
)

clip_denoised.set_output()

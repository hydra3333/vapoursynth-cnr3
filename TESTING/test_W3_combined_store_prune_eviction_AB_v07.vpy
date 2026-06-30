# test_W3_combined_store_prune_eviction_AB.vpy
#
# DESIGNER-OWNED proof harness for W.3 (CMS07.14 §7.5: combined live store-and-prune helper).
# Adapted from test_W2_hot_zone_observation_AB.vpy (same golden 576p50 source, same core-cache-defeat
# via SetVideoCache(mode=0), same vspipe -r 1 deterministic single-thread regime). The
# assemble_jump_segments builder is generalised to assemble_segments (per-segment lengths) because W.3
# needs ONE very long sequential segment (to cross the capacity trigger) plus short recovery segments.
#
# WHAT W.3 ADDS TO THE LIVE PATH (and therefore what must be proven):
#   W.3 is the FIRST phase in which the live output cache EVICTS. Production / AS2-floor / AS2-hole stores
#   are routed through three by-value public wrappers over one shared store→retire→prune critical section,
#   and a temporary per-invocation KDT line is emitted (verified token, cnr3_arAllFramesReady.cpp):
#     [KDT] instance=%d target_N=%d stored_frame=%d kind=%s store=%s retire=%s prune=%s \
#           cap_trigger=%d ckpt_trigger=%d selected=%zu detached=%zu
#
# THE PROOF SHAPE — DELIBERATELY DIFFERENT FROM W.2:
#   W.2 was OBSERVE-ONLY, so its A/B proof was byte-NEUTRALITY (W.2 bytes == W.1 bytes).
#   W.3 CHANGES cache state by design (it evicts), so neutrality of cache *contents* is NOT expected.
#   BUT a CORRECT eviction must still leave every RETURNED frame byte-identical to a non-evicting build,
#   because output[N] = f(source[N], output[N-1]): wrongly evicting a still-needed predecessor would
#   corrupt a returned frame or force a spurious recovery whose bytes differ. So the W.3 A/B proof is:
#     RUN A = W.2 baseline DLL (marker CMS07-W.2-hot-zone-observation-arInitial) — never evicts; the
#             perfect non-evicting reference (its cache simply grows unbounded over this scenario).
#     RUN B = W.3 DLL          (marker CMS07-W.3-combined-live-store-prune-helper) — evicts.
#   PASS REQUIRES BOTH, together (either alone is insufficient):
#     (1) RETURNED-FRAME CORRECTNESS:  fc /b A.y4m B.y4m  reports NO differences
#         -> W.3 eviction removed only no-longer-needed slots; it corrupted no returned frame.
#     (2) EVICTION OBSERVABLY FIRED (RUN B KDT):  cap_trigger=1 AND ckpt_trigger=1 AND detached>0 appear
#         -> without this, (1) would be a VACUOUS pass (identity is trivial if nothing was ever evicted).
#   (Plus: recovery is exercised under eviction pressure, and the AS2 floor/hole combined-helper kinds
#    are reported from the KDT kind= field.)
#
# WHY THE FIRST SEGMENT IS ~1300 FRAMES (the capacity-trigger arithmetic, verified against source):
#   capacity:   active_ceiling = clamp(CNR3_CACHE_BYTE_BUDGET_BYTES / frame_byte_count, [150, 1000])
#               overflow_trigger = active_ceiling * 11 / 10 ; prune fires when slot_count > overflow_trigger
#               For 576p50 4:2:0 8-bit: frame_byte_count = 720*576 + 2*(360*288) = 622,080 bytes
#               BYTE_BUDGET = 1,073,741,824 -> raw_ceiling = 1726 -> clamped to MAX 1000
#               => overflow_trigger = 1100. So the capacity trigger needs slot_count > 1100, i.e. a
#                  sequential run of ~1101+ frames. (The register's "165" is the LARGE-frame case where the
#                  ceiling clamps to the 150 MIN; 576p50 clamps to the 1000 MAX instead.)
#   checkpoint: checkpoint_count > CNR3_CACHE_CHECKPOINT_MAX_RETAIN (48). Checkpoints are laid every
#               CNR3_CACHE_CHECKPOINT_INTERVAL (10) frames (and at frame 0), so the 49th checkpoint lands
#               at frame 480 -> the checkpoint trigger fires from ~frame 480 onward, well inside segment 1.
#   So a single long sequential segment crosses BOTH triggers (checkpoint ~480, capacity ~1100) and then
#   sustains eviction to the segment end. If your build's constants or the clip's bit-depth/subsampling
#   differ, adjust CAPACITY_RUN_LENGTH so it comfortably exceeds your overflow_trigger.
#
#   This harness is therefore LONGER-RUNNING than W.2's 180-frame scenario (here ~1360 frames through CNR3
#   with mode=0). That is inherent to crossing the 576p50 capacity ceiling honestly with the real golden.
#
# DETERMINISM / REGIME: vspipe -r 1 (single request thread, deterministic order). Single-activation per
#   SR-D-04; concurrency is the fmParallel arc, not this harness. No shuffle (byte-compare must reproduce).
#
# DEV-TRACE GUARD: the W.3 KDT line requires both DLLs built with CNR3_KEYSTONE_DEV_TRACE. RUN A (W.2) will
#   NOT emit the W.3 [KDT ... cap_trigger] line (that line is new in W.3); the W.3 KDT checks run on RUN B
#   only, exactly as W.2's HOT-ZONE-OBSERVED checks ran on its RUN B.

from typing import List, Tuple
import vapoursynth as vs

core = vs.core

# --- Source: progressive 576p50 golden (same clip as the W.2 harness). ---
#   The far recovery jump below reaches source frame 4000, so the clip must have >= ~4030 frames.
#   The golden 576p50 clip (~5500) suffices. (A clip-length precondition is asserted below.)
source_filename = r"D:\TEST\000_Example_576p50.mp4"

# --- Scenario sizing (tune here if your build constants or clip format differ). ---
CAPACITY_RUN_LENGTH = 1300   # segment 1 length; must exceed overflow_trigger (1100 for 576p50 8-bit)
NEAR_RECOVERY_START = 1305   # just past the sequential frontier (1299): predecessor absent -> recovery
NEAR_RECOVERY_LENGTH = 30    # recovery with floor = nearest surviving checkpoint + holes + target
FAR_RECOVERY_START = 4000    # far cold region never computed: floor-fresh-start (barren) recovery
FAR_RECOVERY_LENGTH = 30

clip = core.bs.VideoSource(source_filename)
clip = core.std.SetFieldBased(clip, 0)  # force progressive single-instance path


def assemble_segments(
    base_clip: vs.VideoNode,
    segments: List[Tuple[int, int]],
) -> Tuple[vs.VideoNode, int]:
    """Build a test clip from explicit (start, length) segments, spliced in order. Because VapourSynth
    pulls BACKWARD up the graph, CNR3 receives the intended request sequence at its own getFrame, with
    its getFrame N equal to the source frame index of each segment entry. Generalisation of
    test_W2_hot_zone_observation_AB.vpy's assemble_jump_segments to per-segment lengths (W.3 needs one
    long capacity-crossing segment plus short recovery segments). No shuffle: byte-compare must reproduce."""
    if not segments:
        raise ValueError("assemble_segments: at least one (start, length) segment is required.")
    built = []
    for start, length in segments:
        if length <= 0:
            raise ValueError(f"assemble_segments: segment length must be > 0. Got {length} at start {start}.")
        if start < 0 or start >= base_clip.num_frames:
            raise ValueError(
                f"assemble_segments: segment start {start} out of bounds "
                f"for source ({base_clip.num_frames} frames)."
            )
        end_pos = min(start + length, base_clip.num_frames)
        built.append(base_clip[start:end_pos])
    final_clip = core.std.Splice(built)
    return final_clip, final_clip.num_frames


# Clip-length precondition: the far recovery jump must be in-bounds (fail LOUD, not silently truncated).
required_frames = FAR_RECOVERY_START + FAR_RECOVERY_LENGTH
if clip.num_frames < required_frames:
    raise ValueError(
        f"Source clip has {clip.num_frames} frames but the scenario needs >= {required_frames} "
        f"(far recovery jump reaches frame {FAR_RECOVERY_START}). Use a longer golden clip or lower "
        f"FAR_RECOVERY_START."
    )

# Apply CNR3 (single progressive instance), then DEFEAT the downstream VS core cache so every request
# re-enters CNR3::getFrame and its store/prune (and KDT) fires (K.1F / W.2 mechanism).
clip_denoised_base = core.cnr3.CNR3(clip)
clip_denoised_base.std.SetVideoCache(mode=0)  # in place; returns None; do NOT reassign

# The W.3 combined scenario:
#   segment 1  source 0 .. (CAPACITY_RUN_LENGTH-1)      long sequential run:
#                N=0 FRAME0-FRESH-START ; N=1.. PREDECESSOR-PRESENT-COMPUTE
#                checkpoint trigger fires ~frame 480 ; capacity trigger fires ~frame 1100 ; sustained
#                eviction to the segment end. THIS is where cap_trigger=1 / ckpt_trigger=1 / detached>0 appear.
#   segment 2  source 1305 .. 1334                       near-frontier recovery (predecessor 1304 absent):
#                floor = nearest surviving checkpoint (~1290), holes between, target 1305 -> exercises
#                store_as2_floor_and_prune + store_recovery_hole_and_prune UNDER eviction pressure ;
#                then PREDECESSOR-PRESENT for the rest.
#   segment 3  source 4000 .. 4029                       far cold recovery (floor-fresh-start, barren).
clip_denoised, TEST_CLIP_LENGTH = assemble_segments(
    clip_denoised_base,
    segments=[
        (0, CAPACITY_RUN_LENGTH),
        (NEAR_RECOVERY_START, NEAR_RECOVERY_LENGTH),
        (FAR_RECOVERY_START, FAR_RECOVERY_LENGTH),
    ],
)

clip_denoised.set_output()

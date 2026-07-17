# test_TINY_live_eviction_proof.vpy
#
# DESIGNER-OWNED (W3D) live proof harness for the TINY-100 diagnostic cache scaffold
# (CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY). Adapted from test_W3_combined_store_prune_eviction_AB.vpy:
# same golden 576p50 source, same core-cache-defeat via SetVideoCache(mode=0), same vspipe -r 1
# deterministic single-thread regime, same assemble_segments builder.
#
# WHAT THIS PROVES (and how it differs from the W.3 A/B harness):
#   The W.3 harness was a TWO-DLL A/B byte-compare (W.2 baseline vs W.3) proving eviction corrupted no
#   returned frame. THIS harness is a SINGLE run of the TINY DLL, and its job is narrower and different:
#   prove the tiny profile actually EVICTS on a SHORT run — i.e. the scaffold does what it exists to do.
#   Correctness of eviction was already proven by the W.3 arc (four-way 55/55 + the A/B harness) at
#   production scale; the tiny profile runs the SAME eviction code with smaller constants. So here we only
#   need to observe the triggers firing and victims detaching on a few-hundred-frame run.
#
#   The live DLL emits the W.3 KDT store-prune line (cnr3_arAllFramesReady.cpp), one per store invocation:
#     [KDT] instance=%d target_N=%d stored_frame=%d kind=%s store=%s retire=%s prune=%s \
#           cap_trigger=%d ckpt_trigger=%d selected=%zu detached=%zu
#   (If the coder adds profile=%s to that line — recommended — the .bat proves tiny-identity directly from
#    it. Until then, the .bat proves tiny-identity by INFERENCE: see the identity note below.)
#
# ACCEPTANCE (checked in the .bat; do NOT require the cache to return below active ceiling 100 —
# v0.2 says 101..110 between stores is expected hysteresis, not a failure):
#   - tiny identity proven (profile=tiny-100 if present; else cap_trigger=1 on this short run, which a
#     normal 576p50 DLL physically CANNOT produce — see identity note)
#   - cap_trigger=1 observed, preferably on a KDT line that also shows detached>0 (non-vacuity)
#   - ckpt_trigger=1 observed, preferably on a KDT line that also shows detached>0
#   - (INFO) branch=RECOVER observed — recovery exercised under eviction
#
# TINY-100 TRIGGER ARITHMETIC (verified against the tiny profile constants):
#   capacity:   active_ceiling = clamp(CNR3_CACHE_BYTE_BUDGET_BYTES / frame_byte_count, [MIN, MAX])
#               TINY-100: MIN=40, MAX=100. For 576p50 4:2:0 8-bit, frame_byte_count = 622,080 and
#               BYTE_BUDGET = 1,073,741,824 -> raw_ceiling 1726 -> clamped to MAX = 100.
#               overflow_trigger = 100 * 11 / 10 = 110. Capacity prune fires when slot_count > 110,
#               i.e. from ~frame 111 of the sequential segment, then sustains (sawtooth 101..110).
#   checkpoint: checkpoint_count > CNR3_CACHE_CHECKPOINT_MAX_RETAIN (TINY 12). Checkpoints are laid every
#               CNR3_CACHE_CHECKPOINT_INTERVAL (TINY 3) frames (and at frame 0), so the 13th checkpoint
#               lands ~frame 36 -> the checkpoint trigger fires from ~frame 36 onward.
#   So a single ~200-frame sequential segment crosses BOTH tiny triggers (checkpoint ~36, capacity ~111)
#   with wide margin and sustains eviction to its end. (Contrast: the production profile needs ~1300.)
#
# IDENTITY NOTE (the 0E/0F lesson from W.3): the live DLL does NOT emit CNR3_CACHE_PROFILE_NAME today
#   (it is emitted only by the selftest binary). So tiny-identity is proven by INFERENCE unless/until the
#   coder adds profile=%s to the live KDT line: on the 576p50 golden, a NORMAL DLL's ceiling clamps to
#   1000 (trigger 1100), so it CANNOT emit cap_trigger=1 within a ~200-frame run — only the TINY DLL
#   (trigger 110) can. Hence cap_trigger=1 on this short 576p50 run is itself proof the tiny DLL is loaded.
#
# DETERMINISM / REGIME: vspipe -r 1 (single request thread, deterministic order). Single-activation per
#   SR-D-04; concurrency is the fmParallel arc, not this harness.
#
# DEV-TRACE GUARD: the KDT line requires the tiny DLL built with CNR3_KEYSTONE_DEV_TRACE (so cap_trigger
#   etc. are emitted). Build the tiny DLL with BOTH CNR3_SCAFFOLD_TINYCACHE_FOR_DIAGS_ONLY and
#   CNR3_KEYSTONE_DEV_TRACE defined.

from typing import List, Tuple
import vapoursynth as vs

core = vs.core

# --- Source: progressive 576p50 golden (same clip as the W.2 / W.3 harnesses). ---
source_filename = r"D:\TEST\000_Example_576p50.mp4"

# --- Scenario sizing (tuned for the TINY-100 triggers; retune if constants/clip format differ). ---
CAPACITY_RUN_LENGTH = 200    # segment 1 length; must exceed the tiny overflow_trigger (110). 200 gives
                             # ~89 frames of sustained eviction after the trigger, i.e. many cap-prune cycles.
NEAR_RECOVERY_START = 205    # just past the sequential frontier (199): predecessor absent -> recovery
NEAR_RECOVERY_LENGTH = 30    # floor = nearest surviving checkpoint + holes + target, UNDER eviction
FAR_RECOVERY_START = 500     # far cold region never computed: floor-fresh-start (barren) recovery
FAR_RECOVERY_LENGTH = 30

clip = core.bs.VideoSource(source_filename)
clip = core.std.SetFieldBased(clip, 0)  # force progressive single-instance path


def assemble_segments(
    base_clip: vs.VideoNode,
    segments: List[Tuple[int, int]],
) -> Tuple[vs.VideoNode, int]:
    """Build a test clip from explicit (start, length) segments spliced in order, so CNR3's getFrame N
    equals the source frame index of each segment entry. Same builder as the W.3 harness. No shuffle."""
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

# The tiny live scenario:
#   segment 1  source 0 .. 199        long sequential run: crosses the tiny checkpoint trigger (~36) and
#                                     the tiny capacity trigger (~111), then sustains eviction to frame 199.
#                                     THIS is where cap_trigger=1 / ckpt_trigger=1 / detached>0 appear.
#   segment 2  source 205 .. 234      near-frontier recovery (predecessor 204 absent) UNDER eviction.
#   segment 3  source 500 .. 529      far cold recovery (floor-fresh-start, barren).
clip_denoised, TEST_CLIP_LENGTH = assemble_segments(
    clip_denoised_base,
    segments=[
        (0, CAPACITY_RUN_LENGTH),
        (NEAR_RECOVERY_START, NEAR_RECOVERY_LENGTH),
        (FAR_RECOVERY_START, FAR_RECOVERY_LENGTH),
    ],
)

clip_denoised.set_output()

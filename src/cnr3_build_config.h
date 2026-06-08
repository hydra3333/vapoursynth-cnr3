#pragma once

// -----------------------------------------------------------------------------
// CNR3 build-time diagnostic configuration
//
// These switches are compile-time controls for development diagnostics.
//
// Code guarded with if constexpr using these constants is compiled out of the
// generated executable code when the relevant constant is false.
//
// These diagnostics must not be required for correctness.
// -----------------------------------------------------------------------------

/*
    Edit/version marker printed in debug logs so test output can confirm that
    the expected source edit was compiled and loaded.

    Update this string for each coherent source-change set.
*/
inline constexpr const char* CNR3_EDIT_VERSION = 
    "CMS02-H2B-bounded-checkpoint-search-helper-proof-v1-PASSED";

/*
    Temporary CMS02-F proof hook.

    When true, cnr3_get_frame() performs an immediate post-store lookup of the
    frame it just stored, using the real CMS02-F find-and-addref helper. It then
    releases the caller-owned lookup reference immediately.

    This is for debug proof only. It must be set false or removed after the
    lookup/addref/release path is proven.
*/
inline constexpr bool CNR3_FOR_DEBUG_ONLY_FORCE_CACHE_LOOKUP_PROBE = false;

/*
    CMS02-G recovery-plan skeleton gate.

    This keeps early recovery-planning helpers available for compile-time and
    diagnostic proving without changing runtime frame processing. The first
    skeleton is deliberately not wired into cnr3_get_frame().

    Set true only in a dedicated proof patch, then disable again after the
    recovery-plan pin/unpin path is proven.
*/
inline constexpr bool CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_PLAN_SKELETON = false;

/*
    CMS02-G recovery-walk skeleton gate.

    This is a later debug-only scaffold layered above the recovery-plan helper.
    It logs the checkpoint-to-request walk range that recovery would need, but
    must not recompute frames, store recovered frames, return recovered frames,
    or change strict-streaming behaviour.

    The proof path requires both this flag and
    CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_PLAN_SKELETON to be true. Keep both false
    outside a dedicated proof run.
*/
inline constexpr bool CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_WALK_SKELETON = false;

/*
    CMS02-G recovery-start reference skeleton gate.

    This proves that a future recovery path can obtain a caller-owned reference
    to the selected checkpoint output frame, then release it cleanly.

    It must not recompute frames, store recovered frames, return recovered
    frames, or change strict-streaming behaviour.

    The proof path requires this flag and
    CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_PLAN_SKELETON to be true. Keep both false
    outside a dedicated proof run.
*/
inline constexpr bool CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_START_REF_SKELETON = false;

/*
    CMS02-G per-invocation source-request-plan skeleton gate.

    This proves the frameData shape needed for a future recovery path to carry
    source-frame request planning from arInitial to arAllFramesReady.

    It must not recompute frames, store recovered frames, return recovered
    frames, change output authority, or enable any parallel VapourSynth mode.

    Keep false outside a dedicated proof run.
*/
inline constexpr bool CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_SOURCE_REQUEST_PLAN_SKELETON = false;

/*
    CMS02-G.8 recovery decision/walk skeleton gate.

    This proof-only scaffold logs the future bounded recovery walk decisions:
        - selected checkpoint
        - walk range
        - whether each frame would use cache or be recomputed
        - whether the needed source frame is covered by the frameData request plan

    It must not recompute outputs, store recovered outputs, return recovered
    outputs, change output authority, or enable any parallel VapourSynth mode.

    Keep false outside a dedicated proof run.
*/
inline constexpr bool CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_DECISION_WALK_SKELETON = false;

/*
    CMS02-G.9 recovery source-frame-set skeleton gate.

    This proof-only scaffold retrieves, holds, and releases the source frames
    needed for the future checkpoint-to-request recovery walk.

    It must not recompute outputs, store recovered outputs, return recovered
    outputs, change output authority, or enable any parallel VapourSynth mode.

    Keep false outside a dedicated proof run.
*/
inline constexpr bool CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_SOURCE_FRAME_SET_SKELETON = false;

/*
    CMS02-G.10ABC recovery compute dry-run skeleton gate.

    This proof-only scaffold logs the future recovery compute orchestration
    while the local G.9 source-frame set is still held.

    It must not allocate recovered output frames, compute recovered pixels,
    call process_cnr3_frame() for recovery, store recovered outputs, return
    recovered outputs, change output authority, mutate old strict-streaming
    state, or enable any parallel VapourSynth mode.

    Keep false outside a dedicated proof run.
*/
inline constexpr bool CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_COMPUTE_DRY_RUN_SKELETON = false;

/*
    CMS02-G.10D.1 local single-frame recovery compute proof gate.

    This proof-only scaffold computes at most one local recovered frame when
    the selected checkpoint is the immediate predecessor. The recovered frame is
    released immediately and is never stored, returned, or made authoritative.

    It must not call process_cnr3_frame() for recovery, store recovered outputs,
    return recovered outputs, change output authority, mutate old strict-streaming
    state, or enable any parallel VapourSynth mode.

    Keep false outside a dedicated proof run.
*/
inline constexpr bool CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_LOCAL_SINGLE_COMPUTE_PROOF = false;

/*
    CMS02-G.10D.2 local bounded recovery-walk compute proof gate.

    This proof-only scaffold walks from the selected checkpoint to the requested
    frame, using cached outputs where available and computing missing outputs
    locally with rolling predecessor ownership.

    Locally computed recovered frames are only used as temporary predecessors
    inside the proof walk. They are never stored, returned, or made authoritative.

    It must not call process_cnr3_frame() for recovery, store recovered outputs,
    return recovered outputs, change output authority, mutate old strict-streaming
    state, or enable any parallel VapourSynth mode.

    Keep false outside a dedicated proof run.
*/
inline constexpr bool CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_LOCAL_BOUNDED_WALK_COMPUTE_PROOF = false;

/*
    CMS02-G.10D.5 local bounded recovery-walk store proof gate.

    This proof-only scaffold stores locally computed recovered outputs into
    output_cache after successful local recovery computation.

    Stored recovered outputs are never returned in this phase and output_cache
    remains non-authoritative. The normal strict-streaming path still returns
    the frame produced by the normal path.

    If recovery proof stores frame N before the normal strict-streaming path
    stores frame N, the later normal store must be a safe first-in-best-dressed
    duplicate/no-op. It must not replace the cached frame, leak references, or
    count as a store failure.

    It must not return recovered outputs, change output authority, mutate old
    strict-streaming state, or enable any parallel VapourSynth mode.

    Keep false outside a dedicated proof run.
*/
inline constexpr bool CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_LOCAL_BOUNDED_WALK_STORE_PROOF = false;

/*
    CMS02-G.10D.6 recovery-store difference-measurement proof gate.

    This proof-only scaffold compares a recovery-stored cached output frame
    against the normal strict-streaming output frame before the normal path's
    duplicate store no-op.

    Sample differences are measured and reported. They do not fail the proof by
    themselves, because bounded recovery from a checkpoint can legitimately
    produce subtly different recursive history from full strict-streaming output.

    Structural failures such as lookup failure, dimension mismatch, unsupported
    sample size, or lookup-reference cleanup failure remain proof failures.

    It must not return recovered outputs, change output authority, mutate old
    strict-streaming state, or enable any parallel VapourSynth mode.

    Keep false outside a dedicated proof run.
*/
inline constexpr bool CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_STORE_DIFFERENCE_MEASUREMENT_PROOF = false;

/*
    CMS02-G.10D.7 recovery-return decision dry-run gate.

    This proof-only scaffold looks up the recovery-stored cached output that a
    future output-authoritative path could return, records whether it would be
    returnable, then releases the caller-owned lookup reference.

    It must not return recovered outputs, transfer lookup references to
    VapourSynth, change output authority, skip normal strict-path computation,
    mutate old strict-streaming state, or enable any parallel VapourSynth mode.

    Keep false outside a dedicated proof run.
*/
inline constexpr bool CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_RETURN_DECISION_DRY_RUN = false;

/*
    CMS02-G.10D.9 recovery-return transfer proof gate.

    This proof-only scaffold looks up a recovery-stored cached output frame,
    marks the caller-owned lookup reference as transferred, and returns that
    reference to VapourSynth.

    This proves transfer mechanics only. It does not make recovery output
    generally authoritative, does not enable a production recovery-return
    policy, does not use exact_match as a return condition, does not mutate old
    strict-streaming state from the recovery-return path, and does not enable
    any parallel VapourSynth mode.

    Keep false outside a dedicated proof run.
*/
inline constexpr bool CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_RETURN_TRANSFER_PROOF = false;

/*
    CMS02-H.2 bounded warm-up no-prior-checkpoint decision scaffold gate.

    This proof-only scaffold detects when no prior checkpoint is available,
    calculates the bounded warm-up range that a future warm-up recovery path
    would need, and records scan-friendly diagnostics.

    It must not request extra frames, retrieve source frames, compute warm-up
    outputs, store warm-up outputs, return warm-up outputs, change output
    authority, use exact_match as a return condition, mutate old strict-streaming
    state, or enable any parallel VapourSynth mode.

    Keep false outside a dedicated proof run.
*/
inline constexpr bool CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_DECISION_SCAFFOLD = false;

/*
    CMS02-H.2B bounded checkpoint-search helper proof gate.

    This proof-only scaffold runs after the normal output-cache store/prune path
    and proves that bounded recovery planning searches only inside the bounded
    checkpoint interval before pinning.

    It must not request extra frames, retrieve source frames, compute warm-up
    outputs, store warm-up outputs beyond the normal path, return warm-up outputs,
    change output authority, mutate old strict-streaming state, or enable any
    parallel VapourSynth mode.

    Keep false outside a dedicated proof run.
*/
inline constexpr bool CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_CHECKPOINT_SEARCH_PROOF = false;

/*
    Small diagnostic bound for the CMS02-H.2B bounded checkpoint-search proof.

    This is intentionally smaller than the normal recovery bound so a short
    sequential proof run can show both available checkpoint-start plans and
    bounded warm-up-needed decisions.
*/
static constexpr int CNR3_FOR_DEBUG_ONLY_BOUNDED_CHECKPOINT_SEARCH_PROOF_BOUND = 2;

/*
    Maximum forward distance allowed by the first bounded recovery-plan helper.
    perform recovery, recomputation, or frame generation.
*/
static constexpr int CNR3_RECOVERY_MAX_FORWARD_FRAMES = 50;

/*
    CMS02-G.7C source-request proof range.

    When CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_SOURCE_REQUEST_PLAN_SKELETON is
    true, arInitial requests this many predecessor source frames plus the normal
    requested source frame.

    Normal disabled-state value:
        2

    Enabled whole-walk proof runs:
        Temporarily switch to CNR3_RECOVERY_MAX_FORWARD_FRAMES when the active
        proof validates all source frames from checkpoint+1 to requested.

    This is proof-only request/retrieve scaffolding. It must not recompute
    outputs, store recovered outputs, return recovered outputs, or change output
    authority.
*/
static constexpr int CNR3_FOR_DEBUG_ONLY_RECOVERY_SOURCE_REQUEST_BACK_FRAMES = 2;
// static constexpr int CNR3_FOR_DEBUG_ONLY_RECOVERY_SOURCE_REQUEST_BACK_FRAMES = CNR3_RECOVERY_MAX_FORWARD_FRAMES;

/*
    Master development diagnostics switch.

    This is intended for diagnostics that are useful during CNR3 development,
    maintenance, and runtime-behaviour proving.

    Individual diagnostic areas may default from this switch while still being
    independently controllable below.
*/
constexpr bool CNR3_DEV_DIAGNOSTICS = true;

/*
    Cache-manager development diagnostics.

    Defaults to CNR3_DEV_DIAGNOSTICS, but may be set independently if
    cache-manager diagnostics need to be enabled or disabled separately from
    other CNR3 development diagnostics.
*/
constexpr bool CNR3_OUTPUT_CACHE_DEV_DIAGNOSTICS =
    CNR3_DEV_DIAGNOSTICS;

/*
    Memory diagnostics.

    Defaults to CNR3_DEV_DIAGNOSTICS, but may be set independently because memory
    diagnostics can be more expensive and noisier than normal debug output.

    These diagnostics are intended to help correlate CNR3/VapourSynth runtime
    behaviour with process and system memory use.
*/
constexpr bool CNR3_MEMORY_DIAGNOSTICS =
    CNR3_DEV_DIAGNOSTICS;

/*
    Memory diagnostics periodic frame interval.

    When debug=1 is active, a memory snapshot is printed every time
    frame_number > 0 and frame_number % CNR3_MEMORY_DIAG_FRAME_INTERVAL == 0.

    Set to 0 to disable periodic in-run snapshots entirely.
*/
// static constexpr int CNR3_MEMORY_DIAG_FRAME_INTERVAL = 500;
static constexpr int CNR3_MEMORY_DIAG_FRAME_INTERVAL = 500;

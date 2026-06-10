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
"CMS02-H14.3-selected-authority-log-reason-cleanup-v1-ENABLED";

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
    CMS02-H.3 bounded warm-up source-request-plan scaffold gate.

    This proof-only scaffold runs after the normal output-cache store/prune path,
    asks whether an interval-bounded checkpoint-start plan is available, and if
    not, derives the source-frame range a future bounded warm-up recovery path
    would need.

    It must not request source frames, retrieve source frames, hold source
    frames, compute warm-up outputs, store warm-up outputs beyond the normal
    path, return warm-up outputs, change output authority, mutate old
    strict-streaming state, or enable any parallel VapourSynth mode.

    Keep false outside a dedicated proof run.
*/
inline constexpr bool CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_SOURCE_REQUEST_PLAN_SCAFFOLD = false;

/*
    Small diagnostic bound for the CMS02-H.3 bounded warm-up source-request-plan
    scaffold.

    This deliberately matches the H2B diagnostic bound so the 20-frame proof run
    produces the same checkpoint-plan-available versus warm-up-needed pattern.
*/
static constexpr int CNR3_FOR_DEBUG_ONLY_BOUNDED_WARMUP_SOURCE_REQUEST_PLAN_PROOF_BOUND = 2;

/*
    CMS02-H.4 bounded warm-up source-frame-set request/acquire/release gate.

    This proof-only scaffold uses a dedicated H4 plan structure. It requests the
    conservative bounded warm-up source window in arInitial, then retrieves,
    holds, and releases those same source frames in arAllFramesReady.

    This follows VapourSynth's arInitial/arAllFramesReady lifecycle rule:
    any source frame retrieved with getFrameFilter() in arAllFramesReady must
    have been requested with requestFrameFilter() in arInitial of the same
    callback activation.

    It must not reuse the G-phase recovery source-request-plan structure, alter
    H3, compute warm-up outputs, store warm-up outputs, return warm-up outputs,
    change output authority, mutate old strict-streaming state, or enable any
    parallel VapourSynth mode.

    Keep false outside a dedicated proof run.
*/
inline constexpr bool CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_SOURCE_FRAME_SET_PROOF = false;

/*
    Small diagnostic bound for the CMS02-H.4 bounded warm-up source-frame-set
    request/acquire/release proof.

    With a 20-frame sequential proof run and this bound set to 2, the expected
    source-frame request/retrieve/release totals are:
        frame 0:  1
        frame 1:  2
        frames 2-19: 18 * 3
        total: 57
*/
static constexpr int CNR3_FOR_DEBUG_ONLY_BOUNDED_WARMUP_SOURCE_FRAME_SET_PROOF_BOUND = 2;

/*
    CMS02-H.5 bounded warm-up local-compute proof gate.

    This proof-only scaffold uses the H4-proven source-frame request/retrieve
    lifecycle, then locally computes bounded warm-up outputs from S..N. The
    start frame S is initialised through an explicit bounded-warm-up reset/copy
    policy. Frames after S use process_cnr3_frame_with_explicit_previous_output()
    with the previous local output frame.

    It must not call process_cnr3_frame() for warm-up compute, duplicate pixel
    algorithms, store warm-up outputs, return warm-up outputs, change output
    authority, mutate old strict-streaming state, or enable any parallel
    VapourSynth mode.

    Keep false outside a dedicated proof run.
*/
inline constexpr bool CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_LOCAL_COMPUTE_PROOF = false;

/*
    Small diagnostic bound for the CMS02-H.5 bounded warm-up local-compute
    proof. With a 20-frame sequential proof run and this bound set to 2, the
    expected source/local-output totals are 57. Frames 0..2 start from frame 0;
    frames 3..19 use an explicit nonzero bounded reset/copy start.
*/
static constexpr int CNR3_FOR_DEBUG_ONLY_BOUNDED_WARMUP_LOCAL_COMPUTE_PROOF_BOUND = 2;

/*
    CMS02-H.6 bounded warm-up store proof gate.

    This proof-only scaffold uses a new H6 compute-and-store helper rather than
    changing the proven H5 local-compute helper. It reuses the H4/H5 source
    lifecycle and existing explicit-predecessor processing boundary, then stores
    locally computed bounded warm-up outputs into output_cache.

    H6 must not return warm-up outputs, change output authority, mutate old
    strict-streaming state, or enable any parallel VapourSynth mode. While this
    gate is enabled, cache-hit return is deliberately bypassed so H6-stored
    proof frames cannot become returned output in the same proof run.

    Keep false outside a dedicated proof run.
*/
inline constexpr bool CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_STORE_PROOF = false;

/*
    Small diagnostic bound for the CMS02-H.6 bounded warm-up store proof.

    The first H6 proof uses the same bound as H5 so the local compute/store
    total remains comparable to the proven 57-frame H4/H5 ownership run.
*/
static constexpr int CNR3_FOR_DEBUG_ONLY_BOUNDED_WARMUP_STORE_PROOF_BOUND = 2;

/*
    CMS02-H.7 bounded warm-up return-decision dry-run gate.

    This proof-only scaffold reuses the H6 compute-and-store path to make sure
    bounded warm-up output[N] is present in output_cache, then performs only a
    dry-run return decision for the requested frame. It may take and release a
    caller-owned lookup reference to prove candidate availability and ownership
    accounting.

    H7 must not return warm-up outputs, transfer a frame reference to
    VapourSynth, change output authority, mutate old strict-streaming state, or
    enable any parallel VapourSynth mode. While this gate is enabled, cache-hit
    return is deliberately bypassed so H7 remains decision-only.

    Keep false outside a dedicated proof run.
*/
inline constexpr bool CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_DECISION_DRY_RUN = false;

/*
    Small diagnostic bound for the CMS02-H.7 bounded warm-up return-decision
    dry-run proof. H7 uses the same bound as H6 so store and decision evidence
    can be compared directly with the H6 proof run.
*/
static constexpr int CNR3_FOR_DEBUG_ONLY_BOUNDED_WARMUP_RETURN_DECISION_DRY_RUN_BOUND = 2;

/*
    CMS02-H.8 bounded warm-up return-transfer proof gate.

    This proof-only scaffold reuses the H6 compute-and-store path to make sure
    bounded warm-up output[N] is present in output_cache, then actually returns
    the caller-owned lookup reference for output[N] to VapourSynth under this
    proof gate.

    H8 is the first bounded warm-up phase that transfers a lookup reference to
    VapourSynth as the returned frame. It must prove that the transferred lookup
    reference is not also released locally. It must not mutate old strict-streaming
    state or enable any parallel VapourSynth mode. Full output-authority cleanup
    remains a later phase.

    Keep false outside a dedicated proof run.
*/
inline constexpr bool CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_TRANSFER_PROOF = false;

/*
    Small diagnostic bound for the CMS02-H.8 bounded warm-up return-transfer
    proof. H8 uses the same bound as H6/H7 so store, decision, and transfer
    evidence can be compared directly.
*/
static constexpr int CNR3_FOR_DEBUG_ONLY_BOUNDED_WARMUP_RETURN_TRANSFER_PROOF_BOUND = 2;

/*
    Temporary CMS02-H9 bounded warm-up authority-integration proof gate.

    H9 keeps the H8 return-transfer mechanism proof-gated, but treats it as the
    candidate output-authoritative path for the invocation. It proves that the
    bounded warm-up/cache candidate can be returned directly while the old
    strict-streaming state is left untouched.

    This is still a proof gate. It must not be left enabled in committed normal
    builds. Old strict-cache retirement and final fmParallel readiness remain
    later phases.
*/
inline constexpr bool CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_AUTHORITY_INTEGRATION_PROOF = false;

/*
    Small diagnostic bound for the CMS02-H9 authority-integration proof.
    Keep aligned with H6/H7/H8 so the proof counts remain directly comparable.
*/
static constexpr int CNR3_FOR_DEBUG_ONLY_BOUNDED_WARMUP_AUTHORITY_INTEGRATION_PROOF_BOUND = 2;


/*
    Temporary CMS02-H10 old strict-state bypass proof gate.

    H10 keeps the bounded warm-up/cache authority path proof-gated, but adds
    explicit diagnostics proving that old_strict_cache is bypassed as an output
    authority. The proof checks that old_strict_cache.next_needed and
    old_strict_cache.prev_output are not changed by the bounded warm-up
    authority path.

    This is still a proof gate. It must not be left enabled in committed normal
    builds. Old strict-cache deletion/retirement remains a later phase.
*/
inline constexpr bool CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_OLD_STRICT_BYPASS_PROOF = false;

/*
    Small diagnostic bound for the CMS02-H10 old strict-state bypass proof.
    Keep aligned with H6/H7/H8/H9 so the proof counts remain directly comparable.
*/
static constexpr int CNR3_FOR_DEBUG_ONLY_BOUNDED_WARMUP_OLD_STRICT_BYPASS_PROOF_BOUND = 2;

/*
    Temporary CMS02-H11 old strict-state quarantine proof gate.

    H11 keeps the bounded warm-up/cache authority path proof-gated and proves
    that old_strict_cache is not part of output authority while the new path is
    active. It treats old strict state as quarantined: readable for diagnostics,
    but not a producer, source of predecessor authority, or return authority.

    This is still a proof gate. It must not be left enabled in committed normal
    builds. Physical deletion/retirement of old strict-cache code remains a
    later, separate cleanup phase.
*/
inline constexpr bool CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_OLD_STRICT_QUARANTINE_PROOF = false;

/*
    Small diagnostic bound for the CMS02-H11 old strict-state quarantine proof.
    Keep aligned with H6/H7/H8/H9/H10 so the proof counts remain directly
    comparable.
*/
static constexpr int CNR3_FOR_DEBUG_ONLY_BOUNDED_WARMUP_OLD_STRICT_QUARANTINE_PROOF_BOUND = 2;

/*
    Temporary CMS02-H13 old strict streaming-gate quarantine proof gate.

    H13 reuses the bounded warm-up/cache authority path proved by H8-H11, but
    targets the remaining old strict streaming gate that rejects non-monotonic
    requests through next_needed. It proves that a deterministic out-of-order
    request can be handled by the bounded warm-up/output-cache authority path
    before the old strict streaming rejection path is reached.

    This is still a proof gate. It must not be left enabled in committed normal
    builds. Old strict-cache deletion/retirement remains a later, separate
    cleanup phase.
*/
inline constexpr bool CNR3_FOR_DEBUG_ONLY_ENABLE_OLD_STRICT_STREAMING_GATE_QUARANTINE_PROOF = false;

/*
    Small diagnostic bound for the CMS02-H13 old strict streaming-gate
    quarantine proof. Keep aligned with H6-H11 so the proof counts remain
    directly comparable.
*/
static constexpr int CNR3_FOR_DEBUG_ONLY_OLD_STRICT_STREAMING_GATE_QUARANTINE_PROOF_BOUND = 2;

/*
    Temporary CMS02-H14 output-cache authority cutover scaffold.

    This is a migration gate only, not a public mode and not a long-term
    authority selector.  It exists so the H13-proven output-cache authority
    path can be promoted and validated in small steps before old strict
    streaming authority is retired.

    H14.2 enables this in a proof build to promote the H13-proven bounded
    warm-up/output-cache authority mechanics into the selected cutover path.
    This gate remains temporary and must be returned to false for committed
    default behaviour until the cutover has passed wider validation.
*/
inline constexpr bool CNR3_CMS02_H14_TEMP_ENABLE_OUTPUT_CACHE_AUTHORITY_CUTOVER = true;

/*
    Small diagnostic bound for the CMS02-H14 output-cache authority cutover.
    Keep aligned with H6-H13 so the proof counts remain directly comparable.
*/
static constexpr int CNR3_CMS02_H14_OUTPUT_CACHE_AUTHORITY_CUTOVER_BOUND = 2;

static_assert(
    (
        (CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_SOURCE_FRAME_SET_PROOF ? 1 : 0) +
        (CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_LOCAL_COMPUTE_PROOF ? 1 : 0) +
        (CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_STORE_PROOF ? 1 : 0) +
        (CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_DECISION_DRY_RUN ? 1 : 0) +
        (CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_RETURN_TRANSFER_PROOF ? 1 : 0) +
        (CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_AUTHORITY_INTEGRATION_PROOF ? 1 : 0) +
        (CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_OLD_STRICT_BYPASS_PROOF ? 1 : 0) +
        (CNR3_FOR_DEBUG_ONLY_ENABLE_BOUNDED_WARMUP_OLD_STRICT_QUARANTINE_PROOF ? 1 : 0) +
        (CNR3_FOR_DEBUG_ONLY_ENABLE_OLD_STRICT_STREAMING_GATE_QUARANTINE_PROOF ? 1 : 0)
        ) <= 1,
    "Only one CMS02-H bounded warm-up proof gate may be enabled at a time."
    );

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

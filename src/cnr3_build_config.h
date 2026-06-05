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
inline constexpr const char* CNR3_EDIT_VERSION = "CMS02-G8A-recovery-decision-walk-skeleton-disabled-v1";
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
    CMS02-G.7C widened source-request proof range.

    When CNR3_FOR_DEBUG_ONLY_ENABLE_RECOVERY_SOURCE_REQUEST_PLAN_SKELETON is
    true, arInitial requests this many predecessor source frames plus the normal
    requested source frame.

    This is proof-only request/retrieve scaffolding. It must not recompute
    outputs, store recovered outputs, return recovered outputs, or change output
    authority.
*/
static constexpr int CNR3_FOR_DEBUG_ONLY_RECOVERY_SOURCE_REQUEST_BACK_FRAMES = 2;

/*
    Maximum forward distance allowed by the first bounded recovery-plan helper.    The helper treats the value as a safety bound only. It does not itself
    perform recovery, recomputation, or frame generation.
*/
static constexpr int CNR3_RECOVERY_MAX_FORWARD_FRAMES = 50;

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

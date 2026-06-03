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
    "PRE-CMS02-G-memdiag-format-v2";

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
static constexpr int CNR3_MEMORY_DIAG_FRAME_INTERVAL = 5;

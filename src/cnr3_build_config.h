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
constexpr bool CNR3_CACHE_MANAGER_DEV_DIAGNOSTICS =
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

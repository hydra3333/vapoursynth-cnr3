#pragma once

#include "cnr3_common.h"

/*
    CNR3 per-instance configuration scaffold.

    CMS07-B.2.2 introduces only the smallest useful instance configuration
    boundary:
        - a per-instance diagnostic ID;
        - a validity helper;
        - no VapourSynth parsing;
        - no cache state;
        - no pixel-processing parameters;
        - no old Cnr3Data shape.

    This module is the future home for parsed user options and clip-derived
    configuration that are genuinely per instance. It must not become a cache
    owner, diagnostics accumulator, frameData object, or pixel-processing
    implementation.

    Cache ownership, slots, pins, checkpoints, hot zones, prune, and recovery
    state belong in cnr3_cache_core.*.
*/

/*
    Allocate a new per-instance diagnostic ID.

    The ID is used only to distinguish simultaneous CNR3 instances in stderr
    diagnostics and future proof summaries. It must not be used to share runtime
    state between instances.
*/
[[nodiscard]] Cnr3InstanceId cnr3_allocate_instance_id() noexcept;

/*
    Minimal configuration object for this phase.

    Later phases may add parsed user parameters and clip-derived constants here
    when those fields are introduced deliberately. Do not pre-populate this
    structure with old Cnr3Data fields.
*/
struct Cnr3InstanceConfig {
    Cnr3InstanceId instance_id{};
};

/*
    Construct a minimal default instance configuration.

    This does not parse user parameters, inspect VapourSynth formats, allocate
    cache state, or initialise diagnostics accumulators.
*/
[[nodiscard]] Cnr3InstanceConfig cnr3_make_default_instance_config() noexcept;

/*
    Validate only the fields that exist in CMS07-B.2.2.

    A true result means the scaffold is mechanically valid. It does not mean the
    eventual filter parameters, input format, cache, or pixel path have been
    validated.
*/
[[nodiscard]] bool cnr3_instance_config_is_valid(
    const Cnr3InstanceConfig& config
) noexcept;

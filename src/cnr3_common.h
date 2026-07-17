#pragma once

#include <cstdint>

#include "cnr3_build_config.h"

/*
    CNR3 common shared definitions.

    CMS07-B.2.1 keeps this header deliberately small.

    This header may contain:
        - tiny shared mechanical types;
        - shared status/result codes;
        - tiny constexpr helpers that are genuinely cross-module.

    This header must not contain:
        - VapourSynth headers;
        - old strict streaming cache state;
        - CMS06 output cache-manager state;
        - old proof-phase fields;
        - pixel-processing tables;
        - memory diagnostics accumulators;
        - monolithic per-instance runtime state.

    Per-instance user/configuration state belongs in cnr3_instance_config.*.
    Cache ownership, pins, slots, checkpoints, hot zones, prune, and recovery
    state belong in cnr3_cache_core.*.
    Diagnostics accumulation/printing belongs in the diagnostics modules.
*/

/*
    A CNR3 instance ID is a diagnostic/logging aid only.

    It helps distinguish simultaneous filter instances in stderr diagnostics.
    It does not identify global shared state and must not be used to share cache
    or runtime state between instances.
*/
struct Cnr3InstanceId {
    int value = 0;
};

[[nodiscard]] constexpr bool cnr3_instance_id_is_valid(
    Cnr3InstanceId instance_id
) noexcept {
    return instance_id.value > 0;
}

/*
    Shared frame-number constants/helpers.

    VapourSynth frame numbers are non-negative. CNR3 uses -1 only as a local
    sentinel for "no frame" / "not yet set"; it must never be passed as a real
    source or output frame request.
*/
inline constexpr int CNR3_INVALID_FRAME_NUMBER = -1;

[[nodiscard]] constexpr bool cnr3_frame_number_is_valid(
    int frame_number
) noexcept {
    return frame_number >= 0;
}

/*
    Small shared integer clamp helper.

    Keep this here only because clamp-like bounds checks are needed by multiple
    layers. Do not grow this header into a general algorithm or pixel-processing
    utility header.
*/
[[nodiscard]] constexpr int cnr3_clamp_int(
    int value,
    int low,
    int high
) noexcept {
    return (value < low) ? low : ((value > high) ? high : value);
}

/*
    Cross-module status codes.

    These are intentionally generic. They describe mechanical success/failure
    classes that may be shared by cache, diagnostics, configuration, and future
    integration code.

    Detailed human error messages remain at the call site that has enough
    context to explain the failure. VapourSynth-facing errors are still mapped
    later through mapSetError() during create/configuration and setFilterError()
    during frame processing.
*/
enum class Cnr3Status : std::uint8_t {
    ok = 0,

    invalid_argument,
    unsupported_format,
    allocation_failed,

    not_found,
    duplicate,
    capacity_exceeded,

    invariant_violation,
    lifecycle_violation,
    ownership_violation,

    vapoursynth_error,
    not_implemented
};

[[nodiscard]] constexpr const char* cnr3_status_name(
    Cnr3Status status
) noexcept {
    switch (status) {
    case Cnr3Status::ok:
        return "ok";
    case Cnr3Status::invalid_argument:
        return "invalid_argument";
    case Cnr3Status::unsupported_format:
        return "unsupported_format";
    case Cnr3Status::allocation_failed:
        return "allocation_failed";
    case Cnr3Status::not_found:
        return "not_found";
    case Cnr3Status::duplicate:
        return "duplicate";
    case Cnr3Status::capacity_exceeded:
        return "capacity_exceeded";
    case Cnr3Status::invariant_violation:
        return "invariant_violation";
    case Cnr3Status::lifecycle_violation:
        return "lifecycle_violation";
    case Cnr3Status::ownership_violation:
        return "ownership_violation";
    case Cnr3Status::vapoursynth_error:
        return "vapoursynth_error";
    case Cnr3Status::not_implemented:
        return "not_implemented";
    }

    return "unknown";
}

[[nodiscard]] constexpr bool cnr3_status_is_ok(
    Cnr3Status status
) noexcept {
    return status == Cnr3Status::ok;
}

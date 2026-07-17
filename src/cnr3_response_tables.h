#pragma once

/*
    CNR3 response table helpers.

    CMS07-P.1A salvaged only the pure vscnr2-style signed-difference
    response-table construction and safe lookup helpers.

    CMS07-P.2A adds an explicit, instance-agnostic configuration surface for
    building native-bit-depth Y/U/V response tables. These are only weight
    lookup tables. The Y table is the luma-difference response used by the
    later chroma blend decision; it is not a luma filter and must not be used
    to modify the output luma plane.

    Per CMS07.3 V8.1, native-subsampling traversal, downSampleLuma, and the
    int64 weighted blend are deliberate later pixel-layer phases. This module
    therefore does not read subSamplingW/H, does not downsample luma, and does
    not blend pixels. Those belong to P.3A/P.4A/P.5A.

    This module remains independent of cache state, frame scheduling, source
    lifecycle, and VapourSynth map/error handling.

    Deliberately not present here yet:
        - Cnr3Data-driven table assembly;
        - VSMap parsing or error reporting;
        - mode-string parsing;
        - native-subsampling frame traversal;
        - downSampleLuma;
        - int64 weighted blend or scene-change handling;
        - predecessor, recovery, cache, or getFrame logic.
*/

#include "cnr3_common.h"

#include <cstdint>
#include <vector>

enum class Cnr3ResponseCurveKind : std::uint8_t {
    narrow = 0,
    wide
};

struct Cnr3ResponsePlaneConfig {
    int threshold_8bit = 0;
    int strength_8bit = 0;
    Cnr3ResponseCurveKind curve = Cnr3ResponseCurveKind::narrow;
};

struct Cnr3ResponseTableConfig {
    int sample_peak = 0;

    Cnr3ResponsePlaneConfig y{};
    Cnr3ResponsePlaneConfig u{};
    Cnr3ResponsePlaneConfig v{};
};

struct Cnr3ResponseTables {
    int sample_peak = 0;
    int table_offset = 0;
    int table_size = 0;

    std::vector<int> y{};
    std::vector<int> u{};
    std::vector<int> v{};
};

[[nodiscard]] int get_cnr3_table_value_for_signed_diff(
    const std::vector<int>& table,
    int table_offset,
    int signed_diff
) noexcept;

[[nodiscard]] Cnr3Status build_cnr3_weight_table(
    std::vector<int>& table,
    int table_offset,
    int table_size,
    int sample_peak,
    int threshold,
    int strength,
    bool wide_response
) noexcept;

[[nodiscard]] int cnr3_scale_8bit_parameter_to_sample_peak(
    int value_8bit,
    int sample_peak
) noexcept;

[[nodiscard]] Cnr3Status cnr3_response_table_geometry_for_sample_peak(
    int sample_peak,
    int& table_offset,
    int& table_size
) noexcept;

[[nodiscard]] Cnr3Status build_cnr3_response_tables(
    const Cnr3ResponseTableConfig& config,
    Cnr3ResponseTables& tables
) noexcept;

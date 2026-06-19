#pragma once

/*
    CNR3 response table helpers.

    CMS07-P.1A salvages only the pure vscnr2-style signed-difference
    response-table construction and safe lookup helpers. This module remains
    independent of cache state, frame scheduling, source lifecycle, and
    VapourSynth map/error handling.

    Deliberately not present here yet:
        - Cnr3Data-driven table assembly;
        - VSMap parsing or error reporting;
        - blend, luma downsample, scene-change, or frame traversal;
        - predecessor, recovery, cache, or getFrame logic.
*/

#include "cnr3_common.h"

#include <vector>

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

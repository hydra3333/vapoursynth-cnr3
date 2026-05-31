#pragma once

#include "cnr3_build_config.h"
#include "cnr3_common.h"

// -----------------------------------------------------------------------------
// CNR3 response table helpers
//
// These helpers build and read the signed-difference response tables used by
// the vscnr2-style Y/U/V weighting model.
//
// They do not perform chroma blending themselves.
// -----------------------------------------------------------------------------

int get_cnr3_table_value_for_signed_diff(
    const std::vector<int> &table,
    int table_offset,
    int signed_diff
);

void build_cnr3_weight_table(
    std::vector<int> &table,
    int table_offset,
    int table_size,
    int sample_peak,
    int threshold,
    int strength,
    bool wide_response
);

bool build_cnr3_lookup_tables(
    Cnr3Data &d,
    VSMap *out,
    const VSAPI *vsapi
);

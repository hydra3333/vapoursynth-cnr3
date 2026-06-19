#include "cnr3_response_tables.h"

#include <cmath>
#include <cstdlib>
#include <new>

int get_cnr3_table_value_for_signed_diff(
    const std::vector<int>& table,
    int table_offset,
    int signed_diff
) noexcept {
    const int index = signed_diff + table_offset;

    if (index < 0 || index >= static_cast<int>(table.size())) {
        return 0;
    }

    return table[static_cast<std::size_t>(index)];
}

Cnr3Status build_cnr3_weight_table(
    std::vector<int>& table,
    int table_offset,
    int table_size,
    int sample_peak,
    int threshold,
    int strength,
    bool wide_response
) noexcept {
    if (table_size <= 0 || table_offset < 0 || table_offset >= table_size) {
        return Cnr3Status::invalid_argument;
    }

    if (sample_peak < 0) {
        return Cnr3Status::invalid_argument;
    }

    try {
        table.assign(static_cast<std::size_t>(table_size), 0);
    }
    catch (const std::bad_alloc&) {
        return Cnr3Status::allocation_failed;
    }

    threshold = cnr3_clamp_int(threshold, 0, sample_peak);
    strength = cnr3_clamp_int(strength, 0, sample_peak);

    if (threshold == 0) {
        table[static_cast<std::size_t>(table_offset)] = strength;
        return Cnr3Status::ok;
    }

    constexpr double pi = 3.141592653589793238462643383279502884;

    for (int signed_diff = -threshold; signed_diff <= threshold; ++signed_diff) {
        const int index = signed_diff + table_offset;

        if (index < 0 || index >= table_size) {
            continue;
        }

        const int abs_diff = std::abs(signed_diff);

        const double angle = wide_response ?
            (
                static_cast<double>(abs_diff) *
                static_cast<double>(abs_diff) *
                pi /
                (
                    static_cast<double>(threshold) *
                    static_cast<double>(threshold)
                )
            ) :
            (
                static_cast<double>(abs_diff) *
                pi /
                static_cast<double>(threshold)
            );

        /*
            Keep vscnr2-compatible integer division before applying the cosine
            curve. Odd strength 255 therefore peaks at 254, not 255.
        */
        const double half_strength = static_cast<double>(strength / 2);

        const int value = cnr3_clamp_int(
            static_cast<int>(half_strength * (1.0 + std::cos(angle))),
            0,
            sample_peak
        );

        table[static_cast<std::size_t>(index)] = value;
    }

    return Cnr3Status::ok;
}

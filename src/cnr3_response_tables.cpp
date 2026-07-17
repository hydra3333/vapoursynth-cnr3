#include "cnr3_response_tables.h"

#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <limits>
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

int cnr3_scale_8bit_parameter_to_sample_peak(
    int value_8bit,
    int sample_peak
) noexcept {
    if (sample_peak <= 0) {
        return 0;
    }

    const int clamped_value_8bit = cnr3_clamp_int(value_8bit, 0, 255);

    /*
        CMS07.3 V8.1 requires native-bit-depth pixel computation. The exact
        8-bit-parameter to native-peak scaling rule is codified here for the
        active pixel layer: round-to-nearest from the historical 8-bit domain.
        This preserves the prior integration helper's formula while making the
        P.2A response-table surface the forward authority for later pixel phases.
    */
    return static_cast<int>(
        (
            static_cast<std::int64_t>(clamped_value_8bit) *
            static_cast<std::int64_t>(sample_peak) +
            127
        ) / 255
    );
}

Cnr3Status cnr3_response_table_geometry_for_sample_peak(
    int sample_peak,
    int& table_offset,
    int& table_size
) noexcept {
    table_offset = 0;
    table_size = 0;

    constexpr int max_sample_peak_without_table_size_overflow =
        (std::numeric_limits<int>::max() - 1) / 2;

    if (
        sample_peak <= 0 ||
        sample_peak > max_sample_peak_without_table_size_overflow
        ) {
        return Cnr3Status::invalid_argument;
    }

    table_offset = sample_peak;
    table_size = (sample_peak * 2) + 1;

    return Cnr3Status::ok;
}

Cnr3Status build_cnr3_response_tables(
    const Cnr3ResponseTableConfig& config,
    Cnr3ResponseTables& tables
) noexcept {
    int table_offset = 0;
    int table_size = 0;

    const Cnr3Status geometry_status =
        cnr3_response_table_geometry_for_sample_peak(
            config.sample_peak,
            table_offset,
            table_size
        );

    if (geometry_status != Cnr3Status::ok) {
        return geometry_status;
    }

    const auto build_plane = [
        table_offset,
        table_size,
        sample_peak = config.sample_peak
    ](
        const Cnr3ResponsePlaneConfig& plane_config,
        std::vector<int>& table
    ) noexcept -> Cnr3Status {
        const int threshold = cnr3_scale_8bit_parameter_to_sample_peak(
            plane_config.threshold_8bit,
            sample_peak
        );
        const int strength = cnr3_scale_8bit_parameter_to_sample_peak(
            plane_config.strength_8bit,
            sample_peak
        );
        const bool wide_response =
            plane_config.curve == Cnr3ResponseCurveKind::wide;

        return build_cnr3_weight_table(
            table,
            table_offset,
            table_size,
            sample_peak,
            threshold,
            strength,
            wide_response
        );
    };

    Cnr3ResponseTables next{};
    next.sample_peak = config.sample_peak;
    next.table_offset = table_offset;
    next.table_size = table_size;

    Cnr3Status status = build_plane(config.y, next.y);

    if (status != Cnr3Status::ok) {
        return status;
    }

    status = build_plane(config.u, next.u);

    if (status != Cnr3Status::ok) {
        return status;
    }

    status = build_plane(config.v, next.v);

    if (status != Cnr3Status::ok) {
        return status;
    }

    tables.sample_peak = next.sample_peak;
    tables.table_offset = next.table_offset;
    tables.table_size = next.table_size;
    tables.y.swap(next.y);
    tables.u.swap(next.u);
    tables.v.swap(next.v);

    return Cnr3Status::ok;
}

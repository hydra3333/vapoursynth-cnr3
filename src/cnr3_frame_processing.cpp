#include "cnr3_frame_processing.h"

namespace {

[[nodiscard]] bool cnr3_value_is_inclusive_range(
    int value,
    int low,
    int high
) noexcept {
    return value >= low && value <= high;
}

} // namespace

Cnr3Status cnr3_blend_scale_for_bit_depth(
    int bits_per_sample,
    std::int64_t& blend_scale
) noexcept {
    blend_scale = 0;

    if (bits_per_sample < 8 || bits_per_sample > 16) {
        return Cnr3Status::invalid_argument;
    }

    const int shift2 = bits_per_sample << 1;
    blend_scale = std::int64_t{1} << shift2;

    return Cnr3Status::ok;
}

std::int64_t cnr3_calculate_combined_blend_weight(
    int y_response,
    int chroma_response
) noexcept {
    return static_cast<std::int64_t>(y_response) *
        static_cast<std::int64_t>(chroma_response);
}

Cnr3Status cnr3_blend_chroma_sample(
    int current_source_sample,
    int previous_filtered_sample,
    int y_response,
    int chroma_response,
    int bits_per_sample,
    int& output_sample
) noexcept {
    std::int64_t shift = 0;

    const Cnr3Status scale_status = cnr3_blend_scale_for_bit_depth(
        bits_per_sample,
        shift
    );

    if (scale_status != Cnr3Status::ok) {
        return scale_status;
    }

    const int sample_peak = (1 << bits_per_sample) - 1;

    if (
        !cnr3_value_is_inclusive_range(current_source_sample, 0, sample_peak) ||
        !cnr3_value_is_inclusive_range(previous_filtered_sample, 0, sample_peak) ||
        !cnr3_value_is_inclusive_range(y_response, 0, sample_peak) ||
        !cnr3_value_is_inclusive_range(chroma_response, 0, sample_peak)
        ) {
        return Cnr3Status::invalid_argument;
    }

    const int shift2 = bits_per_sample << 1;
    const std::int64_t shift1 = shift >> 1;
    const std::int64_t weight = cnr3_calculate_combined_blend_weight(
        y_response,
        chroma_response
    );

    /*
        Response bounds make the fixed-point blend a convex combination:

            weight = y_response * chroma_response <= sample_peak^2 < shift

        Therefore shift - weight is non-negative. Do not widen response inputs
        beyond 0..sample_peak without revisiting this invariant.
    */
    const std::int64_t blended_sample = (
        weight * static_cast<std::int64_t>(previous_filtered_sample) +
        (shift - weight) * static_cast<std::int64_t>(current_source_sample) +
        shift1
    ) >> shift2;

    output_sample = static_cast<int>(blended_sample);

    return Cnr3Status::ok;
}

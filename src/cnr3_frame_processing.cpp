#include "cnr3_frame_processing.h"

#include <limits>

namespace {

[[nodiscard]] bool cnr3_value_is_inclusive_range(
    int value,
    int low,
    int high
) noexcept {
    return value >= low && value <= high;
}

[[nodiscard]] bool cnr3_shifted_coordinate_is_valid(
    int coordinate,
    int shift
) noexcept {
    return coordinate >= 0 &&
        shift >= 0 &&
        shift <= 1 &&
        coordinate <= (std::numeric_limits<int>::max() >> shift);
}

[[nodiscard]] Cnr3Status cnr3_sample_peak_for_bit_depth(
    int bits_per_sample,
    int& sample_peak
) noexcept {
    sample_peak = 0;

    if (bits_per_sample < 8 || bits_per_sample > 16) {
        return Cnr3Status::invalid_argument;
    }

    sample_peak = (1 << bits_per_sample) - 1;
    return Cnr3Status::ok;
}

} // namespace

Cnr3Status cnr3_downsample_luma_tap_coordinates(
    int chroma_x,
    int chroma_y,
    int luma_width,
    int luma_height,
    int sub_sampling_w,
    int sub_sampling_h,
    Cnr3DownsampledLumaTapCoordinates& coordinates
) noexcept {
    if (
        luma_width <= 0 ||
        luma_height <= 0 ||
        sub_sampling_w < 0 ||
        sub_sampling_w > 1 ||
        sub_sampling_h < 0 ||
        sub_sampling_h > 1 ||
        !cnr3_shifted_coordinate_is_valid(chroma_x, sub_sampling_w) ||
        !cnr3_shifted_coordinate_is_valid(chroma_y, sub_sampling_h)
        ) {
        return Cnr3Status::invalid_argument;
    }

    const int x0 = chroma_x << sub_sampling_w;
    const int y0 = chroma_y << sub_sampling_h;

    if (x0 >= luma_width || y0 >= luma_height) {
        return Cnr3Status::invalid_argument;
    }

    Cnr3DownsampledLumaTapCoordinates resolved{};
    resolved.x0 = x0;
    resolved.y0 = y0;

    /*
        vsCnr2 always reads temp + 1 horizontally, even when subSamplingW is 0.
        CNR3 keeps that degenerate 4:4:4/4:4:0 shape but clamps the edge tap so
        the last column never relies on frame-padding slack.
    */
    resolved.x1 = (x0 < luma_width - 1) ? (x0 + 1) : x0;

    /*
        The second row is y0 + subSamplingH. For 4:2:2 and 4:4:4 it is the same
        row, matching vsCnr2's counted-twice degenerate average.
    */
    resolved.y1 = (y0 + sub_sampling_h < luma_height) ?
        (y0 + sub_sampling_h) : y0;

    coordinates = resolved;
    return Cnr3Status::ok;
}

Cnr3Status cnr3_downsample_luma_sample(
    int top_left_sample,
    int top_right_sample,
    int bottom_left_sample,
    int bottom_right_sample,
    int bits_per_sample,
    int& output_sample
) noexcept {
    int sample_peak = 0;

    const Cnr3Status peak_status = cnr3_sample_peak_for_bit_depth(
        bits_per_sample,
        sample_peak
    );

    if (peak_status != Cnr3Status::ok) {
        return peak_status;
    }

    if (
        !cnr3_value_is_inclusive_range(top_left_sample, 0, sample_peak) ||
        !cnr3_value_is_inclusive_range(top_right_sample, 0, sample_peak) ||
        !cnr3_value_is_inclusive_range(bottom_left_sample, 0, sample_peak) ||
        !cnr3_value_is_inclusive_range(bottom_right_sample, 0, sample_peak)
        ) {
        return Cnr3Status::invalid_argument;
    }

    output_sample = (
        top_left_sample +
        top_right_sample +
        bottom_left_sample +
        bottom_right_sample +
        2
    ) >> 2;

    return Cnr3Status::ok;
}

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

    int sample_peak = 0;

    if (
        cnr3_sample_peak_for_bit_depth(bits_per_sample, sample_peak) != Cnr3Status::ok ||
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

#include "cnr3_frame_processing.h"

#include "cnr3_response_tables.h"

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

[[nodiscard]] bool cnr3_plane_shape_is_valid(
    int width,
    int height,
    int stride
) noexcept {
    return width > 0 &&
        height > 0 &&
        stride >= width &&
        height <= (std::numeric_limits<int>::max() / stride);
}

[[nodiscard]] bool cnr3_const_plane_view_is_valid(
    const Cnr3ConstPlaneBufferView& view
) noexcept {
    return view.samples != nullptr &&
        cnr3_plane_shape_is_valid(view.width, view.height, view.stride);
}

[[nodiscard]] bool cnr3_mutable_plane_view_is_valid(
    const Cnr3MutablePlaneBufferView& view
) noexcept {
    return view.samples != nullptr &&
        cnr3_plane_shape_is_valid(view.width, view.height, view.stride);
}

[[nodiscard]] bool cnr3_const_plane_dimensions_match(
    const Cnr3ConstPlaneBufferView& view,
    int width,
    int height
) noexcept {
    return view.width == width && view.height == height;
}

[[nodiscard]] bool cnr3_mutable_plane_dimensions_match(
    const Cnr3MutablePlaneBufferView& view,
    int width,
    int height
) noexcept {
    return view.width == width && view.height == height;
}

[[nodiscard]] Cnr3Status cnr3_expected_chroma_dimension_for_luma_dimension(
    int luma_dimension,
    int sub_sampling,
    int& chroma_dimension
) noexcept {
    chroma_dimension = 0;

    if (
        luma_dimension <= 0 ||
        sub_sampling < 0 ||
        sub_sampling > 1
        ) {
        return Cnr3Status::invalid_argument;
    }

    const int divisor = 1 << sub_sampling;
    const int round_up = divisor - 1;

    if (luma_dimension > std::numeric_limits<int>::max() - round_up) {
        return Cnr3Status::invalid_argument;
    }

    chroma_dimension = (luma_dimension + round_up) >> sub_sampling;
    return Cnr3Status::ok;
}

[[nodiscard]] int cnr3_plane_sample_at(
    const Cnr3ConstPlaneBufferView& view,
    int x,
    int y
) noexcept {
    return view.samples[(y * view.stride) + x];
}

void cnr3_write_plane_sample(
    Cnr3MutablePlaneBufferView& view,
    int x,
    int y,
    int value
) noexcept {
    view.samples[(y * view.stride) + x] = value;
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

Cnr3Status cnr3_blend_chroma_sample_from_response_tables(
    int current_downsampled_luma_sample,
    int previous_downsampled_luma_sample,
    int current_source_chroma_sample,
    int previous_filtered_chroma_sample,
    const std::vector<int>& y_response_table,
    const std::vector<int>& chroma_response_table,
    int table_offset,
    int bits_per_sample,
    Cnr3ChromaBlendSampleResult& result
) noexcept {
    int sample_peak = 0;

    const Cnr3Status peak_status = cnr3_sample_peak_for_bit_depth(
        bits_per_sample,
        sample_peak
    );

    if (peak_status != Cnr3Status::ok) {
        return peak_status;
    }

    int expected_table_offset = 0;
    int expected_table_size = 0;

    const Cnr3Status geometry_status =
        cnr3_response_table_geometry_for_sample_peak(
            sample_peak,
            expected_table_offset,
            expected_table_size
        );

    if (geometry_status != Cnr3Status::ok) {
        return geometry_status;
    }

    if (
        table_offset != expected_table_offset ||
        y_response_table.size() != static_cast<std::size_t>(expected_table_size) ||
        chroma_response_table.size() != static_cast<std::size_t>(expected_table_size) ||
        !cnr3_value_is_inclusive_range(current_downsampled_luma_sample, 0, sample_peak) ||
        !cnr3_value_is_inclusive_range(previous_downsampled_luma_sample, 0, sample_peak) ||
        !cnr3_value_is_inclusive_range(current_source_chroma_sample, 0, sample_peak) ||
        !cnr3_value_is_inclusive_range(previous_filtered_chroma_sample, 0, sample_peak)
        ) {
        return Cnr3Status::invalid_argument;
    }

    const int luma_signed_diff =
        current_downsampled_luma_sample - previous_downsampled_luma_sample;
    const int chroma_signed_diff =
        current_source_chroma_sample - previous_filtered_chroma_sample;

    const int y_response = get_cnr3_table_value_for_signed_diff(
        y_response_table,
        table_offset,
        luma_signed_diff
    );

    const int chroma_response = get_cnr3_table_value_for_signed_diff(
        chroma_response_table,
        table_offset,
        chroma_signed_diff
    );

    int blended_sample = 0;

    const Cnr3Status blend_status = cnr3_blend_chroma_sample(
        current_source_chroma_sample,
        previous_filtered_chroma_sample,
        y_response,
        chroma_response,
        bits_per_sample,
        blended_sample
    );

    if (blend_status != Cnr3Status::ok) {
        return blend_status;
    }

    Cnr3ChromaBlendSampleResult resolved{};
    resolved.luma_signed_diff = luma_signed_diff;
    resolved.chroma_signed_diff = chroma_signed_diff;
    resolved.y_response = y_response;
    resolved.chroma_response = chroma_response;
    resolved.output_sample = blended_sample;

    result = resolved;
    return Cnr3Status::ok;
}


Cnr3Status cnr3_downsample_luma_plane_to_chroma_grid(
    const Cnr3ConstPlaneBufferView& source_luma_plane,
    int sub_sampling_w,
    int sub_sampling_h,
    int bits_per_sample,
    Cnr3MutablePlaneBufferView& output_downsampled_luma_plane,
    Cnr3DownsampledLumaPlaneProcessSummary& summary
) noexcept {
    if (
        !cnr3_const_plane_view_is_valid(source_luma_plane) ||
        !cnr3_mutable_plane_view_is_valid(output_downsampled_luma_plane)
        ) {
        return Cnr3Status::invalid_argument;
    }

    int expected_output_width = 0;
    int expected_output_height = 0;

    const Cnr3Status width_status =
        cnr3_expected_chroma_dimension_for_luma_dimension(
            source_luma_plane.width,
            sub_sampling_w,
            expected_output_width
        );

    if (width_status != Cnr3Status::ok) {
        return width_status;
    }

    const Cnr3Status height_status =
        cnr3_expected_chroma_dimension_for_luma_dimension(
            source_luma_plane.height,
            sub_sampling_h,
            expected_output_height
        );

    if (height_status != Cnr3Status::ok) {
        return height_status;
    }

    if (
        !cnr3_mutable_plane_dimensions_match(
            output_downsampled_luma_plane,
            expected_output_width,
            expected_output_height
        )
        ) {
        return Cnr3Status::invalid_argument;
    }

    const int sample_count = expected_output_width * expected_output_height;
    std::vector<int> resolved_outputs;

    try {
        resolved_outputs.resize(static_cast<std::size_t>(sample_count));
    } catch (...) {
        return Cnr3Status::allocation_failed;
    }

    for (int y = 0; y < expected_output_height; ++y) {
        for (int x = 0; x < expected_output_width; ++x) {
            Cnr3DownsampledLumaTapCoordinates taps{};

            const Cnr3Status coordinate_status =
                cnr3_downsample_luma_tap_coordinates(
                    x,
                    y,
                    source_luma_plane.width,
                    source_luma_plane.height,
                    sub_sampling_w,
                    sub_sampling_h,
                    taps
                );

            if (coordinate_status != Cnr3Status::ok) {
                return coordinate_status;
            }

            int output_sample = 0;

            const Cnr3Status sample_status = cnr3_downsample_luma_sample(
                cnr3_plane_sample_at(source_luma_plane, taps.x0, taps.y0),
                cnr3_plane_sample_at(source_luma_plane, taps.x1, taps.y0),
                cnr3_plane_sample_at(source_luma_plane, taps.x0, taps.y1),
                cnr3_plane_sample_at(source_luma_plane, taps.x1, taps.y1),
                bits_per_sample,
                output_sample
            );

            if (sample_status != Cnr3Status::ok) {
                return sample_status;
            }

            resolved_outputs[static_cast<std::size_t>((y * expected_output_width) + x)] =
                output_sample;
        }
    }

    for (int y = 0; y < expected_output_height; ++y) {
        for (int x = 0; x < expected_output_width; ++x) {
            cnr3_write_plane_sample(
                output_downsampled_luma_plane,
                x,
                y,
                resolved_outputs[static_cast<std::size_t>((y * expected_output_width) + x)]
            );
        }
    }

    Cnr3DownsampledLumaPlaneProcessSummary resolved_summary{};
    resolved_summary.source_width = source_luma_plane.width;
    resolved_summary.source_height = source_luma_plane.height;
    resolved_summary.output_width = expected_output_width;
    resolved_summary.output_height = expected_output_height;
    resolved_summary.samples_processed = sample_count;
    resolved_summary.first_output_sample = resolved_outputs.front();
    resolved_summary.last_output_sample = resolved_outputs.back();

    summary = resolved_summary;
    return Cnr3Status::ok;
}


Cnr3Status cnr3_process_chroma_plane_from_downsampled_luma(
    const Cnr3ConstPlaneBufferView& current_downsampled_luma_plane,
    const Cnr3ConstPlaneBufferView& previous_downsampled_luma_plane,
    const Cnr3ConstPlaneBufferView& current_source_chroma_plane,
    const Cnr3ConstPlaneBufferView& previous_filtered_chroma_plane,
    const std::vector<int>& y_response_table,
    const std::vector<int>& chroma_response_table,
    int table_offset,
    int bits_per_sample,
    Cnr3MutablePlaneBufferView& output_chroma_plane,
    Cnr3ChromaPlaneProcessSummary& summary
) noexcept {
    if (
        !cnr3_const_plane_view_is_valid(current_downsampled_luma_plane) ||
        !cnr3_const_plane_view_is_valid(previous_downsampled_luma_plane) ||
        !cnr3_const_plane_view_is_valid(current_source_chroma_plane) ||
        !cnr3_const_plane_view_is_valid(previous_filtered_chroma_plane) ||
        !cnr3_mutable_plane_view_is_valid(output_chroma_plane)
        ) {
        return Cnr3Status::invalid_argument;
    }

    const int width = current_source_chroma_plane.width;
    const int height = current_source_chroma_plane.height;

    if (
        !cnr3_const_plane_dimensions_match(current_downsampled_luma_plane, width, height) ||
        !cnr3_const_plane_dimensions_match(previous_downsampled_luma_plane, width, height) ||
        !cnr3_const_plane_dimensions_match(previous_filtered_chroma_plane, width, height) ||
        !cnr3_mutable_plane_dimensions_match(output_chroma_plane, width, height)
        ) {
        return Cnr3Status::invalid_argument;
    }

    const int sample_count = width * height;
    std::vector<int> resolved_outputs;

    try {
        resolved_outputs.resize(static_cast<std::size_t>(sample_count));
    } catch (...) {
        return Cnr3Status::allocation_failed;
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Cnr3ChromaBlendSampleResult sample_result{};

            const Cnr3Status sample_status =
                cnr3_blend_chroma_sample_from_response_tables(
                    cnr3_plane_sample_at(current_downsampled_luma_plane, x, y),
                    cnr3_plane_sample_at(previous_downsampled_luma_plane, x, y),
                    cnr3_plane_sample_at(current_source_chroma_plane, x, y),
                    cnr3_plane_sample_at(previous_filtered_chroma_plane, x, y),
                    y_response_table,
                    chroma_response_table,
                    table_offset,
                    bits_per_sample,
                    sample_result
                );

            if (sample_status != Cnr3Status::ok) {
                return sample_status;
            }

            resolved_outputs[static_cast<std::size_t>((y * width) + x)] =
                sample_result.output_sample;
        }
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            cnr3_write_plane_sample(
                output_chroma_plane,
                x,
                y,
                resolved_outputs[static_cast<std::size_t>((y * width) + x)]
            );
        }
    }

    Cnr3ChromaPlaneProcessSummary resolved_summary{};
    resolved_summary.width = width;
    resolved_summary.height = height;
    resolved_summary.samples_processed = sample_count;
    resolved_summary.first_output_sample = resolved_outputs.front();
    resolved_summary.last_output_sample = resolved_outputs.back();

    summary = resolved_summary;
    return Cnr3Status::ok;
}

#include "cnr3_frame_processing.h"

#include "cnr3_response_tables.h"

#include "VapourSynth4.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
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


[[nodiscard]] bool cnr3_native_plane_byte_shape_is_valid(
    int width,
    int height,
    int stride_bytes,
    int storage_bytes
) noexcept {
    if (
        width <= 0 ||
        height <= 0 ||
        storage_bytes <= 0 ||
        width > (std::numeric_limits<int>::max() / storage_bytes)
        ) {
        return false;
    }

    const int active_row_bytes = width * storage_bytes;

    return stride_bytes >= active_row_bytes &&
        height <= (std::numeric_limits<int>::max() / stride_bytes);
}

[[nodiscard]] bool cnr3_const_native_plane_byte_view_is_valid(
    const Cnr3ConstNativePlaneByteView& view,
    int storage_bytes
) noexcept {
    return view.data != nullptr &&
        cnr3_native_plane_byte_shape_is_valid(
            view.width,
            view.height,
            view.stride_bytes,
            storage_bytes
        );
}

[[nodiscard]] bool cnr3_mutable_native_plane_byte_view_is_valid(
    const Cnr3MutableNativePlaneByteView& view,
    int storage_bytes
) noexcept {
    return view.data != nullptr &&
        cnr3_native_plane_byte_shape_is_valid(
            view.width,
            view.height,
            view.stride_bytes,
            storage_bytes
        );
}

[[nodiscard]] bool cnr3_native_plane_dimensions_match(
    const Cnr3ConstNativePlaneByteView& native_plane,
    const Cnr3MutablePlaneBufferView& scalar_plane
) noexcept {
    return native_plane.width == scalar_plane.width &&
        native_plane.height == scalar_plane.height;
}

[[nodiscard]] bool cnr3_native_plane_dimensions_match(
    const Cnr3ConstPlaneBufferView& scalar_plane,
    const Cnr3MutableNativePlaneByteView& native_plane
) noexcept {
    return scalar_plane.width == native_plane.width &&
        scalar_plane.height == native_plane.height;
}

[[nodiscard]] std::size_t cnr3_native_plane_byte_offset(
    int x,
    int y,
    int stride_bytes,
    int storage_bytes
) noexcept {
    return
        (static_cast<std::size_t>(y) * static_cast<std::size_t>(stride_bytes)) +
        (static_cast<std::size_t>(x) * static_cast<std::size_t>(storage_bytes));
}

[[nodiscard]] bool cnr3_vapoursynth_plane_number_is_supported(
    int plane
) noexcept {
    return plane >= 0 && plane < 3;
}

[[nodiscard]] bool cnr3_vapoursynth_stride_is_valid_for_storage(
    ptrdiff_t stride,
    int storage_bytes,
    int& stride_bytes
) noexcept {
    stride_bytes = 0;

    if (
        storage_bytes <= 0 ||
        stride <= 0 ||
        stride > static_cast<ptrdiff_t>(std::numeric_limits<int>::max()) ||
        (stride % static_cast<ptrdiff_t>(storage_bytes)) != 0
        ) {
        return false;
    }

    stride_bytes = static_cast<int>(stride);
    return true;
}

[[nodiscard]] bool cnr3_vapoursynth_plane_api_is_valid_for_read(
    const VSAPI* vsapi
) noexcept {
    return vsapi != nullptr &&
        vsapi->getFrameWidth != nullptr &&
        vsapi->getFrameHeight != nullptr &&
        vsapi->getStride != nullptr &&
        vsapi->getReadPtr != nullptr;
}

[[nodiscard]] bool cnr3_vapoursynth_plane_api_is_valid_for_write(
    const VSAPI* vsapi
) noexcept {
    return vsapi != nullptr &&
        vsapi->getFrameWidth != nullptr &&
        vsapi->getFrameHeight != nullptr &&
        vsapi->getStride != nullptr &&
        vsapi->getWritePtr != nullptr;
}

void cnr3_publish_vapoursynth_plane_summary(
    int plane,
    int width,
    int height,
    int stride_bytes,
    int bits_per_sample,
    int storage_bytes,
    bool read_view_created,
    bool write_view_created,
    Cnr3VapourSynthPlaneByteViewSummary& summary
) noexcept {
    Cnr3VapourSynthPlaneByteViewSummary local{};
    local.plane = plane;
    local.width = width;
    local.height = height;
    local.stride_bytes = stride_bytes;
    local.bits_per_sample = bits_per_sample;
    local.storage_bytes = storage_bytes;
    local.read_view_created = read_view_created;
    local.write_view_created = write_view_created;
    summary = local;
}

[[nodiscard]] bool cnr3_subsampling_factor_is_valid(
    int sub_sampling
) noexcept {
    return sub_sampling >= 0 && sub_sampling <= 1;
}

[[nodiscard]] bool cnr3_luma_chroma_dimensions_match_subsampling(
    int luma_dimension,
    int chroma_dimension,
    int sub_sampling
) noexcept {
    if (
        luma_dimension <= 0 ||
        chroma_dimension <= 0 ||
        !cnr3_subsampling_factor_is_valid(sub_sampling)
        ) {
        return false;
    }

    const int divisor = 1 << sub_sampling;
    return (luma_dimension % divisor) == 0 &&
        chroma_dimension == (luma_dimension / divisor);
}

[[nodiscard]] bool cnr3_const_native_plane_dimensions_match(
    const Cnr3ConstNativePlaneByteView& left,
    const Cnr3ConstNativePlaneByteView& right
) noexcept {
    return left.width == right.width &&
        left.height == right.height &&
        left.bits_per_sample == right.bits_per_sample;
}

[[nodiscard]] bool cnr3_const_mutable_native_plane_dimensions_match(
    const Cnr3ConstNativePlaneByteView& left,
    const Cnr3MutableNativePlaneByteView& right
) noexcept {
    return left.width == right.width &&
        left.height == right.height &&
        left.bits_per_sample == right.bits_per_sample;
}

[[nodiscard]] bool cnr3_triplet_plane_dimensions_are_compatible(
    const Cnr3VapourSynthFrameTripletNativeViews& views,
    int sub_sampling_w,
    int sub_sampling_h
) noexcept {
    if (
        !cnr3_const_native_plane_dimensions_match(
            views.current_source_y,
            views.previous_filtered_y
        ) ||
        !cnr3_const_mutable_native_plane_dimensions_match(
            views.current_source_y,
            views.destination_y
        ) ||
        !cnr3_const_native_plane_dimensions_match(
            views.current_source_u,
            views.previous_filtered_u
        ) ||
        !cnr3_const_mutable_native_plane_dimensions_match(
            views.current_source_u,
            views.destination_u
        ) ||
        !cnr3_const_native_plane_dimensions_match(
            views.current_source_v,
            views.previous_filtered_v
        ) ||
        !cnr3_const_mutable_native_plane_dimensions_match(
            views.current_source_v,
            views.destination_v
        ) ||
        !cnr3_const_native_plane_dimensions_match(
            views.current_source_u,
            views.current_source_v
        )
        ) {
        return false;
    }

    return cnr3_luma_chroma_dimensions_match_subsampling(
        views.current_source_y.width,
        views.current_source_u.width,
        sub_sampling_w
    ) &&
        cnr3_luma_chroma_dimensions_match_subsampling(
            views.current_source_y.height,
            views.current_source_u.height,
            sub_sampling_h
        );
}

void cnr3_publish_vapoursynth_frame_triplet_summary(
    int bits_per_sample,
    int storage_bytes,
    int sub_sampling_w,
    int sub_sampling_h,
    int luma_width,
    int luma_height,
    int chroma_width,
    int chroma_height,
    Cnr3VapourSynthFrameTripletViewSummary& summary
) noexcept {
    Cnr3VapourSynthFrameTripletViewSummary local{};
    local.bits_per_sample = bits_per_sample;
    local.storage_bytes = storage_bytes;
    local.sub_sampling_w = sub_sampling_w;
    local.sub_sampling_h = sub_sampling_h;
    local.luma_width = luma_width;
    local.luma_height = luma_height;
    local.chroma_width = chroma_width;
    local.chroma_height = chroma_height;
    local.current_source_views_created = true;
    local.previous_filtered_views_created = true;
    local.destination_views_created = true;
    local.triplet_views_created = true;
    summary = local;
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

Cnr3Status cnr3_make_vapoursynth_read_plane_byte_view(
    const VSFrame* frame,
    const VSAPI* vsapi,
    int plane,
    int bits_per_sample,
    Cnr3ConstNativePlaneByteView& native_plane,
    Cnr3VapourSynthPlaneByteViewSummary& summary
) noexcept {
    native_plane = Cnr3ConstNativePlaneByteView{};
    summary = Cnr3VapourSynthPlaneByteViewSummary{};

    int storage_bytes = 0;

    if (
        frame == nullptr ||
        !cnr3_vapoursynth_plane_api_is_valid_for_read(vsapi) ||
        !cnr3_vapoursynth_plane_number_is_supported(plane) ||
        cnr3_native_storage_bytes_for_bit_depth(bits_per_sample, storage_bytes) !=
            Cnr3Status::ok
        ) {
        return Cnr3Status::invalid_argument;
    }

    const int width = vsapi->getFrameWidth(frame, plane);
    const int height = vsapi->getFrameHeight(frame, plane);

    int stride_bytes = 0;

    if (
        !cnr3_vapoursynth_stride_is_valid_for_storage(
            vsapi->getStride(frame, plane),
            storage_bytes,
            stride_bytes
        ) ||
        !cnr3_native_plane_byte_shape_is_valid(
            width,
            height,
            stride_bytes,
            storage_bytes
        )
        ) {
        return Cnr3Status::invalid_argument;
    }

    const std::uint8_t* read_ptr = vsapi->getReadPtr(frame, plane);

    if (read_ptr == nullptr) {
        return Cnr3Status::invalid_argument;
    }

    Cnr3ConstNativePlaneByteView local{};
    local.data = read_ptr;
    local.width = width;
    local.height = height;
    local.stride_bytes = stride_bytes;
    local.bits_per_sample = bits_per_sample;

    native_plane = local;

    cnr3_publish_vapoursynth_plane_summary(
        plane,
        width,
        height,
        stride_bytes,
        bits_per_sample,
        storage_bytes,
        true,
        false,
        summary
    );

    return Cnr3Status::ok;
}

Cnr3Status cnr3_make_vapoursynth_write_plane_byte_view(
    VSFrame* frame,
    const VSAPI* vsapi,
    int plane,
    int bits_per_sample,
    Cnr3MutableNativePlaneByteView& native_plane,
    Cnr3VapourSynthPlaneByteViewSummary& summary
) noexcept {
    native_plane = Cnr3MutableNativePlaneByteView{};
    summary = Cnr3VapourSynthPlaneByteViewSummary{};

    int storage_bytes = 0;

    if (
        frame == nullptr ||
        !cnr3_vapoursynth_plane_api_is_valid_for_write(vsapi) ||
        !cnr3_vapoursynth_plane_number_is_supported(plane) ||
        cnr3_native_storage_bytes_for_bit_depth(bits_per_sample, storage_bytes) !=
            Cnr3Status::ok
        ) {
        return Cnr3Status::invalid_argument;
    }

    const int width = vsapi->getFrameWidth(frame, plane);
    const int height = vsapi->getFrameHeight(frame, plane);

    int stride_bytes = 0;

    if (
        !cnr3_vapoursynth_stride_is_valid_for_storage(
            vsapi->getStride(frame, plane),
            storage_bytes,
            stride_bytes
        ) ||
        !cnr3_native_plane_byte_shape_is_valid(
            width,
            height,
            stride_bytes,
            storage_bytes
        )
        ) {
        return Cnr3Status::invalid_argument;
    }

    std::uint8_t* write_ptr = vsapi->getWritePtr(frame, plane);

    if (write_ptr == nullptr) {
        return Cnr3Status::invalid_argument;
    }

    Cnr3MutableNativePlaneByteView local{};
    local.data = write_ptr;
    local.width = width;
    local.height = height;
    local.stride_bytes = stride_bytes;
    local.bits_per_sample = bits_per_sample;

    native_plane = local;

    cnr3_publish_vapoursynth_plane_summary(
        plane,
        width,
        height,
        stride_bytes,
        bits_per_sample,
        storage_bytes,
        false,
        true,
        summary
    );

    return Cnr3Status::ok;
}

Cnr3Status cnr3_make_caller_supplied_vapoursynth_frame_triplet_views(
    const VSFrame* current_source_frame,
    const VSFrame* previous_filtered_output_frame,
    VSFrame* destination_frame,
    const VSAPI* vsapi,
    int bits_per_sample,
    int sub_sampling_w,
    int sub_sampling_h,
    Cnr3VapourSynthFrameTripletNativeViews& views,
    Cnr3VapourSynthFrameTripletViewSummary& summary
) noexcept {
    views = Cnr3VapourSynthFrameTripletNativeViews{};
    summary = Cnr3VapourSynthFrameTripletViewSummary{};

    int storage_bytes = 0;

    if (
        current_source_frame == nullptr ||
        previous_filtered_output_frame == nullptr ||
        destination_frame == nullptr ||
        vsapi == nullptr ||
        !cnr3_subsampling_factor_is_valid(sub_sampling_w) ||
        !cnr3_subsampling_factor_is_valid(sub_sampling_h) ||
        cnr3_native_storage_bytes_for_bit_depth(bits_per_sample, storage_bytes) !=
            Cnr3Status::ok
        ) {
        return Cnr3Status::invalid_argument;
    }

    Cnr3VapourSynthFrameTripletNativeViews local{};
    Cnr3VapourSynthPlaneByteViewSummary plane_summary{};

    Cnr3Status status = cnr3_make_vapoursynth_read_plane_byte_view(
        current_source_frame,
        vsapi,
        0,
        bits_per_sample,
        local.current_source_y,
        plane_summary
    );

    if (status != Cnr3Status::ok) {
        return status;
    }

    status = cnr3_make_vapoursynth_read_plane_byte_view(
        current_source_frame,
        vsapi,
        1,
        bits_per_sample,
        local.current_source_u,
        plane_summary
    );

    if (status != Cnr3Status::ok) {
        return status;
    }

    status = cnr3_make_vapoursynth_read_plane_byte_view(
        current_source_frame,
        vsapi,
        2,
        bits_per_sample,
        local.current_source_v,
        plane_summary
    );

    if (status != Cnr3Status::ok) {
        return status;
    }

    status = cnr3_make_vapoursynth_read_plane_byte_view(
        previous_filtered_output_frame,
        vsapi,
        0,
        bits_per_sample,
        local.previous_filtered_y,
        plane_summary
    );

    if (status != Cnr3Status::ok) {
        return status;
    }

    status = cnr3_make_vapoursynth_read_plane_byte_view(
        previous_filtered_output_frame,
        vsapi,
        1,
        bits_per_sample,
        local.previous_filtered_u,
        plane_summary
    );

    if (status != Cnr3Status::ok) {
        return status;
    }

    status = cnr3_make_vapoursynth_read_plane_byte_view(
        previous_filtered_output_frame,
        vsapi,
        2,
        bits_per_sample,
        local.previous_filtered_v,
        plane_summary
    );

    if (status != Cnr3Status::ok) {
        return status;
    }

    status = cnr3_make_vapoursynth_write_plane_byte_view(
        destination_frame,
        vsapi,
        0,
        bits_per_sample,
        local.destination_y,
        plane_summary
    );

    if (status != Cnr3Status::ok) {
        return status;
    }

    status = cnr3_make_vapoursynth_write_plane_byte_view(
        destination_frame,
        vsapi,
        1,
        bits_per_sample,
        local.destination_u,
        plane_summary
    );

    if (status != Cnr3Status::ok) {
        return status;
    }

    status = cnr3_make_vapoursynth_write_plane_byte_view(
        destination_frame,
        vsapi,
        2,
        bits_per_sample,
        local.destination_v,
        plane_summary
    );

    if (status != Cnr3Status::ok) {
        return status;
    }

    if (!cnr3_triplet_plane_dimensions_are_compatible(local, sub_sampling_w, sub_sampling_h)) {
        return Cnr3Status::invalid_argument;
    }

    views = local;

    cnr3_publish_vapoursynth_frame_triplet_summary(
        bits_per_sample,
        storage_bytes,
        sub_sampling_w,
        sub_sampling_h,
        local.current_source_y.width,
        local.current_source_y.height,
        local.current_source_u.width,
        local.current_source_u.height,
        summary
    );

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



Cnr3Status cnr3_native_storage_bytes_for_bit_depth(
    int bits_per_sample,
    int& storage_bytes
) noexcept {
    storage_bytes = 0;

    if (bits_per_sample == 8) {
        storage_bytes = 1;
        return Cnr3Status::ok;
    }

    if (bits_per_sample > 8 && bits_per_sample <= 16) {
        storage_bytes = 2;
        return Cnr3Status::ok;
    }

    return Cnr3Status::invalid_argument;
}

Cnr3Status cnr3_load_native_plane_sample(
    const Cnr3ConstNativePlaneByteView& plane,
    int x,
    int y,
    int& output_sample
) noexcept {
    int storage_bytes = 0;
    const Cnr3Status storage_status = cnr3_native_storage_bytes_for_bit_depth(
        plane.bits_per_sample,
        storage_bytes
    );

    if (storage_status != Cnr3Status::ok) {
        return storage_status;
    }

    int sample_peak = 0;

    if (
        cnr3_sample_peak_for_bit_depth(plane.bits_per_sample, sample_peak) != Cnr3Status::ok ||
        !cnr3_const_native_plane_byte_view_is_valid(plane, storage_bytes) ||
        x < 0 ||
        y < 0 ||
        x >= plane.width ||
        y >= plane.height
        ) {
        return Cnr3Status::invalid_argument;
    }

    const auto* bytes = static_cast<const std::uint8_t*>(plane.data);
    const std::size_t offset = cnr3_native_plane_byte_offset(
        x,
        y,
        plane.stride_bytes,
        storage_bytes
    );

    int sample = 0;

    if (storage_bytes == 1) {
        sample = bytes[offset];
    } else {
        std::uint16_t native_sample = 0;
        std::memcpy(&native_sample, bytes + offset, sizeof(native_sample));
        sample = static_cast<int>(native_sample);
    }

    if (!cnr3_value_is_inclusive_range(sample, 0, sample_peak)) {
        return Cnr3Status::invalid_argument;
    }

    output_sample = sample;
    return Cnr3Status::ok;
}

Cnr3Status cnr3_store_native_plane_sample(
    Cnr3MutableNativePlaneByteView& plane,
    int x,
    int y,
    int sample
) noexcept {
    int storage_bytes = 0;
    const Cnr3Status storage_status = cnr3_native_storage_bytes_for_bit_depth(
        plane.bits_per_sample,
        storage_bytes
    );

    if (storage_status != Cnr3Status::ok) {
        return storage_status;
    }

    int sample_peak = 0;

    if (
        cnr3_sample_peak_for_bit_depth(plane.bits_per_sample, sample_peak) != Cnr3Status::ok ||
        !cnr3_mutable_native_plane_byte_view_is_valid(plane, storage_bytes) ||
        x < 0 ||
        y < 0 ||
        x >= plane.width ||
        y >= plane.height ||
        !cnr3_value_is_inclusive_range(sample, 0, sample_peak)
        ) {
        return Cnr3Status::invalid_argument;
    }

    auto* bytes = static_cast<std::uint8_t*>(plane.data);
    const std::size_t offset = cnr3_native_plane_byte_offset(
        x,
        y,
        plane.stride_bytes,
        storage_bytes
    );

    if (storage_bytes == 1) {
        bytes[offset] = static_cast<std::uint8_t>(sample);
    } else {
        const std::uint16_t native_sample = static_cast<std::uint16_t>(sample);
        std::memcpy(bytes + offset, &native_sample, sizeof(native_sample));
    }

    return Cnr3Status::ok;
}

Cnr3Status cnr3_copy_native_plane_to_scalar_buffer(
    const Cnr3ConstNativePlaneByteView& native_plane,
    Cnr3MutablePlaneBufferView& scalar_plane
) noexcept {
    int storage_bytes = 0;
    const Cnr3Status storage_status = cnr3_native_storage_bytes_for_bit_depth(
        native_plane.bits_per_sample,
        storage_bytes
    );

    if (storage_status != Cnr3Status::ok) {
        return storage_status;
    }

    if (
        !cnr3_const_native_plane_byte_view_is_valid(native_plane, storage_bytes) ||
        !cnr3_mutable_plane_view_is_valid(scalar_plane) ||
        !cnr3_native_plane_dimensions_match(native_plane, scalar_plane)
        ) {
        return Cnr3Status::invalid_argument;
    }

    const int sample_count = native_plane.width * native_plane.height;
    std::vector<int> resolved_samples;

    try {
        resolved_samples.resize(static_cast<std::size_t>(sample_count));
    } catch (...) {
        return Cnr3Status::allocation_failed;
    }

    for (int y = 0; y < native_plane.height; ++y) {
        for (int x = 0; x < native_plane.width; ++x) {
            int sample = 0;

            const Cnr3Status sample_status = cnr3_load_native_plane_sample(
                native_plane,
                x,
                y,
                sample
            );

            if (sample_status != Cnr3Status::ok) {
                return sample_status;
            }

            resolved_samples[static_cast<std::size_t>((y * native_plane.width) + x)] =
                sample;
        }
    }

    for (int y = 0; y < native_plane.height; ++y) {
        for (int x = 0; x < native_plane.width; ++x) {
            cnr3_write_plane_sample(
                scalar_plane,
                x,
                y,
                resolved_samples[static_cast<std::size_t>((y * native_plane.width) + x)]
            );
        }
    }

    return Cnr3Status::ok;
}

Cnr3Status cnr3_copy_scalar_buffer_to_native_plane(
    const Cnr3ConstPlaneBufferView& scalar_plane,
    Cnr3MutableNativePlaneByteView& native_plane
) noexcept {
    int storage_bytes = 0;
    const Cnr3Status storage_status = cnr3_native_storage_bytes_for_bit_depth(
        native_plane.bits_per_sample,
        storage_bytes
    );

    if (storage_status != Cnr3Status::ok) {
        return storage_status;
    }

    if (
        !cnr3_const_plane_view_is_valid(scalar_plane) ||
        !cnr3_mutable_native_plane_byte_view_is_valid(native_plane, storage_bytes) ||
        !cnr3_native_plane_dimensions_match(scalar_plane, native_plane)
        ) {
        return Cnr3Status::invalid_argument;
    }

    std::vector<std::uint8_t> resolved_bytes;

    try {
        resolved_bytes.assign(
            static_cast<std::size_t>(native_plane.stride_bytes) *
                static_cast<std::size_t>(native_plane.height),
            std::uint8_t{0}
        );
    } catch (...) {
        return Cnr3Status::allocation_failed;
    }

    Cnr3MutableNativePlaneByteView resolved_plane{
        resolved_bytes.data(),
        native_plane.width,
        native_plane.height,
        native_plane.stride_bytes,
        native_plane.bits_per_sample
    };

    for (int y = 0; y < scalar_plane.height; ++y) {
        for (int x = 0; x < scalar_plane.width; ++x) {
            const Cnr3Status sample_status = cnr3_store_native_plane_sample(
                resolved_plane,
                x,
                y,
                cnr3_plane_sample_at(scalar_plane, x, y)
            );

            if (sample_status != Cnr3Status::ok) {
                return sample_status;
            }
        }
    }

    auto* destination = static_cast<std::uint8_t*>(native_plane.data);

    for (int y = 0; y < native_plane.height; ++y) {
        for (int x = 0; x < native_plane.width; ++x) {
            const std::size_t offset = cnr3_native_plane_byte_offset(
                x,
                y,
                native_plane.stride_bytes,
                storage_bytes
            );

            if (storage_bytes == 1) {
                destination[offset] = resolved_bytes[offset];
            } else {
                std::memcpy(destination + offset, resolved_bytes.data() + offset, 2U);
            }
        }
    }

    return Cnr3Status::ok;
}



Cnr3Status cnr3_downsample_native_luma_plane_to_scalar_chroma_grid(
    const Cnr3ConstNativePlaneByteView& source_luma_native_plane,
    int sub_sampling_w,
    int sub_sampling_h,
    Cnr3MutablePlaneBufferView& output_downsampled_luma_plane,
    Cnr3DownsampledLumaPlaneProcessSummary& summary
) noexcept {
    int storage_bytes = 0;
    const Cnr3Status storage_status = cnr3_native_storage_bytes_for_bit_depth(
        source_luma_native_plane.bits_per_sample,
        storage_bytes
    );

    if (storage_status != Cnr3Status::ok) {
        return storage_status;
    }

    if (
        !cnr3_const_native_plane_byte_view_is_valid(
            source_luma_native_plane,
            storage_bytes
        ) ||
        !cnr3_mutable_plane_view_is_valid(output_downsampled_luma_plane)
        ) {
        return Cnr3Status::invalid_argument;
    }

    const int source_sample_count =
        source_luma_native_plane.width * source_luma_native_plane.height;
    std::vector<int> source_luma_scalar;

    try {
        source_luma_scalar.resize(static_cast<std::size_t>(source_sample_count));
    } catch (...) {
        return Cnr3Status::allocation_failed;
    }

    Cnr3MutablePlaneBufferView source_luma_scalar_plane{
        source_luma_scalar.data(),
        source_luma_native_plane.width,
        source_luma_native_plane.height,
        source_luma_native_plane.width
    };

    const Cnr3Status copy_status = cnr3_copy_native_plane_to_scalar_buffer(
        source_luma_native_plane,
        source_luma_scalar_plane
    );

    if (copy_status != Cnr3Status::ok) {
        return copy_status;
    }

    const Cnr3ConstPlaneBufferView source_luma_scalar_const_plane{
        source_luma_scalar.data(),
        source_luma_native_plane.width,
        source_luma_native_plane.height,
        source_luma_native_plane.width
    };

    return cnr3_downsample_luma_plane_to_chroma_grid(
        source_luma_scalar_const_plane,
        sub_sampling_w,
        sub_sampling_h,
        source_luma_native_plane.bits_per_sample,
        output_downsampled_luma_plane,
        summary
    );
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

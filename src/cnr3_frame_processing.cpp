#include "cnr3_frame_processing.h"

#include "cnr3_response_tables.h"

#include "VapourSynth4.h"

#include <cmath>
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

[[nodiscard]] bool cnr3_scdthr_is_valid(
    double scdthr
) noexcept {
    return std::isfinite(scdthr) && scdthr >= 0.0 && scdthr <= 100.0;
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

[[nodiscard]] bool cnr3_plane_sample_count_is_valid(
    int width,
    int height,
    int& sample_count
) noexcept {
    sample_count = 0;

    if (
        width <= 0 ||
        height <= 0 ||
        width > (std::numeric_limits<int>::max() / height)
        ) {
        return false;
    }

    sample_count = width * height;
    return true;
}

[[nodiscard]] Cnr3Status cnr3_allocate_scalar_plane_storage(
    int width,
    int height,
    std::vector<int>& storage
) noexcept {
    int sample_count = 0;

    if (!cnr3_plane_sample_count_is_valid(width, height, sample_count)) {
        return Cnr3Status::invalid_argument;
    }

    try {
        storage.assign(static_cast<std::size_t>(sample_count), 0);
    } catch (...) {
        return Cnr3Status::allocation_failed;
    }

    return Cnr3Status::ok;
}

[[nodiscard]] Cnr3Status cnr3_validate_response_tables_for_frame_process(
    int bits_per_sample,
    const Cnr3ResponseTables& tables
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
        tables.sample_peak != sample_peak ||
        tables.table_offset != expected_table_offset ||
        tables.table_size != expected_table_size ||
        tables.y.size() != static_cast<std::size_t>(expected_table_size) ||
        tables.u.size() != static_cast<std::size_t>(expected_table_size) ||
        tables.v.size() != static_cast<std::size_t>(expected_table_size)
        ) {
        return Cnr3Status::invalid_argument;
    }

    return Cnr3Status::ok;
}

/*
    Convert scalar samples into a throwaway native staging buffer. This deliberately
    preserves per-sample native-store validation, but it does not allocate a second
    all-or-nothing buffer: the caller has already allocated staged_bytes and the real
    destination frame is still protected by the later combined Y/U/V commit gate.
    Use cnr3_copy_scalar_buffer_to_native_plane when writing to a real destination.
*/
[[nodiscard]] Cnr3Status cnr3_convert_scalar_plane_into_native_staging_bytes(
    const Cnr3ConstPlaneBufferView& scalar_plane,
    Cnr3MutableNativePlaneByteView& staged_plane
) noexcept {
    int storage_bytes = 0;
    const Cnr3Status storage_status = cnr3_native_storage_bytes_for_bit_depth(
        staged_plane.bits_per_sample,
        storage_bytes
    );

    if (storage_status != Cnr3Status::ok) {
        return storage_status;
    }

    if (
        !cnr3_const_plane_view_is_valid(scalar_plane) ||
        !cnr3_mutable_native_plane_byte_view_is_valid(staged_plane, storage_bytes) ||
        !cnr3_native_plane_dimensions_match(scalar_plane, staged_plane)
        ) {
        return Cnr3Status::invalid_argument;
    }

    for (int y = 0; y < scalar_plane.height; ++y) {
        for (int x = 0; x < scalar_plane.width; ++x) {
            const Cnr3Status sample_status = cnr3_store_native_plane_sample(
                staged_plane,
                x,
                y,
                cnr3_plane_sample_at(scalar_plane, x, y)
            );

            if (sample_status != Cnr3Status::ok) {
                return sample_status;
            }
        }
    }

    return Cnr3Status::ok;
}

[[nodiscard]] Cnr3Status cnr3_stage_scalar_plane_to_native_bytes(
    const Cnr3ConstPlaneBufferView& scalar_plane,
    const Cnr3MutableNativePlaneByteView& destination_shape,
    std::vector<std::uint8_t>& staged_bytes
) noexcept {
    if (
        destination_shape.stride_bytes <= 0 ||
        destination_shape.height <= 0 ||
        destination_shape.height >
            (std::numeric_limits<int>::max() / destination_shape.stride_bytes)
        ) {
        return Cnr3Status::invalid_argument;
    }

    try {
        staged_bytes.assign(
            static_cast<std::size_t>(destination_shape.stride_bytes) *
                static_cast<std::size_t>(destination_shape.height),
            std::uint8_t{0}
        );
    } catch (...) {
        return Cnr3Status::allocation_failed;
    }

    Cnr3MutableNativePlaneByteView staged_plane{
        staged_bytes.data(),
        destination_shape.width,
        destination_shape.height,
        destination_shape.stride_bytes,
        destination_shape.bits_per_sample
    };

    int storage_bytes = 0;
    const Cnr3Status storage_status = cnr3_native_storage_bytes_for_bit_depth(
        staged_plane.bits_per_sample,
        storage_bytes
    );

    if (storage_status != Cnr3Status::ok) {
        return storage_status;
    }

    int sample_peak = 0;

    if (
        cnr3_sample_peak_for_bit_depth(staged_plane.bits_per_sample, sample_peak) !=
            Cnr3Status::ok ||
        !cnr3_const_plane_view_is_valid(scalar_plane) ||
        !cnr3_mutable_native_plane_byte_view_is_valid(staged_plane, storage_bytes) ||
        !cnr3_native_plane_dimensions_match(scalar_plane, staged_plane)
        ) {
        return Cnr3Status::invalid_argument;
    }

    auto* const staged_base = static_cast<std::uint8_t*>(staged_plane.data);
    const std::size_t staged_stride_bytes =
        static_cast<std::size_t>(staged_plane.stride_bytes);

    /*
        Staging-private fast path. The scalar samples are Tier-2 values produced
        through the Tier-1 source gate, but the store primitive's reject-on-
        out-of-range contract is still reproduced here before narrowing. The
        buffer is throwaway staging: a failure can leave staged_bytes partially
        written, but the real destination frame is not committed until all Y/U/V
        staging has succeeded.
    */
    if (storage_bytes == 1) {
        for (int y = 0; y < scalar_plane.height; ++y) {
            const int* __restrict scalar_row =
                scalar_plane.samples +
                (static_cast<std::size_t>(y) *
                    static_cast<std::size_t>(scalar_plane.stride));
            std::uint8_t* __restrict staged_row =
                staged_base + (static_cast<std::size_t>(y) * staged_stride_bytes);

            for (int x = 0; x < scalar_plane.width; ++x) {
                const int sample = scalar_row[x];

                if (!cnr3_value_is_inclusive_range(sample, 0, sample_peak)) {
                    return Cnr3Status::invalid_argument;
                }

                staged_row[x] = static_cast<std::uint8_t>(sample);
            }
        }
    } else {
        for (int y = 0; y < scalar_plane.height; ++y) {
            const int* __restrict scalar_row =
                scalar_plane.samples +
                (static_cast<std::size_t>(y) *
                    static_cast<std::size_t>(scalar_plane.stride));
            std::uint8_t* __restrict staged_row =
                staged_base + (static_cast<std::size_t>(y) * staged_stride_bytes);

            for (int x = 0; x < scalar_plane.width; ++x) {
                const int sample = scalar_row[x];

                if (!cnr3_value_is_inclusive_range(sample, 0, sample_peak)) {
                    return Cnr3Status::invalid_argument;
                }

                const std::uint16_t native_sample =
                    static_cast<std::uint16_t>(sample);
                std::memcpy(
                    staged_row + (static_cast<std::size_t>(x) * sizeof(native_sample)),
                    &native_sample,
                    sizeof(native_sample)
                );
            }
        }
    }

    return Cnr3Status::ok;
}

/*
    Stage native passthrough bytes only; do not publish to the destination frame here.
    The caller preserves all-or-nothing Y/U/V commit after combined validation.
*/
[[nodiscard]] Cnr3Status cnr3_stage_native_plane_passthrough_to_bytes(
    const Cnr3ConstNativePlaneByteView& source_native,
    const Cnr3MutableNativePlaneByteView& destination_shape,
    std::vector<std::uint8_t>& staged_bytes
) noexcept {
    int source_storage_bytes = 0;
    int destination_storage_bytes = 0;

    const Cnr3Status source_storage_status =
        cnr3_native_storage_bytes_for_bit_depth(
            source_native.bits_per_sample,
            source_storage_bytes
        );

    if (source_storage_status != Cnr3Status::ok) {
        return source_storage_status;
    }

    const Cnr3Status destination_storage_status =
        cnr3_native_storage_bytes_for_bit_depth(
            destination_shape.bits_per_sample,
            destination_storage_bytes
        );

    if (destination_storage_status != Cnr3Status::ok) {
        return destination_storage_status;
    }

    if (
        source_native.bits_per_sample != destination_shape.bits_per_sample ||
        source_storage_bytes != destination_storage_bytes ||
        !cnr3_const_native_plane_byte_view_is_valid(source_native, source_storage_bytes) ||
        !cnr3_mutable_native_plane_byte_view_is_valid(
            destination_shape,
            destination_storage_bytes
        ) ||
        source_native.width != destination_shape.width ||
        source_native.height != destination_shape.height
        ) {
        return Cnr3Status::invalid_argument;
    }

    try {
        staged_bytes.assign(
            static_cast<std::size_t>(destination_shape.stride_bytes) *
                static_cast<std::size_t>(destination_shape.height),
            std::uint8_t{0}
        );
    } catch (...) {
        return Cnr3Status::allocation_failed;
    }

    const auto* source_bytes = static_cast<const std::uint8_t*>(source_native.data);
    const std::size_t active_row_bytes =
        static_cast<std::size_t>(destination_shape.width) *
        static_cast<std::size_t>(destination_storage_bytes);

    for (int y = 0; y < destination_shape.height; ++y) {
        const std::size_t source_offset = cnr3_native_plane_byte_offset(
            0,
            y,
            source_native.stride_bytes,
            source_storage_bytes
        );
        const std::size_t staged_offset = cnr3_native_plane_byte_offset(
            0,
            y,
            destination_shape.stride_bytes,
            destination_storage_bytes
        );

        std::memcpy(
            staged_bytes.data() + staged_offset,
            source_bytes + source_offset,
            active_row_bytes
        );
    }

    return Cnr3Status::ok;
}

[[nodiscard]] bool cnr3_staged_native_active_copy_is_valid(
    const std::vector<std::uint8_t>& staged_bytes,
    const Cnr3MutableNativePlaneByteView& destination
) noexcept {
    int storage_bytes = 0;

    if (
        cnr3_native_storage_bytes_for_bit_depth(
            destination.bits_per_sample,
            storage_bytes
        ) != Cnr3Status::ok ||
        !cnr3_mutable_native_plane_byte_view_is_valid(destination, storage_bytes)
        ) {
        return false;
    }

    const std::size_t required_size =
        static_cast<std::size_t>(destination.stride_bytes) *
        static_cast<std::size_t>(destination.height);

    return staged_bytes.size() >= required_size;
}

void cnr3_commit_staged_native_active_samples(
    const std::vector<std::uint8_t>& staged_bytes,
    Cnr3MutableNativePlaneByteView& destination
) noexcept {
    int storage_bytes = 0;

    if (
        cnr3_native_storage_bytes_for_bit_depth(
            destination.bits_per_sample,
            storage_bytes
        ) != Cnr3Status::ok
        ) {
        return;
    }

    auto* const destination_base = static_cast<std::uint8_t*>(destination.data);
    const auto* const staged_base = staged_bytes.data();
    const std::size_t stride_bytes =
        static_cast<std::size_t>(destination.stride_bytes);
    const std::size_t active_row_bytes =
        static_cast<std::size_t>(destination.width) *
        static_cast<std::size_t>(storage_bytes);

    // staged_bytes is destination-stride-pitched by construction. The staging
    // path allocates stride_bytes * height and builds the staged view with the
    // destination stride, so the same row offset is valid for both buffers.
    for (int y = 0; y < destination.height; ++y) {
        std::uint8_t* __restrict destination_row =
            destination_base + (static_cast<std::size_t>(y) * stride_bytes);
        const std::uint8_t* __restrict staged_row =
            staged_base + (static_cast<std::size_t>(y) * stride_bytes);

        std::memcpy(destination_row, staged_row, active_row_bytes);
    }
}

void cnr3_publish_caller_supplied_frame_process_summary(
    const Cnr3VapourSynthFrameTripletViewSummary& triplet_summary,
    const Cnr3ChromaPlaneProcessSummary& u_summary,
    const Cnr3ChromaPlaneProcessSummary& v_summary,
    Cnr3CallerSuppliedFrameProcessSummary& summary
) noexcept {
    Cnr3CallerSuppliedFrameProcessSummary local{};
    local.bits_per_sample = triplet_summary.bits_per_sample;
    local.storage_bytes = triplet_summary.storage_bytes;
    local.sub_sampling_w = triplet_summary.sub_sampling_w;
    local.sub_sampling_h = triplet_summary.sub_sampling_h;
    local.luma_width = triplet_summary.luma_width;
    local.luma_height = triplet_summary.luma_height;
    local.chroma_width = triplet_summary.chroma_width;
    local.chroma_height = triplet_summary.chroma_height;
    local.luma_samples_copied = triplet_summary.luma_width * triplet_summary.luma_height;
    local.chroma_u_samples_processed = u_summary.samples_processed;
    local.chroma_v_samples_processed = v_summary.samples_processed;
    local.first_u_output_sample = u_summary.first_output_sample;
    local.last_u_output_sample = u_summary.last_output_sample;
    local.first_v_output_sample = v_summary.first_output_sample;
    local.last_v_output_sample = v_summary.last_output_sample;
    local.memcpy_byte_view_path_used = true;
    local.typed_row_pointer_optimization_deferred = true;
    local.frame_processed = true;
    summary = local;
}


struct Cnr3SceneChangeStats {
    std::int64_t diff_total = 0;
    std::int64_t scene_change_threshold = 0;
    int samples_examined = 0;
    bool scene_chroma = false;
    bool scene_change = false;
};

[[nodiscard]] std::int64_t cnr3_abs_int64(
    std::int64_t value
) noexcept {
    return (value < 0) ? -value : value;
}

[[nodiscard]] Cnr3Status cnr3_add_scene_diff(
    std::int64_t diff,
    std::int64_t& diff_total
) noexcept {
    if (diff < 0 || diff > (std::numeric_limits<std::int64_t>::max() - diff_total)) {
        return Cnr3Status::invalid_argument;
    }

    diff_total += diff;
    return Cnr3Status::ok;
}

[[nodiscard]] Cnr3Status cnr3_detect_scene_change_from_scalar_planes(
    const Cnr3ConstPlaneBufferView& current_downsampled_luma_plane,
    const Cnr3ConstPlaneBufferView& previous_downsampled_luma_plane,
    const Cnr3ConstPlaneBufferView& current_source_u_plane,
    const Cnr3ConstPlaneBufferView& previous_filtered_u_plane,
    const Cnr3ConstPlaneBufferView& current_source_v_plane,
    const Cnr3ConstPlaneBufferView& previous_filtered_v_plane,
    int sub_sampling_w,
    int sub_sampling_h,
    int bits_per_sample,
    const Cnr3SceneChangeConfig& config,
    Cnr3SceneChangeStats& stats
) noexcept {
    stats = Cnr3SceneChangeStats{};

    int sample_peak = 0;

    if (
        config.scene_change_threshold < 0 ||
        !cnr3_subsampling_factor_is_valid(sub_sampling_w) ||
        !cnr3_subsampling_factor_is_valid(sub_sampling_h) ||
        cnr3_sample_peak_for_bit_depth(bits_per_sample, sample_peak) != Cnr3Status::ok ||
        !cnr3_const_plane_view_is_valid(current_downsampled_luma_plane) ||
        !cnr3_const_plane_view_is_valid(previous_downsampled_luma_plane) ||
        !cnr3_const_plane_view_is_valid(current_source_u_plane) ||
        !cnr3_const_plane_view_is_valid(previous_filtered_u_plane) ||
        !cnr3_const_plane_view_is_valid(current_source_v_plane) ||
        !cnr3_const_plane_view_is_valid(previous_filtered_v_plane) ||
        !cnr3_const_plane_dimensions_match(
            previous_downsampled_luma_plane,
            current_downsampled_luma_plane.width,
            current_downsampled_luma_plane.height
        ) ||
        !cnr3_const_plane_dimensions_match(
            current_source_u_plane,
            current_downsampled_luma_plane.width,
            current_downsampled_luma_plane.height
        ) ||
        !cnr3_const_plane_dimensions_match(
            previous_filtered_u_plane,
            current_downsampled_luma_plane.width,
            current_downsampled_luma_plane.height
        ) ||
        !cnr3_const_plane_dimensions_match(
            current_source_v_plane,
            current_downsampled_luma_plane.width,
            current_downsampled_luma_plane.height
        ) ||
        !cnr3_const_plane_dimensions_match(
            previous_filtered_v_plane,
            current_downsampled_luma_plane.width,
            current_downsampled_luma_plane.height
        )
        ) {
        return Cnr3Status::invalid_argument;
    }

    stats.scene_change_threshold = config.scene_change_threshold;
    stats.scene_chroma = config.scene_chroma;

    const int luma_scale_shift = sub_sampling_w + sub_sampling_h;

    for (int y = 0; y < current_downsampled_luma_plane.height; ++y) {
        for (int x = 0; x < current_downsampled_luma_plane.width; ++x) {
            const int current_luma = cnr3_plane_sample_at(current_downsampled_luma_plane, x, y);
            const int previous_luma = cnr3_plane_sample_at(previous_downsampled_luma_plane, x, y);
            const int current_u = cnr3_plane_sample_at(current_source_u_plane, x, y);
            const int previous_u = cnr3_plane_sample_at(previous_filtered_u_plane, x, y);
            const int current_v = cnr3_plane_sample_at(current_source_v_plane, x, y);
            const int previous_v = cnr3_plane_sample_at(previous_filtered_v_plane, x, y);

            if (
                !cnr3_value_is_inclusive_range(current_luma, 0, sample_peak) ||
                !cnr3_value_is_inclusive_range(previous_luma, 0, sample_peak) ||
                !cnr3_value_is_inclusive_range(current_u, 0, sample_peak) ||
                !cnr3_value_is_inclusive_range(previous_u, 0, sample_peak) ||
                !cnr3_value_is_inclusive_range(current_v, 0, sample_peak) ||
                !cnr3_value_is_inclusive_range(previous_v, 0, sample_peak)
                ) {
                return Cnr3Status::invalid_argument;
            }

            const std::int64_t luma_diff =
                (static_cast<std::int64_t>(current_luma) -
                    static_cast<std::int64_t>(previous_luma)) << luma_scale_shift;

            Cnr3Status status = cnr3_add_scene_diff(
                cnr3_abs_int64(luma_diff),
                stats.diff_total
            );

            if (status != Cnr3Status::ok) {
                return status;
            }

            if (config.scene_chroma) {
                status = cnr3_add_scene_diff(
                    cnr3_abs_int64(
                        static_cast<std::int64_t>(current_u) -
                        static_cast<std::int64_t>(previous_u)
                    ),
                    stats.diff_total
                );

                if (status != Cnr3Status::ok) {
                    return status;
                }

                status = cnr3_add_scene_diff(
                    cnr3_abs_int64(
                        static_cast<std::int64_t>(current_v) -
                        static_cast<std::int64_t>(previous_v)
                    ),
                    stats.diff_total
                );

                if (status != Cnr3Status::ok) {
                    return status;
                }
            }

            ++stats.samples_examined;

            if (stats.diff_total > config.scene_change_threshold) {
                stats.scene_change = true;
                return Cnr3Status::ok;
            }
        }
    }

    return Cnr3Status::ok;
}

void cnr3_publish_chroma_copy_summary_from_scalar_plane(
    const Cnr3ConstPlaneBufferView& plane,
    Cnr3ChromaPlaneProcessSummary& summary
) noexcept {
    Cnr3ChromaPlaneProcessSummary local{};
    local.width = plane.width;
    local.height = plane.height;
    local.samples_processed = plane.width * plane.height;
    local.first_output_sample = cnr3_plane_sample_at(plane, 0, 0);
    local.last_output_sample = cnr3_plane_sample_at(
        plane,
        plane.width - 1,
        plane.height - 1
    );
    summary = local;
}


} // namespace

Cnr3Status cnr3_make_scene_change_config_from_vscnr2_scdthr(
    double scdthr,
    int full_width,
    int full_height,
    int bits_per_sample,
    int sub_sampling_w,
    int sub_sampling_h,
    bool scene_chroma,
    Cnr3SceneChangeConfig& out_config
) noexcept {
    out_config = Cnr3SceneChangeConfig{};

    int sample_peak = 0;

    if (
        !cnr3_scdthr_is_valid(scdthr) ||
        full_width <= 0 ||
        full_height <= 0 ||
        !cnr3_subsampling_factor_is_valid(sub_sampling_w) ||
        !cnr3_subsampling_factor_is_valid(sub_sampling_h) ||
        cnr3_sample_peak_for_bit_depth(bits_per_sample, sample_peak) != Cnr3Status::ok
        ) {
        return Cnr3Status::invalid_argument;
    }

    /*
        vsCnr2 derives diff_max from full-frame dimensions, not chroma-grid
        dimensions. CNR3's detector walks the chroma grid but shifts each luma
        difference up by sub_sampling_w + sub_sampling_h, which preserves the
        same full-frame luma-area unit. Using chroma-grid dimensions here would
        make the threshold too small for subsampled formats.

        The luma-only branch is deliberately bare 219. In the scene_chroma
        branch, the combined luma+chroma ceiling is shifted down by the
        subsampling area, matching the vsCnr2 conditional-expression shape.
    */
    const int max_pixel_diff = scene_chroma
        ? ((219 + (224 * 2)) >> (sub_sampling_w + sub_sampling_h))
        : 219;

    /*
        CNR3 deliberately applies the same accuracy policy used for P.2A native
        parameter scaling: keep the 8-bit-domain threshold full precision, scale
        to native depth by sample_peak / 255, then round once. This is a recorded
        improvement over vsCnr2's power-of-two depth factor and truncation.
    */
    const long double base8 =
        (static_cast<long double>(scdthr) *
            static_cast<long double>(full_width) *
            static_cast<long double>(full_height) *
            static_cast<long double>(max_pixel_diff)) /
        100.0L;

    const long double native_threshold =
        base8 * static_cast<long double>(sample_peak) / 255.0L;

    if (
        native_threshold < 0.0L ||
        native_threshold > static_cast<long double>(std::numeric_limits<std::int64_t>::max())
        ) {
        return Cnr3Status::invalid_argument;
    }

    out_config.scene_change_threshold =
        static_cast<std::int64_t>(std::llround(native_threshold));
    out_config.scene_chroma = scene_chroma;

    return Cnr3Status::ok;
}

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


namespace {

Cnr3Status cnr3_process_caller_supplied_vapoursynth_frame_triplet_impl(
    const VSFrame* current_source_frame,
    const VSFrame* previous_filtered_output_frame,
    VSFrame* destination_frame,
    const VSAPI* vsapi,
    int bits_per_sample,
    int sub_sampling_w,
    int sub_sampling_h,
    const Cnr3ResponseTables& response_tables,
    const Cnr3SceneChangeConfig* scene_config,
    Cnr3CallerSuppliedFrameProcessSummary& summary
) noexcept {
    summary = Cnr3CallerSuppliedFrameProcessSummary{};

    Cnr3VapourSynthFrameTripletNativeViews views{};
    Cnr3VapourSynthFrameTripletViewSummary triplet_summary{};

    Cnr3Status status = cnr3_make_caller_supplied_vapoursynth_frame_triplet_views(
        current_source_frame,
        previous_filtered_output_frame,
        destination_frame,
        vsapi,
        bits_per_sample,
        sub_sampling_w,
        sub_sampling_h,
        views,
        triplet_summary
    );

    if (status != Cnr3Status::ok) {
        return status;
    }

    status = cnr3_validate_response_tables_for_frame_process(
        bits_per_sample,
        response_tables
    );

    if (status != Cnr3Status::ok) {
        return status;
    }

    std::vector<int> current_downsampled_luma_storage;
    std::vector<int> previous_downsampled_luma_storage;
    std::vector<int> current_u_storage;
    std::vector<int> current_v_storage;
    std::vector<int> previous_u_storage;
    std::vector<int> previous_v_storage;
    std::vector<int> output_u_storage;
    std::vector<int> output_v_storage;

    status = cnr3_allocate_scalar_plane_storage(
        triplet_summary.chroma_width,
        triplet_summary.chroma_height,
        current_downsampled_luma_storage
    );

    if (status != Cnr3Status::ok) {
        return status;
    }

    status = cnr3_allocate_scalar_plane_storage(
        triplet_summary.chroma_width,
        triplet_summary.chroma_height,
        previous_downsampled_luma_storage
    );

    if (status != Cnr3Status::ok) {
        return status;
    }

    status = cnr3_allocate_scalar_plane_storage(
        triplet_summary.chroma_width,
        triplet_summary.chroma_height,
        current_u_storage
    );

    if (status != Cnr3Status::ok) {
        return status;
    }

    status = cnr3_allocate_scalar_plane_storage(
        triplet_summary.chroma_width,
        triplet_summary.chroma_height,
        current_v_storage
    );

    if (status != Cnr3Status::ok) {
        return status;
    }

    status = cnr3_allocate_scalar_plane_storage(
        triplet_summary.chroma_width,
        triplet_summary.chroma_height,
        previous_u_storage
    );

    if (status != Cnr3Status::ok) {
        return status;
    }

    status = cnr3_allocate_scalar_plane_storage(
        triplet_summary.chroma_width,
        triplet_summary.chroma_height,
        previous_v_storage
    );

    if (status != Cnr3Status::ok) {
        return status;
    }

    status = cnr3_allocate_scalar_plane_storage(
        triplet_summary.chroma_width,
        triplet_summary.chroma_height,
        output_u_storage
    );

    if (status != Cnr3Status::ok) {
        return status;
    }

    status = cnr3_allocate_scalar_plane_storage(
        triplet_summary.chroma_width,
        triplet_summary.chroma_height,
        output_v_storage
    );

    if (status != Cnr3Status::ok) {
        return status;
    }

    Cnr3MutablePlaneBufferView current_downsampled_luma_mutable{
        current_downsampled_luma_storage.data(),
        triplet_summary.chroma_width,
        triplet_summary.chroma_height,
        triplet_summary.chroma_width
    };
    Cnr3DownsampledLumaPlaneProcessSummary current_luma_summary{};

    status = cnr3_downsample_native_luma_plane_to_scalar_chroma_grid(
        views.current_source_y,
        sub_sampling_w,
        sub_sampling_h,
        current_downsampled_luma_mutable,
        current_luma_summary
    );

    if (status != Cnr3Status::ok) {
        return status;
    }

    Cnr3MutablePlaneBufferView previous_downsampled_luma_mutable{
        previous_downsampled_luma_storage.data(),
        triplet_summary.chroma_width,
        triplet_summary.chroma_height,
        triplet_summary.chroma_width
    };
    Cnr3DownsampledLumaPlaneProcessSummary previous_luma_summary{};

    status = cnr3_downsample_native_luma_plane_to_scalar_chroma_grid(
        views.previous_filtered_y,
        sub_sampling_w,
        sub_sampling_h,
        previous_downsampled_luma_mutable,
        previous_luma_summary
    );

    if (status != Cnr3Status::ok) {
        return status;
    }

    Cnr3MutablePlaneBufferView current_u_mutable{
        current_u_storage.data(),
        triplet_summary.chroma_width,
        triplet_summary.chroma_height,
        triplet_summary.chroma_width
    };
    Cnr3MutablePlaneBufferView current_v_mutable{
        current_v_storage.data(),
        triplet_summary.chroma_width,
        triplet_summary.chroma_height,
        triplet_summary.chroma_width
    };
    Cnr3MutablePlaneBufferView previous_u_mutable{
        previous_u_storage.data(),
        triplet_summary.chroma_width,
        triplet_summary.chroma_height,
        triplet_summary.chroma_width
    };
    Cnr3MutablePlaneBufferView previous_v_mutable{
        previous_v_storage.data(),
        triplet_summary.chroma_width,
        triplet_summary.chroma_height,
        triplet_summary.chroma_width
    };

    status = cnr3_copy_native_plane_to_scalar_buffer(views.current_source_u, current_u_mutable);

    if (status != Cnr3Status::ok) {
        return status;
    }

    status = cnr3_copy_native_plane_to_scalar_buffer(views.current_source_v, current_v_mutable);

    if (status != Cnr3Status::ok) {
        return status;
    }

    status = cnr3_copy_native_plane_to_scalar_buffer(views.previous_filtered_u, previous_u_mutable);

    if (status != Cnr3Status::ok) {
        return status;
    }

    status = cnr3_copy_native_plane_to_scalar_buffer(views.previous_filtered_v, previous_v_mutable);

    if (status != Cnr3Status::ok) {
        return status;
    }

    const Cnr3ConstPlaneBufferView current_downsampled_luma{
        current_downsampled_luma_storage.data(),
        triplet_summary.chroma_width,
        triplet_summary.chroma_height,
        triplet_summary.chroma_width
    };
    const Cnr3ConstPlaneBufferView previous_downsampled_luma{
        previous_downsampled_luma_storage.data(),
        triplet_summary.chroma_width,
        triplet_summary.chroma_height,
        triplet_summary.chroma_width
    };
    const Cnr3ConstPlaneBufferView current_u{
        current_u_storage.data(),
        triplet_summary.chroma_width,
        triplet_summary.chroma_height,
        triplet_summary.chroma_width
    };
    const Cnr3ConstPlaneBufferView current_v{
        current_v_storage.data(),
        triplet_summary.chroma_width,
        triplet_summary.chroma_height,
        triplet_summary.chroma_width
    };
    const Cnr3ConstPlaneBufferView previous_u{
        previous_u_storage.data(),
        triplet_summary.chroma_width,
        triplet_summary.chroma_height,
        triplet_summary.chroma_width
    };
    const Cnr3ConstPlaneBufferView previous_v{
        previous_v_storage.data(),
        triplet_summary.chroma_width,
        triplet_summary.chroma_height,
        triplet_summary.chroma_width
    };

    Cnr3MutablePlaneBufferView output_u_mutable{
        output_u_storage.data(),
        triplet_summary.chroma_width,
        triplet_summary.chroma_height,
        triplet_summary.chroma_width
    };
    Cnr3MutablePlaneBufferView output_v_mutable{
        output_v_storage.data(),
        triplet_summary.chroma_width,
        triplet_summary.chroma_height,
        triplet_summary.chroma_width
    };

    Cnr3SceneChangeStats scene_stats{};
    bool scene_change_detection_used = false;

    if (scene_config != nullptr) {
        scene_change_detection_used = true;

        status = cnr3_detect_scene_change_from_scalar_planes(
            current_downsampled_luma,
            previous_downsampled_luma,
            current_u,
            previous_u,
            current_v,
            previous_v,
            sub_sampling_w,
            sub_sampling_h,
            bits_per_sample,
            *scene_config,
            scene_stats
        );

        if (status != Cnr3Status::ok) {
            return status;
        }
    }

    Cnr3ChromaPlaneProcessSummary u_summary{};
    Cnr3ChromaPlaneProcessSummary v_summary{};

    if (scene_change_detection_used && scene_stats.scene_change) {
        output_u_storage = current_u_storage;
        output_v_storage = current_v_storage;
        cnr3_publish_chroma_copy_summary_from_scalar_plane(current_u, u_summary);
        cnr3_publish_chroma_copy_summary_from_scalar_plane(current_v, v_summary);
    } else {
        status = cnr3_process_chroma_plane_from_downsampled_luma(
            current_downsampled_luma,
            previous_downsampled_luma,
            current_u,
            previous_u,
            response_tables.y,
            response_tables.u,
            response_tables.table_offset,
            bits_per_sample,
            output_u_mutable,
            u_summary
        );

        if (status != Cnr3Status::ok) {
            return status;
        }

        status = cnr3_process_chroma_plane_from_downsampled_luma(
            current_downsampled_luma,
            previous_downsampled_luma,
            current_v,
            previous_v,
            response_tables.y,
            response_tables.v,
            response_tables.table_offset,
            bits_per_sample,
            output_v_mutable,
            v_summary
        );

        if (status != Cnr3Status::ok) {
            return status;
        }
    }

    std::vector<std::uint8_t> staged_y;
    std::vector<std::uint8_t> staged_u;
    std::vector<std::uint8_t> staged_v;

    status = cnr3_stage_native_plane_passthrough_to_bytes(
        views.current_source_y,
        views.destination_y,
        staged_y
    );

    if (status != Cnr3Status::ok) {
        return status;
    }

    const Cnr3ConstPlaneBufferView output_u{
        output_u_storage.data(),
        triplet_summary.chroma_width,
        triplet_summary.chroma_height,
        triplet_summary.chroma_width
    };

    status = cnr3_stage_scalar_plane_to_native_bytes(
        output_u,
        views.destination_u,
        staged_u
    );

    if (status != Cnr3Status::ok) {
        return status;
    }

    const Cnr3ConstPlaneBufferView output_v{
        output_v_storage.data(),
        triplet_summary.chroma_width,
        triplet_summary.chroma_height,
        triplet_summary.chroma_width
    };

    status = cnr3_stage_scalar_plane_to_native_bytes(
        output_v,
        views.destination_v,
        staged_v
    );

    if (status != Cnr3Status::ok) {
        return status;
    }

    if (
        !cnr3_staged_native_active_copy_is_valid(staged_y, views.destination_y) ||
        !cnr3_staged_native_active_copy_is_valid(staged_u, views.destination_u) ||
        !cnr3_staged_native_active_copy_is_valid(staged_v, views.destination_v)
        ) {
        return Cnr3Status::invalid_argument;
    }

    cnr3_commit_staged_native_active_samples(staged_y, views.destination_y);
    cnr3_commit_staged_native_active_samples(staged_u, views.destination_u);
    cnr3_commit_staged_native_active_samples(staged_v, views.destination_v);

    cnr3_publish_caller_supplied_frame_process_summary(
        triplet_summary,
        u_summary,
        v_summary,
        summary
    );

    if (scene_change_detection_used) {
        summary.scene_change_detection_used = true;
        summary.scene_chroma_used = scene_stats.scene_chroma;
        summary.scene_change_detected = scene_stats.scene_change;
        summary.scene_change_reset_output_used = scene_stats.scene_change;
        summary.recursive_chroma_blend_used = !scene_stats.scene_change;
        summary.scene_change_threshold = scene_stats.scene_change_threshold;
        summary.scene_change_diff_total = scene_stats.diff_total;
        summary.scene_change_samples_examined = scene_stats.samples_examined;
    } else {
        summary.recursive_chroma_blend_used = true;
    }

    return Cnr3Status::ok;
}

} // namespace

Cnr3Status cnr3_process_caller_supplied_vapoursynth_frame_triplet(
    const VSFrame* current_source_frame,
    const VSFrame* previous_filtered_output_frame,
    VSFrame* destination_frame,
    const VSAPI* vsapi,
    int bits_per_sample,
    int sub_sampling_w,
    int sub_sampling_h,
    const Cnr3ResponseTables& response_tables,
    Cnr3CallerSuppliedFrameProcessSummary& summary
) noexcept {
    return cnr3_process_caller_supplied_vapoursynth_frame_triplet_impl(
        current_source_frame,
        previous_filtered_output_frame,
        destination_frame,
        vsapi,
        bits_per_sample,
        sub_sampling_w,
        sub_sampling_h,
        response_tables,
        nullptr,
        summary
    );
}

Cnr3Status cnr3_process_caller_supplied_vapoursynth_frame_triplet_with_scene_change(
    const VSFrame* current_source_frame,
    const VSFrame* previous_filtered_output_frame,
    VSFrame* destination_frame,
    const VSAPI* vsapi,
    int bits_per_sample,
    int sub_sampling_w,
    int sub_sampling_h,
    const Cnr3ResponseTables& response_tables,
    const Cnr3SceneChangeConfig& scene_config,
    Cnr3CallerSuppliedFrameProcessSummary& summary
) noexcept {
    return cnr3_process_caller_supplied_vapoursynth_frame_triplet_impl(
        current_source_frame,
        previous_filtered_output_frame,
        destination_frame,
        vsapi,
        bits_per_sample,
        sub_sampling_w,
        sub_sampling_h,
        response_tables,
        &scene_config,
        summary
    );
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

    int sample_peak = 0;
    const Cnr3Status peak_status = cnr3_sample_peak_for_bit_depth(
        native_plane.bits_per_sample,
        sample_peak
    );

    if (peak_status != Cnr3Status::ok) {
        return peak_status;
    }

    const auto* base_bytes = static_cast<const std::uint8_t*>(native_plane.data);

    if (storage_bytes == 2) {
        unsigned int out_of_range_sample_seen = 0;
        const std::uint16_t sample_peak_u16 = static_cast<std::uint16_t>(sample_peak);

        for (int y = 0; y < native_plane.height; ++y) {
            const auto* row = base_bytes +
                (static_cast<std::size_t>(y) *
                    static_cast<std::size_t>(native_plane.stride_bytes));

            for (int x = 0; x < native_plane.width; ++x) {
                std::uint16_t native_sample = 0;
                // Preserve the previous unaligned-safe two-byte native load.
                std::memcpy(
                    &native_sample,
                    row + (static_cast<std::size_t>(x) * sizeof(native_sample)),
                    sizeof(native_sample)
                );

                out_of_range_sample_seen |=
                    static_cast<unsigned int>(native_sample > sample_peak_u16);
            }
        }

        if (out_of_range_sample_seen != 0U) {
            return Cnr3Status::invalid_argument;
        }
    }

    if (storage_bytes == 1) {
        // For the 8-bit storage path, uint8_t already spans exactly [0, 255].
        for (int y = 0; y < native_plane.height; ++y) {
            const std::uint8_t* __restrict source_row = base_bytes +
                (static_cast<std::size_t>(y) *
                    static_cast<std::size_t>(native_plane.stride_bytes));
            int* __restrict resolved_row = resolved_samples.data() +
                (static_cast<std::size_t>(y) *
                    static_cast<std::size_t>(native_plane.width));

            for (int x = 0; x < native_plane.width; ++x) {
                resolved_row[x] = static_cast<int>(source_row[x]);
            }
        }

        for (int y = 0; y < scalar_plane.height; ++y) {
            const int* __restrict resolved_row = resolved_samples.data() +
                (static_cast<std::size_t>(y) *
                    static_cast<std::size_t>(native_plane.width));
            int* __restrict scalar_row = scalar_plane.samples +
                (static_cast<std::size_t>(y) *
                    static_cast<std::size_t>(scalar_plane.stride));

            for (int x = 0; x < scalar_plane.width; ++x) {
                scalar_row[x] = resolved_row[x];
            }
        }

        return Cnr3Status::ok;
    } else {
        for (int y = 0; y < native_plane.height; ++y) {
            const auto* row = base_bytes +
                (static_cast<std::size_t>(y) *
                    static_cast<std::size_t>(native_plane.stride_bytes));

            for (int x = 0; x < native_plane.width; ++x) {
                std::uint16_t native_sample = 0;
                // Preserve the previous unaligned-safe two-byte native load.
                std::memcpy(
                    &native_sample,
                    row + (static_cast<std::size_t>(x) * sizeof(native_sample)),
                    sizeof(native_sample)
                );

                resolved_samples[static_cast<std::size_t>((y * native_plane.width) + x)] =
                    static_cast<int>(native_sample);
            }
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

    int expected_output_width = 0;
    int expected_output_height = 0;

    const Cnr3Status width_status =
        cnr3_expected_chroma_dimension_for_luma_dimension(
            source_luma_native_plane.width,
            sub_sampling_w,
            expected_output_width
        );

    if (width_status != Cnr3Status::ok) {
        return width_status;
    }

    const Cnr3Status height_status =
        cnr3_expected_chroma_dimension_for_luma_dimension(
            source_luma_native_plane.height,
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

    int sample_peak = 0;
    const Cnr3Status peak_status = cnr3_sample_peak_for_bit_depth(
        source_luma_native_plane.bits_per_sample,
        sample_peak
    );

    if (peak_status != Cnr3Status::ok) {
        return peak_status;
    }

    const auto* base_bytes =
        static_cast<const std::uint8_t*>(source_luma_native_plane.data);

    if (storage_bytes == 2) {
        unsigned int out_of_range_sample_seen = 0;
        const std::uint16_t sample_peak_u16 = static_cast<std::uint16_t>(sample_peak);

        for (int y = 0; y < source_luma_native_plane.height; ++y) {
            const auto* row = base_bytes +
                (static_cast<std::size_t>(y) *
                    static_cast<std::size_t>(source_luma_native_plane.stride_bytes));

            for (int x = 0; x < source_luma_native_plane.width; ++x) {
                std::uint16_t native_sample = 0;
                // Preserve the unaligned-safe two-byte native load used by P.8A.
                std::memcpy(
                    &native_sample,
                    row + (static_cast<std::size_t>(x) * sizeof(native_sample)),
                    sizeof(native_sample)
                );

                out_of_range_sample_seen |=
                    static_cast<unsigned int>(native_sample > sample_peak_u16);
            }
        }

        if (out_of_range_sample_seen != 0U) {
            return Cnr3Status::invalid_argument;
        }
    }

    const int sample_count = expected_output_width * expected_output_height;
    std::vector<int> resolved_outputs;

    try {
        resolved_outputs.resize(static_cast<std::size_t>(sample_count));
    } catch (...) {
        return Cnr3Status::allocation_failed;
    }

    const int source_width = source_luma_native_plane.width;
    const int source_height = source_luma_native_plane.height;
    const int source_stride_bytes = source_luma_native_plane.stride_bytes;

    if (storage_bytes == 1) {
        for (int y = 0; y < expected_output_height; ++y) {
            const int y0 = y << sub_sampling_h;

            if (y0 >= source_height) {
                return Cnr3Status::invalid_argument;
            }

            /*
                Preserve the asymmetric vsCnr2-compatible tap geometry exactly:
                x1 intentionally uses x0 + 1 when available, even when
                sub_sampling_w is 0, while y1 uses y0 + sub_sampling_h. Do not
                symmetrise these rules; 4:4:4 still reads horizontal neighbours.
            */
            const int y1 = (y0 + sub_sampling_h < source_height) ?
                (y0 + sub_sampling_h) : y0;

            const auto* const row0 = base_bytes +
                (static_cast<std::size_t>(y0) * static_cast<std::size_t>(source_stride_bytes));
            const auto* const row1 = base_bytes +
                (static_cast<std::size_t>(y1) * static_cast<std::size_t>(source_stride_bytes));
            int* const resolved_row = resolved_outputs.data() +
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(expected_output_width));

            for (int x = 0; x < expected_output_width; ++x) {
                const int x0 = x << sub_sampling_w;

                if (x0 >= source_width) {
                    return Cnr3Status::invalid_argument;
                }

                const int x1 = (x0 < source_width - 1) ? (x0 + 1) : x0;
                const int top_left_sample = static_cast<int>(row0[x0]);
                const int top_right_sample = static_cast<int>(row0[x1]);
                const int bottom_left_sample = static_cast<int>(row1[x0]);
                const int bottom_right_sample = static_cast<int>(row1[x1]);

                resolved_row[x] =
                    (
                        top_left_sample +
                        top_right_sample +
                        bottom_left_sample +
                        bottom_right_sample +
                        2
                    ) >> 2;
            }
        }
    } else {
        for (int y = 0; y < expected_output_height; ++y) {
            const int y0 = y << sub_sampling_h;

            if (y0 >= source_height) {
                return Cnr3Status::invalid_argument;
            }

            /*
                Preserve the asymmetric vsCnr2-compatible tap geometry exactly:
                x1 intentionally uses x0 + 1 when available, even when
                sub_sampling_w is 0, while y1 uses y0 + sub_sampling_h. Do not
                symmetrise these rules; 4:4:4 still reads horizontal neighbours.
            */
            const int y1 = (y0 + sub_sampling_h < source_height) ?
                (y0 + sub_sampling_h) : y0;

            const auto* const row0 = base_bytes +
                (static_cast<std::size_t>(y0) * static_cast<std::size_t>(source_stride_bytes));
            const auto* const row1 = base_bytes +
                (static_cast<std::size_t>(y1) * static_cast<std::size_t>(source_stride_bytes));
            int* const resolved_row = resolved_outputs.data() +
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(expected_output_width));

            for (int x = 0; x < expected_output_width; ++x) {
                const int x0 = x << sub_sampling_w;

                if (x0 >= source_width) {
                    return Cnr3Status::invalid_argument;
                }

                const int x1 = (x0 < source_width - 1) ? (x0 + 1) : x0;
                const std::size_t x0_offset =
                    static_cast<std::size_t>(x0) * static_cast<std::size_t>(storage_bytes);
                const std::size_t x1_offset =
                    static_cast<std::size_t>(x1) * static_cast<std::size_t>(storage_bytes);
                std::uint16_t top_left_native = 0;
                std::uint16_t top_right_native = 0;
                std::uint16_t bottom_left_native = 0;
                std::uint16_t bottom_right_native = 0;

                // Preserve unaligned-safe native reads; the pre-pass already
                // proved the 9..16-bit source samples are in range.
                std::memcpy(&top_left_native, row0 + x0_offset, sizeof(top_left_native));
                std::memcpy(&top_right_native, row0 + x1_offset, sizeof(top_right_native));
                std::memcpy(&bottom_left_native, row1 + x0_offset, sizeof(bottom_left_native));
                std::memcpy(&bottom_right_native, row1 + x1_offset, sizeof(bottom_right_native));

                resolved_row[x] =
                    (
                        static_cast<int>(top_left_native) +
                        static_cast<int>(top_right_native) +
                        static_cast<int>(bottom_left_native) +
                        static_cast<int>(bottom_right_native) +
                        2
                    ) >> 2;
            }
        }
    }

    for (int y = 0; y < expected_output_height; ++y) {
        const int* const resolved_row = resolved_outputs.data() +
            (static_cast<std::size_t>(y) * static_cast<std::size_t>(expected_output_width));
        int* const output_row = output_downsampled_luma_plane.samples +
            (static_cast<std::size_t>(y) *
                static_cast<std::size_t>(output_downsampled_luma_plane.stride));

        for (int x = 0; x < expected_output_width; ++x) {
            output_row[x] = resolved_row[x];
        }
    }

    Cnr3DownsampledLumaPlaneProcessSummary resolved_summary{};
    resolved_summary.source_width = source_luma_native_plane.width;
    resolved_summary.source_height = source_luma_native_plane.height;
    resolved_summary.output_width = expected_output_width;
    resolved_summary.output_height = expected_output_height;
    resolved_summary.samples_processed = sample_count;
    resolved_summary.first_output_sample = resolved_outputs.front();
    resolved_summary.last_output_sample = resolved_outputs.back();

    summary = resolved_summary;
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

    int sample_peak = 0;
    const Cnr3Status peak_status = cnr3_sample_peak_for_bit_depth(
        bits_per_sample,
        sample_peak
    );

    if (peak_status != Cnr3Status::ok) {
        return peak_status;
    }

    const int sample_count = expected_output_width * expected_output_height;
    std::vector<int> resolved_outputs;

    try {
        resolved_outputs.resize(static_cast<std::size_t>(sample_count));
    } catch (...) {
        return Cnr3Status::allocation_failed;
    }

    const int* const source_samples = source_luma_plane.samples;
    const int source_stride = source_luma_plane.stride;
    const int source_width = source_luma_plane.width;
    const int source_height = source_luma_plane.height;

    for (int y = 0; y < expected_output_height; ++y) {
        const int y0 = y << sub_sampling_h;

        if (y0 >= source_height) {
            return Cnr3Status::invalid_argument;
        }

        /*
            Preserve the asymmetric vsCnr2-compatible tap geometry exactly:
            x1 intentionally uses x0 + 1 when available, even when
            sub_sampling_w is 0, while y1 uses y0 + sub_sampling_h. Do not
            symmetrise these rules; 4:4:4 still reads horizontal neighbours.
        */
        const int y1 = (y0 + sub_sampling_h < source_height) ?
            (y0 + sub_sampling_h) : y0;

        const int* const row0 = source_samples +
            (static_cast<std::size_t>(y0) * static_cast<std::size_t>(source_stride));
        const int* const row1 = source_samples +
            (static_cast<std::size_t>(y1) * static_cast<std::size_t>(source_stride));

        for (int x = 0; x < expected_output_width; ++x) {
            const int x0 = x << sub_sampling_w;

            if (x0 >= source_width) {
                return Cnr3Status::invalid_argument;
            }

            const int x1 = (x0 < source_width - 1) ? (x0 + 1) : x0;
            const int top_left_sample = row0[x0];
            const int top_right_sample = row0[x1];
            const int bottom_left_sample = row1[x0];
            const int bottom_right_sample = row1[x1];

            if (
                !cnr3_value_is_inclusive_range(top_left_sample, 0, sample_peak) ||
                !cnr3_value_is_inclusive_range(top_right_sample, 0, sample_peak) ||
                !cnr3_value_is_inclusive_range(bottom_left_sample, 0, sample_peak) ||
                !cnr3_value_is_inclusive_range(bottom_right_sample, 0, sample_peak)
                ) {
                return Cnr3Status::invalid_argument;
            }

            resolved_outputs[static_cast<std::size_t>((y * expected_output_width) + x)] =
                (
                    top_left_sample +
                    top_right_sample +
                    bottom_left_sample +
                    bottom_right_sample +
                    2
                ) >> 2;
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
        chroma_response_table.size() != static_cast<std::size_t>(expected_table_size)
        ) {
        return Cnr3Status::invalid_argument;
    }

    /*
        Preserve the tested no-partial-output contract for this shared traversal
        primitive: reject any out-of-range scalar input before publishing to the
        destination plane. The fused production loop below then trusts Tier-2
        inputs and contains no per-sample status path.
    */
    for (int y = 0; y < height; ++y) {
        const int* __restrict current_luma_row =
            current_downsampled_luma_plane.samples +
            (static_cast<std::size_t>(y) *
                static_cast<std::size_t>(current_downsampled_luma_plane.stride));
        const int* __restrict previous_luma_row =
            previous_downsampled_luma_plane.samples +
            (static_cast<std::size_t>(y) *
                static_cast<std::size_t>(previous_downsampled_luma_plane.stride));
        const int* __restrict current_chroma_row =
            current_source_chroma_plane.samples +
            (static_cast<std::size_t>(y) *
                static_cast<std::size_t>(current_source_chroma_plane.stride));
        const int* __restrict previous_chroma_row =
            previous_filtered_chroma_plane.samples +
            (static_cast<std::size_t>(y) *
                static_cast<std::size_t>(previous_filtered_chroma_plane.stride));

        for (int x = 0; x < width; ++x) {
            if (
                !cnr3_value_is_inclusive_range(current_luma_row[x], 0, sample_peak) ||
                !cnr3_value_is_inclusive_range(previous_luma_row[x], 0, sample_peak) ||
                !cnr3_value_is_inclusive_range(current_chroma_row[x], 0, sample_peak) ||
                !cnr3_value_is_inclusive_range(previous_chroma_row[x], 0, sample_peak)
                ) {
                return Cnr3Status::invalid_argument;
            }
        }
    }

    const int sample_count = width * height;
    const int shift2 = bits_per_sample << 1;
    const std::int64_t shift = std::int64_t{1} << shift2;
    const std::int64_t shift1 = shift >> 1;
    const int* __restrict y_response_values = y_response_table.data();
    const int* __restrict chroma_response_values = chroma_response_table.data();

    /*
        Production-private fast path per validation policy:
        - scalar inputs are Tier-2 values produced through the Tier-1 source gate;
        - response values are Tier-3 values from sanctioned, geometry-validated tables.
        If either provenance invariant changes, the per-sample range checks from the
        shared blend primitives must be restored or replaced by an equivalent guard.
    */
    int first_output_sample = 0;
    int last_output_sample = 0;
    bool have_output_sample = false;

    for (int y = 0; y < height; ++y) {
        const int* __restrict current_luma_row =
            current_downsampled_luma_plane.samples +
            (static_cast<std::size_t>(y) *
                static_cast<std::size_t>(current_downsampled_luma_plane.stride));
        const int* __restrict previous_luma_row =
            previous_downsampled_luma_plane.samples +
            (static_cast<std::size_t>(y) *
                static_cast<std::size_t>(previous_downsampled_luma_plane.stride));
        const int* __restrict current_chroma_row =
            current_source_chroma_plane.samples +
            (static_cast<std::size_t>(y) *
                static_cast<std::size_t>(current_source_chroma_plane.stride));
        const int* __restrict previous_chroma_row =
            previous_filtered_chroma_plane.samples +
            (static_cast<std::size_t>(y) *
                static_cast<std::size_t>(previous_filtered_chroma_plane.stride));
        int* __restrict output_row =
            output_chroma_plane.samples +
            (static_cast<std::size_t>(y) *
                static_cast<std::size_t>(output_chroma_plane.stride));

        for (int x = 0; x < width; ++x) {
            const int current_luma = current_luma_row[x];
            const int previous_luma = previous_luma_row[x];
            const int current_chroma = current_chroma_row[x];
            const int previous_chroma = previous_chroma_row[x];

            const int luma_signed_diff = current_luma - previous_luma;
            const int chroma_signed_diff = current_chroma - previous_chroma;
            const int y_response_index = luma_signed_diff + table_offset;
            const int chroma_response_index = chroma_signed_diff + table_offset;
            const int y_response =
                (y_response_index >= 0 && y_response_index < expected_table_size) ?
                    y_response_values[y_response_index] :
                    0;
            const int chroma_response =
                (chroma_response_index >= 0 && chroma_response_index < expected_table_size) ?
                    chroma_response_values[chroma_response_index] :
                    0;
            const std::int64_t weight =
                static_cast<std::int64_t>(y_response) *
                static_cast<std::int64_t>(chroma_response);

            /*
                Bit-exact P.3A/P.5A blend arithmetic. Weight pulls toward the
                previous filtered output; shift-weight keeps the current source.
                Keep int64 throughout for 16-bit safety.
            */
            const std::int64_t blended_sample = (
                weight * static_cast<std::int64_t>(previous_chroma) +
                (shift - weight) * static_cast<std::int64_t>(current_chroma) +
                shift1
            ) >> shift2;
            const int output_sample = static_cast<int>(blended_sample);

            output_row[x] = output_sample;

            if (!have_output_sample) {
                first_output_sample = output_sample;
                have_output_sample = true;
            }

            last_output_sample = output_sample;
        }
    }

    Cnr3ChromaPlaneProcessSummary resolved_summary{};
    resolved_summary.width = width;
    resolved_summary.height = height;
    resolved_summary.samples_processed = sample_count;
    resolved_summary.first_output_sample = first_output_sample;
    resolved_summary.last_output_sample = last_output_sample;

    summary = resolved_summary;
    return Cnr3Status::ok;
}

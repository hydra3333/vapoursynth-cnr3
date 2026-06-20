#pragma once

/*
    CNR3 frame-processing boundary.

    The settled algorithmic boundary is:

        output[N] depends on source[N] and previous filtered output[N-1].

    The predecessor is the previous filtered OUTPUT frame, not source[N-1].
    That distinction is essential to CNR2/CNR3 recursive temporal behaviour and
    must not be weakened as pixel implementation grows.

    CMS07-P.3A introduces the scalar weighted chroma blend helper. It
    reproduces the definitional vsCnr2 blend arithmetic exactly while preserving
    the deliberate P.2A native parameter-scaling rule: bit-exact CNR2 blend
    arithmetic and response-curve construction, with proportional round-to-nearest
    parameter scaling that is identical to CNR2 at 8-bit and 16-bit and corrects
    CNR2's integer-factor truncation at 10/12/14-bit.

    CMS07-P.4A introduces only the scalar downsampled-luma tap coordinate and
    four-tap average helpers. They reproduce vsCnr2's downSampleLuma shape:
    x0 = chroma_x << subSamplingW, x1 = x0 + 1, y0 = chroma_y << subSamplingH,
    y1 = y0 + subSamplingH, and (a + b + c + d + 2) >> 2. Right/bottom edge
    reads are clamped deliberately so CNR3 does not rely on frame-padding slack.

    CMS07-P.5A composes the scalar signed-difference, response-table lookup,
    and weighted chroma blend decision. It keeps current-minus-previous
    differences signed end-to-end, uses the P.1A total table lookup, and requires
    the P.2A table geometry convention: table_offset = sample_peak and
    table_size = sample_peak * 2 + 1.

    CMS07-P.6A adds bounded chroma-plane traversal over already-prepared scalar
    sample buffers. It walks current source chroma, previous filtered chroma, and
    current/previous downsampled-luma planes with matching dimensions and composes
    the P.5A scalar bridge for each chroma sample. It publishes output only after
    the whole plane has validated and computed successfully. It is still not
    VapourSynth frame access, source-frame request/retrieve lifecycle, explicit
    previous-output frame acquisition, scene-change handling, or cache integration.

    CMS07-P.7A adds source-luma downsample traversal over scalar int buffers.
    It applies the P.4A tap coordinate and four-tap average helpers across a
    whole source luma plane to produce the downsampled-luma plane consumed by
    P.6A. It preserves explicit stride handling, validates the expected chroma
    grid dimensions from the luma dimensions and subsampling factors, and
    publishes output only after the whole plane succeeds. It is still not real
    VapourSynth frame memory, uint8_t/uint16_t byte-stride reinterpretation,
    scene-change handling, or getFrame/cache integration.

    Accuracy upgrades are permitted only where vsCnr2 is accidentally lossy.
    Definitional integer arithmetic is reproduced bit-exactly. P.3A therefore
    keeps shift2 = depth << 1, shift = 1LL << shift2, shift1 = shift >> 1, and
    the int64 accumulator form exactly.

    Still deliberately deferred to later pixel phases:
        - real frame-buffer byte-stride access and uint8_t/uint16_t reinterpretation;
        - scene-change/reset decisions;
        - explicit previous-output frame acquisition and ownership;
        - VapourSynth frame access and getFrame integration.

    This module must not own or inspect cache slots, pins, checkpoints, hot zones,
    prune, recovery state, VapourSynth request lifecycle state, per-instance
    cache authority, diagnostics counters, or summary printers.
*/

#include "cnr3_common.h"

#include <cstdint>
#include <vector>

struct Cnr3DownsampledLumaTapCoordinates {
    int x0 = 0;
    int x1 = 0;
    int y0 = 0;
    int y1 = 0;
};

[[nodiscard]] Cnr3Status cnr3_downsample_luma_tap_coordinates(
    int chroma_x,
    int chroma_y,
    int luma_width,
    int luma_height,
    int sub_sampling_w,
    int sub_sampling_h,
    Cnr3DownsampledLumaTapCoordinates& coordinates
) noexcept;

[[nodiscard]] Cnr3Status cnr3_downsample_luma_sample(
    int top_left_sample,
    int top_right_sample,
    int bottom_left_sample,
    int bottom_right_sample,
    int bits_per_sample,
    int& output_sample
) noexcept;

[[nodiscard]] Cnr3Status cnr3_blend_scale_for_bit_depth(
    int bits_per_sample,
    std::int64_t& blend_scale
) noexcept;

[[nodiscard]] std::int64_t cnr3_calculate_combined_blend_weight(
    int y_response,
    int chroma_response
) noexcept;

[[nodiscard]] Cnr3Status cnr3_blend_chroma_sample(
    int current_source_sample,
    int previous_filtered_sample,
    int y_response,
    int chroma_response,
    int bits_per_sample,
    int& output_sample
) noexcept;

struct Cnr3ChromaBlendSampleResult {
    int luma_signed_diff = 0;
    int chroma_signed_diff = 0;
    int y_response = 0;
    int chroma_response = 0;
    int output_sample = 0;
};

struct Cnr3ConstPlaneBufferView {
    const int* samples = nullptr;
    int width = 0;
    int height = 0;
    int stride = 0;
};

struct Cnr3MutablePlaneBufferView {
    int* samples = nullptr;
    int width = 0;
    int height = 0;
    int stride = 0;
};

struct Cnr3ChromaPlaneProcessSummary {
    int width = 0;
    int height = 0;
    int samples_processed = 0;
    int first_output_sample = 0;
    int last_output_sample = 0;
};

struct Cnr3DownsampledLumaPlaneProcessSummary {
    int source_width = 0;
    int source_height = 0;
    int output_width = 0;
    int output_height = 0;
    int samples_processed = 0;
    int first_output_sample = 0;
    int last_output_sample = 0;
};

[[nodiscard]] Cnr3Status cnr3_blend_chroma_sample_from_response_tables(
    int current_downsampled_luma_sample,
    int previous_downsampled_luma_sample,
    int current_source_chroma_sample,
    int previous_filtered_chroma_sample,
    const std::vector<int>& y_response_table,
    const std::vector<int>& chroma_response_table,
    int table_offset,
    int bits_per_sample,
    Cnr3ChromaBlendSampleResult& result
) noexcept;

[[nodiscard]] Cnr3Status cnr3_downsample_luma_plane_to_chroma_grid(
    const Cnr3ConstPlaneBufferView& source_luma_plane,
    int sub_sampling_w,
    int sub_sampling_h,
    int bits_per_sample,
    Cnr3MutablePlaneBufferView& output_downsampled_luma_plane,
    Cnr3DownsampledLumaPlaneProcessSummary& summary
) noexcept;

[[nodiscard]] Cnr3Status cnr3_process_chroma_plane_from_downsampled_luma(
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
) noexcept;

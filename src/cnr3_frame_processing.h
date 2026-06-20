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
    table_size = sample_peak * 2 + 1. This is still scalar/vector proof only,
    not frame traversal.

    Accuracy upgrades are permitted only where vsCnr2 is accidentally lossy.
    Definitional integer arithmetic is reproduced bit-exactly. P.3A therefore
    keeps shift2 = depth << 1, shift = 1LL << shift2, shift1 = shift >> 1, and
    the int64 accumulator form exactly.

    Still deliberately deferred to later pixel phases:
        - native-subsampling traversal;
        - frame-buffer downSampleLuma traversal;
        - scene-change/reset decisions;
        - explicit previous-output frame processing;
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

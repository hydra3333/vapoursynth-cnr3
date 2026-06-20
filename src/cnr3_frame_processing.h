#pragma once

/*
    CNR3 frame-processing boundary.

    The settled algorithmic boundary is:

        output[N] depends on source[N] and previous filtered output[N-1].

    The predecessor is the previous filtered OUTPUT frame, not source[N-1].
    That distinction is essential to CNR2/CNR3 recursive temporal behaviour and
    must not be weakened as pixel implementation grows.

    CMS07-P.3A introduces only the scalar weighted chroma blend helper. It
    reproduces the definitional vsCnr2 blend arithmetic exactly while preserving
    the deliberate P.2A native parameter-scaling rule: bit-exact CNR2 blend
    arithmetic and response-curve construction, with proportional round-to-nearest
    parameter scaling that is identical to CNR2 at 8-bit and 16-bit and corrects
    CNR2's integer-factor truncation at 10/12/14-bit.

    Accuracy upgrades are permitted only where vsCnr2 is accidentally lossy.
    Definitional integer arithmetic is reproduced bit-exactly. P.3A therefore
    keeps shift2 = depth << 1, shift = 1LL << shift2, shift1 = shift >> 1, and
    the int64 accumulator form exactly.

    Still deliberately deferred to later pixel phases:
        - signed-difference extraction and table lookup from frame pixels;
        - native-subsampling traversal;
        - downSampleLuma;
        - scene-change/reset decisions;
        - explicit previous-output frame processing;
        - VapourSynth frame access and getFrame integration.

    This module must not own or inspect cache slots, pins, checkpoints, hot zones,
    prune, recovery state, VapourSynth request lifecycle state, per-instance
    cache authority, diagnostics counters, or summary printers.
*/

#include "cnr3_common.h"

#include <cstdint>

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

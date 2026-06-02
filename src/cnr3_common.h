#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "VapourSynth4.h"
#include "VSHelper4.h"

#include "cnr3_build_config.h"
#include "cnr3_memory_diagnostics.h"
#include "old_cnr3_strict_cache.h"
#include "cnr3_output_cache_manager.h"

// -----------------------------------------------------------------------------
// Small shared mechanical helpers
// -----------------------------------------------------------------------------

inline int cnr3_clamp_int(
    int value,
    int low,
    int high
) {
    if (value < low) {
        return low;
    }

    if (value > high) {
        return high;
    }

    return value;
}

// -----------------------------------------------------------------------------
// CNR3 per-instance data
//
// A Cnr3Data object is created per VapourSynth CNR3 filter instance. This header
// shares the type definition across translation units; it does not create shared
// runtime state between instances.
// -----------------------------------------------------------------------------

struct Cnr3Data {
    VSNode *node = nullptr;
    const VSVideoInfo *vi = nullptr;

    // Human-readable ID used to distinguish simultaneous CNR3 filter instances.
    int instance_id = 0;

    std::string mode = "oxx";

    /*
        Public threshold parameters are always interpreted in 8-bit Cnr2/vscnr2-compatible units. 
        For clips above 8-bit depth, CNR3 scales these values internally to the actual sample depth.
    */
    int ln = 35;
    int lm = 192;
    int un = 47;
    int um = 255;
    int vn = 47;
    int vm = 255;

    int bits_per_sample = 8;
    int sample_peak = 255;

    int ln_scaled = 35;
    int lm_scaled = 192;
    int un_scaled = 47;
    int um_scaled = 255;
    int vn_scaled = 47;
    int vm_scaled = 255;

    /*
        Lookup tables used by the Cnr2/vscnr2-style weighting logic.

        The public parameters are 8-bit-domain values. These tables are built
        after those values have been scaled to the actual integer bit depth.

        Important mode semantics:
            'x' does not mean disabled.
            'o' does not mean enabled.

            'x' = narrow response curve.
                  More sensitive to current-vs-previous differences.
                  Blending will be reduced sooner as differences increase.
                  This is safer but less aggressive.

            'o' = wide response curve.
                  Less sensitive to current-vs-previous differences.
                  Blending remains allowed across a wider difference range.
                  This gives stronger chroma stabilisation but has more risk
                  of chroma lag, smearing, or ghosting around real motion.

        Why the historical default mode="oxx" can still make sense:
            Y uses the wider response, so luma structure does not block chroma
            stabilisation too eagerly.

            U and V use narrower responses, so actual chroma changes are
            treated more conservatively.

            This can give useful chroma shimmer reduction while reducing the
            risk of dragging old chroma into genuinely changed areas.

        Table index:
            signed sample difference plus table_offset.

            Example:
                signed_diff = current_sample - previous_sample
                table_index = signed_diff + table_offset

        Table value:
            0..sample_peak weighting value.

        This matches the shape needed by the real vscnr2-style blend, where
        Y and chroma table values are multiplied together before blending
        previous filtered chroma with current source chroma.
    */
    int table_offset = 256;
    int table_size = 513;

    std::vector<int> table_y;
    std::vector<int> table_u;
    std::vector<int> table_v;

    double scdthr = 10.0;

    bool scene_chroma = false;

    /*
        vscnr2-style scene-change threshold.

        This is calculated once in cnr3_create() from:
            scdthr
            frame width/height
            bit depth
            chroma subsampling
            scene_chroma

        During frame processing, accumulated frame difference greater than this
        threshold causes CNR3 to output the current source frame unchanged for
        that frame, matching the vscnr2 scene-change behaviour as closely as
        practical in the current API4 structure.
    */
    int64_t scene_change_threshold = 0;

    /*
        blend=true enables the first real vscnr2-style recursive chroma blend.
        blend=false keeps the current pass-through chroma output while still
                    running the diagnostic read/table paths.

        The option will remain for future maintenance/testing, and release behaviour
        will default to Cnr2-style blending enabled.
    */
    bool blend = true;

    /*
        Frame ordering and recursive-state manager.

        Currently this implements only strict streaming Policy A.
        The struct shape deliberately leaves room for the future cache manager.
    */
    OldCnr3StrictStreamCache old_strict_cache;

    /*
        CMS06 output-frame cache manager.

        Current state: store/prune proving is live, but the output cache is not
        yet output-authoritative. The old strict-streaming cache remains the
        source of returned frames until cache-hit reuse and recovery are proven.

        This cache manager must remain per-instance/per-source and must never be
        global or shared between CNR3 instances.
    */
    Cnr3OutputCacheManager output_cache;

    /*
        Per-instance memory diagnostics accumulator.

        This records process/system memory observations for development
        diagnostics. It does not affect frame processing.

        The measurements are process/system level. They do not attempt to split
        memory ownership between VapourSynth and CNR3.
    */
    Cnr3MemoryStats memory_stats;

    bool debug = false;
};

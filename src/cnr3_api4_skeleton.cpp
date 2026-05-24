/*
    CNR3 - experimental VapourSynth API4 chroma stabiliser

    This is the initial API4 skeleton. It intentionally returns the source
    clip unchanged. Its purpose is to prove that the project can build a
    loadable VapourSynth plugin DLL before the recursive CNR3 algorithm is
    connected.

    Diagnostic output rule:
        CNR3 must never write diagnostics to stdout.
        Debug/status messages must go to stderr.
        VapourSynth errors must use mapSetError() or setFilterError().

    SPDX-License-Identifier: AGPL-3.0-or-later
*/

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>
#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <vector>

#include "VapourSynth4.h"
#include "VSHelper4.h"

// -----------------------------------------------------------------------------
//  API policy:
//      CNR3 is an API4-only VapourSynth plugin.
//      Do not include legacy VapourSynth.h / VSHelper.h.
//      Do not use API3-era types or functions.
// -----------------------------------------------------------------------------
#ifndef VAPOURSYNTH_API_VERSION
#error "CNR3 requires VapourSynth API4 headers. VAPOURSYNTH_API_VERSION is not defined."
#endif
#ifndef VS_EXTERNAL_API
#error "CNR3 requires VapourSynth API4-compatible headers. VS_EXTERNAL_API is not defined."
#endif
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Help identify and track instances.
// i.e. which source stream is being processed and thus which cache to use.
// Interlaced sources are usually have fields separated and processed separately
// before re-interlacing or deinterlacing - which means 2 instances of this plugin.
//
static std::atomic<int> g_cnr3_next_instance_id{1};
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// HELPER functions
// -----------------------------------------------------------------------------

static void cnr3_vfprintf_stderr(
    const char *format,
    va_list args
) {
    /*
        CNR3 diagnostic convention:
            - never write plugin diagnostics to stdout
            - debug/status output goes to stderr
            - VapourSynth user-facing errors use mapSetError/setFilterError

        stdout may be used by vspipe for video/data output, so plugin code
        must not write anything there.

        This helper receives an already-started va_list from a printf-style
        wrapper function and writes the formatted message to stderr.
    */
    if (format == nullptr) {
        return;
    }

    std::vfprintf(stderr, format, args);
    std::fflush(stderr);
}

static void cnr3_debug_printf(
    bool debug_enabled,
    const char *format,
    ...
) {
    /*
        This is a small printf-style helper.

        The "..." is the C/C++ varargs syntax. It allows calls such as:

            cnr3_debug_printf(debug, "frame=%d value=%d\n", n, value);

        The named arguments are:
            debug_enabled
            format

        Everything after format is captured by va_start() into args and passed
        to std::vfprintf().

        Use this only for CNR3 diagnostics. Do not use stdout.
    */
    if (!debug_enabled || format == nullptr) {
        return;
    }

    va_list args;
    va_start(args, format);
    cnr3_vfprintf_stderr(format, args);
    va_end(args);
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// CNR3 cache manager
//
// This section is intentionally self-contained so it can later move to:
//     cnr3_cache.h
//     cnr3_cache.cpp
// -----------------------------------------------------------------------------

struct Cnr3CacheManager {
    /*
        Minimal cache/state manager.

        This is intentionally only the strict streaming subset of the future
        cache manager design.

        Invariant:
            prev_output holds a read-only reference to output[next_needed - 1],
            or nullptr before frame 0 has been processed.

        Initial Policy A:
            Only frame n == next_needed is accepted.

        Future Policy C can extend this struct with:
            reorder buffer
            recent output cache
            checkpoint store
            recovery state
            seek mode
    */
    const VSFrame *prev_output = nullptr;
    int next_needed = 0;
};
// -----------------------------------------------------------------------------
// END CNR3 cache manager
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
        Frame ordering and recursive-state manager.

        Currently this implements only strict streaming Policy A.
        The struct shape deliberately leaves room for the future cache manager.
    */
    Cnr3CacheManager cache;

    bool debug = false;
};

static void cnr3_debug_print_cache_state(
    const Cnr3Data *d,
    const char *where,
    int requested_frame
) {
    if (d == nullptr || !d->debug) {
        return;
    }

    const int next_needed = d->cache.next_needed;
    const int gap = requested_frame - next_needed;

    cnr3_debug_printf(
        d->debug,
        "CNR3 debug: instance=%d, %s: requested=%d, next_needed=%d, gap=%d, prev_output=%s\n",
        d->instance_id,
        where,
        requested_frame,
        next_needed,
        gap,
        d->cache.prev_output != nullptr ? "yes" : "no"
    );
}

static int64_t get_optional_int(
    const VSMap *in,
    const VSAPI *vsapi,
    const char *name,
    int64_t default_value
) {
    int err = 0;
    const int64_t value = vsapi->mapGetInt(in, name, 0, &err);
    return err ? default_value : value;
}

static double get_optional_float(
    const VSMap *in,
    const VSAPI *vsapi,
    const char *name,
    double default_value
) {
    int err = 0;
    const double value = vsapi->mapGetFloat(in, name, 0, &err);
    return err ? default_value : value;
}

static std::string get_optional_data_string(
    const VSMap *in,
    const VSAPI *vsapi,
    const char *name,
    const char *default_value
) {
    int err = 0;
    const char *value = vsapi->mapGetData(in, name, 0, &err);
    if (err || value == nullptr) {
        return std::string(default_value);
    }

    return std::string(value);
}

static bool validate_cnr3_format(
    const VSVideoInfo *vi,
    VSMap *out,
    const VSAPI *vsapi
) {
    if (vi == nullptr) {
        vsapi->mapSetError(out, "CNR3: internal error: video info is null.");
        return false;
    }

    /*
        API4 note:
        Do not rely on helper functions such as isConstantVideoFormat()
        being available in every vendored header set. Check the fields
        directly instead.

        In VapourSynth, variable/unknown format clips have cfUndefined.
        Variable/unknown dimensions are represented by non-positive
        width/height.
    */
    if (vi->format.colorFamily == cfUndefined) {
        vsapi->mapSetError(out, "CNR3: only constant-format video clips are supported.");
        return false;
    }

    if (vi->width <= 0 || vi->height <= 0) {
        vsapi->mapSetError(out, "CNR3: only constant-dimension video clips are supported.");
        return false;
    }

    if (vi->format.colorFamily != cfYUV) {
        vsapi->mapSetError(out, "CNR3: only YUV clips are supported.");
        return false;
    }

    if (vi->format.sampleType != stInteger) {
        vsapi->mapSetError(out, "CNR3: only integer sample clips are supported.");
        return false;
    }

    if (vi->format.bitsPerSample < 8 || vi->format.bitsPerSample > 16) {
        vsapi->mapSetError(out, "CNR3: only 8-bit to 16-bit integer clips are supported.");
        return false;
    }

    if (vi->format.numPlanes != 3) {
        vsapi->mapSetError(out, "CNR3: only 3-plane YUV clips are supported.");
        return false;
    }

    if (vi->format.subSamplingW < 0 || vi->format.subSamplingW > 1) {
        vsapi->mapSetError(out, "CNR3: unsupported horizontal chroma subsampling.");
        return false;
    }

    if (vi->format.subSamplingH < 0 || vi->format.subSamplingH > 1) {
        vsapi->mapSetError(out, "CNR3: unsupported vertical chroma subsampling.");
        return false;
    }

    return true;
}

static int scale_8bit_parameter_to_bit_depth(
    int value_8bit,
    int bits_per_sample
) {
    /*
        Public CNR3 threshold parameters use the historical 8-bit Cnr2/vscnr2
        scale. Internally, integer clips above 8-bit use proportionally scaled
        thresholds.

        Examples:
            8-bit:   35 -> 35
            10-bit:  35 -> approximately 140
            16-bit:  35 -> approximately 8995
    */
    const int peak = (1 << bits_per_sample) - 1;

    return static_cast<int>(
        (static_cast<int64_t>(value_8bit) * peak + 127) / 255
    );
}

static int clamp_int(
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

static int get_cnr3_table_value_for_signed_diff(
    const std::vector<int> &table,
    int table_offset,
    int signed_diff
) {
    /*
        Safe table lookup helper.

        The real blend path will eventually use current-vs-previous signed
        sample differences:

            signed_diff = current_sample - previous_sample

        The table is stored with a positive offset so signed differences can be
        used directly after adding table_offset.
    */
    const int index = signed_diff + table_offset;

    if (index < 0 || index >= static_cast<int>(table.size())) {
        return 0;
    }

    return table[static_cast<size_t>(index)];
}

static void build_cnr3_weight_table(
    std::vector<int> &table,
    int table_offset,
    int table_size,
    int sample_peak,
    int threshold,
    int strength,
    bool wide_response
) {
    /*
        Build one vscnr2-style signed-difference response table.

        This replaces the earlier temporary enabled/disabled scaffold.

        Important:
            mode character 'x' means narrow response, not disabled.
            mode character 'o' means wide response, not enabled.

        A response value near strength means:
            the current-vs-previous difference is small enough that the later
            recursive chroma blend may strongly reuse previous filtered chroma.

        A response value near zero means:
            the difference is large enough that the later recursive chroma
            blend should mostly or entirely keep current source chroma.

        Narrow response:
            The table falls away more quickly as abs(diff) increases.
            This is safer and less aggressive.

        Wide response:
            The table stays higher for longer as abs(diff) increases.
            This is stronger and more tolerant of chroma shimmer, but has more
            risk of chroma lag, smearing, or ghosting around real motion.

        Why default mode="oxx" can still make sense:
            - Y uses wide response so luma structure does not block chroma
              stabilisation too eagerly.
            - U and V use narrow response so actual chroma changes are handled
              more conservatively.
            - This matches the historical default while still making all three
              planes participate in the later blend decision.

        Table storage:
            table[signed_diff + table_offset]

        Table value range:
            0..sample_peak

        The table is signed because the vscnr2-style formula uses signed
        current-vs-previous differences when indexing the Y/U/V response
        tables. For cosine response curves the result is symmetric, but keeping
        signed indexing avoids a later structural change when the real blend is
        connected.
    */

    table.assign(static_cast<size_t>(table_size), 0);

    threshold = clamp_int(threshold, 0, sample_peak);
    strength = clamp_int(strength, 0, sample_peak);

    if (threshold == 0) {
        table[static_cast<size_t>(table_offset)] = strength;
        return;
    }

    constexpr double pi = 3.141592653589793238462643383279502884;

    const int first_diff = -threshold;
    const int last_diff = threshold;

    for (int signed_diff = first_diff; signed_diff <= last_diff; ++signed_diff) {
        const int index = signed_diff + table_offset;

        if (index < 0 || index >= table_size) {
            continue;
        }

        const int abs_diff = std::abs(signed_diff);

        double angle = 0.0;

        if (wide_response) {
            /*
                Wide response.

                Squaring abs_diff keeps the curve higher for longer near zero,
                then it falls toward zero near the threshold.
            */
            angle =
                static_cast<double>(abs_diff) *
                static_cast<double>(abs_diff) *
                pi /
                (
                    static_cast<double>(threshold) *
                    static_cast<double>(threshold)
                );
        } else {
            /*
                Narrow response.

                Linear abs_diff makes the curve fall away sooner.
            */
            angle =
                static_cast<double>(abs_diff) *
                pi /
                static_cast<double>(threshold);
        }

        /*
            Use integer division by 2 before applying the cosine response.
            This intentionally follows the vscnr2-style table shape closely,
            including the fact that an odd maximum strength such as 255 gives
            a peak table value of 254 rather than 255.
        */
        const double half_strength =
            static_cast<double>(strength / 2);

        const int value = clamp_int(
            static_cast<int>(half_strength * (1.0 + std::cos(angle))),
            0,
            sample_peak
        );

        table[static_cast<size_t>(index)] = value;
    }
}

static bool build_cnr3_lookup_tables(
    Cnr3Data &d,
    VSMap *out,
    const VSAPI *vsapi
) {
    /*
        mode is a 3-character string:
            mode[0] controls the luma/Y response curve
            mode[1] controls the U/chroma response curve
            mode[2] controls the V/chroma response curve

        Historical vscnr2/Cnr2-compatible meaning:
            'x' = narrow response curve
            'o' = wide response curve

        Very important:
            'x' does not mean off.
            'o' does not mean on.

        All three planes still get tables. The mode character only changes the
        curve shape used to reduce the later blend weight as
        current-vs-previous differences increase.
    */

    if (d.mode.size() != 3) {
        vsapi->mapSetError(out, "CNR3: internal error: mode must contain exactly 3 characters.");
        return false;
    }

    const bool wide_y = (d.mode[0] != 'x');
    const bool wide_u = (d.mode[1] != 'x');
    const bool wide_v = (d.mode[2] != 'x');

    build_cnr3_weight_table(
        d.table_y,
        d.table_offset,
        d.table_size,
        d.sample_peak,
        d.ln_scaled,
        d.lm_scaled,
        wide_y
    );

    build_cnr3_weight_table(
        d.table_u,
        d.table_offset,
        d.table_size,
        d.sample_peak,
        d.un_scaled,
        d.um_scaled,
        wide_u
    );

    build_cnr3_weight_table(
        d.table_v,
        d.table_offset,
        d.table_size,
        d.sample_peak,
        d.vn_scaled,
        d.vm_scaled,
        wide_v
    );

    if (
        d.table_y.size() != static_cast<size_t>(d.table_size) ||
        d.table_u.size() != static_cast<size_t>(d.table_size) ||
        d.table_v.size() != static_cast<size_t>(d.table_size)
    ) {
        vsapi->mapSetError(out, "CNR3: internal error: lookup-table size mismatch.");
        return false;
    }

    return true;
}

static void copy_plane_bytes(
    const VSFrame *src,
    VSFrame *dst,
    int plane,
    int bytes_per_sample,
    const VSAPI *vsapi
) {
    const uint8_t *srcp = vsapi->getReadPtr(src, plane);
    uint8_t *dstp = vsapi->getWritePtr(dst, plane);

    const int src_stride = vsapi->getStride(src, plane);
    const int dst_stride = vsapi->getStride(dst, plane);

    const int plane_width = vsapi->getFrameWidth(src, plane);
    const int plane_height = vsapi->getFrameHeight(src, plane);

    const size_t row_bytes =
        static_cast<size_t>(plane_width) *
        static_cast<size_t>(bytes_per_sample);

    for (int y = 0; y < plane_height; ++y) {
        std::memcpy(dstp, srcp, row_bytes);

        srcp += src_stride;
        dstp += dst_stride;
    }
}

static void copy_all_planes_unchanged(
    const VSFrame *src,
    VSFrame *dst,
    int bytes_per_sample,
    const VSAPI *vsapi
) {
    copy_plane_bytes(src, dst, 0, bytes_per_sample, vsapi);
    copy_plane_bytes(src, dst, 1, bytes_per_sample, vsapi);
    copy_plane_bytes(src, dst, 2, bytes_per_sample, vsapi);
}

static void process_cnr3_chroma_plane_passthrough_u8(
    const VSFrame *src,
    VSFrame *dst,
    int plane,
    const VSAPI *vsapi
) {
    /*
        Temporary per-sample chroma loop for 8-bit integer clips.

        This is deliberately still pass-through:
            dst[x] = src[x]

        Purpose:
            prove the per-pixel addressing, width, height, and stride logic
            before connecting the recursive CNR3 blend.

        Do not optimise this back into memcpy while algorithm development is
        in progress. The next stages need this loop body as the stable place to
        read current chroma, read previous filtered chroma, compute guard
        weights, and write stabilised chroma.
    */
    const uint8_t *srcp = vsapi->getReadPtr(src, plane);
    uint8_t *dstp = vsapi->getWritePtr(dst, plane);

    const int src_stride = vsapi->getStride(src, plane);
    const int dst_stride = vsapi->getStride(dst, plane);

    const int plane_width = vsapi->getFrameWidth(src, plane);
    const int plane_height = vsapi->getFrameHeight(src, plane);

    for (int y = 0; y < plane_height; ++y) {
        const uint8_t *src_row = srcp;
        uint8_t *dst_row = dstp;

        for (int x = 0; x < plane_width; ++x) {
            const uint8_t current_chroma = src_row[x];

            dst_row[x] = current_chroma;
        }

        srcp += src_stride;
        dstp += dst_stride;
    }
}

static void process_cnr3_chroma_plane_passthrough_u16(
    const VSFrame *src,
    VSFrame *dst,
    int plane,
    const VSAPI *vsapi
) {
    /*
        Temporary per-sample chroma loop for 10/12/16-bit integer clips.

        VapourSynth stores integer formats above 8-bit in 16-bit samples, so
        this path handles all currently accepted high-bit-depth inputs.

        This is deliberately still pass-through:
            dst[x] = src[x]

        Keep this separate from the 8-bit loop for now. The real blend will
        need different sample types and arithmetic widths, and separate loops
        make that easier to review and test.
    */
    const uint8_t *srcp = vsapi->getReadPtr(src, plane);
    uint8_t *dstp = vsapi->getWritePtr(dst, plane);

    const int src_stride = vsapi->getStride(src, plane);
    const int dst_stride = vsapi->getStride(dst, plane);

    const int plane_width = vsapi->getFrameWidth(src, plane);
    const int plane_height = vsapi->getFrameHeight(src, plane);

    for (int y = 0; y < plane_height; ++y) {
        const uint16_t *src_row =
            reinterpret_cast<const uint16_t *>(srcp);

        uint16_t *dst_row =
            reinterpret_cast<uint16_t *>(dstp);

        for (int x = 0; x < plane_width; ++x) {
            const uint16_t current_chroma = src_row[x];

            dst_row[x] = current_chroma;
        }

        srcp += src_stride;
        dstp += dst_stride;
    }
}

static bool process_cnr3_chroma_plane(
    const Cnr3Data *d,
    int frame_number,
    int plane,
    const VSFrame *src,
    const VSFrame *prev_output,
    VSFrame *dst,
    const VSAPI *vsapi
) {
    /*
        Chroma-plane processing function.

        Current behaviour:
            copy the requested chroma plane from the current source frame.

        Purpose:
            this is now the stable call site for the real recursive CNR3 chroma
            stabilisation algorithm.

        Important note:
            Do not simply copy chroma from prev_output as a proof test. Since
            prev_output is the previous filtered output, copying U/V from it
            recursively causes chroma to remain effectively stuck at frame 0:

                output[1].UV = output[0].UV
                output[2].UV = output[1].UV
                output[3].UV = output[2].UV

            That test proved prev_output was readable, but it badly distorted
            colour and was reverted.

        Intended future behaviour:
            for each chroma sample in source frame N:
                read current source chroma from source[N]
                read previous filtered chroma from output[N - 1]
                use Y/U/V guard tables to decide blend strength
                write stabilised chroma to output[N]

        Plane convention:
            plane 1 = U
            plane 2 = V
    */
    if (d == nullptr || src == nullptr || dst == nullptr || vsapi == nullptr) {
        return false;
    }

    if (plane != 1 && plane != 2) {
        return false;
    }

    /*
        Keep this validation even though the current pass-through path does not
        read prev_output. The real recursive algorithm will need it, and the
        frame-level function has already established this as part of the
        recursive precondition.
    */
    if (frame_number > 0 && prev_output == nullptr) {
        return false;
    }

    const int bytes_per_sample = (d->bits_per_sample + 7) / 8;

    /*
        Scaffold stage only.

        At this point, CNR3 intentionally still writes pass-through chroma.
        The purpose of using per-sample loops now is to prove the exact loop
        structure that the real recursive blend will later use.

        Future algorithm notes:
            - frame 0 will initialise from current source chroma
            - frame N > 0 will read prev_output chroma from output[N - 1]
            - Y/U/V guard tables will decide how much previous filtered chroma
              may be reused
            - mode characters 'x' and 'o' must be treated as narrow and wide
              guard response curves, not as disabled and enabled switches

        Why the historical default mode="oxx" can still make sense:
            - Y uses the wider response, so luma structure does not block
              chroma stabilisation too eagerly
            - U and V use narrower responses, so chroma changes are treated
              more conservatively
            - this can give useful chroma shimmer reduction while reducing the
              risk of chroma lag, smearing, or ghosting around real motion

        The lookup-table builder now uses the historical narrow/wide response
        meaning of mode characters. Real chroma blending is still not connected
        in this scaffold step.
    */
    if (bytes_per_sample == 1) {
        process_cnr3_chroma_plane_passthrough_u8(
            src,
            dst,
            plane,
            vsapi
        );

        return true;
    }

    if (bytes_per_sample == 2) {
        process_cnr3_chroma_plane_passthrough_u16(
            src,
            dst,
            plane,
            vsapi
        );

        return true;
    }

    return false;
}

static bool process_cnr3_frame_passthrough_for_now(
    const Cnr3Data *d,
    int frame_number,
    const VSFrame *src,
    VSFrame *dst,
    VSFrameContext *frameCtx,
    const VSAPI *vsapi
) {
    /*
        Temporary frame-processing function.

        Current behaviour:
            copy Y/U/V unchanged.

        Purpose:
            establish the stable structure where the real recursive CNR3
            chroma processing will be inserted.

        Processing structure:
            Y:
                copied unchanged, because CNR3 is a chroma stabiliser.

            U/V:
                routed through separate chroma-plane processing calls.

        Recursive precondition:
            frame 0 does not need a previous output frame.

            frame N > 0 must have d->cache.prev_output available, because the
            real algorithm will use output[N - 1] when producing output[N].
    */
    if (d == nullptr) {
        vsapi->setFilterError("CNR3: internal error: filter data is null.", frameCtx);
        return false;
    }

    if (src == nullptr || dst == nullptr) {
        vsapi->setFilterError("CNR3: internal error: source or destination frame is null.", frameCtx);
        return false;
    }

    if (frame_number < 0) {
        vsapi->setFilterError("CNR3: internal error: negative frame number.", frameCtx);
        return false;
    }

    const VSFrame *prev_output = d->cache.prev_output;

    if (frame_number == 0) {
        cnr3_debug_printf(
            d->debug,
            "CNR3 debug: instance=%d, processing frame 0 using initial-copy path.\n",
            d->instance_id
        );
    } else {
        if (prev_output == nullptr) {
            cnr3_debug_printf(
                d->debug,
                "CNR3 debug: instance=%d, missing previous output for recursive frame=%d.\n",
                d->instance_id,
                frame_number
            );

            vsapi->setFilterError(
                "CNR3: internal error: previous output frame is missing for recursive processing.",
                frameCtx
            );
            return false;
        }

        cnr3_debug_printf(
            d->debug,
            "CNR3 debug: instance=%d, processing frame=%d using recursive previous-output path.\n",
            d->instance_id,
            frame_number
        );
    }

    const int bytes_per_sample = (d->bits_per_sample + 7) / 8;

    /*
        CNR3 is chroma-focused. Luma is copied unchanged.
    */
    copy_plane_bytes(
        src,
        dst,
        0,
        bytes_per_sample,
        vsapi
    );

    if (!process_cnr3_chroma_plane(
        d,
        frame_number,
        1,
        src,
        prev_output,
        dst,
        vsapi
    )) {
        vsapi->setFilterError("CNR3: internal error while processing U plane.", frameCtx);
        return false;
    }

    if (!process_cnr3_chroma_plane(
        d,
        frame_number,
        2,
        src,
        prev_output,
        dst,
        vsapi
    )) {
        vsapi->setFilterError("CNR3: internal error while processing V plane.", frameCtx);
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------
// CNR3 cache manager
// -----------------------------------------------------------------------------
static void cnr3_cache_clear(
    Cnr3CacheManager &cache,
    const VSAPI *vsapi
) {
    if (cache.prev_output != nullptr) {
        vsapi->freeFrame(cache.prev_output);
        cache.prev_output = nullptr;
    }

    cache.next_needed = 0;
}

static void VS_CC cnr3_free(
    void *instanceData,
    VSCore *core,
    const VSAPI *vsapi
) {
    (void)core;

    Cnr3Data *d = static_cast<Cnr3Data *>(instanceData);

    if (d != nullptr) {
        if (d->node != nullptr) {
            vsapi->freeNode(d->node);
            d->node = nullptr;
        }
        
        cnr3_cache_clear(d->cache, vsapi);
        
        delete d;
    }
}

static void cnr3_cache_store_output_frame(
    Cnr3CacheManager &cache,
    const VSFrame *output_frame,
    int frame_number,
    const VSAPI *vsapi
) {
    if (cache.prev_output != nullptr) {
        vsapi->freeFrame(cache.prev_output);
        cache.prev_output = nullptr;
    }

    /*
        addFrameRef() keeps an additional reference to the frame. It does not
        deep-copy pixel data. That is fine here because VapourSynth frames are
        immutable after being returned.

        The stored reference must later be released with freeFrame().
    */
    cache.prev_output = vsapi->addFrameRef(output_frame);

    /*
        In strict streaming mode, after output frame N has been produced,
        the next frame we can correctly accept is N + 1.
    */
    cache.next_needed = frame_number + 1;
}

static const VSFrame *VS_CC cnr3_get_frame(
    int n,
    int activationReason,
    void *instanceData,
    void **frameData,
    VSFrameContext *frameCtx,
    VSCore *core,
    const VSAPI *vsapi
) {
    (void)frameData;

    Cnr3Data *d = static_cast<Cnr3Data *>(instanceData);

    if (activationReason == arInitial) {
        /*
            Normally too noisy for routine debugging.

            Enable temporarily only when investigating VapourSynth scheduling
            or unexpected frame request order.
        */
        /*
        cnr3_debug_print_cache_state(
            d,
            "arInitial/request source frame",
            n
        );
        */

        vsapi->requestFrameFilter(n, d->node, frameCtx);
        return nullptr;
    }

    if (activationReason == arAllFramesReady) {
        /*
            Normally too noisy for routine debugging.

            Enable temporarily only when investigating VapourSynth scheduling
            or unexpected frame request order.
        */
        /*
        cnr3_debug_print_cache_state(
            d,
            "arAllFramesReady/entry",
            n
        );
        */

        const VSFrame *src = vsapi->getFrameFilter(n, d->node, frameCtx);

        if (src == nullptr) {
            vsapi->setFilterError("CNR3: failed to retrieve source frame.", frameCtx);
            return nullptr;
        }

        /*
            Initial recursive Policy A.

            The real recursive algorithm uses d->cache.prev_output when producing frame n.

            Later, this can be replaced by a seek-safe Policy C using recomputation
            or checkpoints.
        */

        if (n != d->cache.next_needed) {
            const int requested_frame = n;
            const int next_needed = d->cache.next_needed;
            const int gap = requested_frame - next_needed;

            cnr3_debug_printf(
                d->debug,
                "CNR3 debug: instance=%d, out-of-order frame request: requested=%d, next_needed=%d, gap=%d, prev_output=%s\n",
                d->instance_id,
                requested_frame,
                next_needed,
                gap,
                d->cache.prev_output != nullptr ? "yes" : "no"
            );

            char error_message[384];

            std::snprintf(
                error_message,
                sizeof(error_message),
                "CNR3: recursive streaming mode currently requires strictly increasing frame requests. "
                "instance=%d, requested=%d, next_needed=%d, gap=%d, prev_output=%s.",
                d->instance_id,
                requested_frame,
                next_needed,
                gap,
                d->cache.prev_output != nullptr ? "yes" : "no"
            );

            vsapi->freeFrame(src);
            vsapi->setFilterError(error_message, frameCtx);
            return nullptr;
        }

        /*
            Normally too noisy for routine debugging.

            The out-of-order debug above is more useful because it captures
            the failure condition directly.
        */
        /*
        cnr3_debug_printf(
            d->debug,
            "CNR3 debug: instance=%d, in-order frame accepted: requested=%d, next_needed=%d, prev_output=%s\n",
            d->instance_id,
            n,
            d->cache.next_needed,
            d->cache.prev_output != nullptr ? "yes" : "no"
        );
        */

        VSFrame *dst = vsapi->newVideoFrame(
            &d->vi->format,
            d->vi->width,
            d->vi->height,
            src,
            core
        );

        if (dst == nullptr) {
            vsapi->freeFrame(src);
            vsapi->setFilterError("CNR3: failed to allocate destination frame.", frameCtx);
            return nullptr;
        }

        if (!process_cnr3_frame_passthrough_for_now(
            d,
            n,
            src,
            dst,
            frameCtx,
            vsapi
        )) {
            vsapi->freeFrame(src);
            vsapi->freeFrame(dst);
            return nullptr;
        }

        cnr3_cache_store_output_frame(
            d->cache,
            dst,
            n,
            vsapi
        );

        cnr3_debug_printf(
            d->debug,
            "CNR3 debug: instance=%d, processed frame: frame=%d, new_next_needed=%d, stored_prev_output=%s\n",
            d->instance_id,
            n,
            d->cache.next_needed,
            d->cache.prev_output != nullptr ? "yes" : "no"
        );
        
        vsapi->freeFrame(src);

        return dst;
    }
    return nullptr;
}
// -----------------------------------------------------------------------------
// END CNR3 cache manager
// -----------------------------------------------------------------------------

static void VS_CC cnr3_create(
    const VSMap *in,
    VSMap *out,
    void *userData,
    VSCore *core,
    const VSAPI *vsapi
) {
    (void)userData;

    Cnr3Data local;

    // an ID to identify and track instances
    local.instance_id = g_cnr3_next_instance_id.fetch_add(1);

    int err = 0;
    local.node = vsapi->mapGetNode(in, "clip", 0, &err);

    if (err || local.node == nullptr) {
        vsapi->mapSetError(out, "CNR3: clip is required.");
        return;
    }

    local.vi = vsapi->getVideoInfo(local.node);

    if (local.vi == nullptr) {
        vsapi->freeNode(local.node);
        vsapi->mapSetError(out, "CNR3: failed to get video info.");
        return;
    }

    if (!validate_cnr3_format(local.vi, out, vsapi)) {
        vsapi->freeNode(local.node);
        return;
    }

    local.mode = get_optional_data_string(in, vsapi, "mode", "oxx");

    local.ln = static_cast<int>(get_optional_int(in, vsapi, "ln", 35));
    local.lm = static_cast<int>(get_optional_int(in, vsapi, "lm", 192));
    local.un = static_cast<int>(get_optional_int(in, vsapi, "un", 47));
    local.um = static_cast<int>(get_optional_int(in, vsapi, "um", 255));
    local.vn = static_cast<int>(get_optional_int(in, vsapi, "vn", 47));
    local.vm = static_cast<int>(get_optional_int(in, vsapi, "vm", 255));

    local.scdthr = get_optional_float(in, vsapi, "scdthr", 10.0);

    local.scene_chroma = get_optional_int(in, vsapi, "scene_chroma", 0) != 0;
    local.debug = get_optional_int(in, vsapi, "debug", 0) != 0;

    if (local.mode.size() != 3) {
        vsapi->freeNode(local.node);
        vsapi->mapSetError(out, "CNR3: mode must be a 3-character string, for example \"oxx\".");
        return;
    }

    for (const char c : local.mode) {
        if (c != 'o' && c != 'x') {
            vsapi->freeNode(local.node);
            vsapi->mapSetError(out, "CNR3: mode may contain only 'o' and 'x' characters.");
            return;
        }
    }

    if (
        local.ln < 0 ||
        local.lm < 0 ||
        local.un < 0 ||
        local.um < 0 ||
        local.vn < 0 ||
        local.vm < 0
    ) {
        vsapi->freeNode(local.node);
        vsapi->mapSetError(out, "CNR3: threshold parameters must be non-negative.");
        return;
    }

    if (local.scdthr < 0.0) {
        vsapi->freeNode(local.node);
        vsapi->mapSetError(out, "CNR3: scdthr must be non-negative.");
        return;
    }
   
    local.bits_per_sample = local.vi->format.bitsPerSample;
    local.sample_peak = (1 << local.bits_per_sample) - 1;

    /*
        Signed-difference table geometry.

        sample_peak:
            maximum legal sample value, for example 255 or 65535.

        table_offset:
            one greater than sample_peak, used to map signed differences into
            positive vector indexes.

        table_size:
            enough entries for all possible signed differences from
            -sample_peak through +sample_peak, plus the offset slot.

        Example for 8-bit:
            sample_peak  = 255
            table_offset = 256
            table_size   = 513
    */
    local.table_offset = local.sample_peak + 1;
    local.table_size = local.table_offset * 2 + 1;

    local.ln_scaled = scale_8bit_parameter_to_bit_depth(local.ln, local.bits_per_sample);
    local.lm_scaled = scale_8bit_parameter_to_bit_depth(local.lm, local.bits_per_sample);
    local.un_scaled = scale_8bit_parameter_to_bit_depth(local.un, local.bits_per_sample);
    local.um_scaled = scale_8bit_parameter_to_bit_depth(local.um, local.bits_per_sample);
    local.vn_scaled = scale_8bit_parameter_to_bit_depth(local.vn, local.bits_per_sample);
    local.vm_scaled = scale_8bit_parameter_to_bit_depth(local.vm, local.bits_per_sample);

    if (!build_cnr3_lookup_tables(local, out, vsapi)) {
        vsapi->freeNode(local.node);
        return;
    }

    if (local.debug) {
        const int y_mid = local.ln_scaled / 2;
        const int u_mid = local.un_scaled / 2;
        const int v_mid = local.vn_scaled / 2;

        cnr3_debug_printf(
            local.debug,
            "CNR3 debug: instance=%d, format=%d-bit YUV, peak=%d, "
            "table_offset=%d, table_size=%d, "
            "ln=%d->%d, lm=%d->%d, "
            "un=%d->%d, um=%d->%d, "
            "vn=%d->%d, vm=%d->%d, "
            "mode=%s, scdthr=%.6f, scene_chroma=%d\n",
            local.instance_id,
            local.bits_per_sample,
            local.sample_peak,
            local.table_offset,
            local.table_size,
            local.ln,
            local.ln_scaled,
            local.lm,
            local.lm_scaled,
            local.un,
            local.un_scaled,
            local.um,
            local.um_scaled,
            local.vn,
            local.vn_scaled,
            local.vm,
            local.vm_scaled,
            local.mode.c_str(),
            local.scdthr,
            local.scene_chroma ? 1 : 0
        );

        cnr3_debug_printf(
            local.debug,
            "CNR3 debug: instance=%d, table samples by signed diff: "
            "Y[0]=%d, Y[%d]=%d, Y[%d]=%d, Y[%d]=%d; "
            "U[0]=%d, U[%d]=%d, U[%d]=%d, U[%d]=%d; "
            "V[0]=%d, V[%d]=%d, V[%d]=%d, V[%d]=%d\n",
            local.instance_id,

            get_cnr3_table_value_for_signed_diff(
                local.table_y,
                local.table_offset,
                0
            ),
            y_mid,
            get_cnr3_table_value_for_signed_diff(
                local.table_y,
                local.table_offset,
                y_mid
            ),
            local.ln_scaled,
            get_cnr3_table_value_for_signed_diff(
                local.table_y,
                local.table_offset,
                local.ln_scaled
            ),
            local.sample_peak,
            get_cnr3_table_value_for_signed_diff(
                local.table_y,
                local.table_offset,
                local.sample_peak
            ),

            get_cnr3_table_value_for_signed_diff(
                local.table_u,
                local.table_offset,
                0
            ),
            u_mid,
            get_cnr3_table_value_for_signed_diff(
                local.table_u,
                local.table_offset,
                u_mid
            ),
            local.un_scaled,
            get_cnr3_table_value_for_signed_diff(
                local.table_u,
                local.table_offset,
                local.un_scaled
            ),
            local.sample_peak,
            get_cnr3_table_value_for_signed_diff(
                local.table_u,
                local.table_offset,
                local.sample_peak
            ),

            get_cnr3_table_value_for_signed_diff(
                local.table_v,
                local.table_offset,
                0
            ),
            v_mid,
            get_cnr3_table_value_for_signed_diff(
                local.table_v,
                local.table_offset,
                v_mid
            ),
            local.vn_scaled,
            get_cnr3_table_value_for_signed_diff(
                local.table_v,
                local.table_offset,
                local.vn_scaled
            ),
            local.sample_peak,
            get_cnr3_table_value_for_signed_diff(
                local.table_v,
                local.table_offset,
                local.sample_peak
            )
        );
    }
    Cnr3Data *data = new Cnr3Data(local);

    VSFilterDependency deps[] = {
        {data->node, rpGeneral}
    };

    vsapi->createVideoFilter(
        out,
        "CNR3",
        data->vi,
        cnr3_get_frame,
        cnr3_free,
        fmUnordered,
        deps,
        1,
        data,
        core
    );
}

VS_EXTERNAL_API(void) VapourSynthPluginInit2(
    VSPlugin *plugin,
    const VSPLUGINAPI *vspapi
) {
    vspapi->configPlugin(
        "com.walshdcw.cnr3",
        "cnr3",
        "CNR3 experimental recursive chroma stabiliser",
        VS_MAKE_VERSION(0, 1),
        VAPOURSYNTH_API_VERSION,
        0,
        plugin
    );

    vspapi->registerFunction(
        "CNR3",
        "clip:vnode;"
        "mode:data:opt;"
        "ln:int:opt;"
        "lm:int:opt;"
        "un:int:opt;"
        "um:int:opt;"
        "vn:int:opt;"
        "vm:int:opt;"
        "scdthr:float:opt;"
        "scene_chroma:int:opt;"
        "debug:int:opt;",
        "clip:vnode;",
        cnr3_create,
        nullptr,
        plugin
    );
}

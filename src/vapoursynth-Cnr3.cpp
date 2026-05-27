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
#include <cstddef>
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

        The real blend path uses current-vs-previous signed sample differences:

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

static int64_t get_cnr3_blend_scale(
    int bits_per_sample
) {
    /*
        Return the denominator used by the vscnr2-style blend formula.

        For 8-bit:
            1 << 16 = 65536

        For 16-bit:
            1 << 32 = 4294967296

        CNR3 currently accepts 8..16-bit integer clips, so int64_t is safely
        large enough for this scale and for intermediate multiply/add work.
    */
    const int shift2 = bits_per_sample << 1;

    return static_cast<int64_t>(1) << shift2;
}

static int64_t calculate_cnr3_combined_blend_weight(
    int y_response,
    int chroma_response
) {
    /*
        vscnr2-style combined blend weight.

        A high weight reuses more previous filtered chroma.
        A low weight keeps more current source chroma.
    */
    return
        static_cast<int64_t>(y_response) *
        static_cast<int64_t>(chroma_response);
}

static int64_t calculate_cnr3_max_possible_blend_weight(
    const Cnr3Data* d,
    const std::vector<int>& chroma_table
) {
    /*
        Maximum possible blend weight for this plane and current table setup.

        This is normally:

            table_y[zero_diff] * table_u_or_v[zero_diff]

        It may be below the mathematical full scale. With 8-bit defaults:

            table_y[0] = 192
            table_u/v[0] = 254
            max_possible_weight = 48768

            48768 / 65536 = about 74.41%

        This value is used only for diagnostics.
    */
    if (d == nullptr) {
        return 0;
    }

    const int y_zero_response = get_cnr3_table_value_for_signed_diff(
        d->table_y,
        d->table_offset,
        0
    );

    const int chroma_zero_response = get_cnr3_table_value_for_signed_diff(
        chroma_table,
        d->table_offset,
        0
    );

    return calculate_cnr3_combined_blend_weight(
        y_zero_response,
        chroma_zero_response
    );
}

static int blend_cnr3_chroma_sample(
    int current_sample,
    int previous_filtered_sample,
    int y_response,
    int chroma_response,
    int bits_per_sample
) {
    /*
        First real vscnr2-style recursive chroma blend.

        vscnr2-style formula:

            weight = table_y[diff_y + offset] * table_uv[diff_uv + offset]

            dst = (
                    weight * previous_filtered_chroma
                    + (shift - weight) * current_source_chroma
                    + shift1
                  ) >> shift2

        where:
            shift2 = bits_per_sample * 2
            shift  = 1 << shift2
            shift1 = shift / 2
    */
    const int shift2 = bits_per_sample << 1;
    const int64_t shift = get_cnr3_blend_scale(bits_per_sample);
    const int64_t shift1 = shift >> 1;

    const int64_t weight = calculate_cnr3_combined_blend_weight(
        y_response,
        chroma_response
    );

    const int64_t blended =
        (
            weight * static_cast<int64_t>(previous_filtered_sample) +
            (shift - weight) * static_cast<int64_t>(current_sample) +
            shift1
        ) >> shift2;

    return static_cast<int>(blended);
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

    const ptrdiff_t src_stride = vsapi->getStride(src, plane);
    const ptrdiff_t dst_stride = vsapi->getStride(dst, plane);

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

static void build_cnr3_downsampled_luma_buffer_u8(
    const Cnr3Data *d,
    const VSFrame *frame,
    int chroma_width,
    int chroma_height,
    std::vector<int> &luma_buffer,
    const VSAPI *vsapi
) {
    /*
        Build a luma buffer at chroma resolution for 8-bit clips.

        vscnr2 downsamples luma before using it as the Y guard for chroma
        processing. It does not use only the top-left corresponding luma
        sample. Its downsample shape is effectively:

            dst[x, y] = (Y[x0, y0] + Y[x0 + 1, y0]
                       + Y[x0, y1] + Y[x0 + 1, y1] + 2) >> 2

        where:
            x0 = chroma_x << subSamplingW
            y0 = chroma_y << subSamplingH
            y1 = y0 + subSamplingH

        For 4:2:0 this is the expected 2x2 luma average.
        For 4:2:2 it becomes a two-sample horizontal average, counted twice,
        matching the vscnr2 structure.
        For 4:4:4 and 4:4:0, x0 + 1 can reach the right edge. CNR3 clamps
        that edge read instead of relying on padding or reading past the row.
    */
    luma_buffer.assign(
        static_cast<size_t>(chroma_width) *
        static_cast<size_t>(chroma_height),
        0
    );

    if (d == nullptr || frame == nullptr || vsapi == nullptr) {
        return;
    }

    const uint8_t* src_luma = vsapi->getReadPtr(frame, 0);
    const ptrdiff_t src_luma_stride = vsapi->getStride(frame, 0);
    const int luma_width = vsapi->getFrameWidth(frame, 0);
    const int luma_height = vsapi->getFrameHeight(frame, 0);

    for (int y = 0; y < chroma_height; ++y) {
        const int y0 = clamp_int(
            y << d->vi->format.subSamplingH,
            0,
            luma_height - 1
        );

        const int y1 = clamp_int(
            y0 + d->vi->format.subSamplingH,
            0,
            luma_height - 1
        );

        const uint8_t *row0 = src_luma + y0 * src_luma_stride;
        const uint8_t *row1 = src_luma + y1 * src_luma_stride;

        int *dst_row =
            luma_buffer.data() +
            static_cast<size_t>(y) * static_cast<size_t>(chroma_width);

        for (int x = 0; x < chroma_width; ++x) {
            const int x0 = clamp_int(
                x << d->vi->format.subSamplingW,
                0,
                luma_width - 1
            );

            const int x1 = clamp_int(
                x0 + 1,
                0,
                luma_width - 1
            );

            dst_row[x] =
                (
                    static_cast<int>(row0[x0]) +
                    static_cast<int>(row0[x1]) +
                    static_cast<int>(row1[x0]) +
                    static_cast<int>(row1[x1]) +
                    2
                ) >> 2;
        }
    }
}

static void build_cnr3_downsampled_luma_buffer_u16(
    const Cnr3Data *d,
    const VSFrame *frame,
    int chroma_width,
    int chroma_height,
    std::vector<int> &luma_buffer,
    const VSAPI *vsapi
) {
    /*
        Build a luma buffer at chroma resolution for 10/12/16-bit clips.

        VapourSynth stores integer formats above 8-bit as 16-bit samples.
        The averaging shape intentionally mirrors the 8-bit helper so the
        high-bit-depth path remains a scaled extension of the vscnr2 8-bit
        behaviour.
    */
    luma_buffer.assign(
        static_cast<size_t>(chroma_width) *
        static_cast<size_t>(chroma_height),
        0
    );

    if (d == nullptr || frame == nullptr || vsapi == nullptr) {
        return;
    }

    const uint8_t* src_luma_base = vsapi->getReadPtr(frame, 0);
    const ptrdiff_t src_luma_stride = vsapi->getStride(frame, 0);
    const int luma_width = vsapi->getFrameWidth(frame, 0);
    const int luma_height = vsapi->getFrameHeight(frame, 0);

    for (int y = 0; y < chroma_height; ++y) {
        const int y0 = clamp_int(
            y << d->vi->format.subSamplingH,
            0,
            luma_height - 1
        );

        const int y1 = clamp_int(
            y0 + d->vi->format.subSamplingH,
            0,
            luma_height - 1
        );

        const uint16_t *row0 =
            reinterpret_cast<const uint16_t *>(
                src_luma_base + y0 * src_luma_stride
            );

        const uint16_t *row1 =
            reinterpret_cast<const uint16_t *>(
                src_luma_base + y1 * src_luma_stride
            );

        int *dst_row =
            luma_buffer.data() +
            static_cast<size_t>(y) * static_cast<size_t>(chroma_width);

        for (int x = 0; x < chroma_width; ++x) {
            const int x0 = clamp_int(
                x << d->vi->format.subSamplingW,
                0,
                luma_width - 1
            );

            const int x1 = clamp_int(
                x0 + 1,
                0,
                luma_width - 1
            );

            dst_row[x] =
                (
                    static_cast<int>(row0[x0]) +
                    static_cast<int>(row0[x1]) +
                    static_cast<int>(row1[x0]) +
                    static_cast<int>(row1[x1]) +
                    2
                ) >> 2;
        }
    }
}

static bool build_cnr3_downsampled_luma_buffer(
    const Cnr3Data *d,
    const VSFrame *frame,
    int chroma_width,
    int chroma_height,
    int bytes_per_sample,
    std::vector<int> &luma_buffer,
    const VSAPI *vsapi
) {
    /*
        Dispatch helper for the temporary scaffold and later blend path.

        The returned buffer has exactly one integer luma value for each chroma
        sample. That lets the chroma loop use:

            diff_y = current_downsampled_y - previous_downsampled_y

        which is much closer to vscnr2 than using one representative full-size
        luma sample per chroma sample.
    */
    if (
        d == nullptr ||
        frame == nullptr ||
        vsapi == nullptr ||
        chroma_width <= 0 ||
        chroma_height <= 0
    ) {
        return false;
    }

    if (bytes_per_sample == 1) {
        build_cnr3_downsampled_luma_buffer_u8(
            d,
            frame,
            chroma_width,
            chroma_height,
            luma_buffer,
            vsapi
        );

        return true;
    }

    if (bytes_per_sample == 2) {
        build_cnr3_downsampled_luma_buffer_u16(
            d,
            frame,
            chroma_width,
            chroma_height,
            luma_buffer,
            vsapi
        );

        return true;
    }

    return false;
}

static const std::vector<int> &cnr3_get_table_for_chroma_plane(
    const Cnr3Data *d,
    int plane
) {
    /*
        Plane convention:
            plane 1 = U
            plane 2 = V

        The caller has already validated that plane is either 1 or 2.
    */
    return (plane == 1) ? d->table_u : d->table_v;
}

struct Cnr3ResponseDebugStats {
    /*
        Compact per-frame, per-plane diagnostic counters.

        These counters do not affect output pixels. They are a scaffold step
        toward the real recursive blend.

        The goal is to answer:
            - are the Y response tables allowing any future blend?
            - are the U/V response tables allowing any future blend?
            - are both responses non-zero at the same chroma sample?

        A future Cnr2-style blend will only be useful where the combined
        response is non-zero.
    */
    uint64_t evaluated_samples = 0;
    uint64_t y_nonzero_samples = 0;
    uint64_t chroma_nonzero_samples = 0;
    uint64_t combined_nonzero_samples = 0;

    uint64_t y_response_sum = 0;
    uint64_t chroma_response_sum = 0;
};

static void cnr3_update_response_debug_stats(
    Cnr3ResponseDebugStats &stats,
    int y_response,
    int chroma_response
) {
    ++stats.evaluated_samples;

    if (y_response > 0) {
        ++stats.y_nonzero_samples;
    }

    if (chroma_response > 0) {
        ++stats.chroma_nonzero_samples;
    }

    if (y_response > 0 && chroma_response > 0) {
        ++stats.combined_nonzero_samples;
    }

    stats.y_response_sum += static_cast<uint64_t>(y_response);
    stats.chroma_response_sum += static_cast<uint64_t>(chroma_response);
}

static void cnr3_print_response_debug_stats(
    const Cnr3Data *d,
    int frame_number,
    int plane,
    const Cnr3ResponseDebugStats &stats
) {
    /*
        Print one compact diagnostic line per frame and chroma plane.

        This intentionally avoids per-pixel logging. Per-pixel logging would be
        unusable with vspipe and would also obscure the scheduling/cache logs.
    */
    if (d == nullptr || !d->debug) {
        return;
    }

    const char plane_name = (plane == 1) ? 'U' : 'V';

    if (stats.evaluated_samples == 0) {
        cnr3_debug_printf(
            d->debug,
            "CNR3 debug: instance=%d, frame=%d, plane=%c, response stats: no previous output available.\n",
            d->instance_id,
            frame_number,
            plane_name
        );

        return;
    }

    const double evaluated =
        static_cast<double>(stats.evaluated_samples);

    const double y_avg =
        static_cast<double>(stats.y_response_sum) / evaluated;

    const double chroma_avg =
        static_cast<double>(stats.chroma_response_sum) / evaluated;

    const double y_nonzero_percent =
        100.0 * static_cast<double>(stats.y_nonzero_samples) / evaluated;

    const double chroma_nonzero_percent =
        100.0 * static_cast<double>(stats.chroma_nonzero_samples) / evaluated;

    const double combined_nonzero_percent =
        100.0 * static_cast<double>(stats.combined_nonzero_samples) / evaluated;

    cnr3_debug_printf(
        d->debug,
        "CNR3 debug: instance=%d, frame=%d, plane=%c, response stats: "
        "samples=%llu, y_nonzero=%llu/%.2f%%, chroma_nonzero=%llu/%.2f%%, "
        "combined_nonzero=%llu/%.2f%%, y_avg=%.2f, chroma_avg=%.2f\n",
        d->instance_id,
        frame_number,
        plane_name,
        static_cast<unsigned long long>(stats.evaluated_samples),
        static_cast<unsigned long long>(stats.y_nonzero_samples),
        y_nonzero_percent,
        static_cast<unsigned long long>(stats.chroma_nonzero_samples),
        chroma_nonzero_percent,
        static_cast<unsigned long long>(stats.combined_nonzero_samples),
        combined_nonzero_percent,
        y_avg,
        chroma_avg
    );
}

struct Cnr3BlendDebugStats {
    /*
        Compact per-frame, per-plane blend-strength counters.

        These counters are diagnostics only. They do not affect output pixels.

        The goal is to answer:
            - did the blend path actually run?
            - how much previous filtered chroma was reused?
            - are weights mostly tiny, moderate, or near the strongest weight
              possible for the current mode/threshold/table setup?

        weight_scale is the mathematical blend denominator:
            8-bit:   65536
            16-bit:  4294967296

        max_possible_weight is the strongest weight this plane can reach with
        the current response tables:

            table_y[0] * table_u_or_v[0]

        This distinction matters because the historical defaults do not reach
        100% mathematical blend weight. For example, 8-bit defaults give:

            192 * 254 / 65536 = about 74.41%

        So "near max" is more useful than "near full scale".
    */
    uint64_t evaluated_samples = 0;
    uint64_t active_blend_samples = 0;
    uint64_t nonzero_weight_samples = 0;
    uint64_t near_max_weight_samples = 0;

    uint64_t weight_sum = 0;
    int64_t weight_min = 0;
    int64_t weight_max = 0;

    bool have_weight = false;
};

static void cnr3_update_blend_debug_stats(
    Cnr3BlendDebugStats &stats,
    int64_t weight,
    int64_t max_possible_weight
) {
    ++stats.evaluated_samples;
    ++stats.active_blend_samples;

    if (weight > 0) {
        ++stats.nonzero_weight_samples;
    }

    /*
        "Near max" means near the strongest weight reachable with the current
        response tables, not near 100% of the mathematical blend denominator.

        This makes the diagnostic useful with historical defaults, where the
        maximum possible 8-bit weight is about 74.41% of full scale.
    */
    if (
        max_possible_weight > 0 &&
        weight >= (max_possible_weight * 95) / 100
    ) {
        ++stats.near_max_weight_samples;
    }

    stats.weight_sum += static_cast<uint64_t>(weight);

    if (!stats.have_weight) {
        stats.weight_min = weight;
        stats.weight_max = weight;
        stats.have_weight = true;
    } else {
        if (weight < stats.weight_min) {
            stats.weight_min = weight;
        }

        if (weight > stats.weight_max) {
            stats.weight_max = weight;
        }
    }
}

static void cnr3_print_blend_debug_stats(
    const Cnr3Data *d,
    int frame_number,
    int plane,
    const Cnr3BlendDebugStats &stats
) {
    /*
        Print one compact blend-strength diagnostic line per frame and chroma
        plane. This is deliberately aggregate-only; do not add per-pixel logs.
    */
    if (d == nullptr || !d->debug) {
        return;
    }

    const char plane_name = (plane == 1) ? 'U' : 'V';

    if (!d->blend) {
        return;
    }

    if (stats.active_blend_samples == 0 || !stats.have_weight) {
        cnr3_debug_printf(
            d->debug,
            "CNR3 debug: instance=%d, frame=%d, plane=%c, blend stats: no active blend samples.\n",
            d->instance_id,
            frame_number,
            plane_name
        );

        return;
    }

    const double active =
        static_cast<double>(stats.active_blend_samples);

    const int64_t weight_scale =
        get_cnr3_blend_scale(d->bits_per_sample);

    const double weight_avg_percent =
        100.0 *
        static_cast<double>(stats.weight_sum) /
        (
            active *
            static_cast<double>(weight_scale)
        );

    const double weight_min_percent =
        100.0 *
        static_cast<double>(stats.weight_min) /
        static_cast<double>(weight_scale);

    const double weight_max_percent =
        100.0 *
        static_cast<double>(stats.weight_max) /
        static_cast<double>(weight_scale);

    const double nonzero_percent =
        100.0 *
        static_cast<double>(stats.nonzero_weight_samples) /
        active;

    const double near_max_percent =
        100.0 *
        static_cast<double>(stats.near_max_weight_samples) /
        active;

    cnr3_debug_printf(
        d->debug,
        "CNR3 debug: instance=%d, frame=%d, plane=%c, blend stats: "
        "samples=%llu, active=%llu, weight_nonzero=%llu/%.2f%%, "
        "weight_near_max=%llu/%.2f%%, weight_avg=%.2f%%, "
        "weight_min=%.2f%%, weight_max=%.2f%%\n",
        d->instance_id,
        frame_number,
        plane_name,
        static_cast<unsigned long long>(stats.evaluated_samples),
        static_cast<unsigned long long>(stats.active_blend_samples),
        static_cast<unsigned long long>(stats.nonzero_weight_samples),
        nonzero_percent,
        static_cast<unsigned long long>(stats.near_max_weight_samples),
        near_max_percent,
        weight_avg_percent,
        weight_min_percent,
        weight_max_percent
    );
}

static void process_cnr3_chroma_plane_passthrough_u8(
    const Cnr3Data *d,
    int frame_number,
    const VSFrame *src,
    const VSFrame *prev_output,
    VSFrame *dst,
    int plane,
    const std::vector<int> &current_luma,
    const std::vector<int> &previous_luma,
    const VSAPI *vsapi
) {
    /*
        Temporary per-sample chroma loop for 8-bit integer clips.

        This is deliberately still pass-through:
            dst[x] = src[x]

        Scaffold purpose:
            - read current source chroma
            - read previous filtered chroma when frame_number > 0
            - calculate signed current-vs-previous chroma difference
            - use vscnr2-style downsampled luma buffers for Y difference
            - look up Y and U/V response-table values
            - still write current source chroma unchanged

        This proves the real algorithm's read paths and table lookup paths
        without yet letting previous-frame chroma affect the image.
    */
    if (d == nullptr || src == nullptr || dst == nullptr || vsapi == nullptr) {
        return;
    }

    const uint8_t *srcp = vsapi->getReadPtr(src, plane);
    uint8_t *dstp = vsapi->getWritePtr(dst, plane);

    const uint8_t *prevp =
        (frame_number > 0 && prev_output != nullptr) ?
        vsapi->getReadPtr(prev_output, plane) :
        nullptr;

    const ptrdiff_t src_stride = vsapi->getStride(src, plane);
    const ptrdiff_t dst_stride = vsapi->getStride(dst, plane);

    const ptrdiff_t prev_stride =
        (prev_output != nullptr) ? vsapi->getStride(prev_output, plane) : 0;

    const int plane_width = vsapi->getFrameWidth(src, plane);
    const int plane_height = vsapi->getFrameHeight(src, plane);

    const std::vector<int> &chroma_table =
        cnr3_get_table_for_chroma_plane(d, plane);

    const int64_t max_possible_blend_weight =
        calculate_cnr3_max_possible_blend_weight(
            d,
            chroma_table
        );

    Cnr3ResponseDebugStats response_stats;
    Cnr3BlendDebugStats blend_stats;

    for (int y = 0; y < plane_height; ++y) {
        const uint8_t *src_row = srcp;
        uint8_t *dst_row = dstp;

        const uint8_t *prev_row =
            (prevp != nullptr) ? prevp : nullptr;

        const size_t luma_row_offset =
            static_cast<size_t>(y) * static_cast<size_t>(plane_width);

        for (int x = 0; x < plane_width; ++x) {
            const uint8_t current_chroma = src_row[x];

            int y_response = d->sample_peak;
            int chroma_response = d->sample_peak;

            if (prev_row != nullptr && !previous_luma.empty()) {
                const uint8_t previous_chroma = prev_row[x];

                const int chroma_signed_diff =
                    static_cast<int>(current_chroma) -
                    static_cast<int>(previous_chroma);

                chroma_response = get_cnr3_table_value_for_signed_diff(
                    chroma_table,
                    d->table_offset,
                    chroma_signed_diff
                );

                const size_t luma_index =
                    luma_row_offset + static_cast<size_t>(x);

                const int current_downsampled_luma = current_luma[luma_index];
                const int previous_downsampled_luma = previous_luma[luma_index];

                const int y_signed_diff =
                    current_downsampled_luma - previous_downsampled_luma;

                y_response = get_cnr3_table_value_for_signed_diff(
                    d->table_y,
                    d->table_offset,
                    y_signed_diff
                );

                cnr3_update_response_debug_stats(
                    response_stats,
                    y_response,
                    chroma_response
                );
            }

            /*
                Development behaviour:

                    blend=false:
                        preserve the known-good pass-through output.

                    blend=true and previous output is available:
                        enable the first real vscnr2-style recursive chroma
                        blend, using previous filtered output as history.

                Frame 0 remains pass-through because prev_row is null.
            */
            if (d->blend && prev_row != nullptr) {
                const uint8_t previous_chroma = prev_row[x];

                const int64_t blend_weight =
                    calculate_cnr3_combined_blend_weight(
                        y_response,
                        chroma_response
                    );

                cnr3_update_blend_debug_stats(
                    blend_stats,
                    blend_weight,
                    max_possible_blend_weight
                );

                const int blended_chroma = blend_cnr3_chroma_sample(
                    static_cast<int>(current_chroma),
                    static_cast<int>(previous_chroma),
                    y_response,
                    chroma_response,
                    d->bits_per_sample
                );

                dst_row[x] = static_cast<uint8_t>(
                    clamp_int(blended_chroma, 0, d->sample_peak)
                );
            } else {
                dst_row[x] = current_chroma;
            }
        }
        
        srcp += src_stride;
        dstp += dst_stride;

        if (prevp != nullptr) {
            prevp += prev_stride;
        }
    }

    cnr3_print_response_debug_stats(
        d,
        frame_number,
        plane,
        response_stats
    );

    cnr3_print_blend_debug_stats(
        d,
        frame_number,
        plane,
        blend_stats
    );
}

static void process_cnr3_chroma_plane_passthrough_u16(
    const Cnr3Data *d,
    int frame_number,
    const VSFrame *src,
    const VSFrame *prev_output,
    VSFrame *dst,
    int plane,
    const std::vector<int> &current_luma,
    const std::vector<int> &previous_luma,
    const VSAPI *vsapi
) {
    /*
        Temporary per-sample chroma loop for 10/12/16-bit integer clips.

        VapourSynth stores integer formats above 8-bit in 16-bit samples, so
        this path handles all currently accepted high-bit-depth inputs.

        This is deliberately still pass-through:
            dst[x] = src[x]

        Scaffold purpose:
            prove the same current/previous read paths, vscnr2-style
            downsampled-luma Y guard, and signed table lookups as the 8-bit
            path, while preserving high-bit-depth pass-through output before
            real blending is connected.
    */
    if (d == nullptr || src == nullptr || dst == nullptr || vsapi == nullptr) {
        return;
    }

    const uint8_t *srcp = vsapi->getReadPtr(src, plane);
    uint8_t *dstp = vsapi->getWritePtr(dst, plane);

    const uint8_t *prevp =
        (frame_number > 0 && prev_output != nullptr) ?
        vsapi->getReadPtr(prev_output, plane) :
        nullptr;

    const ptrdiff_t src_stride = vsapi->getStride(src, plane);
    const ptrdiff_t dst_stride = vsapi->getStride(dst, plane);

    const ptrdiff_t prev_stride =
        (prev_output != nullptr) ? vsapi->getStride(prev_output, plane) : 0;

    const int plane_width = vsapi->getFrameWidth(src, plane);
    const int plane_height = vsapi->getFrameHeight(src, plane);

    const std::vector<int> &chroma_table =
        cnr3_get_table_for_chroma_plane(d, plane);

    const int64_t max_possible_blend_weight =
        calculate_cnr3_max_possible_blend_weight(
            d,
            chroma_table
        );

    Cnr3ResponseDebugStats response_stats;
    Cnr3BlendDebugStats blend_stats;

    for (int y = 0; y < plane_height; ++y) {
        const uint16_t *src_row =
            reinterpret_cast<const uint16_t *>(srcp);

        uint16_t *dst_row =
            reinterpret_cast<uint16_t *>(dstp);

        const uint16_t *prev_row =
            (prevp != nullptr) ?
            reinterpret_cast<const uint16_t *>(prevp) :
            nullptr;

        const size_t luma_row_offset =
            static_cast<size_t>(y) * static_cast<size_t>(plane_width);

        for (int x = 0; x < plane_width; ++x) {
            const uint16_t current_chroma = src_row[x];

            int y_response = d->sample_peak;
            int chroma_response = d->sample_peak;

            if (prev_row != nullptr && !previous_luma.empty()) {
                const uint16_t previous_chroma = prev_row[x];

                const int chroma_signed_diff =
                    static_cast<int>(current_chroma) -
                    static_cast<int>(previous_chroma);

                chroma_response = get_cnr3_table_value_for_signed_diff(
                    chroma_table,
                    d->table_offset,
                    chroma_signed_diff
                );

                const size_t luma_index =
                    luma_row_offset + static_cast<size_t>(x);

                const int current_downsampled_luma = current_luma[luma_index];
                const int previous_downsampled_luma = previous_luma[luma_index];

                const int y_signed_diff =
                    current_downsampled_luma - previous_downsampled_luma;

                y_response = get_cnr3_table_value_for_signed_diff(
                    d->table_y,
                    d->table_offset,
                    y_signed_diff
                );

                cnr3_update_response_debug_stats(
                    response_stats,
                    y_response,
                    chroma_response
                );
            }

            /*
                Development behaviour:

                    blend=false:
                        preserve the known-good pass-through output.

                    blend=true and previous output is available:
                        enable the first real vscnr2-style recursive chroma
                        blend, using previous filtered output as history.

                Frame 0 remains pass-through because prev_row is null.
            */
            if (d->blend && prev_row != nullptr) {
                const uint16_t previous_chroma = prev_row[x];

                const int64_t blend_weight =
                    calculate_cnr3_combined_blend_weight(
                        y_response,
                        chroma_response
                    );

                cnr3_update_blend_debug_stats(
                    blend_stats,
                    blend_weight,
                    max_possible_blend_weight
                );

                const int blended_chroma = blend_cnr3_chroma_sample(
                    static_cast<int>(current_chroma),
                    static_cast<int>(previous_chroma),
                    y_response,
                    chroma_response,
                    d->bits_per_sample
                );

                dst_row[x] = static_cast<uint16_t>(
                    clamp_int(blended_chroma, 0, d->sample_peak)
                );
            } else {
                dst_row[x] = current_chroma;
            }
        }

        srcp += src_stride;
        dstp += dst_stride;

        if (prevp != nullptr) {
            prevp += prev_stride;
        }
    }

    cnr3_print_response_debug_stats(
        d,
        frame_number,
        plane,
        response_stats
    );

    cnr3_print_blend_debug_stats(
        d,
        frame_number,
        plane,
        blend_stats
    );
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
        write from prev_output. The real recursive algorithm will need it, and
        the frame-level function has already established this as part of the
        recursive precondition.
    */
    if (frame_number > 0 && prev_output == nullptr) {
        return false;
    }

    const int bytes_per_sample = (d->bits_per_sample + 7) / 8;

    const int plane_width = vsapi->getFrameWidth(src, plane);
    const int plane_height = vsapi->getFrameHeight(src, plane);

    std::vector<int> current_luma;
    std::vector<int> previous_luma;

    if (!build_cnr3_downsampled_luma_buffer(
        d,
        src,
        plane_width,
        plane_height,
        bytes_per_sample,
        current_luma,
        vsapi
    )) {
        return false;
    }

    if (frame_number > 0) {
        if (!build_cnr3_downsampled_luma_buffer(
            d,
            prev_output,
            plane_width,
            plane_height,
            bytes_per_sample,
            previous_luma,
            vsapi
        )) {
            return false;
        }
    }

    cnr3_debug_printf(
        d->debug && frame_number <= 2,
        "CNR3 debug: process_cnr3_chroma_plane() instance=%d, frame=%d, plane=%c, using downsampled-luma guard buffer: chroma=%dx%d, subsampling=%d:%d, blend=%d\n",
        d->instance_id,
        frame_number,
        plane == 1 ? 'U' : 'V',
        plane_width,
        plane_height,
        d->vi->format.subSamplingW,
        d->vi->format.subSamplingH,
        d->blend ? 1 : 0
    );

    /*
        Scaffold stage only.

        At this point, CNR3 intentionally still writes pass-through chroma.
        The purpose of using per-sample loops now is to prove the exact loop
        structure that the real recursive blend will later use.

        This version now uses vscnr2-style downsampled-luma buffers at chroma
        resolution for the Y guard instead of one representative full-size luma
        sample. That is important because the real-clip diagnostics showed the
        luma guard doing much of the future blend gating.

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
            d,
            frame_number,
            src,
            prev_output,
            dst,
            plane,
            current_luma,
            previous_luma,
            vsapi
        );

        return true;
    }

    if (bytes_per_sample == 2) {
        process_cnr3_chroma_plane_passthrough_u16(
            d,
            frame_number,
            src,
            prev_output,
            dst,
            plane,
            current_luma,
            previous_luma,
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

    /*
        Development/maintenance option.
        Default to recursive Cnr2-style chroma blending enabled.
        blend=false remains available for maintenance/testing because it keeps
        the diagnostic read/table paths active while forcing chroma output to
        pass through unchanged.
   */
    local.blend = get_optional_int(in, vsapi, "blend", 1) != 0;

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
            "mode=%s, scdthr=%f, scene_chroma=%d, blend=%d\n",
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
            local.scene_chroma ? 1 : 0,
            local.blend ? 1 : 0
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
        "org.vapoursynth.cnr3",
        "cnr3",
        "CNR3 recursive chroma stabiliser",
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
        "blend:int:opt;"
        "debug:int:opt;",
        "clip:vnode;",
        cnr3_create,
        nullptr,
        plugin
    );
}

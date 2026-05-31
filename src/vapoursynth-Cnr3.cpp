/*
    CNR3 - VapourSynth API4 chroma stabiliser, based on the venerable CNR2/VSCNR2.

    CNR3 is a redevelopment intended to closely follow the Cnr2/vscnr2 recursive
    temporal chroma-stabilisation model while using VapourSynth API4 only.

    Recursive processing and VapourSynth scheduling:
        The Cnr2/vscnr2 algorithm is inherently temporal and recursive, which
        requires in-order serial frame processing.

        Processing of SOURCE frame N into OUTPUT N requires access to the already
        filtered OUTPUT arising from previously processed SOURCE frame N - 1:
            output[N] depends on both SOURCE[N] and OUTPUT[N - 1]

        That makes the CNR2/vscnr2 algorithm naturally "serial".

        Older VapourSynth-era recursive filters could sometimes rely on
        compatibility-style scheduling parameters and assumptions. In
        particular, 'fmFrameState' meant only one thread would call a filter's
        getframe function at a time and only one frame would be processed at a
        time.

        However, VapourSynth API4 documentation says 'fmFrameState' is
        for compatibility only and MUST NOT BE USED IN NEW FILTERS.

        CNR3 therefore uses 'fmUnordered'.

        In 'fmUnordered', only one thread can call this filter's getframe function
        at a time, which protects CNR3's internal recursive state from
        concurrent entry. HOWEVER, 'fmUnordered' does not guarantee in-order frame
        processing. VapourSynth may STILL call CNR3's getframe for frames in a
        NON-SERIAL ORDER, which effectively defeats a recursive output[N - 1]
        algorithm without special measures.

        INITIAL REDEVELOPMENT APPROACH:
        CNR3 implementation currently uses a strict streaming cache policy:
            - frame 0 initialises the previous-output state
            - frame N requires output[N - 1] to have already been produced
            - out-of-order frame requests are rejected with a clear error
        During testing, use:
            vspipe -r 1
        This is a deliberate correctness-first API4 bridge.
        An upcoming cache manager will relax the strict ordering requirement
        by adding reorder, seek, checkpoint, or recomputation support.
        Until then, CNR3 must be treated as a serial recursive filter.

    Diagnostic output rule:
        CNR3 must never write to stdout, debug/status messages must go to stderr.
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

#include "cnr3_build_config.h"
#include "cnr3_common.h"
#include "cnr3_response_tables.h"

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
static std::atomic<int> g_cnr3_next_instance_id{ 1 };
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// HELPER functions
// -----------------------------------------------------------------------------

static void cnr3_vfprintf_stderr(
    const char* format,
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
    const char* format,
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

static void cnr3_debug_print_cache_state(
    const Cnr3Data* d,
    const char* where,
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

static void cnr3_debug_print_cache_manager_v005_summary(
    const Cnr3Data* d,
    const char* where
) {
    /*
        Thread safety:
            Uses cnr3_cache_manager_get_debug_snapshot(), which locks the cache
            manager once and returns a coherent passive snapshot.

        Caller requirement:
            Caller must not already hold d->cache_manager_v005.cache_mutex.

        Purpose:
            Print one compact v005 cache-manager diagnostic summary line for
            this CNR3 filter instance.

        Important:
            Phase 3B only adds diagnostic plumbing. It does not make the v005
            cache manager participate in frame scheduling or output generation.

        Diagnostic context:
            The where argument is deliberately included in the output line so
            logs show exactly when the snapshot was taken.

        Snapshot behaviour:
            This diagnostic summary is passive. Printing it does not increment
            cache-manager validation counters.
    */

    if (d == nullptr || !d->debug || where == nullptr) {
        return;
    }

    Cnr3CacheManagerV005& cache =
        const_cast<Cnr3CacheManagerV005&>(d->cache_manager_v005);

    Cnr3CacheManagerDebugSnapshot snapshot;

    if (!cnr3_cache_manager_get_debug_snapshot(cache, snapshot)) {
        cnr3_debug_printf(
            d->debug,
            "CNR3 debug: instance=%d, %s: cache_manager_v005 summary unavailable.\n",
            d->instance_id,
            where
        );

        return;
    }

    const Cnr3CacheManagerStats& stats = snapshot.stats;

    cnr3_debug_printf(
        d->debug,
        "CNR3 debug: instance=%d, %s: cache_manager_v005 summary: "
        "active=0, non_checkpoint_count=%llu, checkpoint_count=%llu, "
        "total_cached_frame_count=%llu, highest_cached_frame_number=%d, "
        "has_pinned_checkpoints=%d, total_pin_count=%lld, invariants_ok=%d, "
        "store=%lld/%lld/%lld, remove=%lld/%lld/%lld, "
        "prune_after_store=%lld/%lld/%lld, "
        "find_and_pin=%lld/%lld/%lld, unpin=%lld/%lld/%lld, "
        "validation=%lld/%lld/%lld, integrity_errors=%lld, "
        "post_validation_failures: store=%lld, remove=%lld, "
        "non_checkpoint_prune=%lld, checkpoint_prune=%lld, "
        "prune_after_store=%lld\n", d->instance_id,
        where,
        static_cast<unsigned long long>(snapshot.non_checkpoint_count),
        static_cast<unsigned long long>(snapshot.checkpoint_count),
        static_cast<unsigned long long>(snapshot.total_cached_frame_count),
        snapshot.highest_cached_frame_number,
        snapshot.has_pinned_checkpoints ? 1 : 0,
        static_cast<long long>(snapshot.total_pin_count),
        snapshot.invariants_ok ? 1 : 0,
        static_cast<long long>(stats.cache_store_attempts),
        static_cast<long long>(stats.cache_store_successes),
        static_cast<long long>(stats.cache_store_failures),
        static_cast<long long>(stats.cache_remove_attempts),
        static_cast<long long>(stats.cache_remove_successes),
        static_cast<long long>(stats.cache_remove_failures),
        static_cast<long long>(stats.prune_after_store_attempts),
        static_cast<long long>(stats.prune_after_store_successes),
        static_cast<long long>(stats.prune_after_store_failures),
        static_cast<long long>(stats.checkpoint_find_and_pin_attempts),
        static_cast<long long>(stats.checkpoint_find_and_pin_successes),
        static_cast<long long>(stats.checkpoint_find_and_pin_failures),
        static_cast<long long>(stats.checkpoint_unpin_attempts),
        static_cast<long long>(stats.checkpoint_unpin_successes),
        static_cast<long long>(stats.checkpoint_unpin_failures),
        static_cast<long long>(stats.cache_validation_attempts),
        static_cast<long long>(stats.cache_validation_successes),
        static_cast<long long>(stats.cache_validation_failures),
        static_cast<long long>(stats.cache_integrity_errors),
        static_cast<long long>(stats.cache_store_post_validation_failures),
        static_cast<long long>(stats.cache_remove_post_validation_failures),
        static_cast<long long>(stats.non_checkpoint_prune_post_validation_failures),
        static_cast<long long>(stats.checkpoint_prune_post_validation_failures),
        static_cast<long long>(stats.prune_after_store_post_validation_failures));
}

static int64_t get_optional_int(
    const VSMap* in,
    const VSAPI* vsapi,
    const char* name,
    int64_t default_value
) {
    int err = 0;
    const int64_t value = vsapi->mapGetInt(in, name, 0, &err);
    return err ? default_value : value;
}

static double get_optional_float(
    const VSMap* in,
    const VSAPI* vsapi,
    const char* name,
    double default_value
) {
    int err = 0;
    const double value = vsapi->mapGetFloat(in, name, 0, &err);
    return err ? default_value : value;
}

static std::string get_optional_data_string(
    const VSMap* in,
    const VSAPI* vsapi,
    const char* name,
    const char* default_value
) {
    int err = 0;
    const char* value = vsapi->mapGetData(in, name, 0, &err);
    if (err || value == nullptr) {
        return std::string(default_value);
    }

    return std::string(value);
}

static bool validate_cnr3_format(
    const VSVideoInfo* vi,
    VSMap* out,
    const VSAPI* vsapi
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

static void copy_plane_bytes(
    const VSFrame* src,
    VSFrame* dst,
    int plane,
    int bytes_per_sample,
    const VSAPI* vsapi
) {
    const uint8_t* srcp = vsapi->getReadPtr(src, plane);
    uint8_t* dstp = vsapi->getWritePtr(dst, plane);

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
    const VSFrame* src,
    VSFrame* dst,
    int bytes_per_sample,
    const VSAPI* vsapi
) {
    copy_plane_bytes(src, dst, 0, bytes_per_sample, vsapi);
    copy_plane_bytes(src, dst, 1, bytes_per_sample, vsapi);
    copy_plane_bytes(src, dst, 2, bytes_per_sample, vsapi);
}

static void build_cnr3_downsampled_luma_buffer_u8(
    const Cnr3Data* d,
    const VSFrame* frame,
    int chroma_width,
    int chroma_height,
    std::vector<int>& luma_buffer,
    const VSAPI* vsapi
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
        const int y0 = cnr3_clamp_int(
            y << d->vi->format.subSamplingH,
            0,
            luma_height - 1
        );

        const int y1 = cnr3_clamp_int(
            y0 + d->vi->format.subSamplingH,
            0,
            luma_height - 1
        );

        const uint8_t* row0 = src_luma + y0 * src_luma_stride;
        const uint8_t* row1 = src_luma + y1 * src_luma_stride;

        int* dst_row =
            luma_buffer.data() +
            static_cast<size_t>(y) * static_cast<size_t>(chroma_width);

        for (int x = 0; x < chroma_width; ++x) {
            const int x0 = cnr3_clamp_int(
                x << d->vi->format.subSamplingW,
                0,
                luma_width - 1
            );

            const int x1 = cnr3_clamp_int(
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
    const Cnr3Data* d,
    const VSFrame* frame,
    int chroma_width,
    int chroma_height,
    std::vector<int>& luma_buffer,
    const VSAPI* vsapi
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
        const int y0 = cnr3_clamp_int(
            y << d->vi->format.subSamplingH,
            0,
            luma_height - 1
        );

        const int y1 = cnr3_clamp_int(
            y0 + d->vi->format.subSamplingH,
            0,
            luma_height - 1
        );

        const uint16_t* row0 =
            reinterpret_cast<const uint16_t*>(
                src_luma_base + y0 * src_luma_stride
                );

        const uint16_t* row1 =
            reinterpret_cast<const uint16_t*>(
                src_luma_base + y1 * src_luma_stride
                );

        int* dst_row =
            luma_buffer.data() +
            static_cast<size_t>(y) * static_cast<size_t>(chroma_width);

        for (int x = 0; x < chroma_width; ++x) {
            const int x0 = cnr3_clamp_int(
                x << d->vi->format.subSamplingW,
                0,
                luma_width - 1
            );

            const int x1 = cnr3_clamp_int(
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
    const Cnr3Data* d,
    const VSFrame* frame,
    int chroma_width,
    int chroma_height,
    int bytes_per_sample,
    std::vector<int>& luma_buffer,
    const VSAPI* vsapi
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

struct Cnr3SceneChangeStats {
    /*
        Compact frame-level scene-change diagnostic.

        This mirrors the vscnr2 diff_total/diff_max model:
            - luma difference is calculated from the downsampled-luma buffers
            - luma contribution is scaled by subSamplingW + subSamplingH
            - chroma U/V differences are included only when scene_chroma is true

        If diff_total exceeds diff_max, the frame is treated as a scene change.
    */
    bool evaluated = false;
    bool scene_change = false;

    int evaluated_rows = 0;
    int64_t evaluated_samples = 0;

    int64_t diff_total = 0;
    int64_t diff_max = 0;
};

static Cnr3SceneChangeStats detect_cnr3_scene_change_u8(
    const Cnr3Data* d,
    const VSFrame* src,
    const VSFrame* prev_output,
    int chroma_width,
    int chroma_height,
    const std::vector<int>& current_luma,
    const std::vector<int>& previous_luma,
    const VSAPI* vsapi
) {
    /*
        Detect scene changes using a vscnr2-style diff_total/diff_max metric
        for 8-bit clips.

        vscnr2 accumulates, at chroma resolution:

            diff_total += abs(diff_y << (subSamplingW + subSamplingH))

        and, when scene_chroma is true:

            diff_total += abs(diff_u) + abs(diff_v)

        If diff_total exceeds diff_max, the frame is treated as a scene change.
    */
    Cnr3SceneChangeStats stats;
    stats.diff_max = (d != nullptr) ? d->scene_change_threshold : 0;

    if (
        d == nullptr ||
        src == nullptr ||
        prev_output == nullptr ||
        vsapi == nullptr ||
        chroma_width <= 0 ||
        chroma_height <= 0 ||
        current_luma.size() != previous_luma.size() ||
        current_luma.size() !=
        static_cast<size_t>(chroma_width) * static_cast<size_t>(chroma_height)
        ) {
        return stats;
    }

    stats.evaluated = true;

    const uint8_t* src_u = vsapi->getReadPtr(src, 1);
    const uint8_t* src_v = vsapi->getReadPtr(src, 2);
    const uint8_t* prev_u = vsapi->getReadPtr(prev_output, 1);
    const uint8_t* prev_v = vsapi->getReadPtr(prev_output, 2);

    const ptrdiff_t src_u_stride = vsapi->getStride(src, 1);
    const ptrdiff_t src_v_stride = vsapi->getStride(src, 2);
    const ptrdiff_t prev_u_stride = vsapi->getStride(prev_output, 1);
    const ptrdiff_t prev_v_stride = vsapi->getStride(prev_output, 2);

    const int subsampling_shift =
        d->vi->format.subSamplingW + d->vi->format.subSamplingH;

    int64_t diff_total = 0;

    for (int y = 0; y < chroma_height; ++y) {
        const uint8_t* src_u_row = src_u;
        const uint8_t* src_v_row = src_v;
        const uint8_t* prev_u_row = prev_u;
        const uint8_t* prev_v_row = prev_v;

        const size_t luma_row_offset =
            static_cast<size_t>(y) * static_cast<size_t>(chroma_width);

        for (int x = 0; x < chroma_width; ++x) {
            const size_t luma_index =
                luma_row_offset + static_cast<size_t>(x);

            const int diff_y =
                current_luma[luma_index] - previous_luma[luma_index];

            diff_total += static_cast<int64_t>(
                std::abs(diff_y << subsampling_shift)
                );

            if (d->scene_chroma) {
                const int diff_u =
                    static_cast<int>(src_u_row[x]) -
                    static_cast<int>(prev_u_row[x]);

                const int diff_v =
                    static_cast<int>(src_v_row[x]) -
                    static_cast<int>(prev_v_row[x]);

                diff_total +=
                    static_cast<int64_t>(std::abs(diff_u)) +
                    static_cast<int64_t>(std::abs(diff_v));
            }

            ++stats.evaluated_samples;
        }

        ++stats.evaluated_rows;

        if (diff_total > d->scene_change_threshold) {
            stats.scene_change = true;
            stats.diff_total = diff_total;
            return stats;
        }

        src_u += src_u_stride;
        src_v += src_v_stride;
        prev_u += prev_u_stride;
        prev_v += prev_v_stride;
    }

    stats.diff_total = diff_total;
    return stats;
}

static Cnr3SceneChangeStats detect_cnr3_scene_change_u16(
    const Cnr3Data* d,
    const VSFrame* src,
    const VSFrame* prev_output,
    int chroma_width,
    int chroma_height,
    const std::vector<int>& current_luma,
    const std::vector<int>& previous_luma,
    const VSAPI* vsapi
) {
    /*
        Detect scene changes using a vscnr2-style diff_total/diff_max metric
        for 10/12/14/16-bit clips.

        VapourSynth stores integer formats above 8-bit in 16-bit samples.
    */
    Cnr3SceneChangeStats stats;
    stats.diff_max = (d != nullptr) ? d->scene_change_threshold : 0;

    if (
        d == nullptr ||
        src == nullptr ||
        prev_output == nullptr ||
        vsapi == nullptr ||
        chroma_width <= 0 ||
        chroma_height <= 0 ||
        current_luma.size() != previous_luma.size() ||
        current_luma.size() !=
        static_cast<size_t>(chroma_width) * static_cast<size_t>(chroma_height)
        ) {
        return stats;
    }

    stats.evaluated = true;

    const uint8_t* src_u_base = vsapi->getReadPtr(src, 1);
    const uint8_t* src_v_base = vsapi->getReadPtr(src, 2);
    const uint8_t* prev_u_base = vsapi->getReadPtr(prev_output, 1);
    const uint8_t* prev_v_base = vsapi->getReadPtr(prev_output, 2);

    const ptrdiff_t src_u_stride = vsapi->getStride(src, 1);
    const ptrdiff_t src_v_stride = vsapi->getStride(src, 2);
    const ptrdiff_t prev_u_stride = vsapi->getStride(prev_output, 1);
    const ptrdiff_t prev_v_stride = vsapi->getStride(prev_output, 2);

    const int subsampling_shift =
        d->vi->format.subSamplingW + d->vi->format.subSamplingH;

    int64_t diff_total = 0;

    for (int y = 0; y < chroma_height; ++y) {
        const uint16_t* src_u_row =
            reinterpret_cast<const uint16_t*>(src_u_base);

        const uint16_t* src_v_row =
            reinterpret_cast<const uint16_t*>(src_v_base);

        const uint16_t* prev_u_row =
            reinterpret_cast<const uint16_t*>(prev_u_base);

        const uint16_t* prev_v_row =
            reinterpret_cast<const uint16_t*>(prev_v_base);

        const size_t luma_row_offset =
            static_cast<size_t>(y) * static_cast<size_t>(chroma_width);

        for (int x = 0; x < chroma_width; ++x) {
            const size_t luma_index =
                luma_row_offset + static_cast<size_t>(x);

            const int diff_y =
                current_luma[luma_index] - previous_luma[luma_index];

            diff_total += static_cast<int64_t>(
                std::abs(diff_y << subsampling_shift)
                );

            if (d->scene_chroma) {
                const int diff_u =
                    static_cast<int>(src_u_row[x]) -
                    static_cast<int>(prev_u_row[x]);

                const int diff_v =
                    static_cast<int>(src_v_row[x]) -
                    static_cast<int>(prev_v_row[x]);

                diff_total +=
                    static_cast<int64_t>(std::abs(diff_u)) +
                    static_cast<int64_t>(std::abs(diff_v));
            }

            ++stats.evaluated_samples;
        }

        ++stats.evaluated_rows;

        if (diff_total > d->scene_change_threshold) {
            stats.scene_change = true;
            stats.diff_total = diff_total;
            return stats;
        }

        src_u_base += src_u_stride;
        src_v_base += src_v_stride;
        prev_u_base += prev_u_stride;
        prev_v_base += prev_v_stride;
    }

    stats.diff_total = diff_total;
    return stats;
}

static Cnr3SceneChangeStats detect_cnr3_scene_change(
    const Cnr3Data* d,
    const VSFrame* src,
    const VSFrame* prev_output,
    int chroma_width,
    int chroma_height,
    int bytes_per_sample,
    const std::vector<int>& current_luma,
    const std::vector<int>& previous_luma,
    const VSAPI* vsapi
) {
    /*
        Dispatch the vscnr2-style scene-change detector by storage width.
    */
    if (bytes_per_sample == 1) {
        return detect_cnr3_scene_change_u8(
            d,
            src,
            prev_output,
            chroma_width,
            chroma_height,
            current_luma,
            previous_luma,
            vsapi
        );
    }

    if (bytes_per_sample == 2) {
        return detect_cnr3_scene_change_u16(
            d,
            src,
            prev_output,
            chroma_width,
            chroma_height,
            current_luma,
            previous_luma,
            vsapi
        );
    }

    Cnr3SceneChangeStats stats;
    stats.diff_max = (d != nullptr) ? d->scene_change_threshold : 0;
    return stats;
}

static const std::vector<int>& cnr3_get_table_for_chroma_plane(
    const Cnr3Data* d,
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
    Cnr3ResponseDebugStats& stats,
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
    const Cnr3Data* d,
    int frame_number,
    int plane,
    const Cnr3ResponseDebugStats& stats
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
    Cnr3BlendDebugStats& stats,
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
    }
    else {
        if (weight < stats.weight_min) {
            stats.weight_min = weight;
        }

        if (weight > stats.weight_max) {
            stats.weight_max = weight;
        }
    }
}

static void cnr3_print_blend_debug_stats(
    const Cnr3Data* d,
    int frame_number,
    int plane,
    const Cnr3BlendDebugStats& stats
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

static void cnr3_print_scene_change_debug_stats(
    const Cnr3Data* d,
    int frame_number,
    const Cnr3SceneChangeStats& stats
) {
    /*
        Print compact frame-level scene-change diagnostics.

        Normal non-scene frames are deliberately quiet.

        A line is printed only when:
            - scene-change detection fires, or
            - diff_total reaches at least 80% of diff_max.

        The near-threshold case is useful while tuning scdthr because camera
        wobble, zooms, field jitter, pans, or large object motion can approach
        the vscnr2-style scene-change threshold without being true edit cuts.
    */
    if (d == nullptr || !d->debug) {
        return;
    }

    if (!stats.evaluated) {
        /*
            Verbose diagnostic. Usually disabled because frame 0 and unsupported
            cases are expected to be quiet during normal testing.

        cnr3_debug_printf(
            d->debug,
            "CNR3 debug: instance=%d, frame=%d, scene-change stats: not evaluated.\n",
            d->instance_id,
            frame_number
        );
        */

        return;
    }

    const bool near_scene_change_threshold =
        stats.diff_max > 0 &&
        stats.diff_total >= (stats.diff_max * 80) / 100;

    if (!stats.scene_change && !near_scene_change_threshold) {
        return;
    }

    cnr3_debug_printf(
        d->debug,
        "CNR3 debug: instance=%d, frame=%d, scene-change stats: "
        "rows=%d, samples=%lld, diff_total=%lld, diff_max=%lld, "
        "threshold_percent=%.2f%%, scene_change=%d, scene_chroma=%d\n",
        d->instance_id,
        frame_number,
        stats.evaluated_rows,
        static_cast<long long>(stats.evaluated_samples),
        static_cast<long long>(stats.diff_total),
        static_cast<long long>(stats.diff_max),
        stats.diff_max > 0
        ? (
            100.0 *
            static_cast<double>(stats.diff_total) /
            static_cast<double>(stats.diff_max)
            )
        : 0.0,
        stats.scene_change ? 1 : 0,
        d->scene_chroma ? 1 : 0
    );
}

static void process_cnr3_chroma_plane_u8(
    const Cnr3Data* d,
    int frame_number,
    const VSFrame* src,
    const VSFrame* prev_output,
    VSFrame* dst,
    int plane,
    const std::vector<int>& current_luma,
    const std::vector<int>& previous_luma,
    const VSAPI* vsapi
) {
    /*
        Per-sample chroma processing loop for 8-bit integer clips.

        For each U/V sample, this path:
            - reads current source chroma
            - reads previous filtered chroma when frame_number > 0
            - calculates signed current-vs-previous chroma difference
            - uses vscnr2-style downsampled-luma buffers for Y difference
            - looks up Y and U/V response-table values
            - optionally blends previous filtered chroma with current source
              chroma when d->blend is true

        Frame 0 remains source-copy because there is no previous filtered
        output frame.

        blend=false is retained for maintenance/testing. It keeps the read,
        difference, table, and diagnostic paths active while forcing chroma
        output to pass through unchanged.
    */
    if (d == nullptr || src == nullptr || dst == nullptr || vsapi == nullptr) {
        return;
    }

    const uint8_t* srcp = vsapi->getReadPtr(src, plane);
    uint8_t* dstp = vsapi->getWritePtr(dst, plane);

    const uint8_t* prevp =
        (frame_number > 0 && prev_output != nullptr) ?
        vsapi->getReadPtr(prev_output, plane) :
        nullptr;

    const ptrdiff_t src_stride = vsapi->getStride(src, plane);
    const ptrdiff_t dst_stride = vsapi->getStride(dst, plane);

    const ptrdiff_t prev_stride =
        (prev_output != nullptr) ? vsapi->getStride(prev_output, plane) : 0;

    const int plane_width = vsapi->getFrameWidth(src, plane);
    const int plane_height = vsapi->getFrameHeight(src, plane);

    const std::vector<int>& chroma_table =
        cnr3_get_table_for_chroma_plane(d, plane);

    const int64_t max_possible_blend_weight =
        calculate_cnr3_max_possible_blend_weight(
            d,
            chroma_table
        );

    Cnr3ResponseDebugStats response_stats;
    Cnr3BlendDebugStats blend_stats;

    for (int y = 0; y < plane_height; ++y) {
        const uint8_t* src_row = srcp;
        uint8_t* dst_row = dstp;

        const uint8_t* prev_row =
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
                        force current-source chroma output while keeping
                        diagnostics active.

                    blend=true and previous output is available:
                        enable the vscnr2-style recursive chroma blend, using
                        previous filtered output as history.

                Frame 0 writes current-source chroma because prev_row is null.
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
                    cnr3_clamp_int(blended_chroma, 0, d->sample_peak)
                    );
            }
            else {
                dst_row[x] = current_chroma;
            }
        }

        srcp += src_stride;
        dstp += dst_stride;

        if (prevp != nullptr) {
            prevp += prev_stride;
        }
    }

    /*
        Verbose per-plane response/blend diagnostics.

        Useful when tuning response tables or checking blend strength. Normally
        too noisy now that scene-change detection has its own frame-level debug.

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
    */
}

static void process_cnr3_chroma_plane_u16(
    const Cnr3Data* d,
    int frame_number,
    const VSFrame* src,
    const VSFrame* prev_output,
    VSFrame* dst,
    int plane,
    const std::vector<int>& current_luma,
    const std::vector<int>& previous_luma,
    const VSAPI* vsapi
) {
    /*
        Per-sample chroma processing loop for 10/12/16-bit integer clips.

        VapourSynth stores integer formats above 8-bit in 16-bit samples, so
        this path handles all currently accepted high-bit-depth inputs.

        The high-bit-depth path follows the same structure as the 8-bit path:
            - current source chroma
            - previous filtered output chroma
            - vscnr2-style downsampled-luma Y guard
            - signed Y/U/V response-table lookups
            - optional recursive blend when d->blend is true

        Frame 0 remains source-copy because there is no previous filtered
        output frame.

        blend=false is retained for maintenance/testing. It keeps the read,
        difference, table, and diagnostic paths active while forcing chroma
        output to pass through unchanged.
    */
    if (d == nullptr || src == nullptr || dst == nullptr || vsapi == nullptr) {
        return;
    }

    const uint8_t* srcp = vsapi->getReadPtr(src, plane);
    uint8_t* dstp = vsapi->getWritePtr(dst, plane);

    const uint8_t* prevp =
        (frame_number > 0 && prev_output != nullptr) ?
        vsapi->getReadPtr(prev_output, plane) :
        nullptr;

    const ptrdiff_t src_stride = vsapi->getStride(src, plane);
    const ptrdiff_t dst_stride = vsapi->getStride(dst, plane);

    const ptrdiff_t prev_stride =
        (prev_output != nullptr) ? vsapi->getStride(prev_output, plane) : 0;

    const int plane_width = vsapi->getFrameWidth(src, plane);
    const int plane_height = vsapi->getFrameHeight(src, plane);

    const std::vector<int>& chroma_table =
        cnr3_get_table_for_chroma_plane(d, plane);

    const int64_t max_possible_blend_weight =
        calculate_cnr3_max_possible_blend_weight(
            d,
            chroma_table
        );

    Cnr3ResponseDebugStats response_stats;
    Cnr3BlendDebugStats blend_stats;

    for (int y = 0; y < plane_height; ++y) {
        const uint16_t* src_row =
            reinterpret_cast<const uint16_t*>(srcp);

        uint16_t* dst_row =
            reinterpret_cast<uint16_t*>(dstp);

        const uint16_t* prev_row =
            (prevp != nullptr) ?
            reinterpret_cast<const uint16_t*>(prevp) :
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
                        force current-source chroma output while keeping
                        diagnostics active.

                    blend=true and previous output is available:
                        enable the vscnr2-style recursive chroma blend, using
                        previous filtered output as history.

                Frame 0 writes current-source chroma because prev_row is null.
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
                    cnr3_clamp_int(blended_chroma, 0, d->sample_peak)
                    );
            }
            else {
                dst_row[x] = current_chroma;
            }
        }

        srcp += src_stride;
        dstp += dst_stride;

        if (prevp != nullptr) {
            prevp += prev_stride;
        }
    }

    /*
        Verbose per-plane response/blend diagnostics.

        Useful when tuning response tables or checking blend strength. Normally
        too noisy now that scene-change detection has its own frame-level debug.

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
    */
}

static bool process_cnr3_chroma_plane(
    const Cnr3Data* d,
    int frame_number,
    int plane,
    const VSFrame* src,
    const VSFrame* prev_output,
    VSFrame* dst,
    int shared_chroma_width,
    int shared_chroma_height,
    int bytes_per_sample,
    const std::vector<int>& current_luma,
    const std::vector<int>& previous_luma,
    const VSAPI* vsapi
) {
    /*
        Chroma-plane processing function.

        This is the stable call site for recursive CNR3 chroma stabilisation.

        For each U/V chroma plane, it:
            - validates the requested chroma plane
            - requires previous filtered output for frame N > 0
            - validates that the shared downsampled-luma buffers match this
              chroma plane's dimensions
            - dispatches to the 8-bit or 10/12/16-bit per-sample path

        The downsampled-luma buffers are built once per frame by
        process_cnr3_frame() and shared by the U and V plane paths.

        Important note:
            Do not simply copy chroma from prev_output. Since prev_output is
            the previous filtered output, copying U/V from it directly would
            recursively cause chroma to remain effectively stuck at frame 0:

                output[1].UV = output[0].UV
                output[2].UV = output[1].UV
                output[3].UV = output[2].UV

            The correct algorithm uses response-table weights to decide how
            much previous filtered chroma may be reused at each chroma sample.

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
        Frame N > 0 requires the previous filtered output frame because the
        recursive chroma blend uses output[N - 1] as its temporal history.

        blend=false does not use previous chroma for output pixels, but keeping
        the same precondition makes diagnostics and future maintenance paths
        follow the same frame-ordering contract.
    */
    if (frame_number > 0 && prev_output == nullptr) {
        return false;
    }

    const int plane_width = vsapi->getFrameWidth(src, plane);
    const int plane_height = vsapi->getFrameHeight(src, plane);

    if (
        plane_width != shared_chroma_width ||
        plane_height != shared_chroma_height
        ) {
        return false;
    }

    const size_t expected_luma_samples =
        static_cast<size_t>(shared_chroma_width) *
        static_cast<size_t>(shared_chroma_height);

    if (current_luma.size() != expected_luma_samples) {
        return false;
    }

    if (
        frame_number > 0 &&
        previous_luma.size() != expected_luma_samples
        ) {
        return false;
    }

    /*
        Verbose per-plane geometry diagnostic.

        Useful when changing luma-buffer sharing, chroma-plane dimensions, or
        subsampling handling. Normally too noisy for scene-change testing.

    cnr3_debug_printf(
        d->debug && frame_number <= 2,
        "CNR3 debug: process_cnr3_chroma_plane() instance=%d, frame=%d, plane=%c, using shared downsampled-luma guard buffer: chroma=%dx%d, subsampling=%d:%d, blend=%d\n",
        d->instance_id,
        frame_number,
        plane == 1 ? 'U' : 'V',
        plane_width,
        plane_height,
        d->vi->format.subSamplingW,
        d->vi->format.subSamplingH,
        d->blend ? 1 : 0
    );
    */

    /*
        Chroma stabilisation stage.

        This version uses vscnr2-style downsampled-luma buffers at chroma
        resolution for the Y guard instead of one representative full-size luma
        sample. That is important because real-clip diagnostics showed the luma
        guard doing much of the blend gating.

        Algorithm notes:
            - frame 0 initialises from current source chroma
            - frame N > 0 reads previous filtered chroma from output[N - 1]
            - Y/U/V guard tables decide how much previous filtered chroma may
              be reused
            - mode characters 'x' and 'o' are narrow and wide guard response
              curves, not disabled and enabled switches

        Why the historical default mode="oxx" can still make sense:
            - Y uses the wider response, so luma structure does not block
              chroma stabilisation too eagerly
            - U and V use narrower responses, so chroma changes are treated
              more conservatively
            - this can give useful chroma shimmer reduction while reducing the
              risk of chroma lag, smearing, or ghosting around real motion
    */
    if (bytes_per_sample == 1) {
        process_cnr3_chroma_plane_u8(
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
        process_cnr3_chroma_plane_u16(
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

static bool process_cnr3_frame(
    const Cnr3Data* d,
    int frame_number,
    const VSFrame* src,
    VSFrame* dst,
    VSFrameContext* frameCtx,
    const VSAPI* vsapi
) {
    /*
        Frame-processing function.

        Processing structure:
            Y:
                copied unchanged, because CNR3 is a chroma stabiliser.

            U/V:
                normally routed through recursive chroma-plane stabilisation.

                If vscnr2-style scene-change detection fires, U/V are copied
                from the current source frame and recursive blending is skipped
                for that frame.

        Recursive precondition:
            frame 0 does not need a previous output frame.

            frame N > 0 must have d->cache.prev_output available, because CNR3
            uses output[N - 1] when producing output[N].

        Strict streaming note:
            until the fuller cache manager exists, frame requests must arrive
            in strictly increasing order. Use vspipe -r 1 for current tests.
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

    const VSFrame* prev_output = d->cache.prev_output;

    if (frame_number == 0) {
        /*
            Verbose normal-path diagnostic. Keep disabled unless debugging
            frame-order or cache sequencing.

        cnr3_debug_printf(
            d->debug,
            "CNR3 debug: instance=%d, processing frame 0 using initial-copy path.\n",
            d->instance_id
        );
        */
    }
    else {
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

        /*
            Verbose normal-path diagnostic. Keep disabled unless debugging
            frame-order or cache sequencing.

        cnr3_debug_printf(
            d->debug,
            "CNR3 debug: instance=%d, processing frame=%d using recursive previous-output path.\n",
            d->instance_id,
            frame_number
        );
        */
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

    /*
        Build downsampled-luma guard buffers once per output frame and share
        them between U and V.

        Plane 1 is used as the reference chroma geometry. For ordinary planar
        YUV formats, U and V should have identical dimensions. Check that
        explicitly before sharing the buffers so future format changes fail
        clearly instead of silently using the wrong indexing.
    */
    const int chroma_width = vsapi->getFrameWidth(src, 1);
    const int chroma_height = vsapi->getFrameHeight(src, 1);

    const int v_chroma_width = vsapi->getFrameWidth(src, 2);
    const int v_chroma_height = vsapi->getFrameHeight(src, 2);

    if (
        chroma_width != v_chroma_width ||
        chroma_height != v_chroma_height
        ) {
        vsapi->setFilterError(
            "CNR3: internal error: U and V plane dimensions do not match.",
            frameCtx
        );
        return false;
    }

    std::vector<int> current_luma;
    std::vector<int> previous_luma;

    if (!build_cnr3_downsampled_luma_buffer(
        d,
        src,
        chroma_width,
        chroma_height,
        bytes_per_sample,
        current_luma,
        vsapi
    )) {
        vsapi->setFilterError(
            "CNR3: internal error while building current downsampled-luma buffer.",
            frameCtx
        );
        return false;
    }

    if (frame_number > 0) {
        if (!build_cnr3_downsampled_luma_buffer(
            d,
            prev_output,
            chroma_width,
            chroma_height,
            bytes_per_sample,
            previous_luma,
            vsapi
        )) {
            vsapi->setFilterError(
                "CNR3: internal error while building previous downsampled-luma buffer.",
                frameCtx
            );
            return false;
        }
    }

    if (frame_number > 0) {
        const Cnr3SceneChangeStats scene_stats =
            detect_cnr3_scene_change(
                d,
                src,
                prev_output,
                chroma_width,
                chroma_height,
                bytes_per_sample,
                current_luma,
                previous_luma,
                vsapi
            );

        cnr3_print_scene_change_debug_stats(
            d,
            frame_number,
            scene_stats
        );

        if (scene_stats.scene_change) {
            /*
                vscnr2 returns the current source frame unchanged when scene
                change detection fires. CNR3 has already copied Y unchanged,
                so copy U and V from the current source and skip recursive
                chroma blending for this output frame.
            */
            cnr3_debug_printf(
                d->debug,
                "CNR3 debug: instance=%d, frame=%d, scene change detected; "
                "copying current source chroma and skipping recursive blend.\n",
                d->instance_id,
                frame_number
            );

            copy_plane_bytes(
                src,
                dst,
                1,
                bytes_per_sample,
                vsapi
            );

            copy_plane_bytes(
                src,
                dst,
                2,
                bytes_per_sample,
                vsapi
            );

            return true;
        }
    }

    if (!process_cnr3_chroma_plane(
        d,
        frame_number,
        1,
        src,
        prev_output,
        dst,
        chroma_width,
        chroma_height,
        bytes_per_sample,
        current_luma,
        previous_luma,
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
        chroma_width,
        chroma_height,
        bytes_per_sample,
        current_luma,
        previous_luma,
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
static void VS_CC cnr3_free(
    void* instanceData,
    VSCore* core,
    const VSAPI* vsapi
) {
    (void)core;

    Cnr3Data* d = static_cast<Cnr3Data*>(instanceData);

    if (d != nullptr) {
        if (d->node != nullptr) {
            vsapi->freeNode(d->node);
            d->node = nullptr;
        }

        cnr3_debug_print_cache_manager_v005_summary(
            d,
            "before cnr3_free cleanup"
        );

        cnr3_cache_clear(d->cache, vsapi);

        if (!cnr3_cache_manager_clear(d->cache_manager_v005, vsapi)) {
            cnr3_debug_printf(
                d->debug,
                "CNR3 debug: instance=%d, cache_manager_v005 clear failed during cnr3_free.\n",
                d->instance_id
            );
        }

        delete d;
    }
}

static const VSFrame* VS_CC cnr3_get_frame(
    int n,
    int activationReason,
    void* instanceData,
    void** frameData,
    VSFrameContext* frameCtx,
    VSCore* core,
    const VSAPI* vsapi
) {
    (void)frameData;

    Cnr3Data* d = static_cast<Cnr3Data*>(instanceData);

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

        const VSFrame* src = vsapi->getFrameFilter(n, d->node, frameCtx);

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

        VSFrame* dst = vsapi->newVideoFrame(
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

        if (!process_cnr3_frame(
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

        /*
            Verbose normal-path cache diagnostic. Keep disabled unless debugging
            strict streaming, cache ownership, or future cache-manager behaviour.

        cnr3_debug_printf(
            d->debug,
            "CNR3 debug: instance=%d, processed frame: frame=%d, new_next_needed=%d, stored_prev_output=%s\n",
            d->instance_id,
            n,
            d->cache.next_needed,
            d->cache.prev_output != nullptr ? "yes" : "no"
        );
        */

        vsapi->freeFrame(src);

        return dst;
    }
    return nullptr;
}
// -----------------------------------------------------------------------------
// END CNR3 cache manager
// -----------------------------------------------------------------------------

static void VS_CC cnr3_create(
    const VSMap* in,
    VSMap* out,
    void* userData,
    VSCore* core,
    const VSAPI* vsapi
) {
    (void)userData;

    Cnr3Data* data = new Cnr3Data();
    Cnr3Data& local = *data;

    // an ID to identify and track instances
    local.instance_id = g_cnr3_next_instance_id.fetch_add(1);

    int err = 0;
    local.node = vsapi->mapGetNode(in, "clip", 0, &err);

    if (err || local.node == nullptr) {
        vsapi->mapSetError(out, "CNR3: clip is required.");
        delete data;
        return;
    }

    local.vi = vsapi->getVideoInfo(local.node);

    if (local.vi == nullptr) {
        vsapi->freeNode(local.node);
        local.node = nullptr;
        vsapi->mapSetError(out, "CNR3: failed to get video info.");
        delete data;
        return;
    }

    if (!validate_cnr3_format(local.vi, out, vsapi)) {
        vsapi->freeNode(local.node);
        local.node = nullptr;
        delete data;
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
        local.node = nullptr;
        vsapi->mapSetError(out, "CNR3: mode must be a 3-character string, for example \"oxx\".");
        delete data;
        return;
    }

    for (const char c : local.mode) {
        if (c != 'o' && c != 'x') {
            vsapi->freeNode(local.node);
            local.node = nullptr;
            vsapi->mapSetError(out, "CNR3: mode may contain only 'o' and 'x' characters.");
            delete data;
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
        local.node = nullptr;
        vsapi->mapSetError(out, "CNR3: threshold parameters must be non-negative.");
        delete data;
        return;
    }

    if (local.scdthr < 0.0) {
        vsapi->freeNode(local.node);
        local.node = nullptr;
        vsapi->mapSetError(out, "CNR3: scdthr must be non-negative.");
        delete data;
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

    /*
        vscnr2-style scene-change threshold.

        vscnr2 uses:
            max_pixel_diff = scene_chroma
                ? (219 + 224 * 2) >> (subsw + subsh)
                : 219

            diff_max = (
                scdthr * width * height * max_pixel_diff / 100.0
            ) << (depth - 8)

        CNR3 keeps that model. The threshold is compared against a per-frame
        accumulated diff_total during process_cnr3_frame().
    */
    const int subsampling_shift =
        local.vi->format.subSamplingW +
        local.vi->format.subSamplingH;

    const int max_pixel_diff =
        (!local.scene_chroma)
        ? 219
        : ((219 + (224 * 2)) >> subsampling_shift);

    local.scene_change_threshold =
        static_cast<int64_t>(
            (
                local.scdthr *
                static_cast<double>(local.vi->width) *
                static_cast<double>(local.vi->height) *
                static_cast<double>(max_pixel_diff)
                ) /
            100.0
            ) << (local.bits_per_sample - 8);

    if (!build_cnr3_lookup_tables(local, out, vsapi)) {
        vsapi->freeNode(local.node);
        local.node = nullptr;
        delete data;
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
            "mode=%s, scdthr=%f, scene_chroma=%d, scene_change_threshold=%lld, blend=%d\n",
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
            static_cast<long long>(local.scene_change_threshold),
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

    cnr3_debug_print_cache_manager_v005_summary(
        data,
        "after cnr3_create configuration before createVideoFilter"
    );

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
    VSPlugin* plugin,
    const VSPLUGINAPI* vspapi
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


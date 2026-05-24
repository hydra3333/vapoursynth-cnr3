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

        Table index:
            absolute sample difference, from 0 to sample_peak.

        Table value:
            0..256 weighting value.
    */
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
        "CNR3 debug: %s: requested=%d, next_needed=%d, gap=%d, prev_output=%s\n",
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

static void build_cnr3_weight_table(
    std::vector<int> &table,
    int sample_peak,
    int threshold_low,
    int threshold_high,
    bool enabled
) {
    /*
        Build a soft threshold table for one guard plane.

        This is intentionally simple and explicit at this stage.

        If the guard is disabled by mode:
            every difference gets full weight.

        If enabled:
            abs(diff) <= threshold_low:
                full weight, 256

            abs(diff) >= threshold_high:
                zero weight, 0

            threshold_low < abs(diff) < threshold_high:
                raised-cosine fade from 256 down to 0

        This preserves the core Cnr2/vscnr2 idea: small differences are safe
        for chroma stabilisation, large differences are not, and the transition
        should be gradual rather than a hard binary cutoff.
    */

    table.assign(static_cast<size_t>(sample_peak) + 1U, 0);

    if (!enabled) {
        std::fill(table.begin(), table.end(), 256);
        return;
    }

    threshold_low = clamp_int(threshold_low, 0, sample_peak);
    threshold_high = clamp_int(threshold_high, 0, sample_peak);

    if (threshold_high < threshold_low) {
        std::swap(threshold_high, threshold_low);
    }

    if (threshold_high == threshold_low) {
        for (int diff = 0; diff <= sample_peak; ++diff) {
            table[static_cast<size_t>(diff)] = (diff <= threshold_low) ? 256 : 0;
        }

        return;
    }

    constexpr double pi = 3.141592653589793238462643383279502884;

    for (int diff = 0; diff <= sample_peak; ++diff) {
        if (diff <= threshold_low) {
            table[static_cast<size_t>(diff)] = 256;
        } else if (diff >= threshold_high) {
            table[static_cast<size_t>(diff)] = 0;
        } else {
            const double position =
                static_cast<double>(diff - threshold_low) /
                static_cast<double>(threshold_high - threshold_low);

            /*
                Raised cosine:
                    position 0.0 -> 256
                    position 1.0 -> 0
            */
            const double weight = 0.5 * (1.0 + std::cos(position * pi));
            table[static_cast<size_t>(diff)] =
                clamp_int(static_cast<int>(weight * 256.0 + 0.5), 0, 256);
        }
    }
}

static bool build_cnr3_lookup_tables(
    Cnr3Data &d,
    VSMap *out,
    const VSAPI *vsapi
) {
    /*
        mode is a 3-character string:
            mode[0] controls the luma/Y guard
            mode[1] controls the U/chroma guard
            mode[2] controls the V/chroma guard

        For now:
            'o' means enabled
            'x' means disabled

        This follows the public Cnr2/vscnr2-style mode convention but keeps
        the implementation explicit for maintainability.
    */

    if (d.mode.size() != 3) {
        vsapi->mapSetError(out, "CNR3: internal error: mode must contain exactly 3 characters.");
        return false;
    }

    const bool enable_y = (d.mode[0] == 'o');
    const bool enable_u = (d.mode[1] == 'o');
    const bool enable_v = (d.mode[2] == 'o');

    build_cnr3_weight_table(
        d.table_y,
        d.sample_peak,
        d.ln_scaled,
        d.lm_scaled,
        enable_y
    );

    build_cnr3_weight_table(
        d.table_u,
        d.sample_peak,
        d.un_scaled,
        d.um_scaled,
        enable_u
    );

    build_cnr3_weight_table(
        d.table_v,
        d.sample_peak,
        d.vn_scaled,
        d.vm_scaled,
        enable_v
    );

    if (
        d.table_y.size() != static_cast<size_t>(d.sample_peak) + 1U ||
        d.table_u.size() != static_cast<size_t>(d.sample_peak) + 1U ||
        d.table_v.size() != static_cast<size_t>(d.sample_peak) + 1U
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
        cnr3_debug_print_cache_state(
            d,
            "arInitial/request source frame",
            n
        );

        vsapi->requestFrameFilter(n, d->node, frameCtx);
        return nullptr;
    }

    if (activationReason == arAllFramesReady) {
        cnr3_debug_print_cache_state(
            d,
            "arAllFramesReady/entry",
            n
        );

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
                "CNR3 debug: out-of-order frame request: requested=%d, next_needed=%d, gap=%d, prev_output=%s\n",
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
                "requested=%d, next_needed=%d, gap=%d, prev_output=%s.",
                requested_frame,
                next_needed,
                gap,
                d->cache.prev_output != nullptr ? "yes" : "no"
            );

            vsapi->freeFrame(src);
            vsapi->setFilterError(error_message, frameCtx);
            return nullptr;
        }

        cnr3_debug_printf(
            d->debug,
            "CNR3 debug: in-order frame accepted: requested=%d, next_needed=%d, prev_output=%s\n",
            n,
            d->cache.next_needed,
            d->cache.prev_output != nullptr ? "yes" : "no"
        );

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

        const int bytes_per_sample = (d->bits_per_sample + 7) / 8;

        copy_all_planes_unchanged(
            src,
            dst,
            bytes_per_sample,
            vsapi
        );

        cnr3_cache_store_output_frame(
            d->cache,
            dst,
            n,
            vsapi
        );

        cnr3_debug_printf(
            d->debug,
            "CNR3 debug: processed frame: frame=%d, new_next_needed=%d, stored_prev_output=%s\n",
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
        const int y_mid = (local.ln_scaled + local.lm_scaled) / 2;
        const int u_mid = (local.un_scaled + local.um_scaled) / 2;
        const int v_mid = (local.vn_scaled + local.vm_scaled) / 2;

        cnr3_debug_printf(
            local.debug,
            "CNR3 debug: format=%d-bit YUV, peak=%d, "
            "ln=%d->%d, lm=%d->%d, "
            "un=%d->%d, um=%d->%d, "
            "vn=%d->%d, vm=%d->%d, "
            "mode=%s, scdthr=%.6f, scene_chroma=%d\n",
            local.bits_per_sample,
            local.sample_peak,
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
            "CNR3 debug: table samples: "
            "Y[0]=%d, Y[%d]=%d, Y[%d]=%d, Y[%d]=%d; "
            "U[0]=%d, U[%d]=%d, U[%d]=%d, U[%d]=%d; "
            "V[0]=%d, V[%d]=%d, V[%d]=%d, V[%d]=%d\n",
            local.table_y[0],
            local.ln_scaled,
            local.table_y[static_cast<size_t>(local.ln_scaled)],
            y_mid,
            local.table_y[static_cast<size_t>(y_mid)],
            local.lm_scaled,
            local.table_y[static_cast<size_t>(local.lm_scaled)],

            local.table_u[0],
            local.un_scaled,
            local.table_u[static_cast<size_t>(local.un_scaled)],
            u_mid,
            local.table_u[static_cast<size_t>(u_mid)],
            local.um_scaled,
            local.table_u[static_cast<size_t>(local.um_scaled)],

            local.table_v[0],
            local.vn_scaled,
            local.table_v[static_cast<size_t>(local.vn_scaled)],
            v_mid,
            local.table_v[static_cast<size_t>(v_mid)],
            local.vm_scaled,
            local.table_v[static_cast<size_t>(local.vm_scaled)]
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

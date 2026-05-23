/*
    CNR3 - experimental VapourSynth API4 chroma stabiliser

    This is the initial API4 skeleton. It intentionally returns the source
    clip unchanged. Its purpose is to prove that the project can build a
    loadable VapourSynth plugin DLL before the recursive CNR3 algorithm is
    connected.

    SPDX-License-Identifier: AGPL-3.0-or-later
*/

#include <cstdint>
#include <cstring>
#include <string>
#include <cstdio>

#include "VapourSynth4.h"
#include "VSHelper4.h"

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

    double scdthr = 10.0;

    bool scene_chroma = false;
    bool debug = false;
};

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

        delete d;
    }
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
    (void)core;

    Cnr3Data *d = static_cast<Cnr3Data *>(instanceData);

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
        return nullptr;
    }

    if (activationReason == arAllFramesReady) {
        const VSFrame *src = vsapi->getFrameFilter(n, d->node, frameCtx);

        if (src == nullptr) {
            vsapi->setFilterError("CNR3: failed to retrieve source frame.", frameCtx);
            return nullptr;
        }

        /*
            Pass-through skeleton.

            Returning this frame reference transfers it to VapourSynth.
            Do not free it here.
        */
        return src;
    }

    return nullptr;
}

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

    local.bits_per_sample = local.vi->format.bitsPerSample;
    local.sample_peak = (1 << local.bits_per_sample) - 1;

    local.ln_scaled = scale_8bit_parameter_to_bit_depth(local.ln, local.bits_per_sample);
    local.lm_scaled = scale_8bit_parameter_to_bit_depth(local.lm, local.bits_per_sample);
    local.un_scaled = scale_8bit_parameter_to_bit_depth(local.un, local.bits_per_sample);
    local.um_scaled = scale_8bit_parameter_to_bit_depth(local.um, local.bits_per_sample);
    local.vn_scaled = scale_8bit_parameter_to_bit_depth(local.vn, local.bits_per_sample);
    local.vm_scaled = scale_8bit_parameter_to_bit_depth(local.vm, local.bits_per_sample);

    if (local.debug) {
        std::fprintf(
            stderr,
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
    }

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

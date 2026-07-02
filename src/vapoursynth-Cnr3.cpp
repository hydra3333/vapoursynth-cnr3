/*
    CNR3 - VapourSynth API4 temporal chroma stabiliser, based on the
    venerable CNR2/vscnr2.

    CNR3 is a redevelopment inspired by the CNR2/vscnr2 recursive temporal
    chroma-stabilisation model, using VapourSynth API4 only.

    Recursive processing and VapourSynth scheduling:
        The CNR2/vscnr2 algorithm is inherently temporal and recursive, which
        requires in-order serial frame processing in its original execution
        model.

        Processing SOURCE frame N into OUTPUT frame N requires access to the
        already-filtered OUTPUT arising from previously processed SOURCE frame
        N - 1:

            output[N] depends on both source[N] and output[N - 1]

        That makes the CNR2/vscnr2 algorithm naturally serial.

        Older VapourSynth-era recursive filters could sometimes rely on
        compatibility-style scheduling parameters and assumptions. In
        particular, fmFrameState meant only one thread would call a filter's
        getFrame function at a time and only one frame would be processed at a
        time.

        However, VapourSynth API4 documentation says fmFrameState is for
        compatibility only and MUST NOT BE USED IN NEW FILTERS. VapourSynth is
        also moving away from API3: current Windows binaries no longer
        distribute the API R3 headers, and general API R3 plugin support is
        only retained for now.

        CNR3 therefore HAS TO CHANGE from CNR2/vscnr2-era fmFrameState and API3
        assumptions in order to survive as a maintainable modern VapourSynth
        filter.

        CNR3 will initially target fmUnordered integration, then move through
        fmParallelRequests if approved by the phase plan, and finally move to
        fmParallel.

    Project scope:
        - VapourSynth API4 only.
        - Integer planar YUV only.
        - Primary target material: analogue video captures with temporal chroma
          instability, such as VHS/VHS-C and related restoration sources.
        - Correct recursive chroma-stabilisation behaviour before parallel
          performance work.

    Load-bearing recursive fact:
        Modern VapourSynth scheduling can and does request frames out of
        display order, so CNR3 must not rely on display-order calls, a single
        previous-output variable, or strict serial predecessor state.

        CNR3 is not a stateless image filter.

        To compute OUTPUT frame N, the filter needs:
            - SOURCE[N]
            - already-filtered OUTPUT[N - 1]

        The predecessor is the previous filtered OUTPUT, not SOURCE[N - 1].

            output[N] depends on source[N] and output[N - 1]

        This recursive dependency is the central reason CNR3 needs its own
        cache/recovery architecture.

    CMS07.0 restart architecture:
        CMS07.0 holds the new cache design and proof path.

        The first implementation milestone is the cache-manager core in
        isolation, before VapourSynth getFrame wiring and before pixel-layer
        salvage.

        The CMS07 cache manager is a correctness subsystem. It owns output
        frame reference slots, ordered index state, consumer-held pins,
        per-invocation pin-lists, checkpoint flags, hot zones, prune policy,
        recovery planning, validation, and cache diagnostics.

        It must prove:
            - pin/unpin balance is zero;
            - lookup-reference accounting balances;
            - no VSFrame reference leaks;
            - no double-free;
            - eviction never selects a pinned, checkpoint, or in-zone slot;
            - shutdown clear releases all cached references, with a warning on
              any non-zero pin count.

    File role:
        This translation unit is the VapourSynth integration layer.

        It should remain thin. Its long-term responsibilities are plugin
        registration, parameter parsing, instance creation/destruction,
        VapourSynth source-request/retrieve lifecycle, frameData allocation and
        cleanup, error mapping, and calls into the cache and pixel-processing
        layers after those layers are proven.

        It must not contain cache algorithms, recovery algorithms, prune policy,
        pixel loops, response-table construction, or memory-diagnostic internals.

        During CMS07-K.1D this file contains the temporary live getFrame frame-0
        proof path. It proves frame-0 source-verbatim fresh-start output
        creation, cache store, and return plumbing. Nonzero frames are refused
        until predecessor-present processing is wired.

    Filter-mode posture:
        The final operational target is fmParallel.

        Interim development may pass through safer or narrower stages, but code
        and design must not introduce assumptions that block eventual safe
        fmParallel operation unless the exception is explicit, temporary,
        justified, and recorded.

        Do not reintroduce fmFrameState compatibility assumptions.
        Do not reintroduce old strict-streaming output authority.
        Do not rely on call order as proof that OUTPUT[N - 1] exists.

    Pixel-layer boundary:
        Pixel/frame processing is a separate layer.

        When the pixel layer is later salvaged, it receives SOURCE[N] and an
        explicit previous OUTPUT frame supplied by the cache/recovery layer.
        The pixel layer must not find, cache, pin, recover, prune, schedule, or
        substitute predecessor frames.

        CNR2/vscnr2 may be used as pixel-maths guidance only. CNR3 must not
        adopt CNR2's serialized recovery/predecessor approximation. In
        particular, CNR3 must not substitute SOURCE[N - 1] for the required
        previous filtered OUTPUT[N - 1].

    Diagnostics and output:
        CNR3 must never write diagnostics, debug messages, status messages, or
        summaries to stdout. In common VapourSynth pipelines, stdout may carry
        frame data.

        Diagnostic and debug text must go to stderr.

        VapourSynth creation errors must use mapSetError().
        VapourSynth frame-processing errors must use setFilterError().

        Ongoing diagnostics use DIAG_* gates and observe only.
        Temporary proof scaffolds use SCAFFOLD_* gates, are clearly bounded,
        and must not become required for production correctness.

    SPDX-License-Identifier: AGPL-3.0-or-later
*/

#include "cnr3_build_config.h"
#include "cnr3_plugin_internal.h"

#include "cnr3_instance_config.h"
#include "cnr3_response_tables.h"

#include "VapourSynth4.h"
#include "VSHelper4.h"

#include <new>

namespace {

inline constexpr int CNR3_K1E2_PROOF_DEFAULT_THRESHOLD_8BIT = 255;
inline constexpr int CNR3_K1E2_PROOF_DEFAULT_STRENGTH_8BIT = 255;

Cnr3ResponseTableConfig cnr3_make_k1e2_proof_default_response_table_config(
    int sample_peak
) noexcept {
    Cnr3ResponseTableConfig config{};
    config.sample_peak = sample_peak;

    /*
        CMS07-K.1E.2 proof/default response-table config: enough to drive
        the proven P.11B live path; final user option parsing and full
        default-policy surface are deferred to the post-keystone
        instance-config/error-surface step.
    */
    config.y.threshold_8bit = CNR3_K1E2_PROOF_DEFAULT_THRESHOLD_8BIT;
    config.y.strength_8bit = CNR3_K1E2_PROOF_DEFAULT_STRENGTH_8BIT;
    config.y.curve = Cnr3ResponseCurveKind::narrow;

    config.u.threshold_8bit = CNR3_K1E2_PROOF_DEFAULT_THRESHOLD_8BIT;
    config.u.strength_8bit = CNR3_K1E2_PROOF_DEFAULT_STRENGTH_8BIT;
    config.u.curve = Cnr3ResponseCurveKind::narrow;

    config.v.threshold_8bit = CNR3_K1E2_PROOF_DEFAULT_THRESHOLD_8BIT;
    config.v.strength_8bit = CNR3_K1E2_PROOF_DEFAULT_STRENGTH_8BIT;
    config.v.curve = Cnr3ResponseCurveKind::narrow;

    return config;
}

Cnr3Status cnr3_initialise_k1e2_live_pixel_config(
    const VSVideoInfo& video_info,
    Cnr3FilterData& data
) noexcept {
    const VSVideoFormat* format = &video_info.format;

    if (format == nullptr ||
        format->colorFamily != cfYUV ||
        format->sampleType != stInteger ||
        format->numPlanes != 3) {
        return Cnr3Status::unsupported_format;
    }

    if (format->bitsPerSample < 8 || format->bitsPerSample > 16) {
        return Cnr3Status::unsupported_format;
    }

    if (format->subSamplingW < 0 || format->subSamplingW > 1 ||
        format->subSamplingH < 0 || format->subSamplingH > 1) {
        return Cnr3Status::unsupported_format;
    }

    data.bits_per_sample = format->bitsPerSample;
    data.sub_sampling_w = format->subSamplingW;
    data.sub_sampling_h = format->subSamplingH;
    data.scene_change_scdthr = CNR3_P11C_DEFAULT_SCDTHR;

    Cnr3Status status = cnr3_make_scene_change_config_from_vscnr2_scdthr(
        data.scene_change_scdthr,
        video_info.width,
        video_info.height,
        data.bits_per_sample,
        data.sub_sampling_w,
        data.sub_sampling_h,
        false,
        data.scene_change_config
    );

    if (!cnr3_status_is_ok(status)) {
        return status;
    }

    const int sample_peak = (1 << data.bits_per_sample) - 1;
    const Cnr3ResponseTableConfig table_config =
        cnr3_make_k1e2_proof_default_response_table_config(sample_peak);

    return build_cnr3_response_tables(table_config, data.response_tables);
}

const VSFrame* VS_CC cnr3_get_frame_keystone_live_k1f_proof(
    int n,
    int activation_reason,
    void* instance_data,
    void** frame_data,
    VSFrameContext* frame_ctx,
    VSCore* core,
    const VSAPI* vsapi
) {
    Cnr3FilterData* data = static_cast<Cnr3FilterData*>(instance_data);

    if (data == nullptr || data->source == nullptr || frame_data == nullptr || core == nullptr || vsapi == nullptr) {
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1F cache-hit proof: invalid getFrame state."
        );
        return nullptr;
    }

    if (activation_reason == arError) {
        (void)cnr3_discard_frame_data_with_cache(frame_data, data->output_cache);
        return nullptr;
    }

    if (activation_reason == arAllFramesReady) {
        return cnr3_arAllFramesReady(
            n,
            *data,
            frame_data,
            frame_ctx,
            core,
            vsapi
        );
    }

    if (activation_reason == arInitial) {
        return cnr3_arInitial(
            n,
            *data,
            frame_data,
            frame_ctx,
            core,
            vsapi
        );
    }

    return nullptr;
}

void VS_CC cnr3_free_filter(
    void* instance_data,
    VSCore* core,
    const VSAPI* vsapi
) {
    (void)core;

    Cnr3FilterData* data = static_cast<Cnr3FilterData*>(instance_data);

    if (data == nullptr) {
        return;
    }

#if defined(CNR3_DIAG_PRINT_DSUM01_REQUEST_ORDER)
    cnr3_diag_dsum01_write_request_order_summary_to_stderr(
        data->config.instance_id,
        data->dsum01_request_order
    );
#endif

    if (data->source != nullptr && vsapi != nullptr) {
        vsapi->freeNode(data->source);
        data->source = nullptr;
    }

    delete data;
}

void VS_CC cnr3_create_filter(
    const VSMap* in,
    VSMap* out,
    void* user_data,
    VSCore* core,
    const VSAPI* vsapi
) {
    (void)user_data;

    if (in == nullptr || out == nullptr || core == nullptr || vsapi == nullptr) {
        if (out != nullptr && vsapi != nullptr) {
            vsapi->mapSetError(out, "CNR3: invalid create-filter state.");
        }
        return;
    }

    int clip_error = 0;
    VSNode* source = vsapi->mapGetNode(in, "clip", 0, &clip_error);

    if (clip_error != peSuccess || source == nullptr) {
        vsapi->mapSetError(out, "CNR3: clip argument is required.");
        return;
    }

    const VSVideoInfo* source_info = vsapi->getVideoInfo(source);

    if (source_info == nullptr) {
        vsapi->freeNode(source);
        vsapi->mapSetError(out, "CNR3: clip must be a video node.");
        return;
    }

    Cnr3FilterData* data = new (std::nothrow) Cnr3FilterData{};

    if (data == nullptr) {
        vsapi->freeNode(source);
        vsapi->mapSetError(out, "CNR3: failed to allocate filter instance data.");
        return;
    }

    data->source = source;
    data->video_info = *source_info;
    data->config = cnr3_make_default_instance_config();

    if (!cnr3_instance_config_is_valid(data->config)) {
        cnr3_free_filter(data, core, vsapi);
        vsapi->mapSetError(out, "CNR3: failed to initialise instance configuration.");
        return;
    }

    const Cnr3Status pixel_config_status = cnr3_initialise_k1e2_live_pixel_config(
        *source_info,
        *data
    );

    if (!cnr3_status_is_ok(pixel_config_status)) {
        cnr3_free_filter(data, core, vsapi);
        vsapi->mapSetError(
            out,
            "CNR3 K.1E.2 proof/default config: unsupported clip format or response-table build failure."
        );
        return;
    }

    VSFilterDependency dependencies[] = {
        { source, rpGeneral }
    };

    vsapi->createVideoFilter(
        out,
        "CNR3",
        &data->video_info,
        cnr3_get_frame_keystone_live_k1f_proof,
        cnr3_free_filter,
        fmUnordered,
        dependencies,
        1,
        data,
        core
    );
}

} // namespace

VS_EXTERNAL_API(void) VapourSynthPluginInit2(
    VSPlugin* plugin,
    const VSPLUGINAPI* vspapi
) {
    if (plugin == nullptr || vspapi == nullptr) {
        return;
    }

    vspapi->configPlugin(
        "com.hydra3333.cnr3",
        "cnr3",
        "CNR3",
        VS_MAKE_VERSION(0, 1),
        VAPOURSYNTH_API_VERSION,
        0,
        plugin
    );

    vspapi->registerFunction(
        "CNR3",
        "clip:vnode;",
        "clip:vnode;",
        cnr3_create_filter,
        nullptr,
        plugin
    );
}

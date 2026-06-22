/*
    CNR3 - VapourSynth API4 temporal chroma stabiliser, based on the
    venerable CNR2/vscnr2

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

        During CMS07-K.1C this file contains only a temporary live getFrame
        passthrough scaffold. It proves VapourSynth callback/request/return
        plumbing and must not be treated as real CNR3 filtering, cache output,
        predecessor sourcing, or old-code salvage.

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
#include "cnr3_common.h"
#include "cnr3_instance_config.h"

#include "VapourSynth4.h"
#include "VSHelper4.h"

#include <atomic>
#include <cstdio>
#include <new>

namespace {

struct Cnr3LiveGetFrameScaffoldFrameData {
    int requested_frame = CNR3_INVALID_FRAME_NUMBER;
    bool source_requested = false;
};

struct Cnr3FilterData {
    VSNode* source = nullptr;
    VSVideoInfo video_info{};
    Cnr3InstanceConfig config{};

    std::atomic<int> live_scaffold_requested_total{ 0 };
    std::atomic<int> live_scaffold_retrieved_total{ 0 };
    std::atomic<int> live_scaffold_returned_total{ 0 };
    std::atomic<int> live_scaffold_passthrough_total{ 0 };
};

/*
    The K.1C coordinator harness requires [KDT] output only when real
    getFrame processing occurs. These helpers are therefore called only from
    the live getFrame callback path, never from plugin load, registration,
    create, or free.
*/
void cnr3_trace_live_scaffold_frame(
    const Cnr3FilterData& data,
    int requested_frame,
    bool source_requested,
    bool source_retrieved,
    bool frame_returned
) noexcept {
#if defined(CNR3_KEYSTONE_DEV_TRACE) && defined(CNR3_KEYSTONE_LIVE_GETFRAME_SCAFFOLD)
    std::fprintf(
        stderr,
        "[KDT] instance=%d N=%d LIVE-SCAFFOLD-PASSTHROUGH req=%d got=%d ret=%d flag=SCAFFOLD_NOT_FILTERED\n",
        data.config.instance_id.value,
        requested_frame,
        source_requested ? 1 : 0,
        source_retrieved ? 1 : 0,
        frame_returned ? 1 : 0
    );
#else
    (void)data;
    (void)requested_frame;
    (void)source_requested;
    (void)source_retrieved;
    (void)frame_returned;
#endif
}

void cnr3_trace_live_scaffold_summary(
    const Cnr3FilterData& data
) noexcept {
#if defined(CNR3_KEYSTONE_DEV_TRACE) && defined(CNR3_KEYSTONE_LIVE_GETFRAME_SCAFFOLD)
    std::fprintf(
        stderr,
        "[KDT-SUMMARY] instance=%d live_scaffold_passthrough=%d requested=%d retrieved=%d returned=%d flag=SCAFFOLD_NOT_FILTERED\n",
        data.config.instance_id.value,
        data.live_scaffold_passthrough_total.load(std::memory_order_relaxed),
        data.live_scaffold_requested_total.load(std::memory_order_relaxed),
        data.live_scaffold_retrieved_total.load(std::memory_order_relaxed),
        data.live_scaffold_returned_total.load(std::memory_order_relaxed)
    );
#else
    (void)data;
#endif
}

void cnr3_discard_frame_data(
    void** frame_data
) noexcept {
    if (frame_data == nullptr || *frame_data == nullptr) {
        return;
    }

    delete static_cast<Cnr3LiveGetFrameScaffoldFrameData*>(*frame_data);
    *frame_data = nullptr;
}

void cnr3_set_filter_error(
    VSFrameContext* frame_ctx,
    const VSAPI* vsapi,
    const char* message
) noexcept {
    if (frame_ctx != nullptr && vsapi != nullptr && message != nullptr) {
        vsapi->setFilterError(message, frame_ctx);
    }
}

const VSFrame* VS_CC cnr3_get_frame_keystone_live_passthrough_scaffold_only(
    int n,
    int activation_reason,
    void* instance_data,
    void** frame_data,
    VSFrameContext* frame_ctx,
    VSCore* core,
    const VSAPI* vsapi
) {
    (void)core;

    Cnr3FilterData* data = static_cast<Cnr3FilterData*>(instance_data);

    if (data == nullptr || data->source == nullptr || frame_data == nullptr || vsapi == nullptr) {
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1C live scaffold: invalid getFrame state."
        );
        return nullptr;
    }

    if (activation_reason == arInitial) {
        if (*frame_data != nullptr) {
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 K.1C live scaffold: frameData was unexpectedly non-null at arInitial."
            );
            return nullptr;
        }

        Cnr3LiveGetFrameScaffoldFrameData* request_data =
            new (std::nothrow) Cnr3LiveGetFrameScaffoldFrameData{};

        if (request_data == nullptr) {
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 K.1C live scaffold: failed to allocate frameData."
            );
            return nullptr;
        }

        request_data->requested_frame = n;
        request_data->source_requested = true;
        *frame_data = request_data;

        data->live_scaffold_requested_total.fetch_add(1, std::memory_order_relaxed);
        vsapi->requestFrameFilter(n, data->source, frame_ctx);

        return nullptr;
    }

    if (activation_reason == arAllFramesReady) {
        Cnr3LiveGetFrameScaffoldFrameData* request_data =
            static_cast<Cnr3LiveGetFrameScaffoldFrameData*>(*frame_data);

        if (request_data == nullptr ||
            request_data->requested_frame != n ||
            !request_data->source_requested) {
            cnr3_discard_frame_data(frame_data);
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 K.1C live scaffold: source frame was not requested in this activation."
            );
            return nullptr;
        }

        const VSFrame* source_frame = vsapi->getFrameFilter(n, data->source, frame_ctx);

        if (source_frame == nullptr) {
            cnr3_discard_frame_data(frame_data);
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 K.1C live scaffold: source frame retrieval failed."
            );
            return nullptr;
        }

        data->live_scaffold_retrieved_total.fetch_add(1, std::memory_order_relaxed);
        data->live_scaffold_returned_total.fetch_add(1, std::memory_order_relaxed);
        data->live_scaffold_passthrough_total.fetch_add(1, std::memory_order_relaxed);

        cnr3_trace_live_scaffold_frame(
            *data,
            n,
            true,
            true,
            true
        );
        cnr3_trace_live_scaffold_summary(*data);

        cnr3_discard_frame_data(frame_data);

        /*
            CMS07-K.1C live scaffold only: this returns source[N] to prove
            VapourSynth getFrame plumbing. It is not filtered output[N], is not
            a predecessor, is never stored in the CNR3 output cache, and must be
            removed when real CNR3 output generation replaces this scaffold.
        */
        return source_frame;
    }

    if (activation_reason == arError) {
        cnr3_discard_frame_data(frame_data);
        return nullptr;
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

    VSFilterDependency dependencies[] = {
        { source, rpStrictSpatial }
    };

    vsapi->createVideoFilter(
        out,
        "CNR3",
        &data->video_info,
        cnr3_get_frame_keystone_live_passthrough_scaffold_only,
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

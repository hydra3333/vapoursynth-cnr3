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
#include "cnr3_cache_core.h"
#include "cnr3_common.h"
#include "cnr3_instance_config.h"
#include "cnr3_owned_frame_ref.h"
#include "cnr3_frame_processing.h"
#include "cnr3_response_tables.h"

#include "VapourSynth4.h"
#include "VSHelper4.h"

#include <cstdio>
#include <new>
#include <utility>

namespace {

enum class Cnr3LiveGetFrameBranch {
    none,
    cache_hit_return,
    frame0_fresh_start,
    predecessor_present_compute
};

struct Cnr3LiveGetFrameFrameData {
    Cnr3LiveGetFrameBranch branch = Cnr3LiveGetFrameBranch::none;
    int requested_frame = CNR3_INVALID_FRAME_NUMBER;
    int predecessor_frame = CNR3_INVALID_FRAME_NUMBER;
    bool source_requested = false;
    bool predecessor_pin_taken = false;
    bool cache_hit_pin_taken = false;
    Cnr3CachePinList pin_list{};
};

struct Cnr3FilterData {
    VSNode* source = nullptr;
    VSVideoInfo video_info{};
    Cnr3InstanceConfig config{};
    Cnr3OutputCacheCore output_cache{};
    Cnr3ResponseTables response_tables{};
    int bits_per_sample = 0;
    int sub_sampling_w = -1;
    int sub_sampling_h = -1;
};

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

    const int sample_peak = (1 << data.bits_per_sample) - 1;
    const Cnr3ResponseTableConfig table_config =
        cnr3_make_k1e2_proof_default_response_table_config(sample_peak);

    return build_cnr3_response_tables(table_config, data.response_tables);
}

bool cnr3_live_output_frame_is_checkpoint(
    int frame_number
) noexcept {
    return frame_number == 0 ||
        (frame_number > 0 && (frame_number % CNR3_CACHE_CHECKPOINT_INTERVAL) == 0);
}

/*
    The coordinator harness requires [KDT] output only when real getFrame
    processing occurs. These helpers are therefore called only from the live
    getFrame callback path, never from plugin load, registration, create, or
    free.
*/
void cnr3_trace_live_frame0_fresh_start(
    const Cnr3FilterData& data,
    int requested_frame,
    bool source_requested,
    bool source_retrieved,
    bool copy_frame_succeeded,
    const char* store_status_name,
    bool frame_returned
) noexcept {
#if defined(CNR3_KEYSTONE_DEV_TRACE) && defined(CNR3_KEYSTONE_LIVE_GETFRAME_FRAME0_PROOF)
    std::fprintf(
        stderr,
        "[KDT] instance=%d N=%d FRAME0-FRESH-START req=%d got=%d copyFrame=%s store=%s ret=%d flag=REAL_OUTPUT_FRAME0\n",
        data.config.instance_id.value,
        requested_frame,
        source_requested ? 1 : 0,
        source_retrieved ? 1 : 0,
        copy_frame_succeeded ? "ok" : "fail",
        store_status_name != nullptr ? store_status_name : "unknown",
        frame_returned ? 1 : 0
    );
#else
    (void)data;
    (void)requested_frame;
    (void)source_requested;
    (void)source_retrieved;
    (void)copy_frame_succeeded;
    (void)store_status_name;
    (void)frame_returned;
#endif
}

void cnr3_trace_live_predecessor_present_compute(
    const Cnr3FilterData& data,
    int requested_frame,
    int predecessor_frame,
    Cnr3Status store_status,
    const Cnr3CallerSuppliedFrameProcessSummary& process_summary
) noexcept {
#if defined(CNR3_KEYSTONE_DEV_TRACE)
    std::fprintf(
        stderr,
        "[KDT] instance=%d N=%d branch=PREDECESSOR-PRESENT-COMPUTE "
        "source=%d pred=%d pred_source=output_cache pred_lookup=hit "
        "pred_liveness_basis=pin pred_checkpoint_used_as_pin=0 "
        "pred_pin_taken=1 pred_pin_discharged=1 pred_pin_balance=0 "
        "pred_ref_carried_across_gap=0 "
        "pred_compute_ref_acquired=1 pred_compute_ref_released=1 pred_compute_ref_balance=0 "
        "p11b_called=1 p11c_called=0 scene_change_deferred=1 "
        "output_store=%s output_return_transferred=1 "
        "frame_processed=%d luma_samples_copied=%d "
        "chroma_u_samples_processed=%d chroma_v_samples_processed=%d "
        "memcpy_byte_view_path_used=%d typed_row_pointer_optimization_deferred=%d "
        "first_u_output_sample=%d last_u_output_sample=%d "
        "first_v_output_sample=%d last_v_output_sample=%d\n",
        data.config.instance_id.value,
        requested_frame,
        requested_frame,
        predecessor_frame,
        cnr3_status_name(store_status),
        process_summary.frame_processed ? 1 : 0,
        process_summary.luma_samples_copied,
        process_summary.chroma_u_samples_processed,
        process_summary.chroma_v_samples_processed,
        process_summary.memcpy_byte_view_path_used ? 1 : 0,
        process_summary.typed_row_pointer_optimization_deferred ? 1 : 0,
        process_summary.first_u_output_sample,
        process_summary.last_u_output_sample,
        process_summary.first_v_output_sample,
        process_summary.last_v_output_sample
    );
#else
    (void)data;
    (void)requested_frame;
    (void)predecessor_frame;
    (void)store_status;
    (void)process_summary;
#endif
}

void cnr3_trace_live_cache_hit_return(
    const Cnr3FilterData& data,
    int requested_frame
) noexcept {
#if defined(CNR3_KEYSTONE_DEV_TRACE)
    std::fprintf(
        stderr,
        "[KDT] instance=%d N=%d branch=CACHE-HIT "
        "cache_lookup=hit "
        "source_trigger_requested=1 source_trigger_retrieved=1 "
        "source_trigger_consumed=0 source_trigger_released=1 "
        "pixel_compute=0 p11b_called=0 p11c_called=0 "
        "cache_hit_pin_taken=1 cache_hit_pin_discharged=1 cache_hit_pin_balance=0 "
        "returned_ref_added=1 return_transferred=1\n",
        data.config.instance_id.value,
        requested_frame
    );
#else
    (void)data;
    (void)requested_frame;
#endif
}

void cnr3_trace_live_after_frame2_not_yet_implemented(
    const Cnr3FilterData& data,
    int requested_frame
) noexcept {
#if defined(CNR3_KEYSTONE_DEV_TRACE) && defined(SCAFFOLD_CMS07_K1E3_REFUSE_AFTER_FRAME2_BEFORE_RECOVERY)
    std::fprintf(
        stderr,
        "[KDT] instance=%d N=%d NOT-YET-IMPLEMENTED branch=after-frame2-before-recovery-wiring\n",
        data.config.instance_id.value,
        requested_frame
    );
#else
    (void)data;
    (void)requested_frame;
#endif
}

Cnr3Status cnr3_discard_frame_data_with_cache(
    void** frame_data,
    Cnr3OutputCacheCore& output_cache
) noexcept {
    if (frame_data == nullptr || *frame_data == nullptr) {
        return Cnr3Status::ok;
    }

    Cnr3LiveGetFrameFrameData* request_data =
        static_cast<Cnr3LiveGetFrameFrameData*>(*frame_data);

    const Cnr3Status discharge_status =
        request_data->pin_list.discharge_all(output_cache);

    delete request_data;
    *frame_data = nullptr;

    return discharge_status;
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

bool cnr3_live_store_status_allows_return(
    Cnr3Status status
) noexcept {
    return status == Cnr3Status::ok || status == Cnr3Status::duplicate;
}


Cnr3Status cnr3_store_live_output_frame_for_return(
    Cnr3FilterData& data,
    int frame_number,
    VSFrame* output_frame,
    const VSAPI* vsapi
) noexcept {
    if (output_frame == nullptr || vsapi == nullptr) {
        return Cnr3Status::invalid_argument;
    }

    const VSFrame* cache_frame_ref = vsapi->addFrameRef(output_frame);

    if (cache_frame_ref == nullptr) {
        return Cnr3Status::vapoursynth_error;
    }

    Cnr3OwnedFrameRef cache_owned_frame{};
    const Cnr3Status adopt_status = cache_owned_frame.reset_to_owned_frame(
        cache_frame_ref,
        vsapi
    );

    if (!cnr3_status_is_ok(adopt_status)) {
        vsapi->freeFrame(cache_frame_ref);
        return adopt_status;
    }

    if (cnr3_live_output_frame_is_checkpoint(frame_number)) {
        return data.output_cache.store_checkpoint_owned_frame(
            frame_number,
            std::move(cache_owned_frame)
        );
    }

    return data.output_cache.store_noncheckpoint_owned_frame(
        frame_number,
        std::move(cache_owned_frame)
    );
}

enum class Cnr3LiveCacheHitStartResult {
    miss,
    started,
    failed
};

Cnr3LiveCacheHitStartResult cnr3_try_start_live_cache_hit_return(
    int n,
    Cnr3FilterData& data,
    void** frame_data,
    VSFrameContext* frame_ctx,
    const VSAPI* vsapi
) {
    if (*frame_data != nullptr) {
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1F cache-hit proof: frameData was unexpectedly non-null at arInitial."
        );
        return Cnr3LiveCacheHitStartResult::failed;
    }

    Cnr3LiveGetFrameFrameData* request_data =
        new (std::nothrow) Cnr3LiveGetFrameFrameData{};

    if (request_data == nullptr) {
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1F cache-hit proof: failed to allocate frameData."
        );
        return Cnr3LiveCacheHitStartResult::failed;
    }

    request_data->branch = Cnr3LiveGetFrameBranch::cache_hit_return;
    request_data->requested_frame = n;

    /*
        K.1F cache-hit branch-b: present output[N] is a plan-time fact
        made at arInitial and acted on at arAllFramesReady. Pin it now so
        prune cannot evict the cached frame across the activation gap.
    */
    const Cnr3Status pin_status = data.output_cache.lookup_frame_and_record_pin(
        n,
        request_data->pin_list
    );

    if (pin_status == Cnr3Status::not_found) {
        delete request_data;
        return Cnr3LiveCacheHitStartResult::miss;
    }

    if (!cnr3_status_is_ok(pin_status)) {
        delete request_data;
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1F cache-hit proof: failed to pin cached output[N]."
        );
        return Cnr3LiveCacheHitStartResult::failed;
    }

    request_data->cache_hit_pin_taken = true;
    request_data->source_requested = true;
    *frame_data = request_data;

    /*
        Option C lifecycle trigger: the cache-hit path does not consume
        source[N] for pixel computation, but requests one real source frame so
        the non-source filter returns only from arAllFramesReady.
    */
    vsapi->requestFrameFilter(n, data.source, frame_ctx);

    return Cnr3LiveCacheHitStartResult::started;
}

const VSFrame* cnr3_get_frame_live_cache_hit_return(
    int n,
    Cnr3FilterData& data,
    void** frame_data,
    VSFrameContext* frame_ctx,
    const VSAPI* vsapi
) {
    Cnr3LiveGetFrameFrameData* request_data =
        static_cast<Cnr3LiveGetFrameFrameData*>(*frame_data);

    if (request_data == nullptr ||
        request_data->branch != Cnr3LiveGetFrameBranch::cache_hit_return ||
        request_data->requested_frame != n ||
        !request_data->source_requested ||
        !request_data->cache_hit_pin_taken ||
        request_data->pin_list.pin_count() != 1U) {
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1F cache-hit proof: invalid frameData cache-hit lifecycle."
        );
        return nullptr;
    }

    const VSFrame* source_trigger_frame = vsapi->getFrameFilter(n, data.source, frame_ctx);

    if (source_trigger_frame == nullptr) {
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1F cache-hit proof: triggering source frame retrieval failed."
        );
        return nullptr;
    }

    /*
        The trigger source exists only to guarantee arAllFramesReady. It is
        deliberately not passed to P.11B/P.11C and not stored. The retrieved
        source reference is nevertheless a normal owned reference and must be
        released exactly once outside any cache lock.
    */
    vsapi->freeFrame(source_trigger_frame);
    source_trigger_frame = nullptr;

    Cnr3OwnedFrameRef returned_cache_ref{};
    const Cnr3Status lookup_status = data.output_cache.lookup_frame_and_add_ref(
        n,
        vsapi,
        returned_cache_ref
    );

    if (!cnr3_status_is_ok(lookup_status) || !returned_cache_ref.has_frame()) {
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1F cache-hit proof: pinned cached output[N] was not retrievable."
        );
        return nullptr;
    }

    const Cnr3Status discard_status = cnr3_discard_frame_data_with_cache(
        frame_data,
        data.output_cache
    );

    if (!cnr3_status_is_ok(discard_status)) {
        returned_cache_ref.reset();
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1F cache-hit proof: failed to discharge cache-hit pin-list."
        );
        return nullptr;
    }

    cnr3_trace_live_cache_hit_return(data, n);

    return returned_cache_ref.transfer_to_caller();
}

const VSFrame* cnr3_get_frame_live_predecessor_present_compute(
    int n,
    int activation_reason,
    Cnr3FilterData& data,
    void** frame_data,
    VSFrameContext* frame_ctx,
    VSCore* core,
    const VSAPI* vsapi
) {
    if (activation_reason == arInitial) {
        if (*frame_data != nullptr) {
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 K.1E.3 sequential proof: frameData was unexpectedly non-null at arInitial."
            );
            return nullptr;
        }

        Cnr3LiveGetFrameFrameData* request_data =
            new (std::nothrow) Cnr3LiveGetFrameFrameData{};

        if (request_data == nullptr) {
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 K.1E.3 sequential proof: failed to allocate frameData."
            );
            return nullptr;
        }

        request_data->branch = Cnr3LiveGetFrameBranch::predecessor_present_compute;
        request_data->requested_frame = n;
        request_data->predecessor_frame = n - 1;
        *frame_data = request_data;

        const Cnr3Status pin_status = data.output_cache.lookup_frame_and_record_pin(
            request_data->predecessor_frame,
            request_data->pin_list
        );

        if (!cnr3_status_is_ok(pin_status)) {
            (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 K.1E.3 sequential proof: cached predecessor output[N-1] was not available."
            );
            return nullptr;
        }

        request_data->predecessor_pin_taken = true;
        request_data->source_requested = true;

        vsapi->requestFrameFilter(n, data.source, frame_ctx);

        return nullptr;
    }

    if (activation_reason == arError) {
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        return nullptr;
    }

    if (activation_reason != arAllFramesReady) {
        return nullptr;
    }

    Cnr3LiveGetFrameFrameData* request_data =
        static_cast<Cnr3LiveGetFrameFrameData*>(*frame_data);

    if (request_data == nullptr ||
        request_data->requested_frame != n ||
        request_data->predecessor_frame != n - 1 ||
        !request_data->source_requested ||
        !request_data->predecessor_pin_taken ||
        request_data->pin_list.pin_count() != 1U) {
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1E.3 sequential proof: invalid frameData predecessor/source lifecycle."
        );
        return nullptr;
    }

    const VSFrame* source_frame = vsapi->getFrameFilter(n, data.source, frame_ctx);

    if (source_frame == nullptr) {
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1E.3 sequential proof: source frame retrieval failed."
        );
        return nullptr;
    }

    Cnr3OwnedFrameRef predecessor_compute_ref{};
    const Cnr3Status predecessor_status = data.output_cache.lookup_frame_and_add_ref(
        request_data->predecessor_frame,
        vsapi,
        predecessor_compute_ref
    );

    if (!cnr3_status_is_ok(predecessor_status) ||
        !predecessor_compute_ref.has_frame()) {
        vsapi->freeFrame(source_frame);
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1E.3 sequential proof: failed to acquire predecessor compute reference."
        );
        return nullptr;
    }

    VSFrame* output_frame = vsapi->copyFrame(source_frame, core);

    if (output_frame == nullptr) {
        predecessor_compute_ref.reset();
        vsapi->freeFrame(source_frame);
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1E.3 sequential proof: copyFrame failed."
        );
        return nullptr;
    }

    if (output_frame == source_frame) {
        predecessor_compute_ref.reset();
        vsapi->freeFrame(output_frame);
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1E.3 sequential proof: copyFrame returned the source frame alias."
        );
        return nullptr;
    }

    Cnr3CallerSuppliedFrameProcessSummary process_summary{};
    const Cnr3Status process_status =
        cnr3_process_caller_supplied_vapoursynth_frame_triplet(
            source_frame,
            predecessor_compute_ref.get(),
            output_frame,
            vsapi,
            data.bits_per_sample,
            data.sub_sampling_w,
            data.sub_sampling_h,
            data.response_tables,
            process_summary
        );

    predecessor_compute_ref.reset();
    vsapi->freeFrame(source_frame);
    source_frame = nullptr;

    if (!cnr3_status_is_ok(process_status) || !process_summary.frame_processed) {
        vsapi->freeFrame(output_frame);
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1E.3 sequential proof: P.11B predecessor-present processing failed."
        );
        return nullptr;
    }

    const Cnr3Status store_status = cnr3_store_live_output_frame_for_return(
        data,
        n,
        output_frame,
        vsapi
    );

    if (!cnr3_live_store_status_allows_return(store_status)) {
        vsapi->freeFrame(output_frame);
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1E.3 sequential proof: failed to production-store output[N]."
        );
        return nullptr;
    }

    const int predecessor_frame_for_trace = request_data->predecessor_frame;

    const Cnr3Status discard_status = cnr3_discard_frame_data_with_cache(
        frame_data,
        data.output_cache
    );

    if (!cnr3_status_is_ok(discard_status)) {
        vsapi->freeFrame(output_frame);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1E.3 sequential proof: failed to discharge predecessor pin-list."
        );
        return nullptr;
    }

    cnr3_trace_live_predecessor_present_compute(
        data,
        n,
        predecessor_frame_for_trace,
        store_status,
        process_summary
    );

    return output_frame;
}

const VSFrame* cnr3_get_frame_live_frame0_fresh_start(
    int n,
    int activation_reason,
    Cnr3FilterData& data,
    void** frame_data,
    VSFrameContext* frame_ctx,
    VSCore* core,
    const VSAPI* vsapi
) {
    if (activation_reason == arInitial) {
        if (*frame_data != nullptr) {
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 K.1D frame-0 proof: frameData was unexpectedly non-null at arInitial."
            );
            return nullptr;
        }

        Cnr3LiveGetFrameFrameData* request_data =
            new (std::nothrow) Cnr3LiveGetFrameFrameData{};

        if (request_data == nullptr) {
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 K.1D frame-0 proof: failed to allocate frameData."
            );
            return nullptr;
        }

        request_data->branch = Cnr3LiveGetFrameBranch::frame0_fresh_start;
        request_data->requested_frame = n;
        request_data->source_requested = true;
        *frame_data = request_data;

        vsapi->requestFrameFilter(n, data.source, frame_ctx);

        return nullptr;
    }

    if (activation_reason == arAllFramesReady) {
        Cnr3LiveGetFrameFrameData* request_data =
            static_cast<Cnr3LiveGetFrameFrameData*>(*frame_data);

        if (request_data == nullptr ||
            request_data->branch != Cnr3LiveGetFrameBranch::frame0_fresh_start ||
            request_data->requested_frame != n ||
            !request_data->source_requested) {
            (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 K.1D frame-0 proof: source frame was not requested in this activation."
            );
            return nullptr;
        }

        const VSFrame* source_frame = vsapi->getFrameFilter(n, data.source, frame_ctx);

        if (source_frame == nullptr) {
            (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 K.1D frame-0 proof: source frame retrieval failed."
            );
            return nullptr;
        }

        VSFrame* output_frame = vsapi->copyFrame(source_frame, core);

        if (output_frame == nullptr) {
            vsapi->freeFrame(source_frame);
            (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 K.1D frame-0 proof: copyFrame failed."
            );
            return nullptr;
        }

        if (output_frame == source_frame) {
            vsapi->freeFrame(output_frame);
            (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 K.1D frame-0 proof: copyFrame returned the source frame alias."
            );
            return nullptr;
        }

        vsapi->freeFrame(source_frame);
        source_frame = nullptr;

        const VSFrame* cache_frame_ref = vsapi->addFrameRef(output_frame);

        if (cache_frame_ref == nullptr) {
            vsapi->freeFrame(output_frame);
            (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 K.1D frame-0 proof: failed to add cache frame reference."
            );
            return nullptr;
        }

        Cnr3OwnedFrameRef cache_owned_frame{};
        const Cnr3Status adopt_status = cache_owned_frame.reset_to_owned_frame(
            cache_frame_ref,
            vsapi
        );

        if (!cnr3_status_is_ok(adopt_status)) {
            vsapi->freeFrame(cache_frame_ref);
            vsapi->freeFrame(output_frame);
            (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 K.1D frame-0 proof: failed to adopt cache frame reference."
            );
            return nullptr;
        }

        const Cnr3Status store_status = data.output_cache.store_checkpoint_owned_frame(
            0,
            std::move(cache_owned_frame)
        );

        if (!cnr3_live_store_status_allows_return(store_status)) {
            vsapi->freeFrame(output_frame);
            (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 K.1D frame-0 proof: failed to store output[0] checkpoint."
            );
            return nullptr;
        }

        cnr3_trace_live_frame0_fresh_start(
            data,
            n,
            true,
            true,
            true,
            cnr3_status_name(store_status),
            true
        );

        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);

        /*
            CMS07-K.1D frame-0 only: copyFrame(source[0]) produces the first
            real CNR3 output frame because fresh-start output[0] is
            source-verbatim. The extra addFrameRef() reference is stored in the
            output cache; this original copyFrame reference is returned to
            VapourSynth. Frames after 2 are refused until recovery wiring
            replaces the K.1E.3 temporary boundary, so source[N] cannot remain
            a fallback output.
        */
        return output_frame;
    }

    if (activation_reason == arError) {
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        return nullptr;
    }

    return nullptr;
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
        Cnr3LiveGetFrameFrameData* request_data =
            static_cast<Cnr3LiveGetFrameFrameData*>(*frame_data);

        if (request_data == nullptr) {
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 K.1F cache-hit proof: missing frameData at arAllFramesReady."
            );
            return nullptr;
        }

        switch (request_data->branch) {
        case Cnr3LiveGetFrameBranch::cache_hit_return:
            return cnr3_get_frame_live_cache_hit_return(
                n,
                *data,
                frame_data,
                frame_ctx,
                vsapi
            );
        case Cnr3LiveGetFrameBranch::predecessor_present_compute:
            return cnr3_get_frame_live_predecessor_present_compute(
                n,
                activation_reason,
                *data,
                frame_data,
                frame_ctx,
                core,
                vsapi
            );
        case Cnr3LiveGetFrameBranch::frame0_fresh_start:
            return cnr3_get_frame_live_frame0_fresh_start(
                n,
                activation_reason,
                *data,
                frame_data,
                frame_ctx,
                core,
                vsapi
            );
        case Cnr3LiveGetFrameBranch::none:
        default:
            (void)cnr3_discard_frame_data_with_cache(frame_data, data->output_cache);
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 K.1F cache-hit proof: unknown frameData branch at arAllFramesReady."
            );
            return nullptr;
        }
    }

    if (activation_reason != arInitial) {
        return nullptr;
    }

    const Cnr3LiveCacheHitStartResult cache_hit_start =
        cnr3_try_start_live_cache_hit_return(
            n,
            *data,
            frame_data,
            frame_ctx,
            vsapi
        );

    if (cache_hit_start == Cnr3LiveCacheHitStartResult::started ||
        cache_hit_start == Cnr3LiveCacheHitStartResult::failed) {
        return nullptr;
    }

    if (n > 2) {
        cnr3_trace_live_after_frame2_not_yet_implemented(*data, n);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1F cache-hit proof: frames after 2 are not implemented before recovery wiring."
        );
        return nullptr;
    }

    if (n == 1 || n == 2) {
        return cnr3_get_frame_live_predecessor_present_compute(
            n,
            activation_reason,
            *data,
            frame_data,
            frame_ctx,
            core,
            vsapi
        );
    }

    return cnr3_get_frame_live_frame0_fresh_start(
        n,
        activation_reason,
        *data,
        frame_data,
        frame_ctx,
        core,
        vsapi
    );
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

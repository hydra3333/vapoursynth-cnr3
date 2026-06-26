/*
    CNR3 live getFrame arAllFramesReady branch execution.

    This translation unit owns plugin-side retrieval, branch-tag execution,
    frameData cleanup-before-delete choreography, and live KDT emission. Cache
    state operations are still delegated to the cache core.

    SPDX-License-Identifier: AGPL-3.0-or-later
*/

#include "cnr3_build_config.h"
#include "cnr3_plugin_internal.h"

#include <cstdio>
#include <utility>

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

bool cnr3_live_output_frame_is_checkpoint(
    int frame_number
) noexcept {
    return frame_number == 0 ||
        (frame_number > 0 && (frame_number % CNR3_CACHE_CHECKPOINT_INTERVAL) == 0);
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

const VSFrame* cnr3_complete_live_predecessor_present_compute(
    int n,
    Cnr3FilterData& data,
    void** frame_data,
    VSFrameContext* frame_ctx,
    VSCore* core,
    const VSAPI* vsapi
) {
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
const VSFrame* cnr3_complete_live_frame0_fresh_start(
    int n,
    Cnr3FilterData& data,
    void** frame_data,
    VSFrameContext* frame_ctx,
    VSCore* core,
    const VSAPI* vsapi
) {
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
const VSFrame* cnr3_arAllFramesReady(
    int n,
    Cnr3FilterData& data,
    void** frame_data,
    VSFrameContext* frame_ctx,
    VSCore* core,
    const VSAPI* vsapi
) {
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
            data,
            frame_data,
            frame_ctx,
            vsapi
        );
    case Cnr3LiveGetFrameBranch::predecessor_present_compute:
        return cnr3_complete_live_predecessor_present_compute(
            n,
            data,
            frame_data,
            frame_ctx,
            core,
            vsapi
        );
    case Cnr3LiveGetFrameBranch::frame0_fresh_start:
        return cnr3_complete_live_frame0_fresh_start(
            n,
            data,
            frame_data,
            frame_ctx,
            core,
            vsapi
        );
    case Cnr3LiveGetFrameBranch::none:
    default:
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1F cache-hit proof: unknown frameData branch at arAllFramesReady."
        );
        return nullptr;
    }
}

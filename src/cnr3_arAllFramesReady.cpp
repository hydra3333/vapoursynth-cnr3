/*
    CNR3 live getFrame arAllFramesReady branch execution.

    This translation unit owns plugin-side retrieval, branch-tag execution,
    frameData cleanup-before-delete choreography, and live KDT emission. Cache
    state operations are still delegated to the cache core.

    SPDX-License-Identifier: AGPL-3.0-or-later
*/

#include "cnr3_build_config.h"
#include "cnr3_plugin_internal.h"

#include <algorithm>
#include <cstdio>
#include <string>
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
        "[KDT] instance=%d N=%d FRAME0-FRESH-START req=%d got=%d copyFrame=%s store=%s ret=%d "
        "p11c_called=0 scene_change_detection_used=0 scene_change_deferred=0 "
        "scene_change_not_applicable=1 scene_change_not_applicable_reason=no_previous_filtered_output "
        "flag=REAL_OUTPUT_FRAME0\n",
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
    const Cnr3CallerSuppliedFrameProcessSummary& process_summary,
    bool store_as_checkpoint
) noexcept {
#if defined(CNR3_KEYSTONE_DEV_TRACE)
    const bool resulting_slot_is_checkpoint_expected =
        store_as_checkpoint &&
        (store_status == Cnr3Status::ok || store_status == Cnr3Status::duplicate);

    std::fprintf(
        stderr,
        "[KDT] instance=%d N=%d branch=PREDECESSOR-PRESENT-COMPUTE "
        "source=%d pred=%d pred_source=output_cache pred_lookup=hit "
        "pred_liveness_basis=pin pred_checkpoint_used_as_pin=0 "
        "pred_pin_taken=1 pred_pin_discharged=1 pred_pin_balance=0 "
        "pred_ref_carried_across_gap=0 "
        "pred_compute_ref_acquired=1 pred_compute_ref_released=1 pred_compute_ref_balance=0 "
        "p11b_called=1 p11c_called=1 scene_change_deferred=0 "
        "scene_change_detection_used=%d scene_chroma_used=%d "
        "scene_change_threshold=%lld scene_change_diff_total=%lld "
        "scene_change_samples_examined=%d scene_change_detected=%d "
        "scene_change_reset_output_used=%d recursive_chroma_blend_used=%d "
        "store_as_checkpoint=%d resulting_slot_is_checkpoint_expected=%d "
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
        process_summary.scene_change_detection_used ? 1 : 0,
        process_summary.scene_chroma_used ? 1 : 0,
        static_cast<long long>(process_summary.scene_change_threshold),
        static_cast<long long>(process_summary.scene_change_diff_total),
        process_summary.scene_change_samples_examined,
        process_summary.scene_change_detected ? 1 : 0,
        process_summary.scene_change_reset_output_used ? 1 : 0,
        process_summary.recursive_chroma_blend_used ? 1 : 0,
        store_as_checkpoint ? 1 : 0,
        resulting_slot_is_checkpoint_expected ? 1 : 0,
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
    (void)store_as_checkpoint;
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


const char* cnr3_live_recovery_hole_outcome_name(
    Cnr3LiveRecoveryHoleOutcome outcome
) noexcept {
    switch (outcome) {
    case Cnr3LiveRecoveryHoleOutcome::computed:
        return "computed";
    case Cnr3LiveRecoveryHoleOutcome::adopted_skipped:
        return "adopted-skipped";
    case Cnr3LiveRecoveryHoleOutcome::adopted_post_compute_loser:
        return "adopted-post-compute-loser";
    case Cnr3LiveRecoveryHoleOutcome::none:
    default:
        return "none";
    }
}

const char* cnr3_live_recovery_branch_name(
    Cnr3LiveRecoveryBranch branch
) noexcept {
    switch (branch) {
    case Cnr3LiveRecoveryBranch::exact_anchor:
        return "exact-anchor";
    case Cnr3LiveRecoveryBranch::floor_fresh_start:
        return "floor-fresh-start";
    case Cnr3LiveRecoveryBranch::none:
    default:
        return "none";
    }
}

std::string cnr3_join_frame_numbers_for_kdt(
    const std::vector<int>& frame_numbers
) {
    std::string joined{"["};

    for (std::size_t i = 0U; i < frame_numbers.size(); ++i) {
        if (i != 0U) {
            joined += ",";
        }

        joined += std::to_string(frame_numbers[i]);
    }

    joined += "]";
    return joined;
}

bool cnr3_live_recovery_source_was_requested(
    const Cnr3LiveGetFrameFrameData& request_data,
    int source_frame_number
) noexcept {
    return std::find(
        request_data.source_request_frame_numbers.begin(),
        request_data.source_request_frame_numbers.end(),
        source_frame_number
    ) != request_data.source_request_frame_numbers.end();
}

void cnr3_trace_live_recovery(
    const Cnr3FilterData& data,
    int requested_frame,
    Cnr3LiveRecoveryBranch recovery_branch,
    int recovery_floor_frame,
    Cnr3LiveRecoveryHoleOutcome recovery_floor_outcome,
    const Cnr3CacheRecoverySearchPlan& recovery_plan,
    const std::vector<int>& source_request_frame_numbers,
    const std::vector<Cnr3LiveRecoveryHoleOutcome>& per_hole_outcomes,
    std::size_t pin_list_size_before_discharge,
    const Cnr3CallerSuppliedFrameProcessSummary& target_process_summary
) noexcept {
#if defined(CNR3_KEYSTONE_DEV_TRACE)
    const std::string holes_text =
        cnr3_join_frame_numbers_for_kdt(recovery_plan.hole_frame_numbers);
    const std::string source_requests_text =
        cnr3_join_frame_numbers_for_kdt(source_request_frame_numbers);

    std::string per_hole_text{};

    for (std::size_t i = 0U; i < recovery_plan.hole_frame_numbers.size(); ++i) {
        if (!per_hole_text.empty()) {
            per_hole_text += " ";
        }

        const Cnr3LiveRecoveryHoleOutcome outcome =
            (i < per_hole_outcomes.size())
            ? per_hole_outcomes[i]
            : Cnr3LiveRecoveryHoleOutcome::none;

        per_hole_text += "hole=";
        per_hole_text += std::to_string(recovery_plan.hole_frame_numbers[i]);
        per_hole_text += " outcome=";
        per_hole_text += cnr3_live_recovery_hole_outcome_name(outcome);
    }

    if (per_hole_text.empty()) {
        per_hole_text = "hole=none outcome=none";
    }

    if (recovery_branch == Cnr3LiveRecoveryBranch::floor_fresh_start) {
        std::fprintf(
            stderr,
            "[KDT] instance=%d N=%d branch=RECOVER recover_branch=%s "
            "floor=%d floor_outcome=%s "
            "floor_scene_change_detection_used=0 floor_scene_change_deferred=0 "
            "floor_scene_change_not_applicable=1 "
            "hole_count=%zu holes=%s source_requests=%s %s "
            "pixel_compute=%d p11b_called=%d p11c_called=0 scene_change_deferred=1 "
            "pin_list_size=%zu pin_balance=0 "
            "returned_ref_added=1 return_transferred=1 "
            "frame_processed=%d luma_samples_copied=%d "
            "chroma_u_samples_processed=%d chroma_v_samples_processed=%d "
            "first_u_output_sample=%d last_u_output_sample=%d "
            "first_v_output_sample=%d last_v_output_sample=%d\n",
            data.config.instance_id.value,
            requested_frame,
            cnr3_live_recovery_branch_name(recovery_branch),
            recovery_floor_frame,
            cnr3_live_recovery_hole_outcome_name(recovery_floor_outcome),
            recovery_plan.hole_frame_numbers.size(),
            holes_text.c_str(),
            source_requests_text.c_str(),
            per_hole_text.c_str(),
            target_process_summary.frame_processed ? 1 : 0,
            target_process_summary.frame_processed ? 1 : 0,
            pin_list_size_before_discharge,
            target_process_summary.frame_processed ? 1 : 0,
            target_process_summary.luma_samples_copied,
            target_process_summary.chroma_u_samples_processed,
            target_process_summary.chroma_v_samples_processed,
            target_process_summary.first_u_output_sample,
            target_process_summary.last_u_output_sample,
            target_process_summary.first_v_output_sample,
            target_process_summary.last_v_output_sample
        );
        return;
    }

    std::fprintf(
        stderr,
        "[KDT] instance=%d N=%d branch=RECOVER recover_branch=%s "
        "anchor=%d hole_count=%zu holes=%s source_requests=%s %s "
        "pixel_compute=%d p11b_called=%d p11c_called=0 scene_change_deferred=1 "
        "anchor_pin_taken=%d pin_list_size=%zu pin_balance=0 "
        "returned_ref_added=1 return_transferred=1 "
        "frame_processed=%d luma_samples_copied=%d "
        "chroma_u_samples_processed=%d chroma_v_samples_processed=%d "
        "first_u_output_sample=%d last_u_output_sample=%d "
        "first_v_output_sample=%d last_v_output_sample=%d\n",
        data.config.instance_id.value,
        requested_frame,
        cnr3_live_recovery_branch_name(recovery_branch),
        recovery_plan.anchor_frame_number,
        recovery_plan.hole_frame_numbers.size(),
        holes_text.c_str(),
        source_requests_text.c_str(),
        per_hole_text.c_str(),
        target_process_summary.frame_processed ? 1 : 0,
        target_process_summary.frame_processed ? 1 : 0,
        recovery_plan.anchor_pin_recorded ? 1 : 0,
        pin_list_size_before_discharge,
        target_process_summary.frame_processed ? 1 : 0,
        target_process_summary.luma_samples_copied,
        target_process_summary.chroma_u_samples_processed,
        target_process_summary.chroma_v_samples_processed,
        target_process_summary.first_u_output_sample,
        target_process_summary.last_u_output_sample,
        target_process_summary.first_v_output_sample,
        target_process_summary.last_v_output_sample
    );
#else
    (void)data;
    (void)requested_frame;
    (void)recovery_branch;
    (void)recovery_floor_frame;
    (void)recovery_floor_outcome;
    (void)recovery_plan;
    (void)source_request_frame_numbers;
    (void)per_hole_outcomes;
    (void)pin_list_size_before_discharge;
    (void)target_process_summary;
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

bool cnr3_live_output_frame_should_store_as_checkpoint(
    const Cnr3LiveOutputStoreRequest& request
) noexcept {
    return request.force_checkpoint ||
        cnr3_live_output_frame_is_checkpoint(request.frame_number);
}

Cnr3Status cnr3_store_live_output_frame_for_return(
    Cnr3FilterData& data,
    const Cnr3LiveOutputStoreRequest& request,
    VSFrame* output_frame,
    const VSAPI* vsapi
) noexcept {
    if (
        request.frame_number < 0 ||
        output_frame == nullptr ||
        vsapi == nullptr
        ) {
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

    if (cnr3_live_output_frame_should_store_as_checkpoint(request)) {
        return data.output_cache.store_checkpoint_owned_frame(
            request.frame_number,
            std::move(cache_owned_frame)
        );
    }

    return data.output_cache.store_noncheckpoint_owned_frame(
        request.frame_number,
        std::move(cache_owned_frame)
    );
}


Cnr3Status cnr3_store_live_output_frame_for_authoritative_return(
    Cnr3FilterData& data,
    const Cnr3LiveOutputStoreRequest& request,
    VSFrame* output_frame,
    const VSAPI* vsapi,
    const VSFrame*& out_return_frame,
    Cnr3Status& out_store_status,
    bool& out_returned_cached_winner
) noexcept {
    out_return_frame = nullptr;
    out_store_status = Cnr3Status::invariant_violation;
    out_returned_cached_winner = false;

    if (output_frame == nullptr || vsapi == nullptr) {
        if (output_frame != nullptr && vsapi != nullptr) {
            vsapi->freeFrame(output_frame);
        }

        out_store_status = Cnr3Status::invalid_argument;
        return out_store_status;
    }

    out_store_status = cnr3_store_live_output_frame_for_return(
        data,
        request,
        output_frame,
        vsapi
    );

    if (out_store_status == Cnr3Status::ok) {
        out_return_frame = output_frame;
        return Cnr3Status::ok;
    }

    if (out_store_status != Cnr3Status::duplicate) {
        vsapi->freeFrame(output_frame);
        return out_store_status;
    }

    /*
        A duplicate target store means another activation's first-in-best-
        dressed output[N] is authoritative. Discard this activation's computed
        loser and return a fresh reference to the cached winner instead.
    */
    vsapi->freeFrame(output_frame);
    output_frame = nullptr;

    Cnr3OwnedFrameRef cached_winner_ref{};
    const Cnr3Status lookup_status = data.output_cache.lookup_frame_and_add_ref(
        request.frame_number,
        vsapi,
        cached_winner_ref
    );

    if (!cnr3_status_is_ok(lookup_status) || !cached_winner_ref.has_frame()) {
        return !cnr3_status_is_ok(lookup_status)
            ? lookup_status
            : Cnr3Status::invariant_violation;
    }

    out_returned_cached_winner = true;
    out_return_frame = cached_winner_ref.transfer_to_caller();
    return Cnr3Status::ok;
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
        cnr3_process_caller_supplied_vapoursynth_frame_triplet_with_scene_change(
            source_frame,
            predecessor_compute_ref.get(),
            output_frame,
            vsapi,
            data.bits_per_sample,
            data.sub_sampling_w,
            data.sub_sampling_h,
            data.response_tables,
            data.scene_change_config,
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
            "CNR3 P.11C.3 branch-c proof: predecessor-present scene processing failed."
        );
        return nullptr;
    }

    const Cnr3LiveOutputStoreRequest store_request{
        n,
        process_summary.scene_change_detected
    };
    const bool store_as_checkpoint =
        cnr3_live_output_frame_should_store_as_checkpoint(store_request);

    const VSFrame* return_frame = nullptr;
    Cnr3Status store_status = Cnr3Status::invariant_violation;
    bool returned_cached_winner = false;
    const Cnr3Status return_status =
        cnr3_store_live_output_frame_for_authoritative_return(
            data,
            store_request,
            output_frame,
            vsapi,
            return_frame,
            store_status,
            returned_cached_winner
        );
    output_frame = nullptr;

    if (!cnr3_status_is_ok(return_status) || return_frame == nullptr) {
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1E.3 sequential proof: failed to store/return authoritative output[N]."
        );
        return nullptr;
    }

    const int predecessor_frame_for_trace = request_data->predecessor_frame;
    (void)returned_cached_winner;

    const Cnr3Status discard_status = cnr3_discard_frame_data_with_cache(
        frame_data,
        data.output_cache
    );

    if (!cnr3_status_is_ok(discard_status)) {
        vsapi->freeFrame(return_frame);
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
        process_summary,
        store_as_checkpoint
    );

    return return_frame;
}

const VSFrame* cnr3_complete_live_recovery(
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
        request_data->branch != Cnr3LiveGetFrameBranch::recovery ||
        request_data->requested_frame != n ||
        !request_data->source_requested ||
        request_data->recovery_plan.requested_frame != n ||
        request_data->per_hole_outcomes.size() !=
            request_data->recovery_plan.hole_frame_numbers.size()) {
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 D.3 floor-fresh-start proof: invalid recovery frameData lifecycle."
        );
        return nullptr;
    }

    const bool exact_anchor_recovery =
        request_data->recovery_branch == Cnr3LiveRecoveryBranch::exact_anchor;
    const bool floor_fresh_start_recovery =
        request_data->recovery_branch == Cnr3LiveRecoveryBranch::floor_fresh_start;

    if ((!exact_anchor_recovery && !floor_fresh_start_recovery) ||
        (exact_anchor_recovery &&
            (!request_data->recovery_plan.anchor_found ||
                !request_data->recovery_plan.anchor_pin_recorded ||
                request_data->pin_list.pin_count() == 0U)) ||
        (floor_fresh_start_recovery &&
            (request_data->recovery_plan.anchor_found ||
                request_data->recovery_plan.anchor_pin_recorded ||
                !cnr3_frame_number_is_valid(request_data->recovery_floor_frame) ||
                request_data->recovery_floor_frame >= n))) {
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 D.3 floor-fresh-start proof: invalid recovery branch foundation."
        );
        return nullptr;
    }

    Cnr3CacheRecoverySearchPlan& recovery_plan = request_data->recovery_plan;

    if (floor_fresh_start_recovery) {
        const int floor_frame = request_data->recovery_floor_frame;

        if (!cnr3_live_recovery_source_was_requested(*request_data, floor_frame)) {
            (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 D.3 floor-fresh-start proof: floor source was not requested at arInitial."
            );
            return nullptr;
        }

        const Cnr3Status floor_adopt_status = data.output_cache.lookup_frame_and_record_pin(
            floor_frame,
            request_data->pin_list
        );

        if (cnr3_status_is_ok(floor_adopt_status)) {
            request_data->recovery_floor_outcome =
                Cnr3LiveRecoveryHoleOutcome::adopted_skipped;
        }
        else if (floor_adopt_status != Cnr3Status::not_found) {
            (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 D.3 floor-fresh-start proof: pre-compute floor adopt-and-skip lookup failed."
            );
            return nullptr;
        }
        else {
            const VSFrame* floor_source_frame = vsapi->getFrameFilter(
                floor_frame,
                data.source,
                frame_ctx
            );

            if (floor_source_frame == nullptr) {
                (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
                cnr3_set_filter_error(
                    frame_ctx,
                    vsapi,
                    "CNR3 D.3 floor-fresh-start proof: floor source frame retrieval failed."
                );
                return nullptr;
            }

            VSFrame* floor_output_frame = vsapi->copyFrame(floor_source_frame, core);

            if (floor_output_frame == nullptr) {
                vsapi->freeFrame(floor_source_frame);
                (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
                cnr3_set_filter_error(
                    frame_ctx,
                    vsapi,
                    "CNR3 D.3 floor-fresh-start proof: floor copyFrame failed."
                );
                return nullptr;
            }

            if (floor_output_frame == floor_source_frame) {
                vsapi->freeFrame(floor_output_frame);
                (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
                cnr3_set_filter_error(
                    frame_ctx,
                    vsapi,
                    "CNR3 D.3 floor-fresh-start proof: floor copyFrame returned the source frame alias."
                );
                return nullptr;
            }

            vsapi->freeFrame(floor_source_frame);
            floor_source_frame = nullptr;

            Cnr3OwnedFrameRef floor_owned_frame{};
            const Cnr3Status adopt_floor_status = floor_owned_frame.reset_to_owned_frame(
                floor_output_frame,
                vsapi
            );

            if (!cnr3_status_is_ok(adopt_floor_status)) {
                vsapi->freeFrame(floor_output_frame);
                (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
                cnr3_set_filter_error(
                    frame_ctx,
                    vsapi,
                    "CNR3 D.3 floor-fresh-start proof: failed to adopt floor fresh-start output."
                );
                return nullptr;
            }

            floor_output_frame = nullptr;

            Cnr3CacheAs2StoreRecordSummary floor_store_summary{};
            const Cnr3Status floor_store_status =
                data.output_cache.store_owned_frame_and_record_pin(
                    floor_frame,
                    std::move(floor_owned_frame),
                    cnr3_live_output_frame_is_checkpoint(floor_frame),
                    request_data->pin_list,
                    floor_store_summary
                );

            if (!cnr3_status_is_ok(floor_store_status) ||
                !floor_store_summary.pin_recorded) {
                (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
                cnr3_set_filter_error(
                    frame_ctx,
                    vsapi,
                    "CNR3 D.3 floor-fresh-start proof: failed to store/pin floor fresh-start output."
                );
                return nullptr;
            }

            request_data->recovery_floor_outcome =
                floor_store_summary.duplicate_existing_slot
                ? Cnr3LiveRecoveryHoleOutcome::adopted_post_compute_loser
                : Cnr3LiveRecoveryHoleOutcome::computed;
        }
    }

    if (floor_fresh_start_recovery) {
        /*
            The bounded-search plan had no pre-existing anchor. After the floor
            output has been fresh-started or adopted, stored, and pinned, record
            that materialized floor as the carried plan's anchor so the existing
            planned-hole AS2 wrapper can validate the walked holes.

            This is a now-true consumer-foundation fact, not a bypassed
            precondition. The wrapper validates the structural walk contract: a
            present, pinned foundation at anchor_frame_number, plus contiguous
            holes above it. It does not care whether that foundation was found by
            the bounded search or created by the floor fallback.

            Keep this after the floor pin is recorded in pin_list and before any
            planned-hole wrapper call. Moving it earlier would assert a premature
            anchor; moving it later would make the hole wrapper see the old
            no-anchor plan.
        */
        recovery_plan.anchor_found = true;
        recovery_plan.anchor_frame_number = request_data->recovery_floor_frame;
        recovery_plan.anchor_is_checkpoint =
            cnr3_live_output_frame_is_checkpoint(request_data->recovery_floor_frame);
        recovery_plan.anchor_pin_recorded = true;
    }

    for (std::size_t hole_index = 0U;
        hole_index < recovery_plan.hole_frame_numbers.size();
        ++hole_index
        ) {
        const int hole_frame = recovery_plan.hole_frame_numbers[hole_index];

        if (!cnr3_live_recovery_source_was_requested(*request_data, hole_frame)) {
            (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 D.3 floor-fresh-start proof: hole source was not requested at arInitial."
            );
            return nullptr;
        }

        const Cnr3Status adopt_status = data.output_cache.lookup_frame_and_record_pin(
            hole_frame,
            request_data->pin_list
        );

        if (cnr3_status_is_ok(adopt_status)) {
            request_data->per_hole_outcomes[hole_index] =
                Cnr3LiveRecoveryHoleOutcome::adopted_skipped;
            continue;
        }

        if (adopt_status != Cnr3Status::not_found) {
            (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 D.3 floor-fresh-start proof: pre-compute hole adopt-and-skip lookup failed."
            );
            return nullptr;
        }

        Cnr3OwnedFrameRef predecessor_compute_ref{};
        const Cnr3Status predecessor_status = data.output_cache.lookup_frame_and_add_ref(
            hole_frame - 1,
            vsapi,
            predecessor_compute_ref
        );

        if (!cnr3_status_is_ok(predecessor_status) ||
            !predecessor_compute_ref.has_frame()) {
            (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 D.3 floor-fresh-start proof: failed to acquire hole predecessor compute reference."
            );
            return nullptr;
        }

        const VSFrame* source_frame = vsapi->getFrameFilter(
            hole_frame,
            data.source,
            frame_ctx
        );

        if (source_frame == nullptr) {
            predecessor_compute_ref.reset();
            (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 D.3 floor-fresh-start proof: hole source frame retrieval failed."
            );
            return nullptr;
        }

        VSFrame* hole_output_frame = vsapi->copyFrame(source_frame, core);

        if (hole_output_frame == nullptr) {
            predecessor_compute_ref.reset();
            vsapi->freeFrame(source_frame);
            (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 D.3 floor-fresh-start proof: hole copyFrame failed."
            );
            return nullptr;
        }

        if (hole_output_frame == source_frame) {
            predecessor_compute_ref.reset();
            vsapi->freeFrame(hole_output_frame);
            (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 D.3 floor-fresh-start proof: hole copyFrame returned the source frame alias."
            );
            return nullptr;
        }

        Cnr3CallerSuppliedFrameProcessSummary hole_process_summary{};
        const Cnr3Status hole_process_status =
            cnr3_process_caller_supplied_vapoursynth_frame_triplet(
                source_frame,
                predecessor_compute_ref.get(),
                hole_output_frame,
                vsapi,
                data.bits_per_sample,
                data.sub_sampling_w,
                data.sub_sampling_h,
                data.response_tables,
                hole_process_summary
            );

        predecessor_compute_ref.reset();
        vsapi->freeFrame(source_frame);
        source_frame = nullptr;

        if (!cnr3_status_is_ok(hole_process_status) ||
            !hole_process_summary.frame_processed) {
            vsapi->freeFrame(hole_output_frame);
            (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 D.3 floor-fresh-start proof: P.11B hole processing failed."
            );
            return nullptr;
        }

        Cnr3OwnedFrameRef hole_owned_frame{};
        const Cnr3Status adopt_hole_status = hole_owned_frame.reset_to_owned_frame(
            hole_output_frame,
            vsapi
        );

        if (!cnr3_status_is_ok(adopt_hole_status)) {
            vsapi->freeFrame(hole_output_frame);
            (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 D.3 floor-fresh-start proof: failed to adopt computed hole output."
            );
            return nullptr;
        }

        hole_output_frame = nullptr;

        Cnr3CacheAs2StoreRecordSummary hole_store_summary{};
        const Cnr3Status hole_store_status =
            data.output_cache.store_recovery_plan_hole_owned_frame_and_record_pin(
                recovery_plan,
                hole_frame,
                std::move(hole_owned_frame),
                cnr3_live_output_frame_is_checkpoint(hole_frame),
                request_data->pin_list,
                hole_store_summary
            );

        if (!cnr3_status_is_ok(hole_store_status) ||
            !hole_store_summary.pin_recorded) {
            (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 D.3 floor-fresh-start proof: failed to store/pin computed recovery hole."
            );
            return nullptr;
        }

        request_data->per_hole_outcomes[hole_index] =
            hole_store_summary.duplicate_existing_slot
            ? Cnr3LiveRecoveryHoleOutcome::adopted_post_compute_loser
            : Cnr3LiveRecoveryHoleOutcome::computed;
    }

    if (!cnr3_live_recovery_source_was_requested(*request_data, n)) {
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 D.3 floor-fresh-start proof: target source was not requested at arInitial."
        );
        return nullptr;
    }

    Cnr3OwnedFrameRef target_predecessor_compute_ref{};
    const Cnr3Status target_predecessor_status =
        data.output_cache.lookup_frame_and_add_ref(
            n - 1,
            vsapi,
            target_predecessor_compute_ref
        );

    if (!cnr3_status_is_ok(target_predecessor_status) ||
        !target_predecessor_compute_ref.has_frame()) {
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 D.3 floor-fresh-start proof: failed to acquire target predecessor compute reference."
        );
        return nullptr;
    }

    const VSFrame* target_source_frame = vsapi->getFrameFilter(n, data.source, frame_ctx);

    if (target_source_frame == nullptr) {
        target_predecessor_compute_ref.reset();
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 D.3 floor-fresh-start proof: target source frame retrieval failed."
        );
        return nullptr;
    }

    VSFrame* target_output_frame = vsapi->copyFrame(target_source_frame, core);

    if (target_output_frame == nullptr) {
        target_predecessor_compute_ref.reset();
        vsapi->freeFrame(target_source_frame);
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 D.3 floor-fresh-start proof: target copyFrame failed."
        );
        return nullptr;
    }

    if (target_output_frame == target_source_frame) {
        target_predecessor_compute_ref.reset();
        vsapi->freeFrame(target_output_frame);
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 D.3 floor-fresh-start proof: target copyFrame returned the source frame alias."
        );
        return nullptr;
    }

    Cnr3CallerSuppliedFrameProcessSummary target_process_summary{};
    const Cnr3Status target_process_status =
        cnr3_process_caller_supplied_vapoursynth_frame_triplet(
            target_source_frame,
            target_predecessor_compute_ref.get(),
            target_output_frame,
            vsapi,
            data.bits_per_sample,
            data.sub_sampling_w,
            data.sub_sampling_h,
            data.response_tables,
            target_process_summary
        );

    target_predecessor_compute_ref.reset();
    vsapi->freeFrame(target_source_frame);
    target_source_frame = nullptr;

    if (!cnr3_status_is_ok(target_process_status) ||
        !target_process_summary.frame_processed) {
        vsapi->freeFrame(target_output_frame);
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 D.3 floor-fresh-start proof: P.11B target processing failed."
        );
        return nullptr;
    }

    const VSFrame* return_frame = nullptr;
    Cnr3Status target_store_status = Cnr3Status::invariant_violation;
    bool returned_cached_winner = false;
    const Cnr3Status target_return_status =
        cnr3_store_live_output_frame_for_authoritative_return(
            data,
            Cnr3LiveOutputStoreRequest{ n, false },
            target_output_frame,
            vsapi,
            return_frame,
            target_store_status,
            returned_cached_winner
        );
    target_output_frame = nullptr;

    if (!cnr3_status_is_ok(target_return_status) || return_frame == nullptr) {
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 D.3 floor-fresh-start proof: failed to store/return authoritative target output."
        );
        return nullptr;
    }

    (void)target_store_status;
    (void)returned_cached_winner;

    const Cnr3LiveRecoveryBranch recovery_branch_for_trace =
        request_data->recovery_branch;
    const int recovery_floor_frame_for_trace =
        request_data->recovery_floor_frame;
    const Cnr3LiveRecoveryHoleOutcome floor_outcome_for_trace =
        request_data->recovery_floor_outcome;
    const Cnr3CacheRecoverySearchPlan recovery_plan_for_trace =
        request_data->recovery_plan;
    const std::vector<int> source_requests_for_trace =
        request_data->source_request_frame_numbers;
    const std::vector<Cnr3LiveRecoveryHoleOutcome> hole_outcomes_for_trace =
        request_data->per_hole_outcomes;
    const std::size_t pin_list_size_before_discharge =
        request_data->pin_list.pin_count();

    const Cnr3Status discard_status = cnr3_discard_frame_data_with_cache(
        frame_data,
        data.output_cache
    );

    if (!cnr3_status_is_ok(discard_status)) {
        vsapi->freeFrame(return_frame);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 D.3 floor-fresh-start proof: failed to discharge recovery pin-list."
        );
        return nullptr;
    }

    cnr3_trace_live_recovery(
        data,
        n,
        recovery_branch_for_trace,
        recovery_floor_frame_for_trace,
        floor_outcome_for_trace,
        recovery_plan_for_trace,
        source_requests_for_trace,
        hole_outcomes_for_trace,
        pin_list_size_before_discharge,
        target_process_summary
    );

    return return_frame;
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
    case Cnr3LiveGetFrameBranch::recovery:
        return cnr3_complete_live_recovery(
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

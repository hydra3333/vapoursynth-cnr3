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
#include <cstdint>
#include <cstdio>
#include <limits>
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


#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)

void cnr3_diag_plantrace_add_store_result_frame(
    Cnr3DiagPlanTraceResultFields& fields,
    int frame_number,
    Cnr3Status store_status
) {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return;
    }

    if (store_status == Cnr3Status::duplicate) {
        fields.post_compute_loser_frames.push_back(frame_number);
        return;
    }

    fields.computed_frames.push_back(frame_number);
}

void cnr3_diag_plantrace_add_recovery_outcome_frame(
    Cnr3DiagPlanTraceResultFields& fields,
    int frame_number,
    Cnr3LiveRecoveryHoleOutcome outcome
) {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return;
    }

    switch (outcome) {
    case Cnr3LiveRecoveryHoleOutcome::computed:
        fields.computed_frames.push_back(frame_number);
        return;
    case Cnr3LiveRecoveryHoleOutcome::adopted_skipped:
        fields.adopted_skipped_frames.push_back(frame_number);
        return;
    case Cnr3LiveRecoveryHoleOutcome::adopted_post_compute_loser:
        fields.post_compute_loser_frames.push_back(frame_number);
        return;
    case Cnr3LiveRecoveryHoleOutcome::none:
    default:
        return;
    }
}

Cnr3DiagPlanTraceResultFields cnr3_diag_plantrace_make_cache_hit_result() {
    Cnr3DiagPlanTraceResultFields fields{};
    fields.outcome = Cnr3DiagPlanTraceOutcome::returned_cache_hit;
    return fields;
}

Cnr3DiagPlanTraceResultFields cnr3_diag_plantrace_make_computed_result(
    int frame_number,
    Cnr3Status store_status
) {
    Cnr3DiagPlanTraceResultFields fields{};
    fields.outcome = Cnr3DiagPlanTraceOutcome::returned_computed;
    cnr3_diag_plantrace_add_store_result_frame(fields, frame_number, store_status);

    return fields;
}

Cnr3DiagPlanTraceResultFields cnr3_diag_plantrace_make_recovery_result(
    int frame_number,
    Cnr3LiveRecoveryBranch recovery_branch,
    int recovery_floor_frame,
    Cnr3LiveRecoveryHoleOutcome floor_outcome,
    const Cnr3CacheRecoverySearchPlan& recovery_plan,
    const std::vector<Cnr3LiveRecoveryHoleOutcome>& hole_outcomes,
    Cnr3Status target_store_status
) {
    Cnr3DiagPlanTraceResultFields fields{};
    fields.outcome = Cnr3DiagPlanTraceOutcome::returned_recovered;

    if (recovery_branch == Cnr3LiveRecoveryBranch::floor_fresh_start) {
        cnr3_diag_plantrace_add_recovery_outcome_frame(
            fields,
            recovery_floor_frame,
            floor_outcome
        );
    }

    const std::size_t hole_count = recovery_plan.hole_frame_numbers.size();
    for (std::size_t i = 0U; i < hole_count; ++i) {
        const Cnr3LiveRecoveryHoleOutcome outcome =
            i < hole_outcomes.size() ? hole_outcomes[i] : Cnr3LiveRecoveryHoleOutcome::none;
        cnr3_diag_plantrace_add_recovery_outcome_frame(
            fields,
            recovery_plan.hole_frame_numbers[i],
            outcome
        );
    }

    cnr3_diag_plantrace_add_store_result_frame(fields, frame_number, target_store_status);
    return fields;
}

void cnr3_diag_plantrace_add_frame_once(
    std::vector<int>& frames,
    int frame_number
) {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return;
    }

    for (const int existing_frame : frames) {
        if (existing_frame == frame_number) {
            return;
        }
    }

    frames.push_back(frame_number);
}

Cnr3DiagPlanTraceResultFields cnr3_diag_plantrace_make_failed_result_from_request(
    const Cnr3LiveGetFrameFrameData* request_data,
    Cnr3DiagPlanTraceFailReason fail_reason,
    int error_frame
) {
    return cnr3_live_plantrace_make_failed_result_from_request(
        request_data,
        fail_reason,
        error_frame
    );
}


void cnr3_diag_plantrace_observe_failed_from_request(
    Cnr3FilterData& data,
    int frame_number,
    const Cnr3LiveGetFrameFrameData* request_data,
    Cnr3DiagPlanTraceFailReason fail_reason,
    int error_frame
) noexcept {
    const Cnr3DiagPlanTraceTick enter_tick =
        request_data != nullptr && request_data->plantrace_ar_all_enter_tick != 0
        ? request_data->plantrace_ar_all_enter_tick
        : cnr3_diag_plantrace_sample_tick();

    cnr3_diag_plantrace_observe_failed_result_and_dump(
        data.config.instance_id,
        data.dsum_plantrace,
        frame_number,
        enter_tick,
        cnr3_diag_plantrace_make_failed_result_from_request(
            request_data,
            fail_reason,
            error_frame
        )
    );
}

void cnr3_diag_plantrace_observe_failed_with_progress(
    Cnr3FilterData& data,
    int frame_number,
    Cnr3DiagPlanTraceTick enter_tick,
    const Cnr3DiagPlanTraceResultFields& progress_fields,
    Cnr3DiagPlanTraceFailReason fail_reason,
    int error_frame
) noexcept {
    Cnr3DiagPlanTraceResultFields fields = progress_fields;
    fields.outcome = Cnr3DiagPlanTraceOutcome::failed;
    fields.fail_reason = fail_reason;
    fields.not_reached_frames.clear();
    fields.error_here_frames.clear();
    cnr3_diag_plantrace_add_frame_once(fields.error_here_frames, error_frame);

    cnr3_diag_plantrace_observe_failed_result_and_dump(
        data.config.instance_id,
        data.dsum_plantrace,
        frame_number,
        enter_tick,
        fields
    );
}

#endif

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


int cnr3_live_recovery_foundation_frame(
    const Cnr3LiveGetFrameFrameData& request_data
) noexcept {
    if (request_data.recovery_branch == Cnr3LiveRecoveryBranch::floor_fresh_start) {
        return request_data.recovery_floor_frame;
    }

    return request_data.recovery_plan.anchor_frame_number;
}

int cnr3_live_recovery_depth_from_foundation(
    const Cnr3LiveGetFrameFrameData& request_data,
    int frame_number
) noexcept {
    const int foundation_frame = cnr3_live_recovery_foundation_frame(request_data);

    if (!cnr3_frame_number_is_valid(frame_number) ||
        !cnr3_frame_number_is_valid(foundation_frame) ||
        frame_number < foundation_frame) {
        return 0;
    }

    return frame_number - foundation_frame;
}

#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN)

void cnr3_diag_live_observe_recovery_plan_destroyed_if_needed(
    const Cnr3LiveGetFrameFrameData& request_data
) noexcept {
    if (request_data.branch == Cnr3LiveGetFrameBranch::recovery &&
        request_data.dsum12_recovery_plan_stats != nullptr) {
        cnr3_diag_dsum12_observe_recovery_plan_destroyed(
            *request_data.dsum12_recovery_plan_stats
        );
    }
}

void cnr3_diag_live_observe_recovery_fallback_failure_if_needed(
    const Cnr3LiveGetFrameFrameData& request_data
) noexcept {
    if (request_data.branch == Cnr3LiveGetFrameBranch::recovery &&
        request_data.dsum12_recovery_plan_stats != nullptr) {
        cnr3_diag_dsum12_observe_fallback_failure(
            *request_data.dsum12_recovery_plan_stats
        );
    }
}

void cnr3_diag_live_observe_recovery_honesty_failure_if_needed(
    const Cnr3LiveGetFrameFrameData& request_data
) noexcept {
    if (request_data.branch == Cnr3LiveGetFrameBranch::recovery &&
        request_data.dsum12_recovery_plan_stats != nullptr) {
        cnr3_diag_dsum12_observe_bounded_start_honesty_failure(
            *request_data.dsum12_recovery_plan_stats
        );
    }
}

#endif

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
    const std::vector<bool>& per_hole_scene_summary_available,
    const std::vector<Cnr3CallerSuppliedFrameProcessSummary>& per_hole_process_summaries,
    const std::vector<Cnr3CacheAs2StoreRecordSummary>& per_hole_store_summaries,
    const std::vector<bool>& per_hole_store_as_checkpoint,
    std::size_t pin_list_size_before_discharge,
    const Cnr3CallerSuppliedFrameProcessSummary& target_process_summary,
    bool target_store_as_checkpoint,
    Cnr3Status target_store_status
) noexcept {
#if defined(CNR3_KEYSTONE_DEV_TRACE)
    const std::string holes_text =
        cnr3_join_frame_numbers_for_kdt(recovery_plan.hole_frame_numbers);
    const std::string source_requests_text =
        cnr3_join_frame_numbers_for_kdt(source_request_frame_numbers);
    const bool target_resulting_slot_is_checkpoint_expected =
        target_store_as_checkpoint &&
        (target_store_status == Cnr3Status::ok ||
            target_store_status == Cnr3Status::duplicate);

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

        const bool scene_summary_available =
            i < per_hole_scene_summary_available.size() &&
            per_hole_scene_summary_available[i] &&
            i < per_hole_process_summaries.size() &&
            i < per_hole_store_summaries.size() &&
            i < per_hole_store_as_checkpoint.size();

        if (scene_summary_available) {
            const Cnr3CallerSuppliedFrameProcessSummary& hole_process_summary =
                per_hole_process_summaries[i];
            const Cnr3CacheAs2StoreRecordSummary& hole_store_summary =
                per_hole_store_summaries[i];

            per_hole_text += " hole_scene_change_detection_used=";
            per_hole_text += hole_process_summary.scene_change_detection_used ? "1" : "0";
            per_hole_text += " hole_scene_change_detected=";
            per_hole_text += hole_process_summary.scene_change_detected ? "1" : "0";
            per_hole_text += " hole_scene_change_reset_output_used=";
            per_hole_text += hole_process_summary.scene_change_reset_output_used ? "1" : "0";
            per_hole_text += " hole_recursive_chroma_blend_used=";
            per_hole_text += hole_process_summary.recursive_chroma_blend_used ? "1" : "0";
            per_hole_text += " hole_scene_change_diff_total=";
            per_hole_text += std::to_string(hole_process_summary.scene_change_diff_total);
            per_hole_text += " hole_scene_change_samples_examined=";
            per_hole_text += std::to_string(hole_process_summary.scene_change_samples_examined);
            per_hole_text += " hole_store_as_checkpoint=";
            per_hole_text += per_hole_store_as_checkpoint[i] ? "1" : "0";
            per_hole_text += " hole_resulting_slot_is_checkpoint=";
            per_hole_text += hole_store_summary.resulting_slot_is_checkpoint ? "1" : "0";
            per_hole_text += " hole_checkpoint_promoted=";
            per_hole_text += hole_store_summary.checkpoint_promoted ? "1" : "0";
        }
        else if (outcome == Cnr3LiveRecoveryHoleOutcome::adopted_skipped) {
            per_hole_text += " hole_scene_change_detection_used=0";
            per_hole_text += " hole_scene_change_not_run=1";
        }
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
            "pixel_compute=%d p11b_called=%d p11c_called=1 scene_change_deferred=0 "
            "target_scene_change_detection_used=%d target_scene_chroma_used=%d "
            "target_scene_change_threshold=%lld target_scene_change_diff_total=%lld "
            "target_scene_change_samples_examined=%d target_scene_change_detected=%d "
            "target_scene_change_reset_output_used=%d target_recursive_chroma_blend_used=%d "
            "target_store_as_checkpoint=%d target_resulting_slot_is_checkpoint_expected=%d "
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
            target_process_summary.scene_change_detection_used ? 1 : 0,
            target_process_summary.scene_chroma_used ? 1 : 0,
            static_cast<long long>(target_process_summary.scene_change_threshold),
            static_cast<long long>(target_process_summary.scene_change_diff_total),
            target_process_summary.scene_change_samples_examined,
            target_process_summary.scene_change_detected ? 1 : 0,
            target_process_summary.scene_change_reset_output_used ? 1 : 0,
            target_process_summary.recursive_chroma_blend_used ? 1 : 0,
            target_store_as_checkpoint ? 1 : 0,
            target_resulting_slot_is_checkpoint_expected ? 1 : 0,
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
        "anchor=%d anchor_is_checkpoint=%d hole_count=%zu holes=%s source_requests=%s %s "
        "pixel_compute=%d p11b_called=%d p11c_called=1 scene_change_deferred=0 "
        "target_scene_change_detection_used=%d target_scene_chroma_used=%d "
        "target_scene_change_threshold=%lld target_scene_change_diff_total=%lld "
        "target_scene_change_samples_examined=%d target_scene_change_detected=%d "
        "target_scene_change_reset_output_used=%d target_recursive_chroma_blend_used=%d "
        "target_store_as_checkpoint=%d target_resulting_slot_is_checkpoint_expected=%d "
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
        recovery_plan.anchor_is_checkpoint ? 1 : 0,
        recovery_plan.hole_frame_numbers.size(),
        holes_text.c_str(),
        source_requests_text.c_str(),
        per_hole_text.c_str(),
        target_process_summary.frame_processed ? 1 : 0,
        target_process_summary.frame_processed ? 1 : 0,
        target_process_summary.scene_change_detection_used ? 1 : 0,
        target_process_summary.scene_chroma_used ? 1 : 0,
        static_cast<long long>(target_process_summary.scene_change_threshold),
        static_cast<long long>(target_process_summary.scene_change_diff_total),
        target_process_summary.scene_change_samples_examined,
        target_process_summary.scene_change_detected ? 1 : 0,
        target_process_summary.scene_change_reset_output_used ? 1 : 0,
        target_process_summary.recursive_chroma_blend_used ? 1 : 0,
        target_store_as_checkpoint ? 1 : 0,
        target_resulting_slot_is_checkpoint_expected ? 1 : 0,
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
    (void)per_hole_scene_summary_available;
    (void)per_hole_process_summaries;
    (void)per_hole_store_summaries;
    (void)per_hole_store_as_checkpoint;
    (void)pin_list_size_before_discharge;
    (void)target_process_summary;
    (void)target_store_as_checkpoint;
    (void)target_store_status;
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

#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN)
    cnr3_diag_live_observe_recovery_plan_destroyed_if_needed(*request_data);
#endif

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


#if defined(CNR3_DIAG_COMPUTE_DSUM06_SOURCE_FRAME_LIFECYCLE)

bool cnr3_live_source_frame_was_requested_in_activation(
    const Cnr3LiveGetFrameFrameData& request_data,
    int frame_number
) noexcept {
    if (!request_data.source_requested) {
        return false;
    }

    if (request_data.source_request_frame_numbers.empty()) {
        return request_data.requested_frame == frame_number;
    }

    for (const int requested_source_frame : request_data.source_request_frame_numbers) {
        if (requested_source_frame == frame_number) {
            return true;
        }
    }

    return false;
}

void cnr3_diag_live_observe_source_retrieve(
    Cnr3FilterData& data,
    const Cnr3LiveGetFrameFrameData& request_data,
    int frame_number,
    const VSFrame* frame
) noexcept {
    cnr3_diag_dsum06_observe_source_retrieve(
        data.dsum06_source_frame_lifecycle,
        cnr3_live_source_frame_was_requested_in_activation(
            request_data,
            frame_number
        ),
        frame != nullptr
    );
}

void cnr3_diag_live_observe_source_release(
    Cnr3FilterData& data
) noexcept {
    cnr3_diag_dsum06_observe_source_release(
        data.dsum06_source_frame_lifecycle
    );
}

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM14_SCENE_RESET)

void cnr3_diag_live_observe_scene_outcome(
    Cnr3FilterData& data,
    int frame_number,
    const Cnr3CallerSuppliedFrameProcessSummary& process_summary,
    bool checkpoint_store_requested,
    Cnr3Status checkpoint_store_status,
    bool checkpoint_promoted
) noexcept {
    cnr3_diag_dsum14_observe_scene_outcome(
        data.dsum14_scene_reset,
        frame_number,
        process_summary.scene_chroma_used,
        static_cast<long long>(process_summary.scene_change_threshold),
        process_summary.scene_change_detected,
        process_summary.scene_change_reset_output_used,
        checkpoint_store_requested,
        checkpoint_store_status,
        checkpoint_promoted
    );
}

#endif

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

Cnr3Status cnr3_live_calculate_output_frame_byte_count(
    const Cnr3FilterData& data,
    std::uint64_t& out_frame_byte_count
) noexcept {
    out_frame_byte_count = 0U;

    if (
        data.video_info.width <= 0 ||
        data.video_info.height <= 0 ||
        data.bits_per_sample <= 0 ||
        data.sub_sampling_w < 0 ||
        data.sub_sampling_h < 0 ||
        data.sub_sampling_w >= 31 ||
        data.sub_sampling_h >= 31
        ) {
        return Cnr3Status::invalid_argument;
    }

    const std::uint64_t width = static_cast<std::uint64_t>(data.video_info.width);
    const std::uint64_t height = static_cast<std::uint64_t>(data.video_info.height);
    const std::uint64_t bytes_per_sample =
        data.bits_per_sample > 8 ? 2ULL : 1ULL;
    const std::uint64_t chroma_w_den = 1ULL << data.sub_sampling_w;
    const std::uint64_t chroma_h_den = 1ULL << data.sub_sampling_h;
    const std::uint64_t chroma_width =
        (width + chroma_w_den - 1ULL) >> data.sub_sampling_w;
    const std::uint64_t chroma_height =
        (height + chroma_h_den - 1ULL) >> data.sub_sampling_h;

    const auto checked_mul = [](std::uint64_t left,
        std::uint64_t right,
        std::uint64_t& out_value
    ) noexcept -> bool {
        if (left != 0U && right > (std::numeric_limits<std::uint64_t>::max() / left)) {
            return false;
        }

        out_value = left * right;
        return true;
    };

    std::uint64_t luma_samples = 0U;
    std::uint64_t chroma_samples = 0U;
    std::uint64_t two_chroma_samples = 0U;
    std::uint64_t total_samples = 0U;
    std::uint64_t total_bytes = 0U;

    if (
        !checked_mul(width, height, luma_samples) ||
        !checked_mul(chroma_width, chroma_height, chroma_samples) ||
        !checked_mul(chroma_samples, 2ULL, two_chroma_samples)
        ) {
        return Cnr3Status::capacity_exceeded;
    }

    if (luma_samples > std::numeric_limits<std::uint64_t>::max() - two_chroma_samples) {
        return Cnr3Status::capacity_exceeded;
    }

    total_samples = luma_samples + two_chroma_samples;

    if (!checked_mul(total_samples, bytes_per_sample, total_bytes) || total_bytes == 0U) {
        return Cnr3Status::capacity_exceeded;
    }

    out_frame_byte_count = total_bytes;
    return Cnr3Status::ok;
}

void cnr3_trace_live_combined_store_and_prune(
    const Cnr3FilterData& data,
    const Cnr3CombinedStoreAndPruneSummary& summary
) noexcept {
#if defined(CNR3_KEYSTONE_DEV_TRACE)
    std::fprintf(
        stderr,
        "[KDT] instance=%d profile=%s target_N=%d stored_frame=%d kind=%s "
        "store=%s retire=%s prune=%s "
        "cap_trigger=%d ckpt_trigger=%d selected=%zu detached=%zu\n",
        data.config.instance_id.value,
        CNR3_CACHE_PROFILE_NAME,
        summary.activation_target_frame,
        summary.stored_frame_number,
        cnr3_cache_store_kind_name(summary.store_kind),
        cnr3_status_name(summary.store_status),
        cnr3_status_name(summary.retire_status),
        cnr3_status_name(summary.prune_status),
        summary.prune_summary.trigger_decision.prune_is_required ? 1 : 0,
        summary.prune_summary.trigger_decision.checkpoint_prune_is_required ? 1 : 0,
        summary.prune_summary.selected_candidate_count,
        summary.prune_summary.detached_count
    );
#else
    (void)data;
    (void)summary;
#endif
}

Cnr3Status cnr3_store_live_output_frame_for_return(
    Cnr3FilterData& data,
    const Cnr3LiveOutputStoreRequest& request,
    VSFrame* output_frame,
    const VSAPI* vsapi,
    std::uint64_t frame_byte_count,
    Cnr3CombinedStoreAndPruneSummary& out_summary
) noexcept {
    out_summary = Cnr3CombinedStoreAndPruneSummary{};

    if (
        request.frame_number < 0 ||
        output_frame == nullptr ||
        vsapi == nullptr ||
        frame_byte_count == 0U
        ) {
        out_summary.store_status = Cnr3Status::invalid_argument;
        return Cnr3Status::invalid_argument;
    }

    const VSFrame* cache_frame_ref = vsapi->addFrameRef(output_frame);

    if (cache_frame_ref == nullptr) {
        out_summary.store_status = Cnr3Status::vapoursynth_error;
        return Cnr3Status::vapoursynth_error;
    }

    Cnr3OwnedFrameRef cache_owned_frame{};
    const Cnr3Status adopt_status = cache_owned_frame.reset_to_owned_frame(
        cache_frame_ref,
        vsapi
    );

    if (!cnr3_status_is_ok(adopt_status)) {
        vsapi->freeFrame(cache_frame_ref);
        out_summary.store_status = adopt_status;
        return adopt_status;
    }

    const bool store_as_checkpoint =
        cnr3_live_output_frame_should_store_as_checkpoint(request);

    return data.output_cache.store_production_output_and_prune(
        request.frame_number,
        request.frame_number,
        std::move(cache_owned_frame),
        store_as_checkpoint,
        frame_byte_count,
        out_summary
    );
}


Cnr3Status cnr3_store_live_output_frame_for_authoritative_return(
    Cnr3FilterData& data,
    const Cnr3LiveOutputStoreRequest& request,
    VSFrame* output_frame,
    const VSAPI* vsapi,
    std::uint64_t frame_byte_count,
    const VSFrame*& out_return_frame,
    Cnr3Status& out_store_status,
    bool& out_returned_cached_winner
) noexcept {
    out_return_frame = nullptr;
    out_store_status = Cnr3Status::invariant_violation;
    out_returned_cached_winner = false;

    if (output_frame == nullptr || vsapi == nullptr) {
        if (output_frame != nullptr && vsapi != nullptr) {
#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)
            cnr3_diag_dsum07_observe_temporary_output_released(
                data.dsum07_temp_output_lifecycle
            );
#endif
            vsapi->freeFrame(output_frame);
        }

        out_store_status = Cnr3Status::invalid_argument;
#if defined(CNR3_DIAG_COMPUTE_DSUM09_RETURN_TRANSFER)
        cnr3_diag_dsum09_observe_return_decision(
            data.dsum09_return_transfer,
            false,
            Cnr3DiagDsum09ReturnNoReason::hard_store_failure
        );
#endif
        return out_store_status;
    }

    Cnr3CombinedStoreAndPruneSummary store_summary{};
    const Cnr3Status hard_store_status = cnr3_store_live_output_frame_for_return(
        data,
        request,
        output_frame,
        vsapi,
        frame_byte_count,
        store_summary
    );
    cnr3_trace_live_combined_store_and_prune(data, store_summary);
    out_store_status = store_summary.store_status;

    if (!cnr3_status_is_ok(hard_store_status)) {
#if defined(CNR3_DIAG_COMPUTE_DSUM09_RETURN_TRANSFER)
        cnr3_diag_dsum09_observe_return_decision(
            data.dsum09_return_transfer,
            false,
            Cnr3DiagDsum09ReturnNoReason::hard_store_failure
        );
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)
        cnr3_diag_dsum07_observe_temporary_output_released(
            data.dsum07_temp_output_lifecycle
        );
#endif
        vsapi->freeFrame(output_frame);
        return hard_store_status;
    }

    if (out_store_status == Cnr3Status::ok) {
#if defined(CNR3_DIAG_COMPUTE_DSUM09_RETURN_TRANSFER)
        cnr3_diag_dsum09_observe_return_decision(
            data.dsum09_return_transfer,
            true,
            Cnr3DiagDsum09ReturnNoReason::store_status_not_returnable
        );
        cnr3_diag_dsum09_observe_return_transfer(
            data.dsum09_return_transfer,
            true,
            true
        );
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)
        cnr3_diag_dsum07_observe_temporary_output_transferred(
            data.dsum07_temp_output_lifecycle
        );
#endif
        out_return_frame = output_frame;
        return Cnr3Status::ok;
    }

    if (out_store_status != Cnr3Status::duplicate) {
#if defined(CNR3_DIAG_COMPUTE_DSUM09_RETURN_TRANSFER)
        cnr3_diag_dsum09_observe_return_decision(
            data.dsum09_return_transfer,
            false,
            Cnr3DiagDsum09ReturnNoReason::store_status_not_returnable
        );
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)
        cnr3_diag_dsum07_observe_temporary_output_released(
            data.dsum07_temp_output_lifecycle
        );
#endif
        vsapi->freeFrame(output_frame);
        return out_store_status;
    }

#if defined(CNR3_DIAG_COMPUTE_DSUM09_RETURN_TRANSFER)
    cnr3_diag_dsum09_observe_return_decision(
        data.dsum09_return_transfer,
        true,
        Cnr3DiagDsum09ReturnNoReason::store_status_not_returnable
    );
#endif

    /*
        A duplicate target store means another activation's first-in-best-
        dressed output[N] is authoritative. Discard this activation's computed
        loser and return a fresh reference to the cached winner instead.
    */
#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)
    cnr3_diag_dsum07_observe_temporary_output_released(
        data.dsum07_temp_output_lifecycle
    );
    cnr3_diag_dsum07_observe_duplicate_computed_but_discarded(
        data.dsum07_temp_output_lifecycle
    );
#endif
    vsapi->freeFrame(output_frame);
    output_frame = nullptr;

    Cnr3OwnedFrameRef cached_winner_ref{};
    const Cnr3Status lookup_status = data.output_cache.lookup_frame_and_add_ref(
        request.frame_number,
        vsapi,
        cached_winner_ref,
        Cnr3LookupCountPolicy::none,
        Cnr3LookupSite::duplicate_winner_reacquire
    );

    if (!cnr3_status_is_ok(lookup_status) || !cached_winner_ref.has_frame()) {
#if defined(CNR3_DIAG_COMPUTE_DSUM09_RETURN_TRANSFER)
        cnr3_diag_dsum09_observe_return_no_reason(
            data.dsum09_return_transfer,
            Cnr3DiagDsum09ReturnNoReason::duplicate_winner_lookup_failed
        );
#endif
        return !cnr3_status_is_ok(lookup_status)
            ? lookup_status
            : Cnr3Status::invariant_violation;
    }

#if defined(CNR3_DIAG_COMPUTE_DSUM09_RETURN_TRANSFER)
    cnr3_diag_dsum09_observe_lookup_ref_acquired(data.dsum09_return_transfer);
    cnr3_diag_dsum09_observe_return_transfer(
        data.dsum09_return_transfer,
        true,
        true
    );
    cnr3_diag_dsum09_observe_lookup_ref_transferred(data.dsum09_return_transfer);
#endif
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
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_from_request(
            data,
            n,
            request_data,
            Cnr3DiagPlanTraceFailReason::invalid_lifecycle,
            n
        );
#endif
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1F cache-hit proof: invalid frameData cache-hit lifecycle."
        );
        return nullptr;
    }

    const VSFrame* source_trigger_frame = vsapi->getFrameFilter(n, data.source, frame_ctx);
#if defined(CNR3_DIAG_COMPUTE_DSUM06_SOURCE_FRAME_LIFECYCLE)
    cnr3_diag_live_observe_source_retrieve(data, *request_data, n, source_trigger_frame);
#endif

    if (source_trigger_frame == nullptr) {
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_from_request(
            data,
            n,
            request_data,
            Cnr3DiagPlanTraceFailReason::source_retrieval_failed,
            n
        );
#endif
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
#if defined(CNR3_DIAG_COMPUTE_DSUM06_SOURCE_FRAME_LIFECYCLE)
    cnr3_diag_live_observe_source_release(data);
#endif
    vsapi->freeFrame(source_trigger_frame);
    source_trigger_frame = nullptr;

    Cnr3OwnedFrameRef returned_cache_ref{};
    const Cnr3Status lookup_status = data.output_cache.lookup_frame_and_add_ref(
        n,
        vsapi,
        returned_cache_ref,
        Cnr3LookupCountPolicy::none,
        Cnr3LookupSite::reacquire_already_pinned
    );

    if (!cnr3_status_is_ok(lookup_status) || !returned_cache_ref.has_frame()) {
#if defined(CNR3_DIAG_COMPUTE_DSUM09_RETURN_TRANSFER)
        cnr3_diag_dsum09_observe_return_no_reason(
            data.dsum09_return_transfer,
            Cnr3DiagDsum09ReturnNoReason::duplicate_winner_lookup_failed
        );
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_from_request(
            data,
            n,
            request_data,
            Cnr3DiagPlanTraceFailReason::framedata_missing_or_unknown,
            n
        );
#endif
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1F cache-hit proof: pinned cached output[N] was not retrievable."
        );
        return nullptr;
    }

#if defined(CNR3_DIAG_COMPUTE_DSUM09_RETURN_TRANSFER)
    cnr3_diag_dsum09_observe_lookup_ref_acquired(data.dsum09_return_transfer);
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
    const Cnr3DiagPlanTraceTick plantrace_enter_tick =
        request_data->plantrace_ar_all_enter_tick;
    const Cnr3DiagPlanTraceResultFields plantrace_result_fields =
        cnr3_diag_plantrace_make_cache_hit_result();
#endif
    const Cnr3Status discard_status = cnr3_discard_frame_data_with_cache(
        frame_data,
        data.output_cache
    );

    if (!cnr3_status_is_ok(discard_status)) {
#if defined(CNR3_DIAG_COMPUTE_DSUM09_RETURN_TRANSFER)
        cnr3_diag_dsum09_observe_return_no_reason(
            data.dsum09_return_transfer,
            Cnr3DiagDsum09ReturnNoReason::discard_failed_after_return_ready
        );
        cnr3_diag_dsum09_observe_return_transfer(
            data.dsum09_return_transfer,
            false,
            true
        );
        cnr3_diag_dsum09_observe_lookup_ref_released(data.dsum09_return_transfer);
#endif
        returned_cache_ref.reset();
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_with_progress(
            data,
            n,
            plantrace_enter_tick,
            plantrace_result_fields,
            Cnr3DiagPlanTraceFailReason::discharge_failed,
            n
        );
#endif
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1F cache-hit proof: failed to discharge cache-hit pin-list."
        );
        return nullptr;
    }

    cnr3_trace_live_cache_hit_return(data, n);

#if defined(CNR3_DIAG_COMPUTE_DSUM09_RETURN_TRANSFER)
    cnr3_diag_dsum09_observe_return_transfer(
        data.dsum09_return_transfer,
        true,
        true
    );
    cnr3_diag_dsum09_observe_lookup_ref_transferred(data.dsum09_return_transfer);
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
    cnr3_diag_plantrace_observe_result(
        data.dsum_plantrace,
        n,
        plantrace_enter_tick,
        cnr3_diag_plantrace_sample_tick(),
        plantrace_result_fields
    );
#endif
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
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_from_request(
            data,
            n,
            request_data,
            Cnr3DiagPlanTraceFailReason::invalid_lifecycle,
            n
        );
#endif
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1E.3 sequential proof: invalid frameData predecessor/source lifecycle."
        );
        return nullptr;
    }

    const VSFrame* source_frame = vsapi->getFrameFilter(n, data.source, frame_ctx);
#if defined(CNR3_DIAG_COMPUTE_DSUM06_SOURCE_FRAME_LIFECYCLE)
    cnr3_diag_live_observe_source_retrieve(data, *request_data, n, source_frame);
#endif

    if (source_frame == nullptr) {
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_from_request(
            data,
            n,
            request_data,
            Cnr3DiagPlanTraceFailReason::source_retrieval_failed,
            n
        );
#endif
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
        predecessor_compute_ref,
        Cnr3LookupCountPolicy::none,
        Cnr3LookupSite::reacquire_already_pinned
    );

    if (!cnr3_status_is_ok(predecessor_status) ||
        !predecessor_compute_ref.has_frame()) {
#if defined(CNR3_DIAG_COMPUTE_DSUM06_SOURCE_FRAME_LIFECYCLE)
        cnr3_diag_live_observe_source_release(data);
#endif
        vsapi->freeFrame(source_frame);
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_from_request(
            data,
            n,
            request_data,
            Cnr3DiagPlanTraceFailReason::acquire_ref_failed,
            request_data != nullptr ? request_data->predecessor_frame : n - 1
        );
#endif
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
#if defined(CNR3_DIAG_COMPUTE_DSUM06_SOURCE_FRAME_LIFECYCLE)
        cnr3_diag_live_observe_source_release(data);
#endif
        vsapi->freeFrame(source_frame);
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_from_request(
            data,
            n,
            request_data,
            Cnr3DiagPlanTraceFailReason::copyframe_failed,
            n
        );
#endif
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
#if defined(CNR3_DIAG_COMPUTE_DSUM06_SOURCE_FRAME_LIFECYCLE)
        cnr3_diag_live_observe_source_release(data);
#endif
        vsapi->freeFrame(output_frame);
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_from_request(
            data,
            n,
            request_data,
            Cnr3DiagPlanTraceFailReason::copyframe_source_alias,
            n
        );
#endif
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1E.3 sequential proof: copyFrame returned the source frame alias."
        );
        return nullptr;
    }

#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)
    cnr3_diag_dsum07_observe_temporary_output_created(
        data.dsum07_temp_output_lifecycle
    );
#endif
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
#if defined(CNR3_DIAG_COMPUTE_DSUM06_SOURCE_FRAME_LIFECYCLE)
    cnr3_diag_live_observe_source_release(data);
#endif
    vsapi->freeFrame(source_frame);
    source_frame = nullptr;

    if (!cnr3_status_is_ok(process_status) || !process_summary.frame_processed) {
#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)
        cnr3_diag_dsum07_observe_temporary_output_released(
            data.dsum07_temp_output_lifecycle
        );
#endif
        vsapi->freeFrame(output_frame);
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_from_request(
            data,
            n,
            request_data,
            Cnr3DiagPlanTraceFailReason::scene_processing_failed,
            n
        );
#endif
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

    std::uint64_t frame_byte_count = 0U;
    const Cnr3Status frame_byte_count_status =
        cnr3_live_calculate_output_frame_byte_count(data, frame_byte_count);

    if (!cnr3_status_is_ok(frame_byte_count_status)) {
#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)
        cnr3_diag_dsum07_observe_temporary_output_released(
            data.dsum07_temp_output_lifecycle
        );
#endif
        vsapi->freeFrame(output_frame);
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_from_request(
            data,
            n,
            request_data,
            Cnr3DiagPlanTraceFailReason::byte_estimate_failed,
            n
        );
#endif
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 W.3: failed to compute live output frame byte estimate."
        );
        return nullptr;
    }

    const VSFrame* return_frame = nullptr;
    Cnr3Status store_status = Cnr3Status::invariant_violation;
    bool returned_cached_winner = false;
    const Cnr3Status return_status =
        cnr3_store_live_output_frame_for_authoritative_return(
            data,
            store_request,
            output_frame,
            vsapi,
            frame_byte_count,
            return_frame,
            store_status,
            returned_cached_winner
        );
#if defined(CNR3_DIAG_COMPUTE_DSUM14_SCENE_RESET)
    cnr3_diag_live_observe_scene_outcome(
        data,
        n,
        process_summary,
        store_as_checkpoint,
        store_status,
        store_as_checkpoint && cnr3_live_store_status_allows_return(store_status)
    );
#endif
    output_frame = nullptr;

    if (!cnr3_status_is_ok(return_status) || return_frame == nullptr) {
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_from_request(
            data,
            n,
            request_data,
            Cnr3DiagPlanTraceFailReason::store_prune_failed,
            n
        );
#endif
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
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
    const Cnr3DiagPlanTraceTick plantrace_enter_tick =
        request_data->plantrace_ar_all_enter_tick;
    const Cnr3DiagPlanTraceResultFields plantrace_result_fields =
        cnr3_diag_plantrace_make_computed_result(
            n,
            store_status
        );
#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM13_RECALCULATION)
    cnr3_diag_dsum13_observe_compute_completion(data.dsum13_recalculation, n, 0);
#endif

    const Cnr3Status discard_status = cnr3_discard_frame_data_with_cache(
        frame_data,
        data.output_cache
    );

    if (!cnr3_status_is_ok(discard_status)) {
        vsapi->freeFrame(return_frame);
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_with_progress(
            data,
            n,
            plantrace_enter_tick,
            plantrace_result_fields,
            Cnr3DiagPlanTraceFailReason::discharge_failed,
            n
        );
#endif
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

#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
    cnr3_diag_plantrace_observe_result(
        data.dsum_plantrace,
        n,
        plantrace_enter_tick,
        cnr3_diag_plantrace_sample_tick(),
        plantrace_result_fields
    );
#endif

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
#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN)
        if (request_data != nullptr) {
            cnr3_diag_live_observe_recovery_honesty_failure_if_needed(*request_data);
        }
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_from_request(
            data,
            n,
            request_data,
            Cnr3DiagPlanTraceFailReason::invalid_lifecycle,
            n
        );
#endif
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
#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN)
        cnr3_diag_live_observe_recovery_honesty_failure_if_needed(*request_data);
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_from_request(
            data,
            n,
            request_data,
            Cnr3DiagPlanTraceFailReason::invalid_branch_foundation,
            n
        );
#endif
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 D.3 floor-fresh-start proof: invalid recovery branch foundation."
        );
        return nullptr;
    }

    std::uint64_t frame_byte_count = 0U;
    const Cnr3Status frame_byte_count_status =
        cnr3_live_calculate_output_frame_byte_count(data, frame_byte_count);

    if (!cnr3_status_is_ok(frame_byte_count_status)) {
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_from_request(
            data,
            n,
            request_data,
            Cnr3DiagPlanTraceFailReason::byte_estimate_failed,
            n
        );
#endif
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 W.3: failed to compute recovery output frame byte estimate."
        );
        return nullptr;
    }

    Cnr3CacheRecoverySearchPlan& recovery_plan = request_data->recovery_plan;
    const std::size_t recovery_hole_count = recovery_plan.hole_frame_numbers.size();
    std::vector<bool> per_hole_scene_summary_available(recovery_hole_count, false);
    std::vector<Cnr3CallerSuppliedFrameProcessSummary> per_hole_process_summaries(
        recovery_hole_count
    );
    std::vector<Cnr3CacheAs2StoreRecordSummary> per_hole_store_summaries(
        recovery_hole_count
    );
    std::vector<bool> per_hole_store_as_checkpoint(recovery_hole_count, false);

    if (floor_fresh_start_recovery) {
        const int floor_frame = request_data->recovery_floor_frame;

        if (!cnr3_live_recovery_source_was_requested(*request_data, floor_frame)) {
#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN)
            cnr3_diag_live_observe_recovery_honesty_failure_if_needed(*request_data);
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
            cnr3_diag_plantrace_observe_failed_from_request(
                data,
                n,
                request_data,
                Cnr3DiagPlanTraceFailReason::source_not_requested,
                floor_frame
            );
#endif
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
            request_data->pin_list,
            Cnr3LookupCountPolicy::hit_only,
            Cnr3LookupSite::floor_adopt
        );

        if (cnr3_status_is_ok(floor_adopt_status)) {
            request_data->recovery_floor_outcome =
                Cnr3LiveRecoveryHoleOutcome::adopted_skipped;
        }
        else if (floor_adopt_status != Cnr3Status::not_found) {
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
            cnr3_diag_plantrace_observe_failed_from_request(
                data,
                n,
                request_data,
                Cnr3DiagPlanTraceFailReason::acquire_ref_failed,
                floor_frame
            );
#endif
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
#if defined(CNR3_DIAG_COMPUTE_DSUM06_SOURCE_FRAME_LIFECYCLE)
            cnr3_diag_live_observe_source_retrieve(
                data,
                *request_data,
                floor_frame,
                floor_source_frame
            );
#endif

            if (floor_source_frame == nullptr) {
#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN)
                cnr3_diag_live_observe_recovery_fallback_failure_if_needed(*request_data);
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
                cnr3_diag_plantrace_observe_failed_from_request(
                    data,
                    n,
                    request_data,
                    Cnr3DiagPlanTraceFailReason::source_retrieval_failed,
                    floor_frame
                );
#endif
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
#if defined(CNR3_DIAG_COMPUTE_DSUM06_SOURCE_FRAME_LIFECYCLE)
                cnr3_diag_live_observe_source_release(data);
#endif
                vsapi->freeFrame(floor_source_frame);
#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN)
                cnr3_diag_live_observe_recovery_fallback_failure_if_needed(*request_data);
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
                cnr3_diag_plantrace_observe_failed_from_request(
                    data,
                    n,
                    request_data,
                    Cnr3DiagPlanTraceFailReason::copyframe_failed,
                    floor_frame
                );
#endif
                (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
                cnr3_set_filter_error(
                    frame_ctx,
                    vsapi,
                    "CNR3 D.3 floor-fresh-start proof: floor copyFrame failed."
                );
                return nullptr;
            }

            if (floor_output_frame == floor_source_frame) {
#if defined(CNR3_DIAG_COMPUTE_DSUM06_SOURCE_FRAME_LIFECYCLE)
                cnr3_diag_live_observe_source_release(data);
#endif
                vsapi->freeFrame(floor_output_frame);
#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN)
                cnr3_diag_live_observe_recovery_fallback_failure_if_needed(*request_data);
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
                cnr3_diag_plantrace_observe_failed_from_request(
                    data,
                    n,
                    request_data,
                    Cnr3DiagPlanTraceFailReason::copyframe_source_alias,
                    floor_frame
                );
#endif
                (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
                cnr3_set_filter_error(
                    frame_ctx,
                    vsapi,
                    "CNR3 D.3 floor-fresh-start proof: floor copyFrame returned the source frame alias."
                );
                return nullptr;
            }

#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)
            cnr3_diag_dsum07_observe_temporary_output_created(
                data.dsum07_temp_output_lifecycle
            );
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM06_SOURCE_FRAME_LIFECYCLE)
            cnr3_diag_live_observe_source_release(data);
#endif
            vsapi->freeFrame(floor_source_frame);
            floor_source_frame = nullptr;

            Cnr3OwnedFrameRef floor_owned_frame{};
            const Cnr3Status adopt_floor_status = floor_owned_frame.reset_to_owned_frame(
                floor_output_frame,
                vsapi
            );

            if (!cnr3_status_is_ok(adopt_floor_status)) {
#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)
                cnr3_diag_dsum07_observe_temporary_output_released(
                    data.dsum07_temp_output_lifecycle
                );
#endif
                vsapi->freeFrame(floor_output_frame);
#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN)
                cnr3_diag_live_observe_recovery_fallback_failure_if_needed(*request_data);
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
                cnr3_diag_plantrace_observe_failed_from_request(
                    data,
                    n,
                    request_data,
                    Cnr3DiagPlanTraceFailReason::adopt_failed,
                    floor_frame
                );
#endif
                (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
                cnr3_set_filter_error(
                    frame_ctx,
                    vsapi,
                    "CNR3 D.3 floor-fresh-start proof: failed to adopt floor fresh-start output."
                );
                return nullptr;
            }

#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)
            cnr3_diag_dsum07_observe_temporary_output_stored(
                data.dsum07_temp_output_lifecycle
            );
#endif
            floor_output_frame = nullptr;

            Cnr3CombinedStoreAndPruneSummary floor_store_summary{};
            const Cnr3Status floor_store_status =
                data.output_cache.store_as2_floor_and_prune(
                    floor_frame,
                    n,
                    std::move(floor_owned_frame),
                    cnr3_live_output_frame_is_checkpoint(floor_frame),
                    frame_byte_count,
                    request_data->pin_list,
                    floor_store_summary
                );
            cnr3_trace_live_combined_store_and_prune(data, floor_store_summary);

            if (!cnr3_status_is_ok(floor_store_status) ||
                !floor_store_summary.as2_summary.pin_recorded) {
#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN)
                cnr3_diag_live_observe_recovery_fallback_failure_if_needed(*request_data);
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
                cnr3_diag_plantrace_observe_failed_from_request(
                    data,
                    n,
                    request_data,
                    Cnr3DiagPlanTraceFailReason::store_prune_failed,
                    floor_frame
                );
#endif
                (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
                cnr3_set_filter_error(
                    frame_ctx,
                    vsapi,
                    "CNR3 D.3 floor-fresh-start proof: failed to store/pin/prune floor fresh-start output."
                );
                return nullptr;
            }

            request_data->recovery_floor_outcome =
                floor_store_summary.as2_summary.duplicate_existing_slot
                ? Cnr3LiveRecoveryHoleOutcome::adopted_post_compute_loser
                : Cnr3LiveRecoveryHoleOutcome::computed;
#if defined(CNR3_DIAG_COMPUTE_DSUM13_RECALCULATION)
            cnr3_diag_dsum13_observe_compute_completion(
                data.dsum13_recalculation,
                floor_frame,
                0
            );
#endif
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
#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN)
            cnr3_diag_live_observe_recovery_honesty_failure_if_needed(*request_data);
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
            cnr3_diag_plantrace_observe_failed_from_request(
                data,
                n,
                request_data,
                Cnr3DiagPlanTraceFailReason::source_not_requested,
                hole_frame
            );
#endif
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
            request_data->pin_list,
            Cnr3LookupCountPolicy::hit_only,
            Cnr3LookupSite::hole_adopt
        );

        if (cnr3_status_is_ok(adopt_status)) {
            request_data->per_hole_outcomes[hole_index] =
                Cnr3LiveRecoveryHoleOutcome::adopted_skipped;
            continue;
        }

        if (adopt_status != Cnr3Status::not_found) {
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
            cnr3_diag_plantrace_observe_failed_from_request(
                data,
                n,
                request_data,
                Cnr3DiagPlanTraceFailReason::acquire_ref_failed,
                hole_frame
            );
#endif
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
            predecessor_compute_ref,
            Cnr3LookupCountPolicy::none,
            Cnr3LookupSite::reacquire_already_pinned
        );

        if (!cnr3_status_is_ok(predecessor_status) ||
            !predecessor_compute_ref.has_frame()) {
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
            cnr3_diag_plantrace_observe_failed_from_request(
                data,
                n,
                request_data,
                Cnr3DiagPlanTraceFailReason::acquire_ref_failed,
                hole_frame - 1
            );
#endif
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
#if defined(CNR3_DIAG_COMPUTE_DSUM06_SOURCE_FRAME_LIFECYCLE)
        cnr3_diag_live_observe_source_retrieve(
            data,
            *request_data,
            hole_frame,
            source_frame
        );
#endif

        if (source_frame == nullptr) {
            predecessor_compute_ref.reset();
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
            cnr3_diag_plantrace_observe_failed_from_request(
                data,
                n,
                request_data,
                Cnr3DiagPlanTraceFailReason::source_retrieval_failed,
                hole_frame
            );
#endif
            (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 D.3 floor-fresh-start proof: hole source frame retrieval failed."
            );
            return nullptr;
        }

#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN)
        cnr3_diag_dsum12_observe_hole_source_retrieved(data.dsum12_recovery_plan);
#endif

        VSFrame* hole_output_frame = vsapi->copyFrame(source_frame, core);

        if (hole_output_frame == nullptr) {
            predecessor_compute_ref.reset();
#if defined(CNR3_DIAG_COMPUTE_DSUM06_SOURCE_FRAME_LIFECYCLE)
            cnr3_diag_live_observe_source_release(data);
#endif
            vsapi->freeFrame(source_frame);
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
            cnr3_diag_plantrace_observe_failed_from_request(
                data,
                n,
                request_data,
                Cnr3DiagPlanTraceFailReason::copyframe_failed,
                hole_frame
            );
#endif
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
#if defined(CNR3_DIAG_COMPUTE_DSUM06_SOURCE_FRAME_LIFECYCLE)
            cnr3_diag_live_observe_source_release(data);
#endif
            vsapi->freeFrame(hole_output_frame);
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
            cnr3_diag_plantrace_observe_failed_from_request(
                data,
                n,
                request_data,
                Cnr3DiagPlanTraceFailReason::copyframe_source_alias,
                hole_frame
            );
#endif
            (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 D.3 floor-fresh-start proof: hole copyFrame returned the source frame alias."
            );
            return nullptr;
        }

#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)
        cnr3_diag_dsum07_observe_temporary_output_created(
            data.dsum07_temp_output_lifecycle
        );
#endif
        Cnr3CallerSuppliedFrameProcessSummary hole_process_summary{};
        const Cnr3Status hole_process_status =
            cnr3_process_caller_supplied_vapoursynth_frame_triplet_with_scene_change(
                source_frame,
                predecessor_compute_ref.get(),
                hole_output_frame,
                vsapi,
                data.bits_per_sample,
                data.sub_sampling_w,
                data.sub_sampling_h,
                data.response_tables,
                data.scene_change_config,
                hole_process_summary
            );

        predecessor_compute_ref.reset();
#if defined(CNR3_DIAG_COMPUTE_DSUM06_SOURCE_FRAME_LIFECYCLE)
        cnr3_diag_live_observe_source_release(data);
#endif
        vsapi->freeFrame(source_frame);
        source_frame = nullptr;

        if (!cnr3_status_is_ok(hole_process_status) ||
            !hole_process_summary.frame_processed) {
#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)
            cnr3_diag_dsum07_observe_temporary_output_released(
                data.dsum07_temp_output_lifecycle
            );
#endif
            vsapi->freeFrame(hole_output_frame);
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
            cnr3_diag_plantrace_observe_failed_from_request(
                data,
                n,
                request_data,
                Cnr3DiagPlanTraceFailReason::scene_processing_failed,
                hole_frame
            );
#endif
            (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 P.11C.4 recovery scene proof: P.11C hole processing failed."
            );
            return nullptr;
        }

        Cnr3OwnedFrameRef hole_owned_frame{};
        const Cnr3Status adopt_hole_status = hole_owned_frame.reset_to_owned_frame(
            hole_output_frame,
            vsapi
        );

        if (!cnr3_status_is_ok(adopt_hole_status)) {
#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)
            cnr3_diag_dsum07_observe_temporary_output_released(
                data.dsum07_temp_output_lifecycle
            );
#endif
            vsapi->freeFrame(hole_output_frame);
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
            cnr3_diag_plantrace_observe_failed_from_request(
                data,
                n,
                request_data,
                Cnr3DiagPlanTraceFailReason::adopt_failed,
                hole_frame
            );
#endif
            (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 D.3 floor-fresh-start proof: failed to adopt computed hole output."
            );
            return nullptr;
        }

#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)
        cnr3_diag_dsum07_observe_temporary_output_stored(
            data.dsum07_temp_output_lifecycle
        );
#endif
        hole_output_frame = nullptr;

        const Cnr3LiveOutputStoreRequest hole_store_request{
            hole_frame,
            hole_process_summary.scene_change_detected
        };
        const bool hole_store_as_checkpoint =
            cnr3_live_output_frame_should_store_as_checkpoint(hole_store_request);

        Cnr3CombinedStoreAndPruneSummary hole_store_summary{};
        const Cnr3Status hole_store_status =
            data.output_cache.store_recovery_hole_and_prune(
                recovery_plan,
                hole_frame,
                n,
                std::move(hole_owned_frame),
                hole_store_as_checkpoint,
                frame_byte_count,
                request_data->pin_list,
                hole_store_summary
            );
        cnr3_trace_live_combined_store_and_prune(data, hole_store_summary);

#if defined(CNR3_DIAG_COMPUTE_DSUM14_SCENE_RESET)
        cnr3_diag_live_observe_scene_outcome(
            data,
            hole_frame,
            hole_process_summary,
            hole_store_as_checkpoint,
            hole_store_summary.store_status,
            hole_store_summary.as2_summary.resulting_slot_is_checkpoint
        );
#endif
        if (!cnr3_status_is_ok(hole_store_status) ||
            !hole_store_summary.as2_summary.pin_recorded) {
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
            cnr3_diag_plantrace_observe_failed_from_request(
                data,
                n,
                request_data,
                Cnr3DiagPlanTraceFailReason::store_prune_failed,
                hole_frame
            );
#endif
            (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 D.3 floor-fresh-start proof: failed to store/pin/prune computed recovery hole."
            );
            return nullptr;
        }

        request_data->per_hole_outcomes[hole_index] =
            hole_store_summary.as2_summary.duplicate_existing_slot
            ? Cnr3LiveRecoveryHoleOutcome::adopted_post_compute_loser
            : Cnr3LiveRecoveryHoleOutcome::computed;
#if defined(CNR3_DIAG_COMPUTE_DSUM13_RECALCULATION)
        cnr3_diag_dsum13_observe_compute_completion(
            data.dsum13_recalculation,
            hole_frame,
            cnr3_live_recovery_depth_from_foundation(*request_data, hole_frame)
        );
#endif
        per_hole_scene_summary_available[hole_index] = true;
        per_hole_process_summaries[hole_index] = hole_process_summary;
        per_hole_store_summaries[hole_index] = hole_store_summary.as2_summary;
        per_hole_store_as_checkpoint[hole_index] = hole_store_as_checkpoint;
    }

#if defined(CNR3_DIAG_COMPUTE_DSUM03_RECOVERY_SEARCH)
    cnr3_diag_dsum03_observe_holes_filled(
        data.dsum03_recovery_search,
        recovery_hole_count
    );
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN)
    cnr3_diag_dsum12_observe_holes_filled(
        data.dsum12_recovery_plan,
        recovery_hole_count
    );
#endif

    if (!cnr3_live_recovery_source_was_requested(*request_data, n)) {
#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN)
        cnr3_diag_live_observe_recovery_honesty_failure_if_needed(*request_data);
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_from_request(
            data,
            n,
            request_data,
            Cnr3DiagPlanTraceFailReason::source_not_requested,
            n
        );
#endif
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
            target_predecessor_compute_ref,
            Cnr3LookupCountPolicy::none,
            Cnr3LookupSite::reacquire_already_pinned
        );

    if (!cnr3_status_is_ok(target_predecessor_status) ||
        !target_predecessor_compute_ref.has_frame()) {
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_from_request(
            data,
            n,
            request_data,
            Cnr3DiagPlanTraceFailReason::acquire_ref_failed,
            n - 1
        );
#endif
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 D.3 floor-fresh-start proof: failed to acquire target predecessor compute reference."
        );
        return nullptr;
    }

    const VSFrame* target_source_frame = vsapi->getFrameFilter(n, data.source, frame_ctx);
#if defined(CNR3_DIAG_COMPUTE_DSUM06_SOURCE_FRAME_LIFECYCLE)
    cnr3_diag_live_observe_source_retrieve(data, *request_data, n, target_source_frame);
#endif

    if (target_source_frame == nullptr) {
        target_predecessor_compute_ref.reset();
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_from_request(
            data,
            n,
            request_data,
            Cnr3DiagPlanTraceFailReason::source_retrieval_failed,
            n
        );
#endif
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
#if defined(CNR3_DIAG_COMPUTE_DSUM06_SOURCE_FRAME_LIFECYCLE)
        cnr3_diag_live_observe_source_release(data);
#endif
        vsapi->freeFrame(target_source_frame);
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_from_request(
            data,
            n,
            request_data,
            Cnr3DiagPlanTraceFailReason::copyframe_failed,
            n
        );
#endif
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
#if defined(CNR3_DIAG_COMPUTE_DSUM06_SOURCE_FRAME_LIFECYCLE)
        cnr3_diag_live_observe_source_release(data);
#endif
        vsapi->freeFrame(target_output_frame);
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_from_request(
            data,
            n,
            request_data,
            Cnr3DiagPlanTraceFailReason::copyframe_source_alias,
            n
        );
#endif
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 D.3 floor-fresh-start proof: target copyFrame returned the source frame alias."
        );
        return nullptr;
    }

#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)
    cnr3_diag_dsum07_observe_temporary_output_created(
        data.dsum07_temp_output_lifecycle
    );
#endif
    Cnr3CallerSuppliedFrameProcessSummary target_process_summary{};
    const Cnr3Status target_process_status =
        cnr3_process_caller_supplied_vapoursynth_frame_triplet_with_scene_change(
            target_source_frame,
            target_predecessor_compute_ref.get(),
            target_output_frame,
            vsapi,
            data.bits_per_sample,
            data.sub_sampling_w,
            data.sub_sampling_h,
            data.response_tables,
            data.scene_change_config,
            target_process_summary
        );

    target_predecessor_compute_ref.reset();
#if defined(CNR3_DIAG_COMPUTE_DSUM06_SOURCE_FRAME_LIFECYCLE)
    cnr3_diag_live_observe_source_release(data);
#endif
    vsapi->freeFrame(target_source_frame);
    target_source_frame = nullptr;

    if (!cnr3_status_is_ok(target_process_status) ||
        !target_process_summary.frame_processed) {
#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)
        cnr3_diag_dsum07_observe_temporary_output_released(
            data.dsum07_temp_output_lifecycle
        );
#endif
        vsapi->freeFrame(target_output_frame);
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_from_request(
            data,
            n,
            request_data,
            Cnr3DiagPlanTraceFailReason::scene_processing_failed,
            n
        );
#endif
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 P.11C.4 recovery scene proof: P.11C target processing failed."
        );
        return nullptr;
    }

    const Cnr3LiveOutputStoreRequest target_store_request{
        n,
        target_process_summary.scene_change_detected
    };
    const bool target_store_as_checkpoint =
        cnr3_live_output_frame_should_store_as_checkpoint(target_store_request);

    const VSFrame* return_frame = nullptr;
    Cnr3Status target_store_status = Cnr3Status::invariant_violation;
    bool returned_cached_winner = false;
    const Cnr3Status target_return_status =
        cnr3_store_live_output_frame_for_authoritative_return(
            data,
            target_store_request,
            target_output_frame,
            vsapi,
            frame_byte_count,
            return_frame,
            target_store_status,
            returned_cached_winner
        );
#if defined(CNR3_DIAG_COMPUTE_DSUM14_SCENE_RESET)
    cnr3_diag_live_observe_scene_outcome(
        data,
        n,
        target_process_summary,
        target_store_as_checkpoint,
        target_store_status,
        target_store_as_checkpoint && cnr3_live_store_status_allows_return(target_store_status)
    );
#endif
    target_output_frame = nullptr;

    if (!cnr3_status_is_ok(target_return_status) || return_frame == nullptr) {
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_from_request(
            data,
            n,
            request_data,
            Cnr3DiagPlanTraceFailReason::store_prune_failed,
            n
        );
#endif
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 D.3 floor-fresh-start proof: failed to store/return authoritative target output."
        );
        return nullptr;
    }

    (void)returned_cached_winner;

#if defined(CNR3_DIAG_COMPUTE_DSUM13_RECALCULATION)
    cnr3_diag_dsum13_observe_compute_completion(
        data.dsum13_recalculation,
        n,
        cnr3_live_recovery_depth_from_foundation(*request_data, n)
    );
#endif

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
    const std::vector<bool> per_hole_scene_summary_available_for_trace =
        per_hole_scene_summary_available;
    const std::vector<Cnr3CallerSuppliedFrameProcessSummary> per_hole_process_summaries_for_trace =
        per_hole_process_summaries;
    const std::vector<Cnr3CacheAs2StoreRecordSummary> per_hole_store_summaries_for_trace =
        per_hole_store_summaries;
    const std::vector<bool> per_hole_store_as_checkpoint_for_trace =
        per_hole_store_as_checkpoint;
    const bool target_store_as_checkpoint_for_trace = target_store_as_checkpoint;
    const Cnr3Status target_store_status_for_trace = target_store_status;
    const std::size_t pin_list_size_before_discharge =
        request_data->pin_list.pin_count();
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
    const Cnr3DiagPlanTraceTick plantrace_enter_tick =
        request_data->plantrace_ar_all_enter_tick;
    const Cnr3DiagPlanTraceResultFields plantrace_result_fields =
        cnr3_diag_plantrace_make_recovery_result(
            n,
            recovery_branch_for_trace,
            recovery_floor_frame_for_trace,
            floor_outcome_for_trace,
            recovery_plan_for_trace,
            hole_outcomes_for_trace,
            target_store_status_for_trace
        );
#endif

    const Cnr3Status discard_status = cnr3_discard_frame_data_with_cache(
        frame_data,
        data.output_cache
    );

    if (!cnr3_status_is_ok(discard_status)) {
        vsapi->freeFrame(return_frame);
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_with_progress(
            data,
            n,
            plantrace_enter_tick,
            plantrace_result_fields,
            Cnr3DiagPlanTraceFailReason::discharge_failed,
            n
        );
#endif
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
        per_hole_scene_summary_available_for_trace,
        per_hole_process_summaries_for_trace,
        per_hole_store_summaries_for_trace,
        per_hole_store_as_checkpoint_for_trace,
        pin_list_size_before_discharge,
        target_process_summary,
        target_store_as_checkpoint_for_trace,
        target_store_status_for_trace
    );

#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
    cnr3_diag_plantrace_observe_result(
        data.dsum_plantrace,
        n,
        plantrace_enter_tick,
        cnr3_diag_plantrace_sample_tick(),
        plantrace_result_fields
    );
#endif

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
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_from_request(
            data,
            n,
            request_data,
            Cnr3DiagPlanTraceFailReason::source_not_requested,
            n
        );
#endif
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1D frame-0 proof: source frame was not requested in this activation."
        );
        return nullptr;
    }

    const VSFrame* source_frame = vsapi->getFrameFilter(n, data.source, frame_ctx);
#if defined(CNR3_DIAG_COMPUTE_DSUM06_SOURCE_FRAME_LIFECYCLE)
    cnr3_diag_live_observe_source_retrieve(data, *request_data, n, source_frame);
#endif

    if (source_frame == nullptr) {
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_from_request(
            data,
            n,
            request_data,
            Cnr3DiagPlanTraceFailReason::source_retrieval_failed,
            n
        );
#endif
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
#if defined(CNR3_DIAG_COMPUTE_DSUM06_SOURCE_FRAME_LIFECYCLE)
        cnr3_diag_live_observe_source_release(data);
#endif
        vsapi->freeFrame(source_frame);
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_from_request(
            data,
            n,
            request_data,
            Cnr3DiagPlanTraceFailReason::copyframe_failed,
            n
        );
#endif
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1D frame-0 proof: copyFrame failed."
        );
        return nullptr;
    }

    if (output_frame == source_frame) {
#if defined(CNR3_DIAG_COMPUTE_DSUM06_SOURCE_FRAME_LIFECYCLE)
        cnr3_diag_live_observe_source_release(data);
#endif
        vsapi->freeFrame(output_frame);
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_from_request(
            data,
            n,
            request_data,
            Cnr3DiagPlanTraceFailReason::copyframe_source_alias,
            n
        );
#endif
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1D frame-0 proof: copyFrame returned the source frame alias."
        );
        return nullptr;
    }

#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)
    cnr3_diag_dsum07_observe_temporary_output_created(
        data.dsum07_temp_output_lifecycle
    );
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM06_SOURCE_FRAME_LIFECYCLE)
    cnr3_diag_live_observe_source_release(data);
#endif
    vsapi->freeFrame(source_frame);
    source_frame = nullptr;

    const VSFrame* cache_frame_ref = vsapi->addFrameRef(output_frame);

    if (cache_frame_ref == nullptr) {
#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)
        cnr3_diag_dsum07_observe_temporary_output_released(
            data.dsum07_temp_output_lifecycle
        );
#endif
        vsapi->freeFrame(output_frame);
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_from_request(
            data,
            n,
            request_data,
            Cnr3DiagPlanTraceFailReason::acquire_ref_failed,
            n
        );
#endif
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
#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)
        cnr3_diag_dsum07_observe_temporary_output_released(
            data.dsum07_temp_output_lifecycle
        );
#endif
        vsapi->freeFrame(output_frame);
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_from_request(
            data,
            n,
            request_data,
            Cnr3DiagPlanTraceFailReason::adopt_failed,
            n
        );
#endif
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1D frame-0 proof: failed to adopt cache frame reference."
        );
        return nullptr;
    }

    std::uint64_t frame_byte_count = 0U;
    const Cnr3Status frame_byte_count_status =
        cnr3_live_calculate_output_frame_byte_count(data, frame_byte_count);

    if (!cnr3_status_is_ok(frame_byte_count_status)) {
#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)
        cnr3_diag_dsum07_observe_temporary_output_released(
            data.dsum07_temp_output_lifecycle
        );
#endif
        vsapi->freeFrame(output_frame);
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_from_request(
            data,
            n,
            request_data,
            Cnr3DiagPlanTraceFailReason::byte_estimate_failed,
            n
        );
#endif
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 W.3: failed to compute frame-0 output frame byte estimate."
        );
        return nullptr;
    }

    Cnr3CombinedStoreAndPruneSummary store_summary{};
    const Cnr3Status store_hard_status =
        data.output_cache.store_production_output_and_prune(
            0,
            n,
            std::move(cache_owned_frame),
            true,
            frame_byte_count,
            store_summary
        );
    cnr3_trace_live_combined_store_and_prune(data, store_summary);

    const Cnr3Status store_status = store_summary.store_status;

    const bool frame0_return_allowed =
        cnr3_live_store_status_allows_return(store_status);
#if defined(CNR3_DIAG_COMPUTE_DSUM09_RETURN_TRANSFER)
    cnr3_diag_dsum09_observe_return_decision(
        data.dsum09_return_transfer,
        cnr3_status_is_ok(store_hard_status) && frame0_return_allowed,
        cnr3_status_is_ok(store_hard_status)
            ? Cnr3DiagDsum09ReturnNoReason::store_status_not_returnable
            : Cnr3DiagDsum09ReturnNoReason::hard_store_failure
    );
#endif

    if (!cnr3_status_is_ok(store_hard_status) || !frame0_return_allowed) {
#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)
        cnr3_diag_dsum07_observe_temporary_output_released(
            data.dsum07_temp_output_lifecycle
        );
#endif
        vsapi->freeFrame(output_frame);
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_from_request(
            data,
            n,
            request_data,
            Cnr3DiagPlanTraceFailReason::store_prune_failed,
            n
        );
#endif
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1D frame-0 proof: failed to store/prune output[0] checkpoint."
        );
        return nullptr;
    }

#if defined(CNR3_DIAG_COMPUTE_DSUM13_RECALCULATION)
    cnr3_diag_dsum13_observe_compute_completion(data.dsum13_recalculation, n, 0);
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
    const Cnr3DiagPlanTraceTick plantrace_enter_tick =
        request_data->plantrace_ar_all_enter_tick;
    const Cnr3DiagPlanTraceResultFields plantrace_result_fields =
        cnr3_diag_plantrace_make_computed_result(
            n,
            store_status
        );
#endif

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
#if defined(CNR3_DIAG_COMPUTE_DSUM09_RETURN_TRANSFER)
    cnr3_diag_dsum09_observe_return_transfer(
        data.dsum09_return_transfer,
        true,
        true
    );
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)
    cnr3_diag_dsum07_observe_temporary_output_transferred(
        data.dsum07_temp_output_lifecycle
    );
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
    cnr3_diag_plantrace_observe_result(
        data.dsum_plantrace,
        n,
        plantrace_enter_tick,
        cnr3_diag_plantrace_sample_tick(),
        plantrace_result_fields
    );
#endif
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
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
    const Cnr3DiagPlanTraceTick plantrace_ar_all_enter_tick =
        cnr3_diag_plantrace_sample_tick();
#endif

    Cnr3LiveGetFrameFrameData* request_data =
        static_cast<Cnr3LiveGetFrameFrameData*>(*frame_data);

    if (request_data == nullptr) {
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_minimal_failed_and_dump(
            data.config.instance_id,
            data.dsum_plantrace,
            n,
            Cnr3DiagPlanTraceFailReason::framedata_missing_or_unknown,
            n
        );
#endif
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1F cache-hit proof: missing frameData at arAllFramesReady."
        );
        return nullptr;
    }

#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
    request_data->plantrace_ar_all_enter_tick = plantrace_ar_all_enter_tick;
#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM01_REQUEST_ORDER)
    cnr3_diag_dsum01_observe_ar_all_frames_ready(
        data.dsum01_request_order
    );
#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM02_MEMORY)
    if (CNR3_MEMORY_DIAG_FRAME_INTERVAL > 0 &&
        n > 0 &&
        (n % CNR3_MEMORY_DIAG_FRAME_INTERVAL) == 0) {
        char periodic_label[64]{};
        std::snprintf(
            periodic_label,
            sizeof(periodic_label),
            "frame=%d",
            n
        );

        cnr3_memory_record_and_print_snapshot(
            data.dsum02_memory,
            data.config.instance_id,
            periodic_label,
            false
        );
    }
#endif

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
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
        cnr3_diag_plantrace_observe_failed_from_request(
            data,
            n,
            request_data,
            Cnr3DiagPlanTraceFailReason::framedata_missing_or_unknown,
            n
        );
#endif
        (void)cnr3_discard_frame_data_with_cache(frame_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 K.1F cache-hit proof: unknown frameData branch at arAllFramesReady."
        );
        return nullptr;
    }
}

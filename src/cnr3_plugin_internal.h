/*
    CNR3 private VapourSynth plugin-integration declarations.

    This header is intentionally private to the VapourSynth plugin translation
    units. It carries live getFrame frameData, branch tags, and helper
    declarations that must be shared after splitting vapoursynth-Cnr3.cpp.

    Cache algorithms, prune policy, hot-zone mechanics, and pixel loops remain
    in their own layers; this file only describes plugin-side integration state.

    SPDX-License-Identifier: AGPL-3.0-or-later
*/

#pragma once

#include "cnr3_build_config.h"
#include "cnr3_cache_core.h"
#include "cnr3_common.h"
#include "cnr3_diagnostics.h"
#include "cnr3_frame_processing.h"
#include "cnr3_instance_config.h"
#include "cnr3_memory_diagnostics.h"
#include "cnr3_owned_frame_ref.h"
#include "cnr3_response_tables.h"

#include "VapourSynth4.h"

#include <vector>

#if defined(CNR3_ENABLE_PLAN_RETRY_BIAS)
#include <cstdint>
#include <mutex>
#endif

#if defined(CNR3_ENABLE_PLAN_RETRY_BIAS)

struct Cnr3PlanRetryExperimentStats {
    mutable std::mutex mutex{};
    std::uint64_t plan_attempts_total = 0;
    std::uint64_t plans_dumped_total = 0;
    std::uint64_t retry_sleeps_total = 0;
    std::uint64_t plans_kept_on_attempt_1 = 0;
    std::uint64_t plans_kept_on_attempt_2 = 0;
    std::uint64_t plans_kept_on_attempt_3plus = 0;
    std::uint64_t dumped_plan_holes_total = 0;
    std::uint64_t kept_plan_holes_total = 0;
};

#endif

struct Cnr3FilterData {
    VSNode* source = nullptr;
    VSVideoInfo video_info{};
    Cnr3InstanceConfig config{};
    Cnr3OutputCacheCore output_cache{};
    Cnr3ResponseTables response_tables{};
    int bits_per_sample = 0;
    int sub_sampling_w = -1;
    int sub_sampling_h = -1;
    double scene_change_scdthr = CNR3_P11C_DEFAULT_SCDTHR;
    Cnr3SceneChangeConfig scene_change_config{};
#if defined(CNR3_ENABLE_PLAN_RETRY_BIAS)
    int plan_retry_max = 1;
    Cnr3PlanRetryExperimentStats plan_retry_stats{};
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
    Cnr3DiagPlanTraceBuffer dsum_plantrace{};
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM01_REQUEST_ORDER)
    Cnr3DiagDsum01RequestOrderStats dsum01_request_order{};
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM02_MEMORY)
    Cnr3MemoryStats dsum02_memory{};
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM03_RECOVERY_SEARCH)
    Cnr3DiagDsum03RecoverySearchStats dsum03_recovery_search{};
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM06_SOURCE_FRAME_LIFECYCLE)
    Cnr3DiagDsum06SourceFrameLifecycleStats dsum06_source_frame_lifecycle{};
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM07_TEMP_OUTPUT_LIFECYCLE)
    Cnr3DiagDsum07TempOutputLifecycleStats dsum07_temp_output_lifecycle{};
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM09_RETURN_TRANSFER)
    Cnr3DiagDsum09ReturnTransferStats dsum09_return_transfer{};
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN)
    Cnr3DiagDsum12RecoveryPlanStats dsum12_recovery_plan{};
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM13_RECALCULATION)
    Cnr3DiagDsum13RecalculationStats dsum13_recalculation{};
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM14_SCENE_RESET)
    Cnr3DiagDsum14SceneResetStats dsum14_scene_reset{};
#endif
};

struct Cnr3LiveOutputStoreRequest {
    int frame_number = CNR3_INVALID_FRAME_NUMBER;
    bool force_checkpoint = false;
};

enum class Cnr3LiveGetFrameBranch {
    none,
    cache_hit_return,
    frame0_fresh_start,
    predecessor_present_compute,
    recovery
};

enum class Cnr3LiveRecoveryBranch {
    none,
    exact_anchor,
    floor_fresh_start
};

enum class Cnr3LiveRecoveryHoleOutcome {
    none,
    computed,
    adopted_skipped,
    adopted_post_compute_loser
};

struct Cnr3LiveGetFrameFrameData {
    Cnr3LiveGetFrameBranch branch = Cnr3LiveGetFrameBranch::none;
    int requested_frame = CNR3_INVALID_FRAME_NUMBER;
    int predecessor_frame = CNR3_INVALID_FRAME_NUMBER;
    bool source_requested = false;
    bool predecessor_pin_taken = false;
    bool cache_hit_pin_taken = false;
    Cnr3LiveRecoveryBranch recovery_branch = Cnr3LiveRecoveryBranch::none;
    Cnr3CacheRecoverySearchPlan recovery_plan{};
    int recovery_floor_frame = CNR3_INVALID_FRAME_NUMBER;
    Cnr3LiveRecoveryHoleOutcome recovery_floor_outcome = Cnr3LiveRecoveryHoleOutcome::none;
    std::vector<int> source_request_frame_numbers{};
    std::vector<Cnr3LiveRecoveryHoleOutcome> per_hole_outcomes{};
#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)
    Cnr3DiagPlanTraceTick plantrace_ar_all_enter_tick = 0;
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN)
    Cnr3DiagDsum12RecoveryPlanStats* dsum12_recovery_plan_stats = nullptr;
#endif
    Cnr3CachePinList pin_list{};
};

#if defined(CNR3_DIAG_COMPUTE_DSUM_PLANTRACE)

inline void cnr3_live_plantrace_add_frame_once(
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

inline void cnr3_live_plantrace_add_recovery_outcome_frame(
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

inline Cnr3DiagPlanTraceResultFields cnr3_live_plantrace_make_failed_result_from_request(
    const Cnr3LiveGetFrameFrameData* request_data,
    Cnr3DiagPlanTraceFailReason fail_reason,
    int error_frame
) {
    Cnr3DiagPlanTraceResultFields fields{};
    fields.outcome = Cnr3DiagPlanTraceOutcome::failed;
    fields.fail_reason = fail_reason;

    if (cnr3_frame_number_is_valid(error_frame)) {
        fields.error_here_frames.push_back(error_frame);
    }

    if (request_data == nullptr ||
        request_data->branch != Cnr3LiveGetFrameBranch::recovery) {
        return fields;
    }

    if (request_data->recovery_branch == Cnr3LiveRecoveryBranch::floor_fresh_start) {
        cnr3_live_plantrace_add_recovery_outcome_frame(
            fields,
            request_data->recovery_floor_frame,
            request_data->recovery_floor_outcome
        );
    }

    const std::size_t hole_count = request_data->recovery_plan.hole_frame_numbers.size();
    for (std::size_t i = 0U; i < hole_count; ++i) {
        const int hole_frame = request_data->recovery_plan.hole_frame_numbers[i];
        const Cnr3LiveRecoveryHoleOutcome outcome =
            i < request_data->per_hole_outcomes.size()
            ? request_data->per_hole_outcomes[i]
            : Cnr3LiveRecoveryHoleOutcome::none;

        cnr3_live_plantrace_add_recovery_outcome_frame(fields, hole_frame, outcome);

        if (outcome == Cnr3LiveRecoveryHoleOutcome::none && hole_frame != error_frame) {
            cnr3_live_plantrace_add_frame_once(fields.not_reached_frames, hole_frame);
        }
    }

    if (request_data->requested_frame != error_frame) {
        cnr3_live_plantrace_add_frame_once(
            fields.not_reached_frames,
            request_data->requested_frame
        );
    }

    return fields;
}

#endif

void cnr3_set_filter_error(
    VSFrameContext* frame_ctx,
    const VSAPI* vsapi,
    const char* message
) noexcept;

Cnr3Status cnr3_discard_frame_data_with_cache(
    void** frame_data,
    Cnr3OutputCacheCore& output_cache
) noexcept;

bool cnr3_live_store_status_allows_return(
    Cnr3Status status
) noexcept;

Cnr3Status cnr3_store_live_output_frame_for_return(
    Cnr3FilterData& data,
    const Cnr3LiveOutputStoreRequest& request,
    VSFrame* output_frame,
    const VSAPI* vsapi,
    std::uint64_t frame_byte_count,
    Cnr3CombinedStoreAndPruneSummary& out_summary
) noexcept;

void cnr3_trace_live_frame0_fresh_start(
    const Cnr3FilterData& data,
    int requested_frame,
    bool source_requested,
    bool source_retrieved,
    bool copy_frame_succeeded,
    const char* store_status_name,
    bool frame_returned
) noexcept;

void cnr3_trace_live_predecessor_present_compute(
    const Cnr3FilterData& data,
    int requested_frame,
    int predecessor_frame,
    Cnr3Status store_status,
    const Cnr3CallerSuppliedFrameProcessSummary& process_summary
) noexcept;

void cnr3_trace_live_cache_hit_return(
    const Cnr3FilterData& data,
    int requested_frame
) noexcept;

void cnr3_trace_live_after_frame2_not_yet_implemented(
    const Cnr3FilterData& data,
    int requested_frame
) noexcept;

const VSFrame* cnr3_complete_live_recovery(
    int n,
    Cnr3FilterData& data,
    void** frame_data,
    VSFrameContext* frame_ctx,
    VSCore* core,
    const VSAPI* vsapi
);

const VSFrame* cnr3_get_frame_live_cache_hit_return(
    int n,
    Cnr3FilterData& data,
    void** frame_data,
    VSFrameContext* frame_ctx,
    const VSAPI* vsapi
);

const VSFrame* cnr3_complete_live_predecessor_present_compute(
    int n,
    Cnr3FilterData& data,
    void** frame_data,
    VSFrameContext* frame_ctx,
    VSCore* core,
    const VSAPI* vsapi
);

const VSFrame* cnr3_complete_live_frame0_fresh_start(
    int n,
    Cnr3FilterData& data,
    void** frame_data,
    VSFrameContext* frame_ctx,
    VSCore* core,
    const VSAPI* vsapi
);

const VSFrame* cnr3_arInitial(
    int n,
    Cnr3FilterData& data,
    void** frame_data,
    VSFrameContext* frame_ctx,
    VSCore* core,
    const VSAPI* vsapi
);

const VSFrame* cnr3_arAllFramesReady(
    int n,
    Cnr3FilterData& data,
    void** frame_data,
    VSFrameContext* frame_ctx,
    VSCore* core,
    const VSAPI* vsapi
);

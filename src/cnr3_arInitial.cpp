/*
    CNR3 live getFrame arInitial branch planning.

    This translation unit owns the plugin-side arInitial dispatch and source
    request setup. It does not own cache policy, prune policy, hot-zone logic,
    or pixel processing.

    SPDX-License-Identifier: AGPL-3.0-or-later
*/

#include "cnr3_build_config.h"
#include "cnr3_plugin_internal.h"

#include <cstdio>
#include <new>
#include <string>
#include <utility>

namespace {

Cnr3Status cnr3_delete_unpublished_frame_data(
    Cnr3LiveGetFrameFrameData* request_data,
    Cnr3OutputCacheCore& output_cache
) noexcept {
    if (request_data == nullptr) {
        return Cnr3Status::ok;
    }

    const Cnr3Status discharge_status =
        request_data->pin_list.discharge_all(output_cache);

    delete request_data;
    return discharge_status;
}

const VSFrame* cnr3_publish_live_cache_hit_return(
    int n,
    Cnr3FilterData& data,
    Cnr3LiveGetFrameFrameData* request_data,
    void** frame_data,
    VSFrameContext* frame_ctx,
    const VSAPI* vsapi
) {
    if (request_data == nullptr || frame_data == nullptr || *frame_data != nullptr) {
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 D.3 floor-fresh-start proof: invalid cache-hit frameData publication."
        );
        return nullptr;
    }

    request_data->branch = Cnr3LiveGetFrameBranch::cache_hit_return;
    request_data->requested_frame = n;
    request_data->cache_hit_pin_taken = true;
    request_data->source_requested = true;
#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN)
    cnr3_diag_dsum12_observe_branch_cache_hit(data.dsum12_recovery_plan);
#endif
    *frame_data = request_data;

    /*
        Option C lifecycle trigger: the cache-hit path does not consume
        source[N] for pixel computation, but requests one real source frame so
        the non-source filter returns only from arAllFramesReady.
    */
    vsapi->requestFrameFilter(n, data.source, frame_ctx);

    return nullptr;
}

const VSFrame* cnr3_publish_live_predecessor_present_compute_from_pinned_predecessor(
    int n,
    Cnr3FilterData& data,
    Cnr3LiveGetFrameFrameData* request_data,
    void** frame_data,
    VSFrameContext* frame_ctx,
    const VSAPI* vsapi
) {
    if (request_data == nullptr ||
        frame_data == nullptr ||
        *frame_data != nullptr ||
        request_data->predecessor_frame != n - 1 ||
        !request_data->predecessor_pin_taken ||
        request_data->pin_list.pin_count() != 1U) {
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 D.3 floor-fresh-start proof: invalid predecessor-present pinned start."
        );
        return nullptr;
    }

    request_data->branch = Cnr3LiveGetFrameBranch::predecessor_present_compute;
    request_data->requested_frame = n;
    request_data->source_requested = true;
#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN)
    cnr3_diag_dsum12_observe_branch_pred_present(data.dsum12_recovery_plan);
#endif
    *frame_data = request_data;

    vsapi->requestFrameFilter(n, data.source, frame_ctx);

    return nullptr;
}

const VSFrame* cnr3_publish_live_frame0_fresh_start(
    int n,
    Cnr3FilterData& data,
    Cnr3LiveGetFrameFrameData* request_data,
    void** frame_data,
    VSFrameContext* frame_ctx,
    const VSAPI* vsapi
) {
    if (request_data == nullptr || frame_data == nullptr || *frame_data != nullptr) {
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 D.3 floor-fresh-start proof: invalid frame-0 frameData publication."
        );
        return nullptr;
    }

    request_data->branch = Cnr3LiveGetFrameBranch::frame0_fresh_start;
    request_data->requested_frame = n;
    request_data->source_requested = true;
#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN)
    cnr3_diag_dsum12_observe_branch_frame0(data.dsum12_recovery_plan);
#endif
    *frame_data = request_data;

    vsapi->requestFrameFilter(n, data.source, frame_ctx);

    return nullptr;
}

bool cnr3_exact_anchor_recovery_plan_is_accepted(
    int n,
    const Cnr3CacheRecoverySearchPlan& plan
) noexcept {
    if (!plan.anchor_found || !plan.anchor_pin_recorded || n <= 0) {
        return false;
    }

    const std::size_t hole_count = plan.hole_frame_numbers.size();

    if (hole_count > static_cast<std::size_t>(n)) {
        return false;
    }

    const int expected_anchor_frame = n - static_cast<int>(hole_count) - 1;

    if (plan.anchor_frame_number != expected_anchor_frame) {
        return false;
    }

    const int first_hole_frame = n - static_cast<int>(hole_count);

    for (std::size_t hole_index = 0U;
        hole_index < hole_count;
        ++hole_index
        ) {
        const int expected_hole_frame = first_hole_frame + static_cast<int>(hole_index);

        if (plan.hole_frame_numbers[hole_index] != expected_hole_frame) {
            return false;
        }
    }

    return true;
}

bool cnr3_floor_fresh_start_recovery_plan_is_accepted(
    int n,
    const Cnr3CacheRecoverySearchPlan& plan
) noexcept {
    if (n <= 0 ||
        plan.anchor_found ||
        plan.anchor_pin_recorded ||
        !plan.search_interval_has_frames ||
        !cnr3_frame_number_is_valid(plan.search_lower_frame) ||
        plan.search_lower_frame >= n ||
        plan.requested_frame != n ||
        !plan.requested_frame_is_repair_target ||
        plan.requested_frame_is_in_hole_catalogue ||
        !plan.hole_frame_numbers.empty()) {
        return false;
    }

    return true;
}

const char* cnr3_recovery_refusal_reason(
    const Cnr3CacheRecoverySearchPlan& plan
) noexcept {
    return !plan.anchor_found
        ? "structural-recovery-refusal"
        : "non-exact-or-non-contiguous-plan";
}

std::string cnr3_join_recovery_refusal_frame_numbers_for_kdt(
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

void cnr3_trace_live_recovery_refusal(
    const Cnr3FilterData& data,
    int requested_frame,
    const Cnr3CacheRecoverySearchPlan& plan,
    const char* refusal_reason
) noexcept {
#if defined(CNR3_KEYSTONE_DEV_TRACE)
    const std::string holes_text =
        cnr3_join_recovery_refusal_frame_numbers_for_kdt(plan.hole_frame_numbers);

    std::fprintf(
        stderr,
        "[KDT] instance=%d N=%d REFUSED branch=%s "
        "anchor_found=%d anchor_pin_recorded=%d anchor=%d "
        "hole_count=%zu holes=%s pin_balance=0\n",
        data.config.instance_id.value,
        requested_frame,
        refusal_reason != nullptr ? refusal_reason : "unknown-recovery-refusal",
        plan.anchor_found ? 1 : 0,
        plan.anchor_pin_recorded ? 1 : 0,
        plan.anchor_frame_number,
        plan.hole_frame_numbers.size(),
        holes_text.c_str()
    );
#else
    (void)data;
    (void)requested_frame;
    (void)plan;
    (void)refusal_reason;
#endif
}

void cnr3_trace_live_hot_zone_observation(
    const Cnr3FilterData& data,
    int requested_frame,
    Cnr3Status observation_status
) noexcept {
#if defined(CNR3_KEYSTONE_DEV_TRACE)
    std::fprintf(
        stderr,
        "[KDT] instance=%d N=%d HOT-ZONE-OBSERVED status=%s\n",
        data.config.instance_id.value,
        requested_frame,
        cnr3_status_name(observation_status)
    );
#else
    (void)data;
    (void)requested_frame;
    (void)observation_status;
#endif
}


#if defined(CNR3_DIAG_COMPUTE_DSUM03_RECOVERY_SEARCH)

int cnr3_diag_live_recovery_search_depth(
    int requested_frame,
    const Cnr3CacheRecoverySearchPlan& plan
) noexcept {
    if (!cnr3_frame_number_is_valid(requested_frame)) {
        return 0;
    }

    if (plan.anchor_found && cnr3_frame_number_is_valid(plan.anchor_frame_number)) {
        return requested_frame - plan.anchor_frame_number;
    }

    if (cnr3_frame_number_is_valid(plan.search_lower_frame)) {
        return requested_frame - plan.search_lower_frame;
    }

    return 0;
}

void cnr3_diag_live_observe_recovery_search_result(
    Cnr3FilterData& data,
    int requested_frame,
    Cnr3Status plan_status,
    Cnr3LiveRecoveryBranch accepted_branch,
    const Cnr3CacheRecoverySearchPlan& plan
) noexcept {
    if (!cnr3_status_is_ok(plan_status)) {
        cnr3_diag_dsum03_observe_search_result(
            data.dsum03_recovery_search,
            false,
            Cnr3DiagDsum03RecoveryTermination::failure,
            0
        );
        return;
    }

    if (accepted_branch == Cnr3LiveRecoveryBranch::exact_anchor) {
        cnr3_diag_dsum03_observe_search_result(
            data.dsum03_recovery_search,
            true,
            Cnr3DiagDsum03RecoveryTermination::present_output,
            cnr3_diag_live_recovery_search_depth(requested_frame, plan)
        );
        return;
    }

    if (accepted_branch == Cnr3LiveRecoveryBranch::floor_fresh_start) {
        cnr3_diag_dsum03_observe_search_result(
            data.dsum03_recovery_search,
            true,
            plan.search_lower_frame == 0
                ? Cnr3DiagDsum03RecoveryTermination::frame0
                : Cnr3DiagDsum03RecoveryTermination::bound,
            cnr3_diag_live_recovery_search_depth(requested_frame, plan)
        );
        return;
    }

    cnr3_diag_dsum03_observe_search_result(
        data.dsum03_recovery_search,
        false,
        Cnr3DiagDsum03RecoveryTermination::failure,
        cnr3_diag_live_recovery_search_depth(requested_frame, plan)
    );
}

#endif

Cnr3Status cnr3_fill_recovery_source_request_numbers(
    int n,
    Cnr3LiveGetFrameFrameData& request_data
) {
    request_data.source_request_frame_numbers.clear();

    try {
        if (request_data.recovery_branch == Cnr3LiveRecoveryBranch::floor_fresh_start) {
            const int floor_frame = request_data.recovery_floor_frame;

            if (!cnr3_frame_number_is_valid(floor_frame) || floor_frame >= n) {
                return Cnr3Status::invalid_argument;
            }

            const std::size_t request_count =
                static_cast<std::size_t>(n - floor_frame + 1);
            request_data.source_request_frame_numbers.reserve(request_count);

            for (int source_frame = floor_frame; source_frame <= n; ++source_frame) {
                request_data.source_request_frame_numbers.push_back(source_frame);
            }

            return Cnr3Status::ok;
        }

        const std::size_t request_count =
            request_data.recovery_plan.hole_frame_numbers.size() + 1U;
        request_data.source_request_frame_numbers.reserve(request_count);

        for (const int hole_frame : request_data.recovery_plan.hole_frame_numbers) {
            request_data.source_request_frame_numbers.push_back(hole_frame);
        }

        request_data.source_request_frame_numbers.push_back(n);
    }
    catch (const std::bad_alloc&) {
        return Cnr3Status::allocation_failed;
    }

    return Cnr3Status::ok;
}

Cnr3Status cnr3_fill_floor_fresh_start_hole_numbers(
    int n,
    Cnr3LiveGetFrameFrameData& request_data
) {
    const int floor_frame = request_data.recovery_floor_frame;

    if (!cnr3_frame_number_is_valid(floor_frame) || floor_frame >= n) {
        return Cnr3Status::invalid_argument;
    }

    request_data.recovery_plan.hole_frame_numbers.clear();

    try {
        request_data.recovery_plan.hole_frame_numbers.reserve(
            static_cast<std::size_t>(n - floor_frame - 1)
        );

        for (int hole_frame = floor_frame + 1; hole_frame < n; ++hole_frame) {
            request_data.recovery_plan.hole_frame_numbers.push_back(hole_frame);
        }
    }
    catch (const std::bad_alloc&) {
        return Cnr3Status::allocation_failed;
    }

    return Cnr3Status::ok;
}

const VSFrame* cnr3_start_live_recovery(
    int n,
    Cnr3FilterData& data,
    Cnr3LiveGetFrameFrameData* request_data,
    void** frame_data,
    VSFrameContext* frame_ctx,
    const VSAPI* vsapi
) {
    if (request_data == nullptr || frame_data == nullptr || *frame_data != nullptr) {
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 D.3 floor-fresh-start proof: invalid recovery frameData start."
        );
        return nullptr;
    }

    Cnr3CacheRecoverySearchPlan recovery_plan{};
    const Cnr3Status plan_status =
        data.output_cache.plan_bounded_recovery_search_and_record_anchor_pin(
            n,
            CNR3_CACHE_BOUNDED_RECOVERY_BACK_RADIUS,
            request_data->pin_list,
            recovery_plan
        );

    if (!cnr3_status_is_ok(plan_status)) {
#if defined(CNR3_DIAG_COMPUTE_DSUM03_RECOVERY_SEARCH)
        cnr3_diag_live_observe_recovery_search_result(
            data,
            n,
            plan_status,
            Cnr3LiveRecoveryBranch::none,
            recovery_plan
        );
#endif
        cnr3_delete_unpublished_frame_data(request_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 D.3 floor-fresh-start proof: bounded recovery plan failed."
        );
        return nullptr;
    }

    Cnr3LiveRecoveryBranch recovery_branch = Cnr3LiveRecoveryBranch::none;
    int recovery_floor_frame = CNR3_INVALID_FRAME_NUMBER;

    if (cnr3_exact_anchor_recovery_plan_is_accepted(n, recovery_plan)) {
        recovery_branch = Cnr3LiveRecoveryBranch::exact_anchor;
    }
    else if (cnr3_floor_fresh_start_recovery_plan_is_accepted(n, recovery_plan)) {
        /*
            No anchor exists yet. The floor-start path will materialize,
            store, and pin output[floor] before arAllFramesReady treats that
            floor output as the consumer foundation for the walked holes.
        */
        recovery_branch = Cnr3LiveRecoveryBranch::floor_fresh_start;
        recovery_floor_frame = recovery_plan.search_lower_frame;
    }
    else {
#if defined(CNR3_DIAG_COMPUTE_DSUM03_RECOVERY_SEARCH)
        cnr3_diag_live_observe_recovery_search_result(
            data,
            n,
            plan_status,
            Cnr3LiveRecoveryBranch::none,
            recovery_plan
        );
#endif
        const char* const refusal_reason = cnr3_recovery_refusal_reason(recovery_plan);
        cnr3_trace_live_recovery_refusal(data, n, recovery_plan, refusal_reason);

        const Cnr3Status discard_status =
            cnr3_delete_unpublished_frame_data(request_data, data.output_cache);

        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            cnr3_status_is_ok(discard_status)
            ? refusal_reason
            : "CNR3 D.3 floor-fresh-start proof: recovery refusal pin discharge failed."
        );
        return nullptr;
    }

    request_data->branch = Cnr3LiveGetFrameBranch::recovery;
    request_data->recovery_branch = recovery_branch;
    request_data->requested_frame = n;
    request_data->predecessor_frame = n - 1;
    request_data->recovery_floor_frame = recovery_floor_frame;
    request_data->recovery_plan = std::move(recovery_plan);

    if (request_data->recovery_branch == Cnr3LiveRecoveryBranch::floor_fresh_start) {
        const Cnr3Status floor_holes_status =
            cnr3_fill_floor_fresh_start_hole_numbers(n, *request_data);

        if (!cnr3_status_is_ok(floor_holes_status)) {
            cnr3_delete_unpublished_frame_data(request_data, data.output_cache);
            cnr3_set_filter_error(
                frame_ctx,
                vsapi,
                "CNR3 D.3 floor-fresh-start proof: failed to derive floor-start holes."
            );
            return nullptr;
        }
    }

    const Cnr3Status source_plan_status =
        cnr3_fill_recovery_source_request_numbers(n, *request_data);

    if (!cnr3_status_is_ok(source_plan_status)) {
        cnr3_delete_unpublished_frame_data(request_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 D.3 floor-fresh-start proof: failed to derive recovery source request set."
        );
        return nullptr;
    }

    try {
        request_data->per_hole_outcomes.assign(
            request_data->recovery_plan.hole_frame_numbers.size(),
            Cnr3LiveRecoveryHoleOutcome::none
        );
    }
    catch (const std::bad_alloc&) {
        cnr3_delete_unpublished_frame_data(request_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 D.3 floor-fresh-start proof: failed to allocate per-hole outcome state."
        );
        return nullptr;
    }

    request_data->source_requested = true;
#if defined(CNR3_DIAG_COMPUTE_DSUM03_RECOVERY_SEARCH)
    cnr3_diag_live_observe_recovery_search_result(
        data,
        n,
        plan_status,
        request_data->recovery_branch,
        request_data->recovery_plan
    );
#endif
#if defined(CNR3_DIAG_COMPUTE_DSUM12_RECOVERY_PLAN)
    request_data->dsum12_recovery_plan_stats = &data.dsum12_recovery_plan;
    cnr3_diag_dsum12_observe_recovery_plan_published(
        data.dsum12_recovery_plan,
        request_data->recovery_branch == Cnr3LiveRecoveryBranch::exact_anchor,
        request_data->recovery_branch == Cnr3LiveRecoveryBranch::floor_fresh_start,
        request_data->recovery_plan.anchor_found,
        request_data->recovery_plan.hole_frame_numbers.size(),
        request_data->recovery_branch == Cnr3LiveRecoveryBranch::exact_anchor
            ? n - request_data->recovery_plan.anchor_frame_number
            : 0
    );
#endif
    *frame_data = request_data;

    for (const int source_frame_number : request_data->source_request_frame_numbers) {
        vsapi->requestFrameFilter(source_frame_number, data.source, frame_ctx);
    }

    return nullptr;
}

} // namespace

const VSFrame* cnr3_arInitial(
    int n,
    Cnr3FilterData& data,
    void** frame_data,
    VSFrameContext* frame_ctx,
    VSCore* core,
    const VSAPI* vsapi
) {
    (void)core;

    if (frame_data == nullptr || *frame_data != nullptr) {
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 D.3 floor-fresh-start proof: frameData was unexpectedly non-null at arInitial."
        );
        return nullptr;
    }

#if defined(CNR3_DIAG_COMPUTE_DSUM01_REQUEST_ORDER)
    cnr3_diag_dsum01_observe_ar_initial(
        data.dsum01_request_order,
        n
    );
#endif

    Cnr3LiveGetFrameFrameData* request_data =
        new (std::nothrow) Cnr3LiveGetFrameFrameData{};

    if (request_data == nullptr) {
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 D.3 floor-fresh-start proof: failed to allocate frameData."
        );
        return nullptr;
    }

    const Cnr3Status hot_zone_observation_status =
        data.output_cache.record_hot_zone_observation(n);

    cnr3_trace_live_hot_zone_observation(
        data,
        n,
        hot_zone_observation_status
    );

    if (!cnr3_status_is_ok(hot_zone_observation_status)) {
        cnr3_delete_unpublished_frame_data(request_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 W.2 hot-zone observation failed at arInitial."
        );
        return nullptr;
    }

    /*
        A-safe-1 route step 1: a present output[N] is selected only by an
        atomic find-and-pin that also records the pin for the activation gap.
    */
    const Cnr3Status cache_hit_pin_status =
        data.output_cache.lookup_frame_and_record_pin(n, request_data->pin_list);

    if (cnr3_status_is_ok(cache_hit_pin_status)) {
        return cnr3_publish_live_cache_hit_return(
            n,
            data,
            request_data,
            frame_data,
            frame_ctx,
            vsapi
        );
    }

    if (cache_hit_pin_status != Cnr3Status::not_found) {
        cnr3_delete_unpublished_frame_data(request_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 D.3 floor-fresh-start proof: failed during cache-hit pin attempt."
        );
        return nullptr;
    }

    if (n == 0) {
        return cnr3_publish_live_frame0_fresh_start(
            n,
            data,
            request_data,
            frame_data,
            frame_ctx,
            vsapi
        );
    }

    request_data->predecessor_frame = n - 1;

    /*
        A-safe-1 route step 3: branch-c is entered only after this atomic
        predecessor find-and-pin succeeds. A miss is only a routing opportunity
        for branch-d recovery, never a durable cache-state fact.
    */
    const Cnr3Status predecessor_pin_status =
        data.output_cache.lookup_frame_and_record_pin(
            request_data->predecessor_frame,
            request_data->pin_list
        );

    if (cnr3_status_is_ok(predecessor_pin_status)) {
        request_data->predecessor_pin_taken = true;

        return cnr3_publish_live_predecessor_present_compute_from_pinned_predecessor(
            n,
            data,
            request_data,
            frame_data,
            frame_ctx,
            vsapi
        );
    }

    if (predecessor_pin_status != Cnr3Status::not_found) {
        cnr3_delete_unpublished_frame_data(request_data, data.output_cache);
        cnr3_set_filter_error(
            frame_ctx,
            vsapi,
            "CNR3 D.3 floor-fresh-start proof: failed during predecessor pin attempt."
        );
        return nullptr;
    }

    return cnr3_start_live_recovery(
        n,
        data,
        request_data,
        frame_data,
        frame_ctx,
        vsapi
    );
}

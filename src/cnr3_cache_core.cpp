#include "cnr3_cache_core.h"

#include <algorithm>
#include <cstdio>
#include <limits>
#include <mutex>
#include <new>
#include <type_traits>
#include <utility>

static_assert(
    std::is_nothrow_move_constructible_v<Cnr3CacheSlot>,
    "Cnr3CacheSlot must be nothrow move-constructible for cache vector insertion."
    );

static_assert(
    std::is_nothrow_move_assignable_v<Cnr3CacheSlot>,
    "Cnr3CacheSlot must be nothrow move-assignable for cache slot removal."
    );

static_assert(
    std::is_nothrow_move_assignable_v<Cnr3OwnedFrameRef>,
    "Cnr3OwnedFrameRef must be nothrow move-assignable for cache slot adoption."
    );

namespace {

    [[nodiscard]] bool cnr3_prune_candidate_distance_order_before(
        Cnr3PruneCandidateDistanceOrderEntry left,
        Cnr3PruneCandidateDistanceOrderEntry right
    ) noexcept {
        if (left.nearest_hot_zone_distance != right.nearest_hot_zone_distance) {
            return left.nearest_hot_zone_distance > right.nearest_hot_zone_distance;
        }

        return left.frame_number < right.frame_number;
    }

    [[nodiscard]] int cnr3_frame_distance_to_hot_zone_boundary(
        int frame_number,
        const Cnr3CacheHotZone& hot_zone
    ) noexcept {
        if (!hot_zone.is_active || !cnr3_frame_number_is_valid(frame_number)) {
            return std::numeric_limits<int>::max();
        }

        if (frame_number < hot_zone.low_frame) {
            return hot_zone.low_frame - frame_number;
        }

        if (frame_number > hot_zone.high_frame) {
            return frame_number - hot_zone.high_frame;
        }

        return 0;
    }

    [[nodiscard]] Cnr3Status cnr3_current_minimal_recovery_plan_status(
        const Cnr3CacheRecoverySearchPlan& plan
    ) noexcept {
        if (
            !cnr3_frame_number_is_valid(plan.requested_frame) ||
            plan.max_back_radius <= 0
            ) {
            return Cnr3Status::invalid_argument;
        }

        const int expected_lower_frame =
            (plan.requested_frame > plan.max_back_radius)
            ? (plan.requested_frame - plan.max_back_radius)
            : 0;
        const int expected_upper_frame =
            (plan.requested_frame > 0)
            ? (plan.requested_frame - 1)
            : CNR3_INVALID_FRAME_NUMBER;
        const bool expected_has_interval =
            cnr3_frame_number_is_valid(expected_upper_frame) &&
            expected_lower_frame <= expected_upper_frame;

        if (
            plan.search_lower_frame != expected_lower_frame ||
            plan.search_upper_frame != expected_upper_frame ||
            plan.search_interval_has_frames != expected_has_interval ||
            !plan.requested_frame_is_repair_target ||
            plan.requested_frame_is_in_hole_catalogue
            ) {
            return Cnr3Status::invariant_violation;
        }

        if (!plan.anchor_found) {
            if (
                cnr3_frame_number_is_valid(plan.anchor_frame_number) ||
                plan.anchor_is_checkpoint ||
                plan.anchor_pin_recorded ||
                !plan.hole_frame_numbers.empty()
                ) {
                return Cnr3Status::invariant_violation;
            }

            return Cnr3Status::ok;
        }

        if (
            !cnr3_frame_number_is_valid(plan.anchor_frame_number) ||
            plan.anchor_frame_number < plan.search_lower_frame ||
            plan.anchor_frame_number > plan.search_upper_frame ||
            plan.anchor_frame_number >= plan.requested_frame
            ) {
            return Cnr3Status::invariant_violation;
        }

        const int expected_hole_count =
            plan.requested_frame - plan.anchor_frame_number - 1;

        if (expected_hole_count < 0) {
            return Cnr3Status::invariant_violation;
        }

        if (
            plan.hole_frame_numbers.size() !=
            static_cast<std::size_t>(expected_hole_count)
            ) {
            return Cnr3Status::invariant_violation;
        }

        for (std::size_t hole_index = 0U;
            hole_index < plan.hole_frame_numbers.size();
            ++hole_index) {
            const int expected_hole_frame =
                plan.anchor_frame_number + 1 + static_cast<int>(hole_index);

            if (
                plan.hole_frame_numbers[hole_index] != expected_hole_frame ||
                plan.hole_frame_numbers[hole_index] == plan.requested_frame
                ) {
                return Cnr3Status::invariant_violation;
            }
        }

        return Cnr3Status::ok;
    }

    [[nodiscard]] Cnr3Status cnr3_consider_prune_candidate_bounded(
        Cnr3PruneCandidateDistanceOrderEntry candidate,
        std::size_t max_select_count,
        std::vector<Cnr3PruneCandidateDistanceOrderEntry>& candidate_order
    ) {
        if (max_select_count == 0U) {
            return Cnr3Status::ok;
        }

        if (candidate_order.capacity() < max_select_count) {
            return Cnr3Status::invalid_argument;
        }

        if (!cnr3_prune_candidate_distance_order_entry_is_valid(candidate)) {
            return Cnr3Status::invariant_violation;
        }

        if (candidate_order.size() < max_select_count) {
            candidate_order.push_back(candidate);
            return Cnr3Status::ok;
        }

        auto worst_selected = std::max_element(
            candidate_order.begin(),
            candidate_order.end(),
            cnr3_prune_candidate_distance_order_before
        );

        if (
            worst_selected != candidate_order.end() &&
            cnr3_prune_candidate_distance_order_before(candidate, *worst_selected)
            ) {
            *worst_selected = candidate;
        }

        return Cnr3Status::ok;
    }

} // namespace

#if defined(CNR3_DIAG_COMPUTE_DSUM05_CACHE_INTEGRITY)
#   define CNR3_DSUM05_FAIL(tag) return observe_cache_invariant_failure_locked((tag))
#else
#   define CNR3_DSUM05_FAIL(tag) return false
#endif

Cnr3OutputCacheCore::Cnr3OutputCacheCore() {
#if defined(CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION)
    cnr3_cache_prune_diagnostic_configure(
        prune_diag_stats_,
        CNR3_CACHE_BOUNDED_RECOVERY_BACK_RADIUS,
        CNR3_CACHE_ACTIVE_CEILING_MAX_FRAMES
    );
#endif
}


void cnr3_keystone_request_plan_reset(
    Cnr3KeystoneRequestPlan& plan
) noexcept {
    plan.branch = Cnr3KeystoneRequestPlanBranch::invalid;
    plan.requested_frame = CNR3_INVALID_FRAME_NUMBER;
    plan.floor_frame = CNR3_INVALID_FRAME_NUMBER;
    plan.start_point_frame = CNR3_INVALID_FRAME_NUMBER;
    plan.predecessor_frame = CNR3_INVALID_FRAME_NUMBER;
    plan.floor_fresh_start_approximation = false;
    plan.hard_status = Cnr3Status::ok;
    plan.hole_frame_numbers.clear();
    plan.source_request_frame_numbers.clear();
}

Cnr3Status cnr3_keystone_request_plan_rebuild_source_request_set(
    Cnr3KeystoneRequestPlan& plan
) {
    plan.source_request_frame_numbers.clear();

    const auto append_source_request = [&plan](int frame_number) -> Cnr3Status {
        if (!cnr3_frame_number_is_valid(frame_number)) {
            return Cnr3Status::invalid_argument;
        }

        plan.source_request_frame_numbers.push_back(frame_number);
        return Cnr3Status::ok;
    };

    switch (plan.branch) {
    case Cnr3KeystoneRequestPlanBranch::direct_cached_output_return:
    case Cnr3KeystoneRequestPlanBranch::hard_status:
        return Cnr3Status::ok;

    case Cnr3KeystoneRequestPlanBranch::frame0_fresh_start:
    case Cnr3KeystoneRequestPlanBranch::predecessor_present:
        return append_source_request(plan.requested_frame);

    case Cnr3KeystoneRequestPlanBranch::bounded_recovery_exact_anchor:
    case Cnr3KeystoneRequestPlanBranch::bounded_recovery_floor_fresh_start:
        for (int hole_frame_number : plan.hole_frame_numbers) {
            const Cnr3Status hole_status = append_source_request(hole_frame_number);
            if (!cnr3_status_is_ok(hole_status)) {
                plan.source_request_frame_numbers.clear();
                return hole_status;
            }
        }
        return append_source_request(plan.requested_frame);

    case Cnr3KeystoneRequestPlanBranch::invalid:
        break;
    }

    return Cnr3Status::invalid_argument;
}

#if defined(CNR3_KEYSTONE_DEV_TRACE)
void cnr3_keystone_dev_trace_summary_observe_plan(
    const Cnr3KeystoneRequestPlan& plan,
    Cnr3KeystoneDevTraceSummary& summary
) noexcept {
    ++summary.total_plan_count;

    switch (plan.branch) {
    case Cnr3KeystoneRequestPlanBranch::direct_cached_output_return:
        ++summary.direct_cached_output_return_count;
        break;
    case Cnr3KeystoneRequestPlanBranch::frame0_fresh_start:
        ++summary.frame0_fresh_start_count;
        break;
    case Cnr3KeystoneRequestPlanBranch::predecessor_present:
        ++summary.predecessor_present_count;
        break;
    case Cnr3KeystoneRequestPlanBranch::bounded_recovery_exact_anchor:
        ++summary.bounded_recovery_exact_anchor_count;
        if (static_cast<int>(plan.hole_frame_numbers.size()) > summary.max_recovery_span) {
            summary.max_recovery_span = static_cast<int>(plan.hole_frame_numbers.size());
        }
        break;
    case Cnr3KeystoneRequestPlanBranch::bounded_recovery_floor_fresh_start:
        ++summary.bounded_recovery_floor_fresh_start_count;
        summary.floor_fresh_start_approximation_seen =
            summary.floor_fresh_start_approximation_seen ||
            plan.floor_fresh_start_approximation;
        if (static_cast<int>(plan.hole_frame_numbers.size()) > summary.max_recovery_span) {
            summary.max_recovery_span = static_cast<int>(plan.hole_frame_numbers.size());
        }
        break;
    case Cnr3KeystoneRequestPlanBranch::hard_status:
        ++summary.hard_status_count;
        break;
    case Cnr3KeystoneRequestPlanBranch::invalid:
        break;
    }
}

namespace {

    [[nodiscard]] Cnr3Status cnr3_keystone_snprintf_status(
        int written,
        std::size_t out_buffer_size
    ) noexcept {
        if (written < 0) {
            return Cnr3Status::invariant_violation;
        }

        if (static_cast<std::size_t>(written) >= out_buffer_size) {
            return Cnr3Status::capacity_exceeded;
        }

        return Cnr3Status::ok;
    }

} // namespace

Cnr3Status cnr3_keystone_format_dev_trace_line(
    const Cnr3KeystoneRequestPlan& plan,
    char* out_buffer,
    std::size_t out_buffer_size
) noexcept {
    if (out_buffer == nullptr || out_buffer_size == 0U) {
        return Cnr3Status::invalid_argument;
    }

    out_buffer[0] = '\0';

    int written = 0;

    switch (plan.branch) {
    case Cnr3KeystoneRequestPlanBranch::direct_cached_output_return:
        written = std::snprintf(
            out_buffer,
            out_buffer_size,
            "[KDT] N=%d CACHE-HIT",
            plan.requested_frame
        );
        break;

    case Cnr3KeystoneRequestPlanBranch::frame0_fresh_start:
        written = std::snprintf(
            out_buffer,
            out_buffer_size,
            "[KDT] N=%d FRAME0-FRESH src=%zu",
            plan.requested_frame,
            plan.source_request_frame_numbers.size()
        );
        break;

    case Cnr3KeystoneRequestPlanBranch::predecessor_present:
        written = std::snprintf(
            out_buffer,
            out_buffer_size,
            "[KDT] N=%d PRED-PRESENT pred=%d src=%zu",
            plan.requested_frame,
            plan.predecessor_frame,
            plan.source_request_frame_numbers.size()
        );
        break;

    case Cnr3KeystoneRequestPlanBranch::bounded_recovery_exact_anchor:
        written = std::snprintf(
            out_buffer,
            out_buffer_size,
            "[KDT] N=%d RECOVER floor=%d anchor=%d holes=%zu src=%zu",
            plan.requested_frame,
            plan.floor_frame,
            plan.start_point_frame,
            plan.hole_frame_numbers.size(),
            plan.source_request_frame_numbers.size()
        );
        break;

    case Cnr3KeystoneRequestPlanBranch::bounded_recovery_floor_fresh_start:
        written = std::snprintf(
            out_buffer,
            out_buffer_size,
            "[KDT] N=%d RECOVER floor=%d anchor=FLOOR holes=%zu src=%zu flag=APPROX",
            plan.requested_frame,
            plan.floor_frame,
            plan.hole_frame_numbers.size(),
            plan.source_request_frame_numbers.size()
        );
        break;

    case Cnr3KeystoneRequestPlanBranch::hard_status:
        written = std::snprintf(
            out_buffer,
            out_buffer_size,
            "[KDT] N=%d HARD-STATUS status=%s",
            plan.requested_frame,
            cnr3_status_name(plan.hard_status)
        );
        break;

    case Cnr3KeystoneRequestPlanBranch::invalid:
        written = std::snprintf(
            out_buffer,
            out_buffer_size,
            "[KDT] N=%d INVALID",
            plan.requested_frame
        );
        break;
    }

    return cnr3_keystone_snprintf_status(written, out_buffer_size);
}

Cnr3Status cnr3_keystone_format_dev_trace_summary(
    const Cnr3KeystoneDevTraceSummary& summary,
    char* out_buffer,
    std::size_t out_buffer_size
) noexcept {
    if (out_buffer == nullptr || out_buffer_size == 0U) {
        return Cnr3Status::invalid_argument;
    }

    out_buffer[0] = '\0';

    const int written = std::snprintf(
        out_buffer,
        out_buffer_size,
        "[KDT-SUMMARY] total=%d cache_hit=%d frame0=%d pred_present=%d exact_recovery=%d floor_reset=%d hard=%d max_span=%d floor_approx=%s",
        summary.total_plan_count,
        summary.direct_cached_output_return_count,
        summary.frame0_fresh_start_count,
        summary.predecessor_present_count,
        summary.bounded_recovery_exact_anchor_count,
        summary.bounded_recovery_floor_fresh_start_count,
        summary.hard_status_count,
        summary.max_recovery_span,
        summary.floor_fresh_start_approximation_seen ? "yes" : "no"
    );

    return cnr3_keystone_snprintf_status(written, out_buffer_size);
}

#endif

Cnr3CacheSlotId Cnr3CacheSlotIdSource::allocate() noexcept {
    const std::uint64_t value_to_return = (next_value_ != 0U) ? next_value_ : 1U;

    next_value_ =
        (value_to_return < std::numeric_limits<std::uint64_t>::max())
        ? (value_to_return + 1U)
        : 1U;

    return Cnr3CacheSlotId{ value_to_return };
}

std::uint64_t Cnr3CacheSlotIdSource::next_value_for_diagnostics() const noexcept {
    return next_value_;
}

bool cnr3_cache_slot_has_frame(
    const Cnr3CacheSlot& slot
) noexcept {
    return slot.frame.has_frame();
}

bool cnr3_cache_slot_is_indexable(
    const Cnr3CacheSlot& slot
) noexcept {
    return cnr3_cache_slot_id_is_valid(slot.slot_id) &&
        cnr3_frame_number_is_valid(slot.frame_number) &&
        slot.frame.has_frame();
}

bool cnr3_cache_hot_zone_is_valid(
    const Cnr3CacheHotZone& hot_zone
) noexcept {
    if (!hot_zone.is_active) {
        return
            hot_zone.low_frame == CNR3_INVALID_FRAME_NUMBER &&
            hot_zone.high_frame == CNR3_INVALID_FRAME_NUMBER &&
            hot_zone.last_observed_frame == CNR3_INVALID_FRAME_NUMBER;
    }

    if (!cnr3_frame_number_is_valid(hot_zone.low_frame)) {
        return false;
    }

    if (!cnr3_frame_number_is_valid(hot_zone.high_frame)) {
        return false;
    }

    if (!cnr3_frame_number_is_valid(hot_zone.last_observed_frame)) {
        return false;
    }

    if (hot_zone.low_frame > hot_zone.high_frame) {
        return false;
    }

    if (hot_zone.last_observed_frame < hot_zone.low_frame) {
        return false;
    }

    if (hot_zone.last_observed_frame > hot_zone.high_frame) {
        return false;
    }

    return true;
}

bool cnr3_cache_hot_zone_model_invariants_hold(
    const std::vector<Cnr3CacheHotZone>& hot_zones
) noexcept {
    if (hot_zones.size() > CNR3_CACHE_MAX_HOT_ZONES) {
        return false;
    }

    for (const Cnr3CacheHotZone& hot_zone : hot_zones) {
        if (!cnr3_cache_hot_zone_is_valid(hot_zone)) {
            return false;
        }
    }

    return true;
}

Cnr3Status cnr3_calculate_cache_prune_trigger_decision(
    std::uint64_t frame_byte_count,
    std::size_t current_slot_count,
    std::size_t current_checkpoint_count,
    std::size_t retain_checkpoint_count,
    Cnr3CachePruneTriggerDecision& out_decision
) noexcept {
    out_decision = Cnr3CachePruneTriggerDecision{};

    if (frame_byte_count == 0U) {
        return Cnr3Status::invalid_argument;
    }

    std::uint64_t raw_ceiling =
        CNR3_CACHE_BYTE_BUDGET_BYTES / frame_byte_count;

    if (raw_ceiling < CNR3_CACHE_ACTIVE_CEILING_MIN_FRAMES) {
        raw_ceiling = CNR3_CACHE_ACTIVE_CEILING_MIN_FRAMES;
    }

    if (raw_ceiling > CNR3_CACHE_ACTIVE_CEILING_MAX_FRAMES) {
        raw_ceiling = CNR3_CACHE_ACTIVE_CEILING_MAX_FRAMES;
    }

    const std::size_t active_ceiling = static_cast<std::size_t>(raw_ceiling);
    const std::size_t overflow_trigger =
        (active_ceiling * CNR3_CACHE_OVERFLOW_FACTOR_NUMERATOR) /
        CNR3_CACHE_OVERFLOW_FACTOR_DENOMINATOR;
    const bool prune_is_required = current_slot_count > overflow_trigger;
    const bool checkpoint_prune_is_required =
        current_checkpoint_count > CNR3_CACHE_CHECKPOINT_MAX_RETAIN;
    const std::size_t checkpoint_target_count =
        checkpoint_prune_is_required
        ? std::min(current_checkpoint_count, retain_checkpoint_count)
        : current_checkpoint_count;

    out_decision.frame_byte_count = frame_byte_count;
    out_decision.active_ceiling_frame_count = active_ceiling;
    out_decision.overflow_trigger_frame_count = overflow_trigger;
    out_decision.current_slot_count = current_slot_count;
    out_decision.prune_is_required = prune_is_required;
    out_decision.target_slot_count_after_prune =
        prune_is_required ? active_ceiling : current_slot_count;
    out_decision.target_remove_count =
        prune_is_required ? (current_slot_count - active_ceiling) : 0U;
    out_decision.current_checkpoint_count = current_checkpoint_count;
    out_decision.checkpoint_retain_target_count = retain_checkpoint_count;
    out_decision.checkpoint_prune_is_required = checkpoint_prune_is_required;
    out_decision.checkpoint_target_count_after_prune = checkpoint_target_count;
    out_decision.checkpoint_target_remove_count =
        current_checkpoint_count - checkpoint_target_count;

    return Cnr3Status::ok;
}

bool Cnr3OutputCacheCore::empty() const {
    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return empty_locked();
}

std::size_t Cnr3OutputCacheCore::slot_count() const {
    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return slot_count_locked();
}

std::size_t Cnr3OutputCacheCore::index_count() const {
    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return index_count_locked();
}

std::size_t Cnr3OutputCacheCore::checkpoint_count() const {
    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return checkpoint_count_locked();
}

std::size_t Cnr3OutputCacheCore::hot_zone_count() const {
    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return hot_zone_count_locked();
}

Cnr3CacheHotZoneDiagnosticStats Cnr3OutputCacheCore::hot_zone_diagnostic_stats() const {
    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return hot_zone_diagnostic_stats_locked();
}

#if defined(CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE)

Cnr3CacheOwnershipDiagnosticStats Cnr3OutputCacheCore::ownership_diagnostic_stats() const {
    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return ownership_diagnostic_stats_locked();
}

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM05_CACHE_INTEGRITY)

Cnr3CacheIntegrityDiagnosticStats Cnr3OutputCacheCore::cache_integrity_diagnostic_stats() const {
    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return cache_integrity_diagnostic_stats_locked();
}

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE)

Cnr3CacheStoreDiagnosticStats Cnr3OutputCacheCore::cache_store_diagnostic_stats() const {
    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return cache_store_diagnostic_stats_locked();
}

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION)

Cnr3CachePruneDiagnosticStats Cnr3OutputCacheCore::prune_diagnostic_stats() const {
    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return prune_diagnostic_stats_locked();
}

#endif

bool Cnr3OutputCacheCore::frame_is_inside_hot_zone(
    int frame_number
) const {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return false;
    }

    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return frame_is_inside_hot_zone_locked(frame_number);
}

Cnr3Status Cnr3OutputCacheCore::record_hot_zone_observation(
    int frame_number
) {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return Cnr3Status::invalid_argument;
    }

    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return record_hot_zone_observation_locked(frame_number);
}

Cnr3Status Cnr3OutputCacheCore::retire_decay_eligible_hot_zones(
    int current_frame
) {
    if (!cnr3_frame_number_is_valid(current_frame)) {
        return Cnr3Status::invalid_argument;
    }

    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return retire_decay_eligible_hot_zones_locked(current_frame);
}

int Cnr3OutputCacheCore::total_pin_count() const {
    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return total_pin_count_locked();
}

bool Cnr3OutputCacheCore::cache_state_invariants_hold() const {
    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return cache_state_invariants_hold_locked();
}

Cnr3Status Cnr3OutputCacheCore::store_noncheckpoint_owned_frame(
    int frame_number,
    Cnr3OwnedFrameRef frame
) {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return Cnr3Status::invalid_argument;
    }

    if (!frame.has_frame()) {
        return Cnr3Status::invalid_argument;
    }

    Cnr3Status status = Cnr3Status::invariant_violation;

    /*
        Keep the lock scope nested so a rejected owned frame is released after
        cache_mutex_ is unlocked.
    */
    {
        const std::lock_guard<std::mutex> lock(cache_mutex_);

        status = store_noncheckpoint_owned_frame_locked(frame_number, frame);
    }

    return status;
}

Cnr3Status Cnr3OutputCacheCore::store_checkpoint_owned_frame(
    int frame_number,
    Cnr3OwnedFrameRef frame
) {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return Cnr3Status::invalid_argument;
    }

    if (!frame.has_frame()) {
        return Cnr3Status::invalid_argument;
    }

    Cnr3Status status = Cnr3Status::invariant_violation;

    /*
        Keep the lock scope nested so a rejected owned frame is released after
        cache_mutex_ is unlocked.
    */
    {
        const std::lock_guard<std::mutex> lock(cache_mutex_);

        status = store_checkpoint_owned_frame_locked(frame_number, frame);
    }

    return status;
}


Cnr3Status Cnr3OutputCacheCore::store_owned_frame_and_record_pin(
    int frame_number,
    Cnr3OwnedFrameRef frame,
    bool is_checkpoint,
    Cnr3CachePinList& pin_list,
    Cnr3CacheAs2StoreRecordSummary& out_summary
) {
    out_summary = Cnr3CacheAs2StoreRecordSummary{};

    if (!cnr3_frame_number_is_valid(frame_number)) {
        return Cnr3Status::invalid_argument;
    }

    if (!frame.has_frame()) {
        return Cnr3Status::invalid_argument;
    }

    const Cnr3Status reserve_status = pin_list.reserve_for_additional_pins(1U);

    if (!cnr3_status_is_ok(reserve_status)) {
        return reserve_status;
    }

    Cnr3Status status = Cnr3Status::invariant_violation;

    /*
        frame is intentionally taken by value by this public helper. On a
        first-in-best-dressed duplicate, the locked helper leaves the rejected
        loser in this outer-scope parameter, so it releases after this nested
        lock scope unlocks cache_mutex_. Do not change this to by-reference or
        move the loser's lifetime inside the lock scope; either change would
        free the loser frame while holding the cache lock, violating the
        freeFrame-outside-lock rule.
    */
    {
        const std::lock_guard<std::mutex> lock(cache_mutex_);

        status = store_owned_frame_and_record_pin_locked(
            frame_number,
            frame,
            is_checkpoint,
            pin_list,
            out_summary
        );
    }

    return status;
}

Cnr3Status Cnr3OutputCacheCore::store_recovery_plan_hole_owned_frame_and_record_pin(
    const Cnr3CacheRecoverySearchPlan& recovery_plan,
    int hole_frame_number,
    Cnr3OwnedFrameRef frame,
    bool is_checkpoint,
    Cnr3CachePinList& pin_list,
    Cnr3CacheAs2StoreRecordSummary& out_summary
) {
    out_summary = Cnr3CacheAs2StoreRecordSummary{};

    if (!cnr3_frame_number_is_valid(hole_frame_number)) {
        return Cnr3Status::invalid_argument;
    }

    if (!frame.has_frame()) {
        return Cnr3Status::invalid_argument;
    }

    const Cnr3Status plan_status =
        cnr3_current_minimal_recovery_plan_status(recovery_plan);

    if (!cnr3_status_is_ok(plan_status)) {
        return plan_status;
    }

    if (hole_frame_number == recovery_plan.requested_frame) {
        return Cnr3Status::invalid_argument;
    }

    const auto hole_it = std::find(
        recovery_plan.hole_frame_numbers.begin(),
        recovery_plan.hole_frame_numbers.end(),
        hole_frame_number
    );

    if (hole_it == recovery_plan.hole_frame_numbers.end()) {
        return Cnr3Status::not_found;
    }

    return store_owned_frame_and_record_pin(
        hole_frame_number,
        std::move(frame),
        is_checkpoint,
        pin_list,
        out_summary
    );
}


Cnr3Status Cnr3OutputCacheCore::store_production_output_and_prune(
    int stored_frame_number,
    int activation_target_frame,
    Cnr3OwnedFrameRef frame,
    bool is_checkpoint,
    std::uint64_t frame_byte_count,
    Cnr3CombinedStoreAndPruneSummary& out_summary
) {
    out_summary = Cnr3CombinedStoreAndPruneSummary{};
    out_summary.store_kind = is_checkpoint
        ? Cnr3CacheStoreKind::ProductionCheckpoint
        : Cnr3CacheStoreKind::ProductionNonCheckpoint;
    out_summary.stored_frame_number = stored_frame_number;
    out_summary.activation_target_frame = activation_target_frame;

    if (
        !cnr3_frame_number_is_valid(stored_frame_number) ||
        !cnr3_frame_number_is_valid(activation_target_frame) ||
        !frame.has_frame() ||
        frame_byte_count == 0U
        ) {
        out_summary.store_status = Cnr3Status::invalid_argument;
        return Cnr3Status::invalid_argument;
    }

    return store_owned_frame_and_prune_impl(
        stored_frame_number,
        activation_target_frame,
        frame,
        is_checkpoint,
        out_summary.store_kind,
        frame_byte_count,
        nullptr,
        out_summary
    );
}

Cnr3Status Cnr3OutputCacheCore::store_as2_floor_and_prune(
    int stored_frame_number,
    int activation_target_frame,
    Cnr3OwnedFrameRef frame,
    bool is_checkpoint,
    std::uint64_t frame_byte_count,
    Cnr3CachePinList& pin_list,
    Cnr3CombinedStoreAndPruneSummary& out_summary
) {
    out_summary = Cnr3CombinedStoreAndPruneSummary{};
    out_summary.store_kind = is_checkpoint
        ? Cnr3CacheStoreKind::As2ConsumerCheckpoint
        : Cnr3CacheStoreKind::As2ConsumerNonCheckpoint;
    out_summary.stored_frame_number = stored_frame_number;
    out_summary.activation_target_frame = activation_target_frame;

    if (
        !cnr3_frame_number_is_valid(stored_frame_number) ||
        !cnr3_frame_number_is_valid(activation_target_frame) ||
        !frame.has_frame() ||
        frame_byte_count == 0U
        ) {
        out_summary.store_status = Cnr3Status::invalid_argument;
        return Cnr3Status::invalid_argument;
    }

    return store_owned_frame_and_prune_impl(
        stored_frame_number,
        activation_target_frame,
        frame,
        is_checkpoint,
        out_summary.store_kind,
        frame_byte_count,
        &pin_list,
        out_summary
    );
}

Cnr3Status Cnr3OutputCacheCore::store_recovery_hole_and_prune(
    const Cnr3CacheRecoverySearchPlan& recovery_plan,
    int hole_frame_number,
    int activation_target_frame,
    Cnr3OwnedFrameRef frame,
    bool is_checkpoint,
    std::uint64_t frame_byte_count,
    Cnr3CachePinList& pin_list,
    Cnr3CombinedStoreAndPruneSummary& out_summary
) {
    out_summary = Cnr3CombinedStoreAndPruneSummary{};
    out_summary.store_kind = is_checkpoint
        ? Cnr3CacheStoreKind::As2ConsumerCheckpoint
        : Cnr3CacheStoreKind::As2ConsumerNonCheckpoint;
    out_summary.stored_frame_number = hole_frame_number;
    out_summary.activation_target_frame = activation_target_frame;

    if (
        !cnr3_frame_number_is_valid(hole_frame_number) ||
        !cnr3_frame_number_is_valid(activation_target_frame) ||
        !frame.has_frame() ||
        frame_byte_count == 0U
        ) {
        out_summary.store_status = Cnr3Status::invalid_argument;
        return Cnr3Status::invalid_argument;
    }

    const Cnr3Status plan_status =
        cnr3_current_minimal_recovery_plan_status(recovery_plan);

    if (!cnr3_status_is_ok(plan_status)) {
        out_summary.store_status = plan_status;
        return plan_status;
    }

    if (hole_frame_number == recovery_plan.requested_frame) {
        out_summary.store_status = Cnr3Status::invalid_argument;
        return Cnr3Status::invalid_argument;
    }

    const auto hole_it = std::find(
        recovery_plan.hole_frame_numbers.begin(),
        recovery_plan.hole_frame_numbers.end(),
        hole_frame_number
    );

    if (hole_it == recovery_plan.hole_frame_numbers.end()) {
        out_summary.store_status = Cnr3Status::not_found;
        return Cnr3Status::not_found;
    }

    return store_owned_frame_and_prune_impl(
        hole_frame_number,
        activation_target_frame,
        frame,
        is_checkpoint,
        out_summary.store_kind,
        frame_byte_count,
        &pin_list,
        out_summary
    );
}

Cnr3Status Cnr3OutputCacheCore::store_owned_frame_and_prune_impl(
    int stored_frame_number,
    int activation_target_frame,
    Cnr3OwnedFrameRef& frame,
    bool is_checkpoint,
    Cnr3CacheStoreKind store_kind,
    std::uint64_t frame_byte_count,
    Cnr3CachePinList* pin_list,
    Cnr3CombinedStoreAndPruneSummary& out_summary
) {
    if (
        !cnr3_frame_number_is_valid(stored_frame_number) ||
        !cnr3_frame_number_is_valid(activation_target_frame) ||
        !frame.has_frame() ||
        frame_byte_count == 0U ||
        store_kind == Cnr3CacheStoreKind::Invalid
        ) {
        out_summary.store_status = Cnr3Status::invalid_argument;
        return Cnr3Status::invalid_argument;
    }

    const bool is_as2_kind =
        store_kind == Cnr3CacheStoreKind::As2ConsumerNonCheckpoint ||
        store_kind == Cnr3CacheStoreKind::As2ConsumerCheckpoint;

    if (is_as2_kind != (pin_list != nullptr)) {
        out_summary.store_status = Cnr3Status::invalid_argument;
        return Cnr3Status::invalid_argument;
    }

    std::vector<Cnr3PruneCandidateDistanceOrderEntry> candidate_order{};
    std::vector<Cnr3PruneCandidateDistanceOrderEntry> checkpoint_candidate_order{};
    std::vector<int> selected_frame_numbers{};
    std::vector<Cnr3CacheSlot> detached_slots{};

    if (
        CNR3_CACHE_BOUNDED_PRUNE_MAX_VICTIMS > candidate_order.max_size() ||
        CNR3_CACHE_BOUNDED_PRUNE_MAX_VICTIMS > checkpoint_candidate_order.max_size() ||
        CNR3_CACHE_BOUNDED_PRUNE_MAX_VICTIMS > selected_frame_numbers.max_size() ||
        CNR3_CACHE_BOUNDED_PRUNE_MAX_VICTIMS > detached_slots.max_size()
        ) {
        out_summary.store_status = Cnr3Status::capacity_exceeded;
        return Cnr3Status::capacity_exceeded;
    }

    if (is_as2_kind) {
        const Cnr3Status reserve_pin_status =
            pin_list->reserve_for_additional_pins(1U);

        if (!cnr3_status_is_ok(reserve_pin_status)) {
            out_summary.store_status = reserve_pin_status;
            return reserve_pin_status;
        }
    }

    try {
        candidate_order.reserve(CNR3_CACHE_BOUNDED_PRUNE_MAX_VICTIMS);
        checkpoint_candidate_order.reserve(CNR3_CACHE_BOUNDED_PRUNE_MAX_VICTIMS);
        selected_frame_numbers.reserve(CNR3_CACHE_BOUNDED_PRUNE_MAX_VICTIMS);
        detached_slots.reserve(CNR3_CACHE_BOUNDED_PRUNE_MAX_VICTIMS);
    }
    catch (const std::bad_alloc&) {
        out_summary.store_status = Cnr3Status::allocation_failed;
        return Cnr3Status::allocation_failed;
    }

    Cnr3Status status = Cnr3Status::invariant_violation;
#if defined(CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE)
    bool store_outcome_observed = false;
#endif

    {
        const std::lock_guard<std::mutex> lock(cache_mutex_);

        if (is_as2_kind) {
            Cnr3CacheAs2StoreRecordSummary as2_summary{};
            const Cnr3Status as2_status =
                store_owned_frame_and_record_pin_locked(
                    stored_frame_number,
                    frame,
                    is_checkpoint,
                    *pin_list,
                    as2_summary
                );

            out_summary.as2_summary = as2_summary;

            if (!cnr3_status_is_ok(as2_status)) {
                out_summary.store_status = as2_status;
#if defined(CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE)
                observe_store_outcome_locked(out_summary);
                store_outcome_observed = true;
#endif
                return as2_status;
            }

            if (!as2_summary.pin_recorded) {
                out_summary.store_status = Cnr3Status::invariant_violation;
#if defined(CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE)
                observe_store_outcome_locked(out_summary);
                store_outcome_observed = true;
#endif
                return Cnr3Status::invariant_violation;
            }

            if (as2_summary.duplicate_existing_slot) {
                out_summary.store_status = Cnr3Status::duplicate;
            }
            else if (as2_summary.inserted_new_slot) {
                out_summary.store_status = Cnr3Status::ok;
            }
            else {
                out_summary.store_status = Cnr3Status::invariant_violation;
#if defined(CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE)
                observe_store_outcome_locked(out_summary);
                store_outcome_observed = true;
#endif
                return Cnr3Status::invariant_violation;
            }
        }
        else {
            out_summary.store_status = store_owned_frame_locked(
                stored_frame_number,
                frame,
                is_checkpoint
            );

            if (
                out_summary.store_status != Cnr3Status::ok &&
                out_summary.store_status != Cnr3Status::duplicate
                ) {
#if defined(CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE)
                observe_store_outcome_locked(out_summary);
                store_outcome_observed = true;
#endif
                return out_summary.store_status;
            }
        }

        out_summary.retire_status =
            retire_decay_eligible_hot_zones_locked(activation_target_frame);

        if (!cnr3_status_is_ok(out_summary.retire_status)) {
#if defined(CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE)
            if (!store_outcome_observed) {
                observe_store_outcome_locked(out_summary);
                store_outcome_observed = true;
            }
#endif
            return out_summary.retire_status;
        }

        out_summary.prune_status = execute_bounded_prune_pass_locked(
            frame_byte_count,
            CNR3_CACHE_CHECKPOINT_MIN_RETAIN,
            CNR3_CACHE_BOUNDED_PRUNE_MAX_VICTIMS,
            candidate_order,
            checkpoint_candidate_order,
            selected_frame_numbers,
            detached_slots,
            out_summary.prune_summary
        );

        if (!cnr3_status_is_ok(out_summary.prune_status)) {
#if defined(CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE)
            if (!store_outcome_observed) {
                observe_store_outcome_locked(out_summary);
                store_outcome_observed = true;
            }
#endif
            return out_summary.prune_status;
        }

#if defined(CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE)
        if (!store_outcome_observed) {
            observe_store_outcome_locked(out_summary);
            store_outcome_observed = true;
        }
#endif

        status = Cnr3Status::ok;
    }

    return status;
}

Cnr3Status Cnr3OutputCacheCore::remove_unpinned_frame(
    int frame_number
) {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return Cnr3Status::invalid_argument;
    }

    Cnr3CacheSlot detached_slot{};
    Cnr3Status status = Cnr3Status::invariant_violation;

    /*
        Keep the lock scope nested so the detached slot releases its owned
        frame after cache_mutex_ is unlocked.
    */
    {
        const std::lock_guard<std::mutex> lock(cache_mutex_);

        status = remove_unpinned_frame_locked(frame_number, detached_slot);
    }

    return status;
}

Cnr3Status Cnr3OutputCacheCore::remove_selected_unpinned_frames_bounded(
    const std::vector<int>& candidate_frame_numbers,
    std::size_t max_remove_count,
    std::size_t& out_removed_count
) {
    out_removed_count = 0U;

    if (max_remove_count == 0U || candidate_frame_numbers.empty()) {
        return Cnr3Status::ok;
    }

    for (const int frame_number : candidate_frame_numbers) {
        if (!cnr3_frame_number_is_valid(frame_number)) {
            return Cnr3Status::invalid_argument;
        }
    }

    std::vector<Cnr3CacheSlot> detached_slots{};

    if (max_remove_count > detached_slots.max_size()) {
        return Cnr3Status::capacity_exceeded;
    }

    try {
        detached_slots.reserve(max_remove_count);
    }
    catch (const std::bad_alloc&) {
        return Cnr3Status::allocation_failed;
    }

    Cnr3Status status = Cnr3Status::invariant_violation;

    /*
        Keep the lock scope nested so the detached slots release their owned
        frames after cache_mutex_ is unlocked.
    */
    {
        const std::lock_guard<std::mutex> lock(cache_mutex_);

        status = remove_selected_unpinned_frames_bounded_locked(
            candidate_frame_numbers,
            max_remove_count,
            detached_slots,
            out_removed_count
        );
    }

    return status;
}

Cnr3Status Cnr3OutputCacheCore::remove_unpinned_noncheckpoint_frames_bounded(
    std::size_t max_remove_count,
    std::size_t& out_removed_count
) {
    out_removed_count = 0U;

    if (max_remove_count == 0U) {
        return Cnr3Status::ok;
    }

    std::vector<Cnr3CacheSlot> detached_slots{};

    if (max_remove_count > detached_slots.max_size()) {
        return Cnr3Status::capacity_exceeded;
    }

    try {
        detached_slots.reserve(max_remove_count);
    }
    catch (const std::bad_alloc&) {
        return Cnr3Status::allocation_failed;
    }

    Cnr3Status status = Cnr3Status::invariant_violation;

    /*
        Keep the lock scope nested so the detached slots release their owned
        frames after cache_mutex_ is unlocked.
    */
    {
        const std::lock_guard<std::mutex> lock(cache_mutex_);

        status = remove_unpinned_noncheckpoint_frames_bounded_locked(
            max_remove_count,
            detached_slots,
            out_removed_count
        );
    }

    return status;
}

Cnr3Status Cnr3OutputCacheCore::remove_unpinned_noncheckpoint_frames_outside_hot_zones_bounded(
    std::size_t max_remove_count,
    std::size_t& out_removed_count
) {
    out_removed_count = 0U;

    if (max_remove_count == 0U) {
        return Cnr3Status::ok;
    }

    std::vector<Cnr3CacheSlot> detached_slots{};

    if (max_remove_count > detached_slots.max_size()) {
        return Cnr3Status::capacity_exceeded;
    }

    try {
        detached_slots.reserve(max_remove_count);
    }
    catch (const std::bad_alloc&) {
        return Cnr3Status::allocation_failed;
    }

    Cnr3Status status = Cnr3Status::invariant_violation;

    /*
        Keep the lock scope nested so the detached slots release their owned
        frames after cache_mutex_ is unlocked.
    */
    {
        const std::lock_guard<std::mutex> lock(cache_mutex_);

        status = remove_unpinned_noncheckpoint_frames_outside_hot_zones_bounded_locked(
            max_remove_count,
            detached_slots,
            out_removed_count
        );
    }

    return status;
}

Cnr3Status Cnr3OutputCacheCore::select_unpinned_noncheckpoint_frames_outside_hot_zones_by_distance_bounded(
    std::size_t max_select_count,
    std::vector<Cnr3PruneCandidateDistanceOrderEntry>& out_candidate_order
) const {
    out_candidate_order.clear();

    if (max_select_count == 0U) {
        return Cnr3Status::ok;
    }

    if (max_select_count > out_candidate_order.max_size()) {
        return Cnr3Status::capacity_exceeded;
    }

    if (out_candidate_order.capacity() < max_select_count) {
        return Cnr3Status::invalid_argument;
    }

    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return select_unpinned_noncheckpoint_frames_outside_hot_zones_by_distance_bounded_locked(
        max_select_count,
        out_candidate_order
    );
}

Cnr3Status Cnr3OutputCacheCore::select_composite_prune_candidates_bounded(
    bool noncheckpoint_capacity_permits,
    std::size_t retain_checkpoint_count,
    std::size_t max_select_count,
    std::vector<Cnr3PruneCandidateDistanceOrderEntry>& out_candidate_order
) const {
    out_candidate_order.clear();

    if (max_select_count == 0U) {
        return Cnr3Status::ok;
    }

    if (max_select_count > out_candidate_order.max_size()) {
        return Cnr3Status::capacity_exceeded;
    }

    if (out_candidate_order.capacity() < max_select_count) {
        return Cnr3Status::invalid_argument;
    }

    std::vector<Cnr3PruneCandidateDistanceOrderEntry> checkpoint_candidate_order{};

    if (max_select_count > checkpoint_candidate_order.max_size()) {
        return Cnr3Status::capacity_exceeded;
    }

    try {
        checkpoint_candidate_order.reserve(max_select_count);
    }
    catch (const std::bad_alloc&) {
        return Cnr3Status::allocation_failed;
    }

    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return select_composite_prune_candidates_bounded_locked(
        noncheckpoint_capacity_permits,
        retain_checkpoint_count,
        max_select_count,
        out_candidate_order,
        checkpoint_candidate_order
    );
}

Cnr3Status Cnr3OutputCacheCore::execute_bounded_prune_pass(
    std::uint64_t frame_byte_count,
    std::size_t retain_checkpoint_count,
    std::size_t max_remove_count,
    Cnr3CachePruneExecutionSummary& out_summary
) {
    out_summary = Cnr3CachePruneExecutionSummary{};

    if (frame_byte_count == 0U) {
        return Cnr3Status::invalid_argument;
    }

    std::vector<Cnr3PruneCandidateDistanceOrderEntry> candidate_order{};
    std::vector<Cnr3PruneCandidateDistanceOrderEntry> checkpoint_candidate_order{};
    std::vector<int> selected_frame_numbers{};
    std::vector<Cnr3CacheSlot> detached_slots{};

    if (
        max_remove_count > candidate_order.max_size() ||
        max_remove_count > checkpoint_candidate_order.max_size() ||
        max_remove_count > selected_frame_numbers.max_size() ||
        max_remove_count > detached_slots.max_size()
        ) {
        return Cnr3Status::capacity_exceeded;
    }

    try {
        candidate_order.reserve(max_remove_count);
        checkpoint_candidate_order.reserve(max_remove_count);
        selected_frame_numbers.reserve(max_remove_count);
        detached_slots.reserve(max_remove_count);
    }
    catch (const std::bad_alloc&) {
        return Cnr3Status::allocation_failed;
    }

    Cnr3Status status = Cnr3Status::invariant_violation;

    /*
        Keep the lock scope nested so detached slots release their owned frame
        references after cache_mutex_ is unlocked.
    */
    {
        const std::lock_guard<std::mutex> lock(cache_mutex_);

        status = execute_bounded_prune_pass_locked(
            frame_byte_count,
            retain_checkpoint_count,
            max_remove_count,
            candidate_order,
            checkpoint_candidate_order,
            selected_frame_numbers,
            detached_slots,
            out_summary
        );
    }

    return status;
}

Cnr3Status Cnr3OutputCacheCore::plan_bounded_recovery_search(
    int requested_frame,
    int max_back_radius,
    Cnr3CacheRecoverySearchPlan& out_plan
) const {
    out_plan = Cnr3CacheRecoverySearchPlan{};

    if (!cnr3_frame_number_is_valid(requested_frame)) {
        return Cnr3Status::invalid_argument;
    }

    if (max_back_radius <= 0) {
        return Cnr3Status::invalid_argument;
    }

    const std::size_t max_hole_count = static_cast<std::size_t>(max_back_radius);

    if (max_hole_count > out_plan.hole_frame_numbers.max_size()) {
        return Cnr3Status::capacity_exceeded;
    }

    try {
        out_plan.hole_frame_numbers.reserve(max_hole_count);
    }
    catch (const std::bad_alloc&) {
        return Cnr3Status::allocation_failed;
    }

    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return plan_bounded_recovery_search_locked(
        requested_frame,
        max_back_radius,
        out_plan
    );
}

Cnr3Status Cnr3OutputCacheCore::plan_bounded_recovery_search_and_record_anchor_pin(
    int requested_frame,
    int max_back_radius,
    Cnr3CachePinList& pin_list,
    Cnr3CacheRecoverySearchPlan& out_plan
) {
    out_plan = Cnr3CacheRecoverySearchPlan{};

    if (!cnr3_frame_number_is_valid(requested_frame)) {
        return Cnr3Status::invalid_argument;
    }

    if (max_back_radius <= 0) {
        return Cnr3Status::invalid_argument;
    }

    const std::size_t max_hole_count = static_cast<std::size_t>(max_back_radius);

    if (max_hole_count > out_plan.hole_frame_numbers.max_size()) {
        return Cnr3Status::capacity_exceeded;
    }

    try {
        out_plan.hole_frame_numbers.reserve(max_hole_count);
    }
    catch (const std::bad_alloc&) {
        return Cnr3Status::allocation_failed;
    }

    const Cnr3Status reserve_status = pin_list.reserve_for_additional_pins(1U);

    if (!cnr3_status_is_ok(reserve_status)) {
        return reserve_status;
    }

    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return plan_bounded_recovery_search_and_record_anchor_pin_locked(
        requested_frame,
        max_back_radius,
        pin_list,
        out_plan
    );
}


Cnr3Status Cnr3OutputCacheCore::remove_unpinned_checkpoints_above_retain_count_bounded(
    std::size_t retain_checkpoint_count,
    std::size_t max_remove_count,
    std::size_t& out_removed_count
) {
    out_removed_count = 0U;

    if (max_remove_count == 0U) {
        return Cnr3Status::ok;
    }

    std::vector<Cnr3CacheSlot> detached_slots{};

    if (max_remove_count > detached_slots.max_size()) {
        return Cnr3Status::capacity_exceeded;
    }

    try {
        detached_slots.reserve(max_remove_count);
    }
    catch (const std::bad_alloc&) {
        return Cnr3Status::allocation_failed;
    }

    Cnr3Status status = Cnr3Status::invariant_violation;

    /*
        Keep the lock scope nested so the detached slots release their owned
        frames after cache_mutex_ is unlocked.
    */
    {
        const std::lock_guard<std::mutex> lock(cache_mutex_);

        status = remove_unpinned_checkpoints_above_retain_count_bounded_locked(
            retain_checkpoint_count,
            max_remove_count,
            detached_slots,
            out_removed_count
        );
    }

    return status;
}

Cnr3Status Cnr3OutputCacheCore::lookup_frame_and_add_ref(
    int frame_number,
    const VSAPI* vsapi,
    Cnr3OwnedFrameRef& out_frame
) const {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return Cnr3Status::invalid_argument;
    }

    if (vsapi == nullptr || vsapi->addFrameRef == nullptr || vsapi->freeFrame == nullptr) {
        return Cnr3Status::invalid_argument;
    }

    if (out_frame.has_frame()) {
        return Cnr3Status::invalid_argument;
    }

    const VSFrame* acquired_frame = nullptr;
    Cnr3Status status = Cnr3Status::invariant_violation;

    {
        const std::lock_guard<std::mutex> lock(cache_mutex_);

        status = lookup_frame_and_add_ref_locked(
            frame_number,
            vsapi,
            &acquired_frame
        );
    }

    if (!cnr3_status_is_ok(status)) {
        return status;
    }

    if (acquired_frame == nullptr) {
        return Cnr3Status::vapoursynth_error;
    }

    const Cnr3Status adopt_status =
        out_frame.reset_to_owned_frame(acquired_frame, vsapi);

    if (!cnr3_status_is_ok(adopt_status)) {
        /*
            This should be unreachable because vsapi was validated before the
            lookup. If it ever happens, rebalance the acquired lookup reference
            outside cache_mutex_ before reporting the ownership error.
        */
#if defined(CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE)
        observe_lookup_ref_released_by_cache_core();
        observe_ownership_error();
#endif
        vsapi->freeFrame(acquired_frame);

        return adopt_status;
    }

#if defined(CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE)
    observe_lookup_ref_transferred();
#endif

    return Cnr3Status::ok;
}

Cnr3Status Cnr3OutputCacheCore::lookup_frame_and_record_pin(
    int frame_number,
    Cnr3CachePinList& pin_list
) {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return Cnr3Status::invalid_argument;
    }

    const Cnr3Status reserve_status = pin_list.reserve_for_additional_pins(1U);

    if (!cnr3_status_is_ok(reserve_status)) {
        return reserve_status;
    }

    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return lookup_frame_and_record_pin_locked(frame_number, pin_list);
}

Cnr3Status Cnr3OutputCacheCore::unpin_frame(
    Cnr3CacheSlotPinToken& pin_token
) {
    if (!cnr3_cache_slot_pin_token_is_valid(pin_token)) {
        return Cnr3Status::invalid_argument;
    }

    const std::lock_guard<std::mutex> lock(cache_mutex_);

    return unpin_frame_locked(pin_token);
}

Cnr3Status Cnr3OutputCacheCore::discharge_pin_list(
    Cnr3CachePinList& pin_list
) {
    Cnr3Status first_failure_status = Cnr3Status::ok;

    const std::lock_guard<std::mutex> lock(cache_mutex_);

    for (std::size_t pin_index = 0;
        pin_index < pin_list.used_pin_count_;
        ++pin_index
        ) {
        Cnr3CacheSlotPinToken& pin_token = pin_list.pin_tokens_[pin_index];

        if (!cnr3_cache_slot_pin_token_is_valid(pin_token)) {
            continue;
        }

        const Cnr3Status unpin_status = unpin_frame_locked(pin_token);

        if (
            !cnr3_status_is_ok(unpin_status) &&
            cnr3_status_is_ok(first_failure_status)
            ) {
            first_failure_status = unpin_status;
        }
    }

    if (cnr3_status_is_ok(first_failure_status)) {
        for (std::size_t pin_index = 0;
            pin_index < pin_list.used_pin_count_;
            ++pin_index
            ) {
            cnr3_cache_slot_pin_token_reset(pin_list.pin_tokens_[pin_index]);
        }

        pin_list.used_pin_count_ = 0U;
    }

    return first_failure_status;
}

Cnr3Status Cnr3OutputCacheCore::clear() {
    std::vector<Cnr3CacheSlot> detached_slots{};
    Cnr3Status status = Cnr3Status::invariant_violation;

    /*
        Keep the lock scope nested so detached Cnr3OwnedFrameRef objects are
        destroyed only after cache_mutex_ is unlocked.
    */
    {
        const std::lock_guard<std::mutex> lock(cache_mutex_);

        status = clear_locked(detached_slots);
    }

    return status;
}

bool Cnr3OutputCacheCore::empty_locked() const noexcept {
    return slots_.empty() &&
        frame_index_.empty() &&
        checkpoint_slot_positions_.empty() &&
        hot_zones_.empty();
}

std::size_t Cnr3OutputCacheCore::slot_count_locked() const noexcept {
    return slots_.size();
}

std::size_t Cnr3OutputCacheCore::index_count_locked() const noexcept {
    return frame_index_.size();
}

std::size_t Cnr3OutputCacheCore::checkpoint_count_locked() const noexcept {
    return checkpoint_slot_positions_.size();
}

std::size_t Cnr3OutputCacheCore::hot_zone_count_locked() const noexcept {
    return hot_zones_.size();
}

Cnr3CacheHotZoneDiagnosticStats Cnr3OutputCacheCore::hot_zone_diagnostic_stats_locked() const noexcept {
    return hot_zone_diag_stats_;
}

#if defined(CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE)

Cnr3CacheOwnershipDiagnosticStats Cnr3OutputCacheCore::ownership_diagnostic_stats_locked() const noexcept {
    Cnr3CacheOwnershipDiagnosticStats snapshot = ownership_diag_stats_;
    snapshot.total_pin_count_crosscheck = total_pin_count_locked();
    return snapshot;
}

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM05_CACHE_INTEGRITY)

Cnr3CacheIntegrityDiagnosticStats Cnr3OutputCacheCore::cache_integrity_diagnostic_stats_locked() const noexcept {
    Cnr3CacheIntegrityDiagnosticStats snapshot = cache_integrity_diag_stats_;
    const std::size_t checkpoint_count = checkpoint_count_locked();
    const std::size_t checkpoint_retain_headroom =
        checkpoint_count <= CNR3_CACHE_CHECKPOINT_MAX_RETAIN
        ? CNR3_CACHE_CHECKPOINT_MAX_RETAIN - checkpoint_count
        : 0U;

    cnr3_cache_integrity_diagnostic_set_summary_sample(
        snapshot,
        slot_count_locked(),
        checkpoint_count,
        checkpoint_retain_headroom,
        total_pin_count_locked()
    );
    return snapshot;
}

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE)

Cnr3CacheStoreDiagnosticStats Cnr3OutputCacheCore::cache_store_diagnostic_stats_locked() const noexcept {
    return cache_store_diag_stats_;
}

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION)

Cnr3CachePruneDiagnosticStats Cnr3OutputCacheCore::prune_diagnostic_stats_locked() const {
    return prune_diag_stats_;
}

#endif

bool Cnr3OutputCacheCore::frame_is_inside_hot_zone_locked(
    int frame_number
) const noexcept {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return false;
    }

    for (const Cnr3CacheHotZone& hot_zone : hot_zones_) {
        if (!hot_zone.is_active) {
            continue;
        }

        if (
            frame_number >= hot_zone.low_frame &&
            frame_number <= hot_zone.high_frame
            ) {
            return true;
        }
    }

    return false;
}

int Cnr3OutputCacheCore::nearest_active_hot_zone_boundary_distance_locked(
    int frame_number
) const noexcept {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return 0;
    }

    int nearest_distance = std::numeric_limits<int>::max();

    for (const Cnr3CacheHotZone& hot_zone : hot_zones_) {
        if (!hot_zone.is_active) {
            continue;
        }

        const int distance =
            cnr3_frame_distance_to_hot_zone_boundary(frame_number, hot_zone);

        if (distance < nearest_distance) {
            nearest_distance = distance;
        }
    }

    return nearest_distance;
}

bool Cnr3OutputCacheCore::hot_zone_has_pinned_frame_in_range_locked(
    const Cnr3CacheHotZone& hot_zone
) const noexcept {
    if (!cnr3_cache_hot_zone_is_valid(hot_zone) || !hot_zone.is_active) {
        return false;
    }

    for (const Cnr3CacheSlot& slot : slots_) {
        if (!cnr3_cache_slot_has_frame(slot)) {
            continue;
        }

        if (slot.pin_count <= 0) {
            continue;
        }

        if (
            slot.frame_number >= hot_zone.low_frame &&
            slot.frame_number <= hot_zone.high_frame
            ) {
            return true;
        }
    }

    return false;
}

int Cnr3OutputCacheCore::total_pin_count_locked() const noexcept {
    int total_pin_count = 0;

    for (const Cnr3CacheSlot& slot : slots_) {
        if (slot.pin_count < 0) {
            return -1;
        }

        if (slot.pin_count > (std::numeric_limits<int>::max() - total_pin_count)) {
            return -1;
        }

        total_pin_count += slot.pin_count;
    }

    return total_pin_count;
}

#if defined(CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE)

void Cnr3OutputCacheCore::observe_pin_acquired_locked() noexcept {
    cnr3_cache_ownership_diagnostic_observe_pin_acquired(ownership_diag_stats_);
}

void Cnr3OutputCacheCore::observe_pin_released_locked() noexcept {
    cnr3_cache_ownership_diagnostic_observe_pin_released(ownership_diag_stats_);
}

void Cnr3OutputCacheCore::observe_cache_lookup_query_locked() const noexcept {
    cnr3_cache_ownership_diagnostic_observe_cache_lookup_query(ownership_diag_stats_);
}

void Cnr3OutputCacheCore::observe_cache_lookup_hit_locked() const noexcept {
    cnr3_cache_ownership_diagnostic_observe_cache_lookup_hit(ownership_diag_stats_);
}

void Cnr3OutputCacheCore::observe_lookup_ref_acquired_locked() const noexcept {
    cnr3_cache_ownership_diagnostic_observe_lookup_ref_acquired(ownership_diag_stats_);
}

void Cnr3OutputCacheCore::observe_lookup_ref_released_by_cache_core() const noexcept {
    const std::lock_guard<std::mutex> lock(cache_mutex_);

    cnr3_cache_ownership_diagnostic_observe_lookup_ref_released_by_cache_core(ownership_diag_stats_);
}

void Cnr3OutputCacheCore::observe_lookup_ref_transferred() const noexcept {
    const std::lock_guard<std::mutex> lock(cache_mutex_);

    cnr3_cache_ownership_diagnostic_observe_lookup_ref_transferred(ownership_diag_stats_);
}

void Cnr3OutputCacheCore::observe_ownership_error() const noexcept {
    const std::lock_guard<std::mutex> lock(cache_mutex_);

    cnr3_cache_ownership_diagnostic_observe_ownership_error(ownership_diag_stats_);
}

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM05_CACHE_INTEGRITY)

void Cnr3OutputCacheCore::observe_cache_invariant_check_started_locked() const noexcept {
    const std::size_t checkpoint_count = checkpoint_count_locked();
    const std::size_t checkpoint_retain_headroom =
        checkpoint_count <= CNR3_CACHE_CHECKPOINT_MAX_RETAIN
        ? CNR3_CACHE_CHECKPOINT_MAX_RETAIN - checkpoint_count
        : 0U;

    cnr3_cache_integrity_diagnostic_observe_check(
        cache_integrity_diag_stats_,
        slot_count_locked(),
        checkpoint_count,
        checkpoint_retain_headroom,
        total_pin_count_locked()
    );
}

bool Cnr3OutputCacheCore::observe_cache_invariant_failure_locked(
    const char* site
) const noexcept {
    cnr3_cache_integrity_diagnostic_observe_failure(
        cache_integrity_diag_stats_,
        site
    );

    return false;
}

#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM08_CACHE_STORE)

void Cnr3OutputCacheCore::observe_store_outcome_locked(
    const Cnr3CombinedStoreAndPruneSummary& summary
) noexcept {
    std::size_t store_kind_index = CNR3_CACHE_DIAG_DSUM08_STORE_KIND_COUNT;

    switch (summary.store_kind) {
    case Cnr3CacheStoreKind::ProductionCheckpoint:
        store_kind_index = 0U;
        break;
    case Cnr3CacheStoreKind::ProductionNonCheckpoint:
        store_kind_index = 1U;
        break;
    case Cnr3CacheStoreKind::As2ConsumerCheckpoint:
        store_kind_index = 2U;
        break;
    case Cnr3CacheStoreKind::As2ConsumerNonCheckpoint:
        store_kind_index = 3U;
        break;
    case Cnr3CacheStoreKind::Invalid:
        break;
    }

    const bool duplicate_seen =
        summary.store_status == Cnr3Status::duplicate ||
        summary.as2_summary.duplicate_existing_slot;
    const bool incoming_rejected =
        summary.as2_summary.incoming_frame_rejected ||
        summary.store_status == Cnr3Status::duplicate;
    const bool store_failed =
        summary.store_status != Cnr3Status::ok &&
        summary.store_status != Cnr3Status::duplicate;

    cnr3_cache_store_diagnostic_observe_store(
        cache_store_diag_stats_,
        store_kind_index,
        duplicate_seen,
        incoming_rejected,
        summary.as2_summary.checkpoint_promoted,
        store_failed
    );
}

#endif

void Cnr3OutputCacheCore::observe_hot_zone_create_locked() noexcept {
#if defined(CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE)
    cnr3_cache_hot_zone_diagnostic_observe_create(hot_zone_diag_stats_);
    observe_hot_zone_state_sample_locked();
#endif
}

void Cnr3OutputCacheCore::observe_hot_zone_slide_locked() noexcept {
#if defined(CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE)
    cnr3_cache_hot_zone_diagnostic_observe_slide(hot_zone_diag_stats_);
    observe_hot_zone_state_sample_locked();
#endif
}

void Cnr3OutputCacheCore::observe_hot_zone_merge_locked() noexcept {
#if defined(CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE)
    cnr3_cache_hot_zone_diagnostic_observe_merge(hot_zone_diag_stats_);
    observe_hot_zone_state_sample_locked();
#endif
}

void Cnr3OutputCacheCore::observe_hot_zone_decay_locked() noexcept {
#if defined(CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE)
    cnr3_cache_hot_zone_diagnostic_observe_decay(hot_zone_diag_stats_);
    observe_hot_zone_state_sample_locked();
#endif
}

void Cnr3OutputCacheCore::observe_hot_zone_expiry_locked() noexcept {
#if defined(CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE)
    cnr3_cache_hot_zone_diagnostic_observe_expiry(hot_zone_diag_stats_);
    observe_hot_zone_state_sample_locked();
#endif
}

void Cnr3OutputCacheCore::observe_hot_zone_state_sample_locked() noexcept {
#if defined(CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE)
    cnr3_cache_hot_zone_diagnostic_observe_zone_count_sample(
        hot_zone_diag_stats_,
        hot_zones_.size()
    );

    for (const Cnr3CacheHotZone& hot_zone : hot_zones_) {
        if (!hot_zone.is_active) {
            continue;
        }

        if (hot_zone.high_frame < hot_zone.low_frame) {
            continue;
        }

        cnr3_cache_hot_zone_diagnostic_observe_protected_range_sample(
            hot_zone_diag_stats_,
            hot_zone.high_frame - hot_zone.low_frame + 1
        );
    }
#endif
}

void Cnr3OutputCacheCore::observe_hot_zone_prune_rejections_locked(
    std::size_t rejected_frame_count
) noexcept {
#if defined(CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE)
    cnr3_cache_hot_zone_diagnostic_observe_prune_rejections(
        hot_zone_diag_stats_,
        static_cast<std::uint64_t>(rejected_frame_count)
    );
#else
    (void)rejected_frame_count;
#endif
}



#if defined(CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION)

void Cnr3OutputCacheCore::observe_prune_execution_locked(
    std::uint64_t frame_byte_count,
    const std::vector<Cnr3PruneCandidateDistanceOrderEntry>& candidate_order,
    const std::vector<int>& selected_frame_numbers,
    const Cnr3CachePruneExecutionSummary& prune_summary,
    std::size_t hot_zone_prune_rejection_count
) noexcept {
    cnr3_cache_diag_saturating_increment(prune_diag_stats_.prune_invocations);

    if (
        prune_summary.trigger_decision.prune_is_required ||
        prune_summary.trigger_decision.checkpoint_prune_is_required
        ) {
        cnr3_cache_diag_saturating_increment(prune_diag_stats_.prune_events_triggered);
    }


    cnr3_cache_diag_saturating_add(
        prune_diag_stats_.hot_zone_rejected,
        static_cast<std::uint64_t>(hot_zone_prune_rejection_count)
    );

    if (prune_summary.detached_count == 0U) {
        return;
    }

    cnr3_cache_diag_saturating_add(
        prune_diag_stats_.frames_evicted,
        static_cast<std::uint64_t>(prune_summary.detached_count)
    );

    if (frame_byte_count != 0U) {
        std::uint64_t byte_increment = UINT64_MAX;
        if (
            prune_summary.detached_count <=
            (UINT64_MAX / frame_byte_count)
            ) {
            byte_increment =
                frame_byte_count *
                static_cast<std::uint64_t>(prune_summary.detached_count);
        }

        cnr3_cache_diag_saturating_add(
            prune_diag_stats_.bytes_evicted,
            byte_increment
        );
    }

    const std::size_t evicted_count = std::min(
        prune_summary.detached_count,
        selected_frame_numbers.size()
    );

    for (std::size_t selected_index = 0U;
        selected_index < evicted_count;
        ++selected_index) {
        const int evicted_frame = selected_frame_numbers[selected_index];

        for (const Cnr3PruneCandidateDistanceOrderEntry& candidate : candidate_order) {
            if (candidate.frame_number != evicted_frame) {
                continue;
            }

            if (candidate.is_checkpoint) {
                cnr3_cache_diag_saturating_increment(prune_diag_stats_.checkpoint_prunes);
            }

            break;
        }

        if (
            prune_diag_stats_.ring_capacity == 0U ||
            prune_diag_stats_.recently_evicted_ring.empty()
            ) {
            continue;
        }

        if (prune_diag_stats_.ring_live_count >= prune_diag_stats_.ring_capacity) {
            cnr3_cache_diag_saturating_increment(prune_diag_stats_.ring_wrap_count);
            prune_diag_stats_.ring_saturated = true;
        }

        cnr3_cache_diag_saturating_increment(prune_diag_stats_.total_evicted_records);

        prune_diag_stats_.recently_evicted_ring[prune_diag_stats_.ring_head] =
            Cnr3CachePruneDiagnosticRingEntry{
                evicted_frame,
                prune_diag_stats_.total_evicted_records
            };

        prune_diag_stats_.ring_head =
            (prune_diag_stats_.ring_head + 1U) % prune_diag_stats_.ring_capacity;

        if (prune_diag_stats_.ring_live_count < prune_diag_stats_.ring_capacity) {
            ++prune_diag_stats_.ring_live_count;
        }

#if defined(CNR3_DIAG_DSUM10_RING_WINDOW_DUMP)
        if (
            CNR3_DIAG_DSUM10_RING_WINDOW_INTERVAL != 0 &&
            prune_diag_stats_.total_evicted_records %
                static_cast<std::uint64_t>(CNR3_DIAG_DSUM10_RING_WINDOW_INTERVAL) == 0U &&
            prune_diag_stats_.window_dumps_emitted <
                static_cast<std::uint64_t>(CNR3_DIAG_DSUM10_RING_WINDOW_MAX_DUMPS)
            ) {
            const std::size_t dump_size = std::min(
                static_cast<std::size_t>(CNR3_DIAG_DSUM10_RING_WINDOW_SIZE),
                prune_diag_stats_.ring_live_count
            );
            const std::size_t dump_start =
                prune_diag_stats_.ring_live_count - dump_size;
            const std::size_t oldest_index =
                (prune_diag_stats_.ring_live_count < prune_diag_stats_.ring_capacity)
                ? 0U
                : prune_diag_stats_.ring_head;

            for (std::size_t dump_index = 0U; dump_index < dump_size; ++dump_index) {
                const std::size_t source_index =
                    (oldest_index + dump_start + dump_index) %
                    prune_diag_stats_.ring_capacity;
                const std::size_t target_index =
                    prune_diag_stats_.ring_window_dump_entry_count + dump_index;

                if (target_index >= prune_diag_stats_.ring_window_dump_entries.size()) {
                    break;
                }

                prune_diag_stats_.ring_window_dump_entries[target_index] =
                    prune_diag_stats_.recently_evicted_ring[source_index].frame_number;
            }

            prune_diag_stats_.ring_window_dump_entry_count = std::min(
                prune_diag_stats_.ring_window_dump_entry_count + dump_size,
                prune_diag_stats_.ring_window_dump_entries.size()
            );
            cnr3_cache_diag_saturating_increment(prune_diag_stats_.window_dumps_emitted);
        }
#endif

#if defined(CNR3_DIAG_DSUM10_RING_FULL_DUMP)
        if (
            prune_diag_stats_.ring_saturated &&
            prune_diag_stats_.full_dumps_emitted <
                static_cast<std::uint64_t>(CNR3_DIAG_DSUM10_RING_FULL_MAX_DUMPS)
            ) {
            const std::size_t oldest_index = prune_diag_stats_.ring_head;
            const std::size_t target_base =
                static_cast<std::size_t>(prune_diag_stats_.full_dumps_emitted) *
                prune_diag_stats_.ring_capacity;

            for (std::size_t dump_index = 0U;
                dump_index < prune_diag_stats_.ring_capacity;
                ++dump_index) {
                const std::size_t target_index = target_base + dump_index;

                if (target_index >= prune_diag_stats_.ring_full_dump_entries.size()) {
                    break;
                }

                const std::size_t source_index =
                    (oldest_index + dump_index) % prune_diag_stats_.ring_capacity;
                prune_diag_stats_.ring_full_dump_entries[target_index] =
                    prune_diag_stats_.recently_evicted_ring[source_index].frame_number;
            }

            prune_diag_stats_.ring_full_dump_entry_count = std::min(
                prune_diag_stats_.ring_full_dump_entry_count + prune_diag_stats_.ring_capacity,
                prune_diag_stats_.ring_full_dump_entries.size()
            );
            cnr3_cache_diag_saturating_increment(prune_diag_stats_.full_dumps_emitted);
        }
#endif
    }
}

void Cnr3OutputCacheCore::observe_lookup_miss_rechurn_locked(
    int frame_number
) const noexcept {
    if (
        !cnr3_frame_number_is_valid(frame_number) ||
        prune_diag_stats_.ring_capacity == 0U ||
        prune_diag_stats_.recently_evicted_ring.empty()
        ) {
        return;
    }

    const std::size_t live_count = std::min(
        prune_diag_stats_.ring_live_count,
        prune_diag_stats_.recently_evicted_ring.size()
    );

    if (live_count == 0U) {
        return;
    }

    const std::size_t oldest_index =
        (prune_diag_stats_.ring_live_count < prune_diag_stats_.ring_capacity)
        ? 0U
        : prune_diag_stats_.ring_head;

    bool found = false;
    std::uint64_t newest_eviction_sequence = 0U;

    for (std::size_t i = 0U; i < live_count; ++i) {
        const std::size_t index =
            (oldest_index + i) % prune_diag_stats_.ring_capacity;
        const Cnr3CachePruneDiagnosticRingEntry& entry =
            prune_diag_stats_.recently_evicted_ring[index];

        if (entry.frame_number != frame_number) {
            continue;
        }

        if (!found || entry.eviction_sequence > newest_eviction_sequence) {
            found = true;
            newest_eviction_sequence = entry.eviction_sequence;
        }
    }

    if (!found) {
        return;
    }

    std::uint64_t eviction_gap =
        prune_diag_stats_.total_evicted_records >= newest_eviction_sequence
        ? prune_diag_stats_.total_evicted_records - newest_eviction_sequence
        : 0U;

    if (eviction_gap == 0U) {
        eviction_gap = 1U;
    }

    if (eviction_gap <= CNR3_PRUNE_RECHURN_MAX_EVICTION_GAP) {
        cnr3_cache_diag_saturating_increment(
            prune_diag_stats_.frames_recently_evicted_then_re_requested
        );
    }

    std::size_t top_index = prune_diag_stats_.top_thrasher_count;
    for (std::size_t i = 0U; i < prune_diag_stats_.top_thrasher_count; ++i) {
        if (prune_diag_stats_.top_thrashers[i].frame_number == frame_number) {
            top_index = i;
            break;
        }
    }

    if (top_index < prune_diag_stats_.top_thrasher_count) {
        if (prune_diag_stats_.top_thrashers[top_index].re_churn_count > 0U) {
            cnr3_cache_diag_saturating_increment(
                prune_diag_stats_.frames_re_requested_repeatedly
            );
        }

        cnr3_cache_diag_saturating_increment(
            prune_diag_stats_.top_thrashers[top_index].re_churn_count
        );
    }
    else if (prune_diag_stats_.top_thrasher_count < prune_diag_stats_.top_thrashers.size()) {
        prune_diag_stats_.top_thrashers[prune_diag_stats_.top_thrasher_count] =
            Cnr3CachePruneDiagnosticTopThrashEntry{ frame_number, 1U };
        ++prune_diag_stats_.top_thrasher_count;
    }
    else {
        std::size_t lowest_index = 0U;
        for (std::size_t i = 1U; i < prune_diag_stats_.top_thrashers.size(); ++i) {
            if (
                prune_diag_stats_.top_thrashers[i].re_churn_count <
                prune_diag_stats_.top_thrashers[lowest_index].re_churn_count
                ) {
                lowest_index = i;
            }
        }

        if (prune_diag_stats_.top_thrashers[lowest_index].re_churn_count <= 1U) {
            prune_diag_stats_.top_thrashers[lowest_index] =
                Cnr3CachePruneDiagnosticTopThrashEntry{ frame_number, 1U };
        }
    }

    std::size_t gap_bin = 6U;
    if (eviction_gap <= 10U) {
        gap_bin = 0U;
    }
    else if (eviction_gap <= 50U) {
        gap_bin = 1U;
    }
    else if (eviction_gap <= 100U) {
        gap_bin = 2U;
    }
    else if (eviction_gap <= 500U) {
        gap_bin = 3U;
    }
    else if (eviction_gap <= 1000U) {
        gap_bin = 4U;
    }
    else if (eviction_gap <= 5000U) {
        gap_bin = 5U;
    }

    cnr3_cache_diag_saturating_increment(
        prune_diag_stats_.gap_histogram[gap_bin]
    );
}

#endif

std::size_t Cnr3OutputCacheCore::count_prune_candidates_rejected_by_hot_zone_locked(
    bool noncheckpoint_capacity_permits,
    std::size_t retain_checkpoint_count
) const noexcept {
    std::size_t rejected_frame_count = 0U;

    const std::size_t checkpoint_count = checkpoint_count_locked();
    const bool checkpoint_retention_permits =
        checkpoint_count > retain_checkpoint_count;

    for (const Cnr3CacheSlot& slot : slots_) {
        if (!cnr3_cache_slot_is_indexable(slot)) {
            continue;
        }

        if (slot.pin_count != 0) {
            continue;
        }

        if (!frame_is_inside_hot_zone_locked(slot.frame_number)) {
            continue;
        }

        if (slot.is_checkpoint) {
            if (slot.frame_number == 0 || !checkpoint_retention_permits) {
                continue;
            }

            ++rejected_frame_count;
            continue;
        }

        if (noncheckpoint_capacity_permits) {
            ++rejected_frame_count;
        }
    }

    return rejected_frame_count;
}

Cnr3Status Cnr3OutputCacheCore::merge_closest_active_hot_zones_locked() {
    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    if (hot_zones_.size() < 2U) {
        return Cnr3Status::invariant_violation;
    }

    std::size_t first_merge_position = hot_zones_.size();
    std::size_t second_merge_position = hot_zones_.size();
    int best_distance = std::numeric_limits<int>::max();

    for (std::size_t first_position = 0U;
        first_position < hot_zones_.size();
        ++first_position
        ) {
        const Cnr3CacheHotZone& first_zone = hot_zones_[first_position];

        if (!first_zone.is_active) {
            continue;
        }

        for (std::size_t second_position = first_position + 1U;
            second_position < hot_zones_.size();
            ++second_position
            ) {
            const Cnr3CacheHotZone& second_zone = hot_zones_[second_position];

            if (!second_zone.is_active) {
                continue;
            }

            int distance = 0;

            if (first_zone.high_frame < second_zone.low_frame) {
                distance = second_zone.low_frame - first_zone.high_frame;
            }
            else if (second_zone.high_frame < first_zone.low_frame) {
                distance = first_zone.low_frame - second_zone.high_frame;
            }

            if (distance < best_distance) {
                best_distance = distance;
                first_merge_position = first_position;
                second_merge_position = second_position;
            }
        }
    }

    if (
        first_merge_position >= hot_zones_.size() ||
        second_merge_position >= hot_zones_.size()
        ) {
        return Cnr3Status::invariant_violation;
    }

    Cnr3CacheHotZone& first_zone = hot_zones_[first_merge_position];
    const Cnr3CacheHotZone& second_zone = hot_zones_[second_merge_position];

    if (second_zone.low_frame < first_zone.low_frame) {
        first_zone.low_frame = second_zone.low_frame;
    }

    if (second_zone.high_frame > first_zone.high_frame) {
        first_zone.high_frame = second_zone.high_frame;
    }

    if (second_zone.last_observed_frame > first_zone.last_observed_frame) {
        first_zone.last_observed_frame = second_zone.last_observed_frame;
    }

    hot_zones_.erase(hot_zones_.begin() + second_merge_position);
    observe_hot_zone_merge_locked();

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

Cnr3Status Cnr3OutputCacheCore::record_hot_zone_observation_locked(
    int frame_number
) {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return Cnr3Status::invalid_argument;
    }

    if (frame_number >
        (std::numeric_limits<int>::max() - CNR3_CACHE_HOT_ZONE_FORWARD_RADIUS)
        ) {
        return Cnr3Status::capacity_exceeded;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    const int observed_low_frame =
        (frame_number > CNR3_CACHE_HOT_ZONE_BACK_RADIUS) ?
        (frame_number - CNR3_CACHE_HOT_ZONE_BACK_RADIUS) :
        0;
    const int observed_high_frame =
        frame_number + CNR3_CACHE_HOT_ZONE_FORWARD_RADIUS;

    std::size_t best_zone_position = hot_zones_.size();
    int best_distance = std::numeric_limits<int>::max();

    for (std::size_t zone_position = 0U;
        zone_position < hot_zones_.size();
        ++zone_position
        ) {
        const Cnr3CacheHotZone& hot_zone = hot_zones_[zone_position];

        if (!hot_zone.is_active) {
            continue;
        }

        int distance = 0;

        if (frame_number < hot_zone.low_frame) {
            distance = hot_zone.low_frame - frame_number;
        }
        else if (frame_number > hot_zone.high_frame) {
            distance = frame_number - hot_zone.high_frame;
        }

        if (
            distance <= CNR3_CACHE_JUMP_THRESHOLD &&
            distance < best_distance
            ) {
            best_distance = distance;
            best_zone_position = zone_position;
        }
    }

    if (best_zone_position < hot_zones_.size()) {
        Cnr3CacheHotZone& hot_zone = hot_zones_[best_zone_position];

        hot_zone.is_active = true;
        hot_zone.low_frame = observed_low_frame;
        hot_zone.high_frame = observed_high_frame;
        hot_zone.last_observed_frame = frame_number;
        observe_hot_zone_slide_locked();
    }
    else {
        if (hot_zones_.size() >= CNR3_CACHE_MAX_HOT_ZONES) {
            const Cnr3Status merge_status = merge_closest_active_hot_zones_locked();

            if (!cnr3_status_is_ok(merge_status)) {
                return merge_status;
            }
        }

        if (hot_zones_.size() >= CNR3_CACHE_MAX_HOT_ZONES) {
            return Cnr3Status::capacity_exceeded;
        }

        Cnr3CacheHotZone hot_zone{};
        hot_zone.is_active = true;
        hot_zone.low_frame = observed_low_frame;
        hot_zone.high_frame = observed_high_frame;
        hot_zone.last_observed_frame = frame_number;

        try {
            hot_zones_.push_back(hot_zone);
        }
        catch (const std::bad_alloc&) {
            return Cnr3Status::allocation_failed;
        }

        observe_hot_zone_create_locked();
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

Cnr3Status Cnr3OutputCacheCore::retire_decay_eligible_hot_zones_locked(
    int current_frame
) {
    if (!cnr3_frame_number_is_valid(current_frame)) {
        return Cnr3Status::invalid_argument;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    for (std::size_t zone_position = 0U; zone_position < hot_zones_.size();) {
        const Cnr3CacheHotZone& hot_zone = hot_zones_[zone_position];

        if (!hot_zone.is_active) {
            hot_zones_.erase(hot_zones_.begin() + zone_position);
            continue;
        }

        const bool decay_margin_elapsed =
            current_frame >= hot_zone.last_observed_frame &&
            (current_frame - hot_zone.last_observed_frame) >=
            CNR3_CACHE_HOT_ZONE_DECAY_MARGIN;

        if (decay_margin_elapsed) {
            observe_hot_zone_decay_locked();
        }

        if (
            decay_margin_elapsed &&
            !hot_zone_has_pinned_frame_in_range_locked(hot_zone)
            ) {
            hot_zones_.erase(hot_zones_.begin() + zone_position);
            observe_hot_zone_expiry_locked();
            continue;
        }

        ++zone_position;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

Cnr3Status Cnr3OutputCacheCore::store_noncheckpoint_owned_frame_locked(
    int frame_number,
    Cnr3OwnedFrameRef& frame
) {
    return store_owned_frame_locked(frame_number, frame, false);
}

Cnr3Status Cnr3OutputCacheCore::store_checkpoint_owned_frame_locked(
    int frame_number,
    Cnr3OwnedFrameRef& frame
) {
    return store_owned_frame_locked(frame_number, frame, true);
}

Cnr3Status Cnr3OutputCacheCore::store_owned_frame_locked(
    int frame_number,
    Cnr3OwnedFrameRef& frame,
    bool is_checkpoint
) {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return Cnr3Status::invalid_argument;
    }

    if (!frame.has_frame()) {
        return Cnr3Status::invalid_argument;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    const auto existing_frame_index_it = frame_index_.find(frame_number);

    if (existing_frame_index_it != frame_index_.end()) {
        const std::size_t existing_slot_position = existing_frame_index_it->second;

        if (existing_slot_position >= slots_.size()) {
            return Cnr3Status::invariant_violation;
        }

        Cnr3CacheSlot& existing_slot = slots_[existing_slot_position];

        if (!cnr3_cache_slot_is_indexable(existing_slot)) {
            return Cnr3Status::invariant_violation;
        }

        if (existing_slot.frame_number != frame_number) {
            return Cnr3Status::invariant_violation;
        }

        if (is_checkpoint && !existing_slot.is_checkpoint) {
            if (
                checkpoint_slot_positions_.size() >=
                checkpoint_slot_positions_.max_size()
                ) {
                return Cnr3Status::capacity_exceeded;
            }

            try {
                checkpoint_slot_positions_.reserve(
                    checkpoint_slot_positions_.size() + 1U
                );
            }
            catch (const std::bad_alloc&) {
                return Cnr3Status::allocation_failed;
            }

            existing_slot.is_checkpoint = true;
            checkpoint_slot_positions_.push_back(existing_slot_position);

            if (!cache_state_invariants_hold_locked()) {
                return Cnr3Status::invariant_violation;
            }
        }

        return Cnr3Status::duplicate;
    }

    const std::size_t slot_position = slots_.size();

    if (slot_position >= slots_.max_size()) {
        return Cnr3Status::capacity_exceeded;
    }

    if (
        is_checkpoint &&
        checkpoint_slot_positions_.size() >= checkpoint_slot_positions_.max_size()
        ) {
        return Cnr3Status::capacity_exceeded;
    }

    try {
        slots_.reserve(slot_position + 1U);

        if (is_checkpoint) {
            checkpoint_slot_positions_.reserve(
                checkpoint_slot_positions_.size() + 1U
            );
        }

        const auto insert_result = frame_index_.emplace(frame_number, slot_position);

        if (!insert_result.second) {
            return Cnr3Status::duplicate;
        }
    }
    catch (const std::bad_alloc&) {
        frame_index_.erase(frame_number);

        return Cnr3Status::allocation_failed;
    }

    Cnr3CacheSlot slot{};
    slot.slot_id = slot_id_source_.allocate();
    slot.frame_number = frame_number;
    slot.frame = std::move(frame);
    slot.is_checkpoint = is_checkpoint;
    slot.pin_count = 0;

    slots_.push_back(std::move(slot));

    if (is_checkpoint) {
        checkpoint_slot_positions_.push_back(slot_position);
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

Cnr3Status Cnr3OutputCacheCore::store_owned_frame_and_record_pin_locked(
    int frame_number,
    Cnr3OwnedFrameRef& frame,
    bool is_checkpoint,
    Cnr3CachePinList& pin_list,
    Cnr3CacheAs2StoreRecordSummary& out_summary
) {
    out_summary = Cnr3CacheAs2StoreRecordSummary{};

    if (!cnr3_frame_number_is_valid(frame_number)) {
        return Cnr3Status::invalid_argument;
    }

    if (!frame.has_frame()) {
        return Cnr3Status::invalid_argument;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    const auto existing_frame_index_it = frame_index_.find(frame_number);
    const bool existing_slot_found =
        existing_frame_index_it != frame_index_.end();
    bool existing_slot_was_checkpoint = false;

    if (existing_slot_found) {
        const std::size_t existing_slot_position = existing_frame_index_it->second;

        if (existing_slot_position >= slots_.size()) {
            return Cnr3Status::invariant_violation;
        }

        const Cnr3CacheSlot& existing_slot = slots_[existing_slot_position];

        if (!cnr3_cache_slot_is_indexable(existing_slot)) {
            return Cnr3Status::invariant_violation;
        }

        if (existing_slot.frame_number != frame_number) {
            return Cnr3Status::invariant_violation;
        }

        existing_slot_was_checkpoint = existing_slot.is_checkpoint;
    }

    const Cnr3Status store_status =
        store_owned_frame_locked(frame_number, frame, is_checkpoint);

    if (store_status == Cnr3Status::ok) {
        out_summary.inserted_new_slot = true;
        out_summary.incoming_frame_consumed = true;
    }
    else if (store_status == Cnr3Status::duplicate) {
        out_summary.duplicate_existing_slot = true;
        out_summary.incoming_frame_rejected = true;
        out_summary.checkpoint_promoted =
            is_checkpoint && !existing_slot_was_checkpoint;
    }
    else {
        return store_status;
    }

    const auto frame_index_it = frame_index_.find(frame_number);

    if (frame_index_it == frame_index_.end()) {
        return Cnr3Status::invariant_violation;
    }

    if (frame_index_it->second >= slots_.size()) {
        return Cnr3Status::invariant_violation;
    }

    const Cnr3CacheSlot& stored_slot = slots_[frame_index_it->second];

    if (!cnr3_cache_slot_is_indexable(stored_slot)) {
        return Cnr3Status::invariant_violation;
    }

    if (stored_slot.frame_number != frame_number) {
        return Cnr3Status::invariant_violation;
    }

    /*
        In the duplicate case the incoming frame is rejected, but the
        first-in-best-dressed winner slot for frame_number exists. AS2 still
        pins and records that winner because the caller asked to store-and-pin
        frame N, and frame N is present. Every AS2 call yields one recorded pin
        to discharge, including duplicate stores.
    */
    Cnr3CacheSlotPinToken pin_token{};
    const Cnr3Status pin_status = pin_frame_locked(frame_number, pin_token);

    if (!cnr3_status_is_ok(pin_status)) {
        return pin_status;
    }

    const Cnr3Status record_status =
        pin_list.record_pin_without_allocation(pin_token);

    if (!cnr3_status_is_ok(record_status)) {
        const Cnr3Status unpin_status = unpin_frame_locked(pin_token);

        if (!cnr3_status_is_ok(unpin_status)) {
            return unpin_status;
        }

        return record_status;
    }

    const auto final_frame_index_it = frame_index_.find(frame_number);

    if (final_frame_index_it == frame_index_.end()) {
        return Cnr3Status::invariant_violation;
    }

    if (final_frame_index_it->second >= slots_.size()) {
        return Cnr3Status::invariant_violation;
    }

    const Cnr3CacheSlot& final_slot = slots_[final_frame_index_it->second];

    if (!cnr3_cache_slot_is_indexable(final_slot)) {
        return Cnr3Status::invariant_violation;
    }

    out_summary.frame_number = frame_number;
    out_summary.requested_checkpoint = is_checkpoint;
    out_summary.resulting_slot_is_checkpoint = final_slot.is_checkpoint;
    out_summary.pin_recorded = true;

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

Cnr3Status Cnr3OutputCacheCore::remove_unpinned_frame_locked(
    int frame_number,
    Cnr3CacheSlot& detached_slot
) {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return Cnr3Status::invalid_argument;
    }

    if (
        cnr3_cache_slot_has_frame(detached_slot) ||
        cnr3_cache_slot_id_is_valid(detached_slot.slot_id) ||
        cnr3_frame_number_is_valid(detached_slot.frame_number) ||
        detached_slot.is_checkpoint ||
        detached_slot.pin_count != 0
        ) {
        return Cnr3Status::invalid_argument;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    const auto frame_index_it = frame_index_.find(frame_number);

    if (frame_index_it == frame_index_.end()) {
        return Cnr3Status::not_found;
    }

    const std::size_t slot_position = frame_index_it->second;

    if (slot_position >= slots_.size()) {
        return Cnr3Status::invariant_violation;
    }

    Cnr3CacheSlot& slot = slots_[slot_position];

    if (!cnr3_cache_slot_is_indexable(slot)) {
        return Cnr3Status::invariant_violation;
    }

    if (slot.frame_number != frame_number) {
        return Cnr3Status::invariant_violation;
    }

    if (slot.pin_count != 0) {
        return Cnr3Status::lifecycle_violation;
    }

    const bool removed_checkpoint = slot.is_checkpoint;
    const std::size_t last_slot_position = slots_.size() - 1U;

    bool removed_checkpoint_position = false;

    if (removed_checkpoint) {
        for (std::size_t checkpoint_index = 0;
            checkpoint_index < checkpoint_slot_positions_.size();
            ++checkpoint_index
            ) {
            if (checkpoint_slot_positions_[checkpoint_index] == slot_position) {
                checkpoint_slot_positions_[checkpoint_index] =
                    checkpoint_slot_positions_.back();
                checkpoint_slot_positions_.pop_back();
                removed_checkpoint_position = true;
                break;
            }
        }

        if (!removed_checkpoint_position) {
            return Cnr3Status::invariant_violation;
        }
    }

    frame_index_.erase(frame_index_it);
    detached_slot = std::move(slots_[slot_position]);

    if (slot_position != last_slot_position) {
        Cnr3CacheSlot& moved_slot = slots_[last_slot_position];

        if (!cnr3_cache_slot_is_indexable(moved_slot)) {
            return Cnr3Status::invariant_violation;
        }

        const int moved_frame_number = moved_slot.frame_number;
        auto moved_frame_index_it = frame_index_.find(moved_frame_number);

        if (moved_frame_index_it == frame_index_.end()) {
            return Cnr3Status::invariant_violation;
        }

        slots_[slot_position] = std::move(moved_slot);
        moved_frame_index_it->second = slot_position;

        for (std::size_t& checkpoint_slot_position : checkpoint_slot_positions_) {
            if (checkpoint_slot_position == last_slot_position) {
                checkpoint_slot_position = slot_position;
            }
        }
    }

    slots_.pop_back();

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

Cnr3Status Cnr3OutputCacheCore::remove_selected_unpinned_frames_bounded_locked(
    const std::vector<int>& candidate_frame_numbers,
    std::size_t max_remove_count,
    std::vector<Cnr3CacheSlot>& detached_slots,
    std::size_t& out_removed_count
) {
    out_removed_count = 0U;

    if (max_remove_count == 0U || candidate_frame_numbers.empty()) {
        return Cnr3Status::ok;
    }

    if (detached_slots.capacity() < max_remove_count) {
        return Cnr3Status::invalid_argument;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    for (const int frame_number : candidate_frame_numbers) {
        if (out_removed_count >= max_remove_count) {
            break;
        }

        if (!cnr3_frame_number_is_valid(frame_number)) {
            return Cnr3Status::invalid_argument;
        }

        Cnr3CacheSlot detached_slot{};
        const Cnr3Status remove_status =
            remove_unpinned_frame_locked(frame_number, detached_slot);

        if (!cnr3_status_is_ok(remove_status)) {
            return remove_status;
        }

        detached_slots.push_back(std::move(detached_slot));
        ++out_removed_count;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

Cnr3Status Cnr3OutputCacheCore::remove_unpinned_noncheckpoint_frames_bounded_locked(
    std::size_t max_remove_count,
    std::vector<Cnr3CacheSlot>& detached_slots,
    std::size_t& out_removed_count
) {
    out_removed_count = 0U;

    if (max_remove_count == 0U) {
        return Cnr3Status::ok;
    }

    if (detached_slots.capacity() < max_remove_count) {
        return Cnr3Status::invalid_argument;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    std::size_t slot_position = 0U;

    while (slot_position < slots_.size() && out_removed_count < max_remove_count) {
        const Cnr3CacheSlot& slot = slots_[slot_position];

        if (!cnr3_cache_slot_is_indexable(slot)) {
            return Cnr3Status::invariant_violation;
        }

        if (slot.pin_count != 0 || slot.is_checkpoint) {
            ++slot_position;
            continue;
        }

        const int frame_number = slot.frame_number;
        Cnr3CacheSlot detached_slot{};
        const Cnr3Status remove_status =
            remove_unpinned_frame_locked(frame_number, detached_slot);

        if (!cnr3_status_is_ok(remove_status)) {
            return remove_status;
        }

        detached_slots.push_back(std::move(detached_slot));
        ++out_removed_count;

        /*
            Do not increment slot_position here. The central remove helper may
            move the previous last slot into this position, and that moved slot
            still needs to be evaluated by this bounded selection pass.
        */
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

Cnr3Status Cnr3OutputCacheCore::remove_unpinned_noncheckpoint_frames_outside_hot_zones_bounded_locked(
    std::size_t max_remove_count,
    std::vector<Cnr3CacheSlot>& detached_slots,
    std::size_t& out_removed_count
) {
    out_removed_count = 0U;

    if (max_remove_count == 0U) {
        return Cnr3Status::ok;
    }

    if (detached_slots.capacity() < max_remove_count) {
        return Cnr3Status::invalid_argument;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    std::size_t slot_position = 0U;

    while (slot_position < slots_.size() && out_removed_count < max_remove_count) {
        const Cnr3CacheSlot& slot = slots_[slot_position];

        if (!cnr3_cache_slot_is_indexable(slot)) {
            return Cnr3Status::invariant_violation;
        }

        if (
            slot.pin_count != 0 ||
            slot.is_checkpoint ||
            frame_is_inside_hot_zone_locked(slot.frame_number)
            ) {
            ++slot_position;
            continue;
        }

        const int frame_number = slot.frame_number;
        Cnr3CacheSlot detached_slot{};
        const Cnr3Status remove_status =
            remove_unpinned_frame_locked(frame_number, detached_slot);

        if (!cnr3_status_is_ok(remove_status)) {
            return remove_status;
        }

        detached_slots.push_back(std::move(detached_slot));
        ++out_removed_count;

        /*
            Do not increment slot_position here. The central remove helper may
            move the previous last slot into this position, and that moved slot
            still needs to be evaluated by this bounded selection pass.
        */
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

Cnr3Status Cnr3OutputCacheCore::select_unpinned_noncheckpoint_frames_outside_hot_zones_by_distance_bounded_locked(
    std::size_t max_select_count,
    std::vector<Cnr3PruneCandidateDistanceOrderEntry>& out_candidate_order
) const {
    out_candidate_order.clear();

    if (max_select_count == 0U) {
        return Cnr3Status::ok;
    }

    if (out_candidate_order.capacity() < max_select_count) {
        return Cnr3Status::invalid_argument;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    for (const Cnr3CacheSlot& slot : slots_) {
        if (!cnr3_cache_slot_is_indexable(slot)) {
            return Cnr3Status::invariant_violation;
        }

        if (
            slot.pin_count != 0 ||
            slot.is_checkpoint ||
            frame_is_inside_hot_zone_locked(slot.frame_number)
            ) {
            continue;
        }

        const Cnr3PruneCandidateDistanceOrderEntry candidate{
            slot.frame_number,
            nearest_active_hot_zone_boundary_distance_locked(slot.frame_number)
        };

        if (!cnr3_prune_candidate_distance_order_entry_is_valid(candidate)) {
            return Cnr3Status::invariant_violation;
        }

        if (out_candidate_order.size() < max_select_count) {
            out_candidate_order.push_back(candidate);
            continue;
        }

        auto worst_selected = std::max_element(
            out_candidate_order.begin(),
            out_candidate_order.end(),
            cnr3_prune_candidate_distance_order_before
        );

        if (
            worst_selected != out_candidate_order.end() &&
            cnr3_prune_candidate_distance_order_before(candidate, *worst_selected)
            ) {
            *worst_selected = candidate;
        }
    }

    std::sort(
        out_candidate_order.begin(),
        out_candidate_order.end(),
        cnr3_prune_candidate_distance_order_before
    );

    return Cnr3Status::ok;
}


Cnr3Status Cnr3OutputCacheCore::select_composite_prune_candidates_bounded_locked(
    bool noncheckpoint_capacity_permits,
    std::size_t retain_checkpoint_count,
    std::size_t max_select_count,
    std::vector<Cnr3PruneCandidateDistanceOrderEntry>& out_candidate_order,
    std::vector<Cnr3PruneCandidateDistanceOrderEntry>& checkpoint_candidate_order
) const {
    out_candidate_order.clear();
    checkpoint_candidate_order.clear();

    if (max_select_count == 0U) {
        return Cnr3Status::ok;
    }

    if (out_candidate_order.capacity() < max_select_count) {
        return Cnr3Status::invalid_argument;
    }

    if (checkpoint_candidate_order.capacity() < max_select_count) {
        return Cnr3Status::invalid_argument;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    const std::size_t checkpoint_count = checkpoint_count_locked();
    const std::size_t checkpoint_select_budget =
        (checkpoint_count > retain_checkpoint_count)
        ? std::min(checkpoint_count - retain_checkpoint_count, max_select_count)
        : 0U;

    for (const Cnr3CacheSlot& slot : slots_) {
        if (!cnr3_cache_slot_is_indexable(slot)) {
            return Cnr3Status::invariant_violation;
        }

        if (slot.pin_count != 0 || frame_is_inside_hot_zone_locked(slot.frame_number)) {
            continue;
        }

        if (slot.is_checkpoint) {
            if (slot.frame_number == 0 || checkpoint_select_budget == 0U) {
                continue;
            }

            const Cnr3PruneCandidateDistanceOrderEntry candidate{
                slot.frame_number,
                nearest_active_hot_zone_boundary_distance_locked(slot.frame_number),
                true
            };

            const Cnr3Status consider_status = cnr3_consider_prune_candidate_bounded(
                candidate,
                checkpoint_select_budget,
                checkpoint_candidate_order
            );

            if (!cnr3_status_is_ok(consider_status)) {
                return consider_status;
            }

            continue;
        }

        if (!noncheckpoint_capacity_permits) {
            continue;
        }

        const Cnr3PruneCandidateDistanceOrderEntry candidate{
            slot.frame_number,
            nearest_active_hot_zone_boundary_distance_locked(slot.frame_number),
            false
        };

        const Cnr3Status consider_status = cnr3_consider_prune_candidate_bounded(
            candidate,
            max_select_count,
            out_candidate_order
        );

        if (!cnr3_status_is_ok(consider_status)) {
            return consider_status;
        }
    }

    for (const Cnr3PruneCandidateDistanceOrderEntry& checkpoint_candidate : checkpoint_candidate_order) {
        const Cnr3Status consider_status = cnr3_consider_prune_candidate_bounded(
            checkpoint_candidate,
            max_select_count,
            out_candidate_order
        );

        if (!cnr3_status_is_ok(consider_status)) {
            return consider_status;
        }
    }

    std::sort(
        out_candidate_order.begin(),
        out_candidate_order.end(),
        cnr3_prune_candidate_distance_order_before
    );

    return Cnr3Status::ok;
}


Cnr3Status Cnr3OutputCacheCore::execute_bounded_prune_pass_locked(
    std::uint64_t frame_byte_count,
    std::size_t retain_checkpoint_count,
    std::size_t max_remove_count,
    std::vector<Cnr3PruneCandidateDistanceOrderEntry>& candidate_order,
    std::vector<Cnr3PruneCandidateDistanceOrderEntry>& checkpoint_candidate_order,
    std::vector<int>& selected_frame_numbers,
    std::vector<Cnr3CacheSlot>& detached_slots,
    Cnr3CachePruneExecutionSummary& out_summary
) {
    out_summary = Cnr3CachePruneExecutionSummary{};
    out_summary.retain_checkpoint_count = retain_checkpoint_count;
    out_summary.max_remove_count = max_remove_count;

    candidate_order.clear();
    checkpoint_candidate_order.clear();
    selected_frame_numbers.clear();
    detached_slots.clear();

    if (
        candidate_order.capacity() < max_remove_count ||
        checkpoint_candidate_order.capacity() < max_remove_count ||
        selected_frame_numbers.capacity() < max_remove_count ||
        detached_slots.capacity() < max_remove_count
        ) {
        return Cnr3Status::invalid_argument;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    Cnr3Status status = cnr3_calculate_cache_prune_trigger_decision(
        frame_byte_count,
        slot_count_locked(),
        checkpoint_count_locked(),
        retain_checkpoint_count,
        out_summary.trigger_decision
    );

    if (!cnr3_status_is_ok(status)) {
        return status;
    }

    if (
        (!out_summary.trigger_decision.prune_is_required &&
         !out_summary.trigger_decision.checkpoint_prune_is_required) ||
        max_remove_count == 0U
        ) {
#if defined(CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION)
        observe_prune_execution_locked(
            frame_byte_count,
            candidate_order,
            selected_frame_numbers,
            out_summary,
            0U
        );
#endif
        return Cnr3Status::ok;
    }

    const std::size_t requested_remove_count = std::max(
        out_summary.trigger_decision.target_remove_count,
        out_summary.trigger_decision.checkpoint_target_remove_count
    );
    const std::size_t remove_limit = std::min(
        requested_remove_count,
        max_remove_count
    );

    out_summary.bounded_remove_limit = remove_limit;

    if (remove_limit == 0U) {
#if defined(CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION)
        observe_prune_execution_locked(
            frame_byte_count,
            candidate_order,
            selected_frame_numbers,
            out_summary,
            0U
        );
#endif
        return Cnr3Status::ok;
    }

    const bool noncheckpoint_capacity_permits =
        out_summary.trigger_decision.prune_is_required;

#if defined(CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION) || defined(CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE)
    const std::size_t hot_zone_prune_rejection_count =
        count_prune_candidates_rejected_by_hot_zone_locked(
            noncheckpoint_capacity_permits,
            retain_checkpoint_count
        );
#endif

    status = select_composite_prune_candidates_bounded_locked(
        noncheckpoint_capacity_permits,
        retain_checkpoint_count,
        remove_limit,
        candidate_order,
        checkpoint_candidate_order
    );

    if (!cnr3_status_is_ok(status)) {
        return status;
    }

    out_summary.selected_candidate_count = candidate_order.size();

    for (const Cnr3PruneCandidateDistanceOrderEntry& candidate : candidate_order) {
        if (!cnr3_prune_candidate_distance_order_entry_is_valid(candidate)) {
            return Cnr3Status::invariant_violation;
        }

        selected_frame_numbers.push_back(candidate.frame_number);
    }

    status = remove_selected_unpinned_frames_bounded_locked(
        selected_frame_numbers,
        remove_limit,
        detached_slots,
        out_summary.detached_count
    );

    if (!cnr3_status_is_ok(status)) {
        return status;
    }

    if (out_summary.detached_count != out_summary.selected_candidate_count) {
        return Cnr3Status::invariant_violation;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

#if defined(CNR3_DIAG_COMPUTE_DSUM11_HOT_ZONE)
    observe_hot_zone_prune_rejections_locked(hot_zone_prune_rejection_count);
#endif

#if defined(CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION)
    observe_prune_execution_locked(
        frame_byte_count,
        candidate_order,
        selected_frame_numbers,
        out_summary,
        hot_zone_prune_rejection_count
    );
#endif

    return Cnr3Status::ok;
}


Cnr3Status Cnr3OutputCacheCore::plan_bounded_recovery_search_locked(
    int requested_frame,
    int max_back_radius,
    Cnr3CacheRecoverySearchPlan& out_plan
) const {
    out_plan.hole_frame_numbers.clear();
    out_plan.anchor_pin_recorded = false;

    if (!cnr3_frame_number_is_valid(requested_frame) || max_back_radius <= 0) {
        return Cnr3Status::invalid_argument;
    }

    const std::size_t max_hole_count = static_cast<std::size_t>(max_back_radius);

    if (out_plan.hole_frame_numbers.capacity() < max_hole_count) {
        return Cnr3Status::invalid_argument;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    const int lower_bound =
        (requested_frame > max_back_radius)
        ? (requested_frame - max_back_radius)
        : 0;
    const int upper_bound =
        (requested_frame > 0)
        ? (requested_frame - 1)
        : CNR3_INVALID_FRAME_NUMBER;

    out_plan.requested_frame = requested_frame;
    out_plan.max_back_radius = max_back_radius;
    out_plan.search_lower_frame = lower_bound;
    out_plan.search_upper_frame = upper_bound;
    out_plan.search_interval_has_frames =
        cnr3_frame_number_is_valid(upper_bound) && lower_bound <= upper_bound;
    out_plan.requested_frame_is_repair_target = true;
    out_plan.requested_frame_is_in_hole_catalogue = false;

    if (!out_plan.search_interval_has_frames) {
        return cnr3_current_minimal_recovery_plan_status(out_plan);
    }

    int anchor_frame = CNR3_INVALID_FRAME_NUMBER;
    bool anchor_is_checkpoint = false;

    for (int candidate_frame = upper_bound; candidate_frame >= lower_bound; --candidate_frame) {
        const auto index_it = frame_index_.find(candidate_frame);

        if (index_it != frame_index_.end()) {
            const std::size_t slot_index = index_it->second;

            if (slot_index >= slots_.size()) {
                return Cnr3Status::invariant_violation;
            }

            const Cnr3CacheSlot& slot = slots_[slot_index];

            if (!cnr3_cache_slot_is_indexable(slot)) {
                return Cnr3Status::invariant_violation;
            }

            anchor_frame = candidate_frame;
            anchor_is_checkpoint = slot.is_checkpoint;
            break;
        }

        if (candidate_frame == 0) {
            break;
        }
    }

    if (!cnr3_frame_number_is_valid(anchor_frame)) {
        return cnr3_current_minimal_recovery_plan_status(out_plan);
    }

    out_plan.anchor_found = true;
    out_plan.anchor_frame_number = anchor_frame;
    out_plan.anchor_is_checkpoint = anchor_is_checkpoint;

    for (int frame_number = anchor_frame + 1; frame_number < requested_frame; ++frame_number) {
        const auto index_it = frame_index_.find(frame_number);

        if (index_it == frame_index_.end()) {
            out_plan.hole_frame_numbers.push_back(frame_number);
            continue;
        }

        const std::size_t slot_index = index_it->second;

        if (slot_index >= slots_.size()) {
            return Cnr3Status::invariant_violation;
        }

        if (!cnr3_cache_slot_is_indexable(slots_[slot_index])) {
            return Cnr3Status::invariant_violation;
        }
    }

    return cnr3_current_minimal_recovery_plan_status(out_plan);
}

Cnr3Status Cnr3OutputCacheCore::plan_bounded_recovery_search_and_record_anchor_pin_locked(
    int requested_frame,
    int max_back_radius,
    Cnr3CachePinList& pin_list,
    Cnr3CacheRecoverySearchPlan& out_plan
) {
    const Cnr3Status plan_status = plan_bounded_recovery_search_locked(
        requested_frame,
        max_back_radius,
        out_plan
    );

    if (!cnr3_status_is_ok(plan_status)) {
        return plan_status;
    }

    if (!out_plan.anchor_found) {
        return Cnr3Status::ok;
    }

    const Cnr3Status record_status = lookup_frame_and_record_pin_locked(
        out_plan.anchor_frame_number,
        pin_list
    );

    if (!cnr3_status_is_ok(record_status)) {
        return record_status;
    }

    out_plan.anchor_pin_recorded = true;

    return Cnr3Status::ok;
}


Cnr3Status Cnr3OutputCacheCore::remove_unpinned_checkpoints_above_retain_count_bounded_locked(
    std::size_t retain_checkpoint_count,
    std::size_t max_remove_count,
    std::vector<Cnr3CacheSlot>& detached_slots,
    std::size_t& out_removed_count
) {
    out_removed_count = 0U;

    if (max_remove_count == 0U) {
        return Cnr3Status::ok;
    }

    if (detached_slots.capacity() < max_remove_count) {
        return Cnr3Status::invalid_argument;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    std::size_t slot_position = 0U;

    while (
        slot_position < slots_.size() &&
        out_removed_count < max_remove_count &&
        checkpoint_count_locked() > retain_checkpoint_count
        ) {
        const Cnr3CacheSlot& slot = slots_[slot_position];

        if (!cnr3_cache_slot_is_indexable(slot)) {
            return Cnr3Status::invariant_violation;
        }

        if (
            !slot.is_checkpoint ||
            slot.pin_count != 0 ||
            slot.frame_number == 0
            ) {
            ++slot_position;
            continue;
        }

        const int frame_number = slot.frame_number;
        Cnr3CacheSlot detached_slot{};
        const Cnr3Status remove_status =
            remove_unpinned_frame_locked(frame_number, detached_slot);

        if (!cnr3_status_is_ok(remove_status)) {
            return remove_status;
        }

        detached_slots.push_back(std::move(detached_slot));
        ++out_removed_count;

        /*
            Do not increment slot_position here. The central remove helper may
            move the previous last slot into this position, and that moved slot
            still needs to be evaluated by this bounded selection pass.
        */
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

Cnr3Status Cnr3OutputCacheCore::lookup_frame_and_add_ref_locked(
    int frame_number,
    const VSAPI* vsapi,
    const VSFrame** out_acquired_frame
) const {
    if (out_acquired_frame == nullptr) {
        return Cnr3Status::invalid_argument;
    }

    *out_acquired_frame = nullptr;

    if (!cnr3_frame_number_is_valid(frame_number)) {
        return Cnr3Status::invalid_argument;
    }

    if (vsapi == nullptr || vsapi->addFrameRef == nullptr) {
        return Cnr3Status::invalid_argument;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

#if defined(CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE)
    observe_cache_lookup_query_locked();
#endif

    const auto frame_index_it = frame_index_.find(frame_number);

    if (frame_index_it == frame_index_.end()) {
#if defined(CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION)
        observe_lookup_miss_rechurn_locked(frame_number);
#endif
        return Cnr3Status::not_found;
    }

#if defined(CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE)
    observe_cache_lookup_hit_locked();
#endif

    const std::size_t slot_position = frame_index_it->second;

    if (slot_position >= slots_.size()) {
        return Cnr3Status::invariant_violation;
    }

    const Cnr3CacheSlot& slot = slots_[slot_position];

    if (!cnr3_cache_slot_is_indexable(slot)) {
        return Cnr3Status::invariant_violation;
    }

    if (slot.frame_number != frame_number) {
        return Cnr3Status::invariant_violation;
    }

    const VSFrame* cached_frame = slot.frame.get();

    if (cached_frame == nullptr) {
        return Cnr3Status::invariant_violation;
    }

    const VSFrame* acquired_frame = vsapi->addFrameRef(cached_frame);

    if (acquired_frame == nullptr) {
        return Cnr3Status::vapoursynth_error;
    }

#if defined(CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE)
    observe_lookup_ref_acquired_locked();
#endif

    *out_acquired_frame = acquired_frame;

    return Cnr3Status::ok;
}

Cnr3Status Cnr3OutputCacheCore::lookup_frame_and_record_pin_locked(
    int frame_number,
    Cnr3CachePinList& pin_list
) {
    Cnr3CacheSlotPinToken pin_token{};

    const Cnr3Status pin_status = pin_frame_locked(frame_number, pin_token);

    if (!cnr3_status_is_ok(pin_status)) {
        return pin_status;
    }

    const Cnr3Status record_status =
        pin_list.record_pin_without_allocation(pin_token);

    if (!cnr3_status_is_ok(record_status)) {
        const Cnr3Status unpin_status = unpin_frame_locked(pin_token);

        if (!cnr3_status_is_ok(unpin_status)) {
            return unpin_status;
        }

        return record_status;
    }

    return Cnr3Status::ok;
}

Cnr3Status Cnr3OutputCacheCore::pin_frame_locked(
    int frame_number,
    Cnr3CacheSlotPinToken& out_pin_token
) {
    if (!cnr3_frame_number_is_valid(frame_number)) {
        return Cnr3Status::invalid_argument;
    }

    if (cnr3_cache_slot_pin_token_is_valid(out_pin_token)) {
        return Cnr3Status::invalid_argument;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

#if defined(CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE)
    observe_cache_lookup_query_locked();
#endif

    const auto frame_index_it = frame_index_.find(frame_number);

    if (frame_index_it == frame_index_.end()) {
#if defined(CNR3_DIAG_COMPUTE_DSUM10_PRUNE_EVICTION)
        observe_lookup_miss_rechurn_locked(frame_number);
#endif
        return Cnr3Status::not_found;
    }

#if defined(CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE)
    observe_cache_lookup_hit_locked();
#endif

    const std::size_t slot_position = frame_index_it->second;

    if (slot_position >= slots_.size()) {
        return Cnr3Status::invariant_violation;
    }

    Cnr3CacheSlot& slot = slots_[slot_position];

    if (!cnr3_cache_slot_is_indexable(slot)) {
        return Cnr3Status::invariant_violation;
    }

    if (slot.frame_number != frame_number) {
        return Cnr3Status::invariant_violation;
    }

    if (slot.pin_count == std::numeric_limits<int>::max()) {
        return Cnr3Status::capacity_exceeded;
    }

    ++slot.pin_count;
#if defined(CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE)
    observe_pin_acquired_locked();
#endif

    out_pin_token.slot_id = slot.slot_id;
    out_pin_token.frame_number = slot.frame_number;

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

Cnr3Status Cnr3OutputCacheCore::unpin_frame_locked(
    Cnr3CacheSlotPinToken& pin_token
) {
    if (!cnr3_cache_slot_pin_token_is_valid(pin_token)) {
        return Cnr3Status::invalid_argument;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    const auto frame_index_it = frame_index_.find(pin_token.frame_number);

    if (frame_index_it == frame_index_.end()) {
        return Cnr3Status::lifecycle_violation;
    }

    const std::size_t slot_position = frame_index_it->second;

    if (slot_position >= slots_.size()) {
        return Cnr3Status::invariant_violation;
    }

    Cnr3CacheSlot& slot = slots_[slot_position];

    if (!cnr3_cache_slot_is_indexable(slot)) {
        return Cnr3Status::invariant_violation;
    }

    if (slot.frame_number != pin_token.frame_number) {
        return Cnr3Status::invariant_violation;
    }

    if (slot.slot_id.value != pin_token.slot_id.value) {
        return Cnr3Status::lifecycle_violation;
    }

    if (slot.pin_count <= 0) {
        return Cnr3Status::lifecycle_violation;
    }

    --slot.pin_count;
#if defined(CNR3_DIAG_COMPUTE_DSUM04_OWNERSHIP_BALANCE)
    observe_pin_released_locked();
#endif
    cnr3_cache_slot_pin_token_reset(pin_token);

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

Cnr3Status Cnr3OutputCacheCore::clear_locked(
    std::vector<Cnr3CacheSlot>& detached_slots
) {
    if (!detached_slots.empty()) {
        return Cnr3Status::invalid_argument;
    }

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    for (const Cnr3CacheSlot& slot : slots_) {
        if (slot.pin_count != 0) {
            return Cnr3Status::lifecycle_violation;
        }
    }

    detached_slots.swap(slots_);
    frame_index_.clear();
    checkpoint_slot_positions_.clear();
    hot_zones_.clear();

    if (!cache_state_invariants_hold_locked()) {
        return Cnr3Status::invariant_violation;
    }

    return Cnr3Status::ok;
}

bool Cnr3OutputCacheCore::cache_state_invariants_hold_locked() const noexcept {
#if defined(CNR3_DIAG_COMPUTE_DSUM05_CACHE_INTEGRITY)
    observe_cache_invariant_check_started_locked();
#endif

    if (!cnr3_cache_hot_zone_model_invariants_hold(hot_zones_)) {
        CNR3_DSUM05_FAIL("hot_zone_model");
    }

    for (const auto& frame_index_entry : frame_index_) {
        const int frame_number = frame_index_entry.first;
        const std::size_t slot_position = frame_index_entry.second;

        if (slot_position >= slots_.size()) {
            CNR3_DSUM05_FAIL("frame_index_slot_range");
        }

        const Cnr3CacheSlot& slot = slots_[slot_position];

        if (!cnr3_cache_slot_is_indexable(slot)) {
            CNR3_DSUM05_FAIL("frame_index_slot_state");
        }

        if (slot.frame_number != frame_number) {
            CNR3_DSUM05_FAIL("frame_index_number_mismatch");
        }
    }

    for (std::size_t slot_position = 0; slot_position < slots_.size(); ++slot_position) {
        const Cnr3CacheSlot& slot = slots_[slot_position];

        if (slot.pin_count < 0) {
            CNR3_DSUM05_FAIL("slot_negative_pin");
        }

        if (!cnr3_cache_slot_has_frame(slot)) {
            if (cnr3_cache_slot_id_is_valid(slot.slot_id)) {
                CNR3_DSUM05_FAIL("empty_slot_has_id");
            }

            if (cnr3_frame_number_is_valid(slot.frame_number)) {
                CNR3_DSUM05_FAIL("empty_slot_has_frame_number");
            }

            if (slot.is_checkpoint) {
                CNR3_DSUM05_FAIL("empty_slot_checkpoint");
            }

            if (slot.pin_count != 0) {
                CNR3_DSUM05_FAIL("empty_slot_pin");
            }

            continue;
        }

        if (!cnr3_cache_slot_id_is_valid(slot.slot_id)) {
            CNR3_DSUM05_FAIL("live_slot_missing_id");
        }

        if (!cnr3_frame_number_is_valid(slot.frame_number)) {
            CNR3_DSUM05_FAIL("live_slot_invalid_frame_number");
        }

        const auto frame_index_it = frame_index_.find(slot.frame_number);

        if (frame_index_it == frame_index_.end()) {
            CNR3_DSUM05_FAIL("live_slot_missing_index");
        }

        if (frame_index_it->second != slot_position) {
            CNR3_DSUM05_FAIL("live_slot_index_mismatch");
        }
    }

    std::size_t checkpoint_slot_count = 0;

    for (std::size_t slot_position = 0; slot_position < slots_.size(); ++slot_position) {
        if (slots_[slot_position].is_checkpoint) {
            ++checkpoint_slot_count;
        }
    }

    if (checkpoint_slot_count != checkpoint_slot_positions_.size()) {
        CNR3_DSUM05_FAIL("checkpoint_count");
    }

    for (std::size_t checkpoint_index = 0;
        checkpoint_index < checkpoint_slot_positions_.size();
        ++checkpoint_index
        ) {
        const std::size_t checkpoint_slot_position =
            checkpoint_slot_positions_[checkpoint_index];

        if (checkpoint_slot_position >= slots_.size()) {
            CNR3_DSUM05_FAIL("checkpoint_position_range");
        }

        for (std::size_t compare_index = checkpoint_index + 1U;
            compare_index < checkpoint_slot_positions_.size();
            ++compare_index
            ) {
            if (checkpoint_slot_positions_[compare_index] == checkpoint_slot_position) {
                CNR3_DSUM05_FAIL("checkpoint_position_duplicate");
            }
        }

        const Cnr3CacheSlot& slot = slots_[checkpoint_slot_position];

        if (!slot.is_checkpoint) {
            CNR3_DSUM05_FAIL("checkpoint_slot_class");
        }

        if (!cnr3_cache_slot_is_indexable(slot)) {
            CNR3_DSUM05_FAIL("checkpoint_slot_indexable");
        }
    }

    return true;
}

#undef CNR3_DSUM05_FAIL

bool Cnr3CachePinList::empty() const noexcept {
    return pin_count() == 0U;
}

std::size_t Cnr3CachePinList::pin_count() const noexcept {
    std::size_t valid_pin_count = 0;

    for (std::size_t pin_index = 0; pin_index < used_pin_count_; ++pin_index) {
        if (cnr3_cache_slot_pin_token_is_valid(pin_tokens_[pin_index])) {
            ++valid_pin_count;
        }
    }

    return valid_pin_count;
}

Cnr3Status Cnr3CachePinList::reserve_for_additional_pins(
    std::size_t additional_pin_count
) {
    if (additional_pin_count == 0U) {
        return Cnr3Status::ok;
    }

    if (additional_pin_count > (pin_tokens_.max_size() - used_pin_count_)) {
        return Cnr3Status::capacity_exceeded;
    }

    const std::size_t required_slot_count = used_pin_count_ + additional_pin_count;

    if (pin_tokens_.size() >= required_slot_count) {
        return Cnr3Status::ok;
    }

    try {
        pin_tokens_.resize(required_slot_count);
    }
    catch (const std::bad_alloc&) {
        return Cnr3Status::allocation_failed;
    }

    return Cnr3Status::ok;
}

Cnr3Status Cnr3CachePinList::record_pin(
    Cnr3CacheSlotPinToken& pin_token
) {
    if (!cnr3_cache_slot_pin_token_is_valid(pin_token)) {
        return Cnr3Status::invalid_argument;
    }

    const Cnr3Status reserve_status = reserve_for_additional_pins(1U);

    if (!cnr3_status_is_ok(reserve_status)) {
        return reserve_status;
    }

    return record_pin_without_allocation(pin_token);
}

Cnr3Status Cnr3CachePinList::record_pin_without_allocation(
    Cnr3CacheSlotPinToken& pin_token
) noexcept {
    if (!cnr3_cache_slot_pin_token_is_valid(pin_token)) {
        return Cnr3Status::invalid_argument;
    }

    if (used_pin_count_ >= pin_tokens_.size()) {
        return Cnr3Status::capacity_exceeded;
    }

    pin_tokens_[used_pin_count_] = pin_token;
    ++used_pin_count_;

    cnr3_cache_slot_pin_token_reset(pin_token);

    return Cnr3Status::ok;
}

Cnr3Status Cnr3CachePinList::discharge_all(
    Cnr3OutputCacheCore& cache
) {
    return cache.discharge_pin_list(*this);
}

/*
    CMS07 cache-core implementation notes.

    CMS07-C.3C introduced the single non-recursive std::mutex skeleton.
    CMS07-C.3D split public read-only observers from private lock-protected
    observer helpers. CMS07-C.3E added a structural invariant observer.
    CMS07-C.4 introduced non-checkpoint store. CMS07-C.5 introduced
    immediate lookup/addref. CMS07-C.6 introduced clear/teardown detach.
    CMS07-C.7 introduced balanced slot pin/unpin mechanics. CMS07-C.8
    introduced explicit lookup-pin reservation. CMS07-D.1 introduced
    per-invocation pin-list record/discharge. CMS07-D.2A aligned AS comments
    with the CMS07 register. CMS07-D.3A introduced the AS1-compliant combined
    lookup-pin-record helper. CMS07-E.1A strengthened the isolated store helper
    proof. CMS07-E.2A reconciled the original E.2 lookup-pin-record obligation
    as satisfied by the D.3A combined helper; no second lookup-pin-record helper
    is required. CMS07-F.1A introduced the central single-slot remove helper.
    CMS07-F.2A introduced a bounded selected-detach proof for the AS5 batch
    shape. CMS07-F.3A introduced a narrow unpinned non-checkpoint selection
    proof. CMS07-F.4A introduced a checkpoint retention-boundary proof.
    CMS07-G.1A introduced cache policy constants. CMS07-G.2A introduced the
    hot-zone data model. CMS07-G.3A introduces hot-zone slide/spawn update
    behaviour; merge, retirement, and final prune-policy use remain deferred.

    The current public read-only observers acquire the mutex once at their outer
    boundary using RAII scoped guards.

    Future lock-protected helpers that are called from AS implementations must
    assume the caller already holds the mutex and must not acquire it
    themselves. Manual lock()/unlock() calls are forbidden. This is required to
    keep std::mutex non-recursive, to avoid AS self-deadlock, and to make
    accidental AS-boundary splitting structurally hard to write.

    The comments below are intentionally placed in this source file because AS
    implementation will live here initially. Keeping AS functions close to the
    state they protect reduces the risk of hiding or weakening the lock-scope
    contract.

    V5 firewall:
        VapourSynth's internal frame-reference count atomicity protects only a
        single addFrameRef/freeFrame operation. It does not make any CMS07 cache
        critical section optional, smaller, splittable, reorderable, or movable.

        The protected operation is the whole cache-state decision, not merely
        the frame-reference bump. A find-and-pin-record operation, for example,
        is a single cache-lock operation because the find, pin-count increment,
        and pin-list record must not be separated by a prune window.

    Atomic-scope rule:
        CMS07 AS1-AS7 are designer-owned, mandatory, indivisible cache-lock
        scopes. Do not shrink, split, merge, reorder, or reinterpret them
        without explicit CMS update / user approval.

    Slow work rule:
        Source requests, source retrieval, pixel compute, batch freeFrame after
        detach, diagnostic formatting/printing, and heap-heavy summary
        construction must happen outside CMS07 cache atomic/locked scopes.
*/

/*
    CMS07 8.7 atomic-scope register copy.

    This block is the source-code copy of the CMS07.0 8.7 AS1-AS7 register.
    It is intentionally not paraphrased; only C++ comment indentation and
    ASCII punctuation are normalised for source style. If this block and
    CMS07.0 differ, update this block from the CMS rather than editing the
    rule locally.

    Atomic-scope register - DESIGNER-OWNED, MANDATORY, boundaries inviolable.

    Each entry is one indivisible cache-lock critical section. The coder
    IMPLEMENTS these exactly; the coder may NOT shrink, split, merge, or
    reorder the contents of a scope without explicit designer agreement. All
    slow work (pixel compute, VS source requests, the batch freeFrame) is
    OUTSIDE these scopes by mandate.

    AS1  arInitial plan-and-pin (Section 9.1):
           { Phase-1 descending bounded search [max(0,N-B), N];
             pin the start point and every present reused frame;
             catalogue the output holes;
             append every pin to frameData pin-list;
             update/slide hot zone(s) for N }
           - one lock acquisition, indivisible.

    AS2  arAllFramesReady per-hole store-and-pin (Section 9.2), repeated per hole:
           { first-in-best-dressed check;
             store computed output (or adopt existing winner);
             pin it; append to pin-list }
           - one lock acquisition PER hole; compute happens OUTSIDE before this.

    AS3  reused-frame pin during ascending fill (Section 9.2/9.5), as encountered:
           { confirm output[K] present; addFrameRef under lock; append to pin-list }
           - find-and-pin is one indivisible unit (the find-and-add-ref primitive, D06).

    AS4  final unpin (Section 4.5):
           { for every entry on the pin-list: unpin (decrement) }
           - one lock acquisition for the whole list at end of arAllFramesReady.

    AS5  bounded prune decide+detach (Section 7.3 a+b):
           { evaluate composite eviction predicate;
             select up to K victims (greatest-distance-first);
             detach each victim slot from the index (central remove helper, D09);
             collect freed VSFrame* refs into a local list }
           - one lock acquisition; the batch freeFrame (c) is OUTSIDE this scope.

    AS6  checkpoint establish (Sections 6.3/6.4):
           { on store of a grid frame or a detected-cut frame, set is_checkpoint;
             insert into checkpoint pool / ordered index }
           - folded into the relevant AS2 store scope (same lock), not a separate lock.

    AS7  zone retirement / merge (Sections 5.5/5.6):
           { test no-pins-in-range + decay-margin; mark zone inactive / merge }
           - performed under the same lock during AS1 or the prune pass; never split.

    The register is the single authority on critical-section boundaries. If an
    operation needed in practice is not covered here, it is raised to the
    designer - NOT improvised with an ad-hoc smaller lock.
*/
